#include "treeview.hpp"

#include <algorithm>
#include <cstddef>
#include <utility>

#include "../io/serialize.hpp"
#include "../html/convert.hpp"

namespace scribbolyth::treeview
{

    int CountNodes(const std::vector<TreeNode>& nodes);
    TreeNode* FindParent(std::vector<TreeNode>& roots, TreeNode* child);
    void CollectAllDepth(TreeNode& node, int depth, std::vector<std::pair<TreeNode*, int>>& out);

    class TreeView : public ftxui::ComponentBase
    {
        public:
            TreeView(std::shared_ptr<EditorState> state)
                : state_(std::move(state))
            {
                state_->operations["deselect_node"] = [this](const std::string&, int)
                {
                    if (state_->mode == Mode::TREE) selected_ = nullptr;
                    RefreshActiveNode();
                };
                state_->operations["move_up"] = [this](const std::string&, int count)
                {
                    if (state_->mode == Mode::TREE) MoveSelection(-count);
                    RefreshActiveNode();
                };
                state_->operations["move_down"] = [this](const std::string&, int count)
                {
                    if (state_->mode == Mode::TREE) MoveSelection(count);
                    RefreshActiveNode();
                };
                state_->operations["move_file_start"] = [this](const std::string&, int)
                {
                    if (state_->mode == Mode::TREE) MoveToStart();
                    RefreshActiveNode();
                };
                state_->operations["move_file_end"] = [this](const std::string&, int)
                {
                    if (state_->mode == Mode::TREE) MoveToEnd();
                    RefreshActiveNode();
                };
                state_->operations["tree_open"] = [this](const std::string&, int count)
                {
                    if (state_->mode == Mode::TREE) for (int i = 0; i < count; ++i) OpenSelected();
                    RefreshActiveNode();
                };
                state_->operations["tree_collapse"] = [this](const std::string&, int count)
                {
                    if (state_->mode == Mode::TREE) for (int i = 0; i < count; ++i) CollapseSelected();
                    RefreshActiveNode();
                };
                state_->operations["tree_expand"] = [this](const std::string&, int count)
                {
                    if (state_->mode == Mode::TREE) for (int i = 0; i < count; ++i) ExpandSelected();
                    RefreshActiveNode();
                };
                state_->operations["expand_all"] = [this](const std::string&, int)
                {
                    if (state_->mode == Mode::TREE) ExpandAll();
                    RefreshActiveNode();
                };
                state_->operations["collapse_all"] = [this](const std::string&, int)
                {
                    if (state_->mode == Mode::TREE) CollapseAll();
                    RefreshActiveNode();
                };
                state_->operations["new_node"] = [this](const std::string& name, int)
                {
                    if (!name.empty()) InsertNode(name);
                    RefreshActiveNode();
                };
                state_->operations["new_child"] = [this](const std::string& name, int)
                {
                    if (!name.empty()) InsertChild(name);
                    RefreshActiveNode();
                };
                state_->operations["rename_node"] = [this](const std::string& name, int)
                {
                    if (selected_ && !name.empty()) selected_->name = name;
                    RefreshActiveNode();
                };
                state_->operations["delete_node"] = [this](const std::string&, int)
                {
                    DeleteNode();
                    RefreshActiveNode();
                };
                state_->operations["move_node_up"] = [this](const std::string&, int count)
                {
                    if (state_->mode == Mode::TREE) for (int i = 0; i < count; ++i) MoveNode(-1);
                    RefreshActiveNode();
                };
                state_->operations["move_node_down"] = [this](const std::string&, int count)
                {
                    if (state_->mode == Mode::TREE) for (int i = 0; i < count; ++i) MoveNode(+1);
                    RefreshActiveNode();
                };
                state_->operations["move_parent_up"] = [this](const std::string&, int count)
                {
                    if (state_->mode == Mode::TREE) for (int i = 0; i < count; ++i) MoveParent(-1);
                    RefreshActiveNode();
                };
                state_->operations["move_parent_down"] = [this](const std::string&, int count)
                {
                    if (state_->mode == Mode::TREE) for (int i = 0; i < count; ++i) MoveParent(+1);
                    RefreshActiveNode();
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
                state_->operations["save"] = [this](const std::string&, int)
                {
                    if (current_file_.empty())
                    {
                        state_->status = "No file path set - use :saveas to save";
                        return;
                    }
                    SaveTo(current_file_);
                };
                state_->operations["saveas"] = [this](const std::string& path, int)
                {
                    if (path.empty())
                    {
                        state_->status = "Save as requires a path";
                        return;
                    }
                    SaveTo(path);
                };
                state_->operations["open"] = [this](const std::string& path, int)
                {
                    if (path.empty())
                    {
                        state_->status = "Open requires a path";
                        return;
                    }
                    LoadFrom(path);
                };
                state_->operations["new_document"] = [this](const std::string&, int)
                {
                    roots_.clear();
                    current_file_.clear();
                    selected_ = nullptr;
                    state_->treeview_width = kDefaultTreeviewWidth;
                    RefreshActiveNode();
                    state_->status = "New document - no file path";
                };
                state_->operations["import_html"] = [this](const std::string& path, int)
                {
                    if (path.empty())
                    {
                        state_->status = "Import requires a path";
                        return;
                    }
                    std::vector<TreeNode> loaded;
                    if (!scribbolyth::html::ImportHtmlFile(path, loaded))
                    {
                        state_->status = "Error: could not import " + path;
                        return;
                    }
                    roots_ = std::move(loaded);
                    current_file_.clear();
                    selected_ = nullptr;
                    RefreshActiveNode();
                    state_->status = "Imported " + std::to_string(CountNodes(roots_)) + " nodes from " + path;
                };
                state_->operations["export_html"] = [this](const std::string& path, int)
                {
                    if (path.empty())
                    {
                        state_->status = "Export requires a path";
                        return;
                    }
                    if (state_->template_path.empty())
                    {
                        state_->status = "Error: scribboleth.html template not found";
                        return;
                    }
                    if (!scribbolyth::html::ExportHtmlFile(state_->template_path, path, roots_))
                    {
                        state_->status = "Error: could not export " + path;
                        return;
                    }
                    state_->status = "Exported " + std::to_string(CountNodes(roots_)) + " nodes to " + path;
                };
                state_->operations["treeview_width_increase"] = [this](const std::string&, int count)
                {
                    state_->treeview_width = std::min(kMaxTreeviewWidth,
                        state_->treeview_width + std::max(1, count));
                };
                state_->operations["treeview_width_decrease"] = [this](const std::string&, int count)
                {
                    state_->treeview_width = std::max(kMinTreeviewWidth,
                        state_->treeview_width - std::max(1, count));
                };

                state_->collect_all_nodes = [this]
                {
                    std::vector<std::pair<TreeNode*, int>> out;
                    for (auto& root : roots_)
                    {
                        CollectAllDepth(root, 0, out);
                    }
                    return out;
                };
                state_->reveal_node = [this](TreeNode* target)
                {
                    if (!target) return;
                    for (TreeNode* cur = target; cur; cur = FindParent(roots_, cur))
                    {
                        if (cur != target) cur->expanded = true;
                    }
                    selected_ = target;
                    RefreshActiveNode();
                };

                RefreshActiveNode();
            }
            bool Focusable() const override
            {
                return true;
            }
            bool OnEvent(ftxui::Event event) override;
            ftxui::Element Render() override;

        private:
            std::vector<TreeNode*> VisibleNodes();
            std::vector<TreeNode>& ContainerOf(TreeNode* node);
            void MoveSelection(int dir);
            void MoveToStart();
            void MoveToEnd();
            void ExpandSelected();
            void CollapseSelected();
            void OpenSelected();
            void ExpandAll();
            void CollapseAll();
            void InsertChild(const std::string& name);
            void InsertNode(const std::string& name);
            void DeleteNode();
            void MoveNode(int dir);
            void MoveParent(int dir);
            void RefreshActiveNode();
            void SaveTo(const std::string& path);
            void LoadFrom(const std::string& path);
            bool IsAncestor(TreeNode& ancestor, TreeNode* node);
            void CollectVisibleDepth(TreeNode& node, int depth, std::vector<TreeNode*>& nodes, std::vector<int>& depths);

            std::shared_ptr<EditorState> state_;
            std::vector<TreeNode> roots_;
            TreeNode* selected_ = nullptr;
            std::string current_file_;
    };

