#include "editor.hpp"

#include <algorithm>
#include <string>
#include <vector>

#include <ftxui/dom/elements.hpp>

#include "../bookmark/bookmark.hpp"
#include "../clipboard/clipboard.hpp"
#include "../history/history.hpp"
#include "../search/search.hpp"
#include "../visual_block/visual_block.hpp"

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
            return m == Mode::VISUAL || m == Mode::VISUAL_LINE || m == Mode::VISUAL_BLOCK;
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

        // Read the system clipboard (Win32 API on Windows, wl-paste/xclip/xsel
        // on POSIX). Returns "" when nothing is available or the clipboard is
        // empty.
        std::string ReadClipboard()
        {
            return scribbolyth::clipboard::Read();
        }

        // Write to the system clipboard (Win32 API on Windows,
        // wl-copy/xclip/xsel on POSIX). Returns false when no tool is
        // available.
        bool WriteClipboard(const std::string& text)
        {
            return scribbolyth::clipboard::Write(text);
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
                state_->operations["cursor_word_end"] = [this](const std::string&, int count)
                {
                    if (!Editable()) return;
                    for (int i = 0; i < count; ++i) CursorWordEnd();
                };
                state_->operations["select_word"] = [this](const std::string&, int)
                {
                    if (!Editable()) return;
                    if (!IsVisualMode(state_->mode)) return;
                    SelectInnerWord();
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
                state_->operations["delete_to_eol"] = [this](const std::string&, int)
                {
                    if (!Editable()) return;
                    DeleteToEol();
                    Clamp();
                    Save();
                };
                state_->operations["delete_selection"] = [this](const std::string&, int)
                {
                    DeleteSelection();
                };
                state_->operations["upper_selection"] = [this](const std::string&, int)
                {
                    UpperSelection();
                };
                state_->operations["lower_selection"] = [this](const std::string&, int)
                {
                    LowerSelection();
                };
                state_->operations["block_insert_start"] = [this](const std::string&, int)
                {
                    if (!Editable()) return;
                    EnterBlockInsert(false);
                };
                state_->operations["block_insert_end"] = [this](const std::string&, int)
                {
                    if (!Editable()) return;
                    EnterBlockInsert(true);
                };
                state_->operations["block_file_start"] = [this](const std::string&, int)
                {
                    if (!Editable()) return;
                    row_ = 0;
                    col_ = std::min(col_, static_cast<int>(lines_[row_].size()));
                    last_col_ = col_;
                };
                state_->operations["block_file_end"] = [this](const std::string&, int)
                {
                    if (!Editable()) return;
                    row_ = static_cast<int>(lines_.size()) - 1;
                    col_ = std::min(col_, static_cast<int>(lines_[row_].size()));
                    last_col_ = col_;
                };
                state_->operations["block_to_eol"] = [this](const std::string&, int)
                {
                    if (!Editable()) return;
                    const int b_lo = std::min(visual_row_, row_);
                    const int b_hi = std::max(visual_row_, row_);
                    int widest = 0;
                    for (int r = b_lo; r <= b_hi; ++r)
                    {
                        widest = std::max(widest, static_cast<int>(lines_[r].size()));
                    }
                    col_ = widest;
                    last_col_ = col_;
                };

                state_->operations["yank"] = [this](const std::string&, int)
                {
                    if (active_ == nullptr) return;
                    LoadIfChanged();
                    const std::string text = SelectionText();
                    if (text.empty())
                    {
                        state_->status = "Nothing to copy";
                        return;
                    }
                    if (!WriteClipboard(text))
                    {
                        state_->status = "Clipboard unavailable";
                        return;
                    }
                    if (IsVisualMode(state_->mode))
                    {
                        visual_row_ = -1;
                        visual_col_ = -1;
                        state_->mode = Mode::NORMAL;
                    }
                    state_->status = "Copied";
                };
                state_->operations["cut"] = [this](const std::string&, int)
                {
                    if (active_ == nullptr) return;
                    LoadIfChanged();
                    if (lines_.empty()) lines_.push_back("");
                    const bool had_visual = IsVisualMode(state_->mode) && visual_row_ >= 0;
                    const std::string text = SelectionText();
                    if (text.empty())
                    {
                        state_->status = "Nothing to cut";
                        return;
                    }
                    if (!WriteClipboard(text))
                    {
                        state_->status = "Clipboard unavailable";
                        return;
                    }
                    if (had_visual)
                    {
                        DeleteSelection();
                        state_->status = "Cut";
                        return;
                    }
                    DeleteLine();
                    Clamp();
                    Save();
                    state_->status = "Cut";
                };
                state_->operations["paste"] = [this](const std::string&, int)
                {
                    if (!Editable()) return;
                    LoadIfChanged();
                    const std::string clip = ReadClipboard();
                    if (clip.empty())
                    {
                        state_->status = "Clipboard is empty";
                        return;
                    }
                    PasteText(clip);
                    Clamp();
                    Save();
                    state_->status = "Pasted";
                };
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
                    visual_row_ = -1;
                    visual_col_ = -1;
                };

                state_->search_jump = [this](int dir)
                {
                    StepSearchOccurrence(dir);
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
                if (state_->search_reveal_pending)
                {
                    state_->search_reveal_pending = false;
                    JumpToFirstSearchMatch();
                }

                ftxui::Elements rows;
                const bool sel_active = IsVisualMode(state_->mode) && visual_row_ >= 0;
                const bool line_visual = sel_active && state_->mode == Mode::VISUAL_LINE;
                const int lo = line_visual ? std::min(visual_row_, row_) : 0;
                const int hi = line_visual ? std::max(visual_row_, row_) : -1;

                // Character-wise selection range (VISUAL): the anchor row/col
                // and the cursor row/col, normalized so the first row/col is
                // the upper endpoint. On the last row the upper column is
                // exclusive, mirroring the cursor semantics (col_ is the index
                // of the char under the cursor, and the line end sits past the
                // final char).
                const bool char_sel = sel_active && !line_visual && visual_col_ >= 0;
                int c_first = visual_row_, c_last = row_;
                int c_first_col = visual_col_, c_last_col = col_;
                if (char_sel && (c_first > c_last
                                 || (c_first == c_last && c_first_col > c_last_col)))
                {
                    std::swap(c_first, c_last);
                    std::swap(c_first_col, c_last_col);
                }
                // Search-match highlighting: when the active node is one of
                // the find matches, every occurrence of the query in its text
                // is highlighted so the '/' find is visible in the editor too.
                const bool node_is_search_match =
                    state_->search_active
                    && std::find(state_->search_matches.begin(),
                                 state_->search_matches.end(), active_)
                           != state_->search_matches.end();

                auto RowHighlight = [&](int r, int& h_lo, int& h_hi) -> bool
                {
                    // Column block (Ctrl+V): a rectangle whose rows run from
                    // the anchor to the cursor and whose columns run from the
                    // smaller of the two columns to the larger (inclusive).
                    if (state_->mode == Mode::VISUAL_BLOCK
                        && visual_row_ >= 0 && visual_col_ >= 0)
                    {
                        const int b_lo = std::min(visual_row_, row_);
                        const int b_hi = std::max(visual_row_, row_);
                        const int b_col_lo = std::min(visual_col_, col_);
                        const int b_col_hi = std::max(visual_col_, col_);
                        if (r < b_lo || r > b_hi) return false;
                        const int size = static_cast<int>(lines_[r].size());
                        h_lo = std::min(b_col_lo, size);
                        h_hi = std::min(b_col_hi + 1, size);
                        return h_lo < h_hi;
                    }
                    if (line_visual)
                    {
                        if (r < lo || r > hi) return false;
                        h_lo = 0;
                        h_hi = static_cast<int>(lines_[r].size());
                        return true;
                    }
                    if (!char_sel || r < c_first || r > c_last) return false;
                    const int size = static_cast<int>(lines_[r].size());
                    if (c_first == c_last)
                    {
                        h_lo = std::min(c_first_col, c_last_col);
                        h_hi = std::min(std::max(c_first_col, c_last_col) + 1, size);
                    }
                    else if (r == c_first)
                    {
                        h_lo = c_first_col;
                        h_hi = size;
                    }
                    else if (r == c_last)
                    {
                        h_lo = 0;
                        h_hi = std::min(c_last_col, size);
                    }
                    else
                    {
                        h_lo = 0;
                        h_hi = size;
                    }
                    return h_lo < h_hi;
                };

                for (int r = 0; r < static_cast<int>(lines_.size()); ++r)
                {
                    const int size = static_cast<int>(lines_[r].size());
                    int h_lo = 0, h_hi = -1;
                    const bool highlighted = RowHighlight(r, h_lo, h_hi);
                    const int c = (r == row_) ? std::max(0, col_) : -1;

                    // Split the line at every boundary that changes the
                    // styling (selection start/end and the cursor char) so a
                    // partially selected line still renders correctly.
                    std::vector<int> cuts;
                    cuts.reserve(6);
                    cuts.push_back(0);
                    if (highlighted)
                    {
                        cuts.push_back(h_lo);
                        cuts.push_back(h_hi);
                    }
                    if (c >= 0)
                    {
                        cuts.push_back(c);
                        cuts.push_back(std::min(c + 1, size));
                    }
                    std::vector<std::pair<int, int>> line_matches;
                    if (node_is_search_match)
                    {
                        line_matches = scribbolyth::search::FindLineMatches(
                            lines_[r], state_->search_query);
                        for (const auto& m : line_matches)
                        {
                            cuts.push_back(m.first);
                            cuts.push_back(m.second);
                        }
                    }
                    cuts.push_back(size);
                    std::sort(cuts.begin(), cuts.end());
                    cuts.erase(std::unique(cuts.begin(), cuts.end()), cuts.end());

                    ftxui::Elements parts;
                    for (std::size_t i = 0; i + 1 < cuts.size(); ++i)
                    {
                        const int a = cuts[i];
                        const int b = cuts[i + 1];
                        if (b <= a) continue;
                        const bool cursor_char = (c >= 0 && a == c && b == c + 1);
                        const bool selected = highlighted && a >= h_lo && b <= h_hi;
                        const bool match = std::any_of(
                            line_matches.begin(), line_matches.end(),
                            [&](const auto& m) { return a >= m.first && b <= m.second; });
                        ftxui::Element el = ftxui::text(lines_[r].substr(
                            static_cast<std::size_t>(a), static_cast<std::size_t>(b - a)));
                        if (cursor_char)
                        {
                            el = el | ftxui::inverted | ftxui::focus;
                        }
                        else if (selected)
                        {
                            el = el | ftxui::inverted;
                        }
                        else if (match)
                        {
                            el = el | ftxui::bgcolor(ftxui::Color::Yellow);
                        }
                        parts.push_back(std::move(el));
                    }
                    if (c >= size)
                    {
                        parts.push_back(ftxui::text(" ") | ftxui::inverted | ftxui::focus);
                    }
                    else if (parts.empty())
                    {
                        ftxui::Element sp = ftxui::text(" ");
                        if (highlighted) sp = sp | ftxui::inverted;
                        parts.push_back(std::move(sp));
                    }
                    rows.push_back(ftxui::hbox(std::move(parts)));
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

                // While a block insert (Ctrl+V then I/A) is pending, typed
                // characters are buffered instead of being handled by the
                // regular INSERT path, so the whole block can be updated in a
                // single step when Esc applies the insertion.
                if (block_insert_.active)
                {
                    if (event == ftxui::Event::Backspace)
                    {
                        if (!block_insert_.pending.empty())
                        {
                            block_insert_.pending.pop_back();
                            Backspace();
                        }
                        return true;
                    }
                    if (event.is_character())
                    {
                        block_insert_.pending += event.character();
                        InsertText(event.character());
                        return true;
                    }
                }

                if (scribbolyth::op::HandleKey(state_, event))
                {
                    if (!visual_before && IsVisualMode(state_->mode))
                    {
                        visual_row_ = row_;
                        visual_col_ = col_;
                    }
                    if (visual_before && !IsVisualMode(state_->mode))
                    {
                        visual_row_ = -1;
                        visual_col_ = -1;
                    }
                    if (block_insert_.active && state_->mode == Mode::NORMAL)
                    {
                        ApplyBlockInsert();
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
                visual_col_ = -1;
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
                JumpToFirstSearchMatch();
            }

            // Move the cursor to the first occurrence of the active search
            // query in the freshly loaded node, so the '/' find and n/N
            // navigation reveal the match in the editor (as in Vim).
            void JumpToFirstSearchMatch()
            {
                if (!state_->search_active || active_ == nullptr) return;
                if (std::find(state_->search_matches.begin(),
                              state_->search_matches.end(), active_)
                    == state_->search_matches.end())
                {
                    return;
                }
                for (std::size_t r = 0; r < lines_.size(); ++r)
                {
                    const auto ranges =
                        scribbolyth::search::FindLineMatches(lines_[r],
                                                             state_->search_query);
                    if (!ranges.empty())
                    {
                        SetCursor(static_cast<int>(r), ranges[0].first);
                        return;
                    }
                }
            }

            // Place the cursor on (row, col), clamped to the document.
            void SetCursor(int row, int col)
            {
                if (lines_.empty()) return;
                row_ = std::max(0, std::min(row, static_cast<int>(lines_.size()) - 1));
                col_ = std::max(0, std::min(col, static_cast<int>(lines_[static_cast<std::size_t>(row_)].size())));
                last_col_ = col_;
                visual_row_ = -1;
                visual_col_ = -1;
            }

            // Step the cursor to the next (dir > 0) or previous (dir < 0)
            // occurrence of the active search query inside the current node's
            // text, wrapping around like Vim's n/N. Also works while the
            // highlight is hidden by ':noh' (the query is kept). Does nothing
            // when there is no search or the node has no occurrences.
            void StepSearchOccurrence(int dir)
            {
                if (active_ == nullptr) return;
                if (state_->search_query.empty())
                {
                    state_->status = "No search";
                    return;
                }

                struct Occ
                {
                    int line;
                    int col;
                };
                std::vector<Occ> occs;
                for (std::size_t r = 0; r < lines_.size(); ++r)
                {
                    const auto ranges =
                        scribbolyth::search::FindLineMatches(lines_[r],
                                                             state_->search_query);
                    for (const auto& m : ranges)
                    {
                        occs.push_back(Occ{static_cast<int>(r), m.first});
                    }
                }
                if (occs.empty())
                {
                    state_->status = "No matches in this node";
                    return;
                }

                if (dir > 0)
                {
                    for (const auto& o : occs)
                    {
                        if (o.line > row_ || (o.line == row_ && o.col > col_))
                        {
                            SetCursor(o.line, o.col);
                            return;
                        }
                    }
                    SetCursor(occs.front().line, occs.front().col);
                }
                else
                {
                    for (auto it = occs.rbegin(); it != occs.rend(); ++it)
                    {
                        if (it->line < row_ || (it->line == row_ && it->col < col_))
                        {
                            SetCursor(it->line, it->col);
                            return;
                        }
                    }
                    SetCursor(occs.back().line, occs.back().col);
                }
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
                visual_col_ = -1;
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

            // ASCII-only case folding: the editor is byte-oriented (columns =
            // bytes), so folding a full byte would corrupt UTF-8 text. Other
            // bytes (letters with diacritics, CJK, emoji, ...) are left as-is.
            static char ToUpperAscii(char c)
            {
                return (c >= 'a' && c <= 'z') ? static_cast<char>(c - 'a' + 'A') : c;
            }

            static char ToLowerAscii(char c)
            {
                return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
            }

            void Clamp()
            {
                if (lines_.empty()) lines_.push_back("");
                row_ = std::max(0, std::min(row_, static_cast<int>(lines_.size()) - 1));
                if (state_->mode != Mode::VISUAL_BLOCK)
                    col_ = std::max(0, std::min(col_, static_cast<int>(lines_[row_].size())));
            }

            void CursorUp()
            {
                if (row_ > 0)
                {
                    last_col_ = col_;
                    --row_;
                    if (state_->mode != Mode::VISUAL_BLOCK)
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
                    if (state_->mode != Mode::VISUAL_BLOCK)
                        col_ = std::min(last_col_, static_cast<int>(lines_[row_].size()));
                }
            }

            void CursorLeft()
            {
                if (state_->mode == Mode::VISUAL_BLOCK)
                {
                    col_ = std::max(0, col_ - 1);
                    return;
                }
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
                if (state_->mode == Mode::VISUAL_BLOCK)
                {
                    ++col_;
                    return;
                }
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

            void CursorWordEnd()
            {
                auto& line = lines_[row_];
                const int size = static_cast<int>(line.size());
                if (col_ >= size) return;
                while (col_ < size && line[col_] == ' ') ++col_;
                if (col_ >= size) return;
                if (col_ + 1 >= size || line[col_ + 1] == ' ')
                {
                    while (col_ < size && line[col_] != ' ') ++col_;
                    while (col_ < size && line[col_] == ' ') ++col_;
                    if (col_ >= size) return;
                }
                while (col_ + 1 < size && line[col_] != ' ' && line[col_ + 1] != ' ') ++col_;
            }

            void SelectInnerWord()
            {
                auto& line = lines_[row_];
                const int size = static_cast<int>(line.size());
                int c = col_;
                if (c >= size) c = size - 1;
                if (c < 0) return;
                if (line[c] == ' ')
                {
                    while (c < size && line[c] == ' ') ++c;
                }
                if (c >= size)
                {
                    state_->status = "No word here";
                    return;
                }
                int start = c;
                int end = c;
                while (start > 0 && line[start - 1] != ' ') --start;
                while (end < size && line[end] != ' ') ++end;

                state_->mode = Mode::VISUAL;
                visual_row_ = row_;
                visual_col_ = start;
                col_ = end -1;
            }

            void InsertText(const std::string& text)
            {
                lines_[row_].insert(static_cast<std::size_t>(col_), text);
                col_ += static_cast<int>(text.size());
            }

            // Insert clipboard-style text (possibly multi-line) at the cursor.
            void PasteText(const std::string& text)
            {
                std::vector<std::string> parts = SplitLines(text);
                if (parts.empty()) parts.push_back("");

                if (parts.size() == 1)
                {
                    InsertText(parts[0]);
                    return;
                }

                std::string head = lines_[row_].substr(0, static_cast<std::size_t>(col_));
                std::string tail = lines_[row_].substr(static_cast<std::size_t>(col_));
                lines_[row_] = head + parts[0];
                if (parts.size() > 2)
                {
                    lines_.insert(lines_.begin() + row_ + 1,
                                  parts.begin() + 1, parts.end() - 1);
                }
                lines_.insert(lines_.begin() + row_ + 1 + (parts.size() - 2),
                              parts.back() + tail);
                row_ += static_cast<int>(parts.size()) - 1;
                col_ = static_cast<int>(parts.back().size());
                last_col_ = col_;
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

            // Enter INSERT mode with a pending block insert (Vim Ctrl+V then
            // I/A). The text the user types is accumulated in block_insert_ and
            // shown live on the top row; ApplyBlockInsert() replays it on the
            // remaining rows when Esc is pressed.
            void EnterBlockInsert(bool at_end)
            {
                if (!(state_->mode == Mode::VISUAL_BLOCK
                      && visual_row_ >= 0 && visual_col_ >= 0))
                {
                    return;
                }
                const auto block =
                    visual_block::MakeBlock(visual_row_, visual_col_, row_, col_);
                block_insert_.active = true;
                block_insert_.at_end = at_end;
                block_insert_.col = at_end ? block.col_hi + 1 : block.col_lo;
                block_insert_.pending.clear();
                block_insert_.rows.clear();
                for (int r = block.row_lo; r <= block.row_hi; ++r)
                {
                    block_insert_.rows.push_back(r);
                }
                row_ = block.row_lo;
                col_ = at_end
                           ? std::min(block.col_hi + 1,
                                      static_cast<int>(lines_[row_].size()))
                           : std::min(block.col_lo,
                                      static_cast<int>(lines_[row_].size()));
                last_col_ = col_;
                visual_row_ = -1;
                visual_col_ = -1;
                state_->mode = Mode::INSERT;
            }

            void ApplyBlockInsert()
            {
                if (!block_insert_.active) return;
                block_insert_.active = false;
                if (!block_insert_.pending.empty())
                {
                    for (const int r : block_insert_.rows)
                    {
                        if (r == row_) continue; // already edited live while typing
                        if (block_insert_.at_end)
                        {
                            std::string& line = lines_[r];
                            line.insert(std::min(block_insert_.col,
                                                 static_cast<int>(line.size())),
                                        block_insert_.pending);
                        }
                        else
                        {
                            std::string& line = lines_[r];
                            if (static_cast<int>(line.size()) < block_insert_.col)
                            {
                                line.append(static_cast<std::size_t>(block_insert_.col) -
                                                static_cast<std::size_t>(line.size()),
                                            ' ');
                            }
                            line.insert(static_cast<std::size_t>(block_insert_.col),
                                        block_insert_.pending);
                        }
                    }
                    Save();
                    state_->status = "Block inserted";
                }
                block_insert_.pending.clear();
                block_insert_.rows.clear();
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

            // Vim 'D' = 'd$': delete from the cursor to the end of the line,
            // leaving the line itself (and the newline) in place.
            void DeleteToEol()
            {
                auto& line = lines_[row_];
                if (col_ < static_cast<int>(line.size()))
                {
                    line.erase(static_cast<std::size_t>(col_));
                }
            }

            // The text currently under a VISUAL/VISUAL_LINE selection, or the
            // current line when no selection is active (NORMAL/INSERT).
            // Line-based copies include a trailing newline so a cut+paste
            // round-trip reproduces the lines.
            std::string SelectionText() const
            {
                std::string out;
                if (state_->mode == Mode::VISUAL_BLOCK
                    && visual_row_ >= 0 && visual_col_ >= 0)
                {
                    return visual_block::Extract(
                        lines_,
                        visual_block::MakeBlock(visual_row_, visual_col_, row_, col_));
                }
                if (state_->mode == Mode::VISUAL && visual_row_ >= 0 && visual_col_ >= 0)
                {
                    int aRow = visual_row_, aCol = visual_col_;
                    int bRow = row_, bCol = col_;
                    if (aRow > bRow || (aRow == bRow && aCol > bCol))
                    {
                        std::swap(aRow, bRow);
                        std::swap(aCol, bCol);
                    }
                    if (aRow == bRow)
                    {
                        const int size = static_cast<int>(lines_[aRow].size());
                        aCol = std::min(aCol, size);
                        bCol = std::min(bCol + 1, size);
                        if (bCol > aCol)
                        {
                            out = lines_[aRow].substr(static_cast<std::size_t>(aCol),
                                                      static_cast<std::size_t>(bCol - aCol));
                        }
                        return out;
                    }
                    out += lines_[aRow].substr(static_cast<std::size_t>(aCol));
                    for (int r = aRow + 1; r < bRow; ++r)
                    {
                        out += '\n';
                        out += lines_[r];
                    }
                    out += '\n';
                    out += lines_[bRow].substr(0, static_cast<std::size_t>(std::min(
                        bCol, static_cast<int>(lines_[bRow].size()))));
                    return out;
                }
                if (IsVisualMode(state_->mode) && visual_row_ >= 0)
                {
                    int a = std::min(visual_row_, row_);
                    int b = std::max(visual_row_, row_);
                    for (int r = a; r <= b; ++r)
                    {
                        out += lines_[r];
                        out += '\n';
                    }
                    return out;
                }
                if (row_ >= 0 && row_ < static_cast<int>(lines_.size()))
                {
                    out = lines_[row_];
                    out += '\n';
                }
                return out;
            }

            void DeleteSelection()
            {
                if (active_ == nullptr) return;
                LoadIfChanged();
                if (lines_.empty()) lines_.push_back("");

                if (state_->mode == Mode::VISUAL && visual_row_ >= 0 && visual_col_ >= 0)
                {
                    int aRow = visual_row_, aCol = visual_col_;
                    int bRow = row_, bCol = col_;
                    if (aRow > bRow || (aRow == bRow && aCol > bCol))
                    {
                        std::swap(aRow, bRow);
                        std::swap(aCol, bCol);
                    }
                    if (aRow == bRow)
                    {
                        std::string& line = lines_[aRow];
                        const int size = static_cast<int>(line.size());
                        aCol = std::min(aCol, size);
                        bCol = std::min(bCol + 1, size);
                        if (bCol > aCol)
                        {
                            line.erase(static_cast<std::size_t>(aCol),
                                       static_cast<std::size_t>(bCol - aCol));
                        }
                    }
                    else
                    {
                        std::string head = lines_[aRow].substr(
                            0, static_cast<std::size_t>(aCol));
                        std::string tail = lines_[bRow].substr(static_cast<std::size_t>(
                            std::min(bCol, static_cast<int>(lines_[bRow].size()))));
                        lines_.erase(lines_.begin() + aRow, lines_.begin() + bRow + 1);
                        lines_.insert(lines_.begin() + aRow, head + tail);
                    }
                    if (lines_.empty()) lines_.push_back("");
                    row_ = std::min(aRow, static_cast<int>(lines_.size()) - 1);
                    col_ = std::min(aCol, static_cast<int>(lines_[row_].size()));
                    visual_row_ = -1;
                    visual_col_ = -1;
                    state_->mode = Mode::NORMAL;
                    Save();
                    state_->status = "Selection deleted";
                    return;
                }

                if (state_->mode == Mode::VISUAL_BLOCK
                    && visual_row_ >= 0 && visual_col_ >= 0)
                {
                    const auto block =
                        visual_block::MakeBlock(visual_row_, visual_col_, row_, col_);
                    visual_block::Erase(lines_, block);
                    row_ = block.row_lo;
                    col_ = std::min(block.col_lo,
                                    static_cast<int>(lines_[row_].size()));
                    visual_row_ = -1;
                    visual_col_ = -1;
                    state_->mode = Mode::NORMAL;
                    Clamp();
                    Save();
                    state_->status = "Selection deleted";
                    return;
                }

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
                visual_col_ = -1;
                state_->mode = Mode::NORMAL;
                Save();
                state_->status = "Selection deleted";
            }

            // Vim 'U' (VISUAL): uppercase the selected text. Folding never
            // changes line count or line length, so nothing is erased or
            // reassembled; the cursor is placed at the start of the selection
            // and the mode returns to NORMAL.
            void UpperSelection()
            {
                TransformSelection([](char c) { return ToUpperAscii(c); },
                                   "Selection uppercased");
            }

            // Vim 'u' (VISUAL): lowercase the selected text.
            void LowerSelection()
            {
                TransformSelection([](char c) { return ToLowerAscii(c); },
                                   "Selection lowercased");
            }

            void TransformSelection(char (*fold)(char), const char* status_msg)
            {
                if (active_ == nullptr) return;
                LoadIfChanged();
                if (lines_.empty()) lines_.push_back("");

                if (state_->mode == Mode::VISUAL_BLOCK
                    && visual_row_ >= 0 && visual_col_ >= 0)
                {
                    const auto block =
                        visual_block::MakeBlock(visual_row_, visual_col_, row_, col_);
                    visual_block::Transform(lines_, block, fold);
                    row_ = block.row_lo;
                    col_ = std::min(block.col_lo,
                                    static_cast<int>(lines_[row_].size()));
                    visual_row_ = -1;
                    visual_col_ = -1;
                    state_->mode = Mode::NORMAL;
                    Save();
                    state_->status = status_msg;
                    return;
                }

                if (state_->mode == Mode::VISUAL && visual_row_ >= 0 && visual_col_ >= 0)
                {
                    int aRow = visual_row_, aCol = visual_col_;
                    int bRow = row_, bCol = col_;
                    if (aRow > bRow || (aRow == bRow && aCol > bCol))
                    {
                        std::swap(aRow, bRow);
                        std::swap(aCol, bCol);
                    }
                    if (aRow == bRow)
                    {
                        std::string& line = lines_[aRow];
                        const int size = static_cast<int>(line.size());
                        aCol = std::min(aCol, size);
                        bCol = std::min(bCol + 1, size);
                        for (int c = aCol; c < bCol; ++c)
                        {
                            line[static_cast<std::size_t>(c)] =
                                fold(line[static_cast<std::size_t>(c)]);
                        }
                    }
                    else
                    {
                        for (int c = aCol; c < static_cast<int>(lines_[aRow].size()); ++c)
                        {
                            lines_[aRow][static_cast<std::size_t>(c)] =
                                fold(lines_[aRow][static_cast<std::size_t>(c)]);
                        }
                        for (int r = aRow + 1; r < bRow; ++r)
                        {
                            for (char& ch : lines_[r]) ch = fold(ch);
                        }
                        const int bSize = static_cast<int>(lines_[bRow].size());
                        for (int c = 0; c < std::min(bCol, bSize); ++c)
                        {
                            lines_[bRow][static_cast<std::size_t>(c)] =
                                fold(lines_[bRow][static_cast<std::size_t>(c)]);
                        }
                    }
                    row_ = aRow;
                    col_ = std::min(aCol, static_cast<int>(lines_[aRow].size()));
                    visual_row_ = -1;
                    visual_col_ = -1;
                    state_->mode = Mode::NORMAL;
                    Save();
                    state_->status = status_msg;
                    return;
                }

                int a = (visual_row_ >= 0) ? std::min(visual_row_, row_) : row_;
                int b = (visual_row_ >= 0) ? std::max(visual_row_, row_) : row_;
                if (a > b) std::swap(a, b);
                for (int r = a; r <= b; ++r)
                {
                    for (char& ch : lines_[r]) ch = fold(ch);
                }
                row_ = a;
                col_ = 0;
                visual_row_ = -1;
                visual_col_ = -1;
                state_->mode = Mode::NORMAL;
                Save();
                state_->status = status_msg;
            }

            std::shared_ptr<EditorState> state_;
            scribbolyth::treeview::TreeNode* active_ = nullptr;
            std::vector<std::string> lines_;
            int row_ = 0;
            int col_ = 0;
            int last_col_ = 0;
            int visual_row_ = -1;
            int visual_col_ = -1;

            struct BlockInsert
            {
                bool active = false;
                bool at_end = false;
                int col = 0;
                std::vector<int> rows;
                std::string pending;
            };
            BlockInsert block_insert_;
    };

    ftxui::Component MakeEditor(std::shared_ptr<EditorState> state)
    {
        return ftxui::Make<Editor>(std::move(state));
    }

}
