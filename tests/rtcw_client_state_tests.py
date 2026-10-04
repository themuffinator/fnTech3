"""Exercise RTCW cgame dispatch and frozen state transfers without a game module."""
from pathlib import Path
import os
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
#include "code/client/client_jka_command_values.hpp"
#include "code/qcommon/qcommon.h"
#include "code/cgame/cg_public.h"
#include "code/compat/rtcw_state_adapter.hpp"
#include "code/compat/rtcw_module_abi.h"
#include "code/compat/rtcw_client_adapter.hpp"
#include "code/qcommon/netchan_safety.hpp"
#include "code/client/input_compat.hpp"
#include <algorithm>
#include <cassert>
#include <climits>
#include <cstring>
#include <initializer_list>
static int translationCalls;
static int soundCalls;
sfxHandle_t S_RegisterSound(const char *name, qboolean compressed) {
    assert(name && !std::strcmp(name, "sound/menu/test.wav") && compressed == qfalse);
    ++soundCalls; return 137;
}
void CL_RTCWTranslateString(vm_t *, intptr_t source, intptr_t destination) {
    assert(source == 111 && destination == 222); ++translationCalls;
}
void CL_RTCWLimboChat(vm_t *, intptr_t source) { assert(source == 123); }
// Renderer imports are executed by rtcw_render_import_tests.
template<class VM> static bool CL_RTCWRenderCall(VM *, fntech3::rtcw::module::Call, intptr_t *, intptr_t &) { return false; }
// Audio dispatch is executed by rtcw_music_tests.
template<class VM> static bool CL_RTCWAudioCall(VM *, fntech3::rtcw::module::Call, intptr_t *) { return false; }
// Voice query lifecycle and entity bounds execute in rtcw_voice_tests.
int S_RTCWVoiceAmplitude(intptr_t) { assert(false); return 0; }
template<class VM> static bool CL_RTCWCinematicCall(VM *, fntech3::rtcw::module::Call, intptr_t *, intptr_t &) { return false; }
// Camera dispatch and lifetime execute in rtcw_camera_tests.
template<class VM> static bool CL_RTCWCameraCall(VM *, fntech3::rtcw::module::Call, intptr_t *, intptr_t &) { return false; }
constexpr int MAX_PARSE_ENTITIES = 2048;
struct clSnapshot_t {
    qboolean valid;
    int snapFlags, ping, serverTime, messageNum, parseEntitiesNum, numEntities, serverCommandNum;
    byte areamask[MAX_MAP_AREA_BYTES];
    playerState_t ps;
};
static struct {
    int cmdNumber, parseEntitiesNum;
    usercmd_t cmds[CMD_BACKUP];
    clSnapshot_t snap, snapshots[PACKET_BACKUP];
    entityState_t parseEntities[MAX_PARSE_ENTITIES];
    gameState_t gameState;
    int cgameUserCmdValue, cgameUserCmdPrimary, cgameUserCmdFov;
    float cgameSensitivity;
    rtcwUsercmdExtension_t cgameRTCWCommand;
    vec3_t cgameRTCWLerpOrigin;
    // Present so CL_FinishMove compiles as written; this fixture drives only
    // the RTCW profile, so these stay untouched.
    struct { unsigned char flags, identClient; } cgameWolfETCommand;
    vec3_t cgameWolfETLerpOrigin;
    int serverTime;
    vec3_t viewangles;
} cl;
static struct { glconfig_t glconfig; } cls;
static int podCalls;
int Com_RealTimeRTCW(void *destination) {
    if (destination) ++podCalls;
    return 12345678;
}
void CL_CopyRTCWGlconfig(void *destination, const glconfig_t *source) {
    assert(destination && source == &cls.glconfig); ++podCalls;
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
static int popupCalls;
void CL_RTCWPopup(const char *name, qboolean close) {
    assert(name && !std::strcmp(name, "popup")); popupCalls += close ? 10 : 1;
}
void QDECL Com_Error(errorParm_t, const char *, ...) { throw 7; }
static comGameProfile_t profile;
// Other profiles must never enter their module dispatch in this RTCW fixture.
int Com_GameIsJK2MP(void) { return profile.id == COM_GAME_JK2_MP; }
int Com_GameIsJKAMP(void) { return profile.id == COM_GAME_JKA_MP; }
static bool CL_JK2CgameCall(intptr_t *, intptr_t &, intptr_t &) { assert(false); return false; }
static bool CL_JKACgameCall(intptr_t *, intptr_t &, intptr_t &) { assert(false); return false; }
const comGameProfile_t *Com_GameProfile(void) { return &profile; }
int Com_GameIsRTCW(void) { return profile.id == COM_GAME_RTCW_SP || profile.id == COM_GAME_RTCW_MP; }
int Com_GameIsWolfET(void) { return profile.id == COM_GAME_WOLFET; }
// The dispatcher offers ET its own answer before the shared trap switch. This
// fixture drives only RTCW, so ET declines every call here.
static bool CL_WolfETCgameCall(intptr_t *, intptr_t &, intptr_t &) { return false; }
// Nor is Elite Force's cgame boundary reached: its dispatcher has its own probe.
int Com_GameIsSTVEFMP(void) { return profile.id == COM_GAME_STVEF_MP; }
static bool CL_STVEFCgameCall(intptr_t *, intptr_t &, intptr_t &) { assert(false); return false; }
int Com_GameIsSTVEFSP(void) { return profile.id == COM_GAME_STVEF_SP; }
static bool CL_STVEFSPCgameCall(intptr_t *, intptr_t &, intptr_t &) { assert(false); return false; }
int Com_GameIsRTCWSP(void) { return profile.id == COM_GAME_RTCW_SP; }
int Com_GameIsRTCWMP(void) { return profile.id == COM_GAME_RTCW_MP; }
// Only the key-name import reads a cvar by name here; English names.
int Cvar_VariableIntegerValue(const char *name) { assert(!std::strcmp(name, "cl_language")); return 0; }
static intptr_t CL_CGameTrapForProfile(intptr_t call) { return call; }
struct vm_s { int privateFlag; void *entryPoint, *dllExports; byte *dataBase; int exactDataLength; };
#include "code/client/client_rtcw_sound_name.hpp"
static vm_t module{CVAR_PRIVATE};
static vm_t *cgvm = &module;
static int moveSpeedCalls;
void SV_RTCWSendMoveSpeeds(const vm_t *source, intptr_t entity, intptr_t text) {
    assert(source == cgvm && entity == INT_MIN && text == 123);
    ++moveSpeedCalls;
}
static int modelInfoCalls;
intptr_t SV_RTCWGetModelInfo(const vm_t *source, intptr_t client, intptr_t name, intptr_t output) {
    assert(Com_GameIsRTCWSP() && source == cgvm && client == 37 && name == 123 && output == 456);
    ++modelInfoCalls; return -97531;
}
static int cvarCalls;
void Cvar_RegisterRTCW(void *value, const char *name, const char *text, int flags, int privateFlag) {
    assert(value && !std::strcmp(name, "safe") && !std::strcmp(text, "12"));
    assert(flags == 0x800 && privateFlag == CVAR_PRIVATE); ++cvarCalls;
}
void Cvar_UpdateRTCW(void *value, int privateFlag) {
    assert(value && privateFlag == CVAR_PRIVATE); ++cvarCalls;
}
static size_t lastBounds, previousBounds;
#undef VM_CHECKBOUNDS
#define VM_CHECKBOUNDS(vm, ptr, size) (previousBounds = lastBounds, lastBounds = (size))
class Pointer {
    intptr_t address;
public:
    explicit Pointer(intptr_t p): address(p) {}
    template<class T> operator T *() const { return reinterpret_cast<T *>(address); }
};
#undef VMA
#define VMA(n) Pointer(args[n])
static float ArgumentFloat(intptr_t value) {
    const int bits = static_cast<int>(value); float result;
    std::memcpy(&result, &bits, sizeof(result)); return result;
}
#undef VMF
#define VMF(n) ArgumentFloat(args[n])
'''

CHECKS = r'''
using namespace fntech3::rtcw::module;
template<class T> struct Guarded {
    uint32_t before = 0x11223344;
    T value;
    uint32_t after = 0xaabbccdd;
    void Check() const { assert(before == 0x11223344 && after == 0xaabbccdd); }
};
static void Reset(int count = 2) {
    std::memset(&cl, 0, sizeof(cl));
    cl.snap.messageNum = 0;
    auto &source = cl.snapshots[0];
    source.valid = qtrue; source.messageNum = 0;
    source.numEntities = count; source.parseEntitiesNum = -1;
    cl.parseEntitiesNum = count - 1;
    source.snapFlags = 9; source.ping = 28; source.serverTime = 13579;
    source.serverCommandNum = 0x12345678;
    source.areamask[31] = 0xe1;
    source.ps.commandTime = 13570;
    source.ps.rtcw.ammo[47] = 777;
    source.ps.rtcw.ammoclip[63] = 888;
    source.ps.rtcw.events[1] = 555;
    source.ps.rtcw.aiState = 3;
    for (int i = 0; i < count; ++i) {
        auto &entity = cl.parseEntities[(2047 + i) & 2047];
        entity.number = i + 1;
        entity.pos.trType = TR_RTCW_LINEAR_STOP_BACK;
        entity.rtcw.events[3] = 700 + i;
    }
    cl.cmdNumber = INT_MIN;
    auto &cmd = cl.cmds[0];
    cmd.serverTime = 24680; cmd.weapon = 23; cmd.rtcw.wbuttons = 0x80;
    cmd.rtcw.cld = 512; cmd.rtcw.mpSetup = 17; cmd.rtcw.identClient = 22;
    cl.gameState.dataCount = 6;
    std::memcpy(cl.gameState.stringData, "\0wolf", 6);
    cl.gameState.stringOffsets[2047] = 1;
}
template<class Snapshot, class Command> static void Mode(comGameId_t id) {
    profile.id = id; profile.key = "fixture";
    if (id == COM_GAME_RTCW_MP) {
        intptr_t translated[]{Ordinal(id, Call::CG_TRANSLATE_STRING), 111, 222};
        translationCalls = 0; assert(CL_CgameSystemCalls(translated) == 0 && translationCalls == 1);
    }
    // RTCW supplies a name alone. An exact two-word array catches accidental
    // access to Quake's absent compression argument under the sanitizers.
    soundCalls = 0;
    module.entryPoint = reinterpret_cast<decltype(module.entryPoint)>(1);
    intptr_t sound[]{Ordinal(id, Call::CG_S_REGISTERSOUND), (intptr_t)"sound/menu/test.wav"};
    assert(CL_CgameSystemCalls(sound) == 137 && soundCalls == 1);
    sound[1] = 0; bool soundThrew = false;
    try { CL_CgameSystemCalls(sound); } catch (int) { soundThrew = true; }
    assert(soundThrew && soundCalls == 1);
    byte soundData[256]{}; std::memcpy(soundData + 3, "sound/menu/test.wav", 20);
    module.entryPoint = nullptr; module.dataBase = soundData; module.exactDataLength = sizeof(soundData);
    sound[1] = 3; assert(CL_CgameSystemCalls(sound) == 137 && soundCalls == 2);
    for(intptr_t bad : {intptr_t(-1), intptr_t(257), INTPTR_MAX}) {
        sound[1] = bad; soundThrew = false;
        try { CL_CgameSystemCalls(sound); } catch(int) { soundThrew = true; }
        assert(soundThrew && soundCalls == 2);
    }
    soundData[255] = 'x'; sound[1] = 255; soundThrew = false;
    try { CL_CgameSystemCalls(sound); } catch(int) { soundThrew = true; }
    assert(soundThrew && soundCalls == 2);
    std::memset(soundData + 32, 'x', MAX_QPATH); sound[1] = 32;
    assert(CL_CgameSystemCalls(sound) == 0 && soundCalls == 2);
    module.entryPoint = reinterpret_cast<decltype(module.entryPoint)>(1);
    sound[1] = INTPTR_MAX; // Do not dereference arbitrary native pointers.
    soundThrew = false;
    try { CL_RegisterRTCWSound(&module, static_cast<intptr_t>(-1)); } catch(int) { soundThrew = true; }
    assert(soundThrew && soundCalls == 2);
    module.dataBase = nullptr; module.exactDataLength = 0;
    CheckRTCWKeyImports(CL_CgameSystemCalls, false);
    CheckRTCWCollisionImports(CL_CgameSystemCalls, false);
    popupCalls = 0;
    intptr_t popup[] = {Ordinal(id, Call::CG_INGAME_POPUP), (intptr_t)"popup"};
    assert(CL_CgameSystemCalls(popup) == 0 && popupCalls == 1);
    popup[0] = Ordinal(id, Call::CG_INGAME_CLOSEPOPUP);
    assert(CL_CgameSystemCalls(popup) == 0 && popupCalls == 11);
    Guarded<Snapshot> snapshot;
    Guarded<Command> command;
    for (int count : {0, 2, 256, 257}) {
        Reset(count);
        std::memset(&snapshot.value, 0x5a, sizeof(snapshot.value));
        intptr_t args[] = {Ordinal(id, Call::CG_GETSNAPSHOT), 0, (intptr_t)&snapshot.value};
        assert(CL_CgameSystemCalls(args) == qtrue);
        assert(lastBounds == sizeof(Snapshot));
        snapshot.Check();
        assert(snapshot.value.numEntities == std::min(count, 256));
        assert(snapshot.value.ps.common.ammo[63] == 777);
        assert(snapshot.value.ps.common.ammoclip[63] == 888);
        assert(snapshot.value.ps.common.events[3] == 555);
        assert(snapshot.value.serverCommandSequence == 0x12345678);
        assert(snapshot.value.numServerCommands == 0);
        assert(snapshot.value.snapFlags == 9 && snapshot.value.ping == 28);
        assert(snapshot.value.serverTime == 13579 && snapshot.value.areamask[31] == 0xe1);
        for (int i = 0; i < snapshot.value.numEntities; ++i) {
            assert(snapshot.value.entities[i].number == i + 1);
            assert(snapshot.value.entities[i].pos.trType == 4);
            assert(snapshot.value.entities[i].events[3] == 700 + i);
        }
        const fntech3_rtcw_entityState_t zero{};
        for (int i = snapshot.value.numEntities; i < 256; ++i)
            assert(!std::memcmp(&snapshot.value.entities[i], &zero, sizeof(zero)));
    }
    Reset();
    std::memset(&snapshot.value, 0x5a, sizeof(snapshot.value));
    const auto saved = snapshot;
    auto missing = [&] {
        assert(!CL_GetRTCWSnapshot(0, &snapshot.value));
        assert(!std::memcmp(&snapshot, &saved, sizeof(snapshot)));
    };
    cl.snapshots[0].valid = qfalse; missing(); Reset();
    cl.snapshots[0].messageNum = 32; missing(); Reset();
    cl.parseEntitiesNum = 2047; missing(); Reset();
    cl.snapshots[0].numEntities = -1; missing(); Reset();
    cl.parseEntities[0].pos.trType = TR_QL_ACCEL; missing(); Reset();
    const int invalidBoolean = 2;
    std::memcpy(&cl.snapshots[0].ps.rtcw.serverCursorHintTrace.allsolid, &invalidBoolean, 4);
    missing(); Reset();
    assert(!CL_GetRTCWSnapshot(0, (Snapshot *)nullptr));
    bool threw = false;
    try { CL_GetRTCWSnapshot(1, &snapshot.value); } catch (int) { threw = true; }
    assert(threw && !std::memcmp(&snapshot, &saved, sizeof(snapshot)));
    intptr_t user[] = {Ordinal(id, Call::CG_GETUSERCMD), INT_MIN, (intptr_t)&command.value};
    assert(CL_CgameSystemCalls(user) == qtrue);
    assert(lastBounds == sizeof(Command));
    command.Check();
    assert(command.value.common.serverTime == 24680 && command.value.common.wbuttons == 0x80);
    assert(command.value.common.weapon == 23);
    const auto savedCommand = command;
    assert(!CL_GetRTCWUserCmd(INT_MAX - 63, &command.value));
    assert(!std::memcmp(&command, &savedCommand, sizeof(command)));
    assert(!CL_GetRTCWUserCmd(INT_MIN, (Command *)nullptr));
    Guarded<fntech3_rtcw_gameState_t> gs;
    intptr_t state[] = {Ordinal(id, Call::CG_GETGAMESTATE), (intptr_t)&gs.value};
    assert(CL_CgameSystemCalls(state) == 0 && lastBounds == sizeof(gs.value));
    gs.Check();
    assert(gs.value.stringOffsets[2047] == 1 && gs.value.dataCount == 6);
    assert(!std::strcmp(gs.value.stringData + 1, "wolf"));
    const auto savedState = gs;
    cl.gameState.stringOffsets[2047] = 6; threw = false;
    try { CL_CgameSystemCalls(state); } catch (int) { threw = true; }
    assert(threw && !std::memcmp(&gs, &savedState, sizeof(gs)));
    for (intptr_t bad : {intptr_t(-1), INTPTR_MAX, intptr_t(Ordinal(id, Call::CG_R_SETFOG))}) {
        // An unsupported ordinal must fail before touching absent arguments.
        intptr_t oneWord[] = {bad}; threw = false;
        try { CL_CgameSystemCalls(oneWord); } catch (int) { threw = true; }
        assert(threw);
    }
    for (auto operation : {Call::CG_PRINT, Call::CG_FS_READ, Call::CG_MEMSET, Call::CG_SQRT,
            Call::CG_CM_LOADMAP, Call::CG_CM_NUMINLINEMODELS, Call::CG_CM_INLINEMODEL,
            Call::CG_CM_TEMPBOXMODEL, Call::CG_CM_TEMPCAPSULEMODEL, Call::CG_CM_POINTCONTENTS,
            Call::CG_CM_TRANSFORMEDPOINTCONTENTS}) {
        intptr_t oneWord[] = {Ordinal(id, operation)};
        assert(CL_CgameSystemCalls(oneWord) == 10000 + CL_RTCWCgameSharedCall(operation));
    }
    fntech3_rtcw_vmCvar_t cvar{};
    intptr_t registration[] = {Ordinal(id, Call::CG_CVAR_REGISTER), (intptr_t)&cvar,
        (intptr_t)"safe", (intptr_t)"12", 0x800};
    const int previousCalls = cvarCalls;
    assert(CL_CgameSystemCalls(registration) == 0 && lastBounds == 272);
    intptr_t update[] = {Ordinal(id, Call::CG_CVAR_UPDATE), (intptr_t)&cvar};
    assert(CL_CgameSystemCalls(update) == 0 && lastBounds == 272 && cvarCalls == previousCalls + 2);

    // SP has four arguments; reading MP's fifth would cross this array.
    intptr_t spValues[] = {Ordinal(id, Call::CG_SETUSERCMDVALUE), 255, 511, 0x3f200000, 0x12345};
    intptr_t mpValues[] = {Ordinal(id, Call::CG_SETUSERCMDVALUE), 255, 511, 0x3f200000, 0x12345, 0x181};
    cl.cgameUserCmdPrimary = 77; cl.cgameUserCmdFov = 121;
    assert(CL_CgameSystemCalls(id == COM_GAME_RTCW_SP ? spValues : mpValues) == 0);
    assert(cl.cgameSensitivity == 0.625f);
    assert(cl.cgameUserCmdPrimary == 0 && cl.cgameUserCmdFov == 0);
    usercmd_t finished{};
    finished.rtcw.wbuttons = 0x91; finished.rtcw.wolfkick = -3;
    cl.serverTime = INT_MIN; cl.viewangles[0] = 90; cl.viewangles[1] = -180;
    fntech3::jka::command::selections.Set(255,257);
    CL_FinishMove(&finished);
    assert(finished.raven.forcesel == 0 && finished.raven.invensel == 0);
    assert(finished.serverTime == INT_MIN && finished.weapon == 255);
    assert(finished.angles[0] == 16384 && finished.angles[1] == 32768);
    assert(finished.weaponPrimary == 0 && finished.fov == 0 && finished.rtcw.holdable == 255);
    assert(finished.rtcw.wbuttons == 0x91 && finished.rtcw.wolfkick == -3);
    assert(finished.rtcw.cld == (id == COM_GAME_RTCW_SP ? 0x2345 : 0));
    assert(finished.rtcw.mpSetup == (id == COM_GAME_RTCW_MP ? 0x45 : 0));
    assert(finished.rtcw.identClient == (id == COM_GAME_RTCW_MP ? 0x81 : 0));
    // The command produced here crosses the same history/module boundary.
    cl.cmds[0] = finished;
    assert(CL_CgameSystemCalls(user) == qtrue);
    assert(command.value.common.holdable == 255 && command.value.common.wbuttons == 0x91);
    const auto *tail = reinterpret_cast<const byte *>(&command.value);
    assert(tail[24] == 0x45 && tail[25] == (id == COM_GAME_RTCW_SP ? 0x23 : 0x81));
    fntech3_rtcw_glconfig_t glconfig{};
    intptr_t gl[] = {Ordinal(id, Call::CG_GETGLCONFIG), (intptr_t)&glconfig};
    const int previousPods = podCalls;
    assert(CL_CgameSystemCalls(gl) == 0 && lastBounds == 7268);
    fntech3_rtcw_pc_token_t token{};
    intptr_t parsed[] = {Ordinal(id, Call::CG_PC_READ_TOKEN), 19, (intptr_t)&token};
    assert(CL_CgameSystemCalls(parsed) == 1 && lastBounds == 1040);
    fntech3_rtcw_fontInfo_t font{};
    intptr_t fontArgs[] = {Ordinal(id, Call::CG_R_REGISTERFONT), (intptr_t)"font", 27, (intptr_t)&font};
    assert(CL_CgameSystemCalls(fontArgs) == 0 && lastBounds == 20548 && podCalls == previousPods + 3);
    char filename[128]; int line = 0;
    intptr_t location[] = {Ordinal(id, Call::CG_PC_SOURCE_FILE_AND_LINE), 19, (intptr_t)filename, (intptr_t)&line};
    assert(CL_CgameSystemCalls(location) == 1 && previousBounds == 128 && lastBounds == 4 && line == 37);
    fntech3_rtcw_qtime_t calendar;
    intptr_t timeArgs[] = {Ordinal(id, Call::CG_REAL_TIME), (intptr_t)&calendar};
    assert(CL_CgameSystemCalls(timeArgs) == 12345678 && lastBounds == 36);
    timeArgs[1] = 0;
    assert(CL_CgameSystemCalls(timeArgs) == 12345678 && podCalls == previousPods + 4);
}
int main() {
    profile.id = COM_GAME_RTCW_MP;
    {
        using fntech3::rtcw::module::Call;
        const auto ordinal = fntech3::rtcw::module::Ordinal(profile.id, Call::CG_SETCLIENTLERPORIGIN);
        intptr_t values[] = {ordinal, 0x3fc00000, static_cast<int32_t>(0xc0200000u), static_cast<int32_t>(0x80000000u)};
        assert(CL_CgameSystemCalls(values) == 0);
        intptr_t limbo[] = {fntech3::rtcw::module::Ordinal(profile.id, Call::CG_LIMBOCHAT), 123};
        assert(CL_CgameSystemCalls(limbo) == 0);
        const uint32_t expected[] = {0x3fc00000,0xc0200000u,0x80000000u};
        assert(!std::memcmp(cl.cgameRTCWLerpOrigin, expected, sizeof(expected)));
        for (int i=1;i<=3;++i) for (uint32_t invalid : {0x7f800000u,0xff800000u,0x7fc00000u,0x7fa00001u}) {
            const auto saved=values[i];values[i]=static_cast<int32_t>(invalid);
            bool failed=false;try {CL_CgameSystemCalls(values);}catch(int){failed=true;}
            assert(failed&&!std::memcmp(cl.cgameRTCWLerpOrigin,expected,sizeof(expected)));
            values[i]=saved;
        }
    }
    for (auto id : {COM_GAME_RTCW_SP, COM_GAME_RTCW_MP}) {
        profile.id = id;
        // Exactly two arguments: no trailing words may be read by this import.
        intptr_t args[] = {fntech3::rtcw::module::Ordinal(id,
            fntech3::rtcw::module::Call::CG_SENDMOVESPEEDSTOGAME), INT_MIN, 123};
        const int before = moveSpeedCalls;
        assert(CL_CgameSystemCalls(args) == 0 && moveSpeedCalls == before + 1);
        if (id == COM_GAME_RTCW_SP) {
            // Exactly three words; pointer validation and ownership execute
            // separately in rtcw_model_info_tests through the actual VM bridge.
            intptr_t model[] = {125, 37, 123, 456};
            const int requests = modelInfoCalls;
            assert(CL_CgameSystemCalls(model) == -97531 && modelInfoCalls == requests + 1);
        }
    }
    Mode<fntech3_rtcw_sp_snapshot_t, fntech3_rtcw_sp_usercmd_t>(COM_GAME_RTCW_SP);
    Mode<fntech3_rtcw_mp_snapshot_t, fntech3_rtcw_mp_usercmd_t>(COM_GAME_RTCW_MP);
    for (auto id : {COM_GAME_QUAKE3, COM_GAME_QUAKELIVE}) {
        profile.id = id;
        intptr_t unchanged[] = {42};
        assert(CL_CgameSystemCalls(unchanged) == 10042);
        usercmd_t cmd{};
        cl.cgameUserCmdValue = 17; cl.cgameUserCmdPrimary = 19; cl.cgameUserCmdFov = 110;
        cl.cgameRTCWCommand.holdable = 222;
        fntech3::jka::command::selections.Set(255,257);
        CL_FinishMove(&cmd);
        assert(cmd.raven.forcesel == 0 && cmd.raven.invensel == 0);
        assert(cmd.weapon == 17 && cmd.weaponPrimary == 19 && cmd.fov == 110);
        assert(cmd.rtcw.holdable == 0 && cmd.rtcw.mpSetup == 0 && cmd.rtcw.cld == 0);
    }
}
'''


class RTCWClientStateTests(unittest.TestCase):
    def test_snapshot_usercmd_gamestate_and_native_dispatch(self):
        compiler = shutil.which("clang++") or shutil.which("g++")
        if not compiler:
            self.skipTest("C++17 compiler unavailable")
        source = (ROOT / "code/client/cl_cgame.cpp").read_text(encoding="utf-8")
        bodies = "\n".join(function(source, signature) for signature in (
            "static qboolean CL_GetUserCmd(",
            "template<typename ExternalCommand>\nstatic qboolean CL_GetRTCWUserCmd(",
            "static clSnapshot_t *CL_FindSnapshot(",
            "template<typename ExternalSnapshot>\nstatic qboolean CL_GetRTCWSnapshot(",
            "static intptr_t CL_RTCWCgameSharedCall(",
            "static void CL_RTCWCMTrace(",
            "static void CL_SetRTCWUserCmdValue(",
        ))
        input_source = (ROOT / "code/client/cl_input.cpp").read_text(encoding="utf-8")
        bodies += "\n" + function(input_source, "static void CL_FinishMove(")
        dispatch = function(source, "static intptr_t CL_CgameSystemCalls(")
        dispatch = dispatch[:dispatch.index("\tswitch( operation )")] + "\treturn 10000 + operation;\n}\n"
        (ROOT / ".tmp").mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(prefix="rtcw-client-state-", dir=ROOT / ".tmp") as temp:
            folder = Path(temp)
            fixture = folder / "probe.cpp"
            executable = folder / ("probe.exe" if os.name == "nt" else "probe")
            fixture.write_text(PRELUDE + rtcw_collision_fixture.SERVICES +
                rtcw_key_fixture.source(include_copy=True) + bodies + dispatch +
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
