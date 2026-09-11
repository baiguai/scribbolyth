#pragma once

#include <string>

namespace scribbolyth::text
{
    // Replace typographic (smart) punctuation with plain ASCII equivalents so
    // the result is always renderable in the terminal ("smart" quote/dash
    // characters often do not display). Only the exact UTF-8 sequences of the
    // mapped characters are consumed; every other byte, including other
    // multi-byte characters, passes through untouched.
    std::string ToAsciiPunctuation(const std::string& text);
}