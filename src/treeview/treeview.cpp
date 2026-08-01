#include "treeview.hpp"

#include <algorithm>
#include <cstddef>

namespace scribbolyth::treeview
{

    class TreeView : public ftxui::ComponentBase
    {
        public:
            TreeView(std::shared_ptr<EditorState> state) : state_(std::move(state))
            {
                state_->rename_node = [this](const std::string& name)
                {
                    if (!name.empty()) selected_->name = name;
                };
            }
            bool Focusable() const override
            {
                return true;
            }
            bool OnEvent(ftxui::Event event) override;
            ftxui::Element Render() override;

        private:
            std::vector<TreeNode*> VisibleNodes();
            void MoveSelection(int dir);
            void MoveToStart();
            void MoveToEnd();
            void ExpandSelected();
            void CollapseSelected();
            void OpenSelected();
            void ExpandAll();
            void CollapseAll();
            void NewFolder();
            void RenameNode();

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
        auto file = [](std::string name) {
            TreeNode node;
            node.name = std::move(name);
            node.is_folder = false;
            return node;
        };
        auto folder = [](std::string name, bool expanded, std::vector<TreeNode> children) {
            TreeNode node;
            node.name = std::move(name);
            node.is_folder = true;
            node.expanded = expanded;
            node.children = std::move(children);
            return node;
        };

        TreeNode root;
        root.name = "root";
        root.is_folder = true;
        root.expanded = true;

        TreeNode treeview = folder("treeview", false, {
            file("treeview.cpp"),
            file("treeview.hpp"),
        });
        TreeNode editor = folder("editor", false, {
            file("editor.cpp"),
            file("editor.hpp"),
        });
        TreeNode api = folder("api", false, {
            file("design.md"),
        });

        root.children.push_back(file("Read Me"));
        root.children.push_back(folder("docs", true, {
            file("README.md"),
            std::move(api),
        }));
        root.children.push_back(folder("src", true, {
            std::move(treeview),
            std::move(editor),
        }));
        root.children.push_back(folder("notes", false, {
            file("idea.txt"),
        }));

        return root;
    }

    void CollectVisible(TreeNode& node, std::vector<TreeNode*>& out)
    {
        out.push_back(&node);
        if (node.is_folder && node.expanded)
        {
            for (auto& child : node.children)
            {
                CollectVisible(child, out);
            }
        }
    }

    void SetAllExpanded(TreeNode& node, bool expanded, bool skip_root)
    {
        if (node.is_folder && !skip_root)
        {
            node.expanded = expanded;
        }
        for (auto& child : node.children)
        {
            SetAllExpanded(child, expanded, false);
        }
    }

    TreeNode* FindParent(TreeNode& node, TreeNode* child)
    {
        for (auto& c : node.children)
        {
            if (&c == child)
            {
                return &node;
            }
            if (TreeNode* parent = FindParent(c, child))
            {
                return parent;
            }
        }
        return nullptr;
    }

    std::vector<TreeNode*> TreeView::VisibleNodes()
    {
        std::vector<TreeNode*> out;
        CollectVisible(root_, out);
        return out;
    }

    void TreeView::MoveSelection(int dir)
    {
        auto visible = VisibleNodes();
        if (visible.empty())
        {
            return;
        }
        for (std::size_t i = 0; i < visible.size(); ++i)
        {
            if (visible[i] == selected_)
            {
                int next = static_cast<int>(i) + dir;
                next = std::max(0, std::min(static_cast<int>(visible.size()) - 1, next));
                selected_ = visible[static_cast<std::size_t>(next)];
                return;
            }
        }
        selected_ = visible.front();
    }

    void TreeView::MoveToStart()
    {
        auto visible = VisibleNodes();
        if (!visible.empty())
        {
            selected_ = visible.front();
        }
    }

    void TreeView::MoveToEnd()
    {
        auto visible = VisibleNodes();
        if (!visible.empty())
        {
            selected_ = visible.back();
        }
    }

    void TreeView::ExpandSelected()
    {
        if (!selected_->is_folder)
        {
            return;
        }
        if (!selected_->expanded)
        {
            selected_->expanded = true;
            return;
        }
        if (!selected_->children.empty())
        {
            selected_ = &selected_->children.front();
        }
    }

    void TreeView::CollapseSelected()
    {
        if (selected_->is_folder && selected_->expanded)
        {
            selected_->expanded = false;
            return;
        }
        if (TreeNode* parent = FindParent(root_, selected_))
        {
            selected_ = parent;
        }
    }

    void TreeView::OpenSelected()
    {
        if (selected_->is_folder)
        {
            selected_->expanded = !selected_->expanded;
        }
    }

    void TreeView::ExpandAll()
    {
        SetAllExpanded(root_, true, false);
    }

    void TreeView::CollapseAll()
    {
        SetAllExpanded(root_, false, true);
        auto visible = VisibleNodes();
        if (std::find(visible.begin(), visible.end(), selected_) == visible.end())
        {
            selected_ = &root_;
        }
    }

