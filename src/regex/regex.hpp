#pragma once

#include <memory>
#include <string>

#include <ftxui/component/component.hpp>

struct EditorState;

namespace scribbolyth::regex
{
    struct RegexEntry
    {
        std::string category;
        std::string pattern;
        std::string example;
        std::string description;
        std::string line;   // the padded diagram line shown in the dialog
    };

    // Regex cheat sheet dialog. Reads its entries from `config_path` (the
    // same whitespace-separated format used by commands.conf: CATEGORY,
    // PATTERN, EXAMPLE, then an optionally quoted DESCRIPTION). The dialog is
    // shown while *show is true; Escape hides it. While shown it consumes
    // every event so no app key bindings fire.
    ftxui::Component MakeRegexDialog(std::shared_ptr<EditorState> state,
                                     const std::string& config_path,
                                     bool* show);
}