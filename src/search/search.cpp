#include "search.hpp"

#include <algorithm>
#include <cctype>
#include <regex>
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

        struct Result
        {
            treeview::TreeNode* node;
            std::string line;
        };
    }

    class SearchDialog : public ftxui::ComponentBase
    {
    public:
        SearchDialog(std::shared_ptr<EditorState> state, bool* show)
            : state_(std::move(state)), show_(show) {}

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
                if (!results_.empty())
                {
                    const int sel = std::min(selection_, static_cast<int>(results_.size()) - 1);
                    if (state_->reveal_node)
                    {
                        state_->reveal_node(results_[static_cast<std::size_t>(sel)].node);
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
                "    Up/Down move  Enter jump  Esc cancel  ':x' = titles only  'r:' = regex  ";

            return ftxui::window(ftxui::text(" / Search "),
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
                   ftxui::size(ftxui::WIDTH, ftxui::LESS_THAN, 92) |
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
            selection_ = 0;
            scroll_ = 0;
            regex_error_ = false;
            results_valid_ = false;
        }

        void MoveSelection(int dir)
        {
            if (results_.empty()) return;
            const int total = static_cast<int>(results_.size());
            selection_ = std::max(0, std::min(total - 1, selection_ + dir));
        }

        void Recompute()
        {
            results_.clear();
            regex_error_ = false;

            std::vector<std::pair<treeview::TreeNode*, int>> all;
            if (state_->collect_all_nodes)
            {
                all = state_->collect_all_nodes();
            }

            content_width_ = 72;
            for (const auto& item : all)
            {
                const std::string line = std::string(static_cast<std::size_t>(item.second) * 2, ' ') + item.first->name;
                content_width_ = std::max(content_width_, static_cast<int>(line.size()));
            }

            std::string query = filter_;
            bool is_regex = false;
            bool title_only = false;
            if (query.size() >= 2 && query[0] == 'r' && query[1] == ':')
            {
                is_regex = true;
                query = query.substr(2);
            }
            if (!query.empty() && query[0] == ':')
            {
                title_only = true;
                query = query.substr(1);
            }

            if (is_regex)
            {
                if (!query.empty())
                {
                    try
                    {
                        const std::regex re(query, std::regex::icase);
                        for (const auto& item : all)
                        {
                            if (std::regex_search(item.first->name, re) ||
                                (!title_only && std::regex_search(item.first->text, re)))
                            {
                                results_.push_back(Result{item.first, std::string(static_cast<std::size_t>(item.second) * 2, ' ') + item.first->name});
                            }
                        }
                    }
                    catch (const std::regex_error&)
                    {
                        regex_error_ = true;
                    }
                }
                else
                {
                    for (const auto& item : all)
                    {
                        results_.push_back(Result{item.first, std::string(static_cast<std::size_t>(item.second) * 2, ' ') + item.first->name});
                    }
                }
            }
            else
            {
                const std::string needle = Lower(query);
                for (const auto& item : all)
                {
                    if (needle.empty() ||
                        Lower(item.first->name).find(needle) != std::string::npos ||
                        (!title_only && Lower(item.first->text).find(needle) != std::string::npos))
                    {
                        results_.push_back(Result{item.first, std::string(static_cast<std::size_t>(item.second) * 2, ' ') + item.first->name});
                    }
                }
            }

            selection_ = 0;
            scroll_ = 0;
            results_valid_ = true;
        }

        std::shared_ptr<EditorState> state_;
        bool* show_;
        std::string filter_;
        int selection_ = 0;
        int scroll_ = 0;
        int content_width_ = 72;
        bool results_valid_ = false;
        bool regex_error_ = false;
        std::vector<Result> results_;
        static constexpr int kVisibleRows = 18;
    };

    ftxui::Component MakeSearchDialog(std::shared_ptr<EditorState> state, bool* show)
    {
        return ftxui::Make<SearchDialog>(std::move(state), show);
    }
}
