#include "editor.hpp"

namespace scribbolyth::editor
{

    class Editor : public ftxui::ComponentBase
    {
        public:
            Editor(std::shared_ptr<EditorState> state) : state_(std::move(state))
            {
                auto stub = [](const std::string&, int) {};
                state_->operations["move_left"]          = stub;
                state_->operations["move_right"]         = stub;
                state_->operations["move_page_up"]       = stub;
                state_->operations["move_page_down"]     = stub;
                state_->operations["move_line_start"]    = stub;
                state_->operations["move_line_end"]      = stub;
                state_->operations["move_word_forward"]  = stub;
                state_->operations["move_word_back"]     = stub;
                state_->operations["delete_char"]        = stub;
                state_->operations["backspace_char"]     = stub;
                state_->operations["insert_newline"]     = stub;
                state_->operations["insert_tab"]         = stub;
                state_->operations["delete_line"]        = stub;
                state_->operations["undo"]               = stub;
                state_->operations["yank"]               = stub;
                state_->operations["paste"]              = stub;
            }
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
                return scribbolyth::op::HandleKey(state_, event);
            }

        private:
            std::shared_ptr<EditorState> state_;
    };

    ftxui::Component MakeEditor(std::shared_ptr<EditorState> state)
    {
        return ftxui::Make<Editor>(std::move(state));
    }

}
