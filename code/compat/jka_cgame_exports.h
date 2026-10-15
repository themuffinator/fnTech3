/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_JKA_CGAME_EXPORTS_H
#define FNTECH3_JKA_CGAME_EXPORTS_H
#include "jka_module_abi.h"

/* Include after canonical cg_public.h, which has no include guard. Both C VM
 * routing and C++ clients use this whitelist. Retail 1.01 assets3.pk3's
 * cgamex86.dll SHA256 e5d5525829bfcaf3c0428d87fe804e5a46a75dcf0a141f8288061a3970ff140d:
 * vmMain VA 0x300425a0 branches through 0x30042854. Exports 0..8 consume
 * 3,0,0,3,0,0,2,2,1 arguments; 0 is CG_Init(serverMessage,commandSequence,
 * clientNum). Export 27 (0x3004277d) takes no arguments and sets map-change.
 * Export 31 (0x3004281c) takes no arguments and reads a camera-shake record
 * from the shared buffer.
 * Modern canonical chat/HUD exports 9 onward are Raven callbacks with
 * unrelated records and must never pass through by their numeric ordinal.
 * OpenJK 1a6a643427aa347553e9073dac5570b33337c4d9 supplies matching names.
 */

static inline int FNTech3_JKACgameExportOrdinal(int operation) {
    switch (operation) {
#define JKA_CGAME_EXPORT(name) case name: return FNTECH3_JKA_##name;
    JKA_CGAME_EXPORT(CG_INIT)
    JKA_CGAME_EXPORT(CG_SHUTDOWN)
    JKA_CGAME_EXPORT(CG_CONSOLE_COMMAND)
    JKA_CGAME_EXPORT(CG_DRAW_ACTIVE_FRAME)
    JKA_CGAME_EXPORT(CG_CROSSHAIR_PLAYER)
    JKA_CGAME_EXPORT(CG_LAST_ATTACKER)
    JKA_CGAME_EXPORT(CG_KEY_EVENT)
    JKA_CGAME_EXPORT(CG_MOUSE_EVENT)
    JKA_CGAME_EXPORT(CG_EVENT_HANDLING)
#undef JKA_CGAME_EXPORT
    case CG_RAVEN_MAP_CHANGE: return FNTECH3_JKA_CG_MAP_CHANGE;
    case CG_RAVEN_TRACE: return FNTECH3_JKA_CG_TRACE;
    case CG_RAVEN_CAMERA_SHAKE: return FNTECH3_JKA_CG_FX_CAMERASHAKE;
    default: return -1;
    }
}
static inline int FNTech3_JKACgameExportArgumentCount(int operation) {
    switch (operation) {
    case CG_INIT: case CG_DRAW_ACTIVE_FRAME: return 3;
    case CG_SHUTDOWN: case CG_CONSOLE_COMMAND: case CG_CROSSHAIR_PLAYER:
    case CG_LAST_ATTACKER: case CG_RAVEN_MAP_CHANGE: case CG_RAVEN_TRACE:
    case CG_RAVEN_CAMERA_SHAKE: return 0;
    case CG_KEY_EVENT: case CG_MOUSE_EVENT: return 2;
    case CG_EVENT_HANDLING: return 1;
    default: return -1;
    }
}
static inline int FNTech3_JKACgameEventToExternal(int event) {
    switch (event) {
    case CGAME_EVENT_NONE: return 0;
    case CGAME_EVENT_TEAMMENU: return 1;
    case CGAME_EVENT_SCOREBOARD: return 2;
    case CGAME_EVENT_EDITHUD: return 3;
    default: return -1;
    }
}
#endif
