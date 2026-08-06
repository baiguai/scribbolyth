#pragma once

#include <memory>

#include <ftxui/component/component.hpp>

struct EditorState;

namespace scribbolyth::recent
{
    ftxui::Component MakeRecentDialog(std::shared_ptr<EditorState> state, bool* show);
}
