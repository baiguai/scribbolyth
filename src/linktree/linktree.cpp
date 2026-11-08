#include "linktree.hpp"

#include <algorithm>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>

#include "../editor/editor_state.hpp"
#include "../links/links.hpp"
#include "../text/text.hpp"
#include "../treeview/tree_node.hpp"

namespace scribbolyth::linktree
{
    /*[./Introduction to Linktree.cpp]
        The link tree dialog shows the document as a graph of inter-note
        links: every note that links to other notes is a branch, and the
        notes it links to are its children (recursively). It is a read-only
        navigator: Enter reveals the selected note in the treeview and closes
        the dialog.

        Roots are the notes that have outgoing links but are not themselves
        linked from another note. Any notes left over (for example a cycle of
        notes that all link to each other) are added as additional roots so
        every linked note is reachable. A note already on the current path is
        shown once more as a "(cycle)" leaf and is not expanded further.



        #intro
    */

    namespace
    {
        // Pad Right - shared helper from ../text/text.hpp.
        using scribbolyth::text::PadRight;

        /*!
            Link Node Struct

            One entry of the link forest before it is flattened for display.
            `self_matches` says whether this node's own title matches the
            current filter; `matches` says whether this node or any descendant
            matches. Caching both lets filtering show a matching node's whole
            subtree without rescanning for every row.
        */
        //>>
        struct LinkNode
        {
            treeview::TreeNode* node = nullptr;
            std::vector<std::unique_ptr<LinkNode>> children;
            bool cycle = false;
            bool self_matches = true;
            bool matches = true;
        };
        //<<
        //!

        /*!
            Row Struct

            One visible line of the flattened tree.
        */
        //>>
        struct Row
        {
            treeview::TreeNode* node = nullptr;
            int depth = 0;
            bool has_children = false;
            bool expanded = false;
            bool cycle = false;
        };
        //<<
        //!
    }

    /*!
        Link Tree Dialog Class

        #class
    */
    class LinkTreeDialog : public ftxui::ComponentBase
    {
    public:
        /*!
            Link Tree Dialog Constructor

            !_ctor
        */
        LinkTreeDialog(std::shared_ptr<EditorState> state, bool* show)
            : state_(std::move(state)), show_(show) {}
        //!

        /*!
            Variables

            ----
        */
        //>>
        bool Focusable() const override { return true; }
        //<<
        //!

        /*!
            Ftxui Event Call

            !_method
        */
        bool OnEvent(ftxui::Event event) override
        {
            // FTXUI's own events (terminal resize, screen wakeups) are not
            // key presses: they must not drop a pending "g" prefix.
            if (IsSyntheticEvent(event)) return false;

            EnsureRows();

            if (event.is_mouse())
            {
                return true;
            }

            // While filtering, printable keys edit the filter and Escape
            // cancels it (restoring the unfiltered tree). Enter accepts the
            // filter but keeps the filtered view.
            if (filtering_)
            {
                if (event == ftxui::Event::Escape)
                {
                    filter_.clear();
                    filtering_ = false;
                    Invalidate();
                    return true;
                }
                if (event == ftxui::Event::Return)
                {
                    filtering_ = false;
                    return true;
                }
                if (event == ftxui::Event::Backspace)
                {
                    if (!filter_.empty())
                    {
                        filter_.pop_back();
                        Invalidate();
                    }
                    return true;
                }
                if (event == ftxui::Event::ArrowDown)
                {
                    MoveSelection(+1);
                    return true;
                }
                if (event == ftxui::Event::ArrowUp)
                {
                    MoveSelection(-1);
                    return true;
                }
                if (event.is_character())
                {
                    filter_ += event.character();
                    Invalidate();
                    return true;
                }
                return true;
            }

            if (event == ftxui::Event::Escape)
            {
                Close();
                return true;
            }
            if (event == ftxui::Event::Return)
            {
                Activate();
                return true;
            }
            if (event.is_character() && event.character() == "/")
            {
                filtering_ = true;
                return true;
            }

            if (event.is_character() && event.character() == "g")
            {
                if (pending_g_)
                {
                    pending_g_ = false;
                    MoveToStart();
                    return true;
                }
                pending_g_ = true;
                return true;
            }
            if (event.is_character() && event.character() == "G")
            {
                pending_g_ = false;
                MoveToEnd();
                return true;
            }

            pending_g_ = false;

            if (event == ftxui::Event::ArrowDown
                || (event.is_character() && event.character() == "j"))
            {
                MoveSelection(+1);
                return true;
            }
            if (event == ftxui::Event::ArrowUp
                || (event.is_character() && event.character() == "k"))
            {
                MoveSelection(-1);
                return true;
            }
            if (event == ftxui::Event::ArrowRight
                || (event.is_character() && event.character() == "l"))
            {
                SetExpanded(true);
                return true;
            }
            if (event == ftxui::Event::ArrowLeft
                || (event.is_character() && event.character() == "h"))
            {
                SetExpanded(false);
                return true;
            }

            return true;
        }
        //!

