#include "default_keymaps.hpp"

using namespace ftxui;

Keymap MakeNormalKeymap() {
    Keymap km;

    km.Bind(Event::ArrowUp,       Action::MoveUp);
    km.Bind(Event::ArrowDown,     Action::MoveDown);
    km.Bind(Event::ArrowLeft,     Action::MoveLeft);
    km.Bind(Event::ArrowRight,    Action::MoveRight);
    km.Bind(Event::PageUp,        Action::MovePageUp);
    km.Bind(Event::PageDown,      Action::MovePageDown);
    km.Bind(Event::Home,          Action::MoveLineStart);
    km.Bind(Event::End,           Action::MoveLineEnd);

    km.Bind(Event::Character('h'), Action::MoveLeft);
    km.Bind(Event::Character('j'), Action::MoveDown);
    km.Bind(Event::Character('k'), Action::MoveUp);
    km.Bind(Event::Character('l'), Action::MoveRight);
    km.Bind(Event::Character('i'), Action::EnterInsert);
    km.Bind(Event::Character('v'), Action::EnterVisual);
    km.Bind(Event::Character('V'), Action::EnterVisualLine);
    km.Bind(Event::Character('x'), Action::DeleteChar);
    km.Bind(Event::Character('u'), Action::Undo);
    km.Bind(Event::Character('y'), Action::Yank);
    km.Bind(Event::Character('p'), Action::Paste);
    km.Bind(Event::Character('w'), Action::MoveWordForward);
    km.Bind(Event::Character('b'), Action::MoveWordBack);
    km.Bind(Event::Character('0'), Action::MoveLineStart);
    km.Bind(Event::Character('$'), Action::MoveLineEnd);
    km.Bind(Event::Character('G'), Action::MoveFileEnd);
    km.Bind(Event::Character('d'), Action::DeleteLine);
    km.Bind(Event::Character('D'), Action::DeleteLine);

    km.Bind({Event::Character('g'), Event::Character('g')}, Action::MoveFileStart);
    km.Bind({Event::Character('d'), Event::Character('d')}, Action::DeleteLine);

    km.Bind(Event::Escape,        Action::EnterTree);
    km.Bind(Event::Character(':'), Action::EnterCommand);
    km.Bind(Event::Backspace,     Action::BackspaceChar);

    return km;
}

Keymap MakeInsertKeymap() {
    Keymap km;
    km.Bind(Event::Escape,        Action::EnterNormal);
    km.Bind(Event::Return,        Action::InsertNewline);
    km.Bind(Event::Backspace,     Action::BackspaceChar);
    km.Bind(Event::Tab,           Action::InsertTab);
    return km;
}

Keymap MakeVisualKeymap() {
    Keymap km;
    km.Bind(Event::ArrowUp,       Action::MoveUp);
    km.Bind(Event::ArrowDown,     Action::MoveDown);
    km.Bind(Event::ArrowLeft,     Action::MoveLeft);
    km.Bind(Event::ArrowRight,    Action::MoveRight);
    km.Bind(Event::Character('h'), Action::MoveLeft);
    km.Bind(Event::Character('j'), Action::MoveDown);
    km.Bind(Event::Character('k'), Action::MoveUp);
    km.Bind(Event::Character('l'), Action::MoveRight);
    km.Bind(Event::Character('y'), Action::Yank);
    km.Bind(Event::Character('d'), Action::DeleteLine);
    km.Bind(Event::Character('D'), Action::DeleteLine);
    km.Bind(Event::Escape,        Action::EnterNormal);
    km.Bind(Event::Character(':'), Action::EnterCommand);
    return km;
}

Keymap MakeTreeKeymap() {
    Keymap km;
    km.EnableCounts();
    km.Bind(Event::ArrowUp,       Action::MoveUp);
    km.Bind(Event::ArrowDown,     Action::MoveDown);
    km.Bind(Event::Character('k'), Action::MoveUp);
    km.Bind(Event::Character('j'), Action::MoveDown);
    km.Bind(Event::Character('G'), Action::MoveFileEnd);
    km.Bind({Event::Character('g'), Event::Character('g')}, Action::MoveFileStart);
    km.Bind(Event::Return,        Action::TreeOpen);
    km.Bind(Event::Character('h'), Action::TreeCollapse);
    km.Bind(Event::Character('l'), Action::TreeExpand);
    km.Bind(Event::Character('E'), Action::TreeExpandAll);
    km.Bind(Event::Character('C'), Action::TreeCollapseAll);
    km.Bind(Event::Character('f'), Action::TreeNewFolder);
    km.Bind(Event::Character('R'), Action::TreeRenameNode);
    km.Bind(Event::Character('i'), Action::EnterNormal);
    km.Bind(Event::Character('I'), Action::EnterInsert);
    km.Bind(Event::Character(':'), Action::EnterCommand);
    return km;
}
