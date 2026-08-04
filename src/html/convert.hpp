#pragma once

#include <string>
#include <vector>

#include "../treeview/tree_node.hpp"

namespace scribbolyth::html
{
    using treeview::TreeNode;

    bool ImportHtmlFile(const std::string& path, std::vector<TreeNode>& roots);
    bool ExportHtmlFile(const std::string& template_path, const std::string& out_path,
                        const std::vector<TreeNode>& roots);
}
