#pragma once

#include <memory>
#include <string>
#include <vector>

#include <ftxui/component/component.hpp>

#include "../editor/editor_state.hpp"
#include "../treeview/tree_node.hpp"

namespace scribbolyth::undo
{
    // Rendered document outline (one line per node, all nodes regardless of
    // expansion), used for both snapshot previews and labels.
    std::string LineOf(const treeview::TreeNode& node, int depth);
    std::vector<std::string> DocLines(const std::vector<treeview::TreeNode>& roots);
    std::string FirstLine(const std::vector<treeview::TreeNode>& roots);

    ftxui::Component MakeUndoDialog(std::shared_ptr<EditorState> state, bool* show);
}
