/*
===========================================================================
Copyright (C) 2008 Leszek Godlewski
Copyright (C) 2026 fnTech3 contributors

This file is part of fnTech3. It is adapted from OpenMoHAA's
code/qcommon/cm_terrain.c (https://github.com/openmoh/openmohaa), which is
distributed under the GNU General Public License, version 2 or (at your
option) any later version, and remains under that licence here.

fnTech3 is free software; you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation; either version 2 of the License, or (at your option) any later
version.

fnTech3 is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
A PARTICULAR PURPOSE. See the GNU General Public License for more details.
===========================================================================
*/

/*
Which triangles of a Medal of Honor terrain square exist.

A patch's two variance trees halve it, level by level, down to its 64-unit
squares, each two triangles. A node on the last level holds one bit for each
of its two halves, and a set bit takes that triangle out of the terrain: the
collision model walks through it (code/qcommon/cm_mohaa_terrain.c) and
2015's renderer never draws it (OpenMoHAA renderergl1/tr_terrain.c
R_AllocateTri and tr_surface.c RB_DrawTerrainTris, reference only). This
header is the one place both the collision model and the renderers'
translated world (code/renderercommon/tr_mohaa_world.c) read those bits, so
what is drawn is what is walked on.

A square's mode: 0 has neither triangle; 2 has both (the collision model
calls a concave pair 1); 3 and 4 keep only the first triangle, 5 and 6 only
the second. The first triangle of a square whose i + j is odd is the one at
its (i + 1, j + 1) corner and the second the one at (i, j); where i + j is
even, the first is at (i, j + 1) and the second at (i + 1, j).

Adaptation for fnTech3: the index table and mode lookup are OpenMoHAA's
CM_CalculateTerrainIndices and the head of CM_PickTerrainSquareMode, moved
here from cm_mohaa_terrain.c so both modules share them; the table is built
into the caller's storage rather than a global.
*/
#ifndef FNTECH3_MOHAA_TERRAIN_SQUARES_H
#define FNTECH3_MOHAA_TERRAIN_SQUARES_H

#include "bsp_mohaa.h"

typedef struct {
	unsigned short	iTreeAndMask;
	unsigned short	iNode;
} mohaaVarnodeIndex_t;

/* [the patch's diagonal][i][j][half]: the node and bit that hold a triangle */
typedef struct {
	mohaaVarnodeIndex_t	vni[2][MOHAA_TERRAIN_SQUARES][MOHAA_TERRAIN_SQUARES][2];
} mohaaTerrainSquareTable_t;

typedef struct {
	int i0;
	int j0;
	int i1;
	int j1;
	int i2;
	int j2;
} mohaaTerrainWorknode_t;

static ID_INLINE void MOHAA_CalculateTerrainIndices( mohaaTerrainSquareTable_t *table,
	mohaaTerrainWorknode_t *worknode, int iDiagonal, int iTree ) {
	int					i;
	int					i2;
	int					j2;
	mohaaVarnodeIndex_t	*vni;

	for ( i = 0; i <= 30; i++ ) {
		i2 = worknode[i + 1].i0 + worknode[i + 1].i1;
		j2 = worknode[i + 1].j0 + worknode[i + 1].j1;
		worknode[i * 2 + 2].i0 = worknode[i + 1].i1;
		worknode[i * 2 + 2].j0 = worknode[i + 1].j1;
		worknode[i * 2 + 2].i1 = worknode[i + 1].i2;
		worknode[i * 2 + 2].j1 = worknode[i + 1].j2;
		worknode[i * 2 + 2].i2 = i2 >> 1;
		worknode[i * 2 + 2].j2 = j2 >> 1;
		worknode[i * 2 + 2 + 1].i0 = worknode[i + 1].i2;
		worknode[i * 2 + 2 + 1].j0 = worknode[i + 1].j2;
		worknode[i * 2 + 2 + 1].i1 = worknode[i + 1].i0;
		worknode[i * 2 + 2 + 1].j1 = worknode[i + 1].j0;
		worknode[i * 2 + 2 + 1].i2 = i2 >> 1;
		worknode[i * 2 + 2 + 1].j2 = j2 >> 1;
	}

	for ( i = 32; i < 64; i++ ) {
		i2 = ( worknode[i].i0 + worknode[i].i1 ) >> 1;
		j2 = ( worknode[i].j0 + worknode[i].j1 ) >> 1;

		if ( worknode[i].i0 == worknode[i].i1 ) {
			if ( worknode[i].j0 <= worknode[i].j1 ) {
				vni = &table->vni[iDiagonal][i2][j2][1];
				vni->iNode = (unsigned short)( i - 1 );
				vni->iTreeAndMask = (unsigned short)( iTree | 0x2000 );

				vni = &table->vni[iDiagonal][i2][j2 - 1][0];
				vni->iNode = (unsigned short)( i - 1 );
				vni->iTreeAndMask = (unsigned short)( iTree | 0x1000 );
			} else {
				vni = &table->vni[iDiagonal][i2 - 1][j2][1];
				vni->iNode = (unsigned short)( i - 1 );
				vni->iTreeAndMask = (unsigned short)( iTree | 0x1000 );

				vni = &table->vni[iDiagonal][i2 - 1][j2 - 1][0];
				vni->iNode = (unsigned short)( i - 1 );
				vni->iTreeAndMask = (unsigned short)( iTree | 0x2000 );
			}
		} else {
			if ( worknode[i].i0 <= worknode[i].i1 ) {
				vni = &table->vni[iDiagonal][i2][j2 - 1][0];
				vni->iNode = (unsigned short)( i - 1 );
				vni->iTreeAndMask = (unsigned short)( iTree | 0x2000 );

				vni = &table->vni[iDiagonal][i2 - 1][j2 - 1][0];
				vni->iNode = (unsigned short)( i - 1 );
				vni->iTreeAndMask = (unsigned short)( iTree | 0x1000 );
			} else {
				vni = &table->vni[iDiagonal][i2][j2][1];
				vni->iNode = (unsigned short)( i - 1 );
				vni->iTreeAndMask = (unsigned short)( iTree | 0x1000 );

				vni = &table->vni[iDiagonal][i2 - 1][j2][1];
				vni->iNode = (unsigned short)( i - 1 );
				vni->iTreeAndMask = (unsigned short)( iTree | 0x2000 );
			}
		}
	}
}

