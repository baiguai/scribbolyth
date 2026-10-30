#include "browser.hpp"

#include <algorithm>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/box.hpp>

#include "../config/config.hpp"
#include "../editor/editor_state.hpp"
#include "../op/op.hpp"

namespace fs = std::filesystem;

namespace scribbolyth::browser
{
    /*[./Introduction to Browser.cpp]
        This file handles the file browsing dialog, as well as its key bindings.

        Helper Methods
        * PadRight - soon to be consolidated to a single method.
        * Tail - this also will likely be consolidated.

        Public Members
        * The Ftxui OnEvent handles the key inputs.
        * The Ftxui Render handles the properties of the dialog itself. You can explicitly set its size etc.

        Private Members
        * The navigation and filtering is handled by these.
        * The initial directory is determined via: state_->browser_start_dir
        * Another state helper is: state_->browser_pick
        * `m` bookmarks the shown directory; ` shows the bookmark list.
            * _Render Browser Dialog_ was updated to accomodate the bookmarks footer information.
    */

    namespace
    {
        /*!
            Right Padding

            !_method

            #todo : We should be able to make this a globally accessible method.
        */
        std::string PadRight(const std::string& s, std::size_t width)
        {
            if (s.size() >= width) return s;
            return s + std::string(width - s.size(), ' ');
        }
        //!

        /*!
            Tailing

            !_method

            #todo : We should be able to make this a globally accessible method.
        */
        std::string Tail(const std::string& s, std::size_t width)
        {
            if (s.size() <= width) return s;
            return s.substr(s.size() - width);
        }
        //!

        /*!
            To Lower Case

            !_method

            #todo : We should be able to make this a globally accessible method.
        */
        std::string Lower(const std::string& s)
        {
            std::string out = s;
            for (char& c : out)
            {
                if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
            }
            return out;
        }
        //!

        /*!
            Case Insensitive Less Check

            !_method

            #todo : We should be able to make this a globally accessible method.
        */
        bool NoCaseLess(const std::string& a, const std::string& b)
        {
            return Lower(a) < Lower(b);
        }
        //!

        /*!
            Path Check

            !_method

            Gets the absolute path from the path input variable.
        */
        std::string Normalize(const std::string& path)
        {
            std::error_code ec;
            fs::path abs = fs::absolute(path, ec);
            if (ec) return path;
            fs::path normalized = abs.lexically_normal();
            if (!normalized.empty() && normalized != normalized.root_path()
                && normalized.filename().empty())
            {
                normalized = normalized.parent_path();
            }
            return normalized.string();
        }
        //!
    }

    /*!
        FileBrowserDialog Class
        (ftxui::ComponentBase)
    */
    class FileBrowserDialog : public ftxui::ComponentBase
    {
    public:
        /*!
            Public Members
        */
        /*!
            Constructor

            !_ctor
        */
        FileBrowserDialog(std::shared_ptr<EditorState> state, bool* show)
            : state_(std::move(state)), show_(show) {}
        //!

        /*!
            Variables

            ----
        */
        //>>
        bool Focusable() const override { return true; }
        //<<
        //!

