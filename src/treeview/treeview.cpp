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
                state_->operations["move_up"] = [this](const std::string&, int count)
                {
                    if (state_->mode == Mode::TREE) MoveSelection(-count);
                };
                state_->operations["move_down"] = [this](const std::string&, int count)
                {
                    if (state_->mode == Mode::TREE) MoveSelection(count);
                };
                state_->operations["move_file_start"] = [this](const std::string&, int)
                {
                    if (state_->mode == Mode::TREE) MoveToStart();
                };
                state_->operations["move_file_end"] = [this](const std::string&, int)
                {
                    if (state_->mode == Mode::TREE) MoveToEnd();
                };
                state_->operations["tree_open"] = [this](const std::string&, int count)
                {
                    if (state_->mode == Mode::TREE) for (int i = 0; i < count; ++i) OpenSelected();
                };
                state_->operations["tree_collapse"] = [this](const std::string&, int count)
                {
                    if (state_->mode == Mode::TREE) for (int i = 0; i < count; ++i) CollapseSelected();
                };
                state_->operations["tree_expand"] = [this](const std::string&, int count)
                {
                    if (state_->mode == Mode::TREE) for (int i = 0; i < count; ++i) ExpandSelected();
                };
                state_->operations["expand_all"] = [this](const std::string&, int)
                {
                    if (state_->mode == Mode::TREE) ExpandAll();
                };
                state_->operations["collapse_all"] = [this](const std::string&, int)
                {
                    if (state_->mode == Mode::TREE) CollapseAll();
                };
                state_->operations["new_folder"] = [this](const std::string& name, int)
                {
                    if (!name.empty()) InsertFolder(name);
                };
                state_->operations["new_note"] = [this](const std::string& name, int)
                {
                    if (!name.empty()) InsertNote(name);
                };
                state_->operations["rename_node"] = [this](const std::string& name, int)
                {
                    if (!name.empty()) selected_->name = name;
                };

                state_->operations["move_node_up"] = [this](const std::string&, int count)
                {
                    if (state_->mode == Mode::TREE) for (int i = 0; i < count; ++i) MoveNode(-1);
                };
                state_->operations["move_node_down"] = [this](const std::string&, int count)
                {
                    if (state_->mode == Mode::TREE) for (int i = 0; i < count; ++i) MoveNode(+1);
                };
                state_->operations["move_parent_up"] = [this](const std::string&, int count)
                {
                    if (state_->mode == Mode::TREE) for (int i = 0; i < count; ++i) MoveParent(-1);
                };
                state_->operations["move_parent_down"] = [this](const std::string&, int count)
                {
                    if (state_->mode == Mode::TREE) for (int i = 0; i < count; ++i) MoveParent(+1);
                };

                state_->operations["enter_normal"] = [this](const std::string&, int)
                {
                    state_->mode = Mode::NORMAL;
                    if (state_->focus_editor) state_->focus_editor();
                };
                state_->operations["enter_insert"] = [this](const std::string&, int)
                {
                    state_->mode = Mode::INSERT;
                    if (state_->focus_editor) state_->focus_editor();
                };
                state_->operations["enter_tree"] = [this](const std::string&, int)
                {
                    state_->mode = Mode::TREE;
                    if (state_->focus_treeview) state_->focus_treeview();
                };
                state_->operations["enter_visual"] = [this](const std::string&, int)
                {
                    state_->mode = Mode::VISUAL;
                };
                state_->operations["enter_visual_line"] = [this](const std::string&, int)
                {
                    state_->mode = Mode::VISUAL_LINE;
                };
                state_->operations["enter_command"] = [this](const std::string&, int)
                {
                    scribbolyth::op::OpenCommandLine(state_, "");
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
            void InsertFolder(const std::string& name);
            void InsertNote(const std::string& name);
            void MoveNode(int dir);
            void MoveParent(int dir);
            bool IsAncestor(TreeNode& ancestor, TreeNode* node);
            void CollectVisibleDepth(TreeNode& node, int depth, std::vector<TreeNode*>& nodes, std::vector<int>& depths);

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

    bool TreeView::IsAncestor(TreeNode& ancestor, TreeNode* node)
    {
        for (TreeNode* cur = node; cur; cur = (cur == &root_) ? nullptr : FindParent(root_, cur))
        {
            if (cur == &ancestor)
            {
                return true;
            }
        }
        return false;
    }

    void TreeView::CollectVisibleDepth(TreeNode& node, int depth,
                                       std::vector<TreeNode*>& nodes,
                                       std::vector<int>& depths)
    {
        nodes.push_back(&node);
        depths.push_back(depth);
        if (node.is_folder && node.expanded)
        {
            for (auto& child : node.children)
            {
                CollectVisibleDepth(child, depth + 1, nodes, depths);
            }
        }
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

    void TreeView::MoveNode(int dir)
    {
        if (selected_ == &root_) return;
        TreeNode* parent = FindParent(root_, selected_);
        if (!parent) return;

        auto& children = parent->children;
        for (std::size_t i = 0; i < children.size(); ++i)
        {
            if (&children[i] != selected_) continue;
            std::size_t j = static_cast<std::size_t>(static_cast<int>(i) + dir);
            if (j >= children.size()) return;
            std::swap(children[i], children[j]);
            selected_ = &children[j];
            return;
        }
    }

    void TreeView::MoveParent(int dir)
    {
        if (selected_ == &root_) return;
        TreeNode* parent = FindParent(root_, selected_);
        if (!parent) return;

        std::size_t si = 0;
        auto& children = parent->children;
        while (si < children.size() && &children[si] != selected_) ++si;
        if (si >= children.size()) return;

        if (dir < 0)
        {
            if (parent == &root_) return;
            TreeNode* grand = FindParent(root_, parent);
            if (!grand) return;

            TreeNode moved = std::move(children[si]);
            children.erase(children.begin() + static_cast<std::ptrdiff_t>(si));

            auto& gc = grand->children;
            std::size_t pi = 0;
            while (pi < gc.size() && &gc[pi] != parent) ++pi;
            if (pi >= gc.size()) return;

            auto it = gc.insert(gc.begin() + static_cast<std::ptrdiff_t>(pi) + 1, std::move(moved));
            selected_ = &*it;
        }
        else
        {
            std::vector<TreeNode*> visible;
            std::vector<int> depth;
            CollectVisibleDepth(root_, 0, visible, depth);

            std::size_t si = 0;
            while (si < visible.size() && visible[si] != selected_) ++si;
            if (si >= visible.size()) return;

            TreeNode* target = nullptr;
            int best = -1;
            for (std::size_t i = si; i-- > 0;)
            {
                TreeNode* n = visible[i];
                if (!n->is_folder) continue;
                if (IsAncestor(*n, selected_)) continue;
                if (depth[i] > best)
                {
                    best = depth[i];
                    target = n;
                }
            }
            if (!target) return;

            std::size_t pi = 0;
            while (pi < children.size() && &children[pi] != selected_) ++pi;
            if (pi >= children.size()) return;

            TreeNode moved = std::move(children[pi]);
            children.erase(children.begin() + static_cast<std::ptrdiff_t>(pi));

            target->expanded = true;
            target->children.insert(target->children.begin(), std::move(moved));
            selected_ = &target->children.front();
        }
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

    void TreeView::InsertFolder(const std::string& name)
    {
        auto make_folder = [](std::string name)
        {
            TreeNode node;
            node.name = std::move(name);
            node.is_folder = true;
            node.expanded = false;
            return node;
        };

        if (selected_->is_folder)
        {
            selected_->expanded = true;
            selected_->children.push_back(make_folder(name));
            selected_ = &selected_->children.back();
            return;
        }

        if (TreeNode* parent = FindParent(root_, selected_))
        {
            for (auto it = parent->children.begin(); it != parent->children.end(); ++it)
            {
                if (&*it == selected_)
                {
                    auto inserted = parent->children.insert(it + 1, make_folder(name));
                    selected_ = &*inserted;
                    return;
                }
            }
        }
    }

    void TreeView::InsertNote(const std::string& name)
    {
        auto make_note = [](std::string name)
        {
            TreeNode node;
            node.name = std::move(name);
            node.is_folder = false;
            node.expanded = false;
            return node;
        };

        if (selected_->is_folder)
        {
            selected_->expanded = true;
            selected_->children.push_back(make_note(name));
            selected_ = &selected_->children.back();
            return;
        }

        if (TreeNode* parent = FindParent(root_, selected_))
        {
            for (auto it = parent->children.begin(); it != parent->children.end(); ++it)
            {
                if (&*it == selected_)
                {
                    auto inserted = parent->children.insert(it + 1, make_note(name));
                    selected_ = &*inserted;
                    return;
                }
            }
        }
    }

    bool TreeView::OnEvent(ftxui::Event event)
    {
        return scribbolyth::op::HandleKey(state_, event);
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
