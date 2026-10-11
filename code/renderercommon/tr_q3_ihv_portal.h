/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_TR_Q3_IHV_PORTAL_H
#define FNTECH3_TR_Q3_IHV_PORTAL_H

#include <math.h>

/* The 4.01 IHV renderer's portal views, read from its mirror/portal pass
 * (0x42c160) and portal orientation (0x42bde0). Its portal keyword only marks
 * the shader (0x43a78a) and it has no alphaGen portal, so no range exists:
 * a portal is drawn at any distance, where Q3 1.32 skips a portal farther away
 * than its alphaGen range, and a shader without one is never seen through.
 * The pass stops only inside another portal, for r_noportals, and when no
 * portal entity lies within 64 units of the surface's plane. */
static inline int Q3IHV_PortalRanged( int gameId ) {
	return gameId != COM_GAME_QUAKE3_IHV;
}

/* The camera's roll in degrees about its forward axis (0x42c0b6..0x42c12c).
 * A nonzero frame (native record +52) turns it continuously by time * 0.001 *
 * frame; otherwise it bobs by sin(time * 0.003) * 4 plus skinNum (+76). Q3
 * 1.32 rolls only when oldframe is set, or by skinNum alone; IHV always rolls.
 * The original keeps x87 precision until the single float store. */
static inline float Q3IHV_PortalRoll( int time, int frame, int skinNum ) {
	if ( frame ) {
		volatile float secondsPerMillisecond = 0.001f;
		return (float)( (double)time * secondsPerMillisecond * (double)frame );
	}
	return (float)( sin( (double)time * 0.003 ) * (double)4.0f + (double)skinNum );
}

#endif
