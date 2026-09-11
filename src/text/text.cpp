#include "text.hpp"

#include <string>
#include <utility>
#include <vector>

namespace scribbolyth::text
{
    namespace
    {
        const std::vector<std::pair<std::string, std::string>>& PunctuationMap()
        {
            static const std::vector<std::pair<std::string, std::string>> kMap = {
                {"\xE2\x80\x98", "'"},    // U+2018 left single quote
                {"\xE2\x80\x99", "'"},    // U+2019 right single quote
                {"\xE2\x80\x9A", "'"},    // U+201A low single quote
                {"\xE2\x80\x9C", "\""},   // U+201C left double quote
                {"\xE2\x80\x9D", "\""},   // U+201D right double quote
                {"\xE2\x80\x9E", "\""},   // U+201E low double quote
                {"\xE2\x80\x93", "-"},    // U+2013 en dash
                {"\xE2\x80\x94", "-"},    // U+2014 em dash
                {"\xE2\x80\xA6", "..."},  // U+2026 ellipsis
                {"\xE2\x88\x92", "-"},    // U+2212 minus sign
            };
            return kMap;
        }
    }

    std::string ToAsciiPunctuation(const std::string& text)
    {
        const auto& kMap = PunctuationMap();

        std::string out;
        out.reserve(text.size());
        for (std::size_t i = 0; i < text.size();)
        {
            bool replaced = false;
            for (const auto& entry : kMap)
            {
                if (text.compare(i, entry.first.size(), entry.first) == 0)
                {
                    out += entry.second;
                    i += entry.first.size();
                    replaced = true;
                    break;
                }
            }
            if (!replaced)
            {
                out += text[i];
                ++i;
            }
        }
        return out;
    }
}