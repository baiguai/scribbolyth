#include "treeview.hpp"

namespace scribbolyth::treeview
{

    class TreeView : public ftxui::ComponentBase
    {
        public:
            TreeView(std::shared_ptr<EditorState> state) : state_(std::move(state)) {}
            bool Focusable() const override
            {
                return true;
            }
            ftxui::Element Render() override;

        private:
            std::shared_ptr<EditorState> state_;
            TreeNode root_ = MakeRootFolder();
            TreeNode* selected_ = &root_;
    };

    ftxui::Component MakeTreeView(std::shared_ptr<EditorState> state)
    {
        return ftxui::Make<TreeView>(std::move(state));
    }

    TreeNode MakeRootFolder()
    {
        TreeNode root;
        root.name = "root";
        root.is_folder = true;
        root.expanded = true;

        TreeNode readme;
        readme.name = "Read Me";
        readme.is_folder = false;

        TreeNode docs;
        docs.name = "docs";
        docs.is_folder = true;
        docs.expanded = false;
        docs.children.push_back(std::move(readme));

        root.children.push_back(std::move(docs));

        return root;
    }

    void RenderNode(const TreeNode& node, int depth, ftxui::Elements& rows)
    {
        std::string indent(depth *2, ' ');
        std::string marker = node.is_folder ? (node.expanded ? "▾" : "▸") : " ";
        rows.push_back(ftxui::text(indent + marker + " " + node.name));

        if (!node.is_folder || !node.expanded)
        {
            return;
        }
        for (const auto& child : node.children)
        {
            RenderNode(child, depth + 1, rows);
        }
    }

    ftxui::Element TreeView::Render()
    {
        ftxui::Elements rows;
        RenderNode(root_, 0, rows);
        return ftxui::vbox(std::move(rows)) | ftxui::flex;
    }

}
