#pragma once

#include <memory>
#include <string>
#include <vector>
#include "../editor/editor_state.hpp"
#include "tree_node.hpp"
#include <ftxui/component/component.hpp>

namespace scribbolyth::treeview
{

    ftxui::Component MakeTreeView(std::shared_ptr<EditorState> state, const std::string& default_file = "");

    std::vector<TreeNode> MakeRootNodes();

}
