#include "command.hpp"

namespace scribbolyth::command
{
    void HandleRename(const std::string& args, std::shared_ptr<EditorState> state)
    {
        if (args.empty() || !state->rename_node)
        {
            return;
        }
        state->rename_node(args);
        state->mode = Mode::TREE;
        if (state->focus_treeview) state->focus_treeview();
    }

    bool Execute(const std::string& input, std::shared_ptr<EditorState> state)
    {
        std::string cmd = input;
        if (!cmd.empty() && cmd[0] == ':')
        {
            cmd = cmd.substr(1);
        }
        std::size_t start = cmd.find_first_not_of(" \t");
        if (start == std::string::npos)
        {
            return false;
        }
        cmd = cmd.substr(start);
        std::size_t space = cmd.find_first_of(" \t");
        std::string name = cmd.substr(0, space);
        std::string args = (space == std::string::npos) ? "" : cmd.substr(space + 1);

        if (name == "rename")
        {
            HandleRename(args, state);
            return true;
        }
        return false;
    }
}
