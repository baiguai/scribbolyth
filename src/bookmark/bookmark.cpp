#include "bookmark.hpp"

#include <random>

namespace scribbolyth::bookmark
{
    /*!
        NewId
        Generates a new random ID for bookmarks using an alphabet char array.
    */
    std::string NewId()
    {
        //>>
        static const char alphabet[] = "abcdefghijklmnopqrstuvwxyz0123456789";
        //<<
        /*|
        
            Mersenne Twister logic:

        */
        //>>
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<int> dist(0, static_cast<int>(sizeof(alphabet)) - 2);
        std::string id;
        id.reserve(8);
        for (int i = 0; i < 8; ++i) id += alphabet[dist(gen)];
        //<<
        return id;
    }
    /*!*/
}