        /*!
            Ftxui Browser Dialog Event

            !_method
        */
        bool OnEvent(ftxui::Event event) override
        {
            // FTXUI's own events (terminal resize, screen wakeups) are not
            // key presses: they must not drop a pending "g" prefix.
            if (IsSyntheticEvent(event)) return false;

            if (filter_active_) return OnFilterEvent(event);
            if (bookmarks_active_) return OnBookmarksEvent(event);

            if (event.is_character() && event.character() == "`")
            {
                /*>
                    `

                    Show the bookmarked directories.
                */
                //>>
                OpenBookmarks();
                return true;
                //<<
                //!
            }
            if (event.is_character() && event.character() == "m")
            {
                /*>
                    m

                    Bookmark the directory currently shown.
                */
                //>>
                AddCurrentBookmark();
                return true;
                //<<
                //!
            }

            /*>
                Key Events
            */
            if (event == ftxui::Event::Escape)
            {
                /*>
                    Escape
                */
                //>>
                if (!filter_.empty())
                {
                    ClearFilter();
                    return true;
                }
                Close();
                return true;
                //<<
                //!
            }
            if (event.is_character() && event.character() == "/")
            {
                /*>
                    /
                */
                //>>
                filter_active_ = true;
                return true;
                //<<
                //!
            }
            if (event.is_character() && event.character() == ":"
                && !state_->browser_command.empty())
            {
                /*>
                    :

                    The dialog just picks a directory; `:` hands the currently
                    shown directory back to the save/export command line so the
                    user can type a filename.
                */
                //>>
                const std::string command = std::move(state_->browser_command);
                state_->browser_command.clear();
                const std::string target = (fs::path(dir_) / "").string();
                Close();
                scribbolyth::op::OpenCommandLineWithArgs(state_, command, target);
                return true;
                //<<
                //!
            }
            if (event == ftxui::Event::Return)
            {
                /*>
                    Return

                    Select the file.
                */
                //>>
                Activate();
                return true;
                //<<
                //!
            }
            if (event.is_character() && event.character() == "l")
            {
                /*>
                    l

                    Enter the selected directory.
                */
                //>>
                EnterAt(/*pick_file=*/false);
                return true;
                //<<
                //!
            }
            if (event == ftxui::Event::ArrowDown
                || (event.is_character() && event.character() == "j"))
            {
                /*>
                    j / Arrow Down

                    Move the selection down.
                */
                //>>
                MoveSelection(+1);
                pending_g_ = false;
                return true;
                //<<
                //!
            }
            if (event == ftxui::Event::ArrowUp
                || (event.is_character() && event.character() == "k"))
            {
                /*>
                    k / Arrow Up

                    Move the selection up.
                */
                //>>
                MoveSelection(-1);
                pending_g_ = false;
                return true;
                //<<
                //!
            }
            if (event == ftxui::Event::Backspace
                || (event.is_character() && event.character() == "h"))
            {
                /*>
                    h

                    Move up one directory.
                */
                //>>
                GoUp();
                pending_g_ = false;
                return true;
                //<<
                //!
            }
            if (event.is_character() && event.character() == "g")
            {
                /*>
                    g

                    If the first 'g', await for a second 'g'.
                    If the second 'g', move to the top.
                */
                //>>
                if (pending_g_) JumpToTop();
                else pending_g_ = true;
                return true;
                //<<
                //!
            }
            if (event.is_character() && event.character() == "G")
            {
                /*>
                    G

                    Move to the bottom.
                */
                //>>
                JumpToBottom();
                return true;
                //<<
                //!
            }
            pending_g_ = false;
            return true;

            //!
        }
        //!

