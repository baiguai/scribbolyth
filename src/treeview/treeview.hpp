#pragma once

#include <memory>
#include <string>
#include <vector>
#include "../editor/editor_state.hpp"
#include "tree_node.hpp"
#include <ftxui/component/component.hpp>

namespace scribbolyth::treeview
{

    /*!
        Make Treeview Forward Declaration
    */
    //>>
    ftxui::Component MakeTreeView(std::shared_ptr<EditorState> state);
    //<<
    //!

}
