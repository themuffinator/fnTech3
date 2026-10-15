/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_JKA_UI_EXPORTS_H
#define FNTECH3_JKA_UI_EXPORTS_H

#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"
#include "../ui/ui_public.h"
#include "jka_module_abi.h"

/* C host boundary for the legacy vmMain UI. These operation and argument
 * contracts are frozen from OpenJK 1a6a643427aa347553e9073dac5570b33337c4d9,
 * codemp/ui/ui_public.h and ui_main.c. Ordinal 11 is Raven's MENU_RESET,
 * not the canonical engine's REFRESH_DISPLAY_CONTEXT. Retail uix86.dll
 * SHA256 1179892f84f56805457455e0e01dbaab83f524180f2440b0f8157484eb711429
 * vmMain(0x400178f0) confirms exports 0..11 and their argument counts;
 * 0 returns API 7 and 10 returns true (the maintained source omits 10). */
static inline int FNTech3_JKAUIExportOrdinal(int operation) {
	switch (operation) {
#define FNTECH3_JKA_UI_EXPORT(name) case name: return FNTECH3_JKA_##name;
	FNTECH3_JKA_UI_EXPORT(UI_GETAPIVERSION)
	FNTECH3_JKA_UI_EXPORT(UI_INIT)
	FNTECH3_JKA_UI_EXPORT(UI_SHUTDOWN)
	FNTECH3_JKA_UI_EXPORT(UI_KEY_EVENT)
	FNTECH3_JKA_UI_EXPORT(UI_MOUSE_EVENT)
	FNTECH3_JKA_UI_EXPORT(UI_REFRESH)
	FNTECH3_JKA_UI_EXPORT(UI_IS_FULLSCREEN)
	FNTECH3_JKA_UI_EXPORT(UI_SET_ACTIVE_MENU)
	FNTECH3_JKA_UI_EXPORT(UI_CONSOLE_COMMAND)
	FNTECH3_JKA_UI_EXPORT(UI_DRAW_CONNECT_SCREEN)
	FNTECH3_JKA_UI_EXPORT(UI_HASUNIQUECDKEY)
#undef FNTECH3_JKA_UI_EXPORT
	default: return -1;
	}
}

static inline int FNTech3_JKAUIExportArgumentCount(int operation) {
	switch (operation) {
	case UI_GETAPIVERSION: case UI_SHUTDOWN: case UI_IS_FULLSCREEN:
	case UI_HASUNIQUECDKEY: return 0;
	case UI_INIT: case UI_REFRESH: case UI_SET_ACTIVE_MENU:
	case UI_CONSOLE_COMMAND: case UI_DRAW_CONNECT_SCREEN: return 1;
	case UI_KEY_EVENT: case UI_MOUSE_EVENT: return 2;
	default: return -1;
	}
}

/* How many of a VM_Call's arguments reach the module, or -1 to refuse the
 * call. The engine hands a key event as (key, down, time); retail
 * CL_KeyEvent 0x41c290 and CL_CharEvent 0x41c640 call vmMain(3, key, down),
 * so the time stays behind. */
static inline int FNTech3_JKAUIForwardedArguments(int operation, int nargs) {
	const int count = FNTech3_JKAUIExportArgumentCount(operation);
	if (count < 0) return -1;
	if (operation == UI_KEY_EVENT && nargs == 3) return count;
	return nargs == count ? count : -1;
}

static inline int FNTech3_JKAUIMenuToExternal(int menu) {
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
/* A cgame request already carries an Academy menu selector. Adapt it into
 * the existing engine UI dispatcher, which applies its own export whitelist,
 * argument counts and nested-call lifetime tracking. */
static inline int FNTech3_JKAUIMenuToInternal(int menu) {
    switch (menu) {
    case 0: return UIMENU_NONE;
    case 1: return UIMENU_MAIN;
    case 2: return UIMENU_INGAME;
    case 3: return UIMENU_JKA_PLAYERCONFIG;
    case 4: return UIMENU_TEAM;
    case 5: return UIMENU_POSTGAME;
    default: return -1;
    }
}
#endif
