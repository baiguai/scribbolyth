#pragma once

#include <string>
#include <vector>

#include "../bookmark/bookmark.hpp"
#include "../treeview/tree_node.hpp"

namespace scribbolyth::io
{
    using treeview::TreeNode;

    // Magic line that every written Scribbolyth document begins with, so a
    // file can be identified as one without parsing it. Files saved before
    // this existed (no signature) still load for now; a future release will
    // reject documents that lack it.
    inline constexpr const char* kFileSignature = "//scribbolyth\n";

    std::string Serialize(const std::vector<TreeNode>& roots, int tree_width,
                          const std::vector<bookmark::Bookmark>& bookmarks,
                          const std::vector<std::string>& history);
    // On success `tree_width` is set only if the document carried one;
    // `bookmarks`/`history` are replaced only if the document carried any.
    // Otherwise they are left untouched (callers should pre-set defaults).
    // The content must begin with the kFileSignature line.
    bool Deserialize(const std::string& json, std::vector<TreeNode>& roots,
                     int* tree_width = nullptr,
                     std::vector<bookmark::Bookmark>* bookmarks = nullptr,
                     std::vector<std::string>* history = nullptr);

    // Whether `content` begins with the Scribbolyth file signature.
    bool HasFileSignature(const std::string& content);

    std::string JsonEscape(const std::string& s);

    bool WriteFile(const std::string& path, const std::string& content);
    bool ReadFile(const std::string& path, std::string& content);
}
