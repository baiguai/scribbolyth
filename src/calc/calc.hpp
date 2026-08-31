#pragma once

#include <string>

namespace scribbolyth::calc
{
    // Evaluate a simple arithmetic expression, e.g. "2(1+3)/4" or "7 * 3".
    // Supports + - * / % ^, parentheses, unary plus/minus, implicit
    // multiplication ("2(1+3)") and decimal numbers.
    //
    // On success returns true and the formatted result in `result`; on
    // failure returns false and an explanation in `result`.
    bool Evaluate(const std::string& expr, std::string& result);
}