#include "op.hpp"

#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

#include "../editor/editor_state.hpp"
#include "../mode/mode.hpp"
#include "../search/search.hpp"

namespace scribbolyth::op
{
    /*[./Introduction to Op.cpp]
        This file handles a lot of the command inputs.
        It is responsible for opening the command prompt, as well as executing commands that are entered.

        #command
    */

    namespace
    {
        /*!
            Is Path Command

            !_method
        */
        bool IsPathCommand(const std::string& name)
        {
            return name == "open" || name == "o" || name == "o!"
                || name == "saveas"
                || name == "U" || name == "X" || name == "x"
                || name == "export_note_txt" || name == "export_tree_txt";
        }
        //!

        /*!
            Directory Prefix

            !_method

            Turn a stored directory into the text inserted after the command
            name in a prompt: the directory plus one trailing separator, or ""
            when no directory is known (or it already ends in a separator).
            A trailing separator makes the file browser treat the argument as
            a folder to open.
        */
        std::string DirPrefix(const std::string& dir)
        {
            if (dir.empty()) return "";
            const char sep = static_cast<char>(std::filesystem::path::preferred_separator);
            if (dir.back() == sep || dir.back() == '/' || dir.back() == '\\') return dir;
            return dir + sep;
        }
        //!

        /*!
            Is Path Operation

            !_method
        */
        bool IsPathOp(const std::string& op)
        {
            return op == "open" || op == "open_force"
                || op == "import_html"
                || op == "saveas" || op == "export_html"
                || op == "export_scribbolyth"
                || op == "export_note_txt" || op == "export_tree_txt";
        }
        //!

        /*!
            Default Directory For Operation

            !_method

            Every path prompt starts at the filesystem root. The browser's
            directory bookmarks provide the shortcuts that the old last-used
            directory defaults used to supply.

            Returns "" for a non-path operation.
        */
        std::string DefaultDirForOp(const std::string& op)
        {
            return IsPathOp(op) ? DirPrefix("/") : "";
        }
        //!

        /*!
            Last Directory For Command

            !_method

            The default directory (with trailing separator) for the path
            command `name`, or "" when the command is not a path command or no
            directory is known yet. Used to prefill the command line when Tab
            completes a path command name.
        */
        std::string LastDirForCommand(const std::shared_ptr<EditorState>& state,
                                      const std::string& name)
        {
            auto it = state->commands.find(name);
            if (it == state->commands.end()) return "";
            return DefaultDirForOp(it->second);
        }
        //!

        /*!
            Collect Path Matches

            !_method
        */
        void CollectPathMatches(const std::string& arg, std::vector<std::string>& matches)
        {
            namespace fs = std::filesystem;

            // Decompose the argument into (directory, filename prefix) using
            // std::filesystem so each platform handles its own conventions:
            // '/' and '\' separators, drive letters (C:\), trailing slashes.
            const fs::path p(arg);
            const std::string prefix = p.filename().string();
            const std::string parent = p.parent_path().string();

            // Join candidates with the separator the user is typing. A '/'
            // anywhere wins even on Windows, where both separators are valid.
            char sep = fs::path::preferred_separator;
            if (parent.find('/') != std::string::npos) sep = '/';

            const std::string search = parent.empty() ? "." : parent;

            std::error_code ec;
            for (auto it = fs::directory_iterator(search, ec), end = fs::directory_iterator();
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
                std::string full;
                if (parent.empty())
                {
                    full = entry;
                }
                else if (parent.back() == '/' || parent.back() == '\\')
                {
                    full = parent + entry;   // parent is already a root ("/", "C:\")
                }
                else
                {
                    full = parent + sep + entry;
                }
                if (is_dir) full += sep;
                matches.push_back(std::move(full));
            }
            std::sort(matches.begin(), matches.end());
        }
        //!

        /*!
            Common Prefix

            !_method
        */
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
        //!

        /*!
            Join

            !_method
        */
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
        //!
    }

    /*!
        Complete Command

        !_method
    */
    void CompleteCommand(std::shared_ptr<EditorState> state)
    {
        std::string cmd = state->command_buffer;
        if (!cmd.empty() && cmd[0] == '/') return;  // searching, not a command
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

            // Decide whether the typed text now resolves to one command name.
            std::string resolved;
            if (std::find(matches.begin(), matches.end(), cmd) != matches.end())
            {
                resolved = cmd;
            }
            else
            {
                const std::string common = CommonPrefix(matches);
                if (common != cmd
                    && std::find(matches.begin(), matches.end(), common) != matches.end())
                {
                    resolved = common;
                }
            }

            state->command_path_prefix.clear();
            if (!resolved.empty())
            {
                // Command name is settled: leave a space for the argument and,
                // for a path command, pre-fill the root so Tab lands there
                // (`:open /`, `:U /` etc.).
                std::string completed = resolved + " ";
                const std::string dir = LastDirForCommand(state, resolved);
                if (!dir.empty())
                {
                    completed += dir;
                    state->command_path_prefix = dir;
                }
                state->command_buffer = ":" + completed;
            }
            else
            {
                // Still ambiguous: extend only to the shared prefix.
                state->command_buffer = ":" + CommonPrefix(matches);
            }
            state->command_cursor = static_cast<int>(state->command_buffer.size());
            if (matches.size() > 1) state->status = "Matches: " + Join(matches);
            return;
        }

