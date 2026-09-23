#pragma once

#include <string>
#include <vector>

namespace scribbolyth::bookmark
{
    /*!
        Bookmark Struct
        * id -- string
        * line -- int

        If line == -1 the bookmark targets the node itself.
        If line >= 0 the bookmark targets a specific line within the node's text
    */
    struct Bookmark
    {
        std::string id;
        int line = -1;
    };

    std::string NewId();
    /*!*/
}
