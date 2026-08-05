#include "editor.hpp"

#include <algorithm>
#include <string>
#include <vector>

#include <ftxui/dom/elements.hpp>

#include "../bookmark/bookmark.hpp"

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

                state_->operations["undo"] = [](const std::string&, int) {};
                state_->operations["yank"] = [](const std::string&, int) {};
                state_->operations["paste"] = [](const std::string&, int) {};

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
                for (int r = 0; r < static_cast<int>(lines_.size()); ++r)
                {
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
                            ftxui::text(pre),
                            ftxui::text(at) | ftxui::inverted | ftxui::focus,
                            ftxui::text(post),
                        }));
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
                if (scribbolyth::op::HandleKey(state_, event))
                {
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
                std::string joined;
                for (std::size_t i = 0; i < lines_.size(); ++i)
                {
                    if (i != 0) joined += '\n';
                    joined += lines_[i];
                }
                active_->text = std::move(joined);
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
                std::string rest = line.substr(static_cast<std::size_t>(col_));
                line = line.substr(0, static_cast<std::size_t>(col_));
                lines_.insert(lines_.begin() + row_ + 1, rest);
                ++row_;
                col_ = 0;
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

            std::shared_ptr<EditorState> state_;
            scribbolyth::treeview::TreeNode* active_ = nullptr;
            std::vector<std::string> lines_;
            int row_ = 0;
            int col_ = 0;
            int last_col_ = 0;
    };

    ftxui::Component MakeEditor(std::shared_ptr<EditorState> state)
    {
        return ftxui::Make<Editor>(std::move(state));
    }

}