        /*!
            Render Browser Dialog

            !_method
        */
        ftxui::Element Render() override
        {
            /*>
                Recompute

                Before rendering ensure that everything is in sync with the state_.
            */
            //>>
            Recompute();
            //<<
            //!

            /*>
                Const Variables

                ----
            */
            //>>
            const int total = static_cast<int>(visible_.size());
            const int sel = std::min(selection_, std::max(0, total - 1));
            const int top = ComputeTop(sel, total);
            const int count = std::min(kVisibleRows, std::max(0, total - top));
            const int dlg_width { 90 };

            const std::size_t row_width = static_cast<std::size_t>(content_width_ + 2);

            const int bookmark_total = static_cast<int>(state_->bookmarked_dirs.size());
            const int bookmark_sel = std::min(bookmark_selection_,
                                              std::max(0, bookmark_total - 1));
            const int bookmark_top = ComputeTop(bookmark_sel, bookmark_total);
            const int bookmark_count = std::min(kVisibleRows,
                                                std::max(0, bookmark_total - bookmark_top));
            const std::size_t bookmark_width = static_cast<std::size_t>(dlg_width - 4);
            //<<
            //!

            /*>
                Fetch the items.

                If there are none, output (empty directory).
                This is driven by the total const value.
            */
            //!
            ftxui::Elements rows;
            if (filter_active_)
            {
                rows.push_back(ftxui::text(
                    PadRight(Tail("  / " + filter_ + "_  ", content_width_ + 2), content_width_ + 2))
                               | ftxui::inverted);
            }
            rows.push_back(ftxui::text(PadRight("  " + dir_, content_width_ + 2)) | ftxui::dim);
            rows.push_back(ftxui::separator());

            if (bookmarks_active_)
            {
                if (bookmark_total == 0)
                {
                    rows.push_back(ftxui::text(
                        PadRight("  (no bookmarked directories)", row_width)) | ftxui::dim);
                }
                else
                {
                    for (int i = 0; i < bookmark_count; ++i)
                    {
                        const int index = bookmark_top + i;
                        const std::string& path =
                            state_->bookmarked_dirs[static_cast<std::size_t>(index)];
                        ftxui::Element row = ftxui::text(
                            " " + PadRight(Tail(path, bookmark_width), bookmark_width) + " ");
                        if (index == bookmark_sel) row = row | ftxui::inverted;
                        rows.push_back(row);
                    }
                }
            }
            else if (total == 0)
            {
                rows.push_back(ftxui::text(PadRight(
                    filter_.empty() ? "  (empty directory)" : "  (no matches)", row_width))
                               | ftxui::dim);
            }
            else
            {
                for (int i = 0; i < count; ++i)
                {
                    const int index = top + i;
                    ftxui::Element row = ftxui::text(
                        " " + PadRight(entries_[visible_[static_cast<std::size_t>(index)]].display,
                                       static_cast<std::size_t>(content_width_))
                        + " ");
                    if (index == sel) row = row | ftxui::inverted;
                    rows.push_back(row);
                }
            }
            const int pad_to = kVisibleRows + 3;
            while (static_cast<int>(rows.size()) < pad_to)
            {
                rows.push_back(ftxui::text(PadRight("", row_width)));
            }

            std::string help_move =
                "  j/k move h up l enter gg/G m bookmark Enter pick / filter Esc cancel";
            if (!state_->browser_command.empty())
            {
                help_move =
                    "  j/k move h up l enter gg/G m mark Enter pick : name / filter Esc cancel";
            }
            const std::string help_clear =
                "    Esc clear  j/k move  h up  l enter  m bookmark  Enter pick  / filter  ";
            const std::string help_type = "    type to filter  Enter keep  Esc clear  ";
            const std::string help_bookmarks =
                "    j/k move  Enter open  D remove  Esc back  ";

            std::string footer;
            if (bookmarks_active_)
            {
                footer = "  " + std::to_string(bookmark_total == 0 ? 0 : bookmark_sel + 1)
                         + "/" + std::to_string(bookmark_total);
                footer += help_bookmarks;
            }
            else
            {
                footer = "  " + std::to_string(total == 0 ? 0 : sel + 1) + "/"
                         + std::to_string(total);
                if (!filter_.empty())
                {
                    footer += " (of " + std::to_string(entries_.size()) + ")";
                }
                if (filter_active_)
                {
                    footer += help_type;
                }
                else if (filter_.empty())
                {
                    footer += help_move;
                }
                else
                {
                    footer += help_clear;
                }
            }

            const std::string title = bookmarks_active_
                                          ? " File Browser - Bookmarks "
                                          : " File Browser ";

            /*|

            ----

            To explicitly set the file dialog size, locate the following:
            */
            //>>
            return ftxui::window(ftxui::text(title),
                                 ftxui::vbox({
                                     ftxui::vbox(std::move(rows)),
                                     ftxui::separator(),
                                     ftxui::text(PadRight(footer, dlg_width)) | ftxui::dim,
                                 })) |
                   ftxui::reflect(box_) |
                   ftxui::size(ftxui::WIDTH, ftxui::LESS_THAN, 140) |
                   ftxui::size(ftxui::HEIGHT, ftxui::LESS_THAN, 24);
            //<<
        }
        //!

        //!

    private:
        /*!
            Private Members
        */

        /*!
            Constants

            ----
        */
        //>>
        static constexpr int kVisibleRows = 16;
        //<<
        //!

        /*!
            Entry Struct

        */
        //>>
        struct Entry
        {
            bool is_dir = false;
            std::string display;
            std::string path;
        };
        //<<
        //!

