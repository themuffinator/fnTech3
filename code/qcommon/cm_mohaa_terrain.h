/*
===========================================================================
Copyright (C) 2026 fnTech3 contributors

This file is part of fnTech3 and is distributed under the terms of the GNU
General Public License, version 2 or (at your option) any later version.

Medal of Honor's LOD terrain for collision; see cm_mohaa_terrain.c.
===========================================================================
*/
#ifndef FNTECH3_CM_MOHAA_TERRAIN_H
#define FNTECH3_CM_MOHAA_TERRAIN_H

#include "bsp_mohaa.h"

/* One 64-unit square: its two triangle planes and how it collides.
 * eMode 0 does not collide; 1 and 2 are both halves (1 concave, 2 convex);
 * 3 and 4 are the first half alone and 5 and 6 the second. */
typedef struct {
	vec4_t	plane[2];
	int		eMode;
} mohaaTerrainCollideSquare_t;

typedef struct {
	vec3_t						vBounds[2];
	mohaaTerrainCollideSquare_t	squares[MOHAA_TERRAIN_SQUARES][MOHAA_TERRAIN_SQUARES];
} mohaaTerrainCollide_t;

typedef struct cmTerrain_s {
	int						checkcount;
	int						surfaceFlags;
	int						contents;
	int						shaderNum;
	mohaaTerrainCollide_t	tc;
} cmTerrain_t;

void		CM_MOHAA_PrepareTerrainCollide( void );
void		CM_MOHAA_GenerateTerrainCollide( const dterrainPatchMOHAA_t *patch, mohaaTerrainCollide_t *tc );
void		CM_MOHAA_TraceThroughTerrainCollide( traceWork_t *tw, const mohaaTerrainCollide_t *tc );
qboolean	CM_MOHAA_PositionTestInTerrainCollide( traceWork_t *tw, const mohaaTerrainCollide_t *tc );
qboolean	CM_MOHAA_SightTraceThroughTerrainCollide( traceWork_t *tw, const mohaaTerrainCollide_t *tc );

#endif
