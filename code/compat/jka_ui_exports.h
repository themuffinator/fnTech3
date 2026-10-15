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
	case UI_SET_MODULE_MENU: return FNTECH3_JKA_UI_SET_ACTIVE_MENU;
	default: return -1;
	}
}

static inline int FNTech3_JKAUIExportArgumentCount(int operation) {
	switch (operation) {
	case UI_GETAPIVERSION: case UI_SHUTDOWN: case UI_IS_FULLSCREEN:
	case UI_HASUNIQUECDKEY: return 0;
	case UI_INIT: case UI_REFRESH: case UI_SET_ACTIVE_MENU:
	case UI_CONSOLE_COMMAND: case UI_DRAW_CONNECT_SCREEN:
	case UI_SET_MODULE_MENU: return 1;
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
/* The engine's own menu requests name a menu in its vocabulary and are
 * translated above. A client game's request already holds a number in the UI
 * module's vocabulary, so it goes as UI_SET_MODULE_MENU: export 7 with the
 * number untranslated and unbounded. Retail jamp.exe 1.01 serves cgame import
 * 238 at 0x413032 as VM_Call(uivm, 7, args[1]) and returns zero. uix86.dll
 * 1.01's _UI_SetActiveMenu (0x40010de0) opens menus 0 to 11 -- Raven's none,
 * main, ingame, player, team, end of game, force powers, siege message, siege
 * objectives, voice chat, close-all and siege class -- and ignores any other
 * number (unsigned bound 0x40010dfa), as it ignores every number while no
 * menu is loaded (0x40010de6). */
#endif
