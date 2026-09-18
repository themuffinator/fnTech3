"""Execute original RTCW cvar/import boundaries without loading a game module."""
from pathlib import Path
import os
import re
import shutil
import subprocess
import tempfile
import unittest
from compiler_support import compiler_target_args
import rtcw_key_fixture
import rtcw_collision_fixture

ROOT = Path(__file__).resolve().parents[1]


def function(source, signature):
    start = source.index(signature)
    return source[start:source.index("\n}", start) + 2]


PRELUDE = r'''
#include "code/qcommon/q_shared.h"
#include "code/qcommon/qcommon.h"
#include "code/compat/game_abi_adapter.h"
#include "code/compat/rtcw_module_abi.h"
#include "code/compat/rtcw_state_adapter.hpp"
#include "code/compat/rtcw_client_adapter.hpp"
#include "code/ui/ui_public.h"
#include "code/game/g_public.h"
#include <array>
#include <algorithm>
#include <cassert>
#include <climits>
#include <cstring>
#include <initializer_list>
// Renderer imports are executed by rtcw_render_import_tests.
template<class VM> static bool CL_RTCWRenderCall(VM *, fntech3::rtcw::module::Call, intptr_t *, intptr_t &) { return false; }
// Audio argument arrays are executed separately by rtcw_music_tests.
template<class VM> static bool CL_RTCWAudioCall(VM *, fntech3::rtcw::module::Call, intptr_t *) { return false; }
template<class VM> static bool CL_RTCWCinematicCall(VM *, fntech3::rtcw::module::Call, intptr_t *, intptr_t &) { return false; }
template<class VM> static bool CL_RTCWCameraCall(VM *, fntech3::rtcw::module::Call, intptr_t *, intptr_t &) { return false; }
// World calls and their real game-data host execute in rtcw_server_world_tests.
template<class VM> static bool SV_RTCWWorldCall(VM *, fntech3::rtcw::module::Call, const intptr_t *, intptr_t &) { return false; }
// Bot-library bootstrap has its own argument/lifecycle execution fixture.
template<class VM> static bool SV_RTCWBotLibCall(VM *, fntech3::rtcw::module::Call, const intptr_t *, intptr_t &) { return false; }
// Tag handoff and the real nested cgame callback run in rtcw_tag_handoff_tests.
template<class VM> static intptr_t SV_RTCWGetTag(VM *, intptr_t, intptr_t, intptr_t) { assert(false); return 0; }
// Checked file writes and rename dispatch execute in rtcw_file_write_tests.
template<class VM> static void VM_RTCWRenameFile(VM *, intptr_t, intptr_t) { assert(false); }
static comGameProfile_t profile;
const comGameProfile_t *Com_GameProfile(void) { return &profile; }
int Com_GameIsRTCW(void) { return profile.id == COM_GAME_RTCW_SP || profile.id == COM_GAME_RTCW_MP; }
int Com_GameIsRTCWSP(void) { return profile.id == COM_GAME_RTCW_SP; }
int Com_GameIsRTCWMP(void) { return profile.id == COM_GAME_RTCW_MP; }
// Only the key-name import reads a cvar by name here; English names.
int Cvar_VariableIntegerValue(const char *name) { assert(!std::strcmp(name, "cl_language")); return 0; }
int Com_GameIsWolfET(void) { return profile.id == COM_GAME_WOLFET; }
// ET's independent game and UI boundaries are exercised by wolfet tests and
// probes. This fixture drives only RTCW, so ET declines every call here.
static bool SV_WolfETGameCall(const intptr_t *, intptr_t &, intptr_t &) { assert(false); return false; }
// Nor does Elite Force's game boundary: its fixture profiles are RTCW's.
int Com_GameIsSTVEFMP(void) { return profile.id == COM_GAME_STVEF_MP; }
static bool SV_STVEFGameCall(const intptr_t *, intptr_t &, intptr_t &) { assert(false); return false; }
static bool CL_WolfETUICall(intptr_t *, intptr_t &, intptr_t &) { assert(false); return false; }
// Elite Force's UI dispatcher has its own probe.
static bool CL_STVEFUICall(intptr_t *, intptr_t &, intptr_t &) { assert(false); return false; }
static intptr_t *CL_WolfETUIBrowserArguments(intptr_t, intptr_t *args, std::array<intptr_t, 6> &) { return args; }
void QDECL Com_Error(errorParm_t, const char *, ...) { throw 7; }
static int warnings;
static int translationCalls;
void CL_RTCWTranslateString(vm_t *, intptr_t source, intptr_t destination) {
    assert(source == 111 && destination == 222); ++translationCalls;
}
qboolean CL_RTCWGetLimboString(vm_t *, intptr_t index, intptr_t destination) {
    assert(index == 3 && destination == 777); return qtrue;
}
static int soundCalls, soundHandle, soundChannel;
static bool failSound;
sfxHandle_t S_RegisterSound(const char *name, qboolean compressed) {
    ++soundCalls; assert(name && !std::strcmp(name, "sound/menu/test.wav") && compressed == qfalse);
    if (failSound) throw 10;
    return 137;
}
void S_StartLocalSound(sfxHandle_t handle, int channel) {
    ++soundCalls; soundHandle = handle; soundChannel = channel;
    if (failSound) throw 10;
}
static bool failCalendar;
static int calendarCalls;
int Com_RealTime(qtime_t *destination) {
    ++calendarCalls;
    if (destination && !failCalendar) *destination = {37, 41, 15, 29, 8, 126, 2, 271, 1};
    return 0x12345678;
}
void QDECL Com_Printf(const char *, ...) { ++warnings; }
void QDECL Com_DPrintf(const char *, ...) {}
void Q_strncpyz(char *out, const char *in, int size) {
    assert(size > 0);
    int i = 0;
    while (i + 1 < size && in[i]) { out[i] = in[i]; ++i; }
    out[i] = 0;
}
static cvar_t cvar_indexes[4];
static unsigned cvar_numIndexes = 4;
static int observedFlags, registrations;
static cvar_t *Cvar_FindVar(const char *name) {
    for (auto &cv : cvar_indexes) if (cv.name && !std::strcmp(name, cv.name)) return &cv;
    return nullptr;
}
cvar_t *Cvar_Get(const char *name, const char *, int flags) {
    auto *cv = Cvar_FindVar(name); assert(cv);
    observedFlags = flags; ++registrations; cv->flags |= flags;
    return cv;
}
static int infoFlags, infoCalls, configIndex, publications;
static bool truncateInfo;
int cvar_modifiedFlags;
const char *Cvar_InfoString(int flags, qboolean *truncated) {
    infoFlags = flags; ++infoCalls;
    if (truncated) *truncated = truncateInfo ? qtrue : qfalse;
    return "\\gamestate\\2";
}
void Cvar_InfoStringBuffer(int flags, char *buffer, int length) {
    Q_strncpyz(buffer, Cvar_InfoString(flags, nullptr), length);
}
void SV_SetConfigstring(int index, const char *value) {
    assert(!std::strcmp(value, "\\gamestate\\2")); configIndex = index; ++publications;
}
struct vm_s { int privateFlag, syscallCount; void *entryPoint, *dllExports; byte *dataBase; int exactDataLength; };
#include "code/client/client_rtcw_sound_name.hpp"
static vm_t module{CVAR_PRIVATE, 0};
static vm_t *uivm = &module, *gvm = &module;
static size_t lastBounds, previousBounds;
#undef VM_CHECKBOUNDS
#define VM_CHECKBOUNDS(vm, ptr, size) (previousBounds = lastBounds, lastBounds = (size))
void *VM_ArgPtr(intptr_t value) { return reinterpret_cast<void *>(value); }
struct Client { usercmd_t lastUsercmd; };
static std::array<Client, 128> clients;
static glconfig_t rendererConfig;
static const glconfig_t *RendererConfig() { return &rendererConfig; }
static struct { const glconfig_t *(*GetConfig)(); } re{RendererConfig};
static int podCalls;
static void GetRTCWClientState(fntech3_rtcw_uiClientState_t *destination) {
    assert(destination); ++podCalls;
}
void CL_CopyRTCWGlconfig(void *destination, const glconfig_t *source) {
    assert(destination && source == &rendererConfig); ++podCalls;
}
int CL_RTCWReadToken(int handle, void *destination) {
    assert(handle == 19 && destination); ++podCalls; return 1;
}
int CL_RTCWSourceFileAndLine(int handle, char *filename, int *line) {
    assert(handle == 19 && filename && line); *line = 37; return 1;
}
void CL_RTCWRegisterFont(const char *name, int size, void *destination) {
    assert(!std::strcmp(name, "font") && size == 27 && destination); ++podCalls;
}
static bool SV_IsClientIndex(int index) { return index >= 0 && index < (Com_GameIsRTCWSP() ? 128 : 64); }
static Client &SV_ClientForIndex(int index) { return clients[index]; }
'''

