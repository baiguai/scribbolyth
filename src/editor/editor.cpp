#include "editor.hpp"

namespace scribbolyth::editor
{

    class Editor : public ftxui::ComponentBase
    {
        public:
            Editor(std::shared_ptr<EditorState> state) : state_(std::move(state))
            {}
            ftxui::Element Render() override
            {
                return ftxui::text("Editor") | ftxui::center;
            }
            bool Focusable() const override
            {
                return true;
            }
            bool OnEvent(ftxui::Event event) override
            {
                auto result = state_->ActiveKeymap().Handle(event);
                if (result.pending)
                    return true;
                Action action = result.action;
                switch (action)
                {
                    case Action::EnterCommand:
                        state_->mode = Mode::COMMAND;
                        state_->command_buffer = ":";
                        state_->command_cursor = 1;
                        if (state_->active_child)
                            *state_->active_child = 1;
                        return true;
                    case Action::EnterInsert:
                        state_->mode = Mode::INSERT;
                        return true;
                    case Action::EnterNormal:
                        state_->mode = Mode::NORMAL;
                        return true;
                    case Action::EnterVisual:
                        state_->mode = Mode::VISUAL;
                        return true;
                    case Action::EnterVisualLine:
                        state_->mode = Mode::VISUAL_LINE;
                        return true;
                    case Action::EnterTree:
                        state_->mode = Mode::TREE;
                        if (state_->focus_treeview)
                            state_->focus_treeview();
                        return true;
                    default:
                        return action != Action::None;
                }
            }

        private:
            std::shared_ptr<EditorState> state_;
    };

    ftxui::Component MakeEditor(std::shared_ptr<EditorState> state)
    {
        return ftxui::Make<Editor>(std::move(state));
    }

}
