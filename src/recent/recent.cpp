#include "recent.hpp"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <filesystem>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>

#include "../config/config.hpp"
#include "../editor/editor_state.hpp"

namespace scribbolyth::recent
{
    namespace
    {
        /*!
            Pad Right

            !_method
        */
        std::string PadRight(const std::string& s, std::size_t width)
        {
            if (s.size() >= width) return s;
            return s + std::string(width - s.size(), ' ');
        }
        //!
    }

    /*!
        Recent Dialog Class
    */
    class RecentDialog : public ftxui::ComponentBase
    {
    public:
        /*!
            Public Members
        */

        /*!
            Constructor

            !_cstor
        */
        RecentDialog(std::shared_ptr<EditorState> state, bool* show,
                     std::shared_ptr<bool> force)
            : state_(std::move(state)), show_(show), force_flag_(std::move(force)) {}
        //!

        /*!
            Variables

            ----
        */
        //>>
        bool Focusable() const override { return true; }

        bool pending_g_ = false;
        //<<
        //!

        /*!
            Ftxui Event Handler

            !_method
        */
        bool OnEvent(ftxui::Event event) override
        {
            PickupForce();
            if (filter_active_) return OnFilterEvent(event);
            if (event == ftxui::Event::Escape)
            {
                Close();
                return true;
            }
            if (event.is_character() && event.character() == "/")
            {
                filter_active_ = true;
                return true;
            }
            if (event == ftxui::Event::Return)
            {
                Open();
                return true;
            }
            if (event == ftxui::Event::ArrowDown
                    || (event.is_character() && event.character() == "j"))
            {
                MoveSelection(+1);
                return true;
            }
            if (event == ftxui::Event::ArrowUp
                    || (event.is_character() && event.character() == "k"))
            {
                MoveSelection(-1);
                return true;
            }
            if (event.is_character() && event.character() == "g")
            {
                if (pending_g_)
                {
                    pending_g_ = false;
                    MoveToStart();
                    return true;
                }
                pending_g_ = true;
                return true;
            }
            if ((event.is_character() && event.character() == "G"))
            {
                pending_g_ = false;
                MoveToEnd();
                return true;
            }
            if (event.is_character() && event.character() == "D")
            {
                RemoveSelected();
                return true;
            }
            if (event.is_character() && event.character() == "!")
            {
                force_ = !force_;
                state_->status = force_ ? "Force open - unsaved changes will be discarded"
                                        : "";
                return true;
            }
            if (event.is_character() && event.character() == "J")
            {
                MoveEntry(+1);
                return true;
            }
            if (event.is_character() && event.character() == "K")
            {
                MoveEntry(-1);
                return true;
            }
            if (event.is_character() && event.character().size() == 1
                    && event.character()[0] >= ' ')
            {
                filter_active_ = true;
                if (filter_.size() < kMaxFilter) filter_ += event.character();
                ClampToFilter();
                return true;
            }
            return true; // consume everything else
        }
        //!

