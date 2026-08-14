#pragma once

#include <memory>

#include <ftxui/component/component.hpp>

struct EditorState;

namespace scribbolyth::brokenlinks
{
    ftxui::Component MakeDeadLinksDialog(std::shared_ptr<EditorState>, bool* show);
}
