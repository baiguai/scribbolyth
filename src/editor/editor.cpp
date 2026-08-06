#include "editor.hpp"

#include <algorithm>
#include <string>
#include <vector>

#include <ftxui/dom/elements.hpp>

#include "../bookmark/bookmark.hpp"
#include "../history/history.hpp"

namespace scribbolyth::editor
{

    namespace
    {
        bool IsBlank(const std::string& s)
        {
            for (char c : s)
            {
                if (c != ' ' && c != '\t' && c != '\n' && c != '\r') return false;
            }
            return true;
        }

        bool IsVisualMode(Mode m)
        {
            return m == Mode::VISUAL || m == Mode::VISUAL_LINE;
        }

        std::string TrimBoth(const std::string& s)
        {
            std::size_t start = 0;
            while (start < s.size() && (s[start] == ' ' || s[start] == '\t')) ++start;
            std::size_t end = s.size();
            while (end > start && (s[end - 1] == ' ' || s[end - 1] == '\t')) --end;
            return s.substr(start, end - start);
        }

        // A markdown table separator row: "| --- | --- |", "+-----+-----+" or
        // a bare "----" (the web app's dividerPattern).
        bool IsDividerLine(const std::string& s)
        {
            std::string t;
            t.reserve(s.size());
            for (char c : s)
            {
                if (c != ' ' && c != '\t') t += c;
            }
            if (t.empty()) return false;
            std::size_t i = 0;
            if (t[0] == '|' || t[0] == '+')
            {
                const char left = t[0];
                while (i < t.size() && (t[i] == '|' || t[i] == '+' || t[i] == '-')) ++i;
                return i == t.size() && t.back() == left;
            }
            while (i < t.size() && t[i] == '-') ++i;
            return i == t.size();
        }

        std::vector<std::string> SplitCells(const std::string& line)
        {
            std::vector<std::string> cells;
            std::size_t start = 0;
            while (true)
            {
                std::size_t bar = line.find('|', start);
                if (bar == std::string::npos)
                {
                    cells.push_back(line.substr(start));
                    break;
                }
                cells.push_back(line.substr(start, bar - start));
                start = bar + 1;
            }
            while (!cells.empty() && TrimBoth(cells.front()).empty())
            {
                cells.erase(cells.begin());
            }
            while (!cells.empty() && TrimBoth(cells.back()).empty())
            {
                cells.pop_back();
            }
            return cells;
        }
    }

