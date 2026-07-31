#pragma once

enum class Action {
    MoveUp, MoveDown, MoveLeft, MoveRight,
    MovePageUp, MovePageDown,
    MoveLineStart, MoveLineEnd,
    MoveFileStart, MoveFileEnd,
    MoveWordForward, MoveWordBack,
    InsertNewline, InsertTab,
    DeleteChar, BackspaceChar,
    EnterInsert, EnterNormal, EnterVisual, EnterVisualLine, EnterCommand,
    EnterTree,
    Yank, DeleteLine, Paste,
    Undo, Redo,
    Save, Quit, SearchForward, SearchBackward,
    TreeOpen, TreeCollapse, TreeExpand,
    None,
};
