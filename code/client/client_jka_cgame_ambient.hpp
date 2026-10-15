/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_CLIENT_JKA_CGAME_AMBIENT_HPP
#define FNTECH3_CLIENT_JKA_CGAME_AMBIENT_HPP
#include "../compat/jka_ambient_sets.hpp"
#include "../compat/jka_ambient_player.hpp"
#include "client_jka_sound.hpp"
#include <array>
#include <memory>

/* Retail MP 49/50 load selected definitions even with s_initsound 0. Keep
 * module names, bounded file bytes and registered handles in engine storage.
 * Registration uses Fn audio.
 *
 * Import 48 plays the set the client game names through the measured player
 * (compat/jka_ambient_player.hpp, retail S_UpdateAmbientSet 0x468420). The
 * player and the numbering of parsed sets belong to the engine, not to one
 * client game, as retail's do: a map's sets get new numbers, and the set of
 * the map before fades out on the next one. Both start over only when the
 * sound system does (CL_JKAClearAmbientPlayer from S_Init), whose sound
 * handles the player holds. The loop goes to Fn's ambient-loop path and the
 * subwaves are Fn one-shots on Academy's ambient channel, so the engine's own
 * room, tone and output features apply to both.
 *
 * Import 51 plays a set around a point, an entity's, through the same
 * generator (PlayLocal, retail AS_AddLocalSet 0x4684f0): its loop at the point
 * through the same loop path, placed there by Fn's distance model, and its
 * subwaves on the entity, paced by the entity's last subwave time, which the
 * engine keeps; the import answers as retail's does. */
