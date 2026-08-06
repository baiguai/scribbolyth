#pragma once

#include <functional>
#include <memory>
#include <string>

#include "../keyboard/keymap.hpp"

struct EditorState;

namespace scribbolyth::op
{
    using Operation = std::function<void(const std::string& args, int count)>;
    void OpenCommandLine(std::shared_ptr<EditorState> state, const std::string& command);
    // Like OpenCommandLine, but with pre-filled arguments (e.g. a directory
    // path the user is meant to extend with a filename).
    void OpenCommandLineWithArgs(std::shared_ptr<EditorState> state,
                                 const std::string& command, const std::string& args);
    bool ExecuteCommand(std::shared_ptr<EditorState> state, const std::string& input);

    // Tab-complete the pending command line (command name or file path).
    void CompleteCommand(std::shared_ptr<EditorState> state);

    // Resolve a key event through the active keymap without dispatching.
    Keymap::Result Resolve(std::shared_ptr<EditorState> state, ftxui::Event event);

    // Dispatch a resolved result. Consumes pending sequences and config ops;
    // returns false for unbound events so callers can fall back.
    bool Dispatch(std::shared_ptr<EditorState> state, const Keymap::Result& result);

    // Convenience wrapper: Resolve + Dispatch.
    bool HandleKey(std::shared_ptr<EditorState> state, ftxui::Event event);
}