    class Editor : public ftxui::ComponentBase
    {
        public:
            Editor(std::shared_ptr<EditorState> state) : state_(std::move(state))
            {
                state_->operations["cursor_up"] = [this](const std::string&, int count)
                {
                    if (!Editable()) return;
                    for (int i = 0; i < count; ++i) CursorUp();
                    Clamp();
                };
                state_->operations["cursor_down"] = [this](const std::string&, int count)
                {
                    if (!Editable()) return;
                    for (int i = 0; i < count; ++i) CursorDown();
                    Clamp();
                };
                state_->operations["cursor_left"] = [this](const std::string&, int count)
                {
                    if (!Editable()) return;
                    for (int i = 0; i < count; ++i) CursorLeft();
                    Clamp();
                };
                state_->operations["cursor_right"] = [this](const std::string&, int count)
                {
                    if (!Editable()) return;
                    for (int i = 0; i < count; ++i) CursorRight();
                    Clamp();
                };
                state_->operations["cursor_page_up"] = [this](const std::string&, int count)
                {
                    if (!Editable()) return;
                    for (int i = 0; i < count; ++i) CursorPageUp();
                    Clamp();
                };
                state_->operations["cursor_page_down"] = [this](const std::string&, int count)
                {
                    if (!Editable()) return;
                    for (int i = 0; i < count; ++i) CursorPageDown();
                    Clamp();
                };
                state_->operations["cursor_line_start"] = [this](const std::string&, int)
                {
                    if (!Editable()) return;
                    col_ = 0;
                };
                state_->operations["cursor_line_end"] = [this](const std::string&, int)
                {
                    if (!Editable()) return;
                    col_ = static_cast<int>(lines_[row_].size());
                };
                state_->operations["cursor_word_forward"] = [this](const std::string&, int count)
                {
                    if (!Editable()) return;
                    for (int i = 0; i < count; ++i) CursorWordForward();
                };
                state_->operations["cursor_word_back"] = [this](const std::string&, int count)
                {
                    if (!Editable()) return;
                    for (int i = 0; i < count; ++i) CursorWordBack();
                };
                state_->operations["cursor_file_start"] = [this](const std::string&, int)
                {
                    if (!Editable()) return;
                    row_ = 0;
                    col_ = 0;
                    last_col_ = 0;
                };
                state_->operations["cursor_file_end"] = [this](const std::string&, int)
                {
                    if (!Editable()) return;
                    row_ = static_cast<int>(lines_.size()) - 1;
                    col_ = static_cast<int>(lines_[row_].size());
                    last_col_ = col_;
                };

                state_->operations["insert_newline"] = [this](const std::string&, int)
                {
                    if (!Editable()) return;
                    InsertNewline();
                    Clamp();
                    Save();
                };
                state_->operations["insert_tab"] = [this](const std::string&, int)
                {
                    if (!Editable()) return;
                    InsertText("    ");
                    Save();
                };
                state_->operations["backspace_char"] = [this](const std::string&, int count)
                {
                    if (!Editable()) return;
                    for (int i = 0; i < count; ++i) Backspace();
                    Clamp();
                    Save();
                };
                state_->operations["delete_char"] = [this](const std::string&, int count)
                {
                    if (!Editable()) return;
                    for (int i = 0; i < count; ++i) DeleteChar();
                    Clamp();
                    Save();
                };
                state_->operations["delete_line"] = [this](const std::string&, int count)
                {
                    if (!Editable()) return;
                    for (int i = 0; i < count; ++i) DeleteLine();
                    Clamp();
                    Save();
                };
                state_->operations["delete_selection"] = [this](const std::string&, int)
                {
                    DeleteSelection();
                };

                state_->operations["yank"] = [](const std::string&, int) {};
                state_->operations["paste"] = [](const std::string&, int) {};
                state_->operations["format_table"] = [this](const std::string&, int)
                {
                    FormatTable();
                };

                state_->reveal_line = [this](int line)
                {
                    LoadIfChanged();
                    if (active_ == nullptr) return;
                    row_ = std::max(0, std::min(line, static_cast<int>(lines_.size()) - 1));
                    col_ = 0;
                    last_col_ = 0;
                };

                state_->operations["bookmark"] = [this](const std::string& args, int)
                {
                    if (state_->active_node == nullptr) return;
                    if (state_->active_node->id.empty())
                    {
                        state_->active_node->id = scribbolyth::bookmark::NewId();
                    }

                    scribbolyth::bookmark::Bookmark mark;
                    mark.id = state_->active_node->id;
                    if (args == "cursor" && !IsBlank(state_->active_node->text))
                    {
                        mark.line = row_;
                    }

                    state_->bookmarks.push_back(std::move(mark));
                    if (mark.line >= 0)
                    {
                        state_->status = "Bookmarked: " + state_->active_node->name
                            + " (line " + std::to_string(row_ + 1) + ")";
                    }
                    else
                    {
                        state_->status = "Bookmarked: " + state_->active_node->name;
                    }
                };
            }
            ftxui::Element Render() override
            {
                LoadIfChanged();
                if (active_ == nullptr)
                {
                    return ftxui::text("Select a node to edit") | ftxui::dim | ftxui::center;
                }

                ftxui::Elements rows;
                const bool sel_active = IsVisualMode(state_->mode) && visual_row_ >= 0;
                const int lo = sel_active ? std::min(visual_row_, row_) : 0;
                const int hi = sel_active ? std::max(visual_row_, row_) : -1;
                for (int r = 0; r < static_cast<int>(lines_.size()); ++r)
                {
                    const bool selected = r >= lo && r <= hi;
                    if (r == row_)
                    {
                        int c = std::max(0, col_);
                        std::string pre = lines_[r].substr(0, static_cast<std::size_t>(c));
                        std::string at = (c < static_cast<int>(lines_[r].size()))
                                             ? lines_[r].substr(static_cast<std::size_t>(c), 1)
                                             : " ";
                        std::string post = (c < static_cast<int>(lines_[r].size()))
                                               ? lines_[r].substr(static_cast<std::size_t>(c) + 1)
                                               : "";
                        rows.push_back(ftxui::hbox({
                            ftxui::text(pre) | (selected ? ftxui::inverted : ftxui::nothing),
                            ftxui::text(at) | ftxui::inverted | ftxui::focus,
                            ftxui::text(post) | (selected ? ftxui::inverted : ftxui::nothing),
                        }));
                    }
                    else if (selected)
                    {
                        rows.push_back(ftxui::text(lines_[r]) | ftxui::inverted);
                    }
                    else
                    {
                        rows.push_back(ftxui::text(lines_[r]));
                    }
                }
                return ftxui::vbox(std::move(rows)) | ftxui::frame | ftxui::flex;
            }
            bool Focusable() const override
            {
                return true;
            }
            bool OnEvent(ftxui::Event event) override
            {
                const bool visual_before = IsVisualMode(state_->mode);
                if (scribbolyth::op::HandleKey(state_, event))
                {
                    if (!visual_before && IsVisualMode(state_->mode))
                    {
                        visual_row_ = row_;
                    }
                    if (visual_before && !IsVisualMode(state_->mode))
                    {
                        visual_row_ = -1;
                    }
                    return true;
                }
                if (state_->mode == Mode::INSERT && event.is_character())
                {
                    LoadIfChanged();
                    if (!Editable()) return false;
                    InsertText(event.character());
                    Save();
                    return true;
                }
                return false;
            }

