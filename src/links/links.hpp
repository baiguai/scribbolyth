#pragma once

#include <memory>
#include <string>
#include <vector>

#include <ftxui/component/component.hpp>

struct EditorState;

namespace scribbolyth::treeview
{
    struct TreeNode;
}

namespace scribbolyth::links
{
    /*!
        Link Struct
    */
    //>>
    struct Link
    {
        std::string label;
        std::string target;
        treeview::TreeNode* node = nullptr;
        bool from_markdown = false;
        bool ok = true;
    };
    //<<
    /*|

        A link found in a note's text: `label` is the display text (markdown
        label, or the target itself), `target` is the raw reference (a URL or a
        `_Title_`), `node` is the resolved target node once `Recompute` fills it
        in, and `ok` says whether the target resolves. `from_markdown` marks
        `[label](target)` style links.

    */
    //!

    /*!
        Variables

        ----
    */
    //>>
    std::string Trim(const std::string& s);
    std::string Lower(const std::string& s);
    bool IsUrlTarget(const std::string& target);
    bool IsNoteTarget(const std::string& target);
    std::string NoteTitle(const std::string& target);
    //<<
    //!

    /*!
        Collect Links

        !_method

        Collect every link in `content`, deduplicated by target, in order.
        Mirrors the HTML app's openLinksDialog() scanning rules.
    */
    void CollectLinks(const std::string& content, std::vector<Link>& out);
    //!

    /*!
        Find Link In Line

        !_method

        Find the link whose span covers column `col` (0-based) on a single line.
        Markdown links are matched before standalone URLs so a cursor sitting on
        the URL half of `[Label](https://...)` still reports the labeled link.
        Returns true and fills `out` when a link is found.
    */
    bool FindLinkInLine(const std::string& line, int col, Link& out);
    //!

    /*!
        Follow Link

        !_method

        Open a URL link externally, or reveal the node targeted by a `_Note_`
        link. Status feedback is written to `state->status`, mirroring the links
        dialog's Activate().
    */
    void FollowLink(Link link, const std::shared_ptr<EditorState>& state);
    //!

    ftxui::Component MakeLinksDialog(std::shared_ptr<EditorState> state, bool* show);
}
