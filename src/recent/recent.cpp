#include "recent.hpp"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>

#include "../editor/editor_state.hpp"

namespace scribbolyth::recent
{
    namespace
    {
        std::string PadRight(const std::string& s, std::size_t width)
        {
            if (s.size() >= width) return s;
            return s + std::string(width - s.size(), ' ');
        }
    }

    class RecentDialog : public ftxui::ComponentBase
    {
    public:
        RecentDialog(std::shared_ptr<EditorState> state, bool* show)
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
                Open();
                return true;
            }
            if (event == ftxui::Event::ArrowDown
                    || (event.is_character() && event.character() == "j"))
            {
                MoveSelection(+1);
                return true;
            }
            if (event == ftxui::Event::ArrowDown
                    || (event.is_character() && event.character() == "k"))
            {
                MoveSelection(-1);
                return true;
            }
            return true; // consume everything else
        }

        ftxui::Element Render() override
        {
            const auto& recent = state_->recent_files;
            const int total = static_cast<int>(recent.size());
            const int sel = std::min(selection_, std::max(0, total - 1));

            const int max_top = std::max(0, total - kVisibleRows);
            int top = std::min(scroll_, max_top);
            if (sel < top) top = sel;
            if (sel >= top + kVisibleRows) top = sel - kVisibleRows + 1;
            top = std::max(0, std::min(top, max_top));
            const int count = std::min(kVisibleRows, std::max(0, total - top));

            std::size_t width = 20;
            for (const auto& p : recent) width = std::max(width, p.size());
            const int content_width = static_cast<int>(std::min(width, kMaxContent));

            ftxui::Elements rows;
            const int row_width = content_width + 2;
            if (total == 0)
            {
                rows.push_back(ftxui::text(PadRight("  No recent files", row_width)) | ftxui::dim);
            }
            else
            {
                for (int i = 0; i < count; ++i)
                {
                    ftxui::Element row = ftxui::text(" " + PadRight(recent[static_cast<std::size_t>(top + i)], content_width) + " ");
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
                "   j/k move  Enter open  Esc cancel  ";

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

    private:
        void Close()
        {
            *show_ = false;
            selection_ = 0;
            scroll_ = 0;
        }

        void Open()
        {
            const auto& recent = state_->recent_files;
            if (recent.empty()) return;
            const int sel = std::min(selection_, static_cast<int>(recent.size()) - 1);
            const std::string chosen = recent[static_cast<std::size_t>(sel)];
            state_->status = "";
            Close();
            auto it = state_->operations.find("open");
            if (it != state_->operations.end())
            {
                it->second(chosen, 1);
            }
        }

        void MoveSelection(int dir)
        {
            const auto& recent = state_->recent_files;
            if (recent.empty()) return;
            const int total = static_cast<int>(recent.size());
            selection_ = std::max(0, std::min(total - 1, selection_ + dir));
        }

        std::shared_ptr<EditorState> state_;
        bool* show_;
        int selection_ = 0;
        int scroll_ = 0;
        static constexpr int kVisibleRows = 18;
        static constexpr std::size_t kMaxContent = 88;
    };

    ftxui::Component MakeRecentDialog(std::shared_ptr<EditorState> state, bool* show)
    {
        return ftxui::Make<RecentDialog>(std::move(state), show);
    }
}
