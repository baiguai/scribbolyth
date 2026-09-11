#include "regex.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <vector>

#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>

#include "../editor/editor_state.hpp"

namespace scribbolyth::regex
{
    namespace
    {
        std::vector<std::string> SplitFields(const std::string& line)
        {
            std::vector<std::string> fields;
            std::size_t i = 0;
            while (i < line.size())
            {
                while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) ++i;

                if (i >= line.size()) break;

                if (line[i] == '"')
                {
                    ++i;
                    std::string token;
                    while (i < line.size() && line[i] != '"') token += line[i++];
                    ++i;
                    fields.push_back(token);
                }
                else
                {
                    std::string token;
                    while (i < line.size() && line[i] != ' ' && line[i] != '\t') token += line[i++];
                    fields.push_back(token);
                }
            }
            return fields;
        }

        std::string PadRight(const std::string& s, std::size_t width)
        {
            if (s.size() >= width) return s;
            return s + std::string(width - s.size(), ' ');
        }

        std::string Lower(std::string s)
        {
            std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
                return static_cast<char>(std::tolower(c));
            });
            return s;
        }

        void ParseEntries(const std::string& path, std::vector<RegexEntry>& out)
        {
            std::ifstream file(path);
            if (!file.is_open()) return;

            std::string line;
            while (std::getline(file, line))
            {
                std::size_t hash = line.find_first_not_of(" \t");
                if (hash != std::string::npos && line[hash] == '#') line = line.substr(0, hash);

                auto fields = SplitFields(line);
                if (fields.size() < 4) continue;

                RegexEntry entry;
                entry.category    = fields[0];
                entry.pattern     = fields[1];
                entry.example     = fields[2];
                entry.description = fields[3];

                entry.line = PadRight(entry.category, 10) + " " +
                             PadRight(entry.pattern, 15) + " " +
                             PadRight(entry.example, 20) + " " +
                             entry.description;
                out.push_back(std::move(entry));
            }
        }
    }

    class RegexDialog : public ftxui::ComponentBase
    {
    public:
        RegexDialog(std::shared_ptr<EditorState> state, const std::string& path, bool* show)
            : state_(std::move(state)), show_(show)
        {
            ParseEntries(path, entries_);
            for (const auto& e : entries_)
            {
                content_width_ = std::max(content_width_, static_cast<int>(e.line.size()));
            }
        }

        bool Focusable() const override { return true; }

        bool OnEvent(ftxui::Event event) override
        {
            if (event == ftxui::Event::Escape)
            {
                *show_ = false;
                filter_.clear();
                scroll_ = 0;
                return true;
            }
            if (event == ftxui::Event::ArrowDown)
            {
                scroll_ = std::min(scroll_ + 1, MaxTop());
                return true;
            }
            if (event == ftxui::Event::ArrowUp)
            {
                scroll_ = std::max(0, scroll_ - 1);
                return true;
            }
            if (event == ftxui::Event::Backspace)
            {
                if (!filter_.empty())
                {
                    filter_.pop_back();
                    scroll_ = 0;
                }
                return true;
            }
            if (event.is_character())
            {
                filter_ += event.character();
                scroll_ = 0;
                return true;
            }
            return true;
        }

        ftxui::Element Render() override
        {
            const std::vector<int> filtered = Filtered();
            const int total = static_cast<int>(filtered.size());
            const int top = std::min(scroll_, MaxTop());
            const int count = std::min(kVisibleRows, std::max(0, total - top));

            const std::string header =
                " " + PadRight("CATEGORY", 10) + " " +
                PadRight("PATTERN", 15) + " " +
                PadRight("EXAMPLE", 20) + " " +
                "DESCRIPTION";

            ftxui::Elements rows;
            const int row_width = content_width_ + 2;
            if (total == 0)
            {
                rows.push_back(ftxui::text(PadRight(
                    filter_.empty() ? "  No entries loaded (regex.conf missing?)"
                                    : "  No entries match the filter",
                    row_width)) |
                               ftxui::dim);
            }
            else
            {
                for (int i = 0; i < count; ++i)
                {
                    ftxui::Element row = ftxui::text(" " + PadRight(entries_[filtered[top + i]].line, content_width_) + " ");
                    if (i == 0) row = row | ftxui::inverted;
                    rows.push_back(row);
                }
            }

            // Keep the dialog a fixed size even when few rows match: pad the
            // rows area out to kVisibleRows and every row out to a fixed
            // width, so the window never shrinks.
            while (static_cast<int>(rows.size()) < kVisibleRows)
            {
                rows.push_back(ftxui::text(PadRight("", row_width)));
            }

            const std::string footer =
                "  " + std::to_string(total == 0 ? 0 : top + 1) + "-" +
                std::to_string(top + count) + " of " + std::to_string(total) +
                "    Up/Down scroll  Esc close  type to filter  ";

            return ftxui::window(ftxui::text(" Regex Cheat Sheet "),
                                ftxui::vbox({
                                    ftxui::hbox({
                                        ftxui::text(" Filter: " + filter_ + "_"),
                                        ftxui::filler(),
                                    }),
                                    ftxui::separator(),
                                    ftxui::text(header) | ftxui::bold,
                                    ftxui::separator(),
                                    ftxui::vbox(std::move(rows)),
                                    ftxui::separator(),
                                    ftxui::text(PadRight(footer, row_width)) | ftxui::dim,
                                })) |
                   ftxui::size(ftxui::WIDTH, ftxui::LESS_THAN, 220) |
                   ftxui::size(ftxui::HEIGHT, ftxui::LESS_THAN, 24);
        }

    private:
        int MaxTop() const
        {
            return std::max(0, static_cast<int>(Filtered().size()) - kVisibleRows);
        }

        std::vector<int> Filtered() const
        {
            std::vector<int> result;

            std::string needle = Lower(filter_);
            for (std::size_t i = 0; i < entries_.size(); ++i)
            {
                const std::string& hay = entries_[i].line;
                if (needle.empty() || Lower(hay).find(needle) != std::string::npos)
                {
                    result.push_back(static_cast<int>(i));
                }
            }
            return result;
        }

        std::shared_ptr<EditorState> state_;
        bool* show_;
        std::vector<RegexEntry> entries_;
        std::string filter_;
        int scroll_ = 0;
        int content_width_ = 92;
        static constexpr int kVisibleRows = 17;
    };

    ftxui::Component MakeRegexDialog(std::shared_ptr<EditorState> state,
                                     const std::string& config_path,
                                     bool* show)
    {
        return ftxui::Make<RegexDialog>(std::move(state), config_path, show);
    }
}
