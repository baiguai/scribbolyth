#pragma once

#include <string>

namespace scribbolyth::text
{
    /*!
        Ascii to Punctuation

        !_method

        Replace typographic (smart) punctuation with plain ASCII equivalents so
        the result is always renderable in the terminal ("smart" quote/dash
        characters often do not display). Only the exact UTF-8 sequences of the
        mapped characters are consumed; every other byte, including other
        multi-byte characters, passes through untouched.
    */
    std::string ToAsciiPunctuation(const std::string& text);
    //!

    /*!
        Pad Right Declaration

        !_method

        Pad `s` on the right with spaces until it is `width` columns long.
        Strings that are already at least `width` long are returned unchanged.
        Shared by the dialog modules that align their table columns.
    */
    std::string PadRight(const std::string& s, std::size_t width);
    //!
}
