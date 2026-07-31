#pragma once

#include <memory>
#include <string>
#include <vector>
#include "../editor/editor_state.hpp"
#include <ftxui/component/component.hpp>

namespace scribbolyth::treeview
{

    ftxui::Component MakeTreeView(std::shared_ptr<EditorState> state);

    struct TreeNode
    {
        std::string name;
        bool is_folder;
        bool expanded;
        std::vector<TreeNode> children;
    };

    TreeNode MakeRootFolder();

}
