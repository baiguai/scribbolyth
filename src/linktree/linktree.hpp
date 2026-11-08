#pragma once

#include <memory>

#include <ftxui/component/component.hpp>

struct EditorState;

namespace scribbolyth::linktree
{
    /*!
        Forward Declaration of the MakeLinkTreeDialog Method.

    */
    //>>
    ftxui::Component MakeLinkTreeDialog(std::shared_ptr<EditorState> state, bool* show);
    //<<
    /*|
        Build the link-tree dialog. While *show is true it consumes every
        event, so no app key bindings fire. The tree is built from inter-note
        links: a note's linked notes become its children, recursively.

        j/k (and ArrowUp/ArrowDown) move the selection, h/l fold and unfold a
        branch, gg/G jump to the top/bottom, Enter reveals the selected node
        and closes the dialog, '/' starts filtering (Escape cancels the
        filter), and Escape outside filtering closes the dialog.
    */
    //!
}
