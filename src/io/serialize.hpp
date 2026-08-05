#pragma once

#include <string>
#include <vector>

#include "../bookmark/bookmark.hpp"
#include "../treeview/tree_node.hpp"

namespace scribbolyth::io
{
    using treeview::TreeNode;

    std::string Serialize(const std::vector<TreeNode>& roots, int tree_width,
                          const std::vector<bookmark::Bookmark>& bookmarks);
    // On success `tree_width` is set only if the document carried one;
    // `bookmarks` is replaced only if the document carried any. Otherwise they
    // are left untouched (callers should pre-set defaults).
    bool Deserialize(const std::string& json, std::vector<TreeNode>& roots,
                     int* tree_width = nullptr,
                     std::vector<bookmark::Bookmark>* bookmarks = nullptr);

    std::string JsonEscape(const std::string& s);

    bool WriteFile(const std::string& path, const std::string& content);
    bool ReadFile(const std::string& path, std::string& content);
}
