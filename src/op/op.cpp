#include "op.hpp"

#include "../editor/editor_state.hpp"
#include "../mode/mode.hpp"

namespace scribbolyth::op
{
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
