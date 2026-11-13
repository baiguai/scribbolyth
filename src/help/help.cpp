#include "help.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>

#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>

#include "../editor/editor_state.hpp"
#include "../text/text.hpp"

namespace scribbolyth::help
{
    namespace fs = std::filesystem;

    /*[./Introduction to Help.cpp]
        This file handles the help dialog, and its key bindings.



        #intro
    */

    namespace
    {
        /*!
            Split Fields

            Read the first `limit` fields of the line. Quoted fields may hold
            spaces. `end` receives the offset just after the last field that
            was read, so the caller can pick up the remainder of the line.

            !_method
        */
        std::vector<std::string> SplitFields(const std::string& line, std::size_t limit, std::size_t& end)
        {
            std::vector<std::string> fields;
            std::size_t i = 0;
            while (i < line.size() && fields.size() < limit)
            {
                while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) ++i;

                if (i >= line.size()) break;

                if (line[i] == '"')
                {
                    ++i;
                    std::string token;
                    while (i < line.size() && line[i] != '"') token += line[i++];
                    if (i < line.size()) ++i;
                    fields.push_back(token);
                }
                else
                {
                    std::string token;
                    while (i < line.size() && line[i] != ' ' && line[i] != '\t') token += line[i++];
                    fields.push_back(token);
                }
            }
            end = i;
            return fields;
        }
        //!

        /*!
            Trim

            !_method
        */
        std::string Trim(const std::string& s)
        {
            std::size_t begin = 0;
            std::size_t end = s.size();
            while (begin < end && (s[begin] == ' ' || s[begin] == '\t')) ++begin;
            while (end > begin && (s[end - 1] == ' ' || s[end - 1] == '\t' || s[end - 1] == '\r')) --end;
            return s.substr(begin, end - begin);
        }
        //!

        /*!
            Split Description

            The description field is the short text shown in the list of key
            bindings. Split on a pipe that is not inside quotes and drop the
            quotes, so a legacy '|' topic (now ignored) does not leak into
            the short text.

            !_method
        */
        std::vector<std::string> SplitDescription(const std::string& text)
        {
            std::vector<std::string> parts;
            std::string current;
            bool quoted = false;

            for (std::size_t i = 0; i < text.size(); ++i)
            {
                const char c = text[i];
                if (c == '"')
                {
                    quoted = !quoted;
                    continue;
                }
                if (c == '|' && !quoted)
                {
                    parts.push_back(Trim(current));
                    current.clear();
                    continue;
                }
                current += c;
            }
            parts.push_back(Trim(current));
            return parts;
        }
        //!

        /*!
            Expand Topic

            Turn the escape sequences of a help topic into their literal
            meaning: '\n' becomes a line break, '\t' four spaces and '\\' a
            single backslash. The result is returned as the lines to show.

            !_method
        */
        std::vector<std::string> ExpandTopic(const std::string& text)
        {
            std::string expanded;
            expanded.reserve(text.size());

            for (std::size_t i = 0; i < text.size(); ++i)
            {
                if (text[i] == '\\' && i + 1 < text.size())
                {
                    const char next = text[i + 1];
                    if (next == 'n') { expanded += '\n'; ++i; continue; }
                    if (next == 't') { expanded += "    "; ++i; continue; }
                    if (next == '\\') { expanded += '\\'; ++i; continue; }
                }
                expanded += text[i];
            }

            std::vector<std::string> lines;
            std::string current;
            for (const char c : expanded)
            {
                if (c == '\n')
                {
                    lines.push_back(current);
                    current.clear();
                    continue;
                }
                current += c;
            }
            lines.push_back(current);
            return lines;
        }
        //!

        /*!
            Wrap Lines

            Wrap the lines of a help topic so they fit the dialog. Words are
            kept whole; a word longer than the width is split up.

            !_method
        */
        std::vector<std::string> WrapLines(const std::vector<std::string>& lines, std::size_t width)
        {
            if (width < 8) width = 8;

            std::vector<std::string> wrapped;
            for (const auto& line : lines)
            {
                std::size_t indent = 0;
                while (indent < line.size() && line[indent] == ' ') ++indent;

                std::string current;
                std::istringstream words(line.substr(indent));
                std::string word;
                bool first = true;

                while (words >> word)
                {
                    if (first)
                    {
                        current = std::string(indent, ' ') + word;
                        first = false;
                        continue;
                    }
                    if (current.size() + 1 + word.size() > width)
                    {
                        wrapped.push_back(current);
                        current = word;
                        continue;
                    }
                    current += " " + word;
                }

                // An empty line keeps its own break, a line of words that is
                // longer than the width gets hard split.
                if (first) { wrapped.push_back(line); continue; }
                while (current.size() > width)
                {
                    wrapped.push_back(current.substr(0, width));
                    current = current.substr(width);
                }
                wrapped.push_back(current);
            }
            return wrapped;
        }
        //!

