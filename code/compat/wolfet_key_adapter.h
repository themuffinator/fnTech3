/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_WOLFET_KEY_ADAPTER_H
#define FNTECH3_WOLFET_KEY_ADAPTER_H

#include "wolfet_game_abi.h"
#include "../client/keycodes.h"

enum {
#define FNTECH3_WOLFET_KEY(name, value) FNTECH3_WOLFET_##name = value,
#include "wolfet_keycodes.def"
#undef FNTECH3_WOLFET_KEY
	FNTECH3_WOLFET_K_CHAR_FLAG = 1024
};

/* Physical key numbers and character events are separate contracts. ET's key
 * vocabulary ends at K_AUX16, four short of this engine's, so every number
 * above it is refused rather than becoming an unrelated engine binding. */
static inline int FnTech3_WolfETKeyToInternal(int key) {
	if (key >= 0 && key < 128) return key;
	switch (key) {
#define FNTECH3_WOLFET_KEY(name, value) case FNTECH3_WOLFET_##name: return name;
#include "wolfet_keycodes.def"
#undef FNTECH3_WOLFET_KEY
	default: return -1;
	}
}

static inline int FnTech3_WolfETKeyFromInternal(int key) {
	if (key >= 0 && key < 128) return key;
	switch (key) {
#define FNTECH3_WOLFET_KEY(name, value) case name: return FNTECH3_WOLFET_##name;
#include "wolfet_keycodes.def"
#undef FNTECH3_WOLFET_KEY
	default: return -1;
	}
}

static inline int FnTech3_WolfETKeyEventFromInternal(int key) {
	if ((key & K_CHAR_FLAG) && !(key & ~(K_CHAR_FLAG | 255)))
		return FNTECH3_WOLFET_K_CHAR_FLAG | (key & 255);
	return FnTech3_WolfETKeyFromInternal(key);
}

#endif
