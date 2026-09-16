/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_RTCW_KEY_ADAPTER_H
#define FNTECH3_RTCW_KEY_ADAPTER_H

#include "rtcw_game_abi.h"
#include "../client/keycodes.h"

/* Physical key numbers and character events are separate contracts. Values
 * outside the original key vocabulary never become another engine binding. */
static inline int FnTech3_RTCWKeyToInternal(int key) {
	if (key >= 0 && key < 128) return key;
	switch (key) {
#define FNTECH3_RTCW_KEY(name, value) case FNTECH3_RTCW_##name: return name;
#include "rtcw_keycodes.def"
#undef FNTECH3_RTCW_KEY
	default: return -1;
	}
}

static inline int FnTech3_RTCWKeyFromInternal(int key) {
	if (key >= 0 && key < 128) return key;
	switch (key) {
#define FNTECH3_RTCW_KEY(name, value) case name: return FNTECH3_RTCW_##name;
#include "rtcw_keycodes.def"
#undef FNTECH3_RTCW_KEY
	default: return -1;
	}
}

static inline int FnTech3_RTCWKeyEventFromInternal(int key) {
	if ((key & K_CHAR_FLAG) && !(key & ~(K_CHAR_FLAG | 255)))
		return FNTECH3_RTCW_K_CHAR_FLAG | (key & 255);
	return FnTech3_RTCWKeyFromInternal(key);
}

#endif
