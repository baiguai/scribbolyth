#include "treeview.hpp"

namespace scribbolyth::treeview
{

    class TreeView : public ftxui::ComponentBase
    {
        public:
            TreeView(std::shared_ptr<EditorState> state) : state_(std::move(state))
            {}
            ftxui::Element Render() override
            {
                return ftxui::text("TreeView") | ftxui::center;
            }

        private:
            std::shared_ptr<EditorState> state_;
    };

    ftxui::Component MakeTreeView(std::shared_ptr<EditorState> state)
    {
        return ftxui::Make<TreeView>(std::move(state));
    }

}