        private:
            bool Editable()
            {
                LoadIfChanged();
                return active_ != nullptr
                    && state_->mode != Mode::TREE
                    && state_->mode != Mode::COMMAND;
            }

            void LoadIfChanged()
            {
                if (active_ == state_->active_node) return;
                active_ = state_->active_node;
                visual_row_ = -1;
                if (active_ == nullptr)
                {
                    lines_.clear();
                    row_ = 0;
                    col_ = 0;
                    last_col_ = 0;
                    return;
                }
                lines_ = SplitLines(active_->text);
                row_ = 0;
                col_ = 0;
                last_col_ = 0;
            }

            void Save()
            {
                if (active_ == nullptr) return;
                if (state_->snapshot_undo) state_->snapshot_undo();
                std::string joined;
                for (std::size_t i = 0; i < lines_.size(); ++i)
                {
                    if (i != 0) joined += '\n';
                    joined += lines_[i];
                }
                active_->text = std::move(joined);
                if (!active_->text.empty())
                {
                    scribbolyth::history::Record(*state_, active_->id);
                }
            }

            // Rebuild the selected rows (or the current line when no VISUAL
            // selection is active) into a padded markdown table with a
            // "+---+" separator, mirroring the web app's formatMarkdownTable.
            void FormatTable()
            {
                if (active_ == nullptr) return;
                LoadIfChanged();
                if (lines_.empty()) lines_.push_back("");

                int a = (visual_row_ >= 0) ? std::min(visual_row_, row_) : row_;
                int b = (visual_row_ >= 0) ? std::max(visual_row_, row_) : row_;
                if (a > b) std::swap(a, b);

                // Line-based analog of the web app's sel.trim(): drop blank
                // lines hugging the selection edges.
                while (a <= b && IsBlank(lines_[a])) ++a;
                while (b >= a && IsBlank(lines_[b])) --b;
                if (a > b)
                {
                    state_->status = "No table detected";
                    return;
                }

                bool has_pipe = false;
                for (int r = a; r <= b; ++r)
                {
                    if (lines_[r].find('|') != std::string::npos)
                    {
                        has_pipe = true;
                        break;
                    }
                }
                if (!has_pipe)
                {
                    state_->status = "No table detected";
                    return;
                }

                bool has_divider = false;
                std::vector<std::vector<std::string>> table;
                table.reserve(static_cast<std::size_t>(b - a + 1));
                for (int r = a; r <= b; ++r)
                {
                    if (IsDividerLine(lines_[r]))
                    {
                        has_divider = true;
                        continue;
                    }
                    table.push_back(SplitCells(lines_[r]));
                }
                if (table.empty())
                {
                    state_->status = "No table detected";
                    return;
                }

                std::vector<int> widths;
                for (const auto& row : table)
                {
                    for (std::size_t i = 0; i < row.size(); ++i)
                    {
                        const int w = static_cast<int>(TrimBoth(row[i]).size());
                        if (i >= widths.size()) widths.resize(i + 1, 0);
                        if (w > widths[i]) widths[i] = w;
                    }
                }

                std::string sep = "+";
                for (const int w : widths)
                {
                    sep += std::string(static_cast<std::size_t>(w) + 2, '-') + "+";
                }

                std::vector<std::string> padded;
                padded.reserve(table.size());
                for (const auto& row : table)
                {
                    std::string out = "|";
                    for (std::size_t i = 0; i < row.size(); ++i)
                    {
                        if (i != 0) out += "|";
                        const std::string t = TrimBoth(row[i]);
                        const int pad = (i < widths.size())
                                            ? widths[i] - static_cast<int>(t.size())
                                            : 0;
                        out += " " + t
                            + std::string(static_cast<std::size_t>(pad > 0 ? pad : 0), ' ')
                            + " ";
                    }
                    out += "|";
                    padded.push_back(std::move(out));
                }

                std::vector<std::string> final_rows;
                if (!has_divider)
                {
                    final_rows = padded;
                }
                else
                {
                    final_rows.reserve(padded.size() + 1);
                    std::size_t row_index = 0;
                    for (int r = a; r <= b; ++r)
                    {
                        if (IsDividerLine(lines_[r]))
                        {
                            final_rows.push_back(sep);
                        }
                        else
                        {
                            final_rows.push_back(padded[row_index++]);
                        }
                    }
                }
                final_rows.push_back("");

                lines_.erase(lines_.begin() + a, lines_.begin() + b + 1);
                lines_.insert(lines_.begin() + a, final_rows.begin(), final_rows.end());
                row_ = a;
                col_ = 0;
                visual_row_ = -1;
                state_->mode = Mode::NORMAL;
                Save();
                state_->status = "Table updated";
            }

