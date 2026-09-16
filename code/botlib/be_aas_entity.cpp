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

/*****************************************************************************
 * name:		be_aas_entity.c
 *
 * desc:		AAS entities
 *
 * $Archive: /MissionPack/code/botlib/be_aas_entity.c $
 *
 *****************************************************************************/

#include "../qcommon/q_shared.h"
#include "../qcommon/game_profile.h"
#include "../compat/rtcw_bot_entity.hpp"
#include "l_memory.h"
#include "l_script.h"
#include "l_precomp.h"
#include "l_struct.h"
#include "l_utils.h"
#include "l_log.h"
#include "aasfile.h"
#include "botlib.h"
#include "be_aas.h"
#include "be_aas_funcs.h"
#include "be_interface.h"
#include "be_aas_def.h"

// Each shared entity retains the owner of its links when world selection changes.
static void AAS_UnlinkEntityAreas(aas_entity_t *entity)
{
	auto *selected = aas_currentWorld;
	if (entity->areaWorld) aas_currentWorld = entity->areaWorld;
	AAS_UnlinkFromAreas(entity->areas);
	aas_currentWorld = selected;
	entity->areas = nullptr;
	entity->areaWorld = nullptr;
}

//FIXME: these might change
enum {
	ET_GENERAL,
	ET_PLAYER,
	ET_ITEM,
	ET_MISSILE,
	ET_MOVER
};

