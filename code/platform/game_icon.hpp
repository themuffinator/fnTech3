// Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later.
#pragma once

#include "../qcommon/game_profile.h"
#include <filesystem>
#include <string>
#include <cstdint>

namespace fntech3::icon {
enum class Source { Executable, Png, Bundle };

// Presentation metadata only: never changes the selected profile or loads code.
inline const char *Candidate(comGameId_t profile, Source source, unsigned index)
{
    // One row per profile, in catalogue order. Enemy Territory's, Elite
    // Force's, Jedi Academy's, Jedi Outcast's and Alice's installations
    // carry their executables; no PNG or bundle name is guessed for them.
    static const char *const executables[][3] = {
        {nullptr}, {"quake3.exe", nullptr},
        {"quakelive_steam.exe", "quakelive.exe", nullptr},
        {"WolfSP.exe", nullptr}, {"WolfMP.exe", nullptr},
        {"et.exe", nullptr},
        {"stvoy.exe", nullptr}, {"stvoyHM.exe", nullptr}, {"quake3.exe", nullptr},
        {"jasp.exe", nullptr}, {"jamp.exe", nullptr},
        {"jk2sp.exe", nullptr}, {"jk2mp.exe", nullptr},
        {"alice.exe", nullptr}
    };
    static const char *const pngs[][3] = {
        {nullptr}, {"quake3.png", nullptr}, {"quakelive.png", nullptr},
        {"wolfsp.png", "wolfenstein.png", nullptr},
        {"wolfmp.png", "wolfenstein.png", nullptr},
        {nullptr}, {nullptr}, {nullptr}, {nullptr},
        {nullptr}, {nullptr}, {nullptr}, {nullptr}, {nullptr}
    };
    static const char *const bundles[][3] = {
        {nullptr}, {"Quake3.app", "Quake III Arena.app", nullptr}, {nullptr},
        {"Wolfenstein SP.app", "Return to Castle Wolfenstein.app", nullptr},
        {"Wolfenstein MP.app", nullptr},
        {nullptr}, {nullptr}, {nullptr}, {nullptr},
        {nullptr}, {nullptr}, {nullptr}, {nullptr}, {nullptr}
    };
    // A profile without a row would index past these tables and hand a stray
    // pointer to the path constructor, which is how Enemy Territory's first
    // client start crashed.
    static_assert(sizeof(executables) / sizeof(executables[0]) == COM_GAME_COUNT,
        "Each profile needs an executable icon row");
    static_assert(sizeof(pngs) / sizeof(pngs[0]) == COM_GAME_COUNT,
        "Each profile needs a PNG icon row");
    static_assert(sizeof(bundles) / sizeof(bundles[0]) == COM_GAME_COUNT,
        "Each profile needs a bundle icon row");
    if (profile <= COM_GAME_AUTO || profile >= COM_GAME_COUNT || index >= 3)
        return nullptr;
    switch (source) {
    case Source::Executable: return executables[profile][index];
    case Source::Png: return pngs[profile][index];
    case Source::Bundle: return bundles[profile][index];
    }
    return nullptr;
}

// Never search PATH, mods, paks, downloads or a different installation. Resolve
// links before checking containment; an icon failure must not stop the engine.
inline std::filesystem::path Resolve(const char *root, comGameId_t profile,
                                    Source source, unsigned index)
{
    const char *leaf = Candidate(profile, source, index);
    if (!root || !*root || !leaf) return {};
    try {
        const auto base = std::filesystem::canonical(std::filesystem::u8path(root));
        const auto path = std::filesystem::canonical(base / leaf);
        if (path.parent_path() != base) return {};
        if (source == Source::Bundle) {
            if (std::filesystem::is_directory(path)) return path;
        } else if (std::filesystem::is_regular_file(path)) {
            return path;
        }
    } catch (const std::exception &) {}
    return {};
}

// Check dimensions before handing PNGs to a decoder, so a small compressed
// input cannot ask it to allocate an enormous surface. Full validation belongs
// to SDL's decoder. Caller also caps file size and reads from the same buffer.
inline bool ReasonablePng(const unsigned char *data, size_t size)
{
    static const unsigned char signature[] = {137, 80, 78, 71, 13, 10, 26, 10};
    if (!data || size < 33 || size > 4 * 1024 * 1024) return false;
    for (unsigned i = 0; i < 8; ++i)
        if (data[i] != signature[i]) return false;
    if (data[8] || data[9] || data[10] || data[11] != 13 ||
        data[12] != 'I' || data[13] != 'H' || data[14] != 'D' || data[15] != 'R')
        return false;
    const auto dimension = [data](unsigned offset) {
        return (uint32_t(data[offset]) << 24) | (uint32_t(data[offset + 1]) << 16) |
               (uint32_t(data[offset + 2]) << 8) | data[offset + 3];
    };
    const auto width = dimension(16), height = dimension(20);
    return width && height && width <= 1024 && height <= 1024;
}
} // namespace fntech3::icon
