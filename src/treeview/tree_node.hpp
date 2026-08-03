#pragma once

#include <string>
#include <vector>

namespace scribbolyth::treeview
{

    struct TreeNode
    {
        std::string name;
        bool is_folder;
        bool expanded;
        std::string text;
        std::vector<TreeNode> children;
    };

}
