#include "convert.hpp"

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

                bool ParseBookmarks(std::vector<bookmark::Bookmark>& marks)
                {
                    if (!Consume('[')) return false;
                    SkipWs();
                    if (Consume(']')) return true;
                    while (true)
                    {
                        SkipWs();
                        bookmark::Bookmark mark{};
                        if (!ParseBookmark(mark)) return false;
                        marks.push_back(std::move(mark));
                        SkipWs();
                        if (Consume(',')) continue;
                        if (Consume(']')) return true;
                        return false;
                    }
                }

                bool ParseBookmark(bookmark::Bookmark& mark)
                {
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

                        if (key == "id")
                        {
                            if (!ParseString(mark.id)) return false;
                        }
                        else if (key == "line")
                        {
                            if (!ParseNumber(mark.line)) return false;
                        }
                        else
                        {
                            if (!SkipValue()) return false;
                        }

                        SkipWs();
                        if (Consume(',')) continue;
                        if (Consume('}')) return true;
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

                        if (key == "id")         { if (!ParseString(node.id)) return false; }
                        else if (key == "title") { if (!ParseString(node.name)) return false; }
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

        // Locate the '[' that opens the `let <keyword> = [ ... ];` array and the
        // index of its matching ']' (string- and nesting-aware).
        bool FindJsArray(const std::string& html, const std::string& keyword,
                         std::size_t& open, std::size_t& close)
        {
            std::size_t kw = html.find(keyword);
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

        void AppendNode(std::string& out, const TreeNode& node, int depth)
        {
            std::string pad(depth * 2, ' ');
            std::string pad2((depth + 1) * 2, ' ');

            std::string id = node.id.empty() ? scribbolyth::bookmark::NewId() : node.id;
            out += pad + "{\n";
            out += pad2 + "\"id\": \"" + id + "\",\n";
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

        void ReplaceArray(std::string& html, std::size_t open, std::size_t close,
                          const std::string& json)
        {
            std::size_t end = close + 1;
            while (end < html.size() && (html[end] == ' ' || html[end] == '\t'
                   || html[end] == '\n' || html[end] == '\r')) ++end;
            if (end < html.size() && html[end] == ';') ++end;
            html.replace(open, end - open, json + ";");
        }

        bool ReplaceTreeData(std::string& html, const std::vector<TreeNode>& roots)
        {
            std::size_t open = 0;
            std::size_t close = 0;
            if (!FindJsArray(html, "treeData", open, close)) return false;

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
            ReplaceArray(html, open, close, json);
            return true;
        }

        const TreeNode* FindNodeById(const std::vector<TreeNode>& nodes,
                                     const std::string& id,
                                     std::vector<std::string>& path)
        {
            for (const auto& node : nodes)
            {
                if (node.id == id)
                {
                    path.push_back(node.name);
                    return &node;
                }
                path.push_back(node.name);
                if (const TreeNode* found = FindNodeById(node.children, id, path))
                {
                    return found;
                }
                path.pop_back();
            }
            return nullptr;
        }

        std::string JoinPath(const std::vector<std::string>& path)
        {
            std::string out;
            for (std::size_t i = 0; i < path.size(); ++i)
            {
                if (i != 0) out += " / ";
                out += path[i];
            }
            return out;
        }

        // Serialize bookmarks into the HTML app's format:
        //   { "id", "caption", "path", "line"? }  with `line` being 1-based.
        // Bookmarks whose target node no longer exists are skipped.
        void AppendBookmarksJson(std::string& out,
                                 const std::vector<bookmark::Bookmark>& bookmarks,
                                 const std::vector<TreeNode>& roots)
        {
            out += "[\n";
            std::size_t written = 0;
            for (const auto& mark : bookmarks)
            {
                std::vector<std::string> path;
                const TreeNode* node = FindNodeById(roots, mark.id, path);
                if (!node) continue;

                if (written != 0) out += ",\n";
                out += "  {\"id\": \"" + mark.id + "\",\n";
                out += "   \"caption\": " + scribbolyth::io::JsonEscape(node->name) + ",\n";
                out += "   \"path\": " + scribbolyth::io::JsonEscape(JoinPath(path)) + "";
                if (mark.line >= 0)
                {
                    out += ",\n   \"line\": " + std::to_string(mark.line + 1);
                }
                out += "\n  }";
                ++written;
            }
            out += "\n]";
        }

        bool ReplaceBookmarks(std::string& html,
                              const std::vector<bookmark::Bookmark>& bookmarks,
                              const std::vector<TreeNode>& roots)
        {
            if (bookmarks.empty()) return true;  // keep the template's block as-is

            std::size_t open = 0;
            std::size_t close = 0;
            if (!FindJsArray(html, "bookmarks", open, close)) return false;

            std::string json;
            AppendBookmarksJson(json, bookmarks, roots);
            ReplaceArray(html, open, close, json);
            return true;
        }
    }

    bool ImportHtmlFile(const std::string& path, std::vector<TreeNode>& roots,
                        std::vector<bookmark::Bookmark>* bookmarks)
    {
        std::string content;
        if (!scribbolyth::io::ReadFile(path, content)) return false;

        std::size_t open = 0;
        std::size_t close = 0;
        if (!FindJsArray(content, "treeData", open, close)) return false;

        std::string array = content.substr(open, close - open + 1);
        Parser parser(array);
        std::vector<TreeNode> result;
        if (!parser.ParseArray(result)) return false;

        if (bookmarks)
        {
            std::vector<bookmark::Bookmark> marks;
            std::size_t bm_open = 0;
            std::size_t bm_close = 0;
            if (FindJsArray(content, "bookmarks", bm_open, bm_close))
            {
                std::string bm_array = content.substr(bm_open, bm_close - bm_open + 1);
                Parser bm_parser(bm_array);
                if (!bm_parser.ParseBookmarks(marks)) return false;
            }
            for (auto& mark : marks)
            {
                if (mark.line > 0) mark.line -= 1;  // HTML uses 1-based lines
            }
            *bookmarks = std::move(marks);
        }

        roots = std::move(result);
        return true;
    }

    bool ExportHtmlFile(const std::string& template_path, const std::string& out_path,
                        const std::vector<TreeNode>& roots,
                        const std::vector<bookmark::Bookmark>& bookmarks)
    {
        std::string content;
        if (!scribbolyth::io::ReadFile(template_path, content)) return false;
        if (!ReplaceTreeData(content, roots)) return false;
        if (!ReplaceBookmarks(content, bookmarks, roots)) return false;
        return scribbolyth::io::WriteFile(out_path, content);
    }
}
