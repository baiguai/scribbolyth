#pragma once

#include <memory>
#include <string>

struct EditorState;

namespace scribbolyth::config
{
    bool LoadConfig(const std::string& path, std::shared_ptr<EditorState> state);
}
