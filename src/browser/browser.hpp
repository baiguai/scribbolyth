#pragma once

#include <memory>
#include <string>

#include <ftxui/component/component.hpp>

struct EditorState;

namespace scribbolyth::browser
{
    /*!
        Forward Declaration of the File Browser Dialog
    */
    //>>
    ftxui::Component MakeFileBrowserDialog(std::shared_ptr<EditorState> state, bool* show);
    //<<
    //!
}
