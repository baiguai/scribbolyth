#pragma once

#include <functional>
#include <memory>
#include <string>

struct EditorState;

namespace scribbolyth::op
{
    using Operation = std::function<void(const std::string& args, int count)>;
    void OpenCommandLine(std::shared_ptr<EditorState> state, const std::string& command);
    bool ExecuteCommand(std::shared_ptr<EditorState> state, const std::string& input);
}
