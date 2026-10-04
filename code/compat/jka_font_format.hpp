/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_JKA_FONT_FORMAT_HPP
#define FNTECH3_JKA_FONT_FORMAT_HPP
#include "raven_font_format.hpp"
#include <algorithm>
#include <string>

namespace fntech3::jka::font {
using raven::font::Font;
using raven::font::Glyph;
using raven::font::FileBytes;
using raven::font::GlyphBytes;
using raven::font::GlyphCount;
using raven::font::Characters;
using raven::font::ColorEscape;
using raven::font::Finite;

inline bool Read(const void *data, std::size_t length, Font &destination) noexcept {
    Font next;
    if (!raven::font::Read(data, length, next)) return false;
    /* Retail arialnb has a zero metric header. Pinned OpenJK tr_font.cpp
     * CFontInfo supplies height=pointSize and a rounded 10%+2 descender.
     * This independently expressed fallback admits that installed asset. */
    if (next.height == 0) {
        next.height = next.pointSize;
        next.descender = static_cast<std::int16_t>(std::min<int>(next.height, (next.pointSize+5)/10+2));
        next.ascender = static_cast<std::int16_t>(next.height-next.descender);
    }
    destination = next;
    return true;
}

inline bool Path(const char *name, std::string &destination) {
    if (!name) return false;
    unsigned length = 0;
    for (; length < 64 && name[length]; ++length) {}
    if (length == 64) return false;
    std::string leaf(name, length);
    if (leaf.size() >= 6) {
        bool prefix = true;
        constexpr char directory[]{"fonts/"};
        for (unsigned n = 0; n < 6; ++n) {
            const char letter = leaf[n] >= 'A' && leaf[n] <= 'Z' ? leaf[n]+('a'-'A') : leaf[n];
            if (letter != directory[n]) prefix = false;
        }
        if (prefix) leaf.erase(0, 6);
    }
    if (leaf.empty() || leaf.size() > 48 || leaf.find_first_of(":/\\") != std::string::npos ||
        leaf.find("..") != std::string::npos) return false;
    for (const unsigned char c : leaf) if (c < 32 || c == 127) return false;
    destination = "fonts/"+leaf;
    return true;
}

inline int Width(const Font &font, const char *text, float scale) noexcept {
    if (!text || !Finite(scale) || scale < 0) return 0;
    double line = 0, maximum = 0;
    for (; *text; ++text) {
        if (ColorEscape(text)) { ++text; continue; }
        if (*text == '\r') continue;
        if (*text == '\n') { line = 0; continue; }
        line += font.glyphs[static_cast<unsigned char>(*text)].advance * double(scale);
        maximum = std::max(maximum, line);
    }
    if (maximum >= std::numeric_limits<int>::max()) return std::numeric_limits<int>::max();
    return static_cast<int>(std::ceil(maximum));
}
} // namespace fntech3::jka::font
#endif