namespace {
static_assert(fntech3::jka::ambient::MaxPlayWaves >= fntech3::jka::ambient::MaxWaves);
struct JKACgameAmbientSounds {
    sfxHandle_t loop{};
    std::array<sfxHandle_t, fntech3::jka::ambient::MaxWaves> waves{};
};
struct JKACgameAmbientState {
    fntech3::jka::ambient::Catalog catalog;
    std::vector<char> text;
    std::vector<JKACgameAmbientSounds> sounds;
    std::uint32_t firstId{};            // the number of this parse's first set
    fileHandle_t file{};
};
std::unique_ptr<JKACgameAmbientState> jkaCgameAmbient;
fntech3::jka::ambient::Player jkaAmbientPlayer;
std::uint32_t jkaAmbientNextId{};
// Each entity's last subwave from a set around it (import 51), kept by the
// engine as the player keeps its slots' (PlayLocal).
std::array<std::int32_t, MAX_GENTITIES> jkaLocalWaveTimes{};

/* Where the player's loops and waves go. Retail starts a wave on entity 0
 * (0x468402); the world entity keeps it from belonging to whichever player
 * holds slot 0, and Fn's world sounds keep the one ambient channel. */
struct JKAAmbientSink {
    void Loop(const float *origin, int handle, int volume) {
        S_AddJKAAmbientLoopingSound(origin, handle, volume);
    }
    void Wave(const float *origin, int handle, int volume) {
        EntityWave(origin, ENTITYNUM_WORLD, handle, volume);
    }
    /* A set heard around a point starts its subwave on the client game's
     * entity (S_StartAmbientSound 0x46aaf0). Retail's handle 0 is the silent
     * "sound/null.wav" its registration starts with (0x469ed0), which Fn has
     * no need to play. */
    void EntityWave(const float *origin, int entity, int handle, int volume) {
        if (handle <= 0 || volume <= 0) return; // no registered sound, or silent
        vec3_t at; VectorCopy(origin, at);
        S_StartSoundVolume(at, entity, fntech3::jka::sound::EngineChannel(fntech3::jka::sound::ChanAmbient),
            handle, static_cast<float>(volume) / static_cast<float>(fntech3::jka::ambient::MaxSetVolume));
    }
};

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
    // Retail adds this parse's sets to the engine's under new numbers (0x467c50).
    state.firstId = jkaAmbientNextId;
    jkaAmbientNextId += static_cast<std::uint32_t>(state.sounds.size());
    Com_Printf("Jedi Academy cgame ambient definitions parsed: %u selected names, %u sets\n",
        static_cast<unsigned>(state.catalog.SelectedCount()), static_cast<unsigned>(state.sounds.size()));
}
static bool CL_JKACgameAmbientCall(intptr_t *args, fntech3::jka::module::Call call, intptr_t &result) {
    using fntech3::jka::module::Call;
    switch (call) {
    case Call::CG_S_MUTESOUND:
        // Retail 0x4128f8 hands entity and channel to S_MuteSound 0x46ac30
        // and returns zero. A weapon switch mutes the player's weapon channel.
        if (args[1] < 0 || args[1] >= MAX_GENTITIES || args[2] < INT_MIN || args[2] > INT_MAX)
            Com_Error(ERR_DROP, "Invalid Jedi Academy cgame sound mute");
        S_MuteJKASound(static_cast<int>(args[1]), static_cast<int>(args[2]));
        result = 0;
        return true;
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
        // Retail reads the listener's view origin only for a set it holds.
        const auto *origin = static_cast<const float *>(CL_JKACgameSpan(args[2], sizeof(vec3_t)));
        if (!fntech3::jka::client::FiniteVector(origin))
            Com_Error(ERR_DROP, "Invalid Jedi Academy cgame ambient origin");
        const auto &state = *jkaCgameAmbient;
        const auto index = std::size_t(definition - state.catalog.Definitions().data());
        if (index >= state.sounds.size())
            Com_Error(ERR_DROP, "Jedi Academy cgame ambient handles are not ready");
        fntech3::jka::ambient::PlaySet set;
        set.id = state.firstId + static_cast<std::uint32_t>(index);
        set.loop = state.sounds[index].loop;
        set.waveCount = definition->waves.size();
        for (std::size_t wave = 0; wave < set.waveCount; ++wave) set.waves[wave] = state.sounds[index].waves[wave];
        set.minimumSeconds = definition->minimumSeconds;
        set.maximumSeconds = definition->maximumSeconds;
        set.minimumVolume = definition->minimumVolume;
        set.maximumVolume = definition->maximumVolume;
        JKAAmbientSink sink;
        jkaAmbientPlayer.Update(set, origin, cls.realtime, sink);
        result = 0;
        return true;
    }
    case Call::CG_S_ADDLOCALSET: {
        // Retail 0x412aae hands name, listener, origin, entity and time to
        // AS_AddLocalSet 0x4684f0, which answers its real time for a name it
        // does not hold (0x468504) before reading anything else.
        const auto name = CL_JKACgameAmbientName(args[1]);
        if (name[0] && !jkaCgameAmbient)
            Com_Error(ERR_DROP, "Jedi Academy cgame local ambient set has no loaded catalog");
        const auto *definition = jkaCgameAmbient ? jkaCgameAmbient->catalog.Find(name.data()) : nullptr;
        if (!definition) {
            result = cls.realtime;
            return true;
        }
        const auto *listener = static_cast<const float *>(CL_JKACgameSpan(args[2], sizeof(vec3_t)));
        const auto *origin = static_cast<const float *>(CL_JKACgameSpan(args[3], sizeof(vec3_t)));
        if (!fntech3::jka::client::FiniteVector(listener) || !fntech3::jka::client::FiniteVector(origin))
            Com_Error(ERR_DROP, "Invalid Jedi Academy cgame local ambient origin");
        if (args[4] < 0 || args[4] >= MAX_GENTITIES)
            Com_Error(ERR_DROP, "Invalid Jedi Academy cgame local ambient entity %ld", static_cast<long>(args[4]));
        const auto &state = *jkaCgameAmbient;
        const auto index = std::size_t(definition - state.catalog.Definitions().data());
        if (index >= state.sounds.size())
            Com_Error(ERR_DROP, "Jedi Academy cgame ambient handles are not ready");
        fntech3::jka::ambient::PlaySet set;
        set.id = state.firstId + static_cast<std::uint32_t>(index);
        set.loop = state.sounds[index].loop;
        set.waveCount = definition->waves.size();
        for (std::size_t wave = 0; wave < set.waveCount; ++wave) set.waves[wave] = state.sounds[index].waves[wave];
        set.minimumSeconds = definition->minimumSeconds;
        set.maximumSeconds = definition->maximumSeconds;
        set.minimumVolume = definition->minimumVolume;
        set.maximumVolume = definition->maximumVolume;
        // Any set may be named here, a general or mover set too; it plays at
        // the volume its own record holds, which the general sets' fades write.
        set.masterVolume = jkaAmbientPlayer.MasterVolume(set.id);
        JKAAmbientSink sink;
        const auto entity = static_cast<std::size_t>(args[4]);
        result = fntech3::jka::ambient::PlayLocal(set, definition->radius, listener, origin,
            static_cast<int>(entity), static_cast<std::int32_t>(args[5]), cls.realtime,
            jkaLocalWaveTimes[entity], jkaAmbientPlayer.Generator(), sink);
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

/* The sound system is starting: the playing sets' handles belong to the one
 * that has gone, so the player and the numbering start over, as retail's do
 * when its sound system shuts down (0x469c33..0x469c52). */
void CL_JKAClearAmbientPlayer(void) {
    jkaAmbientPlayer.Reset();
    jkaAmbientNextId = 0;
    jkaLocalWaveTimes.fill(0);
}
#endif
