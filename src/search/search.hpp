#pragma once

#include <memory>

#include <ftxui/component/component.hpp>

struct EditorState;

namespace scribbolyth::search
{
    // Build the node search/filter dialog. While *show is true it consumes
    // every event, so no app key bindings fire. ArrowUp/ArrowDown move the
    // selection, Enter jumps to the selected node (revealing it in the tree),
    // Escape cancels. A leading "r:" makes the query a case-insensitive regex;
    // otherwise it is a case-insensitive substring match against node titles
    // and content. The dialog keeps a fixed size regardless of result count.
    ftxui::Component MakeSearchDialog(std::shared_ptr<EditorState> state, bool* show);
}