        /*!
            Ftxui Render Call

            !_method
        */
        ftxui::Element Render() override
        {
            if (*show_ && !was_shown_)
            {
                Reset();
                was_shown_ = true;
            }
            EnsureRows();

            const int total = static_cast<int>(rows_.size());
            const int sel = std::min(selection_, std::max(0, total - 1));
            selection_ = sel;

            const int max_top = std::max(0, total - kVisibleRows);
            int top = std::min(scroll_, max_top);
            if (sel < top) top = sel;
            if (sel >= top + kVisibleRows) top = sel - kVisibleRows + 1;
            top = std::max(0, std::min(top, max_top));
            const int count = std::min(kVisibleRows, std::max(0, total - top));

            const int row_width = content_width_ + 2;

            ftxui::Elements rows;
            if (total == 0)
            {
                const std::string msg = filter_.empty() ? "  No linked documents"
                                                        : "  No matches";
                rows.push_back(ftxui::text(PadRight(msg, row_width)) | ftxui::dim);
            }
            else
            {
                for (int i = 0; i < count; ++i)
                {
                    const Row& row = rows_[static_cast<std::size_t>(top + i)];
                    std::string line(static_cast<std::size_t>(row.depth) * 2, ' ');
                    if (row.has_children)
                    {
                        line += row.expanded ? "▾ " : "▸ ";
                    }
                    else
                    {
                        line += "  ";
                    }
                    line += row.node->name;
                    if (row.cycle)
                    {
                        line += "  (cycle)";
                    }

                    ftxui::Element element =
                        ftxui::text(" " + PadRight(line, content_width_) + " ");
                    if (row.cycle) element = element | ftxui::dim;
                    if (top + i == sel) element = element | ftxui::inverted;
                    rows.push_back(element);
                }
            }
            while (static_cast<int>(rows.size()) < kVisibleRows)
            {
                rows.push_back(ftxui::text(PadRight("", row_width)));
            }

            std::string filter_line;
            if (filtering_)
            {
                filter_line = " Filter: " + filter_ + "_";
            }
            else if (!filter_.empty())
            {
                filter_line = " Filter: " + filter_;
            }
            else
            {
                filter_line = " Press / to filter";
            }
            ftxui::Element filter_element =
                ftxui::text(PadRight(filter_line, row_width));
            if (!filtering_)
            {
                filter_element = filter_element | ftxui::dim;
            }

            const std::string counter =
                std::to_string(total == 0 ? 0 : sel + 1) + "/" + std::to_string(total);
            const std::string footer =
                "  " + counter + "    "
                + (filtering_
                       ? "Type to filter  Enter apply  Esc cancel"
                       : "j/k move  h/l fold  gg/G top/bottom  / filter  Enter open  Esc close")
                + "  ";

            return ftxui::window(ftxui::text(" V Link Tree "),
                                 ftxui::vbox({
                                     filter_element,
                                     ftxui::separator(),
                                     ftxui::vbox(std::move(rows)),
                                     ftxui::separator(),
                                     ftxui::text(PadRight(footer, row_width)) | ftxui::dim,
                                 })) |
                   ftxui::size(ftxui::WIDTH, ftxui::LESS_THAN, 140) |
                   ftxui::size(ftxui::HEIGHT, ftxui::LESS_THAN, 24);
        }
        //!

    private:
        /*!
            Private Members
        */
        //>>
        static constexpr int kVisibleRows = 18;
        static constexpr int kMaxDepth = 64;
        static constexpr int kMaxNodes = 2000;

        std::shared_ptr<EditorState> state_;
        bool* show_;
        bool was_shown_ = false;
        bool dirty_ = true;
        bool filtering_ = false;
        bool pending_g_ = false;
        std::string filter_;
        int selection_ = 0;
        int scroll_ = 0;
        int content_width_ = 20;
        std::set<treeview::TreeNode*> collapsed_;
        std::vector<Row> rows_;
        //<<
        //!

