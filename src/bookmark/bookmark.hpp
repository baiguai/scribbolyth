#pragma once

#include <string>
#include <vector>

namespace scribbolyth::bookmark
{
    /*!
        Bookmark Struct

    */
    //>>
    struct Bookmark
    {
        std::string id;
        int line = -1;
    };
    //<<
    //!

    /*!
        Variables

        ----
    */
    //>>
    std::string NewId();
    //<<
    //!
}
