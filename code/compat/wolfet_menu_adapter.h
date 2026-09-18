/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_WOLFET_MENU_ADAPTER_H
#define FNTECH3_WOLFET_MENU_ADAPTER_H

/* Enemy Territory numbers its menus in its own order, with five the other
 * Wolfenstein profile does not have. The caller includes ui_public.h first.
 * Both directions return -1 for a menu the other side has no number for, so
 * nothing is mapped onto a neighbour. */
static inline int FnTech3_WolfETMenuFromInternal( int menu ) {
	switch ( menu ) {
#define FNTECH3_WOLFET_MENU(name, value) case UIMENU_##name: return value;
#include "wolfet_menus.def"
#undef FNTECH3_WOLFET_MENU
	default: return -1;
	}
}

static inline int FnTech3_WolfETMenuToInternal( long long menu ) {
	switch ( menu ) {
#define FNTECH3_WOLFET_MENU(name, value) case value: return UIMENU_##name;
#include "wolfet_menus.def"
#undef FNTECH3_WOLFET_MENU
	default: return -1;
	}
}

#endif