        /*!
            Name Matches

            !_method

            Case-insensitive substring match of a note title against the
            current filter. An empty filter matches everything.
        */
        bool NameMatches(const treeview::TreeNode* node) const
        {
            if (filter_.empty()) return true;
            return links::Lower(node->name).find(links::Lower(filter_))
                   != std::string::npos;
        }
        //!

        /*!
            Build Subtree

            !_method

            Build one branch of the link forest. `path` holds the notes on the
            current branch so a link back to an ancestor becomes a "(cycle)"
            leaf instead of recursing forever. `reached` records every note
            placed somewhere in the forest so later roots can skip it.
        */
        std::unique_ptr<LinkNode> BuildSubtree(
            treeview::TreeNode* node,
            std::map<treeview::TreeNode*, std::vector<treeview::TreeNode*>>& children_of,
            std::set<treeview::TreeNode*>& path,
            std::set<treeview::TreeNode*>& reached,
            int& budget,
            int depth)
        {
            auto out = std::make_unique<LinkNode>();
            out->node = node;
            out->self_matches = NameMatches(node);
            out->matches = out->self_matches;
            reached.insert(node);

            if (budget <= 0 || depth >= kMaxDepth) return out;
            --budget;

            path.insert(node);
            const auto it = children_of.find(node);
            if (it != children_of.end())
            {
                for (treeview::TreeNode* child : it->second)
                {
                    if (path.count(child) != 0)
                    {
                        auto leaf = std::make_unique<LinkNode>();
                        leaf->node = child;
                        leaf->cycle = true;
                        leaf->self_matches = NameMatches(child);
                        leaf->matches = leaf->self_matches;
                        out->matches = out->matches || leaf->matches;
                        out->children.push_back(std::move(leaf));
                    }
                    else
                    {
                        auto sub = BuildSubtree(child, children_of, path, reached,
                                                budget, depth + 1);
                        out->matches = out->matches || sub->matches;
                        out->children.push_back(std::move(sub));
                    }
                }
            }
            path.erase(node);
            return out;
        }
        //!

        /*!
            Flatten

            !_method

            Append the visible rows of `node` to rows_. A filtered tree hides
            branches with no matching note. Once a note itself matches, its
            whole subtree is shown (`include_all`), so filtering by a parent
            still lists all of its children. Every remaining branch is forced
            open; otherwise a branch is open unless the user folded it with h.
        */
        void Flatten(const LinkNode* node, int depth, bool include_all)
        {
            if (node == nullptr) return;

            const bool filtering = !filter_.empty();
            if (filtering && !include_all && !node->matches) return;

            // A matching note opens up everything beneath it; ancestors of a
            // match only keep the branches that lead to a match.
            const bool show_subtree =
                include_all || (filtering && node->self_matches);

            const bool has_children = !node->children.empty();
            const bool expanded = has_children
                && (filtering || collapsed_.count(node->node) == 0);

            rows_.push_back(Row{node->node, depth, has_children, expanded,
                                node->cycle});
            if (!expanded) return;
            for (const auto& child : node->children)
            {
                Flatten(child.get(), depth + 1, show_subtree);
            }
        }
        //!

