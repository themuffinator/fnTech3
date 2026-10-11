/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_CLIENT_JKA_CGAME_HPP
#define FNTECH3_CLIENT_JKA_CGAME_HPP
#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"
#include "../compat/jka_module_abi.h"
#include "../compat/jka_client_adapter.hpp"
#include "client_jka_records.hpp"

/* Included after canonical cg_public.h (that legacy header has no guard).
 * Imports with matching argument contracts may use canonical host services.
 * Frozen outputs are handled by the cgame host before consulting this map.
 * Raven renderer effects, Ghoul2, input overrides, ambient sound sets and
 * script callbacks require their own boundaries and are refused by name.
 * No profile path replaces FnQ3/FnQL's renderer or spatial audio features. */
namespace fntech3::jka::cgame {
using module::Call;
inline constexpr intptr_t SharedImport(Call call) noexcept {
    switch (call) {
#define SHARED(name) case Call::name: return name;
    SHARED(CG_PRINT) SHARED(CG_ERROR) SHARED(CG_MILLISECONDS)
    SHARED(CG_CVAR_SET) SHARED(CG_CVAR_VARIABLESTRINGBUFFER)
    SHARED(CG_ARGC) SHARED(CG_ARGV) SHARED(CG_ARGS)
    SHARED(CG_FS_FOPENFILE) SHARED(CG_FS_READ) SHARED(CG_FS_WRITE) SHARED(CG_FS_FCLOSEFILE)
    SHARED(CG_SENDCONSOLECOMMAND) SHARED(CG_ADDCOMMAND) SHARED(CG_REMOVECOMMAND)
    SHARED(CG_SENDCLIENTCOMMAND) SHARED(CG_UPDATESCREEN)
    SHARED(CG_CM_LOADMAP) SHARED(CG_CM_NUMINLINEMODELS) SHARED(CG_CM_INLINEMODEL)
    SHARED(CG_CM_TEMPBOXMODEL) SHARED(CG_CM_TEMPCAPSULEMODEL)
    SHARED(CG_CM_POINTCONTENTS) SHARED(CG_CM_TRANSFORMEDPOINTCONTENTS)
    SHARED(CG_CM_MARKFRAGMENTS)
    SHARED(CG_R_LOADWORLDMAP) SHARED(CG_R_REGISTERMODEL) SHARED(CG_R_REGISTERSKIN)
    SHARED(CG_R_REGISTERSHADER) SHARED(CG_R_REGISTERSHADERNOMIP)
    SHARED(CG_R_CLEARSCENE) SHARED(CG_R_SETCOLOR) SHARED(CG_R_DRAWSTRETCHPIC)
    SHARED(CG_R_ADDREFENTITYTOSCENE) SHARED(CG_R_RENDERSCENE)
    SHARED(CG_R_MODELBOUNDS) SHARED(CG_R_REMAP_SHADER)
    SHARED(CG_GETCURRENTSNAPSHOTNUMBER) SHARED(CG_GETSERVERCOMMAND) SHARED(CG_GETCURRENTCMDNUMBER)
    SHARED(CG_MEMORY_REMAINING) SHARED(CG_KEY_GETCATCHER)
    SHARED(CG_PC_ADD_GLOBAL_DEFINE) SHARED(CG_PC_LOAD_SOURCE) SHARED(CG_PC_FREE_SOURCE)
    SHARED(CG_PC_SOURCE_FILE_AND_LINE)
    SHARED(CG_S_STOPBACKGROUNDTRACK) SHARED(CG_S_ADDLOOPINGSOUND)
    SHARED(CG_S_STOPLOOPINGSOUND)
    SHARED(CG_S_UPDATEENTITYPOSITION) SHARED(CG_S_RESPATIALIZE)
    SHARED(CG_SNAPVECTOR) SHARED(CG_R_INPVS)
    SHARED(CG_CIN_PLAYCINEMATIC) SHARED(CG_CIN_STOPCINEMATIC) SHARED(CG_CIN_RUNCINEMATIC)
    SHARED(CG_CIN_DRAWCINEMATIC) SHARED(CG_CIN_SETEXTENTS)
#undef SHARED
    // Retail jamp.exe 1.01 handlers 0x412975 and 0x4129a1 both enter
    // S_AddLoopingSound (0x46b180). Academy's "real" loop is still a
    // frame submission and uses Fn's ordinary loop, including Doppler.
    case Call::CG_S_ADDREALLOOPINGSOUND: return CG_S_ADDLOOPINGSOUND;
    case Call::CGAME_MEMSET: return TRAP_MEMSET;
    case Call::CGAME_MEMCPY: return TRAP_MEMCPY;
    case Call::CGAME_STRNCPY: return TRAP_STRNCPY;
    case Call::CGAME_SIN: return TRAP_SIN;
    case Call::CGAME_COS: return TRAP_COS;
    case Call::CGAME_ATAN2: return TRAP_ATAN2;
    case Call::CGAME_SQRT: return TRAP_SQRT;
    case Call::CGAME_FLOOR: return CG_FLOOR;
    case Call::CGAME_CEIL: return CG_CEIL;
    case Call::CGAME_ACOS: return CG_ACOS;
    default: return -1;
    }
}
inline constexpr intptr_t SharedImport(intptr_t ordinal) noexcept {
    return SharedImport(module::Lookup(COM_GAME_JKA_MP, module::Module::CGAME,
        module::Direction::IMPORT, ordinal));
}
inline constexpr int CvarFlags(uint32_t external) noexcept { return int(external & 0x7ffu); }
} // namespace fntech3::jka::cgame
#endif
