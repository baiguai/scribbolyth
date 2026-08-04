#include "convert.hpp"

#include <random>
#include <string>
#include <utility>
#include <vector>

#include "../io/serialize.hpp"

namespace scribbolyth::html
{
    namespace
    {
        class Parser
        {
            public:
                explicit Parser(const std::string& s) : s_(s) {}

                bool ParseArray(std::vector<TreeNode>& arr)
                {
                    if (!Consume('[')) return false;
                    SkipWs();
                    if (Consume(']')) return true;
                    while (true)
                    {
                        SkipWs();
                        TreeNode node{};
                        if (!ParseObject(node)) return false;
                        arr.push_back(std::move(node));
                        SkipWs();
                        if (Consume(',')) continue;
                        if (Consume(']')) return true;
                        return false;
                    }
                }

            private:
                const std::string& s_;
                std::size_t i_ = 0;

                void SkipWs()
                {
                    while (i_ < s_.size() && (s_[i_] == ' ' || s_[i_] == '\t'
                           || s_[i_] == '\n' || s_[i_] == '\r')) ++i_;
                }

                bool Consume(char c)
                {
                    if (i_ < s_.size() && s_[i_] == c) { ++i_; return true; }
                    return false;
                }

                bool ParseString(std::string& out)
                {
                    if (!Consume('"')) return false;
                    out.clear();
                    while (i_ < s_.size())
                    {
                        char c = s_[i_++];
                        if (c == '"') return true;
                        if (c != '\\') { out += c; continue; }
                        if (i_ >= s_.size()) return false;

                        char e = s_[i_++];
                        switch (e)
                        {
                            case '"':  out += '"';  break;
                            case '\\': out += '\\'; break;
                            case '/':  out += '/';  break;
                            case 'b':  out += '\b'; break;
                            case 'f':  out += '\f'; break;
                            case 'n':  out += '\n'; break;
                            case 'r':  out += '\r'; break;
                            case 't':  out += '\t'; break;
                            case 'u':
                            {
                                int cp = 0;
                                for (int k = 0; k < 4; ++k)
                                {
                                    char h = (i_ < s_.size()) ? s_[i_++] : '\0';
                                    cp <<= 4;
                                    if (h >= '0' && h <= '9')      cp |= h - '0';
                                    else if (h >= 'a' && h <= 'f') cp |= h - 'a' + 10;
                                    else if (h >= 'A' && h <= 'F') cp |= h - 'A' + 10;
                                    else return false;
                                }
                                if (cp < 0x80)
                                {
                                    out += static_cast<char>(cp);
                                }
                                else if (cp < 0x800)
                                {
                                    out += static_cast<char>(0xC0 | (cp >> 6));
                                    out += static_cast<char>(0x80 | (cp & 0x3F));
                                }
                                else
                                {
                                    out += static_cast<char>(0xE0 | (cp >> 12));
                                    out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
                                    out += static_cast<char>(0x80 | (cp & 0x3F));
                                }
                                break;
                            }
                            default: return false;
                        }
                    }
                    return false;
                }

                bool ParseBool(bool& out)
                {
                    if (s_.compare(i_, 4, "true") == 0)  { i_ += 4; out = true;  return true; }
                    if (s_.compare(i_, 5, "false") == 0) { i_ += 5; out = false; return true; }
                    return false;
                }

                bool ParseNumber(int& out)
                {
                    bool neg = false;
                    if (Consume('-')) neg = true;
                    if (i_ >= s_.size() || s_[i_] < '0' || s_[i_] > '9') return false;
                    int value = 0;
                    while (i_ < s_.size() && s_[i_] >= '0' && s_[i_] <= '9')
                        value = value * 10 + (s_[i_++] - '0');
                    out = neg ? -value : value;
                    return true;
                }

                bool SkipValue()
                {
                    SkipWs();
                    if (i_ >= s_.size()) return false;
                    char c = s_[i_];
                    if (c == '"') { std::string tmp; return ParseString(tmp); }
                    if (c == '[')
                    {
                        ++i_;
                        SkipWs();
                        if (Consume(']')) return true;
                        while (true)
                        {
                            if (!SkipValue()) return false;
                            SkipWs();
                            if (Consume(',')) { SkipWs(); continue; }
                            if (Consume(']')) return true;
                            return false;
                        }
                    }
                    if (c == '{')
                    {
                        ++i_;
                        SkipWs();
                        if (Consume('}')) return true;
                        while (true)
                        {
                            SkipWs();
                            std::string key;
                            if (!ParseString(key)) return false;
                            SkipWs();
                            if (!Consume(':')) return false;
                            if (!SkipValue()) return false;
                            SkipWs();
                            if (Consume(',')) continue;
                            if (Consume('}')) return true;
                            return false;
                        }
                    }
                    if (c == 't' || c == 'f') { bool b; return ParseBool(b); }
                    if (c == 'n') { if (s_.compare(i_, 4, "null") == 0) { i_ += 4; return true; } return false; }
                    if (c == '-' || (c >= '0' && c <= '9'))
                    {
                        int n;
                        return ParseNumber(n);
                    }
                    return false;
                }

