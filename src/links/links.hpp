#pragma once

#include <memory>

#include <ftxui/component/component.hpp>

struct EditorState;

namespace scribbolyth::links
{
    ftxui::Component MakeLinksDialog(std::shared_ptr<EditorState> state, bool* show);
}
