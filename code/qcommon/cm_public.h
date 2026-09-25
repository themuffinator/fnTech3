/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/

#include "qfiles.h"


void		CM_LoadMap( const char *name, qboolean clientload, int *checksum);
void		CM_ClearMap( void );
clipHandle_t CM_InlineModel( int index );		// 0 = world, 1 + are bmodels
clipHandle_t CM_TempBoxModel( const vec3_t mins, const vec3_t maxs, int capsule );
// Gives the temporary box brush the contents of the entity one trace tests;
// the caller puts CONTENTS_BODY back after that trace.
void		CM_SetTempBoxModelContents( int contents );

void		CM_ModelBounds( clipHandle_t model, vec3_t mins, vec3_t maxs );

int			CM_NumClusters (void);
int			CM_NumInlineModels( void );
int			CM_NumLeafs( void );
char		*CM_EntityString (void);

// returns an ORed contents mask
int			CM_PointContents( const vec3_t p, clipHandle_t model );
int			CM_TransformedPointContents( const vec3_t p, clipHandle_t model, const vec3_t origin, const vec3_t angles );

void		CM_BoxTrace( trace_t *results, const vec3_t start, const vec3_t end,
						const vec3_t mins, const vec3_t maxs,
						clipHandle_t model, int brushmask, qboolean capsule );
void		CM_TransformedBoxTrace( trace_t *results, const vec3_t start, const vec3_t end,
						const vec3_t mins, const vec3_t maxs,
						clipHandle_t model, int brushmask,
						const vec3_t origin, const vec3_t angles, qboolean capsule );

byte		*CM_ClusterPVS (int cluster);
int			CM_ClusterBytes( void );

int			CM_PointLeafnum( const vec3_t p );
// Medal of Honor's lights reaching a point, the sun first (cm_mohaa_lights.c)
int			CM_MOHAAGatherLights( const vec3_t point, const byte *areamask, vec3_t *lightPositions,
				vec3_t *lightIntensities, int maxLights );

// only returns non-solid leafs
// overflow if return listsize and if *lastLeaf != list[listsize-1]
int			CM_BoxLeafnums( const vec3_t mins, const vec3_t maxs, int *list,
		 					int listsize, int *lastLeaf );

// Quake III IHV's Quake II-style record: the first node the box straddles,
// or -1 when the box stays on one side of every node it reaches.
// Q3Test 1.05's client game reads brushes in its engine's own shape (cm_test.c)
int			CM_Q3TestBoxBrushes( const vec3_t mins, const vec3_t maxs, void **list, int listsize, int *lastLeaf );
int			CM_BoxLeafnumsTopnode( const vec3_t mins, const vec3_t maxs, int *list,
								int listsize, int *topnode );
qboolean	CM_HeadnodeVisible( int nodenum, const byte *visbits );

int			CM_LeafCluster (int leafnum);
int			CM_LeafArea (int leafnum);

void		CM_AdjustAreaPortalState( int area1, int area2, qboolean open );
int			*CM_AreaPortalCounts( int *count );
int			CM_NumAreas( void );
void		CM_FloodAreaConnections( void );
qboolean	CM_AreasConnected( int area1, int area2 );

int			CM_WriteAreaBits( byte *buffer, int area );

// cm_patch.c
void CM_DrawDebugSurface( void (*drawPoly)(int color, int numPoints, float *points) );