        /*!
            Close the Dialog

            !_method
        */
        void Close()
        {
            *show_ = false;
            selection_ = 0;
            scroll_ = 0;
            pending_g_ = false;
            filter_.clear();
            filter_active_ = false;
            bookmarks_active_ = false;
            bookmark_selection_ = 0;
            state_->browser_command.clear();
            start_dir_.clear();
            needs_refresh_ = true;
        }
        //!

        /*!
            Show Bookmarked Directories

            !_method
        */
        void OpenBookmarks()
        {
            bookmarks_active_ = true;
            bookmark_selection_ = 0;
            pending_g_ = false;
        }
        //!

        /*!
            Add the Current Directory to the Bookmarks

            !_method
        */
        void AddCurrentBookmark()
        {
            const std::string target = Normalize(dir_);
            auto& dirs = state_->bookmarked_dirs;
            if (std::find(dirs.begin(), dirs.end(), target) != dirs.end())
            {
                state_->status = "Directory already bookmarked: " + target;
                return;
            }
            dirs.push_back(target);
            PersistBookmarks();
            state_->status = "Bookmarked directory: " + target;
        }
        //!

        /*!
            Remove the Selected Bookmarked Directory

            !_method
        */
        void RemoveSelectedBookmark()
        {
            auto& dirs = state_->bookmarked_dirs;
            if (dirs.empty()) return;
            const int total = static_cast<int>(dirs.size());
            const int index = std::max(0, std::min(bookmark_selection_, total - 1));
            const std::string removed = dirs[static_cast<std::size_t>(index)];
            dirs.erase(dirs.begin() + index);
            if (!dirs.empty())
            {
                bookmark_selection_ = std::max(0, std::min(bookmark_selection_,
                                                         static_cast<int>(dirs.size()) - 1));
            }
            else
            {
                bookmark_selection_ = 0;
            }
            PersistBookmarks();
            state_->status = "Removed bookmarked directory: " + removed;
        }
        //!

        /*!
            Persist the Bookmarked Directories

            !_method
        */
        void PersistBookmarks()
        {
            if (state_->init_path.empty()) return;
            scribbolyth::config::WriteInit(state_->init_path, state_->current_file,
                                           state_->recent_files,
                                           state_->bookmarked_dirs,
                                           state_->status_timeout);
        }
        //!

        /*!
            Set the Directory

            !_method
        */
        void SetDir(const std::string& path)
        {
            dir_ = Normalize(path);
            needs_refresh_ = true;
            pending_g_ = false;
        }
        //!

        /*!
            Navigate Items

            !_method
        */
        void MoveSelection(int dir)
        {
            if (visible_.empty()) return;
            const int total = static_cast<int>(visible_.size());
            selection_ = std::max(0, std::min(total - 1, selection_ + dir));
        }
        //!

        /*!
            Jump to Top / Bottom

            !_method
        */
        void JumpToTop()
        {
            if (visible_.empty()) return;
            selection_ = 0;
            pending_g_ = false;
        }

        /*|

          !_method
        */
        void JumpToBottom()
        {
            if (visible_.empty()) return;
            selection_ = static_cast<int>(visible_.size()) - 1;
            pending_g_ = false;
        }
        //!

        /*!
            Move Up a Directory

            !_method
        */
        void GoUp()
        {
            const fs::path current(dir_);
            const fs::path parent = current.parent_path();
            if (parent == current) return;
            SetDir(parent.string());
            ClearFilter();
        }
        //!

