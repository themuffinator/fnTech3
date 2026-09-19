/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_STVEF_KEY_ADAPTER_H
#define FNTECH3_STVEF_KEY_ADAPTER_H

#include "rtcw_key_adapter.h"

/* Elite Force keeps original Quake III's key numbers, as RTCW does: the same
 * named keys at the same numbers up to K_AUX16 (232), and K_CHAR_FLAG 1024
 * (the key name tables in stvoy.exe and stvoyHM.exe list every one). This
 * engine numbers four more mouse buttons before the wheel, so from
 * K_MWHEELDOWN upward its numbers sit four above Elite Force's. A key Elite
 * Force has no number for is never delivered, and a module number outside
 * its vocabulary never becomes an engine binding. */
static inline int FnTech3_STVEFKeyToInternal(int key) {
	return FnTech3_RTCWKeyToInternal(key);
}

static inline int FnTech3_STVEFKeyEventFromInternal(int key) {
	return FnTech3_RTCWKeyEventFromInternal(key);
}

#endif
