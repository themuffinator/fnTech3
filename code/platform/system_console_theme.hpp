// Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later.
#pragma once

#include "../qcommon/game_profile.h"
#include <cstdint>

namespace fntech3::syscon {
struct Color {
    std::uint8_t r, g, b;
};

struct Palette {
    Color window, panel, input, footer, border;
    Color text, muted, accent;
    Color button, buttonHover, buttonDown, buttonOff;
};

constexpr Color Mix(Color a, Color b, unsigned percent)
{
    return {
        std::uint8_t((a.r * (100 - percent) + b.r * percent) / 100),
        std::uint8_t((a.g * (100 - percent) + b.g * percent) / 100),
        std::uint8_t((a.b * (100 - percent) + b.b * percent) / 100)
    };
}

constexpr Palette MakePalette(Color background, Color text, Color accent)
{
    return {
        Mix(background, {0, 0, 0}, 40), background,
        Mix(background, text, 4), Mix(background, {0, 0, 0}, 15),
        Mix(background, text, 22), text, Mix(text, background, 25), accent,
        Mix(background, text, 6), Mix(background, text, 10),
        Mix(background, text, 14), Mix(background, {0, 0, 0}, 15)
    };
}

// Engine-owned presentation, inspired by the title artwork and remade icons
// in assets/icons/games. Dark, desaturated surfaces and pale tinted text keep
// branding quiet. Modes and test releases share their parent title's palette.
constexpr Palette ForTitle(comGameTitleId_t title)
{
    switch (title) {
    case COM_GAME_TITLE_QUAKE3: // Crimson emblem, warm silver.
        return MakePalette({36, 27, 29}, {238, 226, 222}, {204, 158, 152});
    case COM_GAME_TITLE_QUAKELIVE: // Red disc, pearl lettering.
        return MakePalette({35, 29, 28}, {239, 232, 225}, {211, 167, 157});
    case COM_GAME_TITLE_RTCW: // Maroon and silver.
        return MakePalette({37, 29, 32}, {238, 228, 225}, {197, 155, 154});
    case COM_GAME_TITLE_WOLFET: // Military charcoal, steel and khaki.
        return MakePalette({32, 34, 30}, {230, 231, 219}, {180, 185, 163});
    case COM_GAME_TITLE_STVEF: // Starfleet navy, silver and gold.
        return MakePalette({28, 34, 42}, {231, 232, 229}, {200, 181, 138});
    case COM_GAME_TITLE_JKA: // Blue saber glow, silver.
        return MakePalette({29, 36, 43}, {224, 232, 239}, {160, 185, 208});
    case COM_GAME_TITLE_JK2: // Cyan saber glow, steel.
        return MakePalette({27, 37, 39}, {224, 234, 235}, {155, 193, 200});
    case COM_GAME_TITLE_ALICE: // Gothic burgundy, porcelain.
        return MakePalette({36, 29, 35}, {234, 223, 229}, {192, 158, 173});
    case COM_GAME_TITLE_FAKK2: // Rust-red metal, bone.
        return MakePalette({40, 32, 28}, {238, 227, 216}, {199, 166, 143});
    case COM_GAME_TITLE_MOHAA: // Olive charcoal, silver and brass.
        return MakePalette({33, 34, 29}, {233, 230, 215}, {188, 177, 142});
    default: // Startup and profile-detection errors have no active title.
        return MakePalette({29, 32, 36}, {230, 233, 237}, {180, 191, 204});
    }
}
} // namespace fntech3::syscon
