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
// sv_world.cpp -- world query functions

#include "server.h"
#include "../qcommon/cm_model_handles.h"
#include "sv_collision_model.hpp"
#include "server_world_bounds.hpp"
#include "../compat/q3_ihv_collision.hpp"
#include "../compat/stvef_game_abi.h"

#include <algorithm>
#include <array>
#include <limits>

/*
================
SV_ClipHandleForEntity

Returns a headnode that can be used for testing or clipping to a
given entity.  If the entity is a bsp model, the headnode will
be returned, otherwise a custom box tree will be constructed.
================
*/
clipHandle_t SV_ClipHandleForEntity( const sharedEntity_t *ent, qboolean capsule ) {
	sharedEntity_t entity;
	if ( !SV_ReadGameEntity( ent, &entity ) ) {
		Com_Error( ERR_DROP, "%s: bad gEnt", __func__ );
		return 0;
	}
	if ( entity.r.bmodel ) {
		// explicit hulls in the BSP model
		return CM_InlineModel( entity.s.modelindex );
	}
	if ( fnql::server::collision::UseCapsuleEntityModel(
		Com_GameProfile()->entityHullPolicy, SV_AsBool( capsule ),
		( entity.r.svFlags & SVF_CAPSULE ) != 0 ) ) {
		// create a temp capsule from bounding box sizes
		return CM_TempBoxModel( entity.r.mins, entity.r.maxs, qtrue );
	}

	// create a temp tree from bounding box sizes
	return CM_TempBoxModel( entity.r.mins, entity.r.maxs, qfalse );
}



/*
===============================================================================

ENTITY CHECKING

To avoid linearly searching through lists of entities during environment testing,
the world is carved up with an evenly spaced, axially aligned bsp tree.  Entities
are kept in chains either at the final leafs, or at the first node that splits
them, which prevents having to deal with multiple fragments of a single entity.

===============================================================================
*/

struct worldSector_s {
	int axis = -1;		// -1 = leaf node
	float dist = 0.0f;
	std::array<worldSector_s *, 2> children{};
	svEntity_t *entities = nullptr;
};
using worldSector_t = worldSector_s;

static constexpr int AREA_DEPTH = 4;
static constexpr int AREA_NODES = 64;

static std::array<worldSector_t, AREA_NODES> sv_worldSectors{};
static int			sv_numworldSectors;


/*
===============
SV_SectorList_f
===============
*/
void SV_SectorList_f( void ) {
	int				i, c;
	worldSector_t	*sec;
	svEntity_t		*ent;

	for ( i = 0 ; i < SV_ArraySize(sv_worldSectors) ; i++ ) {
		sec = &sv_worldSectors[i];

		c = 0;
		for ( ent = sec->entities ; ent ; ent = ent->nextEntityInWorldSector ) {
			c++;
		}
		Com_Printf( "sector %i: %i entities\n", i, c );
	}
}

/*
===============
SV_CreateworldSector

Builds a uniformly subdivided tree for the given world size
===============
*/
static worldSector_t *SV_CreateworldSector( int depth, vec3_t mins, vec3_t maxs ) {
	worldSector_t	*anode;
	vec3_t		size;
	vec3_t		mins1, maxs1, mins2, maxs2;

	if ( sv_numworldSectors >= SV_ArraySize(sv_worldSectors) ) {
		Com_Error( ERR_DROP, "SV_CreateworldSector: AREA_NODES overflow" );
	}
	anode = &sv_worldSectors[sv_numworldSectors];
	sv_numworldSectors++;

	if (depth == AREA_DEPTH) {
		anode->axis = -1;
		anode->children[0] = anode->children[1] = nullptr;
		return anode;
	}
	
	VectorSubtract (maxs, mins, size);
	if (size[0] > size[1]) {
		anode->axis = 0;
	} else {
		anode->axis = 1;
	}

	anode->dist = 0.5 * (maxs[anode->axis] + mins[anode->axis]);
	VectorCopy (mins, mins1);	
	VectorCopy (mins, mins2);	
	VectorCopy (maxs, maxs1);	
	VectorCopy (maxs, maxs2);	
	
	maxs1[anode->axis] = mins2[anode->axis] = anode->dist;
	
	anode->children[0] = SV_CreateworldSector (depth+1, mins2, maxs2);
	anode->children[1] = SV_CreateworldSector (depth+1, mins1, maxs1);

	return anode;
}