            static std::vector<std::string> SplitLines(const std::string& text)
            {
                std::vector<std::string> out;
                std::size_t start = 0;
                while (start <= text.size())
                {
                    std::size_t nl = text.find('\n', start);
                    if (nl == std::string::npos)
                    {
                        out.push_back(text.substr(start));
                        break;
                    }
                    out.push_back(text.substr(start, nl - start));
                    start = nl + 1;
                }
                if (out.empty()) out.push_back("");
                return out;
            }

            void Clamp()
            {
                if (lines_.empty()) lines_.push_back("");
                row_ = std::max(0, std::min(row_, static_cast<int>(lines_.size()) - 1));
                col_ = std::max(0, std::min(col_, static_cast<int>(lines_[row_].size())));
            }

            void CursorUp()
            {
                if (row_ > 0)
                {
                    last_col_ = col_;
                    --row_;
                    col_ = std::min(last_col_, static_cast<int>(lines_[row_].size()));
                }
                else
                {
                    col_ = 0;
                }
            }

            void CursorDown()
            {
                if (row_ < static_cast<int>(lines_.size()) - 1)
                {
                    last_col_ = col_;
                    ++row_;
                    col_ = std::min(last_col_, static_cast<int>(lines_[row_].size()));
                }
            }

            void CursorLeft()
            {
                if (col_ > 0)
                {
                    --col_;
                }
                else if (row_ > 0)
                {
                    --row_;
                    col_ = static_cast<int>(lines_[row_].size());
                }
            }

            void CursorRight()
            {
                if (col_ < static_cast<int>(lines_[row_].size()))
                {
                    ++col_;
                }
                else if (row_ < static_cast<int>(lines_.size()) - 1)
                {
                    ++row_;
                    col_ = 0;
                }
            }