        std::string name = cmd.substr(0, space);
        if (!IsPathCommand(name)) return;

        std::string arg = cmd.substr(space + 1);
        if (arg.empty())
        {
            // Tab on an empty path argument drops in the root directory;
            // the browser / further Tab-completion then work from there.
            const std::string dir = LastDirForCommand(state, name);
            if (!dir.empty())
            {
                state->command_buffer = ":" + name + " " + dir;
                state->command_path_prefix = dir;
                state->command_cursor = static_cast<int>(state->command_buffer.size());
                return;
            }
            // No root prefix: fall through and list the working dir.
        }

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
    //!

    /*!
        Open Command Line

        !_method
    */
    void OpenCommandLine(std::shared_ptr<EditorState> state, const std::string& command,
                         const std::string& initial_prefix)
    {
        state->mode_before_command = state->mode;
        state->mode = Mode::COMMAND;
        state->command_buffer = command.empty() ? ":" : (":" + command + " ");
        // Pre-fill the path prompt (path operations use the root, e.g.
        // `:open /`). Remember exactly what we inserted so the '\' handler
        // in main.cpp can remove it again.
        state->command_path_prefix.clear();
        if (!command.empty() && !initial_prefix.empty())
        {
            state->command_buffer += initial_prefix;
            state->command_path_prefix = initial_prefix;
        }
        state->command_cursor = static_cast<int>(state->command_buffer.size());
        if (state->active_child) *state->active_child = 1;
    }
    //!

    /*!
        Open Search Command

        !_method
    */
    void OpenSearchCommand(std::shared_ptr<EditorState> state)
    {
        state->mode_before_command = state->mode;
        state->mode = Mode::COMMAND;
        state->command_buffer = "/";
        state->command_cursor = static_cast<int>(state->command_buffer.size());
        if (state->active_child) *state->active_child = 1;
    }
    //!

    /*!
        Open Command Line With Args

        !_method
    */
    void OpenCommandLineWithArgs(std::shared_ptr<EditorState> state,
                                 const std::string& command, const std::string& args)
    {
        state->mode_before_command = state->mode;
        state->mode = Mode::COMMAND;
        state->command_buffer = ":" + command + (args.empty() ? "" : " " + args);
        state->command_cursor = static_cast<int>(state->command_buffer.size());
        if (state->active_child) *state->active_child = 1;
    }
    //!

    /*!
        Execute Command

        !_method
    */
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
    //!

    /*!
        Execute Search

        !_method
    */
    bool ExecuteSearch(std::shared_ptr<EditorState> state, const std::string& input)
    {
        if (input.empty() || input[0] != '/') return false;

        // An empty query re-runs the previous search, as in Vim.
        std::string raw = input.substr(1);
        if (raw.empty())
        {
            if (state->search_query.empty())
            {
                state->status = "No previous search";
                return true;
            }
            raw = state->search_query;
        }

        state->search_query = raw;
        state->search_matches = scribbolyth::search::FindMatches(state, raw);
        state->search_index = -1;

        if (state->search_matches.empty())
        {
            state->search_active = true;
            state->status = "Pattern not found: " + raw;
            return true;
        }

        state->search_active = true;
        state->search_index = 0;
        state->search_reveal_pending = true;
        if (state->reveal_node) state->reveal_node(state->search_matches[0]);
        state->status = "Match 1 of " + std::to_string(state->search_matches.size());
        return true;
    }
    //!

    /*!
        Resolve

        !_method
    */
    Keymap::Result Resolve(std::shared_ptr<EditorState> state, ftxui::Event event)
    {
        return state->ActiveKeymap().Handle(event);
    }
    //!

    /*!
        Dispatch

        !_method
    */
    bool Dispatch(std::shared_ptr<EditorState> state, const Keymap::Result& result)
    {
        if (result.pending)
            return true;
        if (result.op.empty())
            return false;

        if (result.args == "prompt" && !result.command.empty() && result.command != "-")
        {
            // Path prompts start at the root; bookmarked directories in the
            // browser provide quick access to the folders the user uses.
            OpenCommandLine(state, result.command, DefaultDirForOp(result.op));
            return true;
        }

        auto it = state->operations.find(result.op);
        if (it == state->operations.end())
            return false;
        it->second((result.args == "-") ? "" : result.args, result.count);
        return true;
    }
    //!


    /*!
        Handle Key

        !_method
    */
    bool HandleKey(std::shared_ptr<EditorState> state, ftxui::Event event)
    {
        // Some terminals deliver Enter as an ESC sequence instead of CR/LF:
        // SS3 "ESC O M" (application-keypad Enter) and CSI "ESC [ 1 9 ~".
        // Normalize them so Return bindings fire in every mode.
        if (event == ftxui::Event::Special("\x1bOM")
            || event == ftxui::Event::Special("\x1b[19~"))
        {
            event = ftxui::Event::Return;
        }
        return Dispatch(state, Resolve(state, event));
    }
    //!
}
