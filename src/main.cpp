#include "main.hpp"

#include <cstdlib>
#include <filesystem>
#include <iostream>

#include "config/config.hpp"
#include "bookmarks/bookmarks.hpp"
#include "browser/browser.hpp"
#include "help/help.hpp"
#include "history/history.hpp"
#include "links/links.hpp"
#include "op/op.hpp"
#include "search/search.hpp"

using namespace ftxui;

int main(int, char** argv) {
    auto state = std::make_shared<EditorState>();

    auto editor_comp = scribbolyth::editor::MakeEditor(state);
    auto treeview_comp = scribbolyth::treeview::MakeTreeView(state);

    state->focus_editor = [editor_comp] { editor_comp->TakeFocus(); };
    state->focus_treeview = [treeview_comp] { treeview_comp->TakeFocus(); };

    auto treeview_wrap = treeview_comp;
    auto editor_wrap = editor_comp;

    auto main_split = ResizableSplitLeft(treeview_wrap, editor_wrap, &state->treeview_width);

    auto screen = ScreenInteractive::Fullscreen();
    auto quit = screen.ExitLoopClosure();

    state->operations["quit"] = [quit](const std::string&, int) { quit(); };
    state->commands["qa"] = "quit";

    bool show_help = false;
    state->operations["show_help"] = [&show_help](const std::string&, int) { show_help = true; };

    bool show_search = false;
    state->operations["search_start"] = [&show_search](const std::string&, int) { show_search = true; };

    bool show_bookmarks = false;
    state->operations["bookmarks"] = [&show_bookmarks](const std::string&, int) { show_bookmarks = true; };
    state->commands["bookmarks"] = "bookmarks";

    bool show_links = false;
    state->operations["links"] = [&show_links](const std::string&, int) { show_links = true; };
    state->commands["links"] = "links";

    bool show_history = false;
    state->operations["history"] = [&show_history](const std::string&, int) { show_history = true; };

    bool show_file_browser = false;
    state->show_file_browser = &show_file_browser;

    namespace fs = std::filesystem;
    fs::path config_path = fs::path(argv[0]).parent_path() / "commands.conf";
    if (!fs::exists(config_path))
    {
        config_path = "commands.conf";
    }
    if (!scribbolyth::config::LoadConfig(config_path.string(), state))
    {
        std::cerr << "Warning: could not load config from " << config_path.string() << "\n";
        std::cerr << "Only the built-in ':qa' command is available.\n";
    }

    fs::path template_path = fs::path(argv[0]).parent_path() / "scribboleth.html";
    if (!fs::exists(template_path))
    {
        template_path = "scribboleth.html";
    }
    state->template_path = template_path.string();

    fs::path init_path;
    if (const char* env = std::getenv("SCRIBBOLYTH_INIT"); env != nullptr && *env != '\0')
    {
        init_path = env;
    }
    else
    {
        init_path = fs::path(argv[0]).parent_path() / "init.conf";
    }
    state->init_path = init_path.string();

    std::string last_file;
    if (scribbolyth::config::ReadInit(init_path.string(), last_file) && !last_file.empty())
    {
        auto it = state->operations.find("open");
        std::error_code ec;
        if (it != state->operations.end() && fs::exists(last_file, ec))
        {
            it->second(last_file, 1);
        }
    }

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

    auto command_handler = CatchEvent(command_wrapper, [state](Event event) {
        if (event == Event::Tab)
        {
            scribbolyth::op::CompleteCommand(state);
            return true;
        }
        if (event == Event::Escape || event == Event::Return) {
            if (event == Event::Return)
                scribbolyth::op::ExecuteCommand(state, state->command_buffer);
            state->command_buffer.clear();
            state->command_cursor = 0;
            if (state->active_child)
                *state->active_child = 0;
            state->mode = state->mode_before_command;
            if (state->mode == Mode::TREE)
            {
                if (state->focus_treeview) state->focus_treeview();
            }
            else
            {
                if (state->focus_editor) state->focus_editor();
            }
            return true;
        }
        return false;
    });

    auto status_bar = Renderer([state]
    {
        ftxui::Elements parts = {
            text(ModeName(state->mode)) | bold,
            // separator(),
            // text(" scribbolyth ") | dim,
        };
        if (!state->status.empty())
        {
            parts.push_back(separator());
            parts.push_back(text(" " + state->status + " ") | dim);
        }
        return hbox(std::move(parts)) | bgcolor(Color::Blue);
    });

    int active_child = 0;
    state->active_child = &active_child;

    auto container = Container::Vertical({
        main_split | flex,
        command_handler,
        status_bar,
    }, &active_child);

    auto help_comp = scribbolyth::help::MakeHelpDialog(state, config_path.string(), &show_help);
    auto search_comp = scribbolyth::search::MakeSearchDialog(state, &show_search);
    auto bookmarks_comp = scribbolyth::bookmarks::MakeBookmarksDialog(state, &show_bookmarks);
    auto links_comp = scribbolyth::links::MakeLinksDialog(state, &show_links);
    auto history_comp = scribbolyth::history::MakeHistoryDialog(state, &show_history);
    auto browser_comp = scribbolyth::browser::MakeFileBrowserDialog(state, &show_file_browser);
    auto root = Modal(Modal(Modal(Modal(Modal(Modal(container, help_comp, &show_help),
                                                  search_comp, &show_search),
                                          bookmarks_comp, &show_bookmarks),
                                  links_comp, &show_links),
                          history_comp, &show_history),
                  browser_comp, &show_file_browser);

    screen.Loop(root);

    return 0;
}
