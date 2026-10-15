/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_CLIENT_JKA_UI_HPP
#define FNTECH3_CLIENT_JKA_UI_HPP

#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"
#include "../ui/ui_public.h"
#include "../compat/jka_module_abi.h"
#include "../compat/jka_game_abi.h"
#include "../compat/jka_ui_abi.h"
#include "client_jka_records.hpp"
#include <array>
#include <cstring>

/* Legacy JKA UI numbers are translated individually, never handed to the
 * canonical dispatcher as though a shared name guaranteed a shared number.
 * This slice serves the common scalar/string services and freezes output
 * records for explicit host adapters. Static renderer records, font and
 * localization use their profile hosts. Key and Ghoul2 imports require their
 * own boundaries and remain recognizable refusals until supplied. No retail-exact rendering
 * option disables FnQ3/FnQL's rendering, audio or UI features here. */
namespace fntech3::jka::ui {
using module::Call;

inline constexpr bool VersionSupported(int version, bool structured) noexcept {
    return !structured && version == FNTECH3_JKA_MP_UI_LEGACY_API_VERSION;
}

inline constexpr intptr_t SharedImport(Call call) noexcept {
    switch (call) {
#define SHARED(name) case Call::name: return name;
    SHARED(UI_ERROR) SHARED(UI_PRINT) SHARED(UI_MILLISECONDS)
    SHARED(UI_CVAR_SET) SHARED(UI_CVAR_VARIABLEVALUE)
    SHARED(UI_CVAR_VARIABLESTRINGBUFFER) SHARED(UI_CVAR_SETVALUE) SHARED(UI_CVAR_RESET)
    SHARED(UI_ARGC) SHARED(UI_ARGV) SHARED(UI_CMD_EXECUTETEXT)
    SHARED(UI_FS_FOPENFILE) SHARED(UI_FS_READ) SHARED(UI_FS_WRITE)
    SHARED(UI_FS_FCLOSEFILE) SHARED(UI_FS_GETFILELIST)
    SHARED(UI_R_REGISTERMODEL) SHARED(UI_R_REGISTERSKIN) SHARED(UI_R_REGISTERSHADERNOMIP)
    SHARED(UI_R_CLEARSCENE) SHARED(UI_R_SETCOLOR) SHARED(UI_R_DRAWSTRETCHPIC)
    SHARED(UI_R_ADDREFENTITYTOSCENE) SHARED(UI_R_RENDERSCENE)
    SHARED(UI_R_MODELBOUNDS) SHARED(UI_UPDATESCREEN)
    SHARED(UI_KEY_GETOVERSTRIKEMODE) SHARED(UI_KEY_SETOVERSTRIKEMODE)
    SHARED(UI_KEY_CLEARSTATES) SHARED(UI_KEY_GETCATCHER)
    SHARED(UI_GETCLIPBOARDDATA) SHARED(UI_GETCONFIGSTRING) SHARED(UI_MEMORY_REMAINING)
    SHARED(UI_LAN_GETPINGQUEUECOUNT) SHARED(UI_LAN_CLEARPING)
    SHARED(UI_LAN_GETPING) SHARED(UI_LAN_GETPINGINFO)
    SHARED(UI_LAN_GETSERVERCOUNT) SHARED(UI_LAN_GETSERVERADDRESSSTRING)
    SHARED(UI_LAN_GETSERVERINFO) SHARED(UI_LAN_MARKSERVERVISIBLE)
    SHARED(UI_LAN_UPDATEVISIBLEPINGS) SHARED(UI_LAN_RESETPINGS)
    SHARED(UI_LAN_LOADCACHEDSERVERS) SHARED(UI_LAN_SAVECACHEDSERVERS)
    SHARED(UI_LAN_ADDSERVER) SHARED(UI_LAN_REMOVESERVER)
    SHARED(UI_LAN_SERVERSTATUS) SHARED(UI_LAN_GETSERVERPING)
    SHARED(UI_LAN_SERVERISVISIBLE) SHARED(UI_LAN_COMPARESERVERS)
    SHARED(UI_PC_ADD_GLOBAL_DEFINE) SHARED(UI_PC_LOAD_SOURCE) SHARED(UI_PC_FREE_SOURCE)
    SHARED(UI_S_STOPBACKGROUNDTRACK) SHARED(UI_S_STARTBACKGROUNDTRACK)
    SHARED(UI_CIN_PLAYCINEMATIC) SHARED(UI_CIN_STOPCINEMATIC) SHARED(UI_CIN_RUNCINEMATIC)
    SHARED(UI_CIN_DRAWCINEMATIC) SHARED(UI_CIN_SETEXTENTS) SHARED(UI_R_REMAP_SHADER)
    SHARED(UI_MEMSET) SHARED(UI_MEMCPY) SHARED(UI_STRNCPY)
    SHARED(UI_SIN) SHARED(UI_COS) SHARED(UI_ATAN2) SHARED(UI_SQRT)
    SHARED(UI_FLOOR) SHARED(UI_CEIL)
#undef SHARED
    default: return -1;
    }
}

inline constexpr intptr_t SharedImport(intptr_t ordinal) noexcept {
    return SharedImport(module::Lookup(COM_GAME_JKA_MP, module::Module::UI,
        module::Direction::IMPORT, ordinal));
}

/* Source browser indices are Local=0, Global=1, Favorites=2. Engine indices
 * retain Quake III's retired MPlayer slot, so two of the three are different. */
inline constexpr int BrowserSource(int source) noexcept {
    return source == 0 ? AS_LOCAL : source == 1 ? AS_GLOBAL : source == 2 ? AS_FAVORITES : -1;
}
inline constexpr unsigned BrowserArguments(Call call) noexcept {
    switch (call) {
    case Call::UI_LAN_GETSERVERCOUNT: case Call::UI_LAN_UPDATEVISIBLEPINGS:
    case Call::UI_LAN_RESETPINGS: return 1;
    case Call::UI_LAN_GETSERVERPING: case Call::UI_LAN_SERVERISVISIBLE:
    case Call::UI_LAN_REMOVESERVER: return 2;
    case Call::UI_LAN_MARKSERVERVISIBLE: case Call::UI_LAN_ADDSERVER: return 3;
    case Call::UI_LAN_GETSERVERADDRESSSTRING: case Call::UI_LAN_GETSERVERINFO: return 4;
    case Call::UI_LAN_COMPARESERVERS: return 5;
    default: return 0;
    }
}
inline bool BrowserArguments(Call call, const intptr_t *source, std::array<intptr_t, 6> &result) noexcept {
    const auto count = BrowserArguments(call);
    if (!source || !count) return false;
    for (unsigned n = 0; n <= count; ++n) result[n] = source[n];
    result[1] = source[1] >= 0 && source[1] <= 2 ? BrowserSource(static_cast<int>(source[1])) : -1;
    return true;
}

inline constexpr int MenuToExternal(int menu) noexcept {
    switch (menu) {
    case UIMENU_NONE: return 0;
    case UIMENU_MAIN: return 1;
    case UIMENU_INGAME: return 2;
    case UIMENU_TEAM: return 4;
    case UIMENU_POSTGAME: return 5;
    case UIMENU_JKA_PLAYERCONFIG: return 3;
    default: return -1;
    }
}

inline constexpr int ExportOrdinal(int operation) noexcept {
    switch (operation) {
#define EXPORT(name) case name: return FNTECH3_JKA_##name;
    EXPORT(UI_GETAPIVERSION) EXPORT(UI_INIT) EXPORT(UI_SHUTDOWN)
    EXPORT(UI_KEY_EVENT) EXPORT(UI_MOUSE_EVENT) EXPORT(UI_REFRESH)
    EXPORT(UI_IS_FULLSCREEN) EXPORT(UI_SET_ACTIVE_MENU) EXPORT(UI_CONSOLE_COMMAND)
    EXPORT(UI_DRAW_CONNECT_SCREEN) EXPORT(UI_HASUNIQUECDKEY)
#undef EXPORT
    case UI_SET_MODULE_MENU: return FNTECH3_JKA_UI_SET_ACTIVE_MENU;
    default: return -1;
    }
}

inline constexpr int CvarFlags(std::uint32_t external) noexcept {
    // Raven's 0x800/0x1000 are INTERNAL/PARENTAL, not the engine's flags.
    // Higher modern-OpenJK flags do not establish retail VM authorization.
    return static_cast<int>(external & 0x7ffu);
}

namespace detail {
template<std::size_t N, std::size_t M>
void Text(char (&to)[N], const char (&from)[M]) noexcept {
    std::size_t n = 0;
    for (; n+1 < N && n < M && from[n]; ++n) to[n] = from[n];
    to[n] = '\0';
}
}

inline bool ClientState(const uiClientState_t &internal, fntech3_jka_mp_uiClientState_t &external) noexcept {
    static_assert(CA_UNINITIALIZED == 0 && CA_DISCONNECTED == 1 && CA_AUTHORIZING == 2 &&
        CA_CONNECTING == 3 && CA_CHALLENGING == 4 && CA_CONNECTED == 5 && CA_LOADING == 6 &&
        CA_PRIMED == 7 && CA_ACTIVE == 8 && CA_CINEMATIC == 9);
    std::uint32_t state;
    static_assert(sizeof(state) == sizeof(internal.connState));
    std::memcpy(&state, &internal.connState, 4);
    if (state > 9) return false;
    fntech3_jka_mp_uiClientState_t next{};
    next.connState = static_cast<std::int32_t>(state);
    next.connectPacketCount = internal.connectPacketCount; next.clientNum = internal.clientNum;
    detail::Text(next.servername, internal.servername);
    detail::Text(next.updateInfoString, internal.updateInfoString);
    detail::Text(next.messageString, internal.messageString);
    external = next;
    return true;
}

using records::CvarToExternal;
using records::CvarToInternal;
using records::Token;
using records::Time;
using records::TextureCompression;
using records::Glconfig;

inline constexpr const char *OpenBoundary(Call call) noexcept {
    switch (call) {
    case Call::UI_R_ADDREFENTITYTOSCENE: case Call::UI_R_RENDERSCENE:
    case Call::UI_GETGLCONFIG: return "renderer record";
    case Call::UI_R_REGISTERFONT: case Call::UI_R_FONT_STRLENPIXELS:
    case Call::UI_R_FONT_STRLENCHARS: case Call::UI_R_FONT_STRHEIGHTPIXELS:
    case Call::UI_R_FONT_DRAWSTRING: return "Raven font";
    case Call::UI_SP_GETNUMLANGUAGES: case Call::UI_SP_GETLANGUAGENAME:
    case Call::UI_SP_GETSTRINGTEXTSTRING: case Call::UI_LANGUAGE_ISASIAN:
    case Call::UI_LANGUAGE_USESSPACES: case Call::UI_ANYLANGUAGE_READCHARFROMSTRING:
        return "localization";
    default:
        return module::Describe(call) && module::Ordinal(COM_GAME_JKA_MP, call) >= 201
            ? "Ghoul2" : "menu service";
    }
}
} // namespace fntech3::jka::ui
#endif