            void CursorPageUp()
            {
                for (int i = 0; i < 10; ++i) CursorUp();
            }

            void CursorPageDown()
            {
                for (int i = 0; i < 10; ++i) CursorDown();
            }

            void CursorWordForward()
            {
                auto& line = lines_[row_];
                while (col_ < static_cast<int>(line.size()) && line[col_] != ' ') ++col_;
                while (col_ < static_cast<int>(line.size()) && line[col_] == ' ') ++col_;
            }

            void CursorWordBack()
            {
                auto& line = lines_[row_];
                while (col_ > 0 && line[col_ - 1] == ' ') --col_;
                while (col_ > 0 && line[col_ - 1] != ' ') --col_;
            }

            void InsertText(const std::string& text)
            {
                lines_[row_].insert(static_cast<std::size_t>(col_), text);
                col_ += static_cast<int>(text.size());
            }

            void InsertNewline()
            {
                std::string& line = lines_[row_];
                std::size_t ws = 0;
                while (ws < line.size() && (line[ws] == ' ' || line[ws] == '\t')) ++ws;

                std::string rest = line.substr(static_cast<std::size_t>(col_));
                line = line.substr(0, static_cast<std::size_t>(col_));

                // Carry the current line's leading whitespace over to the new
                // line so an indented block keeps its shape (block indent on
                // Enter); the cursor lands right after the indentation.
                rest.insert(0, line.substr(0, ws));

                lines_.insert(lines_.begin() + row_ + 1, rest);
                ++row_;
                col_ = static_cast<int>(ws);
            }

            void Backspace()
            {
                if (col_ > 0)
                {
                    lines_[row_].erase(static_cast<std::size_t>(col_) - 1, 1);
                    --col_;
                }
                else if (row_ > 0)
                {
                    col_ = static_cast<int>(lines_[row_ - 1].size());
                    lines_[row_ - 1] += lines_[row_];
                    lines_.erase(lines_.begin() + row_);
                    --row_;
                }
            }

            void DeleteChar()
            {
                auto& line = lines_[row_];
                if (col_ < static_cast<int>(line.size()))
                {
                    line.erase(static_cast<std::size_t>(col_), 1);
                }
                else if (row_ < static_cast<int>(lines_.size()) - 1)
                {
                    lines_[row_] += lines_[row_ + 1];
                    lines_.erase(lines_.begin() + row_ + 1);
                }
            }

            void DeleteLine()
            {
                if (lines_.size() == 1)
                {
                    lines_[0].clear();
                    col_ = 0;
                    return;
                }
                lines_.erase(lines_.begin() + row_);
                if (row_ >= static_cast<int>(lines_.size())) --row_;
            }

            void DeleteSelection()
            {
                if (active_ == nullptr) return;
                LoadIfChanged();
                if (lines_.empty()) lines_.push_back("");

                int a = (visual_row_ >= 0) ? std::min(visual_row_, row_) : row_;
                int b = (visual_row_ >= 0) ? std::max(visual_row_, row_) : row_;
                if (a > b) std::swap(a, b);

                if (static_cast<int>(lines_.size()) == b - a + 1)
                {
                    lines_.assign(1, ""); // leave a blank line - if nothing is left
                                          // row_ = 0;
                }
                else
                {
                    lines_.erase(lines_.begin() + a, lines_.begin() + b + 1);
                    row_ = std::min(a, static_cast<int>(lines_.size()) - 1);
                }
                col_ = 0;
                visual_row_ = -1;
                state_->mode = Mode::NORMAL;
                Save();
                state_->status = "Selection deleted";
            }

            std::shared_ptr<EditorState> state_;
            scribbolyth::treeview::TreeNode* active_ = nullptr;
            std::vector<std::string> lines_;
            int row_ = 0;
            int col_ = 0;
            int last_col_ = 0;
            int visual_row_ = -1;
    };

    ftxui::Component MakeEditor(std::shared_ptr<EditorState> state)
    {
        return ftxui::Make<Editor>(std::move(state));
    }

}
