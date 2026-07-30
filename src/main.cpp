#include "main.hpp"

using namespace ftxui;

int main() {
    auto state = std::make_shared<EditorState>();

    auto editor_comp = scribbolyth::editor::MakeEditor(state);
    auto treeview_comp = scribbolyth::treeview::MakeTreeView(state);

    auto treeview_wrap = treeview_comp;
    auto editor_wrap = editor_comp;

    int left_size = 30;
    auto main_split = ResizableSplitLeft(editor_wrap, treeview_wrap, &left_size);

    auto command_input = Input(&state->command_buffer, ":");

    auto command_wrapper = Renderer(command_input, [state, command_input] {
        if (state->mode != Mode::COMMAND)
            return emptyElement() | size(WIDTH, EQUAL, 0);
        return command_input->Render();
    });

    auto command_handler = CatchEvent(command_wrapper, [state](Event event) {
        if (event == Event::Escape) {
            state->mode = Mode::NORMAL;
            state->command_buffer.clear();
            return true;
        }
        if (event == Event::Return) {
            state->mode = Mode::NORMAL;
            state->command_buffer.clear();
            return true;
        }
        return false;
    });

    auto status_bar = Renderer([state]
    {
        return hbox({
            text(ModeName(state->mode)) | bold,
            separator(),
            text(" scribbolyth ") | dim,
        }) | bgcolor(Color::Blue);
    });

    int active_child = 0;
    state->active_child = &active_child;

    auto container = Container::Vertical({
        main_split | flex,
        command_handler,
        status_bar,
    }, &active_child);

    auto screen = ScreenInteractive::Fullscreen();
    screen.Loop(container);

    return 0;
}
