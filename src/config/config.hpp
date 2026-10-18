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
    // `open_dir` / `import_dir` hold the last directory a document was opened
    // from and last imported from respectively (each cleared when its setting
    // is absent). `status_timeout` keeps the caller's value when the setting
    // is absent or not a number.
    bool ReadInit(const std::string& path, std::string& last_file,
                  std::vector<std::string>& recent_files,
                  std::string& open_dir, std::string& import_dir,
                  int& status_timeout);

    // Persist `last_file` (may be empty), the two remembered directories and
    // the status-bar message timeout to the app's init config.
    bool WriteInit(const std::string& path, const std::string& last_file,
                   const std::vector<std::string>& recent_files,
                   const std::string& open_dir, const std::string& import_dir,
                   int status_timeout);

    // persist the recent files order etc
    bool WriteRecentFiles(const std::string& path, const std::vector<std::string>& recent_files);
}
