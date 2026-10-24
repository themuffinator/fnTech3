/*
===========================================================================
Copyright (C) 2026 fnTech3 contributors

This file is part of fnTech3 and is distributed under the terms of the GNU
General Public License, version 2 or (at your option) any later version.

A static map spot light's cone

A spot from a map's light sidecar (maps/<map>.lights.json, loaded by each
renderer's tr_bsp.c and promoted by R_AddStaticMapLightsToScene) is drawn as
a segment from its origin along its direction for its radius, lit to its
scaled radius around that segment, which is the reach along the axis the
sidecar generators fit (scripts/generate_stock_rtcw_lights.py
drawn_falloff). Unlike a segment light it lights only inside its cone, and
from its origin. A fragment at angle theta off the axis, seen from the
origin, is lit by

	cone = smoothstep( clamp( cos( theta ) * scale + bias, 0, 1 ) )

where scale and bias come from the cosines of the inner and outer
half-angles: 1 inside innerAngle, easing to 0 at outerAngle, 0 beyond it.
The sidecar format's innerAngle defaults to 0, so a spot that gives none
fades from its axis to its outer edge. Its diffuse and specular terms take
the direction to the origin, not to the segment's nearest point, so a
surface the spot faces is lit by its cosine to the light.

Every per-pixel pass evaluates the same term: GLx's ARB programs
(tr_arb.c) and GLSL program (rendererglx/glx_module.cpp), the Vulkan and RTX
raster light shaders (shaders/light_frag.tmpl) and the path tracer
(rendererrtx/shaders/rt_lighting.glsl). R_SpotConeCullSphere lets the front
end skip a surface wholly outside the cone before it gets the pass. Every
other light, point or segment, engine or client game, has no cone.
===========================================================================
*/

#ifndef TR_SPOT_CONE_H
#define TR_SPOT_CONE_H

#include <math.h>

// the narrowest cosine range a cone's edge eases over, for an inner angle
// equal to its outer one (a hard edge)
#define SPOT_CONE_MIN_COSINE_RANGE	1.0e-4f

/*
=================
R_SpotConeCosines

The cosines of a spot's outer and inner half-angles, in degrees. The
sidecar parser clamps the outer angle to 1..179 and the inner one to
0..outer; this repeats both clamps for any other caller.
=================
*/
static ID_INLINE void R_SpotConeCosines( float innerAngle, float outerAngle, float *cosInner, float *cosOuter )
{
	if ( !( outerAngle >= 1.0f ) ) {
		outerAngle = 1.0f;
	} else if ( outerAngle > 179.0f ) {
		outerAngle = 179.0f;
	}
	if ( !( innerAngle >= 0.0f ) ) {
		innerAngle = 0.0f;
	} else if ( innerAngle > outerAngle ) {
		innerAngle = outerAngle;
	}
	*cosOuter = cosf( outerAngle * ( (float)M_PI / 180.0f ) );
	*cosInner = cosf( innerAngle * ( (float)M_PI / 180.0f ) );
}

/*
=================
R_SpotConeScaleBias

The cone term's scale and bias on the cosine of a fragment's angle off the
axis: clamp( cos * scale + bias, 0, 1 ) is 0 at the outer cosine and 1 at
the inner one.
=================
*/
static ID_INLINE void R_SpotConeScaleBias( float cosInner, float cosOuter, float *scale, float *bias )
{
	float range = cosInner - cosOuter;

	if ( !( range >= SPOT_CONE_MIN_COSINE_RANGE ) ) {
		range = SPOT_CONE_MIN_COSINE_RANGE;
	}
	*scale = 1.0f / range;
	*bias = -cosOuter * *scale;
}

/*
=================
R_SpotConeTerm

The cone term itself, as the shaders evaluate it.
=================
*/
static ID_INLINE float R_SpotConeTerm( float cosTheta, float scale, float bias )
{
	float t = cosTheta * scale + bias;

	if ( !( t > 0.0f ) ) {
		return 0.0f;
	}
	if ( t >= 1.0f ) {
		return 1.0f;
	}
	return t * t * ( 3.0f - 2.0f * t );
}

/*
=================
R_SpotConeCullSphere

True when a sphere lies wholly outside a cone with its apex at apex, its
unit axis along axis and the given outer cosine. The signed distance from a
point to the cone's surface, cos(a) |v x axis| - sin(a) (v . axis), never
exceeds the true distance, so a sphere it puts farther away than its radius
cannot touch the cone: the test only rejects what the cone term would leave
black. A sphere round the apex is always kept.
=================
*/
static ID_INLINE qboolean R_SpotConeCullSphere( const vec3_t apex, const vec3_t axis, float cosOuter,
	const vec3_t center, float radius )
{
	vec3_t v;
	float lengthSq;
	float along;
	float across;
	float sinOuter;

	VectorSubtract( center, apex, v );
	lengthSq = DotProduct( v, v );
	if ( lengthSq <= radius * radius ) {
		return qfalse;
	}
	along = DotProduct( v, axis );
	across = lengthSq - along * along;
	across = across > 0.0f ? sqrtf( across ) : 0.0f;
	sinOuter = 1.0f - cosOuter * cosOuter;
	sinOuter = sinOuter > 0.0f ? sqrtf( sinOuter ) : 0.0f;
	return cosOuter * across - sinOuter * along > radius ? qtrue : qfalse;
}

/*
=================
R_SpotConeCullBounds

R_SpotConeCullSphere for a dynamic light's local bounds test: the box's
bounding sphere against the cone from start toward end.
=================
*/
static ID_INLINE qboolean R_SpotConeCullBounds( const vec3_t start, const vec3_t end, float cosOuter,
	const vec3_t mins, const vec3_t maxs )
{
	vec3_t axis;
	vec3_t center;
	vec3_t half;
	float length;

	VectorSubtract( end, start, axis );
	length = DotProduct( axis, axis );
	if ( !( length > 0.0f ) ) {
		return qfalse;
	}
	VectorScale( axis, 1.0f / sqrtf( length ), axis );
	VectorAdd( mins, maxs, center );
	VectorScale( center, 0.5f, center );
	VectorSubtract( maxs, center, half );
	return R_SpotConeCullSphere( start, axis, cosOuter, center, sqrtf( DotProduct( half, half ) ) );
}

#endif // TR_SPOT_CONE_H
