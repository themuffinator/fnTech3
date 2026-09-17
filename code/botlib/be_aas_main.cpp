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
 * name:		be_aas_main.c
 *
 * desc:		AAS
 *
 * $Archive: /MissionPack/code/botlib/be_aas_main.c $
 *
 *****************************************************************************/

#include "../qcommon/q_shared.h"
#include "../qcommon/game_profile.h"
#include "rtcw_navigation.h"
#include "l_memory.h"
#include "l_libvar.h"
#include "l_utils.h"
#include "l_script.h"
#include "l_precomp.h"
#include "l_struct.h"
#include "l_log.h"
#include "aasfile.h"
#include "botlib.h"
#include "be_aas.h"
#include "wolfet_navigation.h"
#include "be_aas_funcs.h"
#include "be_interface.h"
#include "be_aas_def.h"

static aas_t aasWorlds[2];
aas_t *aas_currentWorld = &aasWorlds[0];

static aas_rtcw_cast_callbacks_t rtcwCastCallbacks;

void AAS_SetRTCWCastCallbacks(const aas_rtcw_cast_callbacks_t &callbacks)
{
	rtcwCastCallbacks = Com_GameIsRTCW() ? callbacks : aas_rtcw_cast_callbacks_t{};
}

const aas_rtcw_cast_callbacks_t &AAS_RTCWCastCallbacks()
{
	return rtcwCastCallbacks;
}

aas_t &AAS_EntityWorld()
{
	return Com_GameIsRTCW() ? aasWorlds[0] : aasworld;
}

bool AAS_SetCurrentWorld( int index )
{
	if ( index < 0 || index >= Com_GameProfile()->bots.navigationWorlds
		|| index >= static_cast<int>( ARRAY_LEN( aasWorlds ) ) ) return false;
	aas_currentWorld = &aasWorlds[index];
	return true;
}

/* ET's own AAS_SetAASBlockingEntity. It differs from both RTCW bodies: the
 * reset sentinel is a negative value, a blocked area keeps every caller bit
 * except the inverted low one, and BLOCKINGFLAG_MOVER restricts the block to
 * mover areas and retries unrestricted when no area matched. */
void AAS_WolfETSetBlockingEntity( const float *mins, const float *maxs, int blocking )
{
	// ET's BLOCKINGFLAG_MOVER, spelled ~0x7fffffff in its game headers.
	constexpr int blockingFlagMover = static_cast<int>( 0x80000000u );
	const bool samePoint = mins[0] == maxs[0] && mins[1] == maxs[1] && mins[2] == maxs[2];

	if ( samePoint && blocking < 0 ) {
		for ( int world = 0; AAS_SetCurrentWorld( world ); ++world ) {
			if ( !aasworld.loaded ) continue;
			for ( int area = 1; area < aasworld.numareas; ++area ) AAS_EnableRoutingArea( area, 1 );
		}
		return;
	}

	bool mover = ( blocking & blockingFlagMover ) != 0;
	if ( mover ) blocking &= ~blockingFlagMover;
	const int enable = ( blocking & ~0x1 ) | ( ( blocking & 1 ) ? 0 : 1 );

	for ( ;; ) {
		bool changed = false;
		for ( int world = 0; AAS_SetCurrentWorld( world ); ++world ) {
			if ( !aasworld.loaded ) continue;
			vec3_t lower, upper;
			VectorCopy( mins, lower );
			VectorCopy( maxs, upper );
			int areas[1024];
			const int count = AAS_BBoxAreas( lower, upper, areas, ARRAY_LEN( areas ) );
			for ( int i = 0; i < count; ++i ) {
				if ( areas[i] < 0 || areas[i] >= aasworld.numareasettings ) continue;
				if ( mover && !( aasworld.areasettings[areas[i]].contents & AREACONTENTS_MOVER ) ) continue;
				AAS_EnableRoutingArea( areas[i], enable );
				changed = true;
			}
		}
		// The original retries once with the mover restriction removed.
		if ( !mover || changed ) return;
		mover = false;
	}
}

bool AAS_WolfETAreaCenter( int area, float *center )
{
	if ( area < 0 || area >= aasworld.numareas || !aasworld.areas || !center ) return false;
	VectorCopy( aasworld.areas[area].center, center );
	return true;
}

int AAS_WolfETAreaLadder( int area )
{
	if ( area < 0 || area >= aasworld.numareasettings || !aasworld.areasettings ) return 0;
	return aasworld.areasettings[area].areaflags & AREA_LADDER;
}