    void TreeView::NewFolder()
    {
        auto make_folder = [](std::string name) {
            TreeNode node;
            node.name = std::move(name);
            node.is_folder = true;
            node.expanded = false;
            return node;
        };

        auto next_name = [](const std::vector<TreeNode>& siblings) {
            std::string base = "New Folder";
            std::size_t n = 1;
            while (true)
            {
                std::string candidate = (n == 1) ? base : base + " " + std::to_string(n);
                bool taken = false;
                for (const auto& s : siblings)
                {
                    if (s.name == candidate)
                    {
                        taken = true;
                        break;
                    }
                }
                if (!taken)
                {
                    return candidate;
                }
                ++n;
            }
        };

        if (root_.children.empty() || selected_ == &root_)
        {
            root_.children.push_back(make_folder(next_name(root_.children)));
            selected_ = &root_.children.back();
            return;
        }

        if (TreeNode* parent = FindParent(root_, selected_))
        {
            for (auto it = parent->children.begin(); it != parent->children.end(); ++it)
            {
                if (&*it == selected_)
                {
                    auto inserted = parent->children.insert(it + 1, make_folder(next_name(parent->children)));
                    selected_ = &*inserted;
                    return;
                }
            }
        }
    }

    void TreeView::RenameNode()
    {
        state_->mode = Mode::COMMAND;
        state_->command_buffer = ":rename ";
        state_->command_cursor = 8;
        if (state_->active_child) *state_->active_child = 1;
    }

    bool TreeView::OnEvent(ftxui::Event event)
    {
        if (state_->mode != Mode::TREE)
        {
            auto result = state_->ActiveKeymap().Handle(event);
            if (result.pending)
            {
                return true;
            }
            switch (result.action)
            {
                case Action::EnterTree:
                    state_->mode = Mode::TREE;
                    return true;
                case Action::EnterNormal:
                    state_->mode = Mode::NORMAL;
                    return true;
                case Action::EnterInsert:
                    state_->mode = Mode::INSERT;
                    if (state_->focus_editor)
                        state_->focus_editor();
                    return true;
                case Action::EnterCommand:
                    state_->mode = Mode::COMMAND;
                    state_->command_buffer = ":";
                    state_->command_cursor = 1;
                    if (state_->active_child)
                        *state_->active_child = 1;
                    return true;
                default:
                    return result.action != Action::None;
            }
        }

        auto result = state_->ActiveKeymap().Handle(event);
        if (result.pending)
        {
            return true;
        }
        switch (result.action)
        {
            case Action::MoveDown:
                MoveSelection(result.count);
                return true;
            case Action::MoveUp:
                MoveSelection(-result.count);
                return true;
            case Action::MoveFileStart:
                MoveToStart();
                return true;
            case Action::MoveFileEnd:
                MoveToEnd();
                return true;
            case Action::TreeExpand:
                for (int i = 0; i < result.count; ++i)
                    ExpandSelected();
                return true;
            case Action::TreeCollapse:
                for (int i = 0; i < result.count; ++i)
                    CollapseSelected();
                return true;
            case Action::TreeOpen:
                for (int i = 0; i < result.count; ++i)
                    OpenSelected();
                return true;
            case Action::TreeExpandAll:
                ExpandAll();
                return true;
            case Action::TreeCollapseAll:
                CollapseAll();
                return true;
            case Action::TreeNewFolder:
                NewFolder();
                return true;
            case Action::TreeRenameNode:
                RenameNode();
                return true;
            case Action::EnterNormal:
                state_->mode = Mode::NORMAL;
                if (state_->focus_editor)
                    state_->focus_editor();
                return true;
            case Action::EnterInsert:
                state_->mode = Mode::INSERT;
                if (state_->focus_editor)
                    state_->focus_editor();
                return true;
            case Action::EnterCommand:
                state_->mode = Mode::COMMAND;
                state_->command_buffer = ":";
                state_->command_cursor = 1;
                if (state_->active_child)
                    *state_->active_child = 1;
                return true;
            default:
                return false;
        }
    }

    void RenderNode(const TreeNode& node, int depth, const TreeNode* selected, ftxui::Elements& rows)
    {
        std::string indent(depth * 2, ' ');
        std::string marker = node.is_folder ? (node.expanded ? "▾" : "▸") : " ";
        auto row = ftxui::text(indent + marker + " " + node.name);
        if (&node == selected)
        {
            row |= ftxui::inverted;
        }
        rows.push_back(row);

        if (!node.is_folder || !node.expanded)
        {
            return;
        }
        for (const auto& child : node.children)
        {
            RenderNode(child, depth + 1, selected, rows);
        }
    }

    ftxui::Element TreeView::Render()
    {
        ftxui::Elements rows;
        RenderNode(root_, 0, selected_, rows);
        return ftxui::vbox(std::move(rows)) | ftxui::flex;
    }

}
