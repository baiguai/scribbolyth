#pragma once

#include <string>
#include <vector>

#include "../treeview/tree_node.hpp"

namespace scribbolyth::io
{
    using treeview::TreeNode;

    std::string Serialize(const std::vector<TreeNode>& roots);
    bool Deserialize(const std::string& json, std::vector<TreeNode>& roots);

    bool WriteFile(const std::string& path, const std::string& content);
    bool ReadFile(const std::string& path, std::string& content);
}
