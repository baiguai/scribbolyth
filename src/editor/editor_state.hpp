#pragma once

#include <functional>
#include <string>
#include "../mode/mode.hpp"
#include "../keyboard/default_keymaps.hpp"

struct EditorState {
    Mode mode = Mode::TREE;
    std::string command_buffer;
    int command_cursor = 0;
    int* active_child = nullptr;

    std::function<void()> focus_editor;
    std::function<void()> focus_treeview;
    std::function<void(const std::string&)> rename_node;

    Keymap normal_keymap;
    Keymap insert_keymap;
    Keymap visual_keymap;
    Keymap tree_keymap;

    EditorState()
        : normal_keymap(MakeNormalKeymap()),
          insert_keymap(MakeInsertKeymap()),
          visual_keymap(MakeVisualKeymap()),
          tree_keymap(MakeTreeKeymap()) {}

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
