#pragma once

#include <memory>
#include "editor_state.hpp"
#include <ftxui/component/component.hpp>

namespace scribbolyth::editor {

ftxui::Component MakeEditor(std::shared_ptr<EditorState> state);

}
