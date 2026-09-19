#pragma once

#include <memory>
#include <vector>

#include <ftxui/component/component.hpp>

struct EditorState;

namespace scribbolyth::treeview
{
    struct TreeNode;
}

namespace scribbolyth::search
{
    // How the search dialog's Enter key behaves.
    enum class DialogMode
    {
        Jump,          // Enter jumps to the selected node.
        InsertLink,    // Enter inserts a _Title_ link at the editor cursor.
        CreateResults, // Enter creates a new "Search results: <query>" node
                       // holding one _Title_ link per matching node.
    };

    // Find matches for `raw_query` inside the currently active node only
    // (the same prefix rules as the search dialog apply: a leading "r:" makes
    // it a case-insensitive regex, a leading ":" restricts it to node titles,
    // otherwise it is a case-insensitive substring match). Returns either the
    // active node (when it matches) or nothing, so the Vim-style '/' find and
    // n/N navigation stay confined to the node being edited.
    std::vector<scribbolyth::treeview::TreeNode*> FindMatches(
        std::shared_ptr<EditorState> state, const std::string& raw_query);

    std::vector<std::pair<int, int>> FindLineMatches(const std::string& line,
                                                     const std::string& raw_query);

    // Document-wide search: every node whose title or content satisfies
    // `raw_query`, in document order, using the same prefix rules as
    // FindMatches ("r:" regex, "+:" all words, ":" titles only).
    std::vector<scribbolyth::treeview::TreeNode*> FindAllMatches(
        std::shared_ptr<EditorState> state, const std::string& raw_query);

    // Create a new node named "Search results: <query>" whose body holds one
    // `_Title_` link per matching node (entries with no node pointer are
    // skipped). The node is created like a normal new child: as the first
    // child of the tree selection when one is selected, otherwise as a root
    // node, and becomes the active node. Returns the created node, or nullptr
    // on failure (e.g. no matches to link). `status` receives a short
    // user-facing message.
    scribbolyth::treeview::TreeNode* CreateSearchResults(
        std::shared_ptr<EditorState> state,
        const std::vector<scribbolyth::treeview::TreeNode*>& nodes,
        const std::string& raw_query, std::string* status = nullptr);

    // Build the node search/filter dialog. While *show is true it consumes
    // every event, so no app key bindings fire. ArrowUp/ArrowDown move the
    // selection, Enter behaves per `mode` and closes (creating a results node
    // also collects it), Escape cancels. A leading "r:" makes the query a
    // case-insensitive regex, "+:" requires every word and ":" restricts the
    // match to node titles. The dialog keeps a fixed size regardless of
    // result count.
    ftxui::Component MakeSearchDialog(std::shared_ptr<EditorState> state, bool* show,
                                      DialogMode mode = DialogMode::Jump);
}
