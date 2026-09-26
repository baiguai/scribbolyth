#pragma once

#include <string>

/*!
    Mode Enum
*/
//>>
enum class Mode
{
    TREE,
    NORMAL,
    INSERT,
    VISUAL,
    VISUAL_LINE,
    VISUAL_BLOCK,
    COMMAND
};
//<<
//!

/*!
    Mode Name

    !_method
*/
inline std::string ModeName(Mode m)
{
    switch(m)
    {
        case Mode::TREE:            return "TREE";
        case Mode::NORMAL:          return "NORMAL";
        case Mode::INSERT:          return "INSERT";
        case Mode::VISUAL:          return "VISUAL";
        case Mode::COMMAND:         return "COMMAND";
        case Mode::VISUAL_LINE:     return "VISUAL LINE";
        case Mode::VISUAL_BLOCK:    return "VISUAL BLOCK";
    }
    return "???";
}
//!