CHECKS = r'''
using namespace fntech3::rtcw::module;
template<class T> struct Guarded {
    uint32_t before = 0x11223344;
    T value;
    uint32_t after = 0xaabbccdd;
    void Check() const { assert(before == 0x11223344 && after == 0xaabbccdd); }
};
static void Reset(comGameId_t id) {
    profile.id = id; profile.key = Com_GameIsRTCWSP() ? "rtcw-sp" : "rtcw-mp";
    std::memset(cvar_indexes, 0, sizeof(cvar_indexes));
    cvar_indexes[0].name = const_cast<char *>("private");
    cvar_indexes[0].string = const_cast<char *>("secret");
    cvar_indexes[0].flags = CVAR_PRIVATE;
    cvar_indexes[0].modificationCount = 44;
    cvar_indexes[1].name = const_cast<char *>("safe");
    cvar_indexes[1].string = const_cast<char *>("12.5");
    cvar_indexes[1].modificationCount = 3;
    cvar_indexes[1].integer = -321; cvar_indexes[1].value = 12.5f;
    registrations = 0; module.syscallCount = 0;
}
static void Registration(intptr_t (*dispatch)(intptr_t *), Call registerCall, Call updateCall) {
    Guarded<fntech3_rtcw_vmCvar_t> record;
    std::memset(&record.value, 0xa5, sizeof(record.value));
    intptr_t args[] = {Ordinal(profile.id, registerCall), (intptr_t)&record.value,
        (intptr_t)"safe", (intptr_t)"17", 0x8c09};
    assert(dispatch(args) == 0 && lastBounds == 272);
    record.Check();
    assert(record.value.handle == 1 && record.value.modificationCount == 3);
    assert(record.value.integer == -321 && record.value.value == 12.5f);
    assert(!std::strcmp(record.value.string, "12.5") && record.value.string[255] == 0);
    const int expected = CVAR_VM_CREATED | CVAR_ARCHIVE | CVAR_SYSTEMINFO | CVAR_NORESTART
        | (Com_GameIsRTCWMP() ? CVAR_RTCW_WOLFINFO : 0);
    assert(observedFlags == expected);
    assert(!(observedFlags & (CVAR_PROTECTED | CVAR_QL_WEAPON_RELOAD | CVAR_QL_PMOVE_SETTINGS)));
    intptr_t update[] = {Ordinal(profile.id, updateCall), (intptr_t)&record.value};
    // An unchanged record is preserved even when its entire string lacks NUL.
    std::memset(record.value.string, 0xfe, sizeof(record.value.string));
    const auto unchanged = record;
    assert(dispatch(update) == 0 && lastBounds == 272);
    assert(!std::memcmp(&record, &unchanged, sizeof(record)));
    char text[600]; std::memset(text, 'x', sizeof(text)); text[599] = 0;
    cvar_indexes[1].string = text; ++cvar_indexes[1].modificationCount;
    assert(dispatch(update) == 0); record.Check();
    for (int i = 0; i < 255; ++i) assert(record.value.string[i] == 'x');
    assert(record.value.string[255] == 0 && record.value.modificationCount == 4);
    const auto saved = record;
    for (int bad : {-1, 4, INT_MAX}) {
        record.value.handle = bad; const auto invalid = record; bool threw = false;
        try { dispatch(update); } catch (int) { threw = true; }
        assert(threw && !std::memcmp(&record, &invalid, sizeof(record)));
    }
    record = saved; record.value.handle = 0;
    const auto hidden = record;
    assert(dispatch(update) == 0 && !std::memcmp(&record, &hidden, sizeof(record)));
    args[2] = (intptr_t)"private";
    assert(dispatch(args) == 0 && !std::memcmp(&record, &hidden, sizeof(record)));
    // Null registration creates metadata without attempting a module write.
    args[1] = 0; args[2] = (intptr_t)"safe";
    assert(dispatch(args) == 0);
    update[1] = 0; assert(dispatch(update) == 0);
}
template<class Command> static void Usercmd() {
    const int client = Com_GameIsRTCWSP() ? 127 : 63;
    auto &cmd = clients[client].lastUsercmd; cmd = {};
    cmd.serverTime = INT_MIN; cmd.weapon = 255; cmd.buttons = 0x81;
    cmd.rtcw.wbuttons = 0xab; cmd.rtcw.holdable = 250; cmd.rtcw.wolfkick = -7;
    cmd.forwardmove = -128; cmd.rightmove = 127; cmd.upmove = -1;
    cmd.rtcw.cld = 65535; cmd.rtcw.mpSetup = 91; cmd.rtcw.identClient = 61;
    Guarded<Command> record;
    intptr_t args[] = {Ordinal(profile.id, Call::G_GET_USERCMD), client, (intptr_t)&record.value};
    assert(SV_GameSystemCalls(args) == 0 && lastBounds == 28); record.Check();
    assert(record.value.common.serverTime == INT_MIN && record.value.common.weapon == 255);
    assert(record.value.common.buttons == 0x81 && record.value.common.wbuttons == 0xab);
    assert(record.value.common.holdable == 250 && record.value.common.wolfkick == -7);
    assert(record.value.common.forwardmove == -128 && record.value.common.rightmove == 127);
    const auto *bytes = reinterpret_cast<const byte *>(&record.value);
    assert(bytes[26] == 0 && bytes[27] == 0);
    assert(bytes[24] == (Com_GameIsRTCWSP() ? 255 : 91));
    assert(bytes[25] == (Com_GameIsRTCWSP() ? 255 : 61));
    for (int bad : {-1, client + 1, INT_MAX}) {
        args[1] = bad; const auto saved = record; bool threw = false;
        try { SV_GameSystemCalls(args); } catch (int) { threw = true; }
        assert(threw && !std::memcmp(&record, &saved, sizeof(record)));
    }
    args[1] = client; args[2] = 0; bool threw = false;
    try { SV_GameSystemCalls(args); } catch (int) { threw = true; }
    assert(threw);
}
int main() {
    // Every original combination, plus unknown high bits, cannot acquire QL flags.
    for (auto id : {COM_GAME_RTCW_SP, COM_GAME_RTCW_MP}) {
        for (uint32_t bits = 0; bits <= 0xfff; ++bits) {
            const uint32_t expected = (bits & 0x7ff) | (id == COM_GAME_RTCW_MP && (bits & 0x800) ? 0x04000000 : 0);
            assert(FnTech3_RTCWCvarFlagsToInternal(id, bits) == expected);
            assert(FnTech3_RTCWCvarFlagsToInternal(id, bits | 0xfffff000u) == expected);
        }
        Reset(id); CheckRTCWKeyImports(CL_UISystemCalls, true);
        // Original MP list IDs differ from SP/Q3; queue indices must not be remapped.
        for (intptr_t source : {intptr_t(-1), intptr_t(0), intptr_t(1), intptr_t(2), intptr_t(3), INTPTR_MAX}) {
            std::array<intptr_t, 6> mapped;
            intptr_t args[] = {75, source, -1, 1, 617, 919};
            const std::array<intptr_t, 6> saved = {75, source, -1, 1, 617, 919};
            const int expected[] = {0, 2, 3, -1};
            const int expectedSP[] = {0, -1, 2, 3};
            for (auto op : {UI_LAN_GETSERVERCOUNT, UI_LAN_UPDATEVISIBLEPINGS, UI_LAN_RESETPINGS,
                           UI_LAN_GETSERVERPING, UI_LAN_SERVERISVISIBLE, UI_LAN_MARKSERVERVISIBLE,
                           UI_LAN_GETSERVERADDRESSSTRING, UI_LAN_GETSERVERINFO, UI_LAN_COMPARESERVERS}) {
                mapped.fill(-777);
                const auto out = CL_RTCWUIBrowserArguments(op, args, mapped);
                assert(out == mapped.data() && out[0] == 75);
                assert(out[1] == (source < 0 || source > 3 ? -1 : Com_GameIsRTCWMP() ? expected[source] : expectedSP[source]));
                if (op == UI_LAN_COMPARESERVERS) assert(out[2] == -1 && out[3] == 1 && out[4] == 617 && out[5] == 919);
                if (op == UI_LAN_GETSERVERCOUNT) assert(out[2] == -777); // Never read unused arguments.
                assert(!std::memcmp(args, saved.data(), sizeof(args)));
            }
            for (auto op : {UI_LAN_CLEARPING, UI_LAN_GETPING, UI_LAN_GETPINGINFO, UI_LAN_SERVERSTATUS}) {
                mapped.fill(-777);
                assert(CL_RTCWUIBrowserArguments(op, args, mapped) == args && mapped[0] == -777);
            }
        }
        for (auto call : {Call::UI_LAN_GETPINGQUEUECOUNT, Call::UI_LAN_CLEARPING, Call::UI_LAN_GETPINGINFO,
                          Call::UI_LAN_GETSERVERCOUNT, Call::UI_LAN_GETSERVERADDRESSSTRING,
                          Call::UI_LAN_GETSERVERINFO, Call::UI_LAN_GETSERVERPING, Call::UI_LAN_MARKSERVERVISIBLE,
                          Call::UI_LAN_SERVERISVISIBLE, Call::UI_LAN_UPDATEVISIBLEPINGS, Call::UI_LAN_RESETPINGS,
                          Call::UI_LAN_SERVERSTATUS, Call::UI_LAN_COMPARESERVERS}) {
            intptr_t args[] = {Ordinal(id, call), 2, -1, 1, 617, 919};
            assert(CL_UISystemCalls(args) == 10000 + CL_RTCWUISharedCall(call));
            assert(args[1] == 2); // Do not rewrite the module's argument frame.
        }
        for (auto call : {Call::UI_LAN_GETSERVERADDRESSSTRING, Call::UI_LAN_GETSERVERINFO,
                          Call::UI_LAN_GETPING, Call::UI_LAN_GETPINGINFO, Call::UI_LAN_SERVERSTATUS}) {
            const int buffer = call == Call::UI_LAN_GETSERVERADDRESSSTRING || call == Call::UI_LAN_GETSERVERINFO ? 3 : 2;
            for (intptr_t badLength : {intptr_t(-1), intptr_t(0)}) {
                intptr_t args[] = {Ordinal(id, call), 0, 17, 31, 47, 0};
                args[buffer + 1] = badLength;
                bool failed = false; try { CL_UISystemCalls(args); } catch (int e) { failed = e == 7; }
                assert(failed);
            }
            intptr_t args[] = {Ordinal(id, call), 0, 17, 31, 47, 0};
            args[buffer] = 0;
            bool failed = false; try { CL_UISystemCalls(args); } catch (int e) { failed = e == 7; }
            assert(failed);
        }
        intptr_t resetStatus[] = {Ordinal(id, Call::UI_LAN_SERVERSTATUS), 0, 0, 0};
        assert(CL_UISystemCalls(resetStatus) == 10000 + UI_LAN_SERVERSTATUS);
        intptr_t ping[] = {Ordinal(id, Call::UI_LAN_GETPING), 0, 17, 31, 47};
        lastBounds = 0;
        assert(CL_UISystemCalls(ping) == 10000 + UI_LAN_GETPING && lastBounds == sizeof(int));
        ping[4] = 0;
        bool pingFailed = false; try { CL_UISystemCalls(ping); } catch (int e) { pingFailed = e == 7; }
        assert(pingFailed);
        if (id == COM_GAME_RTCW_MP) {
            intptr_t translated[]{Ordinal(id, Call::UI_CL_TRANSLATE_STRING), 111, 222};
            translationCalls = 0; assert(CL_UISystemCalls(translated) == 0 && translationCalls == 1);
            intptr_t limbo[] = {Ordinal(id, Call::UI_CL_GETLIMBOSTRING), 3, 777};
            assert(CL_UISystemCalls(limbo) == qtrue);
            intptr_t music[]{Ordinal(id, Call::UI_S_STARTBACKGROUNDTRACK)};
            assert(CL_UISystemCalls(music) == 10000 + UI_S_STARTBACKGROUNDTRACK);
            intptr_t update[]{Ordinal(id, Call::UI_CHECKAUTOUPDATE)};
            const int notices = warnings;
            assert(CL_UISystemCalls(update) == 0 && warnings == notices + 1);
            assert(CL_UISystemCalls(update) == 0 && warnings == notices + 1);
            update[0] = Ordinal(id, Call::UI_GET_AUTOUPDATE);
            assert(CL_UISystemCalls(update) == 0 && warnings == notices + 2);
        } else {
            assert(CL_RTCWUISharedCall(Call::UI_S_STARTBACKGROUNDTRACK) == -1);
        }
        // Minimal arrays expose accidental reads of the absent compression
        // argument. The original RTCW UI always requests uncompressed sound.
        soundCalls = 0;
        module.entryPoint = reinterpret_cast<decltype(module.entryPoint)>(1);
        intptr_t sound[]{Ordinal(id, Call::UI_S_REGISTERSOUND), (intptr_t)"sound/menu/test.wav"};
        assert(CL_UISystemCalls(sound) == 137 && soundCalls == 1);
        failSound = true;
        bool soundThrew = false; try { CL_UISystemCalls(sound); } catch (int e) { soundThrew = e == 10; }
        assert(soundThrew && soundCalls == 2); failSound = false;
        sound[1] = 0; soundThrew = false;
        try { CL_UISystemCalls(sound); } catch (int e) { soundThrew = e == 7; }
        assert(soundThrew && soundCalls == 2);
        for (int handle : {INT_MIN, 0, 137, INT_MAX}) for (int channel : {INT_MIN, 0, 7, INT_MAX}) {
            intptr_t local[]{Ordinal(id, Call::UI_S_STARTLOCALSOUND), handle, channel};
            assert(CL_UISystemCalls(local) == 0 && soundHandle == handle && soundChannel == channel);
        }
        if (sizeof(intptr_t) > sizeof(int)) for (int slot : {1,2}) {
            intptr_t local[]{Ordinal(id, Call::UI_S_STARTLOCALSOUND), 137, 1}; local[slot] = INTPTR_MAX;
            const int before = soundCalls; soundThrew = false;
            try { CL_UISystemCalls(local); } catch (int e) { soundThrew = e == 7; }
            assert(soundThrew && soundCalls == before);
        }
        Reset(id); CheckRTCWCollisionImports(SV_GameSystemCalls, true);
        intptr_t pointContents[]{Ordinal(id, Call::G_POINT_CONTENTS)};
        assert(SV_GameSystemCalls(pointContents) == 10000 + G_POINT_CONTENTS);
        Reset(id); Registration(CL_UISystemCalls, Call::UI_CVAR_REGISTER, Call::UI_CVAR_UPDATE);
        Reset(id); Registration(SV_GameSystemCalls, Call::G_CVAR_REGISTER, Call::G_CVAR_UPDATE);
        Reset(id);
        intptr_t create[] = {Ordinal(id, Call::UI_CVAR_CREATE), (intptr_t)"safe", (intptr_t)"0", 0x800};
        assert(CL_UISystemCalls(create) == 0 && registrations == 1);
        assert(observedFlags == (CVAR_VM_CREATED | (Com_GameIsRTCWMP() ? CVAR_RTCW_WOLFINFO : 0)));
        Guarded<std::array<char, 32>> info;
        intptr_t query[] = {Ordinal(id, Call::UI_CVAR_INFOSTRINGBUFFER), 0x8800, (intptr_t)info.value.data(), 32};
        assert(CL_UISystemCalls(query) == 0 && lastBounds == 32); info.Check();
        assert(infoFlags == (Com_GameIsRTCWMP() ? CVAR_RTCW_WOLFINFO : 0));
        assert(!std::strcmp(info.value.data(), "\\gamestate\\2"));
        fntech3_rtcw_uiClientState_t state{};
        intptr_t stateArgs[] = {Ordinal(id, Call::UI_GETCLIENTSTATE), (intptr_t)&state};
        const int previousPods = podCalls;
        assert(CL_UISystemCalls(stateArgs) == 0 && lastBounds == 3084);
        fntech3_rtcw_glconfig_t glconfig{};
        intptr_t gl[] = {Ordinal(id, Call::UI_GETGLCONFIG), (intptr_t)&glconfig};
        assert(CL_UISystemCalls(gl) == 0 && lastBounds == 7268);
        fntech3_rtcw_pc_token_t token{};
        intptr_t parsed[] = {Ordinal(id, Call::UI_PC_READ_TOKEN), 19, (intptr_t)&token};
        assert(CL_UISystemCalls(parsed) == 1 && lastBounds == 1040);
        fntech3_rtcw_fontInfo_t font{};
        intptr_t fontArgs[] = {Ordinal(id, Call::UI_R_REGISTERFONT), (intptr_t)"font", 27, (intptr_t)&font};
        assert(CL_UISystemCalls(fontArgs) == 0 && lastBounds == 20548 && podCalls == previousPods + 4);
        char filename[128]; int line = 0;
        intptr_t location[] = {Ordinal(id, Call::UI_PC_SOURCE_FILE_AND_LINE), 19, (intptr_t)filename, (intptr_t)&line};
        assert(CL_UISystemCalls(location) == 1 && previousBounds == 128 && lastBounds == 4 && line == 37);
        Guarded<fntech3_rtcw_qtime_t> calendar;
        const int32_t expectedTime[9] = {37, 41, 15, 29, 8, 126, 2, 271, 1};
        for (auto dispatch : {CL_UISystemCalls, SV_GameSystemCalls}) {
            intptr_t timeArgs[] = {Ordinal(id, dispatch == CL_UISystemCalls ? Call::UI_REAL_TIME : Call::G_REAL_TIME),
                (intptr_t)&calendar.value};
            assert(dispatch(timeArgs) == 0x12345678 && lastBounds == 36); calendar.Check();
            assert(!std::memcmp(&calendar.value, expectedTime, sizeof(expectedTime)));
            const auto savedTime = calendar; failCalendar = true;
            assert(dispatch(timeArgs) == 0x12345678 && !std::memcmp(&calendar, &savedTime, sizeof(calendar)));
            failCalendar = false; timeArgs[1] = 0;
            const int previousTimeCalls = calendarCalls;
            assert(dispatch(timeArgs) == 0x12345678 && calendarCalls == previousTimeCalls + 1);
        }
        if (Com_GameIsRTCWSP()) Usercmd<fntech3_rtcw_sp_usercmd_t>();
        else Usercmd<fntech3_rtcw_mp_usercmd_t>();
        // Same external number means two different file operations in SP/MP.
        intptr_t collision[] = {15};
        assert(CL_UISystemCalls(collision) == 10000 + (Com_GameIsRTCWSP() ? UI_FS_SEEK : UI_FS_WRITE));
        for (auto call : {Call::UI_PRINT, Call::UI_MEMSET, Call::UI_SQRT, Call::UI_GETCONFIGSTRING,
                          Call::UI_LAN_LOADCACHEDSERVERS, Call::UI_LAN_SAVECACHEDSERVERS,
                          Call::UI_S_STOPBACKGROUNDTRACK}) {
            intptr_t args[] = {Ordinal(id, call)};
            assert(CL_UISystemCalls(args) == 10000 + CL_RTCWUISharedCall(call));
        }
        for (auto call : {Call::G_PRINT, Call::G_LOCATE_GAME_DATA, Call::G_GET_CONFIGSTRING, Call::G_MILLISECONDS}) {
            intptr_t args[] = {Ordinal(id, call)};
            assert(SV_GameSystemCalls(args) == 10000 + SV_RTCWGameSharedCall(call));
        }
        for (intptr_t bad : {intptr_t(-1), INTPTR_MAX, intptr_t(Ordinal(id, Call::UI_R_ADDCORONATOSCENE))}) {
            intptr_t args[] = {bad}; bool threw = false;
            try { CL_UISystemCalls(args); } catch (int) { threw = true; }
            assert(threw);
        }
        for (intptr_t bad : {intptr_t(-1), INTPTR_MAX, intptr_t(Ordinal(id, Call::BOTLIB_AAS_ENTITY_INFO))}) {
            intptr_t args[] = {bad}; bool threw = false;
            try { SV_GameSystemCalls(args); } catch (int) { threw = true; }
            assert(threw);
        }
    }
    Reset(COM_GAME_RTCW_SP);
    intptr_t endgame[] = {2}; bool threw = false;
    try { SV_GameSystemCalls(endgame); } catch (int) { threw = true; }
    assert(threw); // SP endgame must never become MP/Q3 milliseconds.
    module.syscallCount = 1024 * 1024;
    intptr_t print[] = {0}; threw = false;
    try { SV_GameSystemCalls(print); } catch (int) { threw = true; }
    assert(threw);
    for (auto id : {COM_GAME_AUTO, COM_GAME_QUAKE3, COM_GAME_QUAKELIVE, static_cast<comGameId_t>(UINT_MAX)}) {
        Reset(id); assert(FnTech3_RTCWCvarFlagsToInternal(id, UINT_MAX) == 0);
        intptr_t args[] = {1234};
        assert(CL_UISystemCalls(args) == 11234 && SV_GameSystemCalls(args) == 11234);
        threw = false;
        try { Cvar_RegisterRTCW(nullptr, "safe", "0", 0, 0); } catch (int) { threw = true; }
        assert(threw && registrations == 0);
    }
    for (auto id : {COM_GAME_QUAKE3, COM_GAME_QUAKELIVE, COM_GAME_RTCW_SP, COM_GAME_RTCW_MP, COM_GAME_WOLFET}) {
        Reset(id); publications = infoCalls = 0; warnings = 0; truncateInfo = false;
        cvar_modifiedFlags = CVAR_PROTECTED | CVAR_QL_PMOVE_SETTINGS;
        SV_UpdateRTCWCvarConfigstrings(qfalse); assert(publications == 0);
        cvar_modifiedFlags |= CVAR_RTCW_WOLFINFO;
        const int saved = cvar_modifiedFlags;
        SV_UpdateRTCWCvarConfigstrings(qfalse);
        const bool wolfInfo = id == COM_GAME_RTCW_MP || id == COM_GAME_WOLFET;
        assert(publications == (wolfInfo ? 1 : 0));
        assert(infoCalls == publications);
        if (wolfInfo) {
            assert(configIndex == (id == COM_GAME_WOLFET ? 21 : 36) && infoFlags == 0x04000000);
            assert(cvar_modifiedFlags == (CVAR_PROTECTED | CVAR_QL_PMOVE_SETTINGS));
            SV_UpdateRTCWCvarConfigstrings(qfalse); assert(publications == 1);
            truncateInfo = true;
            SV_UpdateRTCWCvarConfigstrings(qtrue); assert(publications == 2 && warnings == 1);
        } else {
            assert(cvar_modifiedFlags == saved);
            SV_UpdateRTCWCvarConfigstrings(qtrue); assert(publications == 0 && infoCalls == 0);
        }
    }
}
'''