/*
===============
SV_ClearWorld

===============
*/
void SV_ClearWorld( void ) {
	clipHandle_t	h;
	vec3_t			mins, maxs;

	sv_worldSectors = {};
	sv_numworldSectors = 0;
	// Drop both sides of the old linkage while preserving snapshot baselines.
	for ( svEntity_t &entity : sv.svEntities ) {
		entity.worldSector = nullptr;
		entity.nextEntityInWorldSector = nullptr;
		entity.wolfetOriginCluster = -1;
	}

	// get world map bounds
	h = CM_InlineModel( 0 );
	CM_ModelBounds( h, mins, maxs );
	SV_CreateworldSector( 0, mins, maxs );
}


/*
===============
SV_UnlinkEntity

===============
*/
void SV_UnlinkEntity( sharedEntity_t *gEnt ) {
	svEntity_t		*ent;
	svEntity_t		*scan;
	worldSector_t	*ws;
	sharedEntity_t entity;

	ent = SV_SvEntityForGentity( gEnt );

	if ( !SV_ReadGameEntity( gEnt, &entity ) ) {
		Com_Error( ERR_DROP, "%s: bad gEnt", __func__ );
		return;
	}
	entity.r.linked = qfalse;
	if ( !SV_WriteGameEntity( gEnt, &entity ) ) {
		Com_Error( ERR_DROP, "%s: game-data write failed", __func__ );
		return;
	}

	ws = ent->worldSector;
	if ( !ws ) {
		return;		// not linked in anywhere
	}
	ent->worldSector = nullptr;

	if ( ws->entities == ent ) {
		ws->entities = ent->nextEntityInWorldSector;
		return;
	}

	for ( scan = ws->entities ; scan ; scan = scan->nextEntityInWorldSector ) {
		if ( scan->nextEntityInWorldSector == ent ) {
			scan->nextEntityInWorldSector = ent->nextEntityInWorldSector;
			return;
		}
	}

	Com_Printf( "WARNING: SV_UnlinkEntity: not found in worldSector\n" );
}


/*
===============
SV_LinkEntity

===============
*/
static constexpr int MAX_TOTAL_ENT_LEAFS = 128;

/*
===============
SV_LinkQuake3IHVClusters

Quake III IHV records an entity's clusters the way Quake II does: deduplicated
cluster numbers, or the first node the box straddles once it reaches
MAX_TOTAL_ENT_LEAFS leafs or more than MAX_ENT_CLUSTERS clusters. The other
profiles keep Quake III's list plus its overflow cluster. Original
0x41bce6..0x41bd72.
===============
*/
static void SV_LinkQuake3IHVClusters( svEntity_t *ent, const int *leafs, int num_leafs, int topnode ) {
	std::array<int, MAX_TOTAL_ENT_LEAFS> clusters{};
	int i, j;

	if ( num_leafs >= MAX_TOTAL_ENT_LEAFS ) {
		ent->numClusters = -1;
		ent->lastCluster = topnode;
		return;
	}
	for ( i = 0; i < num_leafs; i++ ) {
		clusters[i] = CM_LeafCluster( leafs[i] );
	}
	ent->numClusters = 0;
	for ( i = 0; i < num_leafs; i++ ) {
		if ( clusters[i] == -1 ) {
			continue;
		}
		// The original compares against every earlier leaf's cluster, not
		// against the clusters it has already stored.
		for ( j = 0; j < i; j++ ) {
			if ( clusters[j] == clusters[i] ) {
				break;
			}
		}
		if ( j != i ) {
			continue;
		}
		if ( ent->numClusters == MAX_ENT_CLUSTERS ) {
			ent->numClusters = -1;
			ent->lastCluster = topnode;
			return;
		}
		ent->clusternums[ent->numClusters++] = clusters[i];
	}
}

