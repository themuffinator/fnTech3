/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.
Copyright (C) 2015 the OpenMoHAA team
Copyright (C) 2026 fnTech3 contributors

This file is part of fnTech3. It is adapted from OpenMoHAA's
code/server/sv_world.c and code/qcommon/cm_trace_lbd.cpp
(https://github.com/openmoh/openmohaa), which are distributed under the GNU
General Public License, version 2 or (at your option) any later version,
and remain under that licence here.

fnTech3 is free software; you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation; either version 2 of the License, or (at your option) any later
version.

fnTech3 is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
A PARTICULAR PURPOSE. See the GNU General Public License for more details.
===========================================================================
*/
#ifndef FNTECH3_SERVER_MOHAA_WORLD_HPP
#define FNTECH3_SERVER_MOHAA_WORLD_HPP

/* Medal of Honor: Allied Assault's world: which entities are linked where,
 * and the traces and contents queries its game module asks of them.
 *
 * The game's entity record is its own (code/compat/mohaa_game_abi.h), so
 * this world reads it directly rather than through the Quake III shared
 * entity every other profile's world uses. Its rules are 2015's, as
 * OpenMoHAA reconstructs them, and differ from Quake III's where the record
 * does: the entity's own `solid` decides how its state's packed box is
 * written; a rotated entity's bounds are turned by the rotation matrix the
 * game keeps beside it; a box entity is traced as a box of its own
 * contents; and a hit on a character can be resolved to one of nineteen
 * body locations by tracing spheres placed on its skeleton's bones.
 *
 * The world sectors are this file's own. The per-entity cluster and area
 * record is the server's svEntity_t, which the snapshot walk reads. Included
 * by server_mohaa_game.hpp, which defines the entity accessors declared
 * below. */

namespace mohaa_world {

using fntech3::mohaa::GEntity;

/* ---- the game's solid and contents values ------------------------------ */

/* solid_t (OpenMoHAA q_shared.h, Quake III's order). */
enum : std::int32_t {
	SOLID_NOT = 0,
	SOLID_TRIGGER = 1,
	SOLID_BBOX = 2,
	SOLID_BSP = 3
};

/* The packed-box sentinel an inline model's state carries. */
constexpr std::int32_t STATE_SOLID_BMODEL = 0xffffff;

/* surfaceflags.h's 2015 contents, where they are not Quake III's. */
constexpr std::int32_t MOHAA_CONTENTS_SOLID = 0x1;
constexpr std::int32_t MOHAA_CONTENTS_SHOOTONLY = 0x100000;

/* ---- the packed box a state carries for client prediction ------------- */

/* OpenMoHAA q_math.c BoundingBoxToInteger: nine bits of half-width, eight of
 * half-depth, five of floor below the origin and nine of height above it. */
constexpr int BBOX_XBITS = 9;
constexpr int BBOX_YBITS = 8;
constexpr int BBOX_ZBOTTOMBITS = 5;
constexpr int BBOX_ZTOPBITS = 9;
constexpr int BBOX_MAX_X = 1 << BBOX_XBITS;
constexpr int BBOX_MAX_Y = 1 << BBOX_YBITS;
constexpr int BBOX_MAX_BOTTOM_Z = 1 << ( BBOX_ZBOTTOMBITS - 1 );
constexpr int BBOX_REALMAX_BOTTOM_Z = 1 << BBOX_ZBOTTOMBITS;
constexpr int BBOX_MAX_TOP_Z = 1 << BBOX_ZTOPBITS;

int BoundingBoxToInteger( const float *mins, const float *maxs ) {
	int x = (int)maxs[0];
	if ( x < 0 ) x = 0;
	if ( x >= BBOX_MAX_X ) x = BBOX_MAX_X - 1;

	int y = (int)maxs[1];
	if ( y < 0 ) y = 0;
	if ( y >= BBOX_MAX_Y ) y = BBOX_MAX_Y - 1;

	int zd = (int)( mins[2] + BBOX_MAX_BOTTOM_Z );
	if ( zd < 0 ) zd = 0;
	if ( zd >= BBOX_REALMAX_BOTTOM_Z ) zd = BBOX_REALMAX_BOTTOM_Z - 1;

	int zu = (int)maxs[2];
	if ( zu < 0 ) zu = 0;
	if ( zu >= BBOX_MAX_TOP_Z ) zu = BBOX_MAX_TOP_Z - 1;

	return x | ( y << BBOX_XBITS ) | ( zd << ( BBOX_XBITS + BBOX_YBITS ) )
		| ( zu << ( BBOX_XBITS + BBOX_YBITS + BBOX_ZBOTTOMBITS ) );
}

/* OpenMoHAA q_math.c CalculateRotatedBounds2: the eight corners turned by
 * the entity's matrix, and the box around them. */
void CalculateRotatedBounds2( const float trans[3][3], float *mins, float *maxs ) {
	vec3_t rotmins, rotmaxs;

	ClearBounds( rotmins, rotmaxs );
	for ( int i = 0; i < 8; i++ ) {
		vec3_t tmp, rottemp;

		tmp[0] = ( i & 1 ) ? mins[0] : maxs[0];
		tmp[1] = ( i & 2 ) ? mins[1] : maxs[1];
		tmp[2] = ( i & 4 ) ? mins[2] : maxs[2];
		for ( int k = 0; k < 3; k++ ) {
			rottemp[k] = tmp[0] * trans[0][k] + tmp[1] * trans[1][k] + tmp[2] * trans[2][k];
		}
		AddPointToBounds( rottemp, rotmins, rotmaxs );
	}
	VectorCopy( rotmins, mins );
	VectorCopy( rotmaxs, maxs );
}

/* ---- entity access -------------------------------------------------------- */

GEntity *Entity( int number );          // server_mohaa_game.hpp
int EntityNumber( const void *entity ); // server_mohaa_game.hpp
orientation_t TagOrientation( GEntity *ent, int bone ); // the game's export

svEntity_t *SvEntity( const GEntity *ent ) {
	const int number = ent ? ent->s.number : -1;
	if ( number < 0 || number >= MAX_GENTITIES ) {
		Com_Error( ERR_DROP, "Medal of Honor world: bad entity %i", number );
		return nullptr;
	}
	return &sv.svEntities[number];
}

/* ---- the sector tree ------------------------------------------------------ */

struct WorldSector {
	int axis;   // -1 = leaf node
	float dist;
	WorldSector *children[2];
	svEntity_t *entities;
};

constexpr int AREA_DEPTH = 4;
constexpr int AREA_NODES = 64;

WorldSector worldSectors[AREA_NODES];
int numWorldSectors;

WorldSector *CreateWorldSector( int depth, const vec3_t mins, const vec3_t maxs ) {
	WorldSector *anode = &worldSectors[numWorldSectors];
	numWorldSectors++;

	if ( depth == AREA_DEPTH ) {
		anode->axis = -1;
		anode->children[0] = anode->children[1] = nullptr;
		return anode;
	}

	vec3_t size;
	VectorSubtract( maxs, mins, size );
	anode->axis = size[0] > size[1] ? 0 : 1;
	anode->dist = 0.5f * ( maxs[anode->axis] + mins[anode->axis] );

	vec3_t mins1, maxs1, mins2, maxs2;
	VectorCopy( mins, mins1 );
	VectorCopy( mins, mins2 );
	VectorCopy( maxs, maxs1 );
	VectorCopy( maxs, maxs2 );
	maxs1[anode->axis] = mins2[anode->axis] = anode->dist;

	anode->children[0] = CreateWorldSector( depth + 1, mins2, maxs2 );
	anode->children[1] = CreateWorldSector( depth + 1, mins1, maxs1 );
	return anode;
}

/* The engine's own linked-entity chains are cleared with the server state;
 * every svEntity_t starts unlinked. */
void ClearWorld() {
	Com_Memset( worldSectors, 0, sizeof( worldSectors ) );
	numWorldSectors = 0;

	vec3_t mins, maxs;
	CM_ModelBounds( CM_InlineModel( 0 ), mins, maxs );
	CreateWorldSector( 0, mins, maxs );
}

/* ---- linking -------------------------------------------------------------- */

clipHandle_t ClipHandleForEntity( const GEntity *ent ) {
	if ( ent->r.bmodel && ent->solid != SOLID_BBOX ) {
		// explicit hulls in the BSP model
		return CM_InlineModel( ent->s.modelindex );
	}
	// a temporary box of the entity's own contents
	const clipHandle_t handle = CM_TempBoxModel( ent->r.mins, ent->r.maxs, qfalse );
	CM_SetTempBoxModelContents( ent->r.contents );
	return handle;
}

/* The temporary box is shared; put Quake III's body contents back once a
 * sweep that used it is done, as every other profile's world expects. */
void RestoreBoxContents() {
	CM_SetTempBoxModelContents( CONTENTS_BODY );
}

void UnlinkEntity( GEntity *gEnt ) {
	svEntity_t *ent = SvEntity( gEnt );
	if ( !ent ) {
		return;
	}

	gEnt->r.linked = qfalse;

	WorldSector *ws = static_cast<WorldSector *>( static_cast<void *>( ent->worldSector ) );
	if ( !ws ) {
		return; // not linked in anywhere
	}
	ent->worldSector = nullptr;

	if ( ws->entities == ent ) {
		ws->entities = ent->nextEntityInWorldSector;
		return;
	}

	for ( svEntity_t *scan = ws->entities; scan; scan = scan->nextEntityInWorldSector ) {
		if ( scan->nextEntityInWorldSector == ent ) {
			scan->nextEntityInWorldSector = ent->nextEntityInWorldSector;
			return;
		}
	}

	Com_Printf( "WARNING: SV_UnlinkEntity: not found in worldSector\n" );
}

constexpr int MAX_TOTAL_ENT_LEAFS = 128;

void LinkEntity( GEntity *gEnt ) {
	svEntity_t *ent = SvEntity( gEnt );
	if ( !ent ) {
		return;
	}

	if ( ent->worldSector ) {
		UnlinkEntity( gEnt ); // unlink from old position
	}

	switch ( gEnt->solid ) {
	case SOLID_TRIGGER:
		gEnt->s.solid = 0;
		break;
	case SOLID_BBOX:
		if ( !gEnt->r.contents || gEnt->r.contents == MOHAA_CONTENTS_SHOOTONLY ) {
			gEnt->s.solid = 0;
		} else {
			// encode the size into the entityState_t for client prediction
			gEnt->s.solid = BoundingBoxToInteger( gEnt->r.mins, gEnt->r.maxs );
		}
		break;
	case SOLID_BSP:
		if ( gEnt->r.contents == -1 || !( gEnt->r.contents & MOHAA_CONTENTS_SHOOTONLY ) ) {
			gEnt->s.solid = STATE_SOLID_BMODEL;
		} else {
			gEnt->s.solid = 0;
		}
		break;
	default:
		gEnt->s.solid = gEnt->solid;
		break;
	}

	const float *origin = gEnt->s.origin;

	// set the abs box
	if ( gEnt->r.currentAngles[0] || gEnt->r.currentAngles[1] || gEnt->r.currentAngles[2] ) {
		// expand for rotation
		VectorCopy( gEnt->r.mins, gEnt->r.absmin );
		VectorCopy( gEnt->r.maxs, gEnt->r.absmax );
		CalculateRotatedBounds2( gEnt->mat, gEnt->r.absmin, gEnt->r.absmax );
		VectorAdd( origin, gEnt->r.absmin, gEnt->r.absmin );
		VectorAdd( origin, gEnt->r.absmax, gEnt->r.absmax );
	} else {
		VectorAdd( origin, gEnt->r.mins, gEnt->r.absmin );
		VectorAdd( origin, gEnt->r.maxs, gEnt->r.absmax );
	}

	// because movement is clipped an epsilon away from an actual edge,
	// we must fully check even when bounding boxes don't quite touch
	for ( int i = 0; i < 3; i++ ) {
		gEnt->r.absmin[i] -= 1;
		gEnt->r.absmax[i] += 1;
	}

	// link to PVS leafs
	ent->numClusters = 0;
	ent->lastCluster = 0;
	ent->areanum = -1;
	ent->areanum2 = -1;

	// get all leafs, including solids
	int leafs[MAX_TOTAL_ENT_LEAFS];
	int lastLeaf;
	const int num_leafs = CM_BoxLeafnums( gEnt->r.absmin, gEnt->r.absmax, leafs, MAX_TOTAL_ENT_LEAFS, &lastLeaf );

	// if none of the leafs were inside the map, the
	// entity is outside the world and can be considered unlinked
	if ( !num_leafs ) {
		return;
	}

	// set areas, even from clusters that don't fit in the entity array
	for ( int i = 0; i < num_leafs; i++ ) {
		const int area = CM_LeafArea( leafs[i] );
		if ( area != -1 ) {
			// doors may legally straggle two areas,
			// but nothing should ever need more than that
			if ( ent->areanum != -1 && ent->areanum != area ) {
				if ( ent->areanum2 != -1 && ent->areanum2 != area && sv.state == SS_LOADING ) {
					Com_DPrintf( "Object %i touching 3 areas at %f %f %f\n", gEnt->s.number,
						gEnt->r.absmin[0], gEnt->r.absmin[1], gEnt->r.absmin[2] );
				}
				ent->areanum2 = area;
			} else {
				ent->areanum = area;
			}
		}
	}

	// store the areanum to gentity
	gEnt->r.areanum = ent->areanum;

	// store as many explicit clusters as we can
	ent->numClusters = 0;
	int i;
	for ( i = 0; i < num_leafs; i++ ) {
		const int cluster = CM_LeafCluster( leafs[i] );
		if ( cluster != -1 ) {
			ent->clusternums[ent->numClusters++] = cluster;
			if ( ent->numClusters == MAX_ENT_CLUSTERS ) {
				break;
			}
		}
	}

	// store off a last cluster if we need to
	if ( i != num_leafs ) {
		ent->lastCluster = CM_LeafCluster( lastLeaf );
	}

	gEnt->r.linkcount++;

	// find the first world sector node that the ent's box crosses
	WorldSector *node = worldSectors;
	for ( ;; ) {
		if ( node->axis == -1 ) {
			break;
		}
		if ( gEnt->r.absmin[node->axis] > node->dist ) {
			node = node->children[0];
		} else if ( gEnt->r.absmax[node->axis] < node->dist ) {
			node = node->children[1];
		} else {
			break; // crosses the node
		}
	}

	// link it in
	ent->worldSector = static_cast<struct worldSector_s *>( static_cast<void *>( node ) );
	ent->nextEntityInWorldSector = node->entities;
	node->entities = ent;

	gEnt->r.linked = qtrue;
}

/* ---- area queries --------------------------------------------------------- */

struct AreaParms {
	const float *mins;
	const float *maxs;
	int *list;
	int count, maxcount;
};

void AreaEntities_r( WorldSector *node, AreaParms *ap ) {
	WorldSector *nodestack[8];
	int iStackPos = 0;

	for ( ;; ) {
		for ( ;; ) {
			for ( svEntity_t *check = node->entities, *next; check; check = next ) {
				next = check->nextEntityInWorldSector;

				const GEntity *gcheck = Entity( (int)( check - sv.svEntities ) );
				if ( !gcheck ) {
					continue;
				}
				if ( gcheck->r.absmin[0] > ap->maxs[0] || gcheck->r.absmin[1] > ap->maxs[1]
					|| gcheck->r.absmin[2] > ap->maxs[2] || gcheck->r.absmax[0] < ap->mins[0]
					|| gcheck->r.absmax[1] < ap->mins[1] || gcheck->r.absmax[2] < ap->mins[2] ) {
					continue;
				}

				if ( ap->count == ap->maxcount ) {
					Com_Printf( "SV_AreaEntities: MAXCOUNT\n" );
					return;
				}

				ap->list[ap->count] = (int)( check - sv.svEntities );
				ap->count++;
			}

			if ( node->axis == -1 ) {
				break; // terminal node
			}

			// recurse down both sides
			if ( ap->maxs[node->axis] > node->dist ) {
				nodestack[iStackPos++] = node->children[0];
			}
			if ( ap->mins[node->axis] < node->dist ) {
				node = node->children[1];
			} else {
				break;
			}
		}

		if ( !iStackPos ) {
			return;
		}
		iStackPos--;
		node = nodestack[iStackPos];
	}
}

int AreaEntities( const float *mins, const float *maxs, int *entityList, int maxcount ) {
	AreaParms ap;

	ap.mins = mins;
	ap.maxs = maxs;
	ap.list = entityList;
	ap.count = 0;
	ap.maxcount = maxcount;

	if ( numWorldSectors ) {
		AreaEntities_r( worldSectors, &ap );
	}
	return ap.count;
}

/* ---- deep traces: a character's body locations --------------------------- */

/* OpenMoHAA cm_trace_lbd.cpp. A location is a sphere on one of the
 * character's bones, with a second on the same bone for most. */
constexpr int LOCATION_FAIL = -2;
constexpr int MAX_HITLOCATIONS = 19;

const char *const szLocArray[MAX_HITLOCATIONS] = {
	"Bip01 Head", "Bip01 Head", "Bip01 Neck", "Bip01 Spine2", "Bip01 Spine1", "Bip01 Spine",
	"Bip01 Pelvis", "Bip01 R UpperArm", "Bip01 L UpperArm", "Bip01 R Thigh", "Bip01 L Thigh",
	"Bip01 R Forearm", "Bip01 L Forearm", "Bip01 R Calf", "Bip01 L Calf", "Bip01 R Hand",
	"Bip01 L Hand", "Bip01 R Foot", "Bip01 L Foot"
};

const float fLocRadius[MAX_HITLOCATIONS] = {
	5.0f, 5.5f, 4.0f, 9.0f, 8.0f, 9.0f, 9.0f, 7.0f, 7.0f, 8.0f, 8.0f, 5.5f, 5.5f, 7.5f, 7.5f, 6.0f, 6.0f, 6.0f, 6.0f,
};

const vec3_t vLocOffset[MAX_HITLOCATIONS] = {
	{ 2.5f, 3.5f, 0.0f }, { 8.5f, 1.0f, 0.0f }, { 3.5f, -1.0f, 0.0f }, { 5.0f, 1.0f, 3.0f },
	{ 2.0f, 1.0f, 3.0f }, { 1.0f, 1.0f, 3.0f }, { -3.0f, 1.0f, 4.0f }, { 4.0f, 0.0f, 0.0f },
	{ 4.0f, 0.0f, 0.0f }, { 12.0f, 0.0f, 0.0f }, { 12.0f, 0.0f, 0.0f }, { 5.0f, 1.0f, 0.0f },
	{ 5.0f, 1.0f, 0.0f }, { 8.0f, 0.0f, 0.0f }, { 8.0f, 0.0f, 0.0f }, { 5.0f, 1.0f, 0.0f },
	{ 5.0f, 1.0f, 0.0f }, { 3.0f, 2.0f, 0.0f }, { 3.0f, 2.0f, 0.0f },
};

const float fLocRadius_Secondary[MAX_HITLOCATIONS] = {
	0.0f, 5.5f, 4.0f, 9.0f, 8.0f, 9.0f, 9.0f, 6.0f, 6.0f, 8.0f, 8.0f, 5.0f, 5.0f, 6.5f, 6.5f, 0.0f, 0.0f, 5.0f, 5.0f,
};

const vec3_t vLocOffset_Secondary[MAX_HITLOCATIONS] = {
	{ 0.0f, 0.0f, 0.0f }, { 7.0f, -2.5f, 0.0f }, { 2.0f, 2.0f, 0.0f }, { 5.0f, 1.0f, -3.0f },
	{ 2.0f, 1.0f, -3.0f }, { 1.0f, 1.0f, -3.0f }, { -3.0f, 1.0f, -4.0f }, { 11.5f, 0.0f, 0.0f },
	{ 11.5f, 0.0f, 0.0f }, { 22.0f, 0.0f, 0.0f }, { 22.0f, 0.0f, 0.0f }, { 11.5f, 0.0f, 0.0f },
	{ 11.5f, 0.0f, 0.0f }, { 19.0f, 0.0f, 0.0f }, { 19.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f },
	{ 0.0f, 0.0f, 0.0f }, { 4.0f, 9.0f, 0.0f }, { 4.0f, 9.0f, 0.0f },
};

/* Game imports 117 and 118: a location's bone, radius and offset. */
const char *GetHitLocationInfo( int iLocation, float *fRadius, float *vOffset ) {
	if ( iLocation >= 0 && iLocation < MAX_HITLOCATIONS ) {
		*fRadius = fLocRadius[iLocation];
		VectorCopy( vLocOffset[iLocation], vOffset );
		return szLocArray[iLocation];
	}
	*fRadius = 8.0f;
	VectorClear( vOffset );
	return nullptr;
}

const char *GetHitLocationInfoSecondary( int iLocation, float *fRadius, float *vOffset ) {
	if ( iLocation >= 0 && iLocation < MAX_HITLOCATIONS ) {
		*fRadius = fLocRadius_Secondary[iLocation];
		VectorCopy( vLocOffset_Secondary[iLocation], vOffset );
		return szLocArray[iLocation];
	}
	*fRadius = 8.0f;
	VectorClear( vOffset );
	return nullptr;
}

/* A trace in the engine's record plus the two members 2015's adds. */
struct DeepTrace {
	trace_t trace;
	int location = -1;
};

bool LineSegmentToSphereIntersect( trace_t *pTrace, float fRadius, const vec3_t vCntr,
	const vec3_t vStart, const vec3_t vEnd ) {
	vec3_t vDiff, vDir, vProject, vOfs;
	const float fRSqrd = fRadius * fRadius;

	VectorSubtract( vEnd, vStart, vDir );
	const float fLength = VectorNormalize( vDir );

	VectorSubtract( vCntr, vStart, vDiff );
	const float fDSqrd = DotProduct( vDiff, vDir );
	const float fDist = VectorLengthSquared( vDiff );

	if ( fDSqrd < 0 && fDist > fRSqrd ) {
		return false;
	}

	const float fLSqrd = fDist - fDSqrd * fDSqrd;
	if ( fLSqrd > fRSqrd || ( fDSqrd > fLength && fDSqrd - fLength > fRadius ) ) {
		return false;
	}

	vOfs[0] = vStart[0] + vDir[0] * fDSqrd;
	vOfs[1] = vStart[1] + vDir[1] * fDSqrd;
	vOfs[2] = vStart[2] + vDir[2] * fDSqrd;
	VectorSubtract( vCntr, vOfs, vProject );

	const double fOfs = VectorLength( vProject ) / fRadius;
	double fAngSin;

	if ( fOfs >= -0.5 ) {
		if ( fOfs <= 0.5 ) {
			fAngSin = fOfs * ( -0.531387674508458 * fOfs - 2.11e-14 ) + 1.00086138065435;
		} else {
			fAngSin = fOfs
				* ( ( ( ( ( -1507.55394345521 * fOfs + 6580.58002318442 ) * fOfs - 11860.0735285953 ) * fOfs
					+ 11290.7510782536 ) * fOfs - 5986.89654545347 ) * fOfs + 1675.66417006387 )
				- 192.426950291139;
		}
	} else {
		fAngSin = fOfs
			* ( ( ( ( ( -337.31875783205 * fOfs - 1237.54375255107 ) * fOfs - 1802.11467325687 ) * fOfs
				- 1303.19904613494 ) * fOfs - 471.347871690988 ) * fOfs - 70.0883838161826 );
	}

	pTrace->fraction = (float)( ( fDSqrd - fRadius * fAngSin ) / fLength );
	return true;
}

bool TraceDeepSimple( trace_t *results, const vec3_t vEnd, int iBrushMask, int iEntContents,
	const vec3_t vEntMins, const vec3_t vEntMaxs, const vec3_t vEntAngles, vec3_t vTransStart,
	vec3_t vTransEnd ) {
	vec3_t vTemp, vNewMins, vNewMaxs, vForward, vRight, vLeft, vUp;
	const vec3_t vCntr = { 0.0f, 0.0f, 0.0f };

	for ( int i = 0; i < 3; i++ ) {
		vNewMins[i] = vEntMins[i] - 40.0f;
		vNewMaxs[i] = vEntMaxs[i] + 40.0f;
	}

	const clipHandle_t clipHandle = CM_TempBoxModel( vNewMins, vNewMaxs, qfalse );
	CM_SetTempBoxModelContents( iEntContents );
	AngleVectors( vEntAngles, vForward, vRight, vUp );
	VectorNegate( vRight, vLeft );

	VectorCopy( vTransStart, vTemp );
	vTransStart[0] = DotProduct( vTemp, vForward );
	vTransStart[1] = DotProduct( vTemp, vLeft );
	vTransStart[2] = DotProduct( vTemp, vUp );

	VectorCopy( vTransEnd, vTemp );
	vTransEnd[0] = DotProduct( vTemp, vForward );
	vTransEnd[1] = DotProduct( vTemp, vLeft );
	vTransEnd[2] = DotProduct( vTemp, vUp );

	// don't use mins and maxs
	CM_BoxTrace( results, vTransStart, vTransEnd, vCntr, vCntr, clipHandle, iBrushMask, qfalse );
	RestoreBoxContents();

	if ( results->fraction == 1.0f && !results->startsolid && !results->allsolid ) {
		VectorCopy( vEnd, results->endpos );
		return false;
	}
	return true;
}

bool TraceDeepSimple2( trace_t *results, const orientation_t *orPosition, vec3_t vOffset, float fRad,
	int iLocation, const vec3_t vTransStart, const vec3_t vTransEnd ) {
	vec3_t vCntr;

	VectorCopy( orPosition->origin, vCntr );
	for ( int k = 0; k < 3; k++ ) {
		if ( vOffset[k] != 0.0f ) {
			VectorMA( vCntr, vOffset[k], orPosition->axis[k], vCntr );
		}
	}
	if ( LineSegmentToSphereIntersect( results, fRad, vCntr, vTransStart, vTransEnd ) ) {
		return true;
	}

	// the secondary location if any
	if ( !GetHitLocationInfoSecondary( iLocation, &fRad, vOffset ) ) {
		return false;
	}

	VectorCopy( orPosition->origin, vCntr );
	for ( int k = 0; k < 3; k++ ) {
		if ( vOffset[k] != 0.0f ) {
			VectorMA( vCntr, vOffset[k], orPosition->axis[k], vCntr );
		}
	}
	return LineSegmentToSphereIntersect( results, fRad, vCntr, vTransStart, vTransEnd );
}

void TraceDeep( DeepTrace *results, const vec3_t vStart, const vec3_t vEnd, int iBrushMask, GEntity *touch ) {
	vec3_t vTransStart, vTransEnd, vOffset;
	float fRad;

	VectorSubtract( vStart, touch->s.origin, vTransStart );
	VectorSubtract( vEnd, touch->s.origin, vTransEnd );

	// check if something was not hit first
	if ( !TraceDeepSimple( &results->trace, vEnd, iBrushMask, touch->r.contents, touch->r.mins, touch->r.maxs,
		touch->s.angles, vTransStart, vTransEnd ) ) {
		return;
	}

	dtiki_t *tiki = reinterpret_cast<dtiki_t *>( static_cast<std::uintptr_t>( touch->tiki ) );

	// trace through each location
	for ( int iLocation = 0; iLocation < MAX_HITLOCATIONS; iLocation++ ) {
		const char *pszTagName = GetHitLocationInfo( iLocation, &fRad, vOffset );
		const int iBoneNum = MOHAA_TIKI_BoneNumFromName( tiki, pszTagName );
		if ( iBoneNum == -1 ) {
			continue;
		}

		// the orientation comes from the game module
		const orientation_t orPosition = TagOrientation( touch, iBoneNum );

		if ( TraceDeepSimple2( &results->trace, &orPosition, vOffset, fRad, iLocation, vTransStart, vTransEnd ) ) {
			trace_t &t = results->trace;
			for ( int k = 0; k < 3; k++ ) {
				t.endpos[k] = vStart[k] + t.fraction * ( vEnd[k] - vStart[k] );
			}
			t.allsolid = qfalse;
			t.startsolid = qfalse;
			t.contents = touch->r.contents;
			t.entityNum = touch->s.number;
			results->location = iLocation;
			return;
		}
	}

	// fail
	trace_t &t = results->trace;
	t.fraction = 1.0f;
	t.allsolid = qfalse;
	t.startsolid = qfalse;
	VectorCopy( vEnd, t.endpos );
	t.entityNum = ENTITYNUM_NONE;
	results->location = LOCATION_FAIL;
}

/* ---- traces ----------------------------------------------------------- */

struct MoveClip {
	vec3_t boxmins, boxmaxs; // enclose the test object along entire move
	const float *mins;
	const float *maxs; // size of the moving object
	const float *start;
	vec3_t end;
	DeepTrace trace;
	int passEntityNum;
	int contentmask;
	qboolean cylinder;
	qboolean traceDeep;
};

int PassOwner( int passEntityNum ) {
	if ( passEntityNum == ENTITYNUM_NONE ) {
		return -1;
	}
	const GEntity *pass = Entity( passEntityNum );
	const int owner = pass ? pass->r.ownerNum : ENTITYNUM_NONE;
	return owner == ENTITYNUM_NONE ? -1 : owner;
}

void ClipToEntity( trace_t *trace, const vec3_t start, const vec3_t mins, const vec3_t maxs,
	const vec3_t end, int entityNum, int contentmask ) {
	GEntity *touch = Entity( entityNum );

	Com_Memset( trace, 0, sizeof( *trace ) );
	if ( !touch || !( contentmask & touch->r.contents ) ) {
		trace->fraction = 1.0f;
		trace->entityNum = ENTITYNUM_NONE;
		return;
	}

	// might intersect, so do an exact clip
	const clipHandle_t clipHandle = ClipHandleForEntity( touch );
	CM_TransformedBoxTrace( trace, start, end, mins, maxs, clipHandle, contentmask, touch->s.origin,
		touch->r.currentAngles, qfalse );
	RestoreBoxContents();

	if ( trace->fraction < 1 ) {
		trace->entityNum = touch->s.number;
	}
}

void ClipMoveToEntities( MoveClip *clip ) {
	static int touchlist[MAX_GENTITIES];
	const int num = AreaEntities( clip->boxmins, clip->boxmaxs, touchlist, MAX_GENTITIES );
	const int passOwnerNum = PassOwner( clip->passEntityNum );

	for ( int i = 0; i < num; i++ ) {
		if ( clip->trace.trace.allsolid ) {
			return;
		}
		GEntity *touch = Entity( touchlist[i] );
		if ( !touch ) {
			continue;
		}

		// skip non-solids and triggers
		if ( touch->solid == SOLID_NOT || touch->solid == SOLID_TRIGGER ) {
			continue;
		}

		// see if we should ignore this entity
		if ( clip->passEntityNum != ENTITYNUM_NONE ) {
			if ( touchlist[i] == clip->passEntityNum ) {
				continue; // don't clip against the pass entity
			}
			if ( touchlist[i] == passOwnerNum ) {
				continue; // don't clip against the owner of this entity
			}
			if ( touch->r.ownerNum == clip->passEntityNum ) {
				continue; // don't clip against own missiles
			}
			if ( touch->r.ownerNum == passOwnerNum ) {
				continue; // don't clip against other missiles from our owner
			}
		}

		// if it doesn't have any brushes of a type we
		// are looking for, ignore it
		if ( !( clip->contentmask & touch->r.contents ) ) {
			continue;
		}

		DeepTrace trace;
		dtiki_t *tiki = reinterpret_cast<dtiki_t *>( static_cast<std::uintptr_t>( touch->tiki ) );
		if ( clip->traceDeep && tiki && MOHAA_TIKI_IsCharacter( tiki ) ) {
			// an extended trace with an output location
			TraceDeep( &trace, clip->start, clip->end, clip->contentmask, touch );
		} else {
			// might intersect, so do an exact clip
			const clipHandle_t clipHandle = ClipHandleForEntity( touch );
			CM_TransformedBoxTrace( &trace.trace, clip->start, clip->end, clip->mins, clip->maxs, clipHandle,
				clip->contentmask, touch->s.origin, touch->r.currentAngles, clip->cylinder );
			RestoreBoxContents();
		}

		if ( trace.trace.allsolid ) {
			clip->trace.trace.allsolid = qtrue;
			clip->trace.trace.entityNum = touch->s.number;
			trace.trace.entityNum = touch->s.number;
		} else if ( trace.trace.startsolid ) {
			clip->trace.trace.startsolid = qtrue;
			clip->trace.trace.entityNum = touch->s.number;
			trace.trace.entityNum = touch->s.number;
		}

		if ( trace.trace.fraction < clip->trace.trace.fraction ) {
			// make sure we keep a startsolid from a previous trace
			const qboolean oldStart = clip->trace.trace.startsolid;

			trace.trace.entityNum = touch->s.number;
			clip->trace = trace;
			clip->trace.trace.startsolid = ( clip->trace.trace.startsolid || oldStart ) ? qtrue : qfalse;
		}
	}
}

void BoundMove( MoveClip &clip, const vec3_t start, const vec3_t end ) {
	// the bounding box of the entire move
	for ( int i = 0; i < 3; i++ ) {
		if ( end[i] > start[i] ) {
			clip.boxmins[i] = clip.start[i] + clip.mins[i] - 1;
			clip.boxmaxs[i] = clip.end[i] + clip.maxs[i] + 1;
		} else {
			clip.boxmins[i] = clip.end[i] + clip.mins[i] - 1;
			clip.boxmaxs[i] = clip.start[i] + clip.maxs[i] + 1;
		}
	}
}

void Trace( DeepTrace *results, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end,
	int passEntityNum, int contentmask, qboolean cylinder, qboolean traceDeep ) {
	MoveClip clip{};
	clip.trace.location = -1;

	// clip to world
	CM_BoxTrace( &clip.trace.trace, start, end, mins, maxs, 0, contentmask, cylinder );
	clip.trace.trace.entityNum = clip.trace.trace.fraction != 1.0f ? ENTITYNUM_WORLD : ENTITYNUM_NONE;
	if ( clip.trace.trace.fraction == 0 ) {
		*results = clip.trace;
		return; // blocked immediately by the world
	}

	clip.contentmask = contentmask;
	clip.start = start;
	VectorCopy( end, clip.end );
	clip.mins = mins;
	clip.maxs = maxs;
	clip.passEntityNum = passEntityNum;
	clip.cylinder = cylinder;
	clip.traceDeep = traceDeep;
	BoundMove( clip, start, end );

	// clip to other solid entities
	ClipMoveToEntities( &clip );
	*results = clip.trace;
}

/* A sight trace answers only whether the segment is clear. fnTech3's
 * collision has no early-out sight form, so a full trace stands in for it:
 * the answer is the same, found more slowly. */
bool WorldSightClear( const vec3_t start, const vec3_t end, const vec3_t mins, const vec3_t maxs,
	clipHandle_t model, int contentmask, qboolean cylinder ) {
	trace_t trace;
	CM_BoxTrace( &trace, start, end, mins, maxs, model, contentmask, cylinder );
	return trace.fraction >= 1.0f && !trace.startsolid && !trace.allsolid;
}

bool EntitySightClear( const vec3_t start, const vec3_t end, const vec3_t mins, const vec3_t maxs,
	GEntity *touch, int contentmask, qboolean cylinder ) {
	trace_t trace;
	const clipHandle_t clipHandle = ClipHandleForEntity( touch );
	CM_TransformedBoxTrace( &trace, start, end, mins, maxs, clipHandle, contentmask, touch->s.origin,
		touch->r.currentAngles, cylinder );
	RestoreBoxContents();
	return trace.fraction >= 1.0f && !trace.startsolid && !trace.allsolid;
}

/* Game import 62: false if something was hit. */
qboolean SightTraceEntity( GEntity *touch, const vec3_t start, const vec3_t mins, const vec3_t maxs,
	const vec3_t end, int contentmask, qboolean cylinder ) {
	return EntitySightClear( start, end, mins, maxs, touch, contentmask, cylinder ) ? qtrue : qfalse;
}

/* Game import 63: false if something was hit. */
qboolean SightTrace( const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end,
	int passEntityNum, int passEntityNum2, int contentmask, qboolean cylinder ) {
	if ( !WorldSightClear( start, end, mins, maxs, 0, contentmask, cylinder ) ) {
		return qfalse;
	}

	MoveClip clip{};
	clip.contentmask = contentmask;
	clip.start = start;
	VectorCopy( end, clip.end );
	clip.mins = mins;
	clip.maxs = maxs;
	clip.passEntityNum = passEntityNum;
	clip.cylinder = cylinder;
	BoundMove( clip, start, end );

	static int touchlist[MAX_GENTITIES];
	const int num = AreaEntities( clip.boxmins, clip.boxmaxs, touchlist, MAX_GENTITIES );
	const int passOwnerNum = PassOwner( passEntityNum );
	const int passOwnerNum2 = PassOwner( passEntityNum2 );

	for ( int i = 0; i < num; i++ ) {
		GEntity *touch = Entity( touchlist[i] );
		if ( !touch ) {
			continue;
		}

		// skip non-solids and triggers
		if ( touch->solid == SOLID_NOT || touch->solid == SOLID_TRIGGER ) {
			continue;
		}

		// see if we should ignore this entity
		if ( passEntityNum != ENTITYNUM_NONE ) {
			if ( touchlist[i] == passEntityNum || touchlist[i] == passOwnerNum
				|| touch->r.ownerNum == passEntityNum || touch->r.ownerNum == passOwnerNum ) {
				continue;
			}
		}
		if ( passEntityNum2 != ENTITYNUM_NONE ) {
			if ( touchlist[i] == passEntityNum2 || touchlist[i] == passOwnerNum2
				|| touch->r.ownerNum == passEntityNum2 ) {
				continue;
			}
		}

		// if it doesn't have any brushes of a type we
		// are looking for, ignore it
		if ( !( contentmask & touch->r.contents ) ) {
			continue;
		}

		if ( !EntitySightClear( start, end, mins, maxs, touch, contentmask, cylinder ) ) {
			return qfalse;
		}
	}
	return qtrue;
}

/* Game import 66. */
int PointContents( const vec3_t p, int passEntityNum ) {
	// get base contents from world
	int contents = CM_PointContents( p, 0 );

	// or in contents from all the other entities
	static int touch[MAX_GENTITIES];
	const int num = AreaEntities( p, p, touch, MAX_GENTITIES );

	for ( int i = 0; i < num; i++ ) {
		if ( touch[i] == passEntityNum ) {
			continue;
		}
		GEntity *hit = Entity( touch[i] );
		if ( !hit ) {
			continue;
		}
		// might intersect, so do an exact clip
		const clipHandle_t clipHandle = ClipHandleForEntity( hit );
		contents |= CM_TransformedPointContents( p, clipHandle, hit->s.origin, hit->s.angles );
		RestoreBoxContents();
	}
	return contents;
}

/* 2015's trace record (code/compat/mohaa_game_abi.h), filled from the
 * engine's. The hit shader's number is not kept by fnTech3's collision; the
 * modules read it only to archive a trace and to name a surface in a debug
 * command, and are handed 0, the value a trace that hit nothing carries.
 * The hit entity's record is the game's to fill: its own trace wrapper
 * points it at the entity numbered here. */
void ToModuleTrace( const DeepTrace &in, fntech3::mohaa::Trace *out ) {
	Com_Memset( out, 0, sizeof( *out ) );
	out->allsolid = in.trace.allsolid;
	out->startsolid = in.trace.startsolid;
	out->fraction = in.trace.fraction;
	VectorCopy( in.trace.endpos, out->endpos );
	VectorCopy( in.trace.plane.normal, out->plane.normal );
	out->plane.dist = in.trace.plane.dist;
	out->plane.type = in.trace.plane.type;
	out->plane.signbits = in.trace.plane.signbits;
	out->surfaceFlags = in.trace.surfaceFlags;
	out->shaderNum = 0;
	out->contents = in.trace.contents;
	out->entityNum = in.trace.entityNum;
	out->location = in.location;
	out->ent = 0;
}

} // namespace mohaa_world

#endif