                bool ParseObject(TreeNode& node)
                {
                    node = TreeNode{};
                    if (!Consume('{')) return false;
                    SkipWs();
                    if (Consume('}')) return true;
                    while (true)
                    {
                        SkipWs();
                        std::string key;
                        if (!ParseString(key)) return false;
                        SkipWs();
                        if (!Consume(':')) return false;
                        SkipWs();

                        if (key == "title")        { if (!ParseString(node.name)) return false; }
                        else if (key == "content") { if (!ParseString(node.text)) return false; }
                        else if (key == "expanded"){ if (!ParseBool(node.expanded)) return false; }
                        else if (key == "children"){ if (!ParseArray(node.children)) return false; }
                        else { if (!SkipValue()) return false; }

                        SkipWs();
                        if (Consume(',')) continue;
                        if (Consume('}')) return true;
                        return false;
                    }
                }
        };

        // Locate the '[' that opens the `let treeData = [ ... ];` array and the
        // index of its matching ']' (string- and nesting-aware).
        bool FindTreeDataArray(const std::string& html, std::size_t& open, std::size_t& close)
        {
            std::size_t kw = html.find("treeData");
            if (kw == std::string::npos) return false;
            open = html.find('[', kw);
            if (open == std::string::npos) return false;

            bool in_string = false;
            bool escaped = false;
            int depth = 0;
            for (std::size_t j = open; j < html.size(); ++j)
            {
                char ch = html[j];
                if (in_string)
                {
                    if (escaped) { escaped = false; continue; }
                    if (ch == '\\') { escaped = true; continue; }
                    if (ch == '"') in_string = false;
                    continue;
                }
                if (ch == '"') { in_string = true; continue; }
                if (ch == '[' || ch == '{') { ++depth; continue; }
                if (ch == ']' || ch == '}')
                {
                    --depth;
                    if (depth == 0) { close = j; return true; }
                }
            }
            return false;
        }

        std::string RandomId()
        {
            static const char alphabet[] = "abcdefghijklmnopqrstuvwxyz0123456789";
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<int> dist(0, static_cast<int>(sizeof(alphabet)) - 2);
            std::string id;
            for (int i = 0; i < 8; ++i) id += alphabet[dist(gen)];
            return id;
        }

        void AppendNode(std::string& out, const TreeNode& node, int depth)
        {
            std::string pad(depth * 2, ' ');
            std::string pad2((depth + 1) * 2, ' ');

            out += pad + "{\n";
            out += pad2 + "\"id\": \"" + RandomId() + "\",\n";
            out += pad2 + "\"title\": " + scribbolyth::io::JsonEscape(node.name) + ",\n";
            out += pad2 + "\"content\": " + scribbolyth::io::JsonEscape(node.text) + ",\n";
            if (node.children.empty())
            {
                out += pad2 + "\"children\": [],\n";
            }
            else
            {
                out += pad2 + "\"children\": [\n";
                for (std::size_t i = 0; i < node.children.size(); ++i)
                {
                    AppendNode(out, node.children[i], depth + 1);
                    out += (i + 1 < node.children.size()) ? ",\n" : "\n";
                }
                out += pad2 + "],\n";
            }
            out += pad2 + "\"expanded\": " + (node.expanded ? "true" : "false") + "\n";
            out += pad + "}";
        }

        bool ReplaceTreeData(std::string& html, const std::vector<TreeNode>& roots)
        {
            std::size_t open = 0;
            std::size_t close = 0;
            if (!FindTreeDataArray(html, open, close)) return false;

            std::size_t end = close + 1;
            while (end < html.size() && (html[end] == ' ' || html[end] == '\t'
                   || html[end] == '\n' || html[end] == '\r')) ++end;
            if (end < html.size() && html[end] == ';') ++end;

            std::string json;
            if (roots.empty())
            {
                json = "[]";
            }
            else
            {
                json = "[\n";
                for (std::size_t i = 0; i < roots.size(); ++i)
                {
                    AppendNode(json, roots[i], 1);
                    json += (i + 1 < roots.size()) ? ",\n" : "\n";
                }
                json += "]";
            }
            html.replace(open, end - open, json + ";");
            return true;
        }
    }

    bool ImportHtmlFile(const std::string& path, std::vector<TreeNode>& roots)
    {
        std::string content;
        if (!scribbolyth::io::ReadFile(path, content)) return false;

        std::size_t open = 0;
        std::size_t close = 0;
        if (!FindTreeDataArray(content, open, close)) return false;

        std::string array = content.substr(open, close - open + 1);
        Parser parser(array);
        std::vector<TreeNode> result;
        if (!parser.ParseArray(result)) return false;

        roots = std::move(result);
        return true;
    }

    bool ExportHtmlFile(const std::string& template_path, const std::string& out_path,
                        const std::vector<TreeNode>& roots)
    {
        std::string content;
        if (!scribbolyth::io::ReadFile(template_path, content)) return false;
        if (!ReplaceTreeData(content, roots)) return false;
        return scribbolyth::io::WriteFile(out_path, content);
    }
}
