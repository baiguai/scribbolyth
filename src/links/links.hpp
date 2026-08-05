#pragma once

#include <memory>

#include <ftxui/component/component.hpp>

struct EditorState;

namespace scribbolyth::links
{
    // Build the links dialog. While *show is true it consumes every event, so
    // no app key bindings fire. It lists the links found in the active node's
    // text, mirroring the HTML app: markdown-style `[label] <url|_Note_>`
    // pairs, standalone http(s)/file URLs, and `_Note_` references. j/k (and
    // ArrowUp/ArrowDown) move the selection, Enter or a double-click on a row
    // activates the selected link (URLs open in the external browser via
    // xdg-open, `_Note_` links reveal the matching node in the tree), y copies
    // the selected link to the clipboard (URLs verbatim, notes as `_Title_`),
    // Escape cancels. Links whose note cannot be found render dimmed and are
    // skipped.
    ftxui::Component MakeLinksDialog(std::shared_ptr<EditorState> state, bool* show);
}
