#pragma once

#include <memory>
#include <string>
#include "../editor/editor_state.hpp"

namespace scribbolyth::command
{
    bool Execute(const std::string& input, std::shared_ptr<EditorState> state);
}
