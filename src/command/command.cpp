#include "command.hpp"

/*

    ---- Adding New Commands ---------------------------------------------------

    ~/src/editor/editor_state.hpp
        * Add a new callback:
            std::function<void()> cmd_name;
            std::function<void(const std::string&)> cmd_name;

    ~/src/treeview/treeview.cpp
        * Add a Constructor:
            state_->new_folder = [this](const std::string& name)
            {
                if (!name.empty()) InsertFolder(name);
            };

        * Declare any functions you'll need (in privates):
            void NewCmd();
            void InsertNewCmd(const std::string& name);

        * Create the functions you need.

    ~/src/command/command.cpp
        * Add the Handler:
            void HandleCreateFolder(const std::string& args, std::shared_ptr<EditorState> state)
            {
                if (!args.empty() && state->new_folder)
                {
                    state->new_folder(args);
                }
                state->mode = Mode::TREE;
                if (state->focus_treeview) state->focus_treeview();
            }

        * Call the Handler:
            if (name == "create_folder")
            {
                HandleCreateFolder(args, state);
                return true;
            }

    ----------------------------------------------------------------------------

*/

namespace scribbolyth::command
{
    void HandleRename(const std::string& args, std::shared_ptr<EditorState> state)
    {
        if (!args.empty() && state->rename_node)
        {
            state->rename_node(args);
        }
        state->mode = Mode::TREE;
        if (state->focus_treeview) state->focus_treeview();
    }
    void HandleCreateFolder(const std::string& args, std::shared_ptr<EditorState> state)
    {
        if (!args.empty() && state->new_folder)
        {
            state->new_folder(args);
        }
        state->mode = Mode::TREE;
        if (state->focus_treeview) state->focus_treeview();
    }
    void HandleCreateNote(const std::string& args, std::shared_ptr<EditorState> state)
    {
        if (!args.empty() && state->new_note)
        {
            state->new_note(args);
        }
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
        if (name == "create_folder")
        {
            HandleCreateFolder(args, state);
            return true;
        }
        if (name == "create_note")
        {
            HandleCreateNote(args, state);
            return true;
        }
        return false;
    }
}
