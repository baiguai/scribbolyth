#include "op.hpp"

#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

#include "../editor/editor_state.hpp"
#include "../mode/mode.hpp"

namespace scribbolyth::op
{
    namespace
    {
        bool IsPathCommand(const std::string& name)
        {
            return name == "open" || name == "o" || name == "saveas";
        }

        void CollectPathMatches(const std::string& arg, std::vector<std::string>& matches)
        {
            namespace fs = std::filesystem;

            std::string dir;
            std::string prefix;
            std::size_t slash = arg.find_last_of('/');
            if (slash == std::string::npos)
            {
                prefix = arg;
            }
            else if (slash == arg.size() - 1)
            {
                std::string d = arg.substr(0, slash);
                dir = d.empty() ? "/" : d;
            }
            else
            {
                dir = arg.substr(0, slash);
                if (dir.empty() && !arg.empty() && arg[0] == '/') dir = "/";
                prefix = arg.substr(slash + 1);
            }

            std::string search_dir = dir.empty() ? "." : dir;
            std::error_code ec;
            for (auto it = fs::directory_iterator(search_dir, ec), end = fs::directory_iterator();
                 it != end && !ec; it.increment(ec))
            {
                std::string entry = it->path().filename().string();
                if (entry.empty()) continue;
                if (entry[0] == '.')
                {
                    if (prefix.empty() || prefix[0] != '.') continue;
                }
                if (!prefix.empty() && entry.compare(0, prefix.size(), prefix) != 0) continue;

                std::error_code ec2;
                bool is_dir = it->is_directory(ec2);
                std::string full = dir.empty() ? entry : (dir + "/" + entry);
                if (is_dir) full += "/";
                matches.push_back(std::move(full));
            }
            std::sort(matches.begin(), matches.end());
        }

        std::string CommonPrefix(const std::vector<std::string>& items)
        {
            if (items.empty()) return "";
            std::string common = items[0];
            for (std::size_t i = 1; i < items.size(); ++i)
            {
                std::size_t j = 0;
                while (j < common.size() && j < items[i].size() && common[j] == items[i][j]) ++j;
                common = common.substr(0, j);
            }
            return common;
        }

        std::string Join(const std::vector<std::string>& items)
        {
            std::string out;
            for (std::size_t i = 0; i < items.size(); ++i)
            {
                if (i != 0) out += ' ';
                out += items[i];
            }
            return out;
        }
    }

    void CompleteCommand(std::shared_ptr<EditorState> state)
    {
        std::string cmd = state->command_buffer;
        if (!cmd.empty() && cmd[0] == ':') cmd.erase(0, 1);

        std::size_t space = cmd.find(' ');
        if (space == std::string::npos)
        {
            std::vector<std::string> matches;
            for (const auto& [name, op] : state->commands)
            {
                if (name.compare(0, cmd.size(), cmd) == 0) matches.push_back(name);
            }
            if (matches.empty()) return;
            std::sort(matches.begin(), matches.end());

            std::string completed;
            if (std::find(matches.begin(), matches.end(), cmd) != matches.end())
            {
                completed = cmd + " ";
            }
            else
            {
                std::string common = CommonPrefix(matches);
                completed = (common == cmd) ? cmd : (common + " ");
            }
            state->command_buffer = ":" + completed;
            state->command_cursor = static_cast<int>(state->command_buffer.size());
            if (matches.size() > 1) state->status = "Matches: " + Join(matches);
            return;
        }

        std::string name = cmd.substr(0, space);
        if (!IsPathCommand(name)) return;

        std::string arg = cmd.substr(space + 1);
        std::vector<std::string> matches;
        CollectPathMatches(arg, matches);
        if (matches.empty())
        {
            state->status = "No match for: " + arg;
            return;
        }

        std::string completed;
        if (matches.size() == 1)
        {
            completed = matches[0];
        }
        else
        {
            completed = CommonPrefix(matches);
            state->status = "Matches: " + Join(matches);
        }
        if (completed == arg) return;

        state->command_buffer = ":" + name + " " + completed;
        state->command_cursor = static_cast<int>(state->command_buffer.size());
    }

    void OpenCommandLine(std::shared_ptr<EditorState> state, const std::string& command)
    {
        state->mode_before_command = state->mode;
        state->mode = Mode::COMMAND;
        state->command_buffer = command.empty() ? ":" : (":" + command + " ");
        state->command_cursor = static_cast<int>(state->command_buffer.size());
        if (state->active_child) *state->active_child = 1;
    }

    bool ExecuteCommand(std::shared_ptr<EditorState> state, const std::string& input)
    {
        std::string cmd = input;
        if (!cmd.empty() && cmd[0] == ':') cmd = cmd.substr(1);

        std::size_t start = cmd.find_first_not_of(" \t");
        if (start == std::string::npos) return false;

        cmd = cmd.substr(start);
        std::size_t space = cmd.find_first_of(" \t");
        std::string name = cmd.substr(0, space);
        std::string args = (space == std::string::npos) ? "" : cmd.substr(space + 1);

        auto cmd_it = state->commands.find(name);
        if (cmd_it == state->commands.end()) return false;
        auto op_it = state->operations.find(cmd_it->second);
        if (op_it == state->operations.end()) return false;
        op_it->second(args, 1);
        return true;
    }

    Keymap::Result Resolve(std::shared_ptr<EditorState> state, ftxui::Event event)
    {
        return state->ActiveKeymap().Handle(event);
    }

    bool Dispatch(std::shared_ptr<EditorState> state, const Keymap::Result& result)
    {
        if (result.pending)
            return true;
        if (result.op.empty())
            return false;

        if (result.args == "prompt" && !result.command.empty() && result.command != "-")
        {
            OpenCommandLine(state, result.command);
            return true;
        }

        auto it = state->operations.find(result.op);
        if (it == state->operations.end())
            return false;
        it->second((result.args == "-") ? "" : result.args, result.count);
        return true;
    }

    bool HandleKey(std::shared_ptr<EditorState> state, ftxui::Event event)
    {
        return Dispatch(state, Resolve(state, event));
    }
}
