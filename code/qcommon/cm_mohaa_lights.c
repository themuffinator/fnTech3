/*
===========================================================================
Copyright (C) 2026 fnTech3 contributors

This file is part of fnTech3 and is distributed under the terms of the GNU
General Public License, version 2 or (at your option) any later version.
===========================================================================
*/

/*
Medal of Honor's sphere lights, for its client game.

A "2015" map carries its compiler's point lights -- origin, colour,
intensity and the leaf each is in -- and, for each leaf in turn, the lights
that reach it, the sun first where the leaf sees the sky (-2), each list
ending in -1. The renderers bake them into the light grid
(code/renderercommon/tr_mohaa_world.c); 1.00's client game also asks, through
its import 54, which lights reach a point, to cast a character's foot
shadows from each under the video options' "Complex" shadows (`cg_shadows
2`; its one call, 0x30029b98, the feet being "Bip01 L Foot" and "Bip01 R
Foot"). The collision model keeps them for that answer, as it keeps the
map's other data.

Written anew after OpenMoHAA's renderergl1/tr_sphere_shade.cpp
R_GatherLightSources and tr_bsp.c R_LoadSphereLights and
R_LoadSphereLightVis (reference only): in a leaf with lights, the sun if the
leaf's list names it first and a line from the point toward it reaches the
sky, then the leaf's lights whose own area the view can see; in any other
leaf, every light in the map. A light counts when its intensity times 7,500
over its squared distance comes to 5 or more; the strongest come first, at
most eight in all, each given as its origin and that measure over 200 times
its colour, scaled down so no component passes 1. The sun lies 16,384 units
out along its direction with its colour over 510. The sun's colour and
direction are the worldspawn's `suncolor` (or `sunlight`) and `sundirection`
keys, matched exactly, the colour 70 in each component where no key gives
one, as 2015's renderer takes them, the colour times its overbright
multiplier: 2, as r_mapOverBrightBits 1 less r_overBrightBits 0, the
defaults OpenMoHAA registers (tr_init.c, and tr_image.c
R_SetColorMappings), which no 1.00 configuration changes.
*/

#include "cm_local.h"
#include "cm_mohaa_lights.h"

#define MOHAA_GATHERED_LIGHTS	8
#define MOHAA_SUN_DISTANCE		16384.0f
#define MOHAA_SUN_OVERBRIGHT	2.0f

/* The worldspawn's sun keys, which come before every other entity. */
static void CM_MOHAASunKeys( cmMOHAALights_t *lights ) {
	const char *p = cm.entityString;
	char key[MAX_TOKEN_CHARS];
	qboolean inWorld = qfalse;

	VectorSet( lights->sunColor, 70.0f * MOHAA_SUN_OVERBRIGHT, 70.0f * MOHAA_SUN_OVERBRIGHT,
		70.0f * MOHAA_SUN_OVERBRIGHT );
	VectorClear( lights->sunDirection );
	lights->sun = qfalse;
	if ( !p ) {
		return;
	}
	for ( ;; ) {
		const char *token = COM_ParseExt( &p, qtrue );
		if ( !token[0] ) {
			break;
		}
		if ( token[0] == '{' ) {
			if ( inWorld ) {
				break;
			}
			inWorld = qtrue;
			continue;
		}
		if ( token[0] == '}' ) {
			break;
		}
		Q_strncpyz( key, token, sizeof( key ) );
		token = COM_ParseExt( &p, qtrue );
		if ( !strcmp( key, "suncolor" ) || !strcmp( key, "sunlight" ) ) {
			vec3_t color = { 0.0f, 0.0f, 0.0f };
			if ( sscanf( token, "%f %f %f", &color[0], &color[1], &color[2] ) == 3 ) {
				VectorScale( color, MOHAA_SUN_OVERBRIGHT, lights->sunColor );
				lights->sun = qtrue;
			}
		} else if ( !strcmp( key, "sundirection" ) ) {
			vec3_t angles = { 0.0f, 0.0f, 0.0f };
			if ( sscanf( token, "%f %f %f", &angles[0], &angles[1], &angles[2] ) == 3 ) {
				AngleVectors( angles, lights->sunDirection, NULL, NULL );
				lights->sun = qtrue;
			}
		}
	}
}