/*
===============
SV_LinkEntityClusters

The part of linking the snapshot walk reads: which areas and which PVS
clusters an absolute box touches. The entity's record is cleared and filled
from the leafs the box reaches; the return is how many leafs that was, and
zero means the box lies outside the world. SV_LinkEntity uses it, and so does
American McGee's Alice's linker, whose entity record is not a sharedEntity_t.
===============
*/
int SV_LinkEntityClusters( svEntity_t *ent, const vec3_t absmin, const vec3_t absmax, int number ) {
	std::array<int, MAX_TOTAL_ENT_LEAFS> leafs{};
	int			cluster;
	int			num_leafs;
	int			i;
	int			area;
	int			lastLeaf = 0;
	int			topnode = -1;

	ent->numClusters = 0;
	ent->lastCluster = 0;
	ent->areanum = -1;
	ent->areanum2 = -1;

	//get all leafs, including solids
	num_leafs = Com_GameIsQuake3IHV()
		? CM_BoxLeafnumsTopnode( absmin, absmax,
			leafs.data(), SV_ArraySize(leafs), &topnode )
		: CM_BoxLeafnums( absmin, absmax,
			leafs.data(), SV_ArraySize(leafs), &lastLeaf );

	// if none of the leafs were inside the map, the
	// entity is outside the world and can be considered unlinked
	if ( !num_leafs ) {
		return 0;
	}

	// set areas, even from clusters that don't fit in the entity array
	for (i=0 ; i<num_leafs ; i++) {
		area = CM_LeafArea (leafs[i]);
		if (area != -1) {
			// doors may legally straggle two areas,
			// but nothing should ever need more than that
			if (ent->areanum != -1 && ent->areanum != area) {
				if (ent->areanum2 != -1 && ent->areanum2 != area && sv.state == SS_LOADING) {
					Com_DPrintf ("Object %i touching 3 areas at %f %f %f\n",
					number,
					absmin[0], absmin[1], absmin[2]);
				}
				ent->areanum2 = area;
			} else {
				ent->areanum = area;
			}
		}
	}

	if ( Com_GameIsQuake3IHV() ) {
		SV_LinkQuake3IHVClusters( ent, leafs.data(), num_leafs, topnode );
	} else {
		// store as many explicit clusters as we can
		ent->numClusters = 0;
		for (i=0 ; i < num_leafs ; i++) {
			cluster = CM_LeafCluster( leafs[i] );
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
	}
	return num_leafs;
}

void SV_LinkEntity( sharedEntity_t *gEnt ) {
	worldSector_t	*node;
	int			num_leafs;
	int			i, j, k;
	float		*origin, *angles;
	svEntity_t	*ent;
	sharedEntity_t entity;

	ent = SV_SvEntityForGentity( gEnt );

	if ( !SV_ReadGameEntity( gEnt, &entity ) ) {
		Com_Error( ERR_DROP, "%s: bad gEnt", __func__ );
		return;
	}
	const bool ihv = Com_GameProfile()->id == COM_GAME_QUAKE3_IHV;
	origin = entity.r.currentOrigin;
	angles = entity.r.currentAngles;
	if ( ihv && !( entity.r.svFlags & SVF_USE_CURRENT_ORIGIN ) ) {
		origin = entity.s.origin;
		angles = entity.s.angles;
	}
	if ( !SV_ValidWorldBounds( entity.r.mins, entity.r.maxs )
		|| !SV_FiniteWorldVector( origin )
		|| !SV_FiniteWorldVector( angles ) ) {
		Com_Error( ERR_DROP, "%s: invalid entity bounds", __func__ );
		return;
	}
	fntech3::q3ihv::LinkBounds ihvBounds;
	if ( ihv && !fntech3::q3ihv::CalculateLinkBounds( entity.r.mins, entity.r.maxs,
		origin, angles, entity.r.bmodel != 0, static_cast<uint32_t>( entity.r.contents ), ihvBounds ) ) {
		Com_Error( ERR_DROP, "%s: IHV entity bounds overflow", __func__ );
		return;
	}

	// encode the size into the entityState_t for client prediction
	if ( ihv ) {
		entity.s.solid = ihvBounds.solid;
	} else if ( entity.r.bmodel ) {
		entity.s.solid = SOLID_BMODEL;		// a solid_box will never create this value
	} else if ( Com_GameIsSTVEFMP() && ( entity.r.svFlags & SVF_STVEF_SHIELD_BBOX ) ) {
		// Elite Force shields are thin boxes: s.solid carries the half-width
		// along their wide axis and the flag says which axis that is. Retail
		// stvoyHM.exe (SV_LinkEntity 0x436550) tests this before contents,
		// takes a 4-unit X extent to mean a Y-facing shield, and caps the
		// height at 254.
		const bool alongY = entity.r.maxs[0] == 4.0f;
		i = static_cast<int>( std::clamp( entity.r.maxs[alongY ? 1 : 0], 1.0f, 255.0f ) );
		j = static_cast<int>( std::clamp( -entity.r.mins[alongY ? 1 : 0], 1.0f, 255.0f ) );
		k = static_cast<int>( std::clamp( entity.r.maxs[2], 1.0f, 254.0f ) );
		entity.s.eFlags |= alongY ? FNTECH3_STVEF_MP_EF_SHIELD_BOX_Y : FNTECH3_STVEF_MP_EF_SHIELD_BOX_X;
		entity.s.solid = (k<<16) | (j<<8) | i;
	} else if ( entity.r.contents & ( CONTENTS_SOLID | CONTENTS_BODY ) ) {
		// assume that x/y are equal and symmetric
		i = static_cast<int>( std::clamp( entity.r.maxs[0], 1.0f, 255.0f ) );
		// z is not symmetric
		j = static_cast<int>( std::clamp( -entity.r.mins[2], 1.0f, 255.0f ) );
		// and z maxs can be negative...
		k = static_cast<int>( std::clamp( entity.r.maxs[2] + 32, 1.0f, 255.0f ) );

		entity.s.solid = (k<<16) | (j<<8) | i;
	} else {
		entity.s.solid = 0;
	}

	// set the abs box
	if ( ihv ) {
		VectorCopy( ihvBounds.mins, entity.r.absmin );
		VectorCopy( ihvBounds.maxs, entity.r.absmax );
	} else if ( entity.r.bmodel && (angles[0] || angles[1] || angles[2]) ) {
		// expand for rotation
		float		max;

		max = RadiusFromBounds( entity.r.mins, entity.r.maxs );
		for (i=0 ; i<3 ; i++) {
			entity.r.absmin[i] = origin[i] - max;
			entity.r.absmax[i] = origin[i] + max;
		}
	} else {
		// normal
		VectorAdd (origin, entity.r.mins, entity.r.absmin);
		VectorAdd (origin, entity.r.maxs, entity.r.absmax);
	}

	// because movement is clipped an epsilon away from an actual edge,
	// we must fully check even when bounding boxes don't quite touch
	if ( !ihv ) {
		entity.r.absmin[0] -= 1;
		entity.r.absmin[1] -= 1;
		entity.r.absmin[2] -= 1;
		entity.r.absmax[0] += 1;
		entity.r.absmax[1] += 1;
		entity.r.absmax[2] += 1;
	}
	if ( !SV_ValidWorldBounds( entity.r.absmin, entity.r.absmax ) ) {
		Com_Error( ERR_DROP, "%s: entity bounds overflow", __func__ );
		return;
	}
	// Validate the new position before removing the previous world linkage.
	if ( ent->worldSector ) SV_UnlinkEntity( gEnt );
	entity.r.linked = qfalse;

	ent->wolfetOriginCluster = Com_GameIsWolfET() && entity.r.bmodel
		? CM_LeafCluster( CM_PointLeafnum( entity.r.currentOrigin ) ) : -1;

	// link to PVS leafs
	num_leafs = SV_LinkEntityClusters( ent, entity.r.absmin, entity.r.absmax, entity.s.number );

	// if none of the leafs were inside the map, the
	// entity is outside the world and can be considered unlinked
	if ( !num_leafs ) {
		if ( !SV_WriteGameEntity( gEnt, &entity ) ) {
			Com_Error( ERR_DROP, "%s: game-data write failed", __func__ );
		}
		return;
	}

	entity.r.linkcount = entity.r.linkcount == ( std::numeric_limits<int>::max )()
		? ( std::numeric_limits<int>::min )() : entity.r.linkcount + 1;

	// find the first world sector node that the ent's box crosses
	node = sv_worldSectors.data();
	while ( true )
	{
		if (node->axis == -1)
			break;
		if ( entity.r.absmin[node->axis] > node->dist)
			node = node->children[0];
		else if ( entity.r.absmax[node->axis] < node->dist)
			node = node->children[1];
		else
			break;		// crosses the node
	}
	
	entity.r.linked = qtrue;
	if ( !SV_WriteGameEntity( gEnt, &entity ) ) {
		Com_Error( ERR_DROP, "%s: game-data write failed", __func__ );
		return;
	}

	// link it in
	ent->worldSector = node;
	ent->nextEntityInWorldSector = node->entities;
	node->entities = ent;

}

/*
============================================================================

AREA QUERY

Fills in a list of all entities who's absmin / absmax intersects the given
bounds.  This does NOT mean that they actually touch in the case of bmodels.
============================================================================
*/

struct areaParms_t {
	const float	*mins;
	const float	*maxs;
	int			*list;
	int			count, maxcount;
};


/*
====================
SV_AreaEntities_r

====================
*/
static void SV_AreaEntities_r( worldSector_t *node, areaParms_t &ap ) {
	svEntity_t	*check, *next;
	sharedEntity_t entity;

	for ( check = node->entities  ; check ; check = next ) {
		next = check->nextEntityInWorldSector;

		if ( !SV_ReadGameEntityByNum( SV_SvEntityIndex( check ), &entity ) ) {
			Com_Error( ERR_DROP, "%s: invalid world entity", __func__ );
			return;
		}

		if ( entity.r.absmin[0] > ap.maxs[0]
		|| entity.r.absmin[1] > ap.maxs[1]
		|| entity.r.absmin[2] > ap.maxs[2]
		|| entity.r.absmax[0] < ap.mins[0]
		|| entity.r.absmax[1] < ap.mins[1]
		|| entity.r.absmax[2] < ap.mins[2]) {
			continue;
		}

		if ( ap.count == ap.maxcount ) {
			Com_Printf ("SV_AreaEntities: MAXCOUNT\n");
			return;
		}

		ap.list[ap.count] = SV_SvEntityIndex( check );
		ap.count++;
	}
	
	if (node->axis == -1) {
		return;		// terminal node
	}

	// recurse down both sides
	if ( ap.maxs[node->axis] > node->dist ) {
		SV_AreaEntities_r ( node->children[0], ap );
	}
	if ( ap.mins[node->axis] < node->dist ) {
		SV_AreaEntities_r ( node->children[1], ap );
	}
}

/*
================
SV_AreaEntities
================
*/
int SV_AreaEntities( const vec3_t mins, const vec3_t maxs, int *entityList, int maxcount ) {
	areaParms_t		ap{ mins, maxs, entityList, 0, maxcount };

	SV_AreaEntities_r( sv_worldSectors.data(), ap );

	return ap.count;
}



//===========================================================================


struct moveclip_t {
	vec3_t		boxmins, boxmaxs;// enclose the test object along entire move
	const float	*mins;
	const float *maxs;	// size of the moving object
	const float	*start;
	vec3_t		end;
	trace_t		trace;
	int			passEntityNum;
	int			contentmask;
	bool		capsule;
};


/*
====================
SV_ClipToEntity

====================
*/
void SV_ClipToEntity( trace_t *trace, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, int entityNum, int contentmask, qboolean capsule ) {
	sharedEntity_t	*touch;
	sharedEntity_t	entity;
	clipHandle_t	clipHandle;
	float			*origin;
	const float *angles;

	touch = SV_GentityNum( entityNum );
	if ( !SV_ReadGameEntity( touch, &entity ) ) {
		Com_Error( ERR_DROP, "%s: invalid gentity %i", __func__, entityNum );
		return;
	}

	*trace = {};

	// if it doesn't have any brushes of a type we
	// are looking for, ignore it
	if ( ! ( contentmask & entity.r.contents ) ) {
		trace->fraction = 1.0;
		return;
	}

	// might intersect, so do an exact clip
	clipHandle = SV_ClipHandleForEntity( touch, capsule );

	origin = entity.r.currentOrigin;
	angles = entity.r.currentAngles;

	if ( !entity.r.bmodel ) {
		angles = vec3_origin;	// boxes don't rotate
	}

	CM_TransformedBoxTrace ( trace, start, end,
		mins, maxs, clipHandle,  contentmask,
		origin, angles, capsule);

	if ( trace->fraction < 1 ) {
		trace->entityNum = entity.s.number;
	}
}


/*
====================
SV_ClipMoveToEntities

====================
*/
static void SV_ClipMoveToEntities( moveclip_t &clip ) {
	int			i, num;
	// deliberately uninitialized: SV_AreaEntities() fills [0,count) and returns
	// count, which bounds every read below. Value-initializing 1024 ints on
	// every trace is pure memset in the hottest server path.
	std::array<int, MAX_GENTITIES> touchlist;
	sharedEntity_t *touch;
	sharedEntity_t entity;
	int			passOwnerNum;
	trace_t		trace;
	clipHandle_t	clipHandle;
	float		*origin;
	const float *angles;
	const bool ihv = Com_GameProfile()->id == COM_GAME_QUAKE3_IHV;
	const bool checkOwner = ihv ? clip.passEntityNum > 0 : clip.passEntityNum != ENTITYNUM_NONE;

	num = SV_AreaEntities( clip.boxmins, clip.boxmaxs, touchlist.data(), SV_ArraySize(touchlist) );

	if ( checkOwner ) {
		if ( !SV_ReadGameEntityByNum( clip.passEntityNum, &entity ) ) {
			Com_Error( ERR_DROP, "%s: invalid pass entity %i", __func__,
				clip.passEntityNum );
			return;
		}
		passOwnerNum = entity.r.ownerNum;
		if ( !ihv && passOwnerNum == ENTITYNUM_NONE ) {
			passOwnerNum = -1;
		}
	} else {
		passOwnerNum = -1;
	}

	for ( i=0 ; i<num ; i++ ) {
		if ( clip.trace.allsolid ) {
			return;
		}
		touch = SV_GentityNum( touchlist[i] );
		if ( !SV_ReadGameEntity( touch, &entity ) ) {
			Com_Error( ERR_DROP, "%s: invalid gentity %i", __func__,
				touchlist[i] );
			return;
		}

		// see if we should ignore this entity
		if ( ihv ) {
			// Original 0x41c002 compares the native state's signed number.
			// Positive pass values also exclude its owner and entities owned
			// by it; unlike retail Q3, siblings sharing an owner still collide.
			if ( entity.s.number == clip.passEntityNum ) continue;
			if ( checkOwner ) {
				if ( entity.r.ownerNum >= 0 ) {
					sharedEntity_t owner;
					if ( !SV_ReadGameEntityByNum( entity.r.ownerNum, &owner ) ) {
						Com_Error( ERR_DROP, "%s: invalid IHV owner %i", __func__, entity.r.ownerNum );
						return;
					}
					if ( owner.s.number == clip.passEntityNum ) continue;
				}
				if ( touchlist[i] == passOwnerNum ) continue;
			}
		} else if ( checkOwner ) {
			if ( touchlist[i] == clip.passEntityNum ) {
				continue;	// don't clip against the pass entity
			}
			if ( entity.r.ownerNum == clip.passEntityNum ) {
				continue;	// don't clip against own missiles
			}
			if ( entity.r.ownerNum == passOwnerNum ) {
				continue;	// don't clip against other missiles from our owner
			}
		}

		// if it doesn't have any brushes of a type we
		// are looking for, ignore it
		if ( ! ( clip.contentmask & entity.r.contents ) ) {
			continue;
		}

		origin = entity.r.currentOrigin;
		angles = entity.r.currentAngles;
		if ( ihv && !( entity.r.svFlags & SVF_USE_CURRENT_ORIGIN ) ) {
			origin = entity.s.origin;
			angles = entity.s.angles;
		}
		if ( !entity.r.bmodel ) {
			angles = vec3_origin;	// boxes don't rotate
		}
		if ( ihv && ( !SV_ValidWorldBounds( entity.r.mins, entity.r.maxs )
			|| !SV_FiniteWorldVector( origin ) || !SV_FiniteWorldVector( angles ) ) ) {
			Com_Error( ERR_DROP, "%s: invalid IHV entity geometry", __func__ );
			return;
		}

		// might intersect, so do an exact clip
		clipHandle = SV_ClipHandleForEntity( touch, SV_QBool( clip.capsule ) );

		// RTCW MP and ET sweep a box entity as its own contents. The box gets
		// CONTENTS_BODY back once this entity's result is merged, as in retail,
		// on every way out of this iteration.
		const bool entityBoxContents = fnql::server::collision::UseEntityBoxContents(
			Com_GameProfile()->entityBoxContentsPolicy,
			clipHandle == CM_TemporaryModelHandle( qfalse ) );
		if ( entityBoxContents ) {
			CM_SetTempBoxModelContents( entity.r.contents );
		}
		const auto restoreBoxContents = SV_MakeScopeExit( [entityBoxContents]() {
			if ( entityBoxContents ) {
				CM_SetTempBoxModelContents( CONTENTS_BODY );
			}
		} );

		CM_TransformedBoxTrace ( &trace, clip.start, clip.end,
			clip.mins, clip.maxs, clipHandle,  clip.contentmask,
			origin, angles, SV_QBool( clip.capsule ) );

		if ( ihv ) {
			// Original 0x41c0aa..0x41c0dc replaces the whole record, including
			// startsolid, end position and plane, on any of these conditions.
			if ( trace.allsolid || trace.startsolid || trace.fraction < clip.trace.fraction ) {
				trace.entityNum = entity.s.number;
				clip.trace = trace;
			}
			continue;
		}

		if ( trace.allsolid ) {
			clip.trace.allsolid = qtrue;
			trace.entityNum = entity.s.number;
		} else if ( trace.startsolid ) {
			clip.trace.startsolid = qtrue;
			trace.entityNum = entity.s.number;
		}

		if ( trace.fraction < clip.trace.fraction ) {
			// make sure we keep a startsolid from a previous trace
			const bool oldStart = SV_AsBool( clip.trace.startsolid );

			trace.entityNum = entity.s.number;
			clip.trace = trace;
			if ( oldStart ) {
				clip.trace.startsolid = qtrue;
			}
		}
	}
}


/*
==================
SV_Trace

Moves the given mins/maxs volume through the world from start to end.
passEntityNum and entities owned by passEntityNum are explicitly not checked.
==================
*/
void SV_Trace( trace_t *results, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, int passEntityNum, int contentmask, qboolean capsule ) {
	moveclip_t	clip{};
	int			i;
	const bool ihv = Com_GameProfile()->id == COM_GAME_QUAKE3_IHV;

	if ( !mins ) {
		mins = vec3_origin;
	}
	if ( !maxs ) {
		maxs = vec3_origin;
	}
	if ( ihv ) {
		fntech3::q3ihv::SweepBounds bounds;
		if ( !results || !fntech3::q3ihv::CalculateSweepBounds( start, mins, maxs, end, bounds ) ) {
			Com_Error( ERR_DROP, "%s: invalid IHV sweep bounds", __func__ );
			return;
		}
		VectorCopy( bounds.mins, clip.boxmins );
		VectorCopy( bounds.maxs, clip.boxmaxs );
	}

	// clip to world
	CM_BoxTrace( &clip.trace, start, end, mins, maxs, 0, contentmask, capsule );
	clip.trace.entityNum = ihv ? 0 : clip.trace.fraction != 1.0 ? ENTITYNUM_WORLD : ENTITYNUM_NONE;
	// ET uses -2 for world-only traces (original server/sv_world.c).
	if ( clip.trace.fraction == 0 || ( Com_GameIsWolfET() && passEntityNum == -2 ) ) {
		*results = clip.trace;
		return;		// blocked immediately by the world
	}

	clip.contentmask = contentmask;
	clip.start = start;
//	VectorCopy( clip.trace.endpos, clip.end );
	VectorCopy( end, clip.end );
	clip.mins = mins;
	clip.maxs = maxs;
	clip.passEntityNum = passEntityNum;
	clip.capsule = SV_AsBool( capsule );

	// create the bounding box of the entire move
	// we can limit it to the part of the move not
	// already clipped off by the world, which can be
	// a significant savings for line of sight and shot traces
	for ( i=0 ; !ihv && i<3 ; i++ ) {
		if ( end[i] > start[i] ) {
			clip.boxmins[i] = clip.start[i] + clip.mins[i] - 1;
			clip.boxmaxs[i] = clip.end[i] + clip.maxs[i] + 1;
		} else {
			clip.boxmins[i] = clip.end[i] + clip.mins[i] - 1;
			clip.boxmaxs[i] = clip.start[i] + clip.maxs[i] + 1;
		}
	}

	// clip to other solid entities
	SV_ClipMoveToEntities ( clip );

	*results = clip.trace;
}



/*
=============
SV_PointContents
=============
*/
int SV_PointContents( const vec3_t p, int passEntityNum ) {
	// see SV_ClipMoveToEntities(): write-before-read, bounded by the return value
	std::array<int, MAX_GENTITIES> touch;
	sharedEntity_t *hit;
	sharedEntity_t entity;
	int			i, num;
	int			contents, c2;
	clipHandle_t	clipHandle;
	const float		*angles;
	const float		*origin;
	const bool ihv = Com_GameProfile()->id == COM_GAME_QUAKE3_IHV;
	if ( ihv && !SV_FiniteWorldVector( p ) ) {
		Com_Error( ERR_DROP, "%s: invalid IHV point", __func__ );
		return 0;
	}

	// get base contents from world
	contents = CM_PointContents( p, 0 );

	// or in contents from all the other entities
	num = SV_AreaEntities( p, p, touch.data(), SV_ArraySize(touch) );

	for ( i=0 ; i<num ; i++ ) {
		if ( !ihv && touch[i] == passEntityNum ) {
			continue;
		}
		hit = SV_GentityNum( touch[i] );
		if ( !SV_ReadGameEntity( hit, &entity ) ) {
			Com_Error( ERR_DROP, "%s: invalid gentity %i", __func__, touch[i] );
			return contents;
		}
		if ( ihv && entity.s.number == passEntityNum ) continue;
		// Original point queries always use state.origin/state.angles,
		// including boxes and entities with SVF_USE_CURRENT_ORIGIN set.
		origin = ihv ? entity.s.origin : entity.r.currentOrigin;
		angles = ihv ? entity.s.angles : entity.r.currentAngles;
		if ( !ihv && !entity.r.bmodel ) {
			angles = vec3_origin;	// boxes don't rotate
		}
		if ( ihv && ( !SV_ValidWorldBounds( entity.r.mins, entity.r.maxs )
			|| !SV_FiniteWorldVector( origin )
			|| ( entity.r.bmodel && !SV_FiniteWorldVector( angles ) ) ) ) {
			Com_Error( ERR_DROP, "%s: invalid IHV entity geometry", __func__ );
			return contents;
		}
		// might intersect, so do an exact clip
		clipHandle = SV_ClipHandleForEntity( hit, qfalse );

		c2 = CM_TransformedPointContents (p, clipHandle,
			origin, angles);

		contents |= c2;
	}

	return contents;
}


