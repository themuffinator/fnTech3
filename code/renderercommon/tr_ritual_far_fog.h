/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_TR_RITUAL_FAR_FOG_H
#define FNTECH3_TR_RITUAL_FAR_FOG_H

/* Heavy Metal: F.A.K.K.2's and American McGee's Alice's far-plane fog, as
 * their engines draw it over an opaque surface. Retail draws the surface's
 * stages unfogged, then one pass of the fog colour, blended GL_SRC_ALPHA
 * GL_ONE_MINUS_SRC_ALPHA, its alpha min( |v - eye| / distance, 1 ) at each
 * vertex (fakk2.exe 1.02 0x004948a0, factors 0x00494910; alice.exe
 * 0x0049ebb0). FinishShader appends that pass to a copy of the stages
 * (0x004792bc..0x0047937b) and RB_BeginSurface draws the copy (0x0047c5ee;
 * alice.exe 0x0049286e), so a lightmap and the textures over it fog once, as
 * one colour. Only a single-pass `rgbGen lightingDiffuse` skin is fogged by
 * GL fog instead (0x004795c0..0x004795d7, with `r_useglfog`; Alice's flag
 * also follows its developer `r_toggle`, off by default, 0x0048ee7c): the
 * same colour, so here it takes the pass too.
 *
 * A shader whose first stage is blended keeps this engine's per-stage fog
 * (tr_rtcw_fog_blend.h), which fades an additive stage to nothing, a filter
 * to white and a blend toward the fog. Retail fades the blends it can
 * (0x00478c3c..0x00478df6 marks them, 0x00479494..0x004794af drops the pass)
 * and draws the pass over the rest, so its distant filter decals show the fog
 * colour across their whole polygon. The pass keeps to the surface's own
 * pixels by depth equality, as Quake III's fog pass does, where retail takes
 * the last stage's depth test and fogs the cut-out parts of a lone
 * alpha-tested stage too. */

/* Whether a surface whose first stage has these state bits takes the pass. */
static inline qboolean FnRitualFarFogShaderTakesPass( qboolean sky, qboolean firstActive, unsigned firstStateBits ) {
	if ( sky || !firstActive ) {
		return qfalse;
	}
	return ( firstStateBits & GLS_BLEND_BITS ) ? qfalse : qtrue;
}

/* The pass's alpha at a vertex this far from the eye, truncated as retail's
 * ftol does. */
static inline byte FnRitualFarFogAlpha( float distance, float farDistance ) {
	float factor;
	if ( !( farDistance > 0.0f ) || !( distance > 0.0f ) ) {
		return 0;
	}
	factor = distance / farDistance;
	return factor >= 1.0f ? 255 : (byte)( factor * 255.0f );
}

/* The pass's state: blended over the stages, on the surface's own depth. */
static inline unsigned FnRitualFarFogState( unsigned firstStateBits ) {
	return GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA |
		( ( firstStateBits & GLS_DEPTHTEST_DISABLE ) ? GLS_DEPTHTEST_DISABLE : GLS_DEPTHFUNC_EQUAL );
}
#endif
