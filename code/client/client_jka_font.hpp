/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_CLIENT_JKA_FONT_HPP
#define FNTECH3_CLIENT_JKA_FONT_HPP
#include "../compat/jka_font_format.hpp"
#include <algorithm>
#include <array>
#include <utility>
#include <vector>

/* Each including module host owns its font handles. Glyphs from Raven's data
 * use fnTech3's active renderer, colour pipeline and caller's HUD/menu
 * transform. This initial path covers the font's single-byte atlas; Asian
 * multi-page language substitutions remain a separate localization boundary. */
namespace {
struct JKAFont {
    std::string path;
    fntech3::jka::font::Font metrics;
    qhandle_t shader = 0;
};
std::vector<JKAFont> jkaFonts;

void CL_JKAFontReset() { jkaFonts.clear(); }

const char *CL_JKAFontText(vm_t *vm, intptr_t address, unsigned limit = 16384) {
    if (!address) Com_Error(ERR_DROP, "Missing Jedi Academy font string");
    const auto *text = static_cast<const char *>(VM_ExplicitArgPtr(vm, address));
    for (unsigned n = 0; n < limit; ++n) {
        VM_CHECKBOUNDS(vm, address+n, 1);
        if (!text[n]) return text;
    }
    Com_Error(ERR_DROP, "Unterminated Jedi Academy font string");
    return "";
}

int CL_JKARegisterFont(const char *name) {
    std::string path;
    if (!fntech3::jka::font::Path(name, path))
        Com_Error(ERR_DROP, "Invalid Jedi Academy font path");
    for (std::size_t n = 0; n < jkaFonts.size(); ++n) {
        if (!Q_stricmp(jkaFonts[n].path.c_str(), path.c_str())) {
            jkaFonts[n].shader = re.RegisterShaderNoMip(path.c_str());
            return jkaFonts[n].shader ? static_cast<int>(n+1) : 0;
        }
    }
    if (jkaFonts.size() >= 32) Com_Error(ERR_DROP, "Jedi Academy font limit exceeded");
    void *bytes = nullptr;
    const int length = FS_ReadFile((path+".fontdat").c_str(), &bytes);
    JKAFont font;
    const bool valid = length >= 0 && fntech3::jka::font::Read(bytes, static_cast<std::size_t>(length), font.metrics);
    if (bytes) FS_FreeFile(bytes);
    if (!valid) {
        Com_Printf(S_COLOR_YELLOW "Jedi Academy font %s is missing or invalid\n", path.c_str());
        return 0;
    }
    font.path = path;
    font.shader = re.RegisterShaderNoMip(path.c_str());
    if (!font.shader) return 0;
    jkaFonts.push_back(std::move(font));
    return static_cast<int>(jkaFonts.size());
}

const JKAFont *CL_JKAFindFont(intptr_t handle) {
    const auto index = static_cast<std::uint32_t>(handle) & 0x00ffffffu;
    return index && index <= jkaFonts.size() ? &jkaFonts[index-1] : nullptr;
}

void CL_JKAFontScale(float scale) {
    if (!fntech3::jka::font::Finite(scale) || scale < 0 || scale > 128)
        Com_Error(ERR_DROP, "Invalid Jedi Academy font scale");
}

/* The glyphs a string lays out, in drawing order, each after the colour it
 * takes: what CL_JKADrawFont draws and CL_JKAFontArea measures. Invalid
 * arguments stop before anything is laid out. */
template<class Draw, class Color>
void CL_JKALayFont(const JKAFont &font, const char *text, float x, float y,
    const float *rgba, std::uint32_t style, int limit, float scale, Draw draw, Color setColor) {
    CL_JKAFontScale(scale);
    if (limit < -1 || (style & 0x3f000000u))
        Com_Error(ERR_DROP, "Invalid Jedi Academy font drawing arguments");
    if ((style & 0x40000000u) && ((Sys_Milliseconds() >> 7) & 1)) return;
    std::array<float, 4> color{1, 1, 1, 1};
    if (rgba) {
        for (unsigned n = 0; n < 4; ++n) {
            if (!fntech3::jka::font::Finite(rgba[n])) Com_Error(ERR_DROP, "Invalid Jedi Academy font color");
            color[n] = rgba[n];
        }
    }
    const float start = x;
    y += (font.metrics.height-font.metrics.descender/2)*scale;
    setColor(color.data());
    for (; *text; ++text) {
        if (fntech3::jka::font::ColorEscape(text)) {
            std::copy_n(g_color_table[ColorIndex(*++text)], 3, color.begin());
            setColor(color.data());
            continue;
        }
        if (*text == '\r') continue;
        if (*text == '\n') { x = start; y += font.metrics.pointSize*scale; continue; }
        const auto &glyph = font.metrics.glyphs[static_cast<unsigned char>(*text)];
        const float advance = glyph.advance*scale;
        if (limit >= 0 && x+advance-start > limit) break;
        if (glyph.width && glyph.height) {
            const float left = x+glyph.offset*scale, top = y-glyph.baseline*scale;
            if (style & 0x80000000u) {
                const float shadow[]{0, 0, 0, color[3]};
                setColor(shadow);
                draw(left+scale, top+scale, glyph.width*scale, glyph.height*scale, glyph, font.shader);
                setColor(color.data());
            }
            draw(left, top, glyph.width*scale, glyph.height*scale, glyph, font.shader);
        }
        x += advance;
    }
    setColor(nullptr);
}

template<class Draw>
void CL_JKADrawFont(const JKAFont &font, const char *text, float x, float y,
    const float *rgba, std::uint32_t style, int limit, float scale, Draw draw) {
    CL_JKALayFont(font, text, x, y, rgba, style, limit, scale, draw,
        [](const float *color) { re.SetColor(color); });
}

/* The area {x1, y1, x2, y2} a string's glyphs cover, shadows included, in the
 * caller's units; false, with the area untouched, when it lays out none. */
inline bool CL_JKAFontArea(const JKAFont &font, const char *text, float x, float y,
    const float *rgba, std::uint32_t style, int limit, float scale, float area[4]) {
    bool any = false;
    float x1 = 0, y1 = 0, x2 = 0, y2 = 0;
    CL_JKALayFont(font, text, x, y, rgba, style, limit, scale,
        [&](float left, float top, float width, float height, const auto &, qhandle_t) {
            x1 = any ? (std::min)(x1, left) : left; y1 = any ? (std::min)(y1, top) : top;
            x2 = any ? (std::max)(x2, left+width) : left+width; y2 = any ? (std::max)(y2, top+height) : top+height;
            any = true;
        }, [](const float *) {});
    if (any) { area[0] = x1; area[1] = y1; area[2] = x2; area[3] = y2; }
    return any;
}

/* line(area) is called before a string's glyphs are drawn, with the area they
 * cover, or an empty one (x2 < x1) when there are none, and line(nullptr)
 * after them: a host places the string as one line of text. */
template<class Draw, class Line>
bool CL_JKAFontCall(vm_t *vm, intptr_t *args, intptr_t &result, int first, Draw draw, Line line) {
    const auto ordinal = args[0]-first;
    if (ordinal < 0 || ordinal > 4) return false;
    result = 0;
    if (ordinal == 0) { result = CL_JKARegisterFont(CL_JKAFontText(vm, args[1], 64)); return true; }
    if (ordinal == 2) { result = fntech3::jka::font::Characters(CL_JKAFontText(vm, args[1])); return true; }
    const auto *font = CL_JKAFindFont(ordinal == 3 ? args[1] : ordinal == 4 ? args[5] : args[2]);
    if (!font) return true;
    const float scale = ordinal == 4 ? VMF(7) : ordinal == 3 ? VMF(2) : VMF(3);
    CL_JKAFontScale(scale);
    if (ordinal == 1) { result = fntech3::jka::font::Width(font->metrics, CL_JKAFontText(vm, args[1]), scale); return true; }
    if (ordinal == 3) { result = static_cast<int>(font->metrics.pointSize*scale); return true; }
    const float *rgba = nullptr;
    if (args[4]) {
        VM_CHECKBOUNDS(vm, args[4], sizeof(float)*4);
        rgba = static_cast<const float *>(VM_ExplicitArgPtr(vm, args[4]));
    }
    const char *text = CL_JKAFontText(vm, args[3]);
    const float x = static_cast<float>(args[1]), y = static_cast<float>(args[2]);
    const auto style = static_cast<std::uint32_t>(args[5]) & 0xff000000u;
    const int limit = static_cast<int>(args[6]);
    float area[4]{0, 0, -1, -1};
    CL_JKAFontArea(*font, text, x, y, rgba, style, limit, scale, area);
    line(static_cast<const float *>(area));
    CL_JKADrawFont(*font, text, x, y, rgba, style, limit, scale, draw);
    line(static_cast<const float *>(nullptr));
    return true;
}

template<class Draw>
bool CL_JKAFontCall(vm_t *vm, intptr_t *args, intptr_t &result, int first, Draw draw) {
    return CL_JKAFontCall(vm, args, result, first, draw, [](const float *) {});
}
} // namespace
#endif