/*
====================
CMod_LoadMOHAALights

The lights and each leaf's list of them, kept for the map's life; nothing on
a map without them. A list that names a light the map lacks loses that name.
====================
*/
void CMod_LoadMOHAALights( const mohaa_translation_t *translation ) {
	cmMOHAALights_t *out;
	int i, cursor;

	cm.mohaaLights = NULL;
	if ( !translation || translation->numSphereLights <= 0 || !translation->sphereLights ) {
		return;
	}

	out = Hunk_Alloc( sizeof( *out ), h_high );
	out->numLights = translation->numSphereLights;
	out->lights = Hunk_Alloc( out->numLights * sizeof( out->lights[0] ), h_high );
	for ( i = 0; i < out->numLights; i++ ) {
		const dsphereLightMOHAA_t *in = &translation->sphereLights[i];
		cmMOHAALight_t *light = &out->lights[i];
		int k;
		for ( k = 0; k < 3; k++ ) {
			light->origin[k] = LittleFloat( in->origin[k] );
			light->color[k] = LittleFloat( in->color[k] );
		}
		light->intensity = LittleFloat( in->intensity );
		light->leaf = LittleLong( in->leaf );
		if ( light->leaf < 0 || light->leaf >= cm.numLeafs ) {
			light->leaf = -1;
		}
	}

	out->numLeafs = cm.numLeafs;
	out->leafFirst = Hunk_Alloc( ( cm.numLeafs + 1 ) * sizeof( int ), h_high );
	out->leafCount = Hunk_Alloc( ( cm.numLeafs + 1 ) * sizeof( int ), h_high );
	out->leafSun = Hunk_Alloc( ( cm.numLeafs + 1 ) * sizeof( qboolean ), h_high );
	out->list = Hunk_Alloc( ( translation->numSphereLightVis + 1 ) * sizeof( int ), h_high );
	cursor = 0;
	for ( i = 0; i < cm.numLeafs && cursor < translation->numSphereLightVis; i++ ) {
		int entry = LittleLong( translation->sphereLightVis[cursor] );
		out->leafFirst[i] = cursor;
		if ( entry == -2 ) {
			out->leafSun[i] = qtrue;
			cursor++;
		}
		for ( ; cursor < translation->numSphereLightVis; cursor++ ) {
			entry = LittleLong( translation->sphereLightVis[cursor] );
			if ( entry == -1 ) {
				cursor++;
				break;
			}
			if ( entry >= 0 && entry < out->numLights ) {
				out->list[out->leafFirst[i] + out->leafCount[i]++] = entry;
			}
		}
	}

	CM_MOHAASunKeys( out );
	cm.mohaaLights = out;
}

/* A light's measure at a point, or 0 when it does not count. */
static float CM_MOHAALightFalloff( const cmMOHAALight_t *light, const vec3_t point ) {
	vec3_t delta;
	float distanceSquared, falloff;

	if ( light->intensity < 0.0f ) {
		return 0.0f;
	}
	VectorSubtract( light->origin, point, delta );
	distanceSquared = DotProduct( delta, delta );
	if ( distanceSquared <= 0.0f ) {
		return 0.0f;
	}
	falloff = light->intensity * 7500.0f / distanceSquared;
	return falloff >= 5.0f ? falloff : 0.0f;
}

/* The strongest so far, strongest first. */
static void CM_MOHAAKeepLight( int *kept, float *falloffs, int *numKept, int light, float falloff ) {
	int at = *numKept, i;

	while ( at > 0 && falloffs[at - 1] < falloff ) {
		at--;
	}
	if ( at >= MOHAA_GATHERED_LIGHTS ) {
		return;
	}
	for ( i = ( *numKept < MOHAA_GATHERED_LIGHTS ? *numKept : MOHAA_GATHERED_LIGHTS - 1 ); i > at; i-- ) {
		kept[i] = kept[i - 1];
		falloffs[i] = falloffs[i - 1];
	}
	kept[at] = light;
	falloffs[at] = falloff;
	if ( *numKept < MOHAA_GATHERED_LIGHTS ) {
		( *numKept )++;
	}
}