        /*!
            Rebuild

            !_method

            Recompute the link forest from the document and flatten it into
            the visible rows.
        */
        void Rebuild()
        {
            rows_.clear();
            dirty_ = false;
            content_width_ = 20;

            std::vector<std::pair<treeview::TreeNode*, int>> all;
            if (state_->collect_all_nodes)
            {
                all = state_->collect_all_nodes();
            }
            if (all.empty()) return;

            std::map<std::string, treeview::TreeNode*> by_name;
            for (const auto& item : all)
            {
                const std::string key =
                    links::Lower(links::Trim(item.first->name));
                if (!key.empty()) by_name.emplace(key, item.first);
            }

            std::map<treeview::TreeNode*, std::vector<treeview::TreeNode*>> children_of;
            std::set<treeview::TreeNode*> linking;
            std::set<treeview::TreeNode*> incoming;
            for (const auto& item : all)
            {
                treeview::TreeNode* node = item.first;
                std::vector<links::Link> found;
                links::CollectLinks(node->text, found);

                std::set<treeview::TreeNode*> seen;
                for (const auto& link : found)
                {
                    if (!links::IsNoteTarget(link.target)) continue;
                    const std::string key = links::Lower(
                        links::Trim(links::NoteTitle(link.target)));
                    const auto it = by_name.find(key);
                    if (it == by_name.end()) continue;

                    treeview::TreeNode* target = it->second;
                    if (!seen.insert(target).second) continue;
                    children_of[node].push_back(target);
                    linking.insert(node);
                    incoming.insert(target);
                }
            }

            if (linking.empty()) return;

            int budget = kMaxNodes;
            std::set<treeview::TreeNode*> reached;

            const auto add_root = [&](treeview::TreeNode* node)
            {
                std::set<treeview::TreeNode*> path;
                auto tree = BuildSubtree(node, children_of, path, reached,
                                         budget, 0);
                Flatten(tree.get(), 0, false);
            };

            // Notes that link out but are not linked to are the entry points.
            for (const auto& item : all)
            {
                if (budget <= 0) break;
                if (linking.count(item.first) == 0) continue;
                if (incoming.count(item.first) != 0) continue;
                add_root(item.first);
            }
            // Anything left (cycles, or a graph where every linking note is
            // also linked to) gets its own root so nothing is unreachable.
            for (const auto& item : all)
            {
                if (budget <= 0) break;
                if (linking.count(item.first) == 0) continue;
                if (reached.count(item.first) != 0) continue;
                add_root(item.first);
            }

            for (const Row& row : rows_)
            {
                int length = row.depth * 2 + 2
                    + static_cast<int>(row.node->name.size());
                if (row.cycle) length += 9;
                content_width_ = std::max(content_width_, length);
            }
            selection_ = std::max(0, std::min(selection_,
                                              static_cast<int>(rows_.size()) - 1));
        }
        //!

        /*!
            Ensure Rows

            !_method

            Rebuild the rows after any change that affects the forest (filter,
            fold state, first open).
        */
        void EnsureRows()
        {
            if (dirty_) Rebuild();
        }
        //!

        /*!
            Invalidate

            !_method
        */
        void Invalidate()
        {
            dirty_ = true;
            selection_ = 0;
            scroll_ = 0;
        }
        //!

        /*!
            Reset

            !_method

            Start a fresh dialog each time it is opened.
        */
        void Reset()
        {
            filter_.clear();
            filtering_ = false;
            pending_g_ = false;
            selection_ = 0;
            scroll_ = 0;
            collapsed_.clear();
            dirty_ = true;
        }
        //!

        /*!
            Close

            !_method
        */
        void Close()
        {
            *show_ = false;
            was_shown_ = false;
            Reset();
        }
        //!

        /*!
            Activate

            !_method

            Reveal the selected note in the treeview and close the dialog.
        */
        void Activate()
        {
            if (rows_.empty()) return;
            const int sel = std::min(selection_,
                                     static_cast<int>(rows_.size()) - 1);
            treeview::TreeNode* node = rows_[static_cast<std::size_t>(sel)].node;
            if (node != nullptr && state_->reveal_node)
            {
                state_->reveal_node(node);
            }
            state_->status = "";
            Close();
        }
        //!

        /*!
            Move Selection Up/Down

            !_method
        */
        void MoveSelection(int dir)
        {
            if (rows_.empty()) return;
            const int total = static_cast<int>(rows_.size());
            selection_ = std::max(0, std::min(total - 1, selection_ + dir));
        }
        //!

        /*!
            Move To Start

            !_method
        */
        void MoveToStart()
        {
            selection_ = 0;
        }
        //!

        /*!
            Move To End

            !_method
        */
        void MoveToEnd()
        {
            if (!rows_.empty())
            {
                selection_ = static_cast<int>(rows_.size()) - 1;
            }
        }
        //!

        /*!
            Set Expanded

            !_method

            Fold or unfold the selected branch. Only branches (rows with
            children) are affected.
        */
        void SetExpanded(bool expanded)
        {
            if (rows_.empty()) return;
            const int sel = std::min(selection_,
                                     static_cast<int>(rows_.size()) - 1);
            const Row& row = rows_[static_cast<std::size_t>(sel)];
            if (!row.has_children) return;

            if (expanded)
            {
                collapsed_.erase(row.node);
            }
            else
            {
                collapsed_.insert(row.node);
            }
            // Keep the selection on the same node while folding/unfolding.
            dirty_ = true;
        }
        //!

        //!
    };
    //!

    /*>
        Make Link Tree Dialog

        !_method
    */
    ftxui::Component MakeLinkTreeDialog(std::shared_ptr<EditorState> state, bool* show)
    {
        return ftxui::Make<LinkTreeDialog>(std::move(state), show);
    }
    //!
}