        /*!
            Filter Mode Event Handler

            Ignore the vim-style bindings while filtering - every
            printable character extends the filter instead - while
            arrows still move, Return opens and Escape clears.

            !_method
        */
        bool OnFilterEvent(ftxui::Event event)
        {
            if (event == ftxui::Event::Escape)
            {
                filter_.clear();
                filter_active_ = false;
                ClampToFilter();
                return true;
            }
            if (event == ftxui::Event::Return)
            {
                Open();
                return true;
            }
            if (event == ftxui::Event::Backspace)
            {
                if (!filter_.empty()) filter_.pop_back();
                ClampToFilter();
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
            if (event.is_character() && event.character().size() == 1
                    && event.character()[0] >= ' ' && filter_.size() < kMaxFilter)
            {
                filter_ += event.character();
                ClampToFilter();
                return true;
            }
            return true;
        }
        //!

        /*!
            Ftxui Render Method

            !_method
        */
        ftxui::Element Render() override
        {
            PickupForce();
            const auto matches = Matches();
            const int total = static_cast<int>(matches.size());
            const int sel = std::min(selection_, std::max(0, total - 1));

            const int max_top = std::max(0, total - kVisibleRows);
            int top = std::min(scroll_, max_top);
            if (sel < top) top = sel;
            if (sel >= top + kVisibleRows) top = sel - kVisibleRows + 1;
            top = std::max(0, std::min(top, max_top));
            const int count = std::min(kVisibleRows, std::max(0, total - top));

            std::size_t width = 20;
            for (const auto& m : matches) width = std::max(width, state_->recent_files[m].size());
            const int content_width = static_cast<int>(std::min(width, kMaxContent));

            ftxui::Elements rows;
            const int row_width = content_width + 2;
            if (filter_active_)
            {
                rows.push_back(ftxui::text(
                        PadRight("  " + filter_ + "_  ", row_width)) | ftxui::dim);
            }
            if (total == 0)
            {
                rows.push_back(ftxui::text(PadRight(
                        filter_.empty() ? "  No recent files" : "  (no matches)", row_width))
                        | ftxui::dim);
            }
            else
            {
                for (int i = 0; i < count; ++i)
                {
                    ftxui::Element row = ftxui::text(" " + PadRight(
                            state_->recent_files[matches[static_cast<std::size_t>(top + i)]],
                            content_width) + " ");
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
                "   j/k move  J/K reorder  D remove  " +
                std::string(force_ ? "!force-on " : "! force ") +
                "/ filter  Enter open  Esc cancel  ";

            return ftxui::window(ftxui::text(" < Recent Files "),
                                ftxui::vbox({
                                    ftxui::separator(),
                                    ftxui::vbox(std::move(rows)),
                                    ftxui::separator(),
                                    ftxui::text(PadRight(footer, row_width)) | ftxui::dim,
                                })) |
                   ftxui::size(ftxui::WIDTH, ftxui::LESS_THAN, 92) |
                   ftxui::size(ftxui::HEIGHT, ftxui::LESS_THAN, 24);
        }
        //!

        //!

    private:
        /*!
            Private Members
        */

        /*!
            Force Flag

            !_method
        */
        void PickupForce()
        {
            if (force_flag_ && *force_flag_)
            {
                force_ = true;
                *force_flag_ = false;
            }
        }
        //!

        /*!
            Close Recent Dialog

            !_method
        */
        void Close()
        {
            *show_ = false;
            selection_ = 0;
            scroll_ = 0;
            force_ = false;
            filter_.clear();
            filter_active_ = false;
        }
        //!

        /*!
            Open Recent Dialog

            !_method
        */
        void Open()
        {
            auto matches = Matches();
            if (matches.empty()) return;
            const int sel = std::min(selection_, static_cast<int>(matches.size()) - 1);
            const std::string chosen = state_->recent_files[matches[static_cast<std::size_t>(sel)]];
            const bool force = force_;
            state_->status = "";
            Close();
            auto it = state_->operations.find(force ? "open_force" : "open");
            if (it != state_->operations.end())
            {
                it->second(chosen, 1);
            }
        }
        //!

        /*!
            Move Selection to Top

            !_method
        */
        void MoveToStart()
        {
            selection_ = 0;
        }
        //!

        /*!
            Move Selection to Bottom

            !_method
        */
        void MoveToEnd()
        {
            if (Matches().empty()) return;
            selection_ = static_cast<int>(Matches().size()) - 1;
        }
        //!

        /*!
            Move Selection Up / Down

            !_method
        */
        void MoveSelection(int dir)
        {
            if (Matches().empty()) return;
            const int total = static_cast<int>(Matches().size());
            selection_ = std::max(0, std::min(total - 1, selection_ + dir));
        }
        //!
 
        /*!
            Remove Selected Item

            !_method
        */
        void RemoveSelected()
        {
            auto matches = Matches();
            if (matches.empty()) return;
            const int sel = std::min(selection_, static_cast<int>(matches.size()) - 1);
            auto& recent = state_->recent_files;
            recent.erase(recent.begin() + matches[static_cast<std::size_t>(sel)]);
            ClampToFilter();
            if (!state_->init_path.empty())
            {
                scribbolyth::config::WriteRecentFiles(state_->init_path, recent);
            }
            state_->status = "Recent entry removed";
        }
        //!

        /*!
            Move Selected Entry

            !_method
        */
        void MoveEntry(int dir)
        {
            auto matches = Matches();
            if (matches.size() < 2) return;
            const int sel = std::min(selection_, static_cast<int>(matches.size()) - 1);
            const int other = sel + dir;
            if (other < 0 || other >= static_cast<int>(matches.size())) return;
            auto& recent = state_->recent_files;
            std::swap(recent[matches[static_cast<std::size_t>(sel)]],
                    recent[matches[static_cast<std::size_t>(other)]]);
            selection_ = other;
            if (!state_->init_path.empty())
            {
                scribbolyth::config::WriteRecentFiles(state_->init_path, recent);
            }
        }
        //!

        /*!
            Filtered Indices

            !_method
        */
        std::vector<std::size_t> Matches() const
        {
            std::string needle = filter_;
            std::transform(needle.begin(), needle.end(), needle.begin(),
                    [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            std::vector<std::size_t> out;
            const auto& recent = state_->recent_files;
            for (std::size_t i = 0; i < recent.size(); ++i)
            {
                std::string path = recent[i];
                std::transform(path.begin(), path.end(), path.begin(),
                        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                if (needle.empty() || path.find(needle) != std::string::npos)
                {
                    out.push_back(i);
                }
            }
            return out;
        }
        //!

        /*!
            Clamp Selection to Filter

            !_method
        */
        void ClampToFilter()
        {
            const int total = static_cast<int>(Matches().size());
            selection_ = std::max(0, std::min(std::max(0, total - 1), selection_));
            scroll_ = 0;
        }
        //!

        /*!
            Variables

            ----
        */
        //>>
        std::shared_ptr<EditorState> state_;
        bool* show_;
        std::shared_ptr<bool> force_flag_;
        int selection_ = 0;
        int scroll_ = 0;
        bool force_ = false;
        std::string filter_;
        bool filter_active_ = false;
        static constexpr int kVisibleRows = 18;
        static constexpr std::size_t kMaxContent = 88;
        static constexpr std::size_t kMaxFilter = 64;
        //<<
        //!

        //!
    };
    //!

    /*!
        Ftxui Make Dialog

        !_method
    */
    ftxui::Component MakeRecentDialog(std::shared_ptr<EditorState> state, bool* show,
                                      std::shared_ptr<bool> force)
    {
        return ftxui::Make<RecentDialog>(std::move(state), show, std::move(force));
    }
    //!

    /*!
        Limit Recent List Size

        !_method
    */
    std::size_t PruneRecentFiles(std::vector<std::string>& recent_files)
    {
        const std::size_t before = recent_files.size();
        std::error_code ec;
        recent_files.erase(
                std::remove_if(recent_files.begin(), recent_files.end(),
                    [&ec](const std::string& path)
                    {
                        return !std::filesystem::exists(path, ec);
                    }),
                recent_files.end());
        return before - recent_files.size();
    }
    //!
}
