#pragma once

#include <memory>
#include <string>

#include <ftxui/component/component.hpp>

struct EditorState;

namespace scribbolyth::browser
{
    // Build the file-browser dialog. While *show is true it consumes every
    // event, so no app key bindings fire. The treeview sets
    // `state->browser_start_dir` and `state->browser_pick` before showing it
    // via a path command that received a directory argument (open/saveas/
    // import_html/export_html). Keys: j/k (or ArrowDown/ArrowUp) move the
    // selection, h (or Backspace) goes up one level, l or Enter enters a
    // directory, Enter picks the selected file (l does nothing on a file),
    // gg jumps to the first row, G to the last, Escape cancels. Picking a file
    // invokes `state->browser_pick` with its full path and closes the dialog.
    ftxui::Component MakeFileBrowserDialog(std::shared_ptr<EditorState> state, bool* show);
}
