#pragma once

#include <algorithm>
#include <string>
#include <vector>

/*[./Introduction]
    Visual Block mode (Vim Ctrl+V): a rectangular, column-wise selection.
    The anchor and cursor positions define the block's row span and column
    span; every line between row_lo and row_hi has its columns between
    col_lo and col_hi (inclusive) selected. Lines shorter than the block
    simply expose no columns past their end.
*/
namespace scribbolyth::visual_block
{

    /*!
        Block Struct 
    */
    //>>
    struct Block
    {
        int row_lo = 0;
        int row_hi = 0;
        int col_lo = 0;
        int col_hi = 0;
    };
    //<<
    //!

    /*!
        Make Block 

        !_method
    */
    inline Block MakeBlock(int aRow, int aCol, int bRow, int bCol)
    {
        return Block{std::min(aRow, bRow), std::max(aRow, bRow),
                     std::min(aCol, bCol), std::max(aCol, bCol)};
    }
    //!

    bool LineSpan(const std::vector<std::string>& lines, const Block& b, int row,
                  int& lo, int& hi);

    std::string Extract(const std::vector<std::string>& lines, const Block& b);

    // Erase the block's columns from every covered row.
    void Erase(std::vector<std::string>& lines, const Block& b);

    // Apply a case fold to the block's columns on every covered row.
    void Transform(std::vector<std::string>& lines, const Block& b,
                   char (*fold)(char));

    // Insert `text` on every covered row. When `at_end` is false the text is
    // inserted at the block's start column, padding shorter rows with spaces
    // so the inserted text lines up. When `at_end` is true it is inserted
    // just past the block's right edge (or the row's end, whichever is first).
    void Insert(std::vector<std::string>& lines, const Block& b,
                const std::string& text, bool at_end);
}
