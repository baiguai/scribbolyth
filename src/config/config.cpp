#include "config.hpp"

#include <fstream>
#include <string>
#include <vector>

#include <ftxui/component/event.hpp>

#include "../editor/editor_state.hpp"
#include "../op/op.hpp"

namespace scribbolyth::config
{
    namespace
    {
        using ftxui::Event;

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

        bool ParseKey(const std::string& token, Event& out)
        {
            if (token == "Esc")         { out = Event::Escape;    return true; }
            if (token == "Return")      { out = Event::Return;    return true; }
            if (token == "Tab")         { out = Event::Tab;       return true; }
            if (token == "Backspace")   { out = Event::Backspace; return true; }
            if (token == "Space")       { out = Event::Character(' '); return true; }
            if (token == "ArrowUp")     { out = Event::ArrowUp;   return true; }
            if (token == "ArrowDown")   { out = Event::ArrowDown; return true; }
            if (token == "ArrowLeft")   { out = Event::ArrowLeft; return true; }
            if (token == "ArrowRight")  { out = Event::ArrowRight; return true; }
            if (token == "PageUp")      { out = Event::PageUp;    return true; }
            if (token == "PageDown")    { out = Event::PageDown;  return true; }
            if (token == "Home")        { out = Event::Home;      return true; }
            if (token == "End")         { out = Event::End;       return true; }
            if (token.size() == 1)      { out = Event::Character(token[0]); return true; }
            return false;
        }

        Keymap& ModeKeymap(std::shared_ptr<EditorState> state, const std::string& mode)
        {
            if (mode == "TREE")   return state->tree_keymap;
            if (mode == "NORMAL") return state->normal_keymap;
            if (mode == "INSERT") return state->insert_keymap;
            return state->visual_keymap; // VISUAL
        }
    }

    bool LoadConfig(const std::string& path, std::shared_ptr<EditorState> state)
    {
        std::ifstream file(path);
        if (!file.is_open()) return false;

        std::string line;
        while (std::getline(file, line))
        {
            std::size_t hash = line.find_first_not_of(" \t");
            if (hash != std::string::npos && line[hash] == '#') line = line.substr(0, hash);
            auto fields = SplitFields(line);
            if (fields.empty()) continue;
            if (fields.size() != 7) return false;

            const std::string& mode     = fields[0];
            const std::string& key      = fields[1];
            const std::string& repeat   = fields[2];
            const std::string& command  = fields[3];
            const std::string& args     = fields[4];
            const std::string& op       = fields[5];

            if (state->operations.find(op) == state->operations.end()) return false;

            bool can_repeat = (repeat == "yes");

            if (command != "-") state->commands[command] = op;

            if (mode == "GLOBAL")
            {
                continue;
            }

            Keymap& km = ModeKeymap(state, mode);
            if (can_repeat) km.EnableCounts();

            auto key_parts = SplitFields(key);
            std::vector<Event> events;
            for (const auto& t : key_parts)
            {
                Event e;
                if (!ParseKey(t, e)) return false;
                events.push_back(e);
            }

            Binding binding{op, command, args, can_repeat};
            if (events.size() == 1) km.Bind(events[0], std::move(binding));
            else km.Bind(events, std::move(binding));
        }
        return true;
    }
}
