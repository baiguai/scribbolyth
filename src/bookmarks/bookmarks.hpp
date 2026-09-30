#pragma once

#include <memory>

#include <ftxui/component/component.hpp>

/*!
    Forward declares the EditorState Struct and MakeBookmarksDialog.
*/

struct EditorState;

namespace scribbolyth::bookmarks
{
    ftxui::Component MakeBookmarksDialog(std::shared_ptr<EditorState> state, bool* show);
}

/*!*/