//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
int AAS_UpdateEntity(int entnum, bot_entitystate_t *state)
{
	auto &entityWorld = AAS_EntityWorld();
	int relink;
	aas_entity_t *ent;
	vec3_t absmins, absmaxs;

	if (!entityWorld.loaded)
	{
		botimport.Print(PRT_MESSAGE, "AAS_UpdateEntity: not loaded\n");
		return BLERR_NOAASFILE;
	} //end if

	if (entnum < 0 || entnum >= entityWorld.maxentities || !entityWorld.entities)
		return BLERR_INVALIDENTITYNUMBER;
	ent = &entityWorld.entities[entnum];

	if (!state) {
		//unlink the entity
		AAS_UnlinkEntityAreas(ent);
		//unlink the entity from the BSP leaves
		AAS_UnlinkFromBSPLeaves(ent->leaves);
		//
		ent->areas = nullptr;
		//
		ent->leaves = nullptr;
		return BLERR_NOERROR;
	}

	const bool rtcw = Com_GameIsRTCW() != 0;
	aas_entityinfo_t pending = ent->i;
	auto &info = rtcw ? pending : ent->i;
	info.update_time = AAS_Time() - info.ltime;
	info.type = state->type;
	info.flags = state->flags;
	info.ltime = AAS_Time();
	VectorCopy(info.origin, info.lastvisorigin);
	VectorCopy(state->old_origin, info.old_origin);
	info.solid = state->solid;
	info.groundent = state->groundent;
	info.modelindex = state->modelindex;
	info.modelindex2 = state->modelindex2;
	info.frame = state->frame;
	if (!rtcw) info.event = state->event;
	info.eventParm = state->eventParm;
	info.powerups = state->powerups;
	info.weapon = state->weapon;
	info.legsAnim = state->legsAnim;
	info.torsoAnim = state->torsoAnim;
	//number of the entity
	info.number = entnum;
	//updated so set valid flag
	info.valid = qtrue;
	//link everything the first frame
	if (entityWorld.numframes == 1) relink = qtrue;
	else relink = qfalse;
	//
	if (info.solid == SOLID_BSP)
	{
		//if the angles of the model changed
		if (!VectorCompare(state->angles, info.angles))
		{
			VectorCopy(state->angles, info.angles);
			relink = qtrue;
		} //end if
		//get the mins and maxs of the model
		//FIXME: rotate mins and maxs
		AAS_BSPModelMinsMaxsOrigin(info.modelindex, info.angles, info.mins, info.maxs, nullptr);
	} //end if
	else if (info.solid == SOLID_BBOX)
	{
		//if the bounding box size changed
		if (!VectorCompare(state->mins, info.mins) ||
				!VectorCompare(state->maxs, info.maxs))
		{
			VectorCopy(state->mins, info.mins);
			VectorCopy(state->maxs, info.maxs);
			relink = qtrue;
		} //end if
		if (!rtcw) VectorCopy(state->angles, info.angles);
	} //end if
	//if the origin changed
	if (!VectorCompare(state->origin, info.origin))
	{
		VectorCopy(state->origin, info.origin);
		relink = qtrue;
	} //end if
	if (rtcw) {
		// BSP model bounds are computed above. Reject them before publishing
		// the new record or detaching any links from their owning world.
		if (!fntech3::rtcw::bot::ValidBounds(info.origin, info.mins, info.maxs))
			Com_Error(ERR_DROP, "Invalid RTCW navigation entity bounds");
		ent->i = pending;
	}
	//if the entity should be relinked
	if (relink)
	{
		//don't link the world model
		if (entnum != ENTITYNUM_WORLD)
		{
			//absolute mins and maxs
			VectorAdd(info.mins, info.origin, absmins);
			VectorAdd(info.maxs, info.origin, absmaxs);
			//unlink the entity
			AAS_UnlinkEntityAreas(ent);
			//relink the entity to the AAS areas (use the larges bbox)
			ent->areas = AAS_LinkEntityClientBBox(absmins, absmaxs, entnum, PRESENCE_NORMAL);
			ent->areaWorld = ent->areas ? aas_currentWorld : nullptr;
			//unlink the entity from the BSP leaves
			AAS_UnlinkFromBSPLeaves(ent->leaves);
			//link the entity to the world BSP tree
			ent->leaves = AAS_BSPLinkEntity(absmins, absmaxs, entnum, 0);
		} //end if
	} //end if
	return BLERR_NOERROR;
} //end of the function AAS_UpdateEntity
//===========================================================================
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//===========================================================================
void AAS_EntityInfo(int entnum, aas_entityinfo_t *info)
{
	auto &entityWorld = AAS_EntityWorld();
	if (!entityWorld.initialized)
	{
		botimport.Print(PRT_FATAL, "AAS_EntityInfo: aasworld not initialized\n");
		*info = {};
		return;
	} //end if

	if (entnum < 0 || entnum >= entityWorld.maxentities)
	{
		botimport.Print(PRT_FATAL, "AAS_EntityInfo: entnum %d out of range\n", entnum);
		*info = {};
		return;
	} //end if

	*info = entityWorld.entities[entnum].i;
} //end of the function AAS_EntityInfo
#if 0
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
void AAS_EntityOrigin(int entnum, vec3_t origin)
{
	auto &entityWorld = AAS_EntityWorld();
	if (entnum < 0 || entnum >= entityWorld.maxentities)
	{
		botimport.Print(PRT_FATAL, "AAS_EntityOrigin: entnum %d out of range\n", entnum);
		VectorClear(origin);
		return;
	} //end if

	VectorCopy(entityWorld.entities[entnum].i.origin, origin);
} //end of the function AAS_EntityOrigin
#endif
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
int AAS_EntityModelindex(int entnum)
{
	auto &entityWorld = AAS_EntityWorld();
	if (entnum < 0 || entnum >= entityWorld.maxentities)
	{
		botimport.Print(PRT_FATAL, "AAS_EntityModelindex: entnum %d out of range\n", entnum);
		return 0;
	} //end if
	return entityWorld.entities[entnum].i.modelindex;
} //end of the function AAS_EntityModelindex
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
int AAS_EntityType(int entnum)
{
	auto &entityWorld = AAS_EntityWorld();
	if (!entityWorld.initialized) return 0;

	if (entnum < 0 || entnum >= entityWorld.maxentities)
	{
		botimport.Print(PRT_FATAL, "AAS_EntityType: entnum %d out of range\n", entnum);
		return 0;
	} //end if
	return entityWorld.entities[entnum].i.type;
} //end of the AAS_EntityType
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
int AAS_EntityModelNum(int entnum)
{
	auto &entityWorld = AAS_EntityWorld();
	if (!entityWorld.initialized) return 0;

	if (entnum < 0 || entnum >= entityWorld.maxentities)
	{
		botimport.Print(PRT_FATAL, "AAS_EntityModelNum: entnum %d out of range\n", entnum);
		return 0;
	} //end if
	return entityWorld.entities[entnum].i.modelindex;
} //end of the function AAS_EntityModelNum
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
int AAS_OriginOfMoverWithModelNum(int modelnum, vec3_t origin)
{
	auto &entityWorld = AAS_EntityWorld();
	int i;
	aas_entity_t *ent;

	for (i = 0; i < entityWorld.maxentities; i++)
	{
		ent = &entityWorld.entities[i];
		if (ent->i.type == ET_MOVER)
		{
			if (ent->i.modelindex == modelnum)
			{
				VectorCopy(ent->i.origin, origin);
				return qtrue;
			} //end if
		} //end if
	} //end for
	return qfalse;
} //end of the function AAS_OriginOfMoverWithModelNum
#if 0
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
void AAS_EntitySize(int entnum, vec3_t mins, vec3_t maxs)
{
	auto &entityWorld = AAS_EntityWorld();
	aas_entity_t *ent;

	if (!entityWorld.initialized) return;

	if (entnum < 0 || entnum >= entityWorld.maxentities)
	{
		botimport.Print(PRT_FATAL, "AAS_EntitySize: entnum %d out of range\n", entnum);
		return;
	} //end if

	ent = &entityWorld.entities[entnum];
	VectorCopy(ent->i.mins, mins);
	VectorCopy(ent->i.maxs, maxs);
} //end of the function AAS_EntitySize
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
void AAS_EntityBSPData(int entnum, bsp_entdata_t *entdata)
{
	auto &entityWorld = AAS_EntityWorld();
	aas_entity_t *ent;

	ent = &entityWorld.entities[entnum];
	VectorCopy(ent->i.origin, entdata->origin);
	VectorCopy(ent->i.angles, entdata->angles);
	VectorAdd(ent->i.origin, ent->i.mins, entdata->absmins);
	VectorAdd(ent->i.origin, ent->i.maxs, entdata->absmaxs);
	entdata->solid = ent->i.solid;
	entdata->modelnum = ent->i.modelindex - 1;
} //end of the function AAS_EntityBSPData
#endif
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
void AAS_ResetEntityLinks(void)
{
	auto &entityWorld = AAS_EntityWorld();
	int i;
	for (i = 0; i < entityWorld.maxentities; i++)
	{
		entityWorld.entities[i].areas = nullptr;
		entityWorld.entities[i].areaWorld = nullptr;
		entityWorld.entities[i].leaves = nullptr;
	} //end for
} //end of the function AAS_ResetEntityLinks
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
void AAS_InvalidateEntities(void)
{
	auto &entityWorld = AAS_EntityWorld();
	int i;
	for (i = 0; i < entityWorld.maxentities; i++)
	{
		entityWorld.entities[i].i.valid = qfalse;
		entityWorld.entities[i].i.number = i;
	} //end for
} //end of the function AAS_InvalidateEntities
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
void AAS_UnlinkInvalidEntities(void)
{
	auto &entityWorld = AAS_EntityWorld();
	int i;
	aas_entity_t *ent;

	for (i = 0; i < entityWorld.maxentities; i++)
	{
		ent = &entityWorld.entities[i];
		if (!ent->i.valid)
		{
			AAS_UnlinkEntityAreas(ent);
			ent->areas = nullptr;
			AAS_UnlinkFromBSPLeaves( ent->leaves );
			ent->leaves = nullptr;
		} //end for
	} //end for
} //end of the function AAS_UnlinkInvalidEntities
#if 0
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
int AAS_NearestEntity(vec3_t origin, int modelindex)
{
	auto &entityWorld = AAS_EntityWorld();
	int i, bestentnum;
	float dist, bestdist;
	aas_entity_t *ent;
	vec3_t dir;

	bestentnum = 0;
	bestdist = 99999;
	for (i = 0; i < entityWorld.maxentities; i++)
	{
		ent = &entityWorld.entities[i];
		if (ent->i.modelindex != modelindex) continue;
		VectorSubtract(ent->i.origin, origin, dir);
		if (fabs(dir[0]) < 40)
		{
			if (fabs(dir[1]) < 40)
			{
				dist = VectorLength(dir);
				if (dist < bestdist)
				{
					bestdist = dist;
					bestentnum = i;
				} //end if
			} //end if
		} //end if
	} //end for
	return bestentnum;
} //end of the function AAS_NearestEntity
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
static int AAS_BestReachableEntityArea(int entnum)
{
	auto &entityWorld = AAS_EntityWorld();
	aas_entity_t *ent;

	ent = &entityWorld.entities[entnum];
	return AAS_BestReachableLinkArea(ent->areas);
} //end of the function AAS_BestReachableEntityArea
#endif
//===========================================================================
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//===========================================================================
int AAS_NextEntity(int entnum)
{
	auto &entityWorld = AAS_EntityWorld();
	if (!entityWorld.loaded) return 0;

	if (entnum >= entityWorld.maxentities - 1) return 0;
	if (entnum < 0) entnum = -1;
	while(++entnum < entityWorld.maxentities)
	{
		if (entityWorld.entities[entnum].i.valid) return entnum;
	} //end while
	return 0;
} //end of the function AAS_NextEntity
