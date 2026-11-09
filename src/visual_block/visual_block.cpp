#include "visual_block.hpp"

#include <algorithm>

namespace scribbolyth::visual_block
{
    /*[./Introduction to Visual_Block.cpp]
        The Visual Block code file handles the Editor's Visual Block logic.



        #intro
    */


    /*!
        Line Span 

        !_method

        Determine the horizontal slice of the block that applies to a single
        row. Returns false when the row lies outside the block's row range, or
        when the resulting slice would be empty. Otherwise it writes the start
        column to lo and the exclusive end column (col_hi + 1) to hi, clamping
        both to the line length so short lines never read past their end.
    */
    bool LineSpan(const std::vector<std::string>& lines, const Block& b, int row,
                  int& lo, int& hi)
    {
        if (row < b.row_lo || row > b.row_hi) return false;
        const int size = static_cast<int>(lines[static_cast<std::size_t>(row)].size());
        lo = std::min(b.col_lo, size);
        hi = std::min(b.col_hi + 1, size);
        return lo < hi;
    }
    //!

    /*!
        Extract 

        !_method

        Return the text contained inside the block. It walks every row of the
        block, uses LineSpan to get that row's horizontal slice, and appends it
        to the result. Rows whose slice is empty are skipped, and the pieces are
        joined with newlines, so the result is the block's contents as a
        multi-line string.
    */
    std::string Extract(const std::vector<std::string>& lines, const Block& b)
    {
        std::string out;
        for (int r = b.row_lo; r <= b.row_hi; ++r)
        {
            int lo = 0, hi = 0;
            if (!LineSpan(lines, b, r, lo, hi)) continue;
            if (!out.empty()) out += '\n';
            out += lines[static_cast<std::size_t>(r)].substr(
                static_cast<std::size_t>(lo), static_cast<std::size_t>(hi - lo));
        }
        return out;
    }
    //!

    /*!
        Erase 

        !_method

        Remove the block's contents from the document in place. For each row in
        the block it uses LineSpan to locate the horizontal slice and erases
        that range, leaving the characters to the left and right intact. Rows
        outside the block, or with an empty slice, are skipped.
    */
    void Erase(std::vector<std::string>& lines, const Block& b)
    {
        for (int r = b.row_lo; r <= b.row_hi; ++r)
        {
            int lo = 0, hi = 0;
            if (!LineSpan(lines, b, r, lo, hi)) continue;
            lines[static_cast<std::size_t>(r)].erase(
                static_cast<std::size_t>(lo), static_cast<std::size_t>(hi - lo));
        }
    }
    //!

    /*!
        Transform

        !_method

        Apply a character transformation to the block in place. For each row it
        uses LineSpan to find the slice and then replaces every character in
        that range with fold(character), where fold is a passed-in function
        such as tolower or toupper. Characters outside the block are untouched.
    */
    void Transform(std::vector<std::string>& lines, const Block& b,
                   char (*fold)(char))
    {
        for (int r = b.row_lo; r <= b.row_hi; ++r)
        {
            int lo = 0, hi = 0;
            if (!LineSpan(lines, b, r, lo, hi)) continue;
            std::string& line = lines[static_cast<std::size_t>(r)];
            for (int c = lo; c < hi; ++c)
            {
                line[static_cast<std::size_t>(c)] = fold(line[static_cast<std::size_t>(c)]);
            }
        }
    }
    //!

    /*!
        Insert 

        !_method

        Insert text into every row of the block. When at_end is true the text
        goes just after the block's right edge (col_hi + 1, clamped to the line
        end); otherwise it goes at the block's left edge (col_lo), first padding
        the row with spaces if it is shorter than col_lo. Does nothing when text
        is empty. This mirrors Vim's block insert (I) and block append (A).
    */
    void Insert(std::vector<std::string>& lines, const Block& b,
                const std::string& text, bool at_end)
    {
        if (text.empty()) return;
        for (int r = b.row_lo; r <= b.row_hi; ++r)
        {
            std::string& line = lines[static_cast<std::size_t>(r)];
            if (at_end)
            {
                const int pos = std::min(b.col_hi + 1, static_cast<int>(line.size()));
                line.insert(static_cast<std::size_t>(pos), text);
            }
            else
            {
                if (static_cast<int>(line.size()) < b.col_lo)
                {
                    line.append(static_cast<std::size_t>(b.col_lo) -
                                    static_cast<std::size_t>(line.size()),
                                ' ');
                }
                line.insert(static_cast<std::size_t>(b.col_lo), text);
            }
        }
    }
    //!
}