bool AAS_WolfETAreaWaypoint( int area, float * )
{
	/* ET stores one waypoint per area in its route table and answers false
	 * whenever that table is absent. This engine builds no waypoint table, so
	 * the honest answer is always the original's table-absent answer. */
	(void)area;
	return false;
}

void AAS_SetBlockingEntity( const float *mins, const float *maxs, int blocking )
{
	const bool samePoint = mins[0] == maxs[0] && mins[1] == maxs[1] && mins[2] == maxs[2];
	// Original SP uses a signed reset sentinel; original MP uses false.
	const bool reset = samePoint && ( Com_GameIsRTCWSP() ? blocking < 0 : blocking == 0 );
	for ( int world = 0; AAS_SetCurrentWorld( world ); ++world ) {
		if ( !aasworld.loaded ) continue;
		if ( reset ) {
			for ( int area = 1; area < aasworld.numareas; ++area ) AAS_EnableRoutingArea( area, 1 );
		} else {
			vec3_t lower, upper;
			VectorCopy( mins, lower );
			VectorCopy( maxs, upper );
			int areas[128];
			const int count = AAS_BBoxAreas( lower, upper, areas, ARRAY_LEN( areas ) );
			for ( int i = 0; i < count; ++i ) AAS_EnableRoutingArea( areas[i], !blocking );
		}
	}
}

libvar_t *saveroutingcache;

//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
void QDECL AAS_Error(const char *fmt, ...)
{
	char str[1024];
	va_list arglist;

	va_start(arglist, fmt);
	Q_vsnprintf(str, sizeof(str), fmt, arglist);
	va_end(arglist);
	botimport.Print(PRT_FATAL, "%s", str);
} //end of the function AAS_Error
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
int AAS_Loaded(void)
{
	return aasworld.loaded;
} //end of the function AAS_Loaded
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
int AAS_Initialized(void)
{
	return aasworld.initialized;
} //end of the function AAS_Initialized
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
static void AAS_SetInitialized(void)
{
	aasworld.initialized = qtrue;
	botimport.Print(PRT_MESSAGE, "AAS initialized.\n");
#ifdef DEBUG
	//create all the routing cache
	//AAS_CreateAllRoutingCache();
	//
	//AAS_RoutingInfo();
#endif
} //end of the function AAS_SetInitialized
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
void AAS_ContinueInit(float time)
{
	//if no AAS file loaded
	if (!aasworld.loaded) return;
	//if AAS is already initialized
	if (aasworld.initialized) return;
	if (Com_GameIsRTCW()) {
		if (!AAS_InitRTCWRouting()) {
			botimport.Print(PRT_EXIT, "RTCW routing initialization failed for %s\n", aasworld.mapname);
			return;
		}
		AAS_SetInitialized();
		return;
	}
	//calculate reachability, if not finished return
	if (AAS_ContinueInitReachability(time)) return;
	//initialize clustering for the new map
	AAS_InitClustering();
	//if reachability has been calculated and an AAS file should be written
	//or there is a forced data optimization
	if (aasworld.savefile || ((int)LibVarGetValue("forcewrite")))
	{
		//optimize the AAS data
		if ((int)LibVarValue("aasoptimize", "0")) AAS_Optimize();
		//save the AAS file
		if (AAS_WriteAASFile(aasworld.filename))
		{
			botimport.Print(PRT_MESSAGE, "%s written successfully\n", aasworld.filename);
		} //end if
		else
		{
			botimport.Print(PRT_ERROR, "couldn't write %s\n", aasworld.filename);
		} //end else
	} //end if
	//initialize the routing
	AAS_InitRouting();
	//at this point AAS is initialized
	AAS_SetInitialized();
} //end of the function AAS_ContinueInit
//===========================================================================
// called at the start of every frame
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
int AAS_StartFrame(float time)
{
	if (Com_GameIsRTCW()) {
		for (int world = 0; AAS_SetCurrentWorld(world); ++world) {
			aasworld.time = time;
			AAS_InvalidateEntities();
			AAS_ContinueInit(time);
			aasworld.frameroutingupdates = 0;
		}
		// Original RTCW leaves world one selected and advances only its counter.
		if (aasworld.numframes < INT_MAX) ++aasworld.numframes;
		return BLERR_NOERROR;
	}
	aasworld.time = time;
	//unlink all entities that were not updated last frame
	AAS_UnlinkInvalidEntities();
	//invalidate the entities
	AAS_InvalidateEntities();
	//initialize AAS
	AAS_ContinueInit(time);
	//
	aasworld.frameroutingupdates = 0;
	//
	if (botDeveloper)
	{
		if (LibVarGetValue("showcacheupdates"))
		{
			AAS_RoutingInfo();
			LibVarSet("showcacheupdates", "0");
		} //end if
		if (LibVarGetValue("showmemoryusage"))
		{
			PrintUsedMemorySize();
			LibVarSet("showmemoryusage", "0");
		} //end if
		if (LibVarGetValue("memorydump"))
		{
			PrintMemoryLabels();
			LibVarSet("memorydump", "0");
		} //end if
	} //end if
	//
	if (saveroutingcache->value)
	{
		AAS_WriteRouteCache();
		LibVarSet("saveroutingcache", "0");
	} //end if
	//
	aasworld.numframes++;
	return BLERR_NOERROR;
} //end of the function AAS_StartFrame
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
float AAS_Time(void)
{
	return aasworld.time;
} //end of the function AAS_Time
//===========================================================================
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//===========================================================================
void AAS_ProjectPointOntoVector( vec3_t point, vec3_t vStart, vec3_t vEnd, vec3_t vProj )
{
	vec3_t pVec, vec;

	VectorSubtract( point, vStart, pVec );
	VectorSubtract( vEnd, vStart, vec );
	VectorNormalize( vec );
	// project onto the directional vector for this segment
	VectorMA( vStart, DotProduct( pVec, vec ), vec, vProj );
} //end of the function AAS_ProjectPointOntoVector
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
static int AAS_LoadFiles(const char *mapname)
{
	int errnum;
	char aasfile[MAX_PATH];
//	char bspfile[MAX_PATH];

	Q_strncpyz(aasworld.mapname, mapname, sizeof(aasworld.mapname));
	//NOTE: first reset the entity links into the AAS areas and BSP leaves
	// the AAS link heap and BSP link heap are reset after respectively the
	// AAS file and BSP file are loaded
	AAS_ResetEntityLinks();
	// load bsp info
	AAS_LoadBSPFile();

	//load the aas file
	Com_sprintf(aasfile, sizeof(aasfile), "maps/%s.aas", mapname);
	errnum = AAS_LoadAASFile(aasfile);
	if (errnum != BLERR_NOERROR)
		return errnum;

	botimport.Print(PRT_MESSAGE, "loaded %s\n", aasfile);
	Q_strncpyz( aasworld.filename, aasfile, sizeof( aasworld.filename ) );
	return BLERR_NOERROR;
} //end of the function AAS_LoadFiles