        /*!
            Pick a File

            !_method
        */
        void Activate()
        {
            EnterAt(/*pick_file=*/true);
        }
        /*!
            Enter At
            
            !_method
        */
        void EnterAt(bool pick_file)
        {
            if (visible_.empty()) return;
            const int sel = std::min(selection_, static_cast<int>(visible_.size()) - 1);
            const Entry& entry = entries_[visible_[static_cast<std::size_t>(sel)]];
            if (entry.is_dir)
            {
                /*|

                `l` always navigates into the folder. For save/export-style
                ops (`browser_command` set) the dialog is a folder picker:
                Enter hands the chosen folder back to the command line so a
                filename can be typed. `:` picks the currently shown folder
                the same way.

                */
                if (!pick_file || state_->browser_command.empty())
                {
                    SetDir(entry.path);
                    ClearFilter();
                    return;
                }
                const std::string command = std::move(state_->browser_command);
                state_->browser_command.clear();
                const std::string target = (fs::path(entry.path) / "").string();
                Close();
                scribbolyth::op::OpenCommandLineWithArgs(state_, command, target);
                return;
            }
            if (!pick_file) return;
            if (!state_->browser_pick) return;
            const std::string picked = entry.path;
            const auto callback = std::move(state_->browser_pick);
            state_->browser_pick = {};
            Close();
            callback(picked);
        }
        //!

        /*!
            Clear the Filter

            !_method

            Simply clear out the filter variables.
        */
        void ClearFilter()
        {
            filter_.clear();
            filter_active_ = false;
            selection_ = 0;
        }
        //!

        /*!
            Compute the Top

            !_method

            Computation logic:

        */
        int ComputeTop(int sel, int total) const
        {
            //>>
            const int max_top = std::max(0, total - kVisibleRows);
            int top = std::min(scroll_, max_top);
            if (sel < top) top = sel;
            if (sel >= top + kVisibleRows) top = sel - kVisibleRows + 1;
            return std::max(0, std::min(top, max_top));
            //<<
        }
        //!

        /*!
            Handle Filter Event

            !_method
        */
        bool OnFilterEvent(ftxui::Event event)
        {
            /*>
                Escape Key
            */
            if (event == ftxui::Event::Escape)
            {
                //>>
                ClearFilter();
                return true;
                //<<
            }
            //!

            /*>
                Return Key
            */
            if (event == ftxui::Event::Return)
            {
                //>>
                filter_active_ = false;
                selection_ = 0;
                return true;
                //<<
            }
            //!

            /*>
                Backspace Key
            */
            if (event == ftxui::Event::Backspace)
            {
                //>>
                if (!filter_.empty()) filter_.pop_back();
                selection_ = 0;
                return true;
                //<<
            }
            //!

            /*>
                General Key Processing
            */
            if (event.is_character())
            {
                //>>
                filter_ += event.character();
                selection_ = 0;
                return true;
                //<<
            }
            //!

            return true;
        }
        //!

        /*!
            Handle Bookmark List Event

            !_method
        */
        bool OnBookmarksEvent(ftxui::Event event)
        {
            if (event == ftxui::Event::Escape)
            {
                //>>
                bookmarks_active_ = false;
                bookmark_selection_ = 0;
                pending_g_ = false;
                return true;
                //<<
            }

            if (event == ftxui::Event::Return)
            {
                //>>
                const int total = static_cast<int>(state_->bookmarked_dirs.size());
                if (total == 0) return true;
                const int index = std::max(0, std::min(bookmark_selection_, total - 1));
                SetDir(state_->bookmarked_dirs[static_cast<std::size_t>(index)]);
                ClearFilter();
                bookmarks_active_ = false;
                bookmark_selection_ = 0;
                return true;
                //<<
            }

            if (event.is_character() && event.character() == "D")
            {
                //>>
                RemoveSelectedBookmark();
                return true;
                //<<
            }

            if (event.is_character() && event.character() == "m")
            {
                //>>
                AddCurrentBookmark();
                return true;
                //<<
            }

            if (event == ftxui::Event::ArrowDown
                || (event.is_character() && event.character() == "j"))
            {
                //>>
                const int total = static_cast<int>(state_->bookmarked_dirs.size());
                if (total > 0)
                {
                    bookmark_selection_ =
                        std::max(0, std::min(total - 1, bookmark_selection_ + 1));
                }
                pending_g_ = false;
                return true;
                //<<
            }

            if (event == ftxui::Event::ArrowUp
                || (event.is_character() && event.character() == "k"))
            {
                //>>
                bookmark_selection_ = std::max(0, bookmark_selection_ - 1);
                pending_g_ = false;
                return true;
                //<<
            }

            if (event.is_character() && event.character() == "g")
            {
                //>>
                if (pending_g_)
                {
                    bookmark_selection_ = 0;
                    pending_g_ = false;
                }
                else
                {
                    pending_g_ = true;
                }
                return true;
                //<<
            }

            if (event.is_character() && event.character() == "G")
            {
                //>>
                const int total = static_cast<int>(state_->bookmarked_dirs.size());
                bookmark_selection_ = std::max(0, total - 1);
                pending_g_ = false;
                return true;
                //<<
            }

            pending_g_ = false;
            return true;
        }
        //!

