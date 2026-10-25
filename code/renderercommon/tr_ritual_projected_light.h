/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_TR_RITUAL_PROJECTED_LIGHT_H
#define FNTECH3_TR_RITUAL_PROJECTED_LIGHT_H

/* Alice's and F.A.K.K.2's projected dynamic light, the pass r_dlightMode 0
 * draws. Quake III maps every vertex into the light's image by world x and
 * y; retail maps it across the plane of its normal's dominant axis, so a
 * wall beside a light takes it: y and z for a normal mostly along x, else x
 * and z for one mostly along y, else x and y. The image spans the radius over
 * 0.75 about its middle. The light fades with the vertex's distance along its
 * normal, (radius - |dist . normal|) / radius, to nothing past the radius on
 * either side (fakk2.exe 1.02 0x0047d0a1..0x0047d268, alice.exe
 * 0x004932b3..0x0049348b). dist is the light's origin less the vertex. It
 * returns the vertex's clip bits, 63 for a face turned away when the caller
 * culls back faces, and its fade in modulate. */
static ID_INLINE int RB_RitualProjectDlightVertex( const vec3_t dist, const vec3_t normal, float radius,
	qboolean cullBackFaces, float st[2], float *modulate ) {
	const float scale = 0.75f / radius;
	const float ax = fabsf( normal[0] ), ay = fabsf( normal[1] ), az = fabsf( normal[2] );
	const float d = DotProduct( dist, normal );
	int clip = 0;

	if ( ax > ay && ax > az ) {
		st[0] = 0.5f + dist[1] * scale;
		st[1] = 0.5f + dist[2] * scale;
	} else if ( ay > az ) {
		st[0] = 0.5f + dist[0] * scale;
		st[1] = 0.5f + dist[2] * scale;
	} else {
		st[0] = 0.5f + dist[0] * scale;
		st[1] = 0.5f + dist[1] * scale;
	}
	*modulate = 0.0f;
	if ( cullBackFaces && d < 0.0f ) {
		return 63;
	}
	if ( st[0] < 0.0f ) {
		clip |= 1;
	} else if ( st[0] > 1.0f ) {
		clip |= 2;
	}
	if ( st[1] < 0.0f ) {
		clip |= 4;
	} else if ( st[1] > 1.0f ) {
		clip |= 8;
	}
	if ( d > radius ) {
		clip |= 16;
	} else if ( d < -radius ) {
		clip |= 32;
	} else {
		*modulate = ( radius - fabsf( d ) ) / radius;
	}
	return clip;
}

#endif