/* The table every patch's squares are read through; the same for every map. */
static ID_INLINE void MOHAA_BuildTerrainSquareTable( mohaaTerrainSquareTable_t *table ) {
	mohaaTerrainWorknode_t worknode[64];

	Com_Memset( table, 0, sizeof( *table ) );
	Com_Memset( worknode, 0, sizeof( worknode ) );

	worknode[1].i0 = 8;
	worknode[1].j0 = 8;
	worknode[1].i1 = 0;
	worknode[1].j1 = 0;
	worknode[1].i2 = 0;
	worknode[1].j2 = 8;
	MOHAA_CalculateTerrainIndices( table, worknode, 0, 0 );

	worknode[1].i0 = 0;
	worknode[1].j0 = 0;
	worknode[1].i1 = 8;
	worknode[1].j1 = 8;
	worknode[1].i2 = 8;
	worknode[1].j2 = 0;
	MOHAA_CalculateTerrainIndices( table, worknode, 0, 1 );

	worknode[1].i0 = 8;
	worknode[1].j0 = 0;
	worknode[1].i1 = 0;
	worknode[1].j1 = 8;
	worknode[1].i2 = 8;
	worknode[1].j2 = 8;
	MOHAA_CalculateTerrainIndices( table, worknode, 1, 0 );

	worknode[1].i0 = 0;
	worknode[1].j0 = 8;
	worknode[1].i1 = 8;
	worknode[1].j1 = 0;
	worknode[1].i2 = 0;
	worknode[1].j2 = 0;
	MOHAA_CalculateTerrainIndices( table, worknode, 1, 1 );
}

/* Square (i, j)'s mode, 0 to 6, before the collision model tells a concave
 * pair from a convex one. */
static ID_INLINE int MOHAA_TerrainSquareMode( const mohaaTerrainSquareTable_t *table,
	const dterrainPatchMOHAA_t *patch, int i, int j ) {
	static const int	modeTable[] = { 2, 2, 5, 6, 4, 3, 0, 0 };
	int					flags0, flags1;
	const mohaaVarnodeIndex_t	*vni;

	vni = table->vni[( patch->flags & MOHAA_TERPATCH_NEIGHBOR ) ? 1 : 0][i][j];

	if ( ( vni[0].iTreeAndMask & LittleShort( patch->varTree[vni[0].iTreeAndMask & 1][vni[0].iNode].flags ) & 0xFFFE ) != 0 ) {
		flags0 = 2;
	} else {
		flags0 = 0;
	}

	if ( ( vni[1].iTreeAndMask & LittleShort( patch->varTree[vni[1].iTreeAndMask & 1][vni[1].iNode].flags ) & 0xFFFE ) != 0 ) {
		flags1 = 4;
	} else {
		flags1 = 0;
	}

	return modeTable[( ( j + i ) & 1 ) | flags0 | flags1];
}

/* Whether a square of this mode keeps its first (0) or second (1) triangle. */
static ID_INLINE qboolean MOHAA_TerrainSquareHasTriangle( int mode, int half ) {
	switch ( mode ) {
	case 1:
	case 2:
		return qtrue;
	case 3:
	case 4:
		return half == 0 ? qtrue : qfalse;
	case 5:
	case 6:
		return half == 1 ? qtrue : qfalse;
	default:
		return qfalse;
	}
}

#endif
