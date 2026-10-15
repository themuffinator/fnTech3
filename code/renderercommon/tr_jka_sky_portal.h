/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_TR_JKA_SKY_PORTAL_H
#define FNTECH3_TR_JKA_SKY_PORTAL_H

/* Jedi Academy's sky portal, as retail jamp.exe 1.01's renderer serves it.
 *
 * The client game draws a level's sky portal first in each frame, as a scene
 * of its own from the portal's camera (Raven's rdflags 0x8 with 0x10), then
 * the view it is seen from. RE_RenderScene sets a level-wide flag the first
 * time a portal's scene arrives (0x4abe18) and RE_LoadWorldMap clears it
 * (0x490fc3). While it is set, every view but a portal's own draws no sky:
 * RB_EndSurface drops its sky surfaces (0x4af5c9..0x4af5e7) and
 * RB_StageIteratorSky returns at once (0x4b96ad), so the portal's picture,
 * already in the frame, shows wherever the sky would have been drawn. Such a
 * view does not clear the colour first either (RB_BeginDrawingView 0x48a70d),
 * and an Academy world view in Fn's renderers never does. The portal's own
 * scene draws no weather (RB_RenderWorldEffects 0x488ead), which Fn does not
 * draw for this profile at all.
 *
 * Fn's renderers draw a sky only through RB_StageIteratorSky, gated by the
 * scene's own draw-sky word (tr.refdef.rtcwFog.drawSky), so this answers that
 * word for an Academy scene: qtrue for a portal's own scene, and for any other
 * once a portal has been seen in this level, qfalse. The flag lives in `tr`,
 * which R_Init clears before every world load (the client restarts its
 * renderer for each level, CL_ShutdownAll), standing for retail's clear. */
static ID_INLINE qboolean FnJKASkyPortalDrawsSky( qboolean *portalSeen, const refdef_t *view )
{
	if ( !portalSeen || !view ) {
		return qtrue;
	}
	if ( view->jka.skyboxPortal ) {
		*portalSeen = qtrue;
		return qtrue;
	}
	return *portalSeen ? qfalse : qtrue;
}

#endif
