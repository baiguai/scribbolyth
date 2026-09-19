#include "search.hpp"

#include <algorithm>
#include <cctype>
#include <map>
#include <regex>
#include <sstream>
#include <utility>
#include <vector>

#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>

#include "../editor/editor_state.hpp"

namespace scribbolyth::search
{
    namespace
    {
        std::string Lower(std::string s)
        {
            std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
                return static_cast<char>(std::tolower(c));
            });
            return s;
        }

        std::string PadRight(const std::string& s, std::size_t width)
        {
            if (s.size() >= width) return s;
            return s + std::string(width - s.size(), ' ');
        }

        // Escape the ECMAScript metacharacters so an "all words" search term
        // keeps its literal meaning inside the generated regex.
        std::string EscapeRegex(const std::string& s)
        {
            static const std::string special = "\\^$.|?*+()[]{}";
            std::string out;
            out.reserve(s.size());
            for (const char c : s)
            {
                if (special.find(c) != std::string::npos) out += '\\';
                out += c;
            }
            return out;
        }

        // Builds `(?=[\s\S]*\bw1\b)(?=[\s\S]*\bw2\b)...(?=[\s\S]*\bwn\b)[\s\S]*`
        // from the whitespace-separated words of `query`, requiring every word
        // to be present anywhere in the text (any order, any line). `[\s\S]`
        // is the ECMAScript stand-in for DOTALL, so words on different lines
        // still satisfy the lookaheads.
        std::string AllWordsRegex(const std::string& query)
        {
            std::ostringstream re;
            std::istringstream words(query);
            std::string word;
            while (words >> word)
            {
                re << "(?=[\\s\\S]*\\b" << EscapeRegex(word) << "\\b)";
            }
            re << "[\\s\\S]*";
            return re.str();
        }

        std::string IndentName(int depth, const std::string& name)
        {
            return std::string(static_cast<std::size_t>(depth) * 2, ' ') + name;
        }

        struct Result
        {
            treeview::TreeNode* node;
            std::string line;
        };

        // Parsed search query: a leading "r:" selects case-insensitive regex
        // matching, a leading ":" restricts the match to node titles and a
        // leading "+:" requires every word to appear (in any order).
        struct Filter
        {
            std::string query;
            bool is_regex = false;
            bool all_words = false;
            bool title_only = false;
        };

        Filter ParseFilter(const std::string& raw)
        {
            Filter f;
            f.query = raw;
            if (f.query.size() >= 2 && f.query[0] == '+' && f.query[1] == ':')
            {
                f.all_words = true;
                f.query = f.query.substr(2);
            }
            if (f.query.size() >= 2 && f.query[0] == 'r' && f.query[1] == ':')
            {
                f.is_regex = true;
                f.query = f.query.substr(2);
            }
            if (!f.query.empty() && f.query[0] == ':')
            {
                f.title_only = true;
                f.query = f.query.substr(1);
            }
            return f;
        }

        // Whether the filter runs a regex-based match ("r:" regex or
        // "+:" all-words). These are deferred until Return is pressed so
        // that typing stays responsive instead of re-scanning the tree
        // (and re-compiling a regex) on every keystroke.
        bool IsRegexFilter(const std::string& raw)
        {
            const Filter f = ParseFilter(raw);
            return f.is_regex || f.all_words;
        }

        // Case-insensitive match of a node against a parsed filter. An empty
        // query matches everything. When `query` is a regex that fails to
        // compile, returns false and sets `*regex_error`.
        bool NodeMatches(const treeview::TreeNode& node, const Filter& f,
                         bool* regex_error)
        {
            if (f.query.empty()) return true;
            if (f.is_regex)
            {
                try
                {
                    const std::regex re(f.query, std::regex::icase | std::regex_constants::multiline);
                    return std::regex_search(node.name, re)
                        || (!f.title_only && std::regex_search(node.text, re));
                }
                catch (const std::regex_error&)
                {
                    if (regex_error) *regex_error = true;
                    return false;
                }
            }
            if (f.all_words)
            {
                try
                {
                    const std::regex re(AllWordsRegex(f.query), std::regex::icase);
                    return std::regex_search(node.name, re)
                        || (!f.title_only && std::regex_search(node.text, re));
                }
                catch (const std::regex_error&)
                {
                    if (regex_error) *regex_error = true;
                    return false;
                }
            }
            const std::string needle = Lower(f.query);
            return Lower(node.name).find(needle) != std::string::npos
                || (!f.title_only && Lower(node.text).find(needle) != std::string::npos);
        }

        // A tag that is purely hex digits of length 3, 4, 6 or 8 (e.g. "#fff"
        // or "#ff8800") is an HTML color, not a tag.
        bool IsHexColor(const std::string& tag)
        {
            const std::size_t n = tag.size();
            if (n != 3 && n != 4 && n != 6 && n != 8) return false;
            return std::all_of(tag.begin(), tag.end(), [](unsigned char c) {
                return std::isxdigit(c) != 0;
            });
        }

        // Collect `#tag` tokens from `text` into `counts` (keyed by lowercase
        // tag, so the map iteration is sorted and de-duplicated). HTML color
        // codes such as "#fff" or "#ff8800" are ignored.
        void CollectTags(const std::string& text, std::map<std::string, int>& counts)
        {
            static const std::regex kTagRegex(R"(#[\w-]+)");
            for (std::sregex_iterator it(text.begin(), text.end(), kTagRegex), end;
                 it != end; ++it)
            {
                const std::string tag = Lower(it->str().substr(1));
                if (IsHexColor(tag)) continue;
                ++counts[tag];
            }
        }

        bool ContainsTag(const std::string& text, const std::string& tag)
        {
            static const std::regex kTagRegex(R"(#[\w-]+)");
            for (std::sregex_iterator it(text.begin(), text.end(), kTagRegex), end;
                 it != end; ++it)
            {
                if (Lower(it->str().substr(1)) == tag) return true;
            }
            return false;
        }
    }

    std::vector<treeview::TreeNode*> FindMatches(std::shared_ptr<EditorState> state,
                                                 const std::string& raw_query)
    {
        std::vector<treeview::TreeNode*> out;
        if (state->active_node == nullptr) return out;
        const Filter f = ParseFilter(raw_query);
        bool regex_error = false;
        if (NodeMatches(*state->active_node, f, &regex_error))
        {
            out.push_back(state->active_node);
        }
        return out;
    }

    std::vector<treeview::TreeNode*> FindAllMatches(std::shared_ptr<EditorState> state,
                                                    const std::string& raw_query)
    {
        std::vector<treeview::TreeNode*> out;
        std::vector<std::pair<treeview::TreeNode*, int>> all;
        if (state->collect_all_nodes)
        {
            all = state->collect_all_nodes();
        }
        const Filter f = ParseFilter(raw_query);
        bool regex_error = false;
        for (const auto& item : all)
        {
            if (NodeMatches(*item.first, f, &regex_error))
            {
                out.push_back(item.first);
            }
        }
        return out;
    }

    treeview::TreeNode* CreateSearchResults(std::shared_ptr<EditorState> state,
                                            const std::vector<treeview::TreeNode*>& nodes,
                                            const std::string& raw_query,
                                            std::string* status)
    {
        std::string body;
        for (const treeview::TreeNode* node : nodes)
        {
            if (node == nullptr) continue;
            body += "_" + node->name + "_\n";
        }
        if (body.empty())
        {
            if (status) *status = "No matches to collect";
            return nullptr;
        }

        // The node title carries the (prefix-stripped) search string so it
        // reads as a search-results node: e.g. "Search results: vodka lime".
        const std::string title = "Search results: " + ParseFilter(raw_query).query;
        // Created like a normal new child ('A'): under the selected node when
        // one is selected, expanding it, or as a root node otherwise.
        const auto it = state->operations.find("new_child");
        if (it == state->operations.end())
        {
            if (status) *status = "Cannot create a node now";
            return nullptr;
        }
        it->second(title, 1);
        if (state->active_node == nullptr)
        {
            if (status) *status = "Cannot create a node now";
            return nullptr;
        }
        state->active_node->text = std::move(body);
        state->changed = true;
        if (status)
        {
            *status = "Created search-node with " + std::to_string(nodes.size())
                + (nodes.size() == 1 ? " link" : " links");
        }
        return state->active_node;
    }

    std::vector<std::pair<int, int>> FindLineMatches(const std::string& line,
                                                     const std::string& raw_query)
    {
        const Filter f = ParseFilter(raw_query);
        if (f.title_only || f.query.empty()) return {};

        std::vector<std::pair<int, int>> out;
        if (f.all_words)
        {
            // A node matches when its words appear anywhere across its lines,
            // so at the line level each word occurrence is highlighted and
            // each becomes a target for n/N navigation.
            try
            {
                std::istringstream words(f.query);
                std::string word;
                while (words >> word)
                {
                    const std::regex re("\\b" + EscapeRegex(word) + "\\b",
                                        std::regex::icase);
                    for (std::sregex_iterator it(line.begin(), line.end(), re), end;
                         it != end; ++it)
                    {
                        out.emplace_back(static_cast<int>(it->position()),
                                         static_cast<int>(it->position() + it->length()));
                    }
                }
                std::sort(out.begin(), out.end());
            }
            catch (const std::regex_error&)
            {
                return {};
            }
            return out;
        }
        if (f.is_regex)
        {
            try
            {
                const std::regex re(f.query, std::regex::icase);
                for (std::sregex_iterator it(line.begin(), line.end(), re), end;
                     it != end; ++it)
                {
                    out.emplace_back(static_cast<int>(it->position()),
                                     static_cast<int>(it->position() + it->length()));
                }
            }
            catch (const std::regex_error&)
            {
                return {};
            }
            return out;
        }

        const std::string needle = Lower(f.query);
        const std::string hay = Lower(line);
        std::size_t pos = 0;
        while ((pos = hay.find(needle, pos)) != std::string::npos)
        {
            out.emplace_back(static_cast<int>(pos),
                             static_cast<int>(pos + needle.size()));
            pos += needle.size();
        }
        return out;
    }

    class SearchDialog : public ftxui::ComponentBase
    {
    public:
        SearchDialog(std::shared_ptr<EditorState> state, bool* show,
                     DialogMode mode)
            : state_(std::move(state)),
              show_(show),
              mode_(mode) {}

        bool Focusable() const override { return true; }

        bool OnEvent(ftxui::Event event) override
        {
            if (event == ftxui::Event::Escape)
            {
                Close();
                return true;
            }
            if (event == ftxui::Event::Return)
            {
                if (tag_phase_ && !results_.empty())
                {
                    const int sel = std::min(selection_, static_cast<int>(results_.size()) - 1);
                    filter_ = results_[static_cast<std::size_t>(sel)].line;
                    tag_phase_ = false;
                    Invalidate();
                    return true;
                }
                if (IsRegexFilter(filter_) && filter_ != searched_filter_)
                {
                    // First Enter runs the deferred search and shows the
                    // results; a second Enter (or arrow keys + Enter) selects.
                    Recompute(/*force=*/true);
                    return true;
                }
                if (!results_.empty())
                {
                    const int sel = std::min(selection_, static_cast<int>(results_.size()) - 1);
                    treeview::TreeNode* node = results_[static_cast<std::size_t>(sel)].node;
                    if (mode_ == DialogMode::CreateResults)
                    {
                        // Collect every match (not just the selection) into a
                        // new node whose title shows the search string.
                        std::vector<treeview::TreeNode*> matches;
                        for (const auto& r : results_)
                        {
                            if (r.node != nullptr) matches.push_back(r.node);
                        }
                        std::string status;
                        CreateSearchResults(state_, matches, filter_, &status);
                        state_->status = status;
                    }
                    else if (mode_ == DialogMode::InsertLink)
                    {
                        if (state_->insert_text_at_cursor && node != nullptr)
                        {
                            state_->insert_text_at_cursor("_" + node->name + "_");
                            state_->status = "Inserted " + node->name;
                        }
                    }
                    else if (state_->reveal_node)
                    {
                        state_->reveal_node(node);
                        state_->status = "";
                    }
                    Close();
                }
                return true;
            }
            if (event == ftxui::Event::ArrowDown)
            {
                MoveSelection(+1);
                return true;
            }
            if (event == ftxui::Event::ArrowUp)
            {
                MoveSelection(-1);
                return true;
            }
            if (event == ftxui::Event::Backspace)
            {
                if (!filter_.empty())
                {
                    filter_.pop_back();
                    Invalidate();
                }
                return true;
            }
            if (event.is_character())
            {
                filter_ += event.character();
                Invalidate();
                return true;
            }
            return true;
        }

        ftxui::Element Render() override
        {
            if (!results_valid_) Recompute();

            const int total = static_cast<int>(results_.size());
            const int sel = std::min(selection_, std::max(0, total - 1));

            const int max_top = std::max(0, total - kVisibleRows);
            int top = std::min(scroll_, max_top);
            if (sel < top) top = sel;
            if (sel >= top + kVisibleRows) top = sel - kVisibleRows + 1;
            top = std::max(0, std::min(top, max_top));
            const int count = std::min(kVisibleRows, std::max(0, total - top));

            ftxui::Elements rows;
            const int row_width = content_width_ + 2;
            if (total == 0)
            {
                std::string msg;
                if (regex_error_) msg = "  Invalid regex";
                else if (filter_.empty()) msg = "  No nodes in the document";
                else if (IsRegexFilter(filter_) && filter_ != searched_filter_)
                    msg = "  Enter to search";
                else msg = "  No matches";
                rows.push_back(ftxui::text(PadRight(msg, row_width)) | ftxui::dim);
            }
            else
            {
                for (int i = 0; i < count; ++i)
                {
                    ftxui::Element row = ftxui::text(" " + PadRight(results_[static_cast<std::size_t>(top + i)].line, content_width_) + " ");
                    if (top + i == sel) row = row | ftxui::inverted;
                    rows.push_back(row);
                }
            }
            while (static_cast<int>(rows.size()) < kVisibleRows)
            {
                rows.push_back(ftxui::text(PadRight("", row_width)));
            }

            const std::string footer =
                "  " + std::to_string(total == 0 ? 0 : sel + 1) + "/" + std::to_string(total) +
                (mode_ == DialogMode::CreateResults
                     ? "    Up/Down move  Enter collect  "
                     : mode_ == DialogMode::InsertLink
                           ? "    Up/Down move  Enter insert _Title_  "
                           : "    Up/Down move  Enter jump  ") +
                "Esc cancel  ':' = titles only  'r:' = regex  '+:' = all words  '#' = tags  " +
                (IsRegexFilter(filter_) ? "  Enter runs r:/+:  " : "");

            return ftxui::window(ftxui::text(mode_ == DialogMode::InsertLink
                                                 ? " / Insert Link "
                                                 : mode_ == DialogMode::CreateResults
                                                       ? " \\ Results "
                                                       : " / Search "),
                                ftxui::vbox({
                                    ftxui::hbox({
                                        ftxui::text(" Search: " + filter_ + "_"),
                                        ftxui::filler(),
                                    }),
                                    ftxui::separator(),
                                    ftxui::vbox(std::move(rows)),
                                    ftxui::separator(),
                                    ftxui::text(PadRight(footer, row_width)) | ftxui::dim,
                                })) |
                   ftxui::size(ftxui::WIDTH, ftxui::LESS_THAN, 190) |
                   ftxui::size(ftxui::HEIGHT, ftxui::LESS_THAN, 24);
        }

    private:
        void Invalidate()
        {
            results_valid_ = false;
            selection_ = 0;
            scroll_ = 0;
        }

        void Close()
        {
            *show_ = false;
            filter_.clear();
            searched_filter_.clear();
            selection_ = 0;
            scroll_ = 0;
            regex_error_ = false;
            tag_phase_ = false;
            results_valid_ = false;
        }

        void MoveSelection(int dir)
        {
            if (results_.empty()) return;
            const int total = static_cast<int>(results_.size());
            selection_ = std::max(0, std::min(total - 1, selection_ + dir));
        }

        void Recompute(bool force = false)
        {
            results_.clear();
            regex_error_ = false;
            tag_phase_ = false;

            std::vector<std::pair<treeview::TreeNode*, int>> all;
            if (state_->collect_all_nodes)
            {
                all = state_->collect_all_nodes();
            }

            content_width_ = 72;
            for (const auto& item : all)
            {
                content_width_ = std::max(content_width_,
                                          static_cast<int>(IndentName(item.second, item.first->name).size()));
            }

            if (!filter_.empty() && filter_[0] == '#')
            {
                RecomputeTags(all);
            }
            else if (!force && IsRegexFilter(filter_))
            {
                // Deferred: don't scan the tree until the user presses
                // Return; just show a hint.
                searched_filter_.clear();
            }
            else
            {
                const Filter f = ParseFilter(filter_);
                bool regex_error = false;
                for (const auto& item : all)
                {
                    if (NodeMatches(*item.first, f, &regex_error))
                    {
                        results_.push_back(Result{item.first, IndentName(item.second, item.first->name)});
                    }
                }
                if (regex_error) regex_error_ = true;
                searched_filter_ = filter_;
            }

            selection_ = 0;
            scroll_ = 0;
            results_valid_ = true;
        }

        // Tag search: a leading '#' lists matching tags; picking one (or an
        // exact match) lists the nodes whose content carries that tag.
        void RecomputeTags(const std::vector<std::pair<treeview::TreeNode*, int>>& all)
        {
            const std::string typed = Lower(filter_.substr(1));

            std::map<std::string, int> counts;
            for (const auto& item : all)
            {
                CollectTags(item.first->text, counts);
            }

            if (!typed.empty() && counts.find(typed) != counts.end())
            {
                for (const auto& item : all)
                {
                    if (ContainsTag(item.first->text, typed))
                    {
                        results_.push_back(Result{item.first, IndentName(item.second, item.first->name)});
                    }
                }
                return;
            }

            tag_phase_ = true;
            for (const auto& entry : counts)
            {
                if (entry.first.find(typed) != std::string::npos)
                {
                    const std::string line = "#" + entry.first;
                    results_.push_back(Result{nullptr, line});
                    content_width_ = std::max(content_width_, static_cast<int>(line.size()));
                }
            }
        }

        std::shared_ptr<EditorState> state_;
        bool* show_;
        DialogMode mode_ = DialogMode::Jump;
        std::string filter_;
        int selection_ = 0;
        int scroll_ = 0;
        int content_width_ = 72;
        bool results_valid_ = false;
        bool regex_error_ = false;
        bool tag_phase_ = false;
        std::string searched_filter_;
        std::vector<Result> results_;
        static constexpr int kVisibleRows = 18;
    };

    ftxui::Component MakeSearchDialog(std::shared_ptr<EditorState> state, bool* show,
                                      DialogMode mode)
    {
        return ftxui::Make<SearchDialog>(std::move(state), show, mode);
    }
}
