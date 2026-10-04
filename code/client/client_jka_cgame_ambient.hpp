/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_CLIENT_JKA_CGAME_AMBIENT_HPP
#define FNTECH3_CLIENT_JKA_CGAME_AMBIENT_HPP
#include "../compat/jka_ambient_sets.hpp"
#include <array>
#include <memory>

/* Retail MP 49/50 load selected definitions even with s_initsound 0. Keep
 * module names, bounded file bytes and registered handles in engine storage.
 * Registration uses Fn audio; unimplemented ambient playback remains named. */
namespace {
struct JKACgameAmbientSounds {
    sfxHandle_t loop{};
    std::array<sfxHandle_t, fntech3::jka::ambient::MaxWaves> waves{};
};
struct JKACgameAmbientState {
    fntech3::jka::ambient::Catalog catalog;
    std::vector<char> text;
    std::vector<JKACgameAmbientSounds> sounds;
    fileHandle_t file{};
};
std::unique_ptr<JKACgameAmbientState> jkaCgameAmbient;

static void CL_JKACgameAmbientClose() {
    if (!jkaCgameAmbient || !jkaCgameAmbient->file) return;
    const fileHandle_t file = jkaCgameAmbient->file;
    jkaCgameAmbient->file = 0; // An error during close must not close it twice.
    FS_FCloseFile(file);
}
static void CL_JKACgameAmbientClear() {
    CL_JKACgameAmbientClose();
    jkaCgameAmbient.reset();
}
static JKACgameAmbientState &CL_JKACgameAmbientState() {
    if (!jkaCgameAmbient) jkaCgameAmbient = std::make_unique<JKACgameAmbientState>();
    return *jkaCgameAmbient;
}
static std::array<char, fntech3::jka::ambient::NameBytes> CL_JKACgameAmbientName(intptr_t address) {
    std::array<char, fntech3::jka::ambient::NameBytes> name{};
    if (!address) return name;
    for (std::size_t n = 0; n < name.size(); ++n) {
        if (std::uintptr_t(address) > (std::numeric_limits<std::uintptr_t>::max)() - n)
            Com_Error(ERR_DROP, "Invalid Jedi Academy cgame ambient name address");
        name[n] = *static_cast<const char *>(CL_JKACgameSpan(std::uintptr_t(address) + n, 1));
        if (!name[n]) return name;
    }
    Com_Error(ERR_DROP, "Jedi Academy cgame ambient name exceeds 63 bytes");
    return name;
}
static void CL_JKACgameAmbientParse() {
    using namespace fntech3::jka::ambient;
    auto &state = CL_JKACgameAmbientState();
    CL_JKACgameAmbientClose();
    state.text.clear();
    const int length = FS_FOpenFileRead("sound/sound.txt", &state.file, qfalse);
    if (length <= 0 || !state.file || std::size_t(length) > MaxTextBytes) {
        CL_JKACgameAmbientClose();
        Com_Error(ERR_DROP, "Jedi Academy cgame ambient definitions missing or outside the engine bound");
    }
    // Owned storage survives an engine ERR_DROP from filesystem or audio calls.
    state.text.resize(std::size_t(length));
    const int read = FS_Read(state.text.data(), length, state.file);
    CL_JKACgameAmbientClose();
    if (read != length) {
        state.text.clear();
        Com_Error(ERR_DROP, "Jedi Academy cgame ambient definition read was incomplete");
    }
    Failure failure;
    const bool parsed = state.catalog.Parse(std::string_view(state.text.data(), state.text.size()), failure);
    state.text.clear();
    if (!parsed)
        Com_Error(ERR_DROP, "Jedi Academy cgame ambient definitions line %u: %s",
            static_cast<unsigned>(failure.line), failure.reason);
    state.sounds.clear();
    state.sounds.resize(state.catalog.Definitions().size());
    for (std::size_t n = 0; n < state.sounds.size(); ++n) {
        const auto &definition = state.catalog.Definitions()[n];
        auto &sounds = state.sounds[n];
        if (!definition.loop.empty()) sounds.loop = S_RegisterSound(definition.loop.c_str(), qfalse);
        for (std::size_t wave = 0; wave < definition.waves.size(); ++wave)
            sounds.waves[wave] = S_RegisterSound(definition.waves[wave].c_str(), qfalse);
    }
    Com_Printf("Jedi Academy cgame ambient definitions parsed: %u selected names, %u sets\n",
        static_cast<unsigned>(state.catalog.SelectedCount()), static_cast<unsigned>(state.sounds.size()));
}
static bool CL_JKACgameAmbientCall(intptr_t *args, fntech3::jka::module::Call call, intptr_t &result) {
    using fntech3::jka::module::Call;
    switch (call) {
    case Call::CG_S_UPDATEAMBIENTSET: {
        const auto name = CL_JKACgameAmbientName(args[1]);
        // Retail 0x468439 returns before reading origin when lookup is absent.
        // Null/empty names cannot match a bounded catalog definition either.
        if (name[0] && !jkaCgameAmbient)
            Com_Error(ERR_DROP, "Jedi Academy cgame ambient update has no loaded catalog");
        const auto *definition = jkaCgameAmbient ? jkaCgameAmbient->catalog.Find(name.data()) : nullptr;
        if (!definition) {
            result = 0;
            return true;
        }
        Com_Error(ERR_DROP, "Jedi Academy cgame active ambient playback is not served (name=%s)", name.data());
        return true;
    }
    case Call::CG_AS_ADDPRECACHEENTRY: {
        const auto name = CL_JKACgameAmbientName(args[1]);
        if (!CL_JKACgameAmbientState().catalog.Select(name.data()))
            Com_Error(ERR_DROP, "Invalid or excessive Jedi Academy cgame ambient precache name");
        result = 0;
        return true;
    }
    case Call::CG_AS_PARSESETS:
        CL_JKACgameAmbientParse();
        result = 0;
        return true;
    case Call::CG_AS_GETBMODELSOUND: {
        result = -1;
        const auto name = CL_JKACgameAmbientName(args[1]);
        if (!jkaCgameAmbient || args[2] < 0 || args[2] >= intptr_t(fntech3::jka::ambient::MaxWaves)) return true;
        const auto *definition = jkaCgameAmbient->catalog.Find(name.data());
        if (!definition || std::size_t(args[2]) >= definition->waves.size()) return true;
        const auto &definitions = jkaCgameAmbient->catalog.Definitions();
        const auto index = std::size_t(definition - definitions.data());
        if (index >= jkaCgameAmbient->sounds.size())
            Com_Error(ERR_DROP, "Jedi Academy cgame ambient handles are not ready");
        result = jkaCgameAmbient->sounds[index].waves[std::size_t(args[2])];
        return true;
    }
    default:
        return false;
    }
}
} // namespace
#endif