        /*!
            Recompute

            !_method

            * Refresh this dialog via the state_
        */
        void Recompute()
        {
            if (state_->browser_start_dir != start_dir_)
            {
                start_dir_ = state_->browser_start_dir;
                filter_.clear();
                filter_active_ = false;
                SetDir(start_dir_);
                selection_ = 0;
            }
            if (needs_refresh_)
            {
                needs_refresh_ = false;
                Relist();
            }
            if (last_filter_ != filter_) ApplyFilter();
        }

        /*|

            !_method

            * List the currently selected directory's contents.
        */
        void Relist()
        {
            entries_.clear();
            content_width_ = 24;

            std::error_code ec;
            const fs::path dir(dir_);

            const fs::path parent = dir.parent_path();
            if (parent != dir)
            {
                entries_.push_back(Entry{true, "../", parent.string()});
            }

            std::vector<std::string> subdirs;
            std::vector<std::string> files;
            for (fs::directory_iterator it(dir, fs::directory_options::skip_permission_denied, ec),
                 end;
                 it != end; it.increment(ec))
            {
                if (ec) break;
                std::error_code sec;
                const fs::path p = it->path();
                const std::string name = p.filename().string();
                if (name.empty() || name == ".") continue;
                if (fs::is_directory(p, sec)) subdirs.push_back(name);
                else files.push_back(name);
            }
            (void)ec;

            std::sort(subdirs.begin(), subdirs.end(), NoCaseLess);
            std::sort(files.begin(), files.end(), NoCaseLess);

            for (const std::string& name : subdirs)
            {
                entries_.push_back(Entry{true, name + "/", (dir / name).string()});
            }
            for (const std::string& name : files)
            {
                entries_.push_back(Entry{false, name, (dir / name).string()});
            }

            for (const Entry& e : entries_)
            {
                content_width_ = std::max(content_width_, static_cast<int>(e.display.size()));
            }

            ApplyFilter();
        }
        //!

        /*!
            Apply the Filter

            !_method

            * Applies the currently set filter.
        */
        void ApplyFilter()
        {
            last_filter_ = filter_;
            visible_.clear();
            const std::string needle = Lower(filter_);
            for (std::size_t i = 0; i < entries_.size(); ++i)
            {
                if (needle.empty()
                    || Lower(entries_[i].display).find(needle) != std::string::npos)
                {
                    visible_.push_back(i);
                }
            }
            const int total = static_cast<int>(visible_.size());
            selection_ = std::max(0, std::min(selection_, total - 1));
        }
        //!

        /*!
            Variables

            ----
        */
        //>>
        std::shared_ptr<EditorState> state_;
        bool* show_;
        ftxui::Box box_;
        std::string dir_;
        std::string start_dir_;
        bool needs_refresh_ = true;
        std::vector<Entry> entries_;
        std::vector<std::size_t> visible_;
        std::string filter_;
        std::string last_filter_;
        bool filter_active_ = false;
        bool bookmarks_active_ = false;
        int bookmark_selection_ = 0;
        int selection_ = 0;
        int scroll_ = 0;
        int content_width_ = 24;
        bool pending_g_ = false;
        //<<
        //!

        //!
    };
    //!

    /*>
        Make the File Browser Dialog

        !_method
    */
    ftxui::Component MakeFileBrowserDialog(std::shared_ptr<EditorState> state, bool* show)
    {
        return ftxui::Make<FileBrowserDialog>(std::move(state), show);
    }
    //!
}
