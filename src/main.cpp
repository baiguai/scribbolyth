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

    auto screen = ScreenInteractive::Fullscreen();
    auto quit = screen.ExitLoopClosure();

    InputOption command_option;
    command_option.transform = [](InputState state)
    {
        state.element |= bgcolor(Color::Black) | color(Color::White);
        if (state.is_placeholder)
                state.element |= dim;
        return state.element;
    };
    command_option.cursor_position = &state->command_cursor;
    auto command_input = Input(&state->command_buffer, ":", command_option);

    auto command_wrapper = Renderer(command_input, [state, command_input] {
        if (state->mode != Mode::COMMAND)
            return emptyElement() | size(WIDTH, EQUAL, 0);
        return command_input->Render();
    });

    auto command_handler = CatchEvent(command_wrapper, [state, quit](Event event) {
        if (event == Event::Escape || event == Event::Return) {
            if (event == Event::Return && state->command_buffer == ":q")
                quit();
            state->mode = Mode::NORMAL;
            state->command_buffer.clear();
            state->command_cursor = 0;
            if (state->active_child)
                *state->active_child = 0;
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

    screen.Loop(container);

    return 0;
}
