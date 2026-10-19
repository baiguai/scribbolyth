#pragma once

#include <memory>
#include <string>
#include <vector>

struct EditorState;

namespace scribbolyth::config
{
    /*!
        LoadConfig Forward Declaration

    */
    //>>
    bool LoadConfig(const std::string& path, std::shared_ptr<EditorState> state);
    //<<
    //!

    /*!
        ReadInit Forward Declaration

    */
    //>>
    bool ReadInit(const std::string& path, std::string& last_file,
                  std::vector<std::string>& recent_files,
                  std::string& open_dir, std::string& import_dir,
                  int& status_timeout);
    //<<
    //!

    /*!
        WriteInit Forward Declaration

    */
    //>>
    bool WriteInit(const std::string& path, const std::string& last_file,
                   const std::vector<std::string>& recent_files,
                   const std::string& open_dir, const std::string& import_dir,
                   int status_timeout);
    //<<
    //!

    /*!
        WriteRecentFiles Forward Declaration

    */
    //>>
    bool WriteRecentFiles(const std::string& path, const std::vector<std::string>& recent_files);
    //<<
    //!
}