    ftxui::Component MakeTreeView(std::shared_ptr<EditorState> state)
    {
        return ftxui::Make<TreeView>(std::move(state));
    }

    void CollectVisible(TreeNode& node, std::vector<TreeNode*>& out)
    {
        out.push_back(&node);
        if (!node.children.empty() && node.expanded)
        {
            for (auto& child : node.children)
            {
                CollectVisible(child, out);
            }
        }
    }

    void CollectAllDepth(TreeNode& node, int depth, std::vector<std::pair<TreeNode*, int>>& out)
    {
        out.push_back({&node, depth});
        for (auto& child : node.children)
        {
            CollectAllDepth(child, depth + 1, out);
        }
    }

    void SetAllExpanded(TreeNode& node, bool expanded, bool skip_root)
    {
        if (!node.children.empty() && !skip_root)
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

    TreeNode* FindParent(std::vector<TreeNode>& roots, TreeNode* child)
    {
        for (auto& root : roots)
        {
            if (TreeNode* parent = FindParent(root, child))
            {
                return parent;
            }
        }
        return nullptr;
    }

    int CountNodes(const std::vector<TreeNode>& nodes)
    {
        int total = 0;
        for (const auto& node : nodes)
        {
            ++total;
            total += CountNodes(node.children);
        }
        return total;
    }

    std::vector<TreeNode>& TreeView::ContainerOf(TreeNode* node)
    {
        TreeNode* parent = FindParent(roots_, node);
        return parent ? parent->children : roots_;
    }

    std::vector<TreeNode*> TreeView::VisibleNodes()
    {
        std::vector<TreeNode*> out;
        for (auto& root : roots_)
        {
            CollectVisible(root, out);
        }
        return out;
    }

    bool TreeView::IsAncestor(TreeNode& ancestor, TreeNode* node)
    {
        for (TreeNode* cur = node; cur; cur = FindParent(roots_, cur))
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
        if (!node.children.empty() && node.expanded)
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
        if (selected_ == nullptr) return;
        auto& children = ContainerOf(selected_);
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
        if (selected_ == nullptr) return;
        auto& children = ContainerOf(selected_);
        std::size_t si = 0;
        while (si < children.size() && &children[si] != selected_) ++si;
        if (si >= children.size()) return;

        if (dir < 0)
        {
            TreeNode* parent = FindParent(roots_, selected_);
            if (!parent) return;
            auto& container = ContainerOf(parent);
            std::size_t pi = 0;
            while (pi < container.size() && &container[pi] != parent) ++pi;
            if (pi >= container.size()) return;

            TreeNode moved = std::move(children[si]);
            children.erase(children.begin() + static_cast<std::ptrdiff_t>(si));

            auto it = container.insert(container.begin() + static_cast<std::ptrdiff_t>(pi) + 1, std::move(moved));
            selected_ = &*it;
        }
        else
        {
            std::vector<TreeNode*> visible;
            std::vector<int> depth;
            for (auto& root : roots_)
            {
                CollectVisibleDepth(root, 0, visible, depth);
            }

            std::size_t v = 0;
            while (v < visible.size() && visible[v] != selected_) ++v;
            if (v >= visible.size()) return;

            TreeNode* target = nullptr;
            int best = -1;
            for (std::size_t i = v; i-- > 0;)
            {
                TreeNode* n = visible[i];
                // if (n->children.empty()) continue;
                if (IsAncestor(*n, selected_)) continue;
                if (depth[i] > best)
                {
                    best = depth[i];
                    target = n;
                }
            }
            if (!target) return;

            TreeNode moved = std::move(children[si]);
            children.erase(children.begin() + static_cast<std::ptrdiff_t>(si));

            target->expanded = true;
            target->children.insert(target->children.begin(), std::move(moved));
            selected_ = &target->children.front();
        }
    }

    void TreeView::RefreshActiveNode()
    {
        state_->active_node = selected_;
    }

    void TreeView::SaveTo(const std::string& path)
    {
        std::string json = scribbolyth::io::Serialize(roots_, state_->treeview_width);
        if (!scribbolyth::io::WriteFile(path, json))
        {
            state_->status = "Error: could not write " + path;
            return;
        }
        current_file_ = path;
        state_->status = "Saved " + std::to_string(CountNodes(roots_)) + " nodes to " + path;
    }

    void TreeView::LoadFrom(const std::string& path)
    {
        std::string content;
        if (!scribbolyth::io::ReadFile(path, content))
        {
            state_->status = "Error: could not open " + path;
            return;
        }
        std::vector<TreeNode> loaded;
        int loaded_width = state_->treeview_width;
        if (!scribbolyth::io::Deserialize(content, loaded, &loaded_width))
        {
            state_->status = "Error: could not parse " + path;
            return;
        }
        state_->treeview_width = loaded_width;
        roots_ = std::move(loaded);
        current_file_ = path;
        selected_ = nullptr;
        RefreshActiveNode();
        state_->status = "Loaded " + std::to_string(CountNodes(roots_)) + " nodes from " + path;
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
        if (selected_ == nullptr || selected_->children.empty()) return;
        if (!selected_->expanded) { selected_->expanded = true; return; }
        selected_ = &selected_->children.front();
    }

    void TreeView::CollapseSelected()
    {
        if (selected_ == nullptr) return;
        if (!selected_->children.empty() && selected_->expanded)
        {
            selected_->expanded = false;
            return;
        }
        if (TreeNode* parent = FindParent(roots_, selected_))
        {
            selected_ = parent;
        }
    }

    void TreeView::OpenSelected()
    {
        if (selected_ == nullptr || selected_->children.empty()) return;
        selected_->expanded = !selected_->expanded;
    }

    void TreeView::ExpandAll()
    {
        for (auto& root : roots_)
        {
            SetAllExpanded(root, true, false);
        }
    }

    void TreeView::CollapseAll()
    {
        for (auto& root : roots_)
        {
            SetAllExpanded(root, false, false);
        }
        auto visible = VisibleNodes();
        if (std::find(visible.begin(), visible.end(), selected_) == visible.end())
        {
            selected_ = nullptr;
        }
    }

    TreeNode new_node(std::string name)
    {
        TreeNode node;
        node.name = std::move(name);
        return node;
    }

    void TreeView::InsertChild(const std::string& name)
    {
        if (selected_ == nullptr)
        {
            roots_.push_back(new_node(name));
            selected_ = &roots_.back();
            return;
        }
        selected_->expanded = true;
        auto inserted = selected_->children.insert(selected_->children.begin(), new_node(name));
        selected_ = &*inserted;
    }

    void TreeView::InsertNode(const std::string& name)
    {
        if (selected_ == nullptr)
        {
            roots_.push_back(new_node(name));
            selected_ = &roots_.back();
            return;
        }
        auto& children = ContainerOf(selected_);
        for (auto it = children.begin(); it != children.end(); ++it)
        {
            if (&*it == selected_)
            {
                auto inserted = children.insert(it + 1, new_node(name));
                selected_ = &*inserted;
                return;
            }
        }
    }

    void TreeView::DeleteNode()
    {
        if (selected_ == nullptr) return;
        TreeNode* parent = FindParent(roots_, selected_);
        auto& children = ContainerOf(selected_);
        for (std::size_t i = 0; i < children.size(); ++i)
        {
            if (&children[i] != selected_) continue;

            children.erase(children.begin() + static_cast<std::ptrdiff_t>(i));

            if (!children.empty())
            {
                std::size_t next = std::min(i, children.size() -1);
                selected_ = &children[next];
            }
            else
            {
                selected_ = parent;
            }
            return;
        }
    }

    bool TreeView::OnEvent(ftxui::Event event)
    {
        return scribbolyth::op::HandleKey(state_, event);
    }

    void RenderNode(const TreeNode& node, int depth, const TreeNode* selected, ftxui::Elements& rows)
    {
        std::string indent(depth * 2, ' ');
        std::string marker = !node.children.empty() ? (node.expanded ? "▾" : "▸") : " ";
        auto row = ftxui::text(indent + marker + " " + node.name);
        if (&node == selected)
        {
            row |= ftxui::inverted;
            row |= ftxui::focus;
        }
        rows.push_back(row);

        if (node.children.empty() || !node.expanded)
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
        for (const auto& root : roots_)
        {
            RenderNode(root, 0, selected_, rows);
        }
        return ftxui::vbox(std::move(rows)) | ftxui::frame | ftxui::flex;
    }

}
