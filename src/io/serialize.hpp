#pragma once

#include <string>
#include <vector>

#include "../treeview/tree_node.hpp"

namespace scribbolyth::io
{
    using treeview::TreeNode;

    std::string Serialize(const std::vector<TreeNode>& roots, int tree_width);
    // On success `tree_width` is set only if the document carried one; otherwise
    // it is left untouched (callers should pre-set a default).
    bool Deserialize(const std::string& json, std::vector<TreeNode>& roots, int* tree_width = nullptr);

    std::string JsonEscape(const std::string& s);

    bool WriteFile(const std::string& path, const std::string& content);
    bool ReadFile(const std::string& path, std::string& content);
}