        // Pad Right - shared helper from ../text/text.hpp.
        using scribbolyth::text::PadRight;

        /*!
            To Lower

            !_method
        */
        std::string Lower(std::string s)
        {
            std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
                return static_cast<char>(std::tolower(c));
            });
            return s;
        }
        //!

        /*!
            Detail Stem

            The file name (without directory or .txt) of the detailed help
            topic of a binding: the mode, an underscore, then the key made
            file-name safe:

              - a special token (Esc, Return, ArrowUp, Ctrl;...) is simply
                lowercased:            tree_arrowup.txt  (TREE ArrowUp)
              - an upper case letter is written shift-<letter>, so j and J
                stay apart on case-insensitive file systems:
                                       tree_shift-j.txt  (TREE J)
              - a multi-key sequence joins its keys with dashes:
                                       tree_g-g.txt      (TREE "g g")
              - characters that cannot appear in a file name are spelled
                out:                  : colon  / slash  \ backslash
                                       < lt  > gt  * star  ? question
                                       " quote  | pipe
                                       tree_slash.txt   (TREE /)

            GLOBAL rows have no key, so their file is named after the
            command, using the same per-character rules (so :o! and :O!
            stay apart):              global_open.txt   (:open)
                                      global_shift-o!.txt (:O!)

            !_method
        */
        std::string DetailStem(const std::string& mode,
                               const std::string& key,
                               const std::string& command)
        {
            const bool is_command = (key == "-");
            const std::string& k = is_command ? command : key;

            std::string stem = Lower(mode);
            stem += '_';

            // A whole key token (Esc, ArrowUp, Ctrl;...) is lowercased as-is;
            // only single characters need the per-character rules. Command
            // names are case sensitive (o! vs O!), so they always take the
            // per-character rules.
            if (!is_command && k.size() > 1 && k.find(' ') == std::string::npos)
            {
                return stem + Lower(k);
            }

            for (const char c : k)
            {
                switch (c)
                {
                    case ' ':  stem += '-';         continue;
                    case ':':  stem += "colon";     continue;
                    case '/':  stem += "slash";     continue;
                    case '\\': stem += "backslash"; continue;
                    case '<':  stem += "lt";        continue;
                    case '>':  stem += "gt";        continue;
                    case '*':  stem += "star";      continue;
                    case '?':  stem += "question"; continue;
                    case '"':  stem += "quote";     continue;
                    case '|':  stem += "pipe";     continue;
                    default: break;
                }
                if (c >= 'A' && c <= 'Z')
                {
                    stem += "shift-";
                    stem += static_cast<char>(c - 'A' + 'a');
                    continue;
                }
                stem += c;
            }
            return stem;
        }
        //!

        /*!
            Read Detail Topic

            Read the detailed help topic of a binding from its file in the
            commands_detail directory. Line breaks are the file's own;
            Windows carriage returns are dropped and tabs become four
            spaces, matching the '\t' escape of a '|' topic. An unreadable
            or empty file returns "" so the caller can fall back to the
            '|' topic.

            !_method
        */
        std::string ReadDetailTopic(const std::string& path)
        {
            std::ifstream file(path);
            if (!file.is_open()) return "";

            std::string content;
            std::string line;
            bool first = true;
            while (std::getline(file, line))
            {
                if (!line.empty() && line.back() == '\r') line.pop_back();
                if (!first) content += '\n';
                first = false;
                for (const char c : line)
                {
                    if (c == '\t') content += "    ";
                    else content += c;
                }
            }

            // Drop the trailing line break / blank padding of the file.
            while (!content.empty()
                   && (content.back() == '\n' || content.back() == ' '
                       || content.back() == '\t' || content.back() == '\r'))
            {
                content.pop_back();
            }
            return content;
        }
        //!

        /*!
            Parse Entries

            `detail_dir` is the commands_detail folder that sits next to
            commands.conf; a matching file in it overrides the '|' topic
            of a binding.

            !_method
        */
        void ParseEntries(const std::string& path,
                           const std::string& detail_dir,
                           std::vector<HelpEntry>& out)
        {
            std::ifstream file(path);
            if (!file.is_open()) return;

            std::string line;
            while (std::getline(file, line))
            {
                std::size_t hash = line.find_first_not_of(" \t");
                if (hash != std::string::npos && line[hash] == '#') line = line.substr(0, hash);

                std::size_t end = 0;
                auto fields = SplitFields(line, 6, end);
                if (fields.size() < 6) continue;

                HelpEntry entry;
                entry.mode        = fields[0];
                entry.key         = fields[1];
                entry.command     = fields[3];
                entry.op          = fields[5];

                // The rest of the line is the short description shown in
                // the list of key bindings. Anything after an unquoted pipe
                // is ignored: detailed help now comes only from files in
                // commands_detail/.
                const auto parts = SplitDescription(line.substr(end));
                entry.description = parts.empty() ? "" : parts[0];
                entry.topic.clear();
                entry.has_detail = false;

                // A file in commands_detail/ holds the detailed topic. A
                // binding without one has no detailed help: Enter on it
                // does nothing.
                if (!detail_dir.empty())
                {
                    const std::string detail = ReadDetailTopic(
                        (fs::path(detail_dir) /
                         (DetailStem(entry.mode, entry.key, entry.command) + ".txt")).string());
                    if (!detail.empty())
                    {
                        entry.topic = detail;
                        entry.has_detail = true;
                    }
                }

                // Entries without a key binding (GLOBAL) show their command
                // as the "key", e.g. ":qa", so the diagram and the ':'
                // key filter cover them too.
                if (entry.key == "-" && entry.command != "-")
                {
                    entry.key = ":" + entry.command;
                }

                // The diagram (and the filter) shows the binding and its
                // description; the op and command names stay hidden.
                entry.line = PadRight(entry.mode, 6) + " " +
                             PadRight(entry.key, 12) + " " +
                             entry.description;
                out.push_back(std::move(entry));
            }
        }
        //!
    }

    /*!
        Help Dialog Class



        #class
    */
    class HelpDialog : public ftxui::ComponentBase
    {
    public:
        /*!
            Public Members
        */

        /*!
            Help Constructor

            !_ctor
        */
        HelpDialog(std::shared_ptr<EditorState> state, const std::string& path, bool* show)
            : state_(std::move(state)), show_(show)
        {
            // The detailed topic files live in commands_detail/, next to
            // the commands.conf the dialog reads.
            const fs::path detail_dir =
                fs::path(path).parent_path() / "commands_detail";
            ParseEntries(path, detail_dir.string(), entries_);
            for (const auto& e : entries_)
            {
                content_width_ = std::max(content_width_, static_cast<int>(e.line.size()));
            }
        }
        //!

        /*!
            Focusable Const

        */
        //>>
        bool Focusable() const override { return true; }
        //<<
        //!

        /*!
            Event Handler

            !_method
        */
        bool OnEvent(ftxui::Event event) override
        {
            if (topic_open_) return OnTopicEvent(event);

            if (event == ftxui::Event::Escape)
            {
                /*>
                    Escape

                    Hide the help dialog.
                */
                //>>
                *show_ = false;
                filter_.clear();
                selection_ = 0;
                scroll_ = 0;
                return true;
                //<<
                //!
            }
            if (event == ftxui::Event::Return)
            {
                /*>
                    Return

                    Open detailed help on the selected topic.
                */
                //>>
                OpenTopic();
                return true;
                //<<
                //!
            }
            if (event == ftxui::Event::ArrowDown)
            {
                /*>
                    Arrow Down

                    Move selection down.
                */
                //>>
                MoveSelection(+1);
                return true;
                //<<
                //!
            }
            if (event == ftxui::Event::ArrowUp)
            {
                /*>
                    Arrow Up

                    Move selection up.
                */
                //>>
                MoveSelection(-1);
                return true;
                //<<
                //!
            }
            if (event == ftxui::Event::PageDown)
            {
                /*>
                    Page Down

                    Moves one page down (page being the count of visible rows).
                */
                //>>
                MoveSelection(+kVisibleRows);
                return true;
                //<<
                //!
            }
            if (event == ftxui::Event::PageUp)
            {
                /*>
                    Page Up

                    Moves one page up (page being the count of visible rows).
                */
                //>>
                MoveSelection(-kVisibleRows);
                return true;
                //<<
                //!
            }
            if (event == ftxui::Event::Home)
            {
                /*>
                    Home

                    Move to the top of the help topics.
                */
                //>>
                MoveSelection(-std::max(0, static_cast<int>(Filtered().size())));
                return true;
                //<<
                //!
            }
            if (event == ftxui::Event::End)
            {
                /*>
                    End

                    Move to the bottom of the help topics.
                */
                //>>
                MoveSelection(std::max(0, static_cast<int>(Filtered().size())));
                return true;
                //<<
                //!
            }
            if (event == ftxui::Event::Backspace)
            {
                /*>
                    Backspace

                    When filtering, deletes the character before the cursor.
                */
                //>>
                if (!filter_.empty())
                {
                    filter_.pop_back();
                    Refilter();
                }
                return true;
                //<<
                //!
            }
            if (event.is_character())
            {
                /*>
                    Normal Character

                    When filtering enters the specified character.
                */
                //>>
                filter_ += event.character();
                Refilter();
                return true;
                //<<
                //!
            }
            return true;
        }
        //!

        /*!
            Topic Event Handler

            While the full help topic is shown every key is consumed, except
            for the keys that scroll it and the ones that close it again.

            !_method
        */
        bool OnTopicEvent(ftxui::Event event)
        {
            if (event == ftxui::Event::Escape
                    || event == ftxui::Event::Return)
            {
                topic_open_ = false;
                topic_scroll_ = 0;
                return true;
            }
            if (event == ftxui::Event::ArrowDown)
            {
                topic_scroll_ = std::min(topic_scroll_ + 1, MaxTopicTop());
                return true;
            }
            if (event == ftxui::Event::ArrowUp)
            {
                topic_scroll_ = std::max(0, topic_scroll_ - 1);
                return true;
            }
            if (event == ftxui::Event::PageDown)
            {
                topic_scroll_ = std::min(topic_scroll_ + kVisibleRows, MaxTopicTop());
                return true;
            }
            if (event == ftxui::Event::PageUp)
            {
                topic_scroll_ = std::max(0, topic_scroll_ - kVisibleRows);
                return true;
            }
            if (event == ftxui::Event::Home)
            {
                topic_scroll_ = 0;
                return true;
            }
            if (event == ftxui::Event::End)
            {
                topic_scroll_ = MaxTopicTop();
                return true;
            }
            return true;
        }
        //!

        /*!
            Open Topic

            Show the detailed help topic of the highlighted key binding. The
            topic comes from that binding's file in commands_detail/; when
            there is no matching file Enter does nothing.

            !_method
        */
        void OpenTopic()
        {
            const auto filtered = Filtered();
            if (filtered.empty()) return;

            const int sel = std::min(selection_, static_cast<int>(filtered.size()) - 1);
            const HelpEntry& entry = entries_[filtered[sel]];
            if (!entry.has_detail) return;

            const std::string& topic = entry.topic;

            topic_lines_ = WrapLines(ExpandTopic(topic),
                                      static_cast<std::size_t>(std::max(8, InnerWidth() - 2)));
            topic_title_ = entry.mode + " " + entry.key;
            topic_summary_ = entry.description;
            topic_scroll_ = 0;
            topic_open_ = true;
        }
        //!

        /*!
            Render the Help Dialog

            !_method
        */
        ftxui::Element Render() override
        {
            if (topic_open_) return RenderTopic();

            const std::vector<int> filtered = Filtered();
            const int total = static_cast<int>(filtered.size());
            const int sel = std::min(selection_, std::max(0, total - 1));

            // Scroll just enough to keep the selected binding on screen.
            const int max_top = std::max(0, total - kVisibleRows);
            int top = std::min(scroll_, max_top);
            if (sel < top) top = sel;
            if (sel >= top + kVisibleRows) top = sel - kVisibleRows + 1;
            top = std::max(0, std::min(top, max_top));
            scroll_ = top;

            const int count = std::min(kVisibleRows, std::max(0, total - top));

            ftxui::Elements rows;
            const int row_width = content_width_ + 2;
            if (total == 0)
            {
                rows.push_back(ftxui::text(PadRight(
                    filter_.empty() ? "  No bindings loaded (commands.conf missing?)"
                                    : "  No bindings match the filter",
                    row_width)) |
                               ftxui::dim);
            }
            else
            {
                for (int i = 0; i < count; ++i)
                {
                    ftxui::Element row = ftxui::text(" " + PadRight(entries_[filtered[top + i]].line, content_width_) + " ");
                    if (top + i == sel) row = row | ftxui::inverted;
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
                "    Up/Down Enter detailed topic  Esc close  ':key' matches keys only  ";

            return ftxui::window(ftxui::text(" ? Help "),
                                ftxui::vbox({
                                    ftxui::hbox({
                                        ftxui::text(" Filter: " + filter_ + "_"),
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
        //!

        /*!
            Render Topic

            Fill the help dialog with the full help topic.

            !_method
        */
        ftxui::Element RenderTopic()
        {
            const int total = static_cast<int>(topic_lines_.size());
            const int top = std::min(topic_scroll_, MaxTopicTop());
            const int count = std::min(kVisibleRows, std::max(0, total - top));
            const int row_width = InnerWidth();

            ftxui::Elements rows;
            if (total == 0)
            {
                rows.push_back(ftxui::text(PadRight("  ", row_width)));
            }
            else
            {
                for (int i = 0; i < count; ++i)
                {
                    rows.push_back(ftxui::text(PadRight(" " + topic_lines_[top + i], row_width)));
                }
            }
            while (static_cast<int>(rows.size()) < kVisibleRows)
            {
                rows.push_back(ftxui::text(PadRight(" " + std::string(row_width - 1, ' '), row_width)));
            }

            const std::string footer =
                "  " + std::to_string(total == 0 ? 0 : top + 1) + "-" +
                std::to_string(top + count) + " of " + std::to_string(total) +
                "    Up/Down/PgUp/PgDn scroll  Enter/Esc back  ";

            return ftxui::window(ftxui::text(" ? Help - " + topic_title_ + " "),
                                ftxui::vbox({
                                    ftxui::hbox({
                                        ftxui::text(" " + topic_summary_),
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
        //!

        //!

    private:
        /*!
            Private Members
        */

        /*!
            Move Selection Up / Down

            !_method
        */
        void MoveSelection(int dir)
        {
            const int total = static_cast<int>(Filtered().size());
            if (total == 0) return;
            selection_ = std::max(0, std::min(total - 1, selection_ + dir));
        }
        //!

        /*!
            Refilter

            After the filter changes, select the first binding that matches.

            !_method
        */
        void Refilter()
        {
            selection_ = 0;
            scroll_ = 0;
        }
        //!

        /*!
            Inner Width Const

            The width of the dialog content: as wide as the widest binding
            line, but never wider than the dialog itself.

            !_method
        */
        int InnerWidth() const
        {
            return std::min(content_width_ + 2, kMaxInnerWidth);
        }
        //!

        /*!
            Max Topic Top Const

            !_method
        */
        int MaxTopicTop() const
        {
            return std::max(0, static_cast<int>(topic_lines_.size()) - kVisibleRows);
        }
        //!

        /*!
            Filtered Vector

        */
        //>>
        std::vector<int> Filtered() const
        {
            std::vector<int> result;

            std::string needle = Lower(filter_);
            bool key_only = false;
            if (!needle.empty() && needle[0] == ':')
            {
                key_only = true;
                needle = needle.substr(1);
            }

            for (std::size_t i = 0; i < entries_.size(); ++i)
            {
                const std::string& hay = key_only ? entries_[i].key : entries_[i].line;
                if (needle.empty() || Lower(hay).find(needle) != std::string::npos)
                {
                    result.push_back(static_cast<int>(i));
                }
            }
            return result;
        }
        //<<
        //!

        /*!
            Variables

            ----
        */
        //>>
        std::shared_ptr<EditorState> state_;
        bool* show_;
        std::vector<HelpEntry> entries_;
        std::string filter_;
        int selection_ = 0;
        int scroll_ = 0;
        int content_width_ = 72;
        bool topic_open_ = false;
        std::string topic_title_;
        std::string topic_summary_;
        std::vector<std::string> topic_lines_;
        int topic_scroll_ = 0;
        static constexpr int kVisibleRows = 18;
        static constexpr int kMaxInnerWidth = 90;
        //<<
        //!

        //!
    };
    //!

    /*>
        Make Help Dialog

        !_method
    */
    ftxui::Component MakeHelpDialog(std::shared_ptr<EditorState> state,
                                    const std::string& config_path,
                                    bool* show)
    {
        return ftxui::Make<HelpDialog>(std::move(state), config_path, show);
    }
    /*|



        #config
    */
    //!
}