class RTCWModuleCvarTests(unittest.TestCase):
    def test_cvar_records_dispatch_and_mp_info_channel(self):
        compiler = shutil.which("clang++") or shutil.which("g++")
        if not compiler:
            self.skipTest("C++17 compiler unavailable")
        cvar = (ROOT / "code/qcommon/cvar.c").read_text(encoding="utf-8")
        adapter = (ROOT / "code/compat/game_abi_adapter.c").read_text(encoding="utf-8")
        ui = (ROOT / "code/client/cl_ui.cpp").read_text(encoding="utf-8")
        game = (ROOT / "code/server/sv_game.cpp").read_text(encoding="utf-8")
        init = (ROOT / "code/server/sv_init.cpp").read_text(encoding="utf-8")
        bodies = function(adapter, "uint32_t FnTech3_RTCWCvarFlagsToInternal(") + "\n"
        bodies += function(adapter, "void FnTech3_RTCWTimeFromInternal(") + "\n"
        common = (ROOT / "code/qcommon/common.c").read_text(encoding="utf-8")
        bodies += function(common, "int Com_RealTimeRTCW(") + "\n"
        bodies += re.search(r"^#define INVALID_FLAGS .*", cvar, re.M)[0] + "\n"
        for signature in ("void Cvar_Register(", "void Cvar_Update(",
                          "static void Cvar_CopyInternalToRTCW(",
                          "void Cvar_RegisterRTCW(", "void Cvar_UpdateRTCW("):
            bodies += function(cvar, signature) + "\n"
        bodies += function(init, "void SV_UpdateRTCWCvarConfigstrings(") + "\n"
        pointer = game[game.index("class VmArgPtr {"):game.index("\n#undef VMA")]
        bodies += pointer + "\n#define VMA(n) VmArgPtr(VM_ArgPtr(args[n]))\n"
        bodies += function(ui, "static intptr_t CL_RTCWUISharedCall(") + "\n"
        bodies += function(ui, "static intptr_t *CL_RTCWUIBrowserArguments(") + "\n"
        dispatch = function(ui, "static intptr_t CL_UISystemCalls(")
        bodies += dispatch[:dispatch.index("\tswitch( operation )")] + "return 10000 + operation;\n}\n#undef VMA\n"
        for signature in ("static void SV_GetUsercmd(",
                          "template<typename ExternalCommand>\nstatic void SV_GetRTCWUsercmd(",
                          "static void SV_RTCWTrace(",
                          "static intptr_t SV_RTCWGameSharedCall("):
            bodies += function(game, signature) + "\n"
        dispatch = function(game, "static intptr_t SV_GameSystemCalls(")
        bodies += dispatch[:dispatch.index("\tswitch( operation )")] + "return 10000 + operation;\n}\n"
        self.assertIn("SV_UpdateRTCWCvarConfigstrings( qtrue );", function(init, "void SV_SpawnServer("))
        main = (ROOT / "code/server/sv_main.cpp").read_text(encoding="utf-8")
        self.assertIn("SV_UpdateRTCWCvarConfigstrings( qfalse );", function(main, "void SV_Frame("))
        with tempfile.TemporaryDirectory(prefix="rtcw-module-cvar-", dir=ROOT / ".tmp") as temp:
            folder = Path(temp)
            fixture = folder / "probe.cpp"
            executable = folder / ("probe.exe" if os.name == "nt" else "probe")
            fixture.write_text(PRELUDE + rtcw_collision_fixture.SERVICES + rtcw_key_fixture.source() + bodies +
                rtcw_key_fixture.CHECKS + rtcw_collision_fixture.CHECKS + CHECKS, encoding="utf-8")
            target = compiler_target_args(compiler)
            if os.name == "nt" and not target:
                target = ["-DFNTECH3_Q3_ONLY"]
            elif os.name != "nt":
                target = ["-m32"]
            build = subprocess.run([compiler, *target, "-std=c++17", "-I", str(ROOT),
                str(fixture), "-o", str(executable)], capture_output=True, text=True, timeout=60, cwd=ROOT)
            self.assertEqual(build.returncode, 0, build.stdout + build.stderr)
            run = subprocess.run([str(executable)], capture_output=True, text=True, timeout=15, cwd=ROOT)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)


if __name__ == "__main__":
    unittest.main()