/*
====================
CM_MOHAAGatherLights

The lights reaching `point`, the sun first, as positions and colours; the
count written, no more than maxLights or eight. areamask is the view's, whose
set bits are areas it cannot see; NULL sees every area.
====================
*/
int CM_MOHAAGatherLights( const vec3_t point, const byte *areamask, vec3_t *lightPositions,
	vec3_t *lightIntensities, int maxLights ) {
	const cmMOHAALights_t *lights = cm.mohaaLights;
	int kept[MOHAA_GATHERED_LIGHTS];
	float falloffs[MOHAA_GATHERED_LIGHTS];
	int numKept = 0, count = 0, leafnum, i;
	qboolean inLeaf;

	if ( !lights || !point || !lightPositions || !lightIntensities || maxLights <= 0 ) {
		return 0;
	}
	if ( maxLights > MOHAA_GATHERED_LIGHTS ) {
		maxLights = MOHAA_GATHERED_LIGHTS;
	}

	leafnum = CM_PointLeafnum( point );
	inLeaf = cm.vised && leafnum >= 0 && leafnum < lights->numLeafs && cm.leafs[leafnum].area != -1
		&& ( lights->leafCount[leafnum] > 0 || lights->leafSun[leafnum] );

	if ( inLeaf ) {
		for ( i = 0; i < lights->leafCount[leafnum]; i++ ) {
			const int index = lights->list[lights->leafFirst[leafnum] + i];
			const cmMOHAALight_t *light = &lights->lights[index];
			float falloff;
			if ( light->leaf >= 0 && areamask ) {
				const int area = cm.leafs[light->leaf].area;
				if ( area >= 0 && ( areamask[area >> 3] & ( 1 << ( area & 7 ) ) ) ) {
					continue;
				}
			}
			falloff = CM_MOHAALightFalloff( light, point );
			if ( falloff > 0.0f ) {
				CM_MOHAAKeepLight( kept, falloffs, &numKept, index, falloff );
			}
		}
	} else {
		for ( i = 0; i < lights->numLights; i++ ) {
			const float falloff = CM_MOHAALightFalloff( &lights->lights[i], point );
			if ( falloff > 0.0f ) {
				CM_MOHAAKeepLight( kept, falloffs, &numKept, i, falloff );
			}
		}
	}

	if ( lights->sun && inLeaf && lights->leafSun[leafnum] ) {
		trace_t trace;
		vec3_t end;
		VectorMA( point, MOHAA_SUN_DISTANCE, lights->sunDirection, end );
		CM_BoxTrace( &trace, point, end, vec3_origin, vec3_origin, 0, CONTENTS_SOLID, qfalse );
		if ( trace.surfaceFlags & SURF_SKY ) {
			VectorCopy( end, lightPositions[0] );
			VectorScale( lights->sunColor, 1.0f / 510.0f, lightIntensities[0] );
			count = 1;
		}
	}

	for ( i = 0; i < numKept && count < maxLights; i++, count++ ) {
		const cmMOHAALight_t *light = &lights->lights[kept[i]];
		float peak;
		VectorCopy( light->origin, lightPositions[count] );
		VectorScale( light->color, falloffs[i] / 200.0f, lightIntensities[count] );
		peak = lightIntensities[count][0];
		if ( lightIntensities[count][1] > peak ) {
			peak = lightIntensities[count][1];
		}
		if ( lightIntensities[count][2] > peak ) {
			peak = lightIntensities[count][2];
		}
		if ( peak > 1.0f ) {
			VectorScale( lightIntensities[count], 1.0f / peak, lightIntensities[count] );
		}
	}
	return count;
}
