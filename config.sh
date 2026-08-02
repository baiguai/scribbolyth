#!/bin/bash

# Central configuration - edit values here, all scripts pick them up

APP_NAME="scribbolyth"

SOURCES=(
    "src/main.cpp"
    "src/editor/editor.cpp"
    "src/treeview/treeview.cpp"
    "src/keyboard/keymap.cpp"
    "src/keyboard/default_keymaps.cpp"
    "src/command/command.cpp"
    "src/op/op.cpp"
)

LIBS=(
    "ftxui::screen"
    "ftxui::dom"
    "ftxui::component"
)
