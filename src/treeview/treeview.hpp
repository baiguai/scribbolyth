#pragma once

#include <memory>
#include "../editor/editor_state.hpp"
#include <ftxui/component/component.hpp>

namespace scribbolyth::treeview
{

    ftxui::Component MakeTreeView(std::shared_ptr<EditorState> state);

}
