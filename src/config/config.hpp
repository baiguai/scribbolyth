#pragma once

#include <memory>
#include <string>

struct EditorState;

namespace scribbolyth::config
{
    bool LoadConfig(const std::string& path, std::shared_ptr<EditorState> state);

    // Read the app's init config. On success `last_file` holds the value of
    // the `last_file` setting (cleared when the setting is absent/empty).
    bool ReadInit(const std::string& path, std::string& last_file);

    // Persist `last_file` (may be empty) to the app's init config.
    bool WriteInit(const std::string& path, const std::string& last_file);
}
