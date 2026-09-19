/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_Q3_KEY_ADAPTER_H
#define FNTECH3_Q3_KEY_ADAPTER_H

#include "rtcw_key_adapter.h"

/* Quake III's UI and cgame modules take original Quake III's key numbers:
 * quake3.exe 1.32's key name table names K_MWHEELDOWN 183, K_MWHEELUP 184,
 * K_JOY1-K_JOY32 185-216 and K_AUX1-K_AUX16 217-232, and K_CHAR_FLAG is 1024
 * (code/ui/keycodes.h of the GPL release). RTCW and Elite Force kept every one
 * of those numbers, so RTCW's table (rtcw_keycodes.def) serves all three. This
 * engine numbers mouse buttons 6-9 before the wheel, as retail Quake Live does,
 * so from K_MWHEELDOWN upward its numbers sit four above Quake III's; Quake
 * Live's modules take this engine's numbers unchanged. A key Quake III has no
 * number for is never delivered, and a module number outside its vocabulary
 * never becomes an engine binding. */
enum { FNTECH3_Q3_K_AUX16 = FNTECH3_RTCW_K_AUX16 };

static inline int FnTech3_Q3KeyToInternal(int key) {
	return FnTech3_RTCWKeyToInternal(key);
}

static inline int FnTech3_Q3KeyFromInternal(int key) {
	return FnTech3_RTCWKeyFromInternal(key);
}

static inline int FnTech3_Q3KeyEventFromInternal(int key) {
	return FnTech3_RTCWKeyEventFromInternal(key);
}

#endif
