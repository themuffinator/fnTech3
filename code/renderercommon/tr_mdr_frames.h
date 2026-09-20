/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_TR_MDR_FRAMES_H
#define FNTECH3_TR_MDR_FRAMES_H

/* stvoy.exe 1.20 R_AddAnimSurfaces, 0x461410: cap takes precedence over
 * wrapping. A negative or otherwise invalid frame resets both frames. */
static inline int FnMdr_SelectFrames( int count, int cap, int wrap, int *frame, int *oldframe ) {
	if ( count > 0 ) {
		if ( cap ) {
			if ( *frame >= count ) *frame = count - 1;
			if ( *oldframe >= count ) *oldframe = count - 1;
		} else if ( wrap ) {
			*frame %= count;
			*oldframe %= count;
		}
		if ( *frame >= 0 && *frame < count && *oldframe >= 0 && *oldframe < count ) return 1;
	}
	*frame = *oldframe = 0;
	return 0;
}
#endif
