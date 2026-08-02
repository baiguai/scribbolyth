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
}
