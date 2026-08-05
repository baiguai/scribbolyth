#pragma once

#include <string>
#include <vector>

namespace scribbolyth::treeview
{

    struct TreeNode
    {
        std::string id;
        std::string name;
        bool expanded;
        std::string text;
        std::vector<TreeNode> children;
    };

}