static bool AAS_ValidRTCWMapName(const char *name)
{
	// maps/ + name + _bN.aas + terminator must fit each world's filename.
	if (!name || !*name || strlen(name) + 13 > sizeof(aasworld.filename)) return false;
	const char *component = name;
	for (const char *p = name; ; ++p) {
		if (*p == '\\' || *p == ':' || (*p && static_cast<unsigned char>(*p) < 32)) return false;
		if (*p && *p != '/') continue;
		const auto length = p - component;
		if (!length || (length == 1 && *component == '.')
			|| (length == 2 && component[0] == '.' && component[1] == '.')) return false;
		if (!*p) return true;
		component = p + 1;
	}
}

static int AAS_LoadRTCWMap(const char *mapname)
{
	if (!AAS_ValidRTCWMapName(mapname)) {
		botimport.Print(PRT_ERROR, "Invalid RTCW navigation map name\n");
		return BLERR_CANNOTOPENAASFILE;
	}
	// One physical BSP and one entity table serve both navigation hulls.
	// Retire every old world before exposing any part of the new map.
	AAS_SetCurrentWorld(0);
	AAS_ResetEntityLinks();
	for (int world = 0; AAS_SetCurrentWorld(world); ++world) {
		AAS_FreeRoutingCaches();
		AAS_ShutdownAlternativeRouting();
		AAS_FreeAASLinkedEntities();
		AAS_FreeAASLinkHeap();
		AAS_DumpAASData();
		aasworld.filename[0] = aasworld.mapname[0] = '\0';
		aasworld.numreachabilityareas = 0;
	}
	const int bspError = AAS_LoadBSPFile();
	if (bspError != BLERR_NOERROR) return bspError;
	bool loaded = false;
	int error = BLERR_CANNOTOPENAASFILE;
	for (int world = 0; AAS_SetCurrentWorld(world); ++world) {
		char file[MAX_PATH];
		Com_sprintf(aasworld.mapname, sizeof(aasworld.mapname), "%s_b%d", mapname, world);
		Com_sprintf(file, sizeof(file), "maps/%s.aas", aasworld.mapname);
		const int result = AAS_LoadAASFile(file);
		if (result != BLERR_NOERROR) { error = result; continue; }
		Q_strncpyz(aasworld.filename, file, sizeof(aasworld.filename));
		AAS_InitSettings();
		AAS_InitAASLinkHeap();
		AAS_InitAASLinkedEntities();
		AAS_InitAlternativeRouting();
		// These files already contain RTCW reachabilities. Quake's generator
		// must not replace them with routes using another game's movement rules.
		aasworld.numreachabilityareas = aasworld.numareas + 2;
		loaded = true;
		botimport.Print(PRT_MESSAGE, "loaded %s (AAS8, %d areas, %d reachabilities)\n",
			file, aasworld.numareas, aasworld.reachabilitysize);
	}
	// Original SP/MP accept either hull and leave the final world selected.
	return loaded ? BLERR_NOERROR : error;
}
//===========================================================================
// called every time a map changes
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
int AAS_LoadMap(const char *mapname)
{
	int	errnum;

	//if no mapname is provided then the string indexes are updated
	if (!mapname)
	{
		return 0;
	} //end if
	if (Com_GameIsRTCW()) return AAS_LoadRTCWMap(mapname);
	//
	aasworld.initialized = qfalse;
	//NOTE: free the routing caches before loading a new map because
	// to free the caches the old number of areas, number of clusters
	// and number of areas in a clusters must be available
	AAS_FreeRoutingCaches();
	//load the map
	errnum = AAS_LoadFiles(mapname);
	if (errnum != BLERR_NOERROR)
	{
		aasworld.loaded = qfalse;
		return errnum;
	} //end if
	//
	AAS_InitSettings();
	//initialize the AAS link heap for the new map
	AAS_InitAASLinkHeap();
	//initialize the AAS linked entities for the new map
	AAS_InitAASLinkedEntities();
	//initialize reachability for the new map
	AAS_InitReachability();
	//initialize the alternative routing
	AAS_InitAlternativeRouting();
	//everything went ok
	return 0;
} //end of the function AAS_LoadMap
//===========================================================================
// called when the library is first loaded
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
int AAS_Setup(void)
{
	AAS_SetCurrentWorld( 0 );
	aasworld.maxclients = (int) LibVarValue("maxclients", "128");
	aasworld.maxentities = (int) LibVarValue("maxentities", "1024");
	// as soon as it's set to 1 the routing cache will be saved
	saveroutingcache = LibVar("saveroutingcache", "0");
	//allocate memory for the entities
	if (aasworld.entities) FreeMemory(aasworld.entities);
	aasworld.entities = nullptr;
	aasworld.entities = botlib::ClearedHunkAlloc<aas_entity_t>(aasworld.maxentities);
	//invalidate all the entities
	AAS_InvalidateEntities();
	//force some recalculations
	//LibVarSet("forceclustering", "1");			//force clustering calculation
	//LibVarSet("forcereachability", "1");		//force reachability calculation
	aasworld.numframes = 0;
	return BLERR_NOERROR;
} //end of the function AAS_Setup
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
static void AAS_ShutdownWorld(void)
{
	AAS_ShutdownAlternativeRouting();
	//free routing caches
	AAS_FreeRoutingCaches();
	//free aas link heap
	AAS_FreeAASLinkHeap();
	//free aas linked entities
	AAS_FreeAASLinkedEntities();
	//free the aas data
	AAS_DumpAASData();
	//free the entities
	if (aasworld.entities) FreeMemory(aasworld.entities);
	//clear the aasworld structure
	aasworld = {};
	//aas has not been initialized
	aasworld.initialized = qfalse;
	//NOTE: as soon as a new .bsp file is loaded the .bsp file memory is
	// freed and reallocated, so there's no need to free that memory here
	//print shutdown
} //end of the function AAS_Shutdown

void AAS_Shutdown(void)
{
	// BSP entities describe the one physical map shared by navigation worlds.
	AAS_DumpBSPData();
	for ( int world = 0; AAS_SetCurrentWorld( world ); ++world ) AAS_ShutdownWorld();
	AAS_SetCurrentWorld( 0 );
	saveroutingcache = nullptr;
	botimport.Print(PRT_MESSAGE, "AAS shutdown.\n");
}
