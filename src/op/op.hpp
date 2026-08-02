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
    bool ExecuteCommand(std::shared_ptr<EditorState> state, const std::string& input);

    // Resolve a key event through the active keymap without dispatching.
    Keymap::Result Resolve(std::shared_ptr<EditorState> state, ftxui::Event event);

    // Dispatch a resolved result. Consumes pending sequences and config ops;
    // returns false for unbound events so callers can fall back.
    bool Dispatch(std::shared_ptr<EditorState> state, const Keymap::Result& result);

    // Convenience wrapper: Resolve + Dispatch.
    bool HandleKey(std::shared_ptr<EditorState> state, ftxui::Event event);
}
