#pragma once

#include <functional>
#include <string>
#include <map>
#include <utility>
#include <vector>
#include "../bookmark/bookmark.hpp"
#include "../mode/mode.hpp"
#include "../keyboard/keymap.hpp"
#include "../op/op.hpp"
#include "../treeview/tree_node.hpp"

inline constexpr int kDefaultTreeviewWidth = 30;
inline constexpr int kMinTreeviewWidth = 10;
inline constexpr int kMaxTreeviewWidth = 200;

struct EditorState {
    Mode mode = Mode::TREE;
    std::string command_buffer;
    int command_cursor = 0;
    int* active_child = nullptr;
    Mode mode_before_command = Mode::TREE;

    int treeview_width = kDefaultTreeviewWidth;

    scribbolyth::treeview::TreeNode* active_node = nullptr;
    std::string status;
    std::string template_path;
    std::vector<scribbolyth::bookmark::Bookmark> bookmarks;

    // Viewed-node history: node ids of every selected node that has text,
    // most recent last, deduplicated, capped at kHistoryMax. Populated by
    // the treeview; the history dialog resolves ids to live nodes.
    std::vector<std::string> history;
    static constexpr std::size_t kHistoryMax = 30;

    std::function<void()> focus_editor;
    std::function<void()> focus_treeview;

    // Flat enumeration of the document: (node pointer, depth) in document
    // order, ignoring expansion state. Set by the treeview.
    std::function<std::vector<std::pair<scribbolyth::treeview::TreeNode*, int>>()> collect_all_nodes;

    // Select `node` in the tree, expanding every ancestor so it is visible.
    // Set by the treeview.
    std::function<void(scribbolyth::treeview::TreeNode*)> reveal_node;

    // Position the editor cursor on a 0-based line of the currently active
    // node. Set by the editor.
    std::function<void(int)> reveal_line;

    std::map<std::string, scribbolyth::op::Operation> operations;
    std::map<std::string, std::string> commands;

    Keymap normal_keymap;
    Keymap insert_keymap;
    Keymap visual_keymap;
    Keymap tree_keymap;

    Keymap& ActiveKeymap() {
        switch (mode) {
            case Mode::TREE:        return tree_keymap;
            case Mode::INSERT:      return insert_keymap;
            case Mode::VISUAL:
            case Mode::VISUAL_LINE: return visual_keymap;
            case Mode::COMMAND:     return normal_keymap;
            default:                return normal_keymap;
        }
    }
};
