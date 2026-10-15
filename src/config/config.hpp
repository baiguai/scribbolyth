#pragma once

#include <memory>
#include <string>
#include <vector>

struct EditorState;

namespace scribbolyth::config
{
    bool LoadConfig(const std::string& path, std::shared_ptr<EditorState> state);

    // Read the app's init config. On success `last_file` holds the value of
    // the `last_file` setting (cleared when the setting is absent/empty);
    // `open_dir` / `export_dir` hold the last-used directories for the
    // open/save family and the import/export family respectively (each
    // cleared when its setting is absent).
    bool ReadInit(const std::string& path, std::string& last_file,
                  std::vector<std::string>& recent_files,
                  std::string& open_dir, std::string& export_dir);

    // Persist `last_file` (may be empty) and the two last-used directories
    // to the app's init config.
    bool WriteInit(const std::string& path, const std::string& last_file,
                   const std::vector<std::string>& recent_files,
                   const std::string& open_dir, const std::string& export_dir);

    // persist the recent files order etc
    bool WriteRecentFiles(const std::string& path, const std::vector<std::string>& recent_files);
}
