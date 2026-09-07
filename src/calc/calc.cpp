#include "calc.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>

namespace scribbolyth::calc
{
    namespace
    {
        class Evaluator
        {
            public:
                explicit Evaluator(const std::string& s) : s_(s) {}

                bool Run(std::string& result)
                {
                    SkipWs();
                    bool ok = true;
                    const double value = Expr(&ok);
                    if (!ok || !error_.empty())
                    {
                        result = error_.empty() ? "Invalid expression" : error_;
                        return false;
                    }
                    SkipWs();
                    if (pos_ != s_.size())
                    {
                        result = "Invalid expression";
                        return false;
                    }
                    result = Format(value);
                    return true;
                }

            private:
                void SkipWs()
                {
                    while (pos_ < s_.size() && (s_[pos_] == ' ' || s_[pos_] == '\t')) ++pos_;
                }

                bool Consume(char c)
                {
                    if (pos_ < s_.size() && s_[pos_] == c) { ++pos_; return true; }
                    return false;
                }

                bool Fail(const std::string& msg)
                {
                    if (error_.empty()) error_ = msg;
                    return false;
                }

                // expr := term (('+' | '-') term)*
                double Expr(bool* ok)
                {
                    double left = Term(ok);
                    if (!*ok) return 0.0;
                    while (true)
                    {
                        SkipWs();
                        if (Consume('+')) left += Term(ok);
                        else if (Consume('-')) left -= Term(ok);
                        else break;
                        if (!*ok) return 0.0;
                    }
                    return left;
                }

                // term := factor (('*' | '/' | '%' | juxtaposition) factor)*
                double Term(bool* ok)
                {
                    double left = Factor(ok);
                    if (!*ok) return 0.0;
                    while (true)
                    {
                        SkipWs();
                        if (Consume('*'))
                        {
                            left *= Factor(ok);
                        }
                        else if (Consume('/'))
                        {
                            const double d = Factor(ok);
                            if (!*ok) return 0.0;
                            if (d == 0.0)
                            {
                                *ok = Fail("Division by zero");
                                return 0.0;
                            }
                            left /= d;
                        }
                        else if (Consume('%'))
                        {
                            const double d = Factor(ok);
                            if (!*ok) return 0.0;
                            if (d == 0.0)
                            {
                                *ok = Fail("Modulo by zero");
                                return 0.0;
                            }
                            left = std::fmod(left, d);
                        }
                        else if (StartsPrimary())
                        {
                            // Implicit multiplication: 2(1+3), (1+3)2.
                            left *= Factor(ok);
                        }
                        else break;
                        if (!*ok) return 0.0;
                    }
                    return left;
                }

                // factor := unary ('^' factor)?  (right-associative exponent)
                double Factor(bool* ok)
                {
                    const double base = Unary(ok);
                    if (!*ok) return 0.0;
                    SkipWs();
                    if (Consume('^'))
                    {
                        const double power = Factor(ok);
                        if (!*ok) return 0.0;
                        return std::pow(base, power);
                    }
                    return base;
                }

                double Unary(bool* ok)
                {
                    SkipWs();
                    if (pos_ >= s_.size())
                    {
                        *ok = Fail("Missing number");
                        return 0.0;
                    }
                    if (s_[pos_] == '-') { ++pos_; return -Unary(ok); }
                    if (s_[pos_] == '+') { ++pos_; return Unary(ok); }
                    return Primary(ok);
                }

                bool StartsPrimary()
                {
                    if (pos_ >= s_.size()) return false;
                    const char c = s_[pos_];
                    return c == '(' || (c >= '0' && c <= '9') || c == '.';
                }

                // primary := number | '(' expr ')'
                double Primary(bool* ok)
                {
                    SkipWs();
                    if (pos_ >= s_.size())
                    {
                        *ok = Fail("Missing number");
                        return 0.0;
                    }
                    const char c = s_[pos_];
                    if (c == '(')
                    {
                        ++pos_;
                        const double v = Expr(ok);
                        if (!*ok) return 0.0;
                        if (!Consume(')'))
                        {
                            *ok = Fail("Mismatched parentheses");
                            return 0.0;
                        }
                        return v;
                    }
                    if ((c >= '0' && c <= '9') || c == '.')
                    {
                        const std::size_t start = pos_;
                        while (pos_ < s_.size() && s_[pos_] >= '0' && s_[pos_] <= '9') ++pos_;
                        if (pos_ < s_.size() && s_[pos_] == '.')
                        {
                            ++pos_;
                            while (pos_ < s_.size() && s_[pos_] >= '0' && s_[pos_] <= '9') ++pos_;
                        }
                        const std::string token = s_.substr(start, pos_ - start);
                        char* end = nullptr;
                        const double v = std::strtod(token.c_str(), &end);
                        if (end == token.c_str() || *end != '\0')
                        {
                            *ok = Fail("Invalid number");
                            return 0.0;
                        }
                        return v;
                    }
                    *ok = Fail(std::string("Unexpected '") + c + "'");
                    return 0.0;
                }

                std::string Format(double v)
                {
                    if (std::isnan(v)) return "NaN";
                    if (std::isinf(v)) return (v < 0) ? "-inf" : "inf";
                    char buf[64];
                    if (v == std::floor(v) && std::fabs(v) < 1e15)
                    {
                        std::snprintf(buf, sizeof buf, "%.0f", v);
                    }
                    else
                    {
                        std::snprintf(buf, sizeof buf, "%.12g", v);
                    }
                    return buf;
                }

                const std::string& s_;
                std::size_t pos_ = 0;
                std::string error_;
        };
    }

    bool Evaluate(const std::string& expr, std::string& result)
    {
        if (expr.empty())
        {
            result = "Missing expression";
            return false;
        }
        Evaluator eval(expr);
        return eval.Run(result);
    }
}