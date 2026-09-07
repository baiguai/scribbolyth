#pragma once

#include <string>
#include <vector>

#include "../bookmark/bookmark.hpp"
#include "../treeview/tree_node.hpp"

namespace scribbolyth::html
{
    using treeview::TreeNode;

    // The theme material of an imported HTML file, captured verbatim from the
    // web app's own script declarations so a later export can reproduce it.
    // `current_theme` and `themes` hold the full `let currentTheme = ...;` /
    // `let themes = [...];` statements; `style_content` is the inner text of
    // the `<style id="theme_styles">` block. Empty strings mean the piece was
    // absent from the source; `present` is true when any piece was found.
    struct HtmlTheme
    {
        bool present = false;
        std::string current_theme;
        std::string themes;
        std::string style_content;
    };

    bool ImportHtmlFile(const std::string& path, std::vector<TreeNode>& roots,
                        std::vector<bookmark::Bookmark>* bookmarks = nullptr,
                        std::vector<std::string>* history = nullptr,
                        HtmlTheme* theme = nullptr);
    bool ExportHtmlFile(const std::string& template_path, const std::string& out_path,
                        const std::vector<TreeNode>& roots,
                        const std::vector<bookmark::Bookmark>& bookmarks,
                        const std::vector<std::string>& history,
                        const HtmlTheme* theme = nullptr);
}
