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
 * name:		be_aas_route.c
 *
 * desc:		AAS
 *
 * $Archive: /MissionPack/code/botlib/be_aas_route.c $
 *
 *****************************************************************************/

#include <algorithm>

#include "../qcommon/q_shared.h"
#include "../qcommon/game_profile.h"
#include "../compat/rtcw_routing.hpp"
#include "../compat/wolfet_navigation.hpp"
#include "l_utils.h"
#include "l_memory.h"
#include "l_log.h"
#include "l_crc.h"
#include "l_libvar.h"
#include "l_script.h"
#include "l_precomp.h"
#include "l_struct.h"
#include "aasfile.h"
#include "botlib.h"
#include "be_aas.h"
#include "be_aas_funcs.h"
#include "be_interface.h"
#include "be_aas_def.h"
#include "be_ai_move.h"
#include "rtcw_navigation.h"
#include "../compat/rtcw_routecache_format.hpp"

#define ROUTING_DEBUG

//travel time in hundredths of a second = distance * 100 / speed
static constexpr float DISTANCEFACTOR_CROUCH = 1.3f; //crouch speed = 100
static constexpr float DISTANCEFACTOR_SWIM = 1.0f; //should be 0.66, swim speed = 150
static constexpr float DISTANCEFACTOR_WALK = 0.33f; //walk speed = 300

//cache refresh time
[[maybe_unused]] static constexpr float CACHE_REFRESHTIME = 15.0f; //15 seconds refresh time

//maximum number of routing updates each frame
[[maybe_unused]] static constexpr int MAX_FRAMEROUTINGUPDATES = 10;


/*

  area routing cache:
  stores the distances within one cluster to a specific goal area
  this goal area is in this same cluster and could be a cluster portal
  for every cluster there's a list with routing cache for every area
  in that cluster (including the portals of that cluster)
  area cache stores aasworld.clusters[?].numreachabilityareas travel times

  portal routing cache:
  stores the distances of all portals to a specific goal area
  this goal area could be in any cluster and could also be a cluster portal
  for every area (aasworld.numareas) the portal cache stores
  aasworld.numportals travel times

*/

#ifdef ROUTING_DEBUG
int numareacacheupdates;
int numportalcacheupdates;
#endif //ROUTING_DEBUG


//===========================================================================
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//===========================================================================
#ifdef ROUTING_DEBUG
void AAS_RoutingInfo(void)
{
	botimport.Print(PRT_MESSAGE, "%d area cache updates\n", numareacacheupdates);
	botimport.Print(PRT_MESSAGE, "%d portal cache updates\n", numportalcacheupdates);
	botimport.Print(PRT_MESSAGE, "%d bytes routing cache\n", aasworld.routingcachesize);
} //end of the function AAS_RoutingInfo
#endif //ROUTING_DEBUG
//===========================================================================
// returns the number of the area in the cluster
// assumes the given area is in the given cluster or a portal of the cluster
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//===========================================================================
static ID_INLINE int AAS_ClusterAreaNum(int cluster, int areanum)
{
	int side, areacluster;

	areacluster = aasworld.areasettings[areanum].cluster;
	if (areacluster > 0) return aasworld.areasettings[areanum].clusterareanum;
	else
	{
/*#ifdef ROUTING_DEBUG
		if (aasworld.portals[-areacluster].frontcluster != cluster &&
				aasworld.portals[-areacluster].backcluster != cluster)
		{
			botimport.Print(PRT_ERROR, "portal %d: does not belong to cluster %d\n"
											, -areacluster, cluster);
		} //end if
#endif //ROUTING_DEBUG*/
		side = aasworld.portals[-areacluster].frontcluster != cluster;
		return aasworld.portals[-areacluster].clusterareanum[side];
	} //end else
} //end of the function AAS_ClusterAreaNum
//===========================================================================
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//===========================================================================
static void AAS_InitTravelFlagFromType(void)
{
	int i;

	for (i = 0; i < MAX_TRAVELTYPES; i++)
	{
		aasworld.travelflagfortype[i] = TFL_INVALID;
	} //end for
	aasworld.travelflagfortype[TRAVEL_INVALID] = TFL_INVALID;
	aasworld.travelflagfortype[TRAVEL_WALK] = TFL_WALK;
	aasworld.travelflagfortype[TRAVEL_CROUCH] = TFL_CROUCH;
	aasworld.travelflagfortype[TRAVEL_BARRIERJUMP] = TFL_BARRIERJUMP;
	aasworld.travelflagfortype[TRAVEL_JUMP] = TFL_JUMP;
	aasworld.travelflagfortype[TRAVEL_LADDER] = TFL_LADDER;
	aasworld.travelflagfortype[TRAVEL_WALKOFFLEDGE] = TFL_WALKOFFLEDGE;
	aasworld.travelflagfortype[TRAVEL_SWIM] = TFL_SWIM;
	aasworld.travelflagfortype[TRAVEL_WATERJUMP] = TFL_WATERJUMP;
	aasworld.travelflagfortype[TRAVEL_TELEPORT] = TFL_TELEPORT;
	aasworld.travelflagfortype[TRAVEL_ELEVATOR] = TFL_ELEVATOR;
	aasworld.travelflagfortype[TRAVEL_ROCKETJUMP] = TFL_ROCKETJUMP;
	aasworld.travelflagfortype[TRAVEL_BFGJUMP] = TFL_BFGJUMP;
	aasworld.travelflagfortype[TRAVEL_GRAPPLEHOOK] = TFL_GRAPPLEHOOK;
	aasworld.travelflagfortype[TRAVEL_DOUBLEJUMP] = TFL_DOUBLEJUMP;
	aasworld.travelflagfortype[TRAVEL_RAMPJUMP] = TFL_RAMPJUMP;
	aasworld.travelflagfortype[TRAVEL_STRAFEJUMP] = TFL_STRAFEJUMP;
	aasworld.travelflagfortype[TRAVEL_JUMPPAD] = TFL_JUMPPAD;
	aasworld.travelflagfortype[TRAVEL_FUNCBOB] = TFL_FUNCBOB;
} //end of the function AAS_InitTravelFlagFromType
//===========================================================================
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//===========================================================================
static ID_INLINE int AAS_TravelFlagForType_inline(unsigned int traveltype)
{
	if (Com_GameIsRTCW()) return fntech3::rtcw::routing::TravelFlag(traveltype);
	// ET indexes its own bounded type table; Quake's high team annotations
	// are invalid ET types (et.exe 0x00481270, etded.exe 0x0044bbd0).
	if (Com_GameIsWolfET()) return traveltype < MAX_TRAVELTYPES ? aasworld.travelflagfortype[traveltype] : TFL_INVALID;
	int tfl;

	tfl = 0;
	if (traveltype & TRAVELFLAG_NOTTEAM1)
		tfl |= TFL_NOTTEAM1;
	if (traveltype & TRAVELFLAG_NOTTEAM2)
		tfl |= TFL_NOTTEAM2;
	traveltype &= TRAVELTYPE_MASK;
	if (traveltype >= MAX_TRAVELTYPES)
		return TFL_INVALID;
	tfl |= aasworld.travelflagfortype[traveltype];
	return tfl;
} //end of the function AAS_TravelFlagForType_inline
//===========================================================================
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//===========================================================================
int AAS_TravelFlagForType(int traveltype)
{
	return AAS_TravelFlagForType_inline(traveltype);
} //end of the function AAS_TravelFlagForType_inline
//===========================================================================
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//===========================================================================
static void AAS_UnlinkCache(aas_routingcache_t *cache)
{
	if (cache->time_next) cache->time_next->time_prev = cache->time_prev;
	else aasworld.newestcache = cache->time_prev;
	if (cache->time_prev) cache->time_prev->time_next = cache->time_next;
	else aasworld.oldestcache = cache->time_next;
	cache->time_next = nullptr;
	cache->time_prev = nullptr;
} //end of the function AAS_UnlinkCache
//===========================================================================
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//===========================================================================
static void AAS_LinkCache(aas_routingcache_t *cache)
{
	if (aasworld.newestcache)
	{
		aasworld.newestcache->time_next = cache;
		cache->time_prev = aasworld.newestcache;
	} //end if
	else
	{
		aasworld.oldestcache = cache;
		cache->time_prev = nullptr;
	} //end else
	cache->time_next = nullptr;
	aasworld.newestcache = cache;
} //end of the function AAS_LinkCache
//===========================================================================
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//===========================================================================
void AAS_FreeRoutingCache(aas_routingcache_t *cache)
{
	AAS_UnlinkCache(cache);
	aasworld.routingcachesize -= cache->size;
	FreeMemory(cache);
} //end of the function AAS_FreeRoutingCache
//===========================================================================
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//===========================================================================
static void AAS_RemoveRoutingCacheInCluster( int clusternum )
{
	int i;
	aas_routingcache_t *cache, *nextcache;
	aas_cluster_t *cluster;

	if (!aasworld.clusterareacache)
		return;
	cluster = &aasworld.clusters[clusternum];
	for (i = 0; i < cluster->numareas; i++)
	{
		for (cache = aasworld.clusterareacache[clusternum][i]; cache; cache = nextcache)
		{
			nextcache = cache->next;
			AAS_FreeRoutingCache(cache);
		} //end for
		aasworld.clusterareacache[clusternum][i] = nullptr;
	} //end for
} //end of the function AAS_RemoveRoutingCacheInCluster
//===========================================================================
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//===========================================================================
static void AAS_RemoveRoutingCacheUsingArea( int areanum )
{
	int i, clusternum;
	aas_routingcache_t *cache, *nextcache;

	// Area blocking may precede routing initialization or follow partial setup.
	if ( !aasworld.clusterareacache && !aasworld.portalcache ) return;

	clusternum = aasworld.areasettings[areanum].cluster;
	if (clusternum > 0)
	{
		//remove all the cache in the cluster the area is in
		AAS_RemoveRoutingCacheInCluster( clusternum );
	} //end if
	else
	{
		// if this is a portal remove all cache in both the front and back cluster
		AAS_RemoveRoutingCacheInCluster( aasworld.portals[-clusternum].frontcluster );
		AAS_RemoveRoutingCacheInCluster( aasworld.portals[-clusternum].backcluster );
	} //end else
	// remove all portal cache
	if ( !aasworld.portalcache ) return;
	for (i = 0; i < aasworld.numareas; i++)
	{
		//refresh portal cache
		for (cache = aasworld.portalcache[i]; cache; cache = nextcache)
		{
			nextcache = cache->next;
			AAS_FreeRoutingCache(cache);
		} //end for
		aasworld.portalcache[i] = nullptr;
	} //end for
} //end of the function AAS_RemoveRoutingCacheUsingArea
//===========================================================================
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//===========================================================================
int AAS_EnableRoutingArea(int areanum, int enable)
{
	int flags;

	if (areanum <= 0 || areanum >= aasworld.numareas)
	{
		if (botDeveloper)
		{
			botimport.Print(PRT_ERROR, "AAS_EnableRoutingArea: areanum %d out of range\n", areanum);
		} //end if
		return 0;
	} //end if
	flags = aasworld.areasettings[areanum].areaflags & AREA_DISABLED;
	if (enable < 0)
		return !flags;

	if (enable)
		aasworld.areasettings[areanum].areaflags &= ~AREA_DISABLED;
	else
		aasworld.areasettings[areanum].areaflags |= AREA_DISABLED;
	// if the status of the area changed
	if ( (flags & AREA_DISABLED) != (aasworld.areasettings[areanum].areaflags & AREA_DISABLED) )
	{
		//remove all routing cache involving this area
		AAS_RemoveRoutingCacheUsingArea( areanum );
	} //end if
	return !flags;
} //end of the function AAS_EnableRoutingArea
//===========================================================================
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//===========================================================================
static ID_INLINE float AAS_RoutingTime(void)
{
	return AAS_Time();
} //end of the function AAS_RoutingTime
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
static int AAS_GetAreaContentsTravelFlags(int areanum)
{
	// ET's contents query (et.exe 0x0047cf00) has the same numeric liquid and
	// entry-restriction rules as RTCW, including bit 1024 and no bridge bit.
	if (Com_GameIsRTCW() || Com_GameIsWolfET())
		return fntech3::rtcw::routing::ContentsFlags(aasworld.areasettings[areanum].contents);
	int contents, tfl;

	contents = aasworld.areasettings[areanum].contents;
	//Elite Force Holomatch's engine gives a liquid area its liquid's flag
	//alone, and any other area TFL_AIR with TFL_DONOTENTER; it has no team or
	//bridge flags (stvoyHM.exe 0x00499290)
	if (Com_GameIsSTVEFMP())
	{
		if (contents & AREACONTENTS_WATER) return TFL_WATER;
		if (contents & AREACONTENTS_SLIME) return TFL_SLIME;
		if (contents & AREACONTENTS_LAVA) return TFL_LAVA;
		return TFL_AIR | ((contents & AREACONTENTS_DONOTENTER) ? TFL_DONOTENTER : 0);
	} //end if
	tfl = 0;
	if (contents & AREACONTENTS_WATER)
		tfl |= TFL_WATER;
	else if (contents & AREACONTENTS_SLIME)
		tfl |= TFL_SLIME;
	else if (contents & AREACONTENTS_LAVA)
		tfl |= TFL_LAVA;
	else
		tfl |= TFL_AIR;
	if (contents & AREACONTENTS_DONOTENTER)
		tfl |= TFL_DONOTENTER;
	if (contents & AREACONTENTS_NOTTEAM1)
		tfl |= TFL_NOTTEAM1;
	if (contents & AREACONTENTS_NOTTEAM2)
		tfl |= TFL_NOTTEAM2;
	if (aasworld.areasettings[areanum].areaflags & AREA_BRIDGE)
		tfl |= TFL_BRIDGE;
	return tfl;
} //end of the function AAS_GetAreaContentsTravelFlags
//===========================================================================
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//===========================================================================
static ID_INLINE int AAS_AreaContentsTravelFlags_inline(int areanum)
{
	return aasworld.areacontentstravelflags[areanum];
} //end of the function AAS_AreaContentsTravelFlags
//===========================================================================
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//===========================================================================
int AAS_AreaContentsTravelFlags(int areanum)
{
	return aasworld.areacontentstravelflags[areanum];
} //end of the function AAS_AreaContentsTravelFlags
//===========================================================================
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//===========================================================================
static void AAS_InitAreaContentsTravelFlags(void)
{
	int i;

	if (aasworld.areacontentstravelflags) FreeMemory(aasworld.areacontentstravelflags);
	aasworld.areacontentstravelflags = botlib::ClearedAlloc<int>(aasworld.numareas);
	//
	for (i = 0; i < aasworld.numareas; i++) {
		aasworld.areacontentstravelflags[i] = AAS_GetAreaContentsTravelFlags(i);
	}
} //end of the function AAS_InitAreaContentsTravelFlags
//===========================================================================
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//===========================================================================
static void AAS_CreateReversedReachability(void)
{
	int i, n, maxlinks;
	aas_reversedlink_t *revlink;
	aas_reachability_t *reach;
	aas_areasettings_t *settings;
	char *ptr;
#ifdef DEBUG
	int starttime;

	starttime = Sys_MilliSeconds();
#endif
	//free reversed links that have already been created
	if (aasworld.reversedreachability) FreeMemory(aasworld.reversedreachability);
	//allocate memory for the reversed reachability links
	ptr = botlib::ClearedAllocBytes<char>(aasworld.numareas * sizeof(aas_reversedreachability_t) +
							aasworld.reachabilitysize * sizeof(aas_reversedlink_t));
	//
	aasworld.reversedreachability = reinterpret_cast<aas_reversedreachability_t *>(ptr);
	//pointer to the memory for the reversed links
	ptr += aasworld.numareas * sizeof(aas_reversedreachability_t);
	//check all reachabilities of all areas
	for (i = 1; i < aasworld.numareas; i++)
	{
		//settings of the area
		settings = &aasworld.areasettings[i];
		//
		if (settings->numreachableareas > 128)
			botimport.Print(PRT_WARNING, "area %d has more than 128 reachabilities\n", i);
		//create reversed links for the reachabilities
		for (n = 0; n < settings->numreachableareas && n < 128; n++)
		{
			//reachability link
			reach = &aasworld.reachability[settings->firstreachablearea + n];
			//
			revlink = reinterpret_cast<aas_reversedlink_t *>(ptr);
			ptr += sizeof(aas_reversedlink_t);
			//
			revlink->areanum = i;
			revlink->linknum = settings->firstreachablearea + n;
			revlink->next = aasworld.reversedreachability[reach->areanum].first;
			aasworld.reversedreachability[reach->areanum].first = revlink;
			aasworld.reversedreachability[reach->areanum].numlinks++;
		} //end for
	} //end for
	//an area cache starts in its goal area, where no link into it costs any
	//travel time: one zero for each link into the area. Only the first 128
	//reachabilities of each area are reversed, but together they can enter
	//one area more than 128 times
	if (aasworld.startareatraveltimes) FreeMemory(aasworld.startareatraveltimes);
	aasworld.startareatraveltimes = nullptr;
	maxlinks = 1;
	for (i = 0; i < aasworld.numareas; i++)
	{
		if (aasworld.reversedreachability[i].numlinks > maxlinks)
			maxlinks = aasworld.reversedreachability[i].numlinks;
	} //end for
	aasworld.startareatraveltimes = botlib::ClearedAlloc<unsigned short>(maxlinks);
#ifdef DEBUG
	botimport.Print(PRT_MESSAGE, "reversed reachability %d msec\n", Sys_MilliSeconds() - starttime);
#endif
} //end of the function AAS_CreateReversedReachability
//===========================================================================
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//===========================================================================
unsigned short int AAS_AreaTravelTime(int areanum, vec3_t start, vec3_t end)
{
	if (Com_GameIsWolfET()) {
		std::uint16_t cost = 0;
		if (!aasworld.areasettings || aasworld.numareasettings != aasworld.numareas
			|| areanum <= 0 || areanum >= aasworld.numareas
			|| !fntech3::wolfet::navigation::AreaTravelTime(start, end,
				AAS_AreaCrouch(areanum) != 0, cost)) {
			botimport.Print(PRT_ERROR, "Invalid ET area travel cost\n");
			return 0;
		}
		return cost;
	}
	if (Com_GameIsRTCW()) {
		std::uint16_t cost = 0;
		if (areanum <= 0 || areanum >= aasworld.numareas || !aasworld.rtcwGroundSteepness
			|| !fntech3::rtcw::routing::AreaTravelTime(start, end, aasworld.rtcwGroundSteepness[areanum],
				AAS_AreaCrouch(areanum) != 0, AAS_AreaSwim(areanum) != 0, Com_GameIsRTCWSP() != 0, cost)) {
			botimport.Print(PRT_ERROR, "Invalid RTCW area travel cost\n");
			return 0;
		}
		return cost;
	}
	int intdist;
	float dist;
	vec3_t dir;

	VectorSubtract(start, end, dir);
	dist = VectorLength(dir);
	//if crouch only area
	if (AAS_AreaCrouch(areanum)) dist *= DISTANCEFACTOR_CROUCH;
	//if swim area
	else if (AAS_AreaSwim(areanum)) dist *= DISTANCEFACTOR_SWIM;
	//normal walk area
	else dist *= DISTANCEFACTOR_WALK;
	//
	intdist = (int) dist;
	//make sure the distance isn't zero
	if (intdist <= 0) intdist = 1;
	return intdist;
} //end of the function AAS_AreaTravelTime
//===========================================================================
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//===========================================================================
static void AAS_CalculateAreaTravelTimes(void)
{
	int i, l, n, size;
	char *ptr;
	vec3_t end;
	const aas_reversedreachability_t *revreach;
	const aas_reversedlink_t *revlink;
	aas_reachability_t *reach;
	aas_areasettings_t *settings;
#ifdef DEBUG
	int starttime;

	starttime = Sys_MilliSeconds();
#endif
	//if there are still area travel times, free the memory
	if (aasworld.areatraveltimes) FreeMemory(aasworld.areatraveltimes);
	//get the total size of all the area travel times
	size = aasworld.numareas * sizeof(unsigned short **);
	for (i = 0; i < aasworld.numareas; i++)
	{
		revreach = &aasworld.reversedreachability[i];
		//settings of the area
		settings = &aasworld.areasettings[i];
		//
		size += settings->numreachableareas * sizeof(unsigned short *);
		//
		size += settings->numreachableareas *
			PAD(revreach->numlinks, sizeof(long)) * sizeof(unsigned short);
	} //end for
	//allocate memory for the area travel times
	ptr = botlib::ClearedAllocBytes<char>(size);
	aasworld.areatraveltimes = reinterpret_cast<unsigned short ***>(ptr);
	ptr += aasworld.numareas * sizeof(unsigned short **);
	//calcluate the travel times for all the areas
	for (i = 0; i < aasworld.numareas; i++)
	{
		//reversed reachabilities of this area
		revreach = &aasworld.reversedreachability[i];
		//settings of the area
		settings = &aasworld.areasettings[i];
		//
		aasworld.areatraveltimes[i] = reinterpret_cast<unsigned short **>(ptr);
		ptr += settings->numreachableareas * sizeof(unsigned short *);
		//
		for (l = 0; l < settings->numreachableareas; l++)
		{
			aasworld.areatraveltimes[i][l] = reinterpret_cast<unsigned short *>(ptr);
			ptr += PAD(revreach->numlinks, sizeof(long)) * sizeof(unsigned short);
			//reachability link
			reach = &aasworld.reachability[settings->firstreachablearea + l];
			//
			for (n = 0, revlink = revreach->first; revlink; revlink = revlink->next, n++)
			{
				VectorCopy(aasworld.reachability[revlink->linknum].end, end);
				//
				aasworld.areatraveltimes[i][l][n] = AAS_AreaTravelTime(i, end, reach->start);
			} //end for
		} //end for
	} //end for
#ifdef DEBUG
	botimport.Print(PRT_MESSAGE, "area travel times %d msec\n", Sys_MilliSeconds() - starttime);
#endif
} //end of the function AAS_CalculateAreaTravelTimes
//===========================================================================
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//===========================================================================
static int AAS_PortalMaxTravelTime(int portalnum)
{
	int l, n, t, maxt;
	aas_portal_t *portal;
	aas_reversedreachability_t *revreach;
	aas_reversedlink_t *revlink;
	aas_areasettings_t *settings;

	portal = &aasworld.portals[portalnum];
	//reversed reachabilities of this portal area
	revreach = &aasworld.reversedreachability[portal->areanum];
	//settings of the portal area
	settings = &aasworld.areasettings[portal->areanum];
	//
	maxt = 0;
	for (l = 0; l < settings->numreachableareas; l++)
	{
		for (n = 0, revlink = revreach->first; revlink; revlink = revlink->next, n++)
		{
			t = aasworld.areatraveltimes[portal->areanum][l][n];
			if (t > maxt)
			{
				maxt = t;
			} //end if
		} //end for
	} //end for
	return maxt;
} //end of the function AAS_PortalMaxTravelTime
//===========================================================================
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//===========================================================================
static void AAS_InitPortalMaxTravelTimes(void)
{
	int i;

	if (aasworld.portalmaxtraveltimes) FreeMemory(aasworld.portalmaxtraveltimes);

	aasworld.portalmaxtraveltimes = botlib::ClearedAlloc<int>(aasworld.numportals);

	for (i = 0; i < aasworld.numportals; i++)
	{
		aasworld.portalmaxtraveltimes[i] = AAS_PortalMaxTravelTime(i);
		//botimport.Print(PRT_MESSAGE, "portal %d max tt = %d\n", i, aasworld.portalmaxtraveltimes[i]);
	} //end for
} //end of the function AAS_InitPortalMaxTravelTimes
//===========================================================================
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//===========================================================================
/*
static int AAS_FreeOldestCache(void)
{
	int i, j, bestcluster, bestarea, freed;
	float besttime;
	aas_routingcache_t *cache, *bestcache;

	freed = qfalse;
	besttime = 999999999;
	bestcache = nullptr;
	bestcluster = 0;
	bestarea = 0;
	//refresh cluster cache
	for (i = 0; i < aasworld.numclusters; i++)
	{
		for (j = 0; j < aasworld.clusters[i].numareas; j++)
		{
			for (cache = aasworld.clusterareacache[i][j]; cache; cache = cache->next)
			{
				//never remove cache leading towards a portal
				if (aasworld.areasettings[cache->areanum].cluster < 0) continue;
				//if this cache is older than the cache we found so far
				if (cache->time < besttime)
				{
					bestcache = cache;
					bestcluster = i;
					bestarea = j;
					besttime = cache->time;
				} //end if
			} //end for
		} //end for
	} //end for
	if (bestcache)
	{
		cache = bestcache;
		if (cache->prev) cache->prev->next = cache->next;
		else aasworld.clusterareacache[bestcluster][bestarea] = cache->next;
		if (cache->next) cache->next->prev = cache->prev;
		AAS_FreeRoutingCache(cache);
		freed = qtrue;
	} //end if
	besttime = 999999999;
	bestcache = nullptr;
	bestarea = 0;
	for (i = 0; i < aasworld.numareas; i++)
	{
		//refresh portal cache
		for (cache = aasworld.portalcache[i]; cache; cache = cache->next)
		{
			if (cache->time < besttime)
			{
				bestcache = cache;
				bestarea = i;
				besttime = cache->time;
			} //end if
		} //end for
	} //end for
	if (bestcache)
	{
		cache = bestcache;
		if (cache->prev) cache->prev->next = cache->next;
		else aasworld.portalcache[bestarea] = cache->next;
		if (cache->next) cache->next->prev = cache->prev;
		AAS_FreeRoutingCache(cache);
		freed = qtrue;
	} //end if
	return freed;
} //end of the function AAS_FreeOldestCache
*/
//===========================================================================
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//===========================================================================
static int AAS_FreeOldestCache(void)
{
	int clusterareanum;
	aas_routingcache_t *cache;

	if (Com_GameIsRTCW() || Com_GameIsWolfET()) {
		// RTCW and ET expire one ordinary area cache and one portal cache per pass.
		// Equal access times are resolved by table order, not LRU link order.
		bool freed = false;
		for (int kind : {CACHETYPE_AREA, CACHETYPE_PORTAL}) {
			aas_routingcache_t *oldest = nullptr;
			float oldestTime = 999999999.0f;
			const auto consider = [&](aas_routingcache_t *head) {
				for (auto *entry = head; entry; entry = entry->next) {
					if (kind == CACHETYPE_AREA && aasworld.areasettings[entry->areanum].cluster < 0) continue;
					if (entry->time < oldestTime) { oldest = entry; oldestTime = entry->time; }
				}
			};
			if (kind == CACHETYPE_AREA) {
				for (int c = 0; c < aasworld.numclusters; ++c)
					for (int a = 0; a < aasworld.clusters[c].numareas; ++a)
						consider(aasworld.clusterareacache[c][a]);
			} else {
				for (int a = 0; a < aasworld.numareas; ++a) consider(aasworld.portalcache[a]);
			}
			if (!oldest) continue;
			auto &head = kind == CACHETYPE_AREA
				? aasworld.clusterareacache[oldest->cluster][AAS_ClusterAreaNum(oldest->cluster, oldest->areanum)]
				: aasworld.portalcache[oldest->areanum];
			if (oldest->prev) oldest->prev->next = oldest->next;
			else head = oldest->next;
			if (oldest->next) oldest->next->prev = oldest->prev;
			AAS_FreeRoutingCache(oldest);
			freed = true;
		}
		return freed;
	}

	for (cache = aasworld.oldestcache; cache; cache = cache->time_next) {
		// never free area cache leading towards a portal
		if (cache->type == CACHETYPE_AREA && aasworld.areasettings[cache->areanum].cluster < 0) {
			continue;
		}
		break;
	}
	if (cache) {
		// unlink the cache
		if (cache->type == CACHETYPE_AREA) {
			//number of the area in the cluster
			clusterareanum = AAS_ClusterAreaNum(cache->cluster, cache->areanum);
			// unlink from cluster area cache
			if (cache->prev) cache->prev->next = cache->next;
			else aasworld.clusterareacache[cache->cluster][clusterareanum] = cache->next;
			if (cache->next) cache->next->prev = cache->prev;
		}
		else {
			// unlink from portal cache
			if (cache->prev) cache->prev->next = cache->next;
			else aasworld.portalcache[cache->areanum] = cache->next;
			if (cache->next) cache->next->prev = cache->prev;
		}
		AAS_FreeRoutingCache(cache);
		return qtrue;
	}
	return qfalse;
} //end of the function AAS_FreeOldestCache
//===========================================================================
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//===========================================================================
static aas_routingcache_t *AAS_AllocRoutingCache(int numtraveltimes)
{
	aas_routingcache_t *cache;
	int size;

	//
	size = sizeof(aas_routingcache_t)
						+ numtraveltimes * sizeof(unsigned short int)
						+ numtraveltimes * sizeof(unsigned char);
	//
	cache = botlib::ClearedAllocBytes<aas_routingcache_t>(size);
	aasworld.routingcachesize += size;
	cache->reachabilities = reinterpret_cast<unsigned char *>(cache) + sizeof(aas_routingcache_t)
								+ numtraveltimes * sizeof(unsigned short int);
	cache->size = size;
	return cache;
} //end of the function AAS_AllocRoutingCache
//===========================================================================
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//===========================================================================
static void AAS_FreeAllClusterAreaCache(void)
{
	int i, j;
	aas_routingcache_t *cache, *nextcache;
	aas_cluster_t *cluster;

	//free all cluster cache if existing
	if (!aasworld.clusterareacache) return;
	//free caches
	for (i = 0; i < aasworld.numclusters; i++)
	{
		cluster = &aasworld.clusters[i];
		for (j = 0; j < cluster->numareas; j++)
		{
			for (cache = aasworld.clusterareacache[i][j]; cache; cache = nextcache)
			{
				nextcache = cache->next;
				AAS_FreeRoutingCache(cache);
			} //end for
			aasworld.clusterareacache[i][j] = nullptr;
		} //end for
	} //end for
	//free the cluster cache array
	FreeMemory(aasworld.clusterareacache);
	aasworld.clusterareacache = nullptr;
} //end of the function AAS_FreeAllClusterAreaCache
//===========================================================================
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//===========================================================================
static void AAS_InitClusterAreaCache(void)
{
	int i, size;
	char *ptr;

	//
	for (size = 0, i = 0; i < aasworld.numclusters; i++)
	{
		size += aasworld.clusters[i].numareas;
	} //end for
	//two dimensional array with pointers for every cluster to routing cache
	//for every area in that cluster
	ptr = botlib::ClearedAllocBytes<char>(
				aasworld.numclusters * sizeof(aas_routingcache_t **) +
				size * sizeof(aas_routingcache_t *));
	aasworld.clusterareacache = reinterpret_cast<aas_routingcache_t ***>(ptr);
	ptr += aasworld.numclusters * sizeof(aas_routingcache_t **);
	for (i = 0; i < aasworld.numclusters; i++)
	{
		aasworld.clusterareacache[i] = reinterpret_cast<aas_routingcache_t **>(ptr);
		ptr += aasworld.clusters[i].numareas * sizeof(aas_routingcache_t *);
	} //end for
} //end of the function AAS_InitClusterAreaCache
//===========================================================================
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//===========================================================================
static void AAS_FreeAllPortalCache(void)
{
	int i;
	aas_routingcache_t *cache, *nextcache;

	//free all portal cache if existing
	if (!aasworld.portalcache) return;
	//free portal caches
	for (i = 0; i < aasworld.numareas; i++)
	{
		for (cache = aasworld.portalcache[i]; cache; cache = nextcache)
		{
			nextcache = cache->next;
			AAS_FreeRoutingCache(cache);
		} //end for
		aasworld.portalcache[i] = nullptr;
	} //end for
	FreeMemory(aasworld.portalcache);
	aasworld.portalcache = nullptr;
} //end of the function AAS_FreeAllPortalCache
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
static void AAS_InitPortalCache(void)
{
	//
	aasworld.portalcache = botlib::ClearedAlloc<aas_routingcache_t *>(aasworld.numareas);
} //end of the function AAS_InitPortalCache
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
static void AAS_InitRoutingUpdate(void)
{
	int i, maxreachabilityareas;

	//free routing update fields if already existing
	if (aasworld.areaupdate) FreeMemory(aasworld.areaupdate);
	//
	maxreachabilityareas = 0;
	for (i = 0; i < aasworld.numclusters; i++)
	{
		if (aasworld.clusters[i].numreachabilityareas > maxreachabilityareas)
		{
			maxreachabilityareas = aasworld.clusters[i].numreachabilityareas;
		} //end if
	} //end for
	//ET's tactical searches (wolfet_routing.hpp) address these records by area
	//number, and etded.exe 2.60b allocates one for every area (0x0044a1ed)
	if (Com_GameIsWolfET()) maxreachabilityareas = std::max(maxreachabilityareas, aasworld.numareas);
	//allocate memory for the routing update fields
	aasworld.areaupdate = botlib::ClearedAlloc<aas_routingupdate_t>(maxreachabilityareas);
	//
	if (aasworld.portalupdate) FreeMemory(aasworld.portalupdate);
	//allocate memory for the portal update fields
	aasworld.portalupdate = botlib::ClearedAlloc<aas_routingupdate_t>(aasworld.numportals + 1);
} //end of the function AAS_InitRoutingUpdate
//===========================================================================
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//===========================================================================
void AAS_CreateAllRoutingCache(void)
{
	int i, j;
	//int t;

	aasworld.initialized = qtrue;
	botimport.Print(PRT_MESSAGE, "AAS_CreateAllRoutingCache\n");
	for (i = 1; i < aasworld.numareas; i++)
	{
		if (!AAS_AreaReachability(i)) continue;
		for (j = 1; j < aasworld.numareas; j++)
		{
			if (i == j) continue;
			if (!AAS_AreaReachability(j)) continue;
			AAS_AreaTravelTimeToGoalArea(i, aasworld.areas[i].center, j, TFL_DEFAULT);
			//t = AAS_AreaTravelTimeToGoalArea(i, aasworld.areas[i].center, j, TFL_DEFAULT);
			//Log_Write("traveltime from %d to %d is %d", i, j, t);
		} //end for
	} //end for
	aasworld.initialized = qfalse;
} //end of the function AAS_CreateAllRoutingCache
//===========================================================================
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//===========================================================================

//the route cache header
//this header is followed by numportalcache + numareacache aas_routingcache_t
//structures that store routing cache: each cache as the routing holds it,
//cache->size bytes from its first member, so its pointers are stale and its
//layout is this build's (a cache's size differs with the size of a pointer)
struct routecacheheader_t
{
	int ident;
	int version;
	int numareas;
	int numclusters;
	int areacrc;
	int clustercrc;
	int numportalcache;
	int numareacache;
};

static constexpr int RCID = ('C' << 24) + ('R' << 16) + ('E' << 8) + 'M';
static constexpr int RCVERSION = 2;
//the most bytes of caches a dump is read with, the bound RTCW's reader keeps
//(rtcw_routecache_format.hpp): far more than the routing keeps, and far
//enough below 2 GB that no allocation size in the botlib's int arithmetic wraps
static constexpr int MAX_ROUTECACHE_BYTES = 256 * 1024 * 1024;

//void AAS_DecompressVis(byte *in, int numareas, byte *decompressed);
//int AAS_CompressVis(byte *vis, int numareas, byte *dest);

static bool AAS_WriteRTCWRouteCache();

void AAS_WriteRouteCache(void)
{
	if (Com_GameIsRTCW()) {
		AAS_WriteRTCWRouteCache();
		return;
	}
	int i, j, numportalcache, numareacache, totalsize;
	aas_routingcache_t *cache;
	aas_cluster_t *cluster;
	fileHandle_t fp;
	char filename[MAX_QPATH];
	routecacheheader_t routecacheheader;

	//AAS_StartFrame asks for a dump whenever saveroutingcache is set. Until
	//AAS_InitRouting has run there is none to write: while a map's reachability
	//is still computed, over several frames, the areas are counted but the
	//cache tables walked below are not made yet, and a map without navigation
	//has no routing at all
	if (!aasworld.initialized)
	{
		botimport.Print(PRT_WARNING, "no route cache written: routing is not initialized\n");
		return;
	} //end if
	numportalcache = 0;
	for (i = 0; i < aasworld.numareas; i++)
	{
		for (cache = aasworld.portalcache[i]; cache; cache = cache->next)
		{
			numportalcache++;
		} //end for
	} //end for
	numareacache = 0;
	for (i = 0; i < aasworld.numclusters; i++)
	{
		cluster = &aasworld.clusters[i];
		for (j = 0; j < cluster->numareas; j++)
		{
			for (cache = aasworld.clusterareacache[i][j]; cache; cache = cache->next)
			{
				numareacache++;
			} //end for
		} //end for
	} //end for
	// open the file for writing
	Com_sprintf(filename, MAX_QPATH, "maps/%s.rcd", aasworld.mapname);
	botimport.FS_FOpenFile( filename, &fp, FS_WRITE );
	if (!fp)
	{
		AAS_Error("Unable to open file: %s\n", filename);
		return;
	} //end if
	//create the header
	routecacheheader.ident = RCID;
	routecacheheader.version = RCVERSION;
	routecacheheader.numareas = aasworld.numareas;
	routecacheheader.numclusters = aasworld.numclusters;
	routecacheheader.areacrc = CRC_ProcessArray(aasworld.areas, aasworld.numareas);
	routecacheheader.clustercrc = CRC_ProcessArray(aasworld.clusters, aasworld.numclusters);
	routecacheheader.numportalcache = numportalcache;
	routecacheheader.numareacache = numareacache;
	//write the header
	botimport.FS_Write(&routecacheheader, sizeof(routecacheheader_t), fp);
	//
	totalsize = 0;
	//write all the cache
	for (i = 0; i < aasworld.numareas; i++)
	{
		for (cache = aasworld.portalcache[i]; cache; cache = cache->next)
		{
			botimport.FS_Write(cache, cache->size, fp);
			totalsize += cache->size;
		} //end for
	} //end for
	for (i = 0; i < aasworld.numclusters; i++)
	{
		cluster = &aasworld.clusters[i];
		for (j = 0; j < cluster->numareas; j++)
		{
			for (cache = aasworld.clusterareacache[i][j]; cache; cache = cache->next)
			{
				botimport.FS_Write(cache, cache->size, fp);
				totalsize += cache->size;
			} //end for
		} //end for
	} //end for
	// write the visareas
	/*
	for (i = 0; i < aasworld.numareas; i++)
	{
		if (!aasworld.areavisibility[i]) {
			size = 0;
			botimport.FS_Write(&size, sizeof(int), fp);
			continue;
		}
		AAS_DecompressVis( aasworld.areavisibility[i], aasworld.numareas, aasworld.decompressedvis );
		size = AAS_CompressVis( aasworld.decompressedvis, aasworld.numareas, aasworld.decompressedvis );
		botimport.FS_Write(&size, sizeof(int), fp);
		botimport.FS_Write(aasworld.decompressedvis, size, fp);
	}
	*/
	//
	botimport.FS_FCloseFile(fp);
	botimport.Print(PRT_MESSAGE, "\nroute cache written to %s\n", filename);
	botimport.Print(PRT_MESSAGE, "written %d bytes of routing cache\n", totalsize);
} //end of the function AAS_WriteRouteCache
//===========================================================================
// the size AAS_AllocRoutingCache gives a cache of the given travel times
//
// Parameter:			numtraveltimes: not negative
// Returns:				the cache's bytes, which may be more than an int holds
// Changes Globals:		-
//===========================================================================
static long long AAS_RoutingCacheBytes(int numtraveltimes)
{
	return static_cast<long long>(sizeof(aas_routingcache_t))
		+ static_cast<long long>(numtraveltimes) * static_cast<long long>(sizeof(unsigned short int) + sizeof(unsigned char));
} //end of the function AAS_RoutingCacheBytes
//===========================================================================
// whether routing may number an area in a cluster: the area is in the
// cluster, or is a portal on its border
//
// Parameter:			areanum: an area of the map
//						cluster: a cluster of the map
// Returns:				true when the area is in the cluster or on its border
// Changes Globals:		-
//===========================================================================
static bool AAS_RouteCacheAreaInCluster(int areanum, int cluster)
{
	const int areacluster = aasworld.areasettings[areanum].cluster;
	const aas_portal_t *portal;

	if (areacluster >= 0) return areacluster == cluster;
	//a portal's number is not negated before it is known to be one
	if (areacluster <= -aasworld.numportals) return false;
	portal = &aasworld.portals[-areacluster];
	return portal->frontcluster == cluster || portal->backcluster == cluster;
} //end of the function AAS_RouteCacheAreaInCluster
//===========================================================================
// checks a route cache dump's counts of caches against its length before
// any byte of the caches is read: every portal cache takes the size of a
// cache of every portal, and an area cache at least the size of a cache
// without travel times, whatever its cluster
//
// Parameter:			routecacheheader: the dump's header, matched to the map
//						length: the bytes after the header
//						fault: what is wrong, in words, when something is
// Returns:				true when the caches the header counts can fit
// Changes Globals:		-
//===========================================================================
static bool AAS_RouteCacheCountsFit(const routecacheheader_t &routecacheheader, int length, char *fault, int faultsize)
{
	const long long portalbytes = AAS_RoutingCacheBytes(std::max(aasworld.numportals, 0));

	if (length > MAX_ROUTECACHE_BYTES)
	{
		Com_sprintf(fault, faultsize, "has %d bytes of caches, more than the %d read", length, MAX_ROUTECACHE_BYTES);
		return false;
	} //end if
	if (routecacheheader.numportalcache < 0 || routecacheheader.numareacache < 0
		|| routecacheheader.numportalcache > length / portalbytes
		|| routecacheheader.numareacache > (length - routecacheheader.numportalcache * portalbytes) / AAS_RoutingCacheBytes(0))
	{
		Com_sprintf(fault, faultsize, "cannot hold %d portal and %d area caches in %d bytes",
			routecacheheader.numportalcache, routecacheheader.numareacache, length);
		return false;
	} //end if
	return true;
} //end of the function AAS_RouteCacheCountsFit
//===========================================================================
// checks one cache of a route cache dump against the map before routing
// reads it: routing takes a cache for the kind its list holds, keeps it in
// the list of its area (an area cache under the area's number in its
// cluster), indexes a portal cache's travel times by portal and an area
// cache's by the numbers its cluster gives, and a route query from an area
// takes the cache's reachability there as an offset into that area's own
//
// Parameter:			index: the cache's number among those of its kind
//						portal: whether it is a portal cache
//						record: its bytes, remaining of them to the file's end
//						start, fewest: the bounds AAS_RouteCacheRecordsFit gathers
//						fault: what is wrong, in words, when something is
// Returns:				the cache's size when routing may read it, else 0
// Changes Globals:		-
//===========================================================================
static int AAS_RouteCacheRecordSize(int index, bool portal, const byte *record, int remaining,
	const int *start, const int *fewest, char *fault, int faultsize)
{
	const char *kind = portal ? "portal" : "area";
	aas_routingcache_t stored;
	int i, number, numtraveltimes, bound;
	const byte *reachabilities;

	if (remaining < static_cast<int>(sizeof(aas_routingcache_t)))
	{
		Com_sprintf(fault, faultsize, "%s cache %d starts %d bytes from the end of the file, too few for a cache",
			kind, index, remaining);
		return 0;
	} //end if
	//its pointers are the writer's, and only its numbers are read
	memcpy(&stored, record, sizeof(stored));
	if (stored.type != (portal ? CACHETYPE_PORTAL : CACHETYPE_AREA))
	{
		Com_sprintf(fault, faultsize, "%s cache %d has type %d", kind, index, stored.type);
		return 0;
	} //end if
	if (stored.areanum <= 0 || stored.areanum >= aasworld.numareas)
	{
		Com_sprintf(fault, faultsize, "%s cache %d is for area %d, which the map lacks", kind, index, stored.areanum);
		return 0;
	} //end if
	if (stored.cluster <= 0 || stored.cluster >= aasworld.numclusters)
	{
		Com_sprintf(fault, faultsize, "%s cache %d is for cluster %d, which the map lacks", kind, index, stored.cluster);
		return 0;
	} //end if
	if (!AAS_RouteCacheAreaInCluster(stored.areanum, stored.cluster))
	{
		Com_sprintf(fault, faultsize, "%s cache %d is for area %d, which is not in cluster %d",
			kind, index, stored.areanum, stored.cluster);
		return 0;
	} //end if
	if (!portal)
	{
		number = AAS_ClusterAreaNum(stored.cluster, stored.areanum);
		if (number < 0 || number >= aasworld.clusters[stored.cluster].numareas)
		{
			Com_sprintf(fault, faultsize, "area cache %d is for area %d, whose number %d in cluster %d is outside its %d areas",
				index, stored.areanum, number, stored.cluster, aasworld.clusters[stored.cluster].numareas);
			return 0;
		} //end if
	} //end if
	numtraveltimes = portal ? aasworld.numportals : aasworld.clusters[stored.cluster].numreachabilityareas;
	if (numtraveltimes < 0 || stored.size != AAS_RoutingCacheBytes(numtraveltimes))
	{
		Com_sprintf(fault, faultsize, "%s cache %d is %d bytes, not the size of a cache of %d travel times",
			kind, index, stored.size, numtraveltimes);
		return 0;
	} //end if
	if (stored.size > remaining)
	{
		Com_sprintf(fault, faultsize, "%s cache %d is %d bytes, more than the %d left in the file",
			kind, index, stored.size, remaining);
		return 0;
	} //end if
	//each reachability a route query can take must be one of its area's
	reachabilities = record + sizeof(aas_routingcache_t) + numtraveltimes * sizeof(unsigned short int);
	for (i = 0; i < numtraveltimes; i++)
	{
		if (!reachabilities[i]) continue;
		if (portal) bound = fewest[i];
		else if (i < aasworld.clusters[stored.cluster].numareas) bound = fewest[start[stored.cluster] + i];
		else bound = -1;
		if (reachabilities[i] < bound) continue;
		if (portal)
		{
			Com_sprintf(fault, faultsize, "portal cache %d gives portal %d reachability %d, which its area lacks",
				index, i, reachabilities[i]);
		} //end if
		else
		{
			Com_sprintf(fault, faultsize, "area cache %d gives number %d of cluster %d reachability %d, which its area lacks",
				index, i, stored.cluster, reachabilities[i]);
		} //end else
		return 0;
	} //end for
	return stored.size;
} //end of the function AAS_RouteCacheRecordSize
//===========================================================================
// checks every cache of a route cache dump against the map, in the order
// the file holds them, before any is read into the routing
//
// Parameter:			routecacheheader: the dump's header, with counts that fit
//						caches: the length bytes after the header
//						fault: what is wrong, in words, when something is
// Returns:				true when routing may read every cache
// Changes Globals:		-
//===========================================================================
static bool AAS_RouteCacheRecordsFit(const routecacheheader_t &routecacheheader, const byte *caches, int length,
	char *fault, int faultsize)
{
	const int numcaches = routecacheheader.numportalcache + routecacheheader.numareacache;
	int i, side, joined[2], number, reachabilities, size, at, *start, *fewest;
	const aas_areasettings_t *settings;

	//for each portal, then each number each cluster gives, the fewest
	//reachabilities of the areas a route query reads a cache there for, or
	//-1 where no area has the number: a query from an area in a cluster, or
	//from a portal on its border, reads an area cache at the number the
	//cluster gives the area, and one from a portal reads a portal cache at
	//its portal; where areas share a number, each reads the fewest
	start = botlib::Alloc<int>(aasworld.numclusters + 1);
	start[0] = std::max(aasworld.numportals, 0);
	for (i = 0; i < aasworld.numclusters; i++)
	{
		start[i + 1] = start[i] + std::max(aasworld.clusters[i].numareas, 0);
	} //end for
	fewest = botlib::Alloc<int>(std::max(start[aasworld.numclusters], 1));
	for (i = 0; i < start[aasworld.numclusters]; i++)
	{
		fewest[i] = -1;
	} //end for
	const auto fewer = [fewest](int slot, int count) {
		if (fewest[slot] < 0 || count < fewest[slot]) fewest[slot] = count;
	};
	for (i = 1; i < aasworld.numareas; i++)
	{
		settings = &aasworld.areasettings[i];
		reachabilities = std::max(settings->numreachableareas, 0);
		//an area is numbered in its cluster, a portal in each cluster it joins
		if (settings->cluster > 0)
		{
			joined[0] = settings->cluster;
			joined[1] = 0;
		} //end if
		else if (settings->cluster < 0 && settings->cluster > -aasworld.numportals)
		{
			fewer(-settings->cluster, reachabilities);
			joined[0] = aasworld.portals[-settings->cluster].frontcluster;
			joined[1] = aasworld.portals[-settings->cluster].backcluster;
		} //end else if
		else continue;
		for (side = 0; side < 2; side++)
		{
			if (joined[side] <= 0 || joined[side] >= aasworld.numclusters) continue;
			number = AAS_ClusterAreaNum(joined[side], i);
			if (number >= 0 && number < aasworld.clusters[joined[side]].numareas)
				fewer(start[joined[side]] + number, reachabilities);
		} //end for
	} //end for
	size = 1;
	for (i = 0, at = 0; size && i < numcaches; i++, at += size)
	{
		if (i < routecacheheader.numportalcache)
			size = AAS_RouteCacheRecordSize(i, true, caches + at, length - at, start, fewest, fault, faultsize);
		else
			size = AAS_RouteCacheRecordSize(i - routecacheheader.numportalcache, false, caches + at, length - at,
				start, fewest, fault, faultsize);
	} //end for
	if (size && at != length)
	{
		Com_sprintf(fault, faultsize, "has %d bytes after its last cache", length - at);
		size = 0;
	} //end if
	FreeMemory(fewest);
	FreeMemory(start);
	return size != 0;
} //end of the function AAS_RouteCacheRecordsFit
//===========================================================================
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//===========================================================================
#include <vector>
#include <cstddef>
#include "rtcw_routecache_loader.hpp"
#include "rtcw_routecache_writer.hpp"
#include "wolfet_navigation.h"
#include "wolfet_visibility.hpp"

static int AAS_ReadRouteCache(void)
{
	if (Com_GameIsRTCW()) return AAS_ReadRTCWRouteCache();
	int i, length, numread, numtraveltimes, clusterareanum;
	fileHandle_t fp;
	char filename[MAX_QPATH], fault[256];
	routecacheheader_t routecacheheader;
	aas_routingcache_t stored, *cache, **head;
	byte *caches;
	const byte *record;

	Com_sprintf(filename, MAX_QPATH, "maps/%s.rcd", aasworld.mapname);
	length = botimport.FS_FOpenFile( filename, &fp, FS_READ );
	if (!fp)
	{
		return qfalse;
	} //end if
	//a file shorter than the header is not a dump either
	if (length < static_cast<int>(sizeof(routecacheheader_t))
		|| botimport.FS_Read(&routecacheheader, sizeof(routecacheheader_t), fp ) != static_cast<int>(sizeof(routecacheheader_t))
		|| routecacheheader.ident != RCID)
	{
		AAS_Error("%s is not a route cache dump\n", filename);
		botimport.FS_FCloseFile(fp);
		return qfalse;
	} //end if
	if (routecacheheader.version != RCVERSION)
	{
		AAS_Error("route cache dump has wrong version %d, should be %d\n", routecacheheader.version, RCVERSION);
		botimport.FS_FCloseFile(fp);
		return qfalse;
	} //end if
	//a dump made for another AAS file is left alone
	if (routecacheheader.numareas != aasworld.numareas
		|| routecacheheader.numclusters != aasworld.numclusters
		|| routecacheheader.areacrc != CRC_ProcessArray(aasworld.areas, aasworld.numareas)
		|| routecacheheader.clustercrc != CRC_ProcessArray(aasworld.clusters, aasworld.numclusters))
	{
		botimport.FS_FCloseFile(fp);
		return qfalse;
	} //end if
	//the counts must fit the file before any of it is read
	length -= static_cast<int>(sizeof(routecacheheader_t));
	if (!AAS_RouteCacheCountsFit(routecacheheader, length, fault, sizeof(fault)))
	{
		AAS_Error("%s: %s\n", filename, fault);
		botimport.FS_FCloseFile(fp);
		return qfalse;
	} //end if
	caches = length > 0 ? botlib::AllocBytes<byte>(length) : nullptr;
	numread = length > 0 ? botimport.FS_Read(caches, length, fp) : 0;
	botimport.FS_FCloseFile(fp);
	//every cache is checked against the map before any is read into the routing
	if (numread != length)
	{
		Com_sprintf(fault, sizeof(fault), "could read %d of its %d bytes of caches", std::max(numread, 0), length);
	} //end if
	if (numread != length || !AAS_RouteCacheRecordsFit(routecacheheader, caches, length, fault, sizeof(fault)))
	{
		AAS_Error("%s: %s\n", filename, fault);
		if (caches) FreeMemory(caches);
		return qfalse;
	} //end if
	//read each cache into one the routing allocates, as it allocates its own,
	//and link it where routing looks for it, as the newest cache
	record = caches;
	for (i = 0; i < routecacheheader.numportalcache + routecacheheader.numareacache; i++)
	{
		memcpy(&stored, record, sizeof(stored));
		numtraveltimes = i < routecacheheader.numportalcache ? aasworld.numportals
			: aasworld.clusters[stored.cluster].numreachabilityareas;
		cache = AAS_AllocRoutingCache(numtraveltimes);
		cache->type = stored.type;
		cache->time = stored.time;
		cache->cluster = stored.cluster;
		cache->areanum = stored.areanum;
		VectorCopy(stored.origin, cache->origin);
		cache->starttraveltime = stored.starttraveltime;
		cache->travelflags = stored.travelflags;
		memcpy(reinterpret_cast<byte *>(cache) + offsetof(aas_routingcache_t, traveltimes),
			record + offsetof(aas_routingcache_t, traveltimes), numtraveltimes * sizeof(unsigned short int));
		memcpy(cache->reachabilities, record + sizeof(aas_routingcache_t) + numtraveltimes * sizeof(unsigned short int),
			numtraveltimes * sizeof(unsigned char));
		if (i < routecacheheader.numportalcache)
		{
			head = &aasworld.portalcache[cache->areanum];
		} //end if
		else
		{
			clusterareanum = AAS_ClusterAreaNum(cache->cluster, cache->areanum);
			head = &aasworld.clusterareacache[cache->cluster][clusterareanum];
		} //end else
		cache->prev = nullptr;
		cache->next = *head;
		if (*head) (*head)->prev = cache;
		*head = cache;
		AAS_LinkCache(cache);
		record += stored.size;
	} //end for
	// legacy visareas are intentionally not loaded from route cache dumps
	//
	if (caches) FreeMemory(caches);
	botimport.Print(PRT_MESSAGE, "route cache read from %s: %d portal and %d area caches, %d bytes\n",
		filename, routecacheheader.numportalcache, routecacheheader.numareacache, length);
	return qtrue;
} //end of the function AAS_ReadRouteCache
//===========================================================================
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//===========================================================================
static constexpr int MAX_REACHABILITYPASSAREAS = 32;

static void AAS_InitReachabilityAreas(void)
{
	int i, j, numareas, areas[MAX_REACHABILITYPASSAREAS];
	int numreachareas;
	aas_reachability_t *reach;
	vec3_t start, end;

	if (aasworld.reachabilityareas)
		FreeMemory(aasworld.reachabilityareas);
	if (aasworld.reachabilityareaindex)
		FreeMemory(aasworld.reachabilityareaindex);

	aasworld.reachabilityareas = botlib::ClearedAlloc<aas_reachabilityareas_t>(aasworld.reachabilitysize);
	aasworld.reachabilityareaindex = botlib::ClearedAlloc<int>(aasworld.reachabilitysize * MAX_REACHABILITYPASSAREAS);
	numreachareas = 0;
	for (i = 0; i < aasworld.reachabilitysize; i++)
	{
		reach = &aasworld.reachability[i];
		numareas = 0;
		switch(reach->traveltype & TRAVELTYPE_MASK)
		{
			//trace areas from start to end
			case TRAVEL_BARRIERJUMP:
			case TRAVEL_WATERJUMP:
				VectorCopy(reach->start, end);
				end[2] = reach->end[2];
				numareas = AAS_TraceAreas(reach->start, end, areas, nullptr, MAX_REACHABILITYPASSAREAS);
				break;
			case TRAVEL_WALKOFFLEDGE:
				VectorCopy(reach->end, start);
				start[2] = reach->start[2];
				numareas = AAS_TraceAreas(start, reach->end, areas, nullptr, MAX_REACHABILITYPASSAREAS);
				break;
			case TRAVEL_GRAPPLEHOOK:
				numareas = AAS_TraceAreas(reach->start, reach->end, areas, nullptr, MAX_REACHABILITYPASSAREAS);
				break;

			//trace arch
			case TRAVEL_JUMP: break;
			case TRAVEL_ROCKETJUMP: break;
			case TRAVEL_BFGJUMP: break;
			case TRAVEL_JUMPPAD: break;

			//trace from reach->start to entity center, along entity movement
			//and from entity center to reach->end
			case TRAVEL_ELEVATOR: break;
			case TRAVEL_FUNCBOB: break;

			//no areas in between
			case TRAVEL_WALK: break;
			case TRAVEL_CROUCH: break;
			case TRAVEL_LADDER: break;
			case TRAVEL_SWIM: break;
			case TRAVEL_TELEPORT: break;
			default: break;
		} //end switch
		aasworld.reachabilityareas[i].firstarea = numreachareas;
		aasworld.reachabilityareas[i].numareas = numareas;
		for (j = 0; j < numareas; j++)
		{
			aasworld.reachabilityareaindex[numreachareas++] = areas[j];
		} //end for
	} //end for
} //end of the function AAS_InitReachabilityAreas
//===========================================================================
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//===========================================================================
#include "rtcw_routecache_build.hpp"
#include "rtcw_routing_init.hpp"

static void AAS_InitRoutingLimits(void)
{
	if (Com_GameIsWolfET()) {
		// ET source observations: be_aas_route.c snapshots both limits at map
		// initialization. The server supplies the game-type-specific frame limit.
		aasworld.max_routingcachesize = fntech3::wolfet::navigation::CacheBudget(
			LibVarValue("max_routingcache", "16384"));
		aasworld.wolfetMaxFrameRoutingUpdates = fntech3::wolfet::navigation::FrameRoutingUpdates(
			LibVarValue("bot_frameroutingupdates", "1000"));
	} else {
		aasworld.max_routingcachesize = 1024 * (int) LibVarValue("max_routingcache", "12288");
	}
}

void AAS_InitRouting(void)
{
	if (Com_GameIsRTCW()) {
		if (!AAS_InitRTCWRouting()) botimport.Print(PRT_EXIT, "RTCW routing initialization failed\n");
		return;
	}
	AAS_InitTravelFlagFromType();
	//
	AAS_InitAreaContentsTravelFlags();
	//initialize the routing update fields
	AAS_InitRoutingUpdate();
	//create reversed reachability links used by the routing update algorithm
	AAS_CreateReversedReachability();
	//initialize the cluster cache
	AAS_InitClusterAreaCache();
	//initialize portal cache
	AAS_InitPortalCache();
	//initialize the area travel times
	AAS_CalculateAreaTravelTimes();
	//calculate the maximum travel times through portals
	AAS_InitPortalMaxTravelTimes();
	//get the areas reachabilities go through
	AAS_InitReachabilityAreas();
	//
#ifdef ROUTING_DEBUG
	numareacacheupdates = 0;
	numportalcacheupdates = 0;
#endif //ROUTING_DEBUG
	//
	aasworld.routingcachesize = 0;
	AAS_InitRoutingLimits();
	// read any routing cache if available
	AAS_ReadRouteCache();
	// ET derives waypoints and visibility itself; it has no cache to read.
	AAS_WolfETBuildVisibility();
} //end of the function AAS_InitRouting
//===========================================================================
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//===========================================================================
void AAS_FreeRTCWVisibility(void)
{
	if (aasworld.rtcwVisibilityOwnedRows && aasworld.rtcwVisibility)
		for (int area = 0; area < aasworld.numareas; ++area)
			if (aasworld.rtcwVisibility[area].data) FreeMemory(aasworld.rtcwVisibility[area].data);
	if (aasworld.rtcwVisibility) FreeMemory(aasworld.rtcwVisibility);
	aasworld.rtcwVisibility = nullptr;
	if (aasworld.rtcwVisibilityData) FreeMemory(aasworld.rtcwVisibilityData);
	aasworld.rtcwVisibilityData = nullptr;
	if (aasworld.rtcwDecodedVisibility) FreeMemory(aasworld.rtcwDecodedVisibility);
	aasworld.rtcwDecodedVisibility = nullptr;
	if (aasworld.rtcwVisibilityScratch) FreeMemory(aasworld.rtcwVisibilityScratch);
	aasworld.rtcwVisibilityScratch = nullptr;
	if (aasworld.rtcwWaypoints) FreeMemory(aasworld.rtcwWaypoints);
	aasworld.rtcwWaypoints = nullptr;
	aasworld.rtcwVisibilityArea = -1;
	aasworld.rtcwRouteCacheLoaded = qfalse;
	aasworld.rtcwVisibilityOwnedRows = qfalse;
	aasworld.rtcwRoutingBuilding = qfalse;
}

void AAS_FreeRoutingCaches(void)
{
	if (aasworld.rtcwTacticalNodes) FreeMemory(aasworld.rtcwTacticalNodes);
	aasworld.rtcwTacticalNodes = nullptr;
	aasworld.rtcwTacticalCapacity = 0;
	aasworld.rtcwTacticalToken = 0;
	AAS_FreeRTCWVisibility();
	AAS_WolfETFreeVisibility();
	if (aasworld.hidetraveltimes) FreeMemory(aasworld.hidetraveltimes);
	aasworld.hidetraveltimes = nullptr;
	// free all the existing cluster area cache
	AAS_FreeAllClusterAreaCache();
	// free all the existing portal cache
	AAS_FreeAllPortalCache();
	// free cached travel times within areas
	if (aasworld.areatraveltimes) FreeMemory(aasworld.areatraveltimes);
	aasworld.areatraveltimes = nullptr;
	// free the travel times within a goal area
	if (aasworld.startareatraveltimes) FreeMemory(aasworld.startareatraveltimes);
	aasworld.startareatraveltimes = nullptr;
	// free cached maximum travel time through cluster portals
	if (aasworld.portalmaxtraveltimes) FreeMemory(aasworld.portalmaxtraveltimes);
	aasworld.portalmaxtraveltimes = nullptr;
	// free reversed reachability links
	if (aasworld.reversedreachability) FreeMemory(aasworld.reversedreachability);
	aasworld.reversedreachability = nullptr;
	// free routing algorithm memory
	if (aasworld.areaupdate) FreeMemory(aasworld.areaupdate);
	aasworld.areaupdate = nullptr;
	if (aasworld.portalupdate) FreeMemory(aasworld.portalupdate);
	aasworld.portalupdate = nullptr;
	// free lists with areas the reachabilities go through
	if (aasworld.reachabilityareas) FreeMemory(aasworld.reachabilityareas);
	aasworld.reachabilityareas = nullptr;
	// free the reachability area index
	if (aasworld.reachabilityareaindex) FreeMemory(aasworld.reachabilityareaindex);
	aasworld.reachabilityareaindex = nullptr;
	// free area contents travel flags look up table
	if (aasworld.areacontentstravelflags) FreeMemory(aasworld.areacontentstravelflags);
	aasworld.areacontentstravelflags = nullptr;
} //end of the function AAS_FreeRoutingCaches
//===========================================================================
// update the given routing cache
//
// Parameter:			areacache		: routing cache to update
// Returns:				-
// Changes Globals:		-
//===========================================================================
static void AAS_UpdateAreaRoutingCache(aas_routingcache_t *areacache)
{
	const bool rtcw = Com_GameIsRTCW() != 0;
	const bool wolfet = Com_GameIsWolfET() != 0;
	//Elite Force Holomatch's engine starts from the goal area's first row of
	//travel times, as RTCW does, and enters any area whatever its
	//AREA_DISABLED flag (stvoyHM.exe 0x004992d0)
	const bool holomatch = Com_GameIsSTVEFMP() != 0;
	const bool goalrow = rtcw || wolfet || holomatch;
	int i, nextareanum, cluster, badtravelflags, clusterareanum, linknum;
	int numreachabilityareas;
	unsigned short int t;
	aas_routingupdate_t *updateliststart, *updatelistend, *curupdate, *nextupdate;
	aas_reachability_t *reach;
	const aas_reversedreachability_t *revreach;
	const aas_reversedlink_t *revlink;

#ifdef ROUTING_DEBUG
	numareacacheupdates++;
#endif //ROUTING_DEBUG
	//number of reachability areas within this cluster
	numreachabilityareas = aasworld.clusters[areacache->cluster].numreachabilityareas;
	//
	if (!rtcw && !wolfet) aasworld.frameroutingupdates++;
	//clear the routing update fields
	//
	badtravelflags = ~areacache->travelflags;
	//
	clusterareanum = AAS_ClusterAreaNum(areacache->cluster, areacache->areanum);
	if (clusterareanum < 0 || clusterareanum >= numreachabilityareas) return;
	// RTCW, ET and Holomatch start at the goal's first outgoing cost row. A
	// missing row leaves the new cache unreachable. The corresponding retail
	// engines exclude areas without reachabilities before reading their rows:
	// numbers those past a cluster's reachability areas, returned for above.
	if (goalrow && (!aasworld.areasettings[areacache->areanum].numreachableareas
		|| !aasworld.areatraveltimes || !aasworld.areatraveltimes[areacache->areanum]
		|| !aasworld.areatraveltimes[areacache->areanum][0])) return;
	//
	curupdate = &aasworld.areaupdate[clusterareanum];
	curupdate->areanum = areacache->areanum;
	//VectorCopy(areacache->origin, curupdate->start);
	//RTCW, ET (et.exe 0x0048132e) and Holomatch start from the goal's first
	//row of travel times, the times within it from each link's end to its first
	//reachability; Quake III from zeros, which quake3.exe 1.32 kept in a stack
	//array of 128 and read past for an area entered by more links
	curupdate->areatraveltimes = goalrow ? aasworld.areatraveltimes[areacache->areanum][0] : aasworld.startareatraveltimes;
	curupdate->tmptraveltime = areacache->starttraveltime;
	//
	areacache->traveltimes[clusterareanum] = areacache->starttraveltime;
	//put the area to start with in the current read list
	curupdate->next = nullptr;
	curupdate->prev = nullptr;
	updateliststart = curupdate;
	updatelistend = curupdate;
	//while there are updates in the current list
	while (updateliststart)
	{
		curupdate = updateliststart;
		//
		if (curupdate->next) curupdate->next->prev = nullptr;
		else updatelistend = nullptr;
		updateliststart = curupdate->next;
		//
		curupdate->inlist = qfalse;
		//check all reversed reachability links
		revreach = &aasworld.reversedreachability[curupdate->areanum];
		//
		for (i = 0, revlink = revreach->first; revlink; revlink = revlink->next, i++)
		{
			linknum = revlink->linknum;
			reach = &aasworld.reachability[linknum];
			//if there is used an undesired travel type
			if (AAS_TravelFlagForType_inline(reach->traveltype) & badtravelflags) continue;
			//if not allowed to enter the next area; Holomatch's engine does not ask
			if (!holomatch && (aasworld.areasettings[reach->areanum].areaflags & AREA_DISABLED)) continue;
			//if the next area has a not allowed travel flag
			if (AAS_AreaContentsTravelFlags_inline(reach->areanum) & badtravelflags) continue;
			//number of the area the reversed reachability leads to
			nextareanum = revlink->areanum;
			//get the cluster number of the area
			cluster = aasworld.areasettings[nextareanum].cluster;
			//don't leave the cluster
			if (cluster > 0 && cluster != areacache->cluster) continue;
			//get the number of the area in the cluster
			clusterareanum = AAS_ClusterAreaNum(areacache->cluster, nextareanum);
			if (clusterareanum < 0 || clusterareanum >= numreachabilityareas) continue;
			//time already travelled plus the traveltime through
			//the current area plus the travel time from the reachability
			t = curupdate->tmptraveltime +
						//AAS_AreaTravelTime(curupdate->areanum, curupdate->start, reach->end) +
						curupdate->areatraveltimes[i] +
							reach->traveltime;
			if (wolfet) t = fntech3::wolfet::navigation::AvoidTime(t,
				aasworld.areasettings[reach->areanum].areaflags, areacache->travelflags);
			if ((rtcw || wolfet) && aasworld.frameroutingupdates < INT_MAX) ++aasworld.frameroutingupdates;
			// ET does not publish a path into an area without its cost rows
			// (et.exe 0x004814a4). Also reject a missing individual row safely.
			const int nextrow = linknum - aasworld.areasettings[nextareanum].firstreachablearea;
			if (wolfet && (!aasworld.areatraveltimes[nextareanum]
				|| nextrow < 0 || nextrow >= aasworld.areasettings[nextareanum].numreachableareas
				|| !aasworld.areatraveltimes[nextareanum][nextrow])) continue;
			//
			if (!areacache->traveltimes[clusterareanum] ||
					areacache->traveltimes[clusterareanum] > t)
			{
				areacache->traveltimes[clusterareanum] = t;
				areacache->reachabilities[clusterareanum] = linknum - aasworld.areasettings[nextareanum].firstreachablearea;
				nextupdate = &aasworld.areaupdate[clusterareanum];
				nextupdate->areanum = nextareanum;
				nextupdate->tmptraveltime = t;
				//VectorCopy(reach->start, nextupdate->start);
				nextupdate->areatraveltimes = aasworld.areatraveltimes[nextareanum][nextrow];
				if (!nextupdate->inlist)
				{
					// we add the update to the end of the list
					// we could also use a B+ tree to have a real sorted list
					// on travel time which makes for faster routing updates
					nextupdate->next = nullptr;
					nextupdate->prev = updatelistend;
					if (updatelistend) updatelistend->next = nextupdate;
					else updateliststart = nextupdate;
					updatelistend = nextupdate;
					nextupdate->inlist = qtrue;
				} //end if
			} //end if
		} //end for
	} //end while
} //end of the function AAS_UpdateAreaRoutingCache
//===========================================================================
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//===========================================================================
static aas_routingcache_t *AAS_GetAreaRoutingCache(int clusternum, int areanum, int travelflags, bool forceUpdate = false)
{
	int clusterareanum;
	aas_routingcache_t *cache, *clustercache;

	//number of the area in the cluster
	clusterareanum = AAS_ClusterAreaNum(clusternum, areanum);
	//pointer to the cache for the area in the cluster
	clustercache = aasworld.clusterareacache[clusternum][clusterareanum];
	//find the cache without undesired travel flags
	for (cache = clustercache; cache; cache = cache->next)
	{
		//if there aren't used any undesired travel types for the cache
		if (cache->travelflags == travelflags) break;
	} //end for
	//if there was no cache
	if (!cache)
	{
		// Published RTCW defers new ordinary area caches after 100 eligible
		// link updates; ET uses its configured map-initialization limit.
		// Portal construction can finish its dependent caches.
		if (Com_GameIsRTCW() && !forceUpdate && aasworld.frameroutingupdates > 100) return nullptr;
		if (Com_GameIsWolfET() && !forceUpdate
			&& aasworld.frameroutingupdates > aasworld.wolfetMaxFrameRoutingUpdates) return nullptr;
		cache = AAS_AllocRoutingCache(aasworld.clusters[clusternum].numreachabilityareas);
		cache->cluster = clusternum;
		cache->areanum = areanum;
		VectorCopy(aasworld.areas[areanum].center, cache->origin);
		cache->starttraveltime = 1;
		cache->travelflags = travelflags;
		cache->prev = nullptr;
		cache->next = clustercache;
		if (clustercache) clustercache->prev = cache;
		aasworld.clusterareacache[clusternum][clusterareanum] = cache;
		AAS_UpdateAreaRoutingCache(cache);
	} //end if
	else
	{
		AAS_UnlinkCache(cache);
	} //end else
	//the cache has been accessed
	cache->time = AAS_RoutingTime();
	cache->type = CACHETYPE_AREA;
	AAS_LinkCache(cache);
	return cache;
} //end of the function AAS_GetAreaRoutingCache
//===========================================================================
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//===========================================================================
static void AAS_UpdatePortalRoutingCache(aas_routingcache_t *portalcache)
{
	int i, portalnum, clusterareanum, clusternum;
	unsigned short int t;
	aas_portal_t *portal;
	aas_cluster_t *cluster;
	aas_routingcache_t *cache;
	aas_routingupdate_t *updateliststart, *updatelistend, *curupdate, *nextupdate;

#ifdef ROUTING_DEBUG
	numportalcacheupdates++;
#endif //ROUTING_DEBUG
	//clear the routing update fields
	//
	curupdate = &aasworld.portalupdate[aasworld.numportals];
	curupdate->cluster = portalcache->cluster;
	curupdate->areanum = portalcache->areanum;
	curupdate->tmptraveltime = portalcache->starttraveltime;
	//if the start area is a cluster portal, store the travel time for that portal
	clusternum = aasworld.areasettings[portalcache->areanum].cluster;
	if (clusternum < 0)
	{
		portalcache->traveltimes[-clusternum] = portalcache->starttraveltime;
	} //end if
	//put the area to start with in the current read list
	curupdate->next = nullptr;
	curupdate->prev = nullptr;
	updateliststart = curupdate;
	updatelistend = curupdate;
	//while there are updates in the current list
	while (updateliststart)
	{
		curupdate = updateliststart;
		//remove the current update from the list
		if (curupdate->next) curupdate->next->prev = nullptr;
		else updatelistend = nullptr;
		updateliststart = curupdate->next;
		//current update is removed from the list
		curupdate->inlist = qfalse;
		//
		cluster = &aasworld.clusters[curupdate->cluster];
		//
		cache = AAS_GetAreaRoutingCache(curupdate->cluster,
								curupdate->areanum, portalcache->travelflags, true);
		//take all portals of the cluster
		for (i = 0; i < cluster->numportals; i++)
		{
			portalnum = aasworld.portalindex[cluster->firstportal + i];
			portal = &aasworld.portals[portalnum];
			//if this is the portal of the current update continue
			if (portal->areanum == curupdate->areanum) continue;
			//
			clusterareanum = AAS_ClusterAreaNum(curupdate->cluster, portal->areanum);
			if (clusterareanum >= cluster->numreachabilityareas) continue;
			//
			t = cache->traveltimes[clusterareanum];
			if (!t) continue;
			t += curupdate->tmptraveltime;
			//
			if (!portalcache->traveltimes[portalnum] ||
					portalcache->traveltimes[portalnum] > t)
			{
				portalcache->traveltimes[portalnum] = t;
				if (Com_GameIsRTCW()) portalcache->reachabilities[portalnum] = cache->reachabilities[clusterareanum];
				nextupdate = &aasworld.portalupdate[portalnum];
				if (portal->frontcluster == curupdate->cluster)
				{
					nextupdate->cluster = portal->backcluster;
				} //end if
				else
				{
					nextupdate->cluster = portal->frontcluster;
				} //end else
				nextupdate->areanum = portal->areanum;
				//add travel time through the actual portal area for the next update
				nextupdate->tmptraveltime = t + aasworld.portalmaxtraveltimes[portalnum];
				if (!nextupdate->inlist)
				{
					// we add the update to the end of the list
					// we could also use a B+ tree to have a real sorted list
					// on travel time which makes for faster routing updates
					nextupdate->next = nullptr;
					nextupdate->prev = updatelistend;
					if (updatelistend) updatelistend->next = nextupdate;
					else updateliststart = nextupdate;
					updatelistend = nextupdate;
					nextupdate->inlist = qtrue;
				} //end if
			} //end if
		} //end for
	} //end while
} //end of the function AAS_UpdatePortalRoutingCache
//===========================================================================
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//===========================================================================
static aas_routingcache_t *AAS_GetPortalRoutingCache(int clusternum, int areanum, int travelflags)
{
	aas_routingcache_t *cache;

	//find the cached portal routing if existing
	for (cache = aasworld.portalcache[areanum]; cache; cache = cache->next)
	{
		if (cache->travelflags == travelflags) break;
	} //end for
	//if the portal routing isn't cached
	if (!cache)
	{
		cache = AAS_AllocRoutingCache(aasworld.numportals);
		cache->cluster = clusternum;
		cache->areanum = areanum;
		VectorCopy(aasworld.areas[areanum].center, cache->origin);
		cache->starttraveltime = 1;
		cache->travelflags = travelflags;
		//add the cache to the cache list
		cache->prev = nullptr;
		cache->next = aasworld.portalcache[areanum];
		if (aasworld.portalcache[areanum]) aasworld.portalcache[areanum]->prev = cache;
		aasworld.portalcache[areanum] = cache;
		//update the cache
		AAS_UpdatePortalRoutingCache(cache);
	} //end if
	else
	{
		AAS_UnlinkCache(cache);
	} //end else
	//the cache has been accessed
	cache->time = AAS_RoutingTime();
	cache->type = CACHETYPE_PORTAL;
	AAS_LinkCache(cache);
	return cache;
} //end of the function AAS_GetPortalRoutingCache
//===========================================================================
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//===========================================================================
static int AAS_AreaRouteToGoalArea(int areanum, vec3_t origin, int goalareanum, int travelflags, int *traveltime, int *reachnum)
{
	int clusternum, goalclusternum, portalnum, i, clusterareanum, bestreachnum;
	unsigned short int t, besttime;
	aas_portal_t *portal;
	aas_cluster_t *cluster;
	aas_routingcache_t *areacache, *portalcache;
	aas_reachability_t *reach;

	if (!aasworld.initialized && !(Com_GameIsRTCW() && aasworld.rtcwRoutingBuilding)) return qfalse;

	if (areanum == goalareanum)
	{
		*traveltime = 1;
		*reachnum = 0;
		return qtrue;
	}
	//check !AAS_AreaReachability(areanum) with custom developer-only debug message
	if (areanum <= 0 || areanum >= aasworld.numareas)
	{
		if (botDeveloper)
		{
			botimport.Print(PRT_ERROR, "AAS_AreaTravelTimeToGoalArea: areanum %d out of range\n", areanum);
		} //end if
		return qfalse;
	} //end if
	if (goalareanum <= 0 || goalareanum >= aasworld.numareas)
	{
		if (botDeveloper)
		{
			botimport.Print(PRT_ERROR, "AAS_AreaTravelTimeToGoalArea: goalareanum %d out of range\n", goalareanum);
		} //end if
		return qfalse;
	} //end if
	if (!Com_GameIsRTCW() && (!aasworld.areasettings[areanum].numreachableareas || !aasworld.areasettings[goalareanum].numreachableareas))
	{
		return qfalse;
	} //end if
	// Cluster zero denotes geometry outside the routing graph. Do not index
	// an empty cluster cache when a module supplies one of those area numbers.
	if (Com_GameIsRTCW() && (!aasworld.areasettings[areanum].cluster
		|| !aasworld.areasettings[goalareanum].cluster)) return qfalse;

	// make sure the routing cache doesn't grow to large
	while ( aasworld.routingcachesize > aasworld.max_routingcachesize ) {
		if ( !AAS_FreeOldestCache() ) {
			break;
		}
	}

	//
	if (AAS_AreaDoNotEnter(areanum) || AAS_AreaDoNotEnter(goalareanum))
	{
		travelflags |= TFL_DONOTENTER;
	} //end if
	if ((Com_GameIsRTCW() || Com_GameIsWolfET()) && ((aasworld.areasettings[areanum].contents
		| aasworld.areasettings[goalareanum].contents) & 1024)) {
		travelflags |= fntech3::rtcw::routing::DoNotEnterLarge;
	}
	//NOTE: the number of routing updates is limited per frame
	/*
	if (aasworld.frameroutingupdates > MAX_FRAMEROUTINGUPDATES)
	{
#ifdef DEBUG
		//Log_Write("WARNING: AAS_AreaTravelTimeToGoalArea: frame routing updates overflowed");
#endif
		return 0;
	} //end if
	*/
	//
	clusternum = aasworld.areasettings[areanum].cluster;
	goalclusternum = aasworld.areasettings[goalareanum].cluster;
	//check if the area is a portal of the goal area cluster
	if (clusternum < 0 && goalclusternum > 0)
	{
		portal = &aasworld.portals[-clusternum];
		if (portal->frontcluster == goalclusternum ||
				portal->backcluster == goalclusternum)
		{
			clusternum = goalclusternum;
		} //end if
	} //end if
	//check if the goalarea is a portal of the area cluster
	else if (clusternum > 0 && goalclusternum < 0)
	{
		portal = &aasworld.portals[-goalclusternum];
		if (portal->frontcluster == clusternum ||
				portal->backcluster == clusternum)
		{
			goalclusternum = clusternum;
		} //end if
	} //end if
	//if both areas are in the same cluster
	//NOTE: there might be a shorter route via another cluster!!! but we don't care
	if (clusternum > 0 && goalclusternum > 0 && clusternum == goalclusternum)
	{
		//
		areacache = AAS_GetAreaRoutingCache(clusternum, goalareanum, travelflags);
		if (!areacache) return qfalse;
		//the number of the area in the cluster
		clusterareanum = AAS_ClusterAreaNum(clusternum, areanum);
		//the cluster the area is in
		cluster = &aasworld.clusters[clusternum];
		//if the area is NOT a reachability area
		if (clusterareanum >= cluster->numreachabilityareas) return 0;
		//if it is possible to travel to the goal area through this cluster
		if (areacache->traveltimes[clusterareanum] != 0)
		{
			*reachnum = aasworld.areasettings[areanum].firstreachablearea +
							areacache->reachabilities[clusterareanum];
			if (!origin) {
				*traveltime = areacache->traveltimes[clusterareanum];
				return qtrue;
			}
			reach = &aasworld.reachability[*reachnum];
			*traveltime = areacache->traveltimes[clusterareanum] +
							AAS_AreaTravelTime(areanum, origin, reach->start);
			//
			return qtrue;
		} //end if
	} //end if
	//
	clusternum = aasworld.areasettings[areanum].cluster;
	goalclusternum = aasworld.areasettings[goalareanum].cluster;
	//if the goal area is a portal
	if (goalclusternum < 0)
	{
		//just assume the goal area is part of the front cluster
		portal = &aasworld.portals[-goalclusternum];
		goalclusternum = portal->frontcluster;
	} //end if
	//get the portal routing cache
	portalcache = AAS_GetPortalRoutingCache(goalclusternum, goalareanum, travelflags);
	//if the area is a cluster portal, read directly from the portal cache
	if (clusternum < 0)
	{
		*traveltime = portalcache->traveltimes[-clusternum];
		*reachnum = aasworld.areasettings[areanum].firstreachablearea +
						portalcache->reachabilities[-clusternum];
		return qtrue;
	} //end if
	//
	besttime = 0;
	bestreachnum = -1;
	//the cluster the area is in
	cluster = &aasworld.clusters[clusternum];
	if (Com_GameIsRTCW() && AAS_ClusterAreaNum(clusternum, areanum) >= cluster->numreachabilityareas) return qfalse;
	//find the portal of the area cluster leading towards the goal area
	for (i = 0; i < cluster->numportals; i++)
	{
		portalnum = aasworld.portalindex[cluster->firstportal + i];
		//if the goal area isn't reachable from the portal
		if (!portalcache->traveltimes[portalnum]) continue;
		//
		portal = &aasworld.portals[portalnum];
		if (Com_GameIsRTCW() && (aasworld.areasettings[portal->areanum].areaflags & AREA_DISABLED)) continue;
		//get the cache of the portal area
		areacache = AAS_GetAreaRoutingCache(clusternum, portal->areanum, travelflags);
		if (!areacache) return qfalse;
		//current area inside the current cluster
		clusterareanum = AAS_ClusterAreaNum(clusternum, areanum);
		//if the area is NOT a reachability area
		if (clusterareanum >= cluster->numreachabilityareas) continue;
		//if the portal is NOT reachable from this area
		if (!areacache->traveltimes[clusterareanum]) continue;
		//total travel time is the travel time the portal area is from
		//the goal area plus the travel time towards the portal area
		t = portalcache->traveltimes[portalnum] + areacache->traveltimes[clusterareanum];
		//FIXME: add the exact travel time through the actual portal area
		//NOTE: for now we just add the largest travel time through the portal area
		//		because we can't directly calculate the exact travel time
		//		to be more specific we don't know which reachability was used to travel
		//		into the portal area
		t += aasworld.portalmaxtraveltimes[portalnum];
		//
		// RTCW also needs the first reachability when origin is omitted.
		if (Com_GameIsRTCW()) {
			*reachnum = aasworld.areasettings[areanum].firstreachablearea
				+ areacache->reachabilities[clusterareanum];
		}
		if (origin)
		{
			*reachnum = aasworld.areasettings[areanum].firstreachablearea +
							areacache->reachabilities[clusterareanum];
			reach = aasworld.reachability + *reachnum;
			t += AAS_AreaTravelTime(areanum, origin, reach->start);
		} //end if
		//if the time is better than the one already found
		if (!besttime || t < besttime)
		{
			bestreachnum = *reachnum;
			besttime = t;
		} //end if
	} //end for
	if (bestreachnum < 0) {
		return qfalse;
	}
	*reachnum = bestreachnum;
	*traveltime = besttime;
	return qtrue;
} //end of the function AAS_AreaRouteToGoalArea
//===========================================================================
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//===========================================================================
int AAS_AreaTravelTimeToGoalArea(int areanum, vec3_t origin, int goalareanum, int travelflags)
{
	int traveltime, reachnum = 0;

	if (AAS_AreaRouteToGoalArea(areanum, origin, goalareanum, travelflags, &traveltime, &reachnum))
	{
		return traveltime;
	}
	return 0;
} //end of the function AAS_AreaTravelTimeToGoalArea

bool AAS_RTCWMovementRoutingReady()
{
	// Deep geometry and cache-index validation belongs to AAS initialization.
	// These checks also make movement queries safe during partial teardown.
	return Com_GameIsRTCW() && aasworld.loaded && aasworld.initialized
		&& aasworld.numareas > 1 && aasworld.numareas <= 65535
		&& aasworld.numareasettings == aasworld.numareas && aasworld.areasettings
		&& aasworld.areas && aasworld.reachabilitysize > 0 && aasworld.reachability
		&& aasworld.clusters && aasworld.numclusters > 0
		&& aasworld.portals && aasworld.numportals > 0
		&& aasworld.portalindexsize >= 0 && (aasworld.portalindexsize == 0 || aasworld.portalindex)
		&& aasworld.areacontentstravelflags && aasworld.rtcwGroundSteepness
		&& aasworld.areatraveltimes && aasworld.reversedreachability
		&& aasworld.areaupdate && aasworld.portalupdate && aasworld.portalmaxtraveltimes
		&& aasworld.clusterareacache && aasworld.portalcache;
}

int AAS_RTCWMovementReachability(const float *origin, int area, int lastGoal,
	int previousArea, int goal, int routeFlags, int movementFlags)
{
	if (!AAS_RTCWMovementRoutingReady() || !origin || area <= 0 || area >= aasworld.numareas
		|| goal <= 0 || goal >= aasworld.numareas) return 0;
	vec3_t from;
	for (unsigned axis = 0; axis < 3; ++axis) {
		if (!fntech3::rtcw::routing::Coordinate(origin[axis])) return 0;
		from[axis] = origin[axis];
	}
	const auto &settings = aasworld.areasettings[area];
	const int first = settings.firstreachablearea, count = settings.numreachableareas;
	// Zero is the original iterator's sentinel, even if storage exists there.
	// An empty or invalid span cannot borrow the following area's links.
	if (first <= 0 || count <= 0 || first >= aasworld.reachabilitysize
		|| count > aasworld.reachabilitysize - first) return 0;
	const int endpointContents = settings.contents | aasworld.areasettings[goal].contents;
	const int entryFlags = ((endpointContents & AREACONTENTS_DONOTENTER) ? TFL_DONOTENTER : 0)
		| ((endpointContents & 1024) ? fntech3::rtcw::routing::DoNotEnterLarge : 0);
	routeFlags |= entryFlags;
	movementFlags |= entryFlags;
	const bool singlePlayer = Com_GameIsRTCWSP();
	int best = 0, bestCost = 0;
	for (int number = first; number < first + count; ++number) {
		const auto &reach = aasworld.reachability[number];
		if (reach.areanum <= 0 || reach.areanum >= aasworld.numareas
			|| (lastGoal == goal && reach.areanum == previousArea)
			|| (AAS_TravelFlagForType(reach.traveltype) & ~movementFlags)
			|| (AAS_AreaContentsTravelFlags(reach.areanum) & ~movementFlags)
			|| (singlePlayer && !AAS_AreaReachability(reach.areanum))) continue;
		vec3_t destination;
		VectorCopy(reach.end, destination);
		int cost, next;
		if (!AAS_AreaRouteToGoalArea(reach.areanum, destination, goal, routeFlags, &cost, &next)
			|| cost <= 0) continue;
		// SP rejects only a first routing step back into the current area.
		// A same-area route deliberately checks the stored dummy record zero.
		if (singlePlayer && (next < 0 || next >= aasworld.reachabilitysize
			|| aasworld.reachability[next].areanum == area)) continue;
		cost += reach.traveltime;
		if (!singlePlayer) {
			vec3_t start;
			VectorCopy(reach.start, start);
			cost += AAS_AreaTravelTime(area, from, start);
		}
		if (!best || cost < bestCost) {
			best = number;
			bestCost = cost;
		}
	}
	return best;
}

bool AAS_RTCWMovementFuturePosition(int reachNumber, const float *goalOrigin, int goalArea,
	int travelFlags, float *position)
{
	if (!AAS_RTCWMovementRoutingReady() || !goalOrigin || !position
		|| goalArea <= 0 || goalArea >= aasworld.numareas || reachNumber <= 0
		|| reachNumber >= aasworld.reachabilitysize) return false;
	for (int axis = 0; axis < 3; ++axis)
		if (!fntech3::rtcw::routing::Coordinate(goalOrigin[axis])) return false;
	auto current = aasworld.reachability[reachNumber];
	const auto valid = [&]() {
		if (current.areanum <= 0 || current.areanum >= aasworld.numareas) return false;
		for (int axis = 0; axis < 3; ++axis)
			if (!fntech3::rtcw::routing::Coordinate(current.end[axis])) return false;
		return true;
	};
	if (!valid()) return false;
	if (current.areanum == goalArea) {
		VectorCopy(goalOrigin, position);
		return true;
	}
	int initialCost, next;
	if (!AAS_AreaRouteToGoalArea(current.areanum, current.end, goalArea, travelFlags, &initialCost, &next))
		return false;
	if (!Com_GameIsRTCWSP()) {
		// A route cache can name stored record zero. Only the movement-link
		// iterator treats zero as its stop sentinel.
		if (next < 0 || next >= aasworld.reachabilitysize) return false;
		current = aasworld.reachability[next];
		if (!valid()) return false;
		VectorCopy(current.end, position);
		return true;
	}
	int previousCost = 999999;
	// Original SP allows equal costs. Bound cycles without discarding valid
	// equal-cost steps, and publish only a completed look-ahead target.
	for (int step = 0; step < 4096; ++step) {
		int cost;
		if (!AAS_AreaRouteToGoalArea(current.areanum, current.end, goalArea, travelFlags, &cost, &next)
			|| cost > previousCost || next < 0 || next >= aasworld.reachabilitysize) return false;
		previousCost = cost;
		current = aasworld.reachability[next];
		if (!valid()) return false;
		if (current.areanum == goalArea) {
			VectorCopy(goalOrigin, position);
			return true;
		}
		if (initialCost - cost > 120) {
			VectorCopy(current.end, position);
			return true;
		}
	}
	return false;
}
//===========================================================================
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//===========================================================================
static int AAS_AreaReachabilityToGoalArea(int areanum, vec3_t origin, int goalareanum, int travelflags)
{
	int traveltime, reachnum = 0;

	if (AAS_AreaRouteToGoalArea(areanum, origin, goalareanum, travelflags, &traveltime, &reachnum))
	{
		return reachnum;
	}
	return 0;
} //end of the function AAS_AreaReachabilityToGoalArea
//===========================================================================
// predict the route and stop on one of the stop events
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//===========================================================================
int AAS_PredictRoute(aas_predictroute_t *route, int areanum, vec3_t origin,
							int goalareanum, int travelflags, int maxareas, int maxtime,
							int stopevent, int stopcontents, int stoptfl, int stopareanum)
{
	int curareanum, reachnum, i, j, testareanum;
	vec3_t curorigin;
	aas_reachability_t *reach;
	aas_reachabilityareas_t *reachareas;

	//init output
	route->stopevent = RSE_NONE;
	route->endarea = goalareanum;
	route->endcontents = 0;
	route->endtravelflags = 0;
	VectorCopy(origin, route->endpos);
	route->time = 0;

	curareanum = areanum;
	VectorCopy(origin, curorigin);

	for (i = 0; curareanum != goalareanum && (!maxareas || i < maxareas) && i < aasworld.numareas; i++)
	{
		reachnum = AAS_AreaReachabilityToGoalArea(curareanum, curorigin, goalareanum, travelflags);
		if (!reachnum)
		{
			route->stopevent = RSE_NOROUTE;
			return qfalse;
		} //end if
		reach = &aasworld.reachability[reachnum];
		//
		if (stopevent & RSE_USETRAVELTYPE)
		{
			if (AAS_TravelFlagForType_inline(reach->traveltype) & stoptfl)
			{
				route->stopevent = RSE_USETRAVELTYPE;
				route->endarea = curareanum;
				route->endcontents = aasworld.areasettings[curareanum].contents;
				route->endtravelflags = AAS_TravelFlagForType_inline(reach->traveltype);
				VectorCopy(reach->start, route->endpos);
				return qtrue;
			} //end if
			if (AAS_AreaContentsTravelFlags_inline(reach->areanum) & stoptfl)
			{
				route->stopevent = RSE_USETRAVELTYPE;
				route->endarea = reach->areanum;
				route->endcontents = aasworld.areasettings[reach->areanum].contents;
				route->endtravelflags = AAS_AreaContentsTravelFlags_inline(reach->areanum);
				VectorCopy(reach->end, route->endpos);
				route->time += AAS_AreaTravelTime(areanum, origin, reach->start);
				route->time += reach->traveltime;
				return qtrue;
			} //end if
		} //end if
		reachareas = &aasworld.reachabilityareas[reachnum];
		for (j = 0; j < reachareas->numareas + 1; j++)
		{
			if (j >= reachareas->numareas)
				testareanum = reach->areanum;
			else
				testareanum = aasworld.reachabilityareaindex[reachareas->firstarea + j];
			if (stopevent & RSE_ENTERCONTENTS)
			{
				if (aasworld.areasettings[testareanum].contents & stopcontents)
				{
					route->stopevent = RSE_ENTERCONTENTS;
					route->endarea = testareanum;
					route->endcontents = aasworld.areasettings[testareanum].contents;
					VectorCopy(reach->end, route->endpos);
					route->time += AAS_AreaTravelTime(areanum, origin, reach->start);
					route->time += reach->traveltime;
					return qtrue;
				} //end if
			} //end if
			if (stopevent & RSE_ENTERAREA)
			{
				if (testareanum == stopareanum)
				{
					route->stopevent = RSE_ENTERAREA;
					route->endarea = testareanum;
					route->endcontents = aasworld.areasettings[testareanum].contents;
					VectorCopy(reach->start, route->endpos);
					return qtrue;
				} //end if
			} //end if
		} //end for

		route->time += AAS_AreaTravelTime(areanum, origin, reach->start);
		route->time += reach->traveltime;
		route->endarea = reach->areanum;
		route->endcontents = aasworld.areasettings[reach->areanum].contents;
		route->endtravelflags = AAS_TravelFlagForType_inline(reach->traveltype);
		VectorCopy(reach->end, route->endpos);
		//
		curareanum = reach->areanum;
		VectorCopy(reach->end, curorigin);
		//
		if (maxtime && route->time > maxtime)
			break;
	} //end while
	if (curareanum != goalareanum)
		return qfalse;
	return qtrue;
} //end of the function AAS_PredictRoute
#if 0
//===========================================================================
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//===========================================================================
int AAS_BridgeWalkable(int areanum)
{
	return qfalse;
} //end of the function AAS_BridgeWalkable
#endif
//===========================================================================
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//===========================================================================
void AAS_ReachabilityFromNum(int num, aas_reachability_t *reach)
{
	if (!aasworld.initialized)
	{
		*reach = {};
		return;
	} //end if
	if (num < 0 || num >= aasworld.reachabilitysize)
	{
		*reach = {};
		return;
	} //end if
	*reach = aasworld.reachability[num];
} //end of the function AAS_ReachabilityFromNum
//===========================================================================
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//===========================================================================
int AAS_NextAreaReachability(int areanum, int reachnum)
{
	aas_areasettings_t *settings;

	if (!aasworld.initialized) return 0;

	if (areanum <= 0 || areanum >= aasworld.numareas)
	{
		botimport.Print(PRT_ERROR, "AAS_NextAreaReachability: areanum %d out of range\n", areanum);
		return 0;
	} //end if

	settings = &aasworld.areasettings[areanum];
	if (Com_GameIsRTCW()) {
		const int first = settings->firstreachablearea, count = settings->numreachableareas;
		if (!aasworld.reachability || first <= 0 || count <= 0 || first >= aasworld.reachabilitysize
			|| count > aasworld.reachabilitysize - first
			|| (reachnum && (reachnum < first || reachnum >= first + count))) return 0;
	}
	if (!reachnum)
	{
		return settings->firstreachablearea;
	} //end if
	if (reachnum < settings->firstreachablearea)
	{
		botimport.Print(PRT_FATAL, "AAS_NextAreaReachability: reachnum < settings->firstreachableara");
		return 0;
	} //end if
	reachnum++;
	if (reachnum >= settings->firstreachablearea + settings->numreachableareas)
	{
		return 0;
	} //end if
	return reachnum;
} //end of the function AAS_NextAreaReachability
//===========================================================================
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//===========================================================================
int AAS_NextModelReachability(int num, int modelnum)
{
	int i;
	if (Com_GameIsRTCW() && (!aasworld.initialized || !aasworld.reachability)) return 0;

	if (num <= 0) num = 1;
	else if (num >= aasworld.reachabilitysize) return 0;
	else num++;
	//
	for (i = num; i < aasworld.reachabilitysize; i++)
	{
		const int travel = Com_GameIsRTCW() ? aasworld.reachability[i].traveltype
			: aasworld.reachability[i].traveltype & TRAVELTYPE_MASK;
		if (travel == TRAVEL_ELEVATOR)
		{
			if (aasworld.reachability[i].facenum == modelnum) return i;
		} //end if
		else if (travel == TRAVEL_FUNCBOB)
		{
			if ((aasworld.reachability[i].facenum & 0x0000FFFF) == modelnum) return i;
		} //end if
	} //end for
	return 0;
} //end of the function AAS_NextModelReachability
//===========================================================================
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//===========================================================================
int AAS_RandomGoalArea(int areanum, int travelflags, int *goalareanum, vec3_t goalorigin)
{
	int i, n, t;
	vec3_t start, end;
	aas_trace_t trace;

	//if the area has no reachabilities
	if (!AAS_AreaReachability(areanum)) return qfalse;
	//
	n = aasworld.numareas * random();
	for (i = 0; i < aasworld.numareas; i++)
	{
		if (n <= 0) n = 1;
		if (n >= aasworld.numareas) n = 1;
		if (AAS_AreaReachability(n))
		{
			t = AAS_AreaTravelTimeToGoalArea(areanum, aasworld.areas[areanum].center, n, travelflags);
			//if the goal is reachable
			if (t > 0)
			{
				if (AAS_AreaSwim(n))
				{
					*goalareanum = n;
					VectorCopy(aasworld.areas[n].center, goalorigin);
					//botimport.Print(PRT_MESSAGE, "found random goal area %d\n", *goalareanum);
					return qtrue;
				} //end if
				VectorCopy(aasworld.areas[n].center, start);
				if (!AAS_PointAreaNum(start))
					Log_Write("area %d center %f %f %f in solid?", n, start[0], start[1], start[2]);
				VectorCopy(start, end);
				end[2] -= 300;
				trace = AAS_TraceClientBBox(start, end, PRESENCE_CROUCH, -1);
				if (!trace.startsolid && trace.fraction < 1 && AAS_PointAreaNum(trace.endpos) == n)
				{
					if (AAS_AreaGroundFaceArea(n) > 300)
					{
						*goalareanum = n;
						VectorCopy(trace.endpos, goalorigin);
						//botimport.Print(PRT_MESSAGE, "found random goal area %d\n", *goalareanum);
						return qtrue;
					} //end if
				} //end if
			} //end if
		} //end if
		n++;
	} //end for
	return qfalse;
} //end of the function AAS_RandomGoalArea
//===========================================================================
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//===========================================================================
static int AAS_AreaVisible(int srcarea, int destarea)
{
	if (!Com_GameIsRTCW() || !aasworld.rtcwRouteCacheLoaded
		|| srcarea < 0 || srcarea >= aasworld.numareas
		|| destarea < 0 || destarea >= aasworld.numareas
		|| !aasworld.rtcwVisibility || !aasworld.rtcwDecodedVisibility)
		return qfalse;
	if (aasworld.rtcwVisibilityArea != srcarea) {
		const auto &row = aasworld.rtcwVisibility[srcarea];
		// Missing rows leave the previous decoded row intact, as in RTCW.
		if (!row.data || row.bytes <= 0) return qfalse;
		if (!fntech3::rtcw::routecache::DecompressVisibility(
			{row.data, static_cast<std::size_t>(row.bytes)}, aasworld.numareas,
			aasworld.rtcwDecodedVisibility, aasworld.numareas)) return qfalse;
		aasworld.rtcwVisibilityArea = srcarea;
	}
	return aasworld.rtcwDecodedVisibility[destarea];
} //end of the function AAS_AreaVisible

bool AAS_RTCWRouteFirstVisiblePosition(const float *source, const float *destination,
	int travelFlags, float *position)
{
	if (!Com_GameIsRTCWSP() || !aasworld.initialized || !source || !destination || !position)
		return false;
	vec3_t cursor, target;
	for (unsigned axis = 0; axis < 3; ++axis) {
		if (!fntech3::rtcw::routing::Coordinate(source[axis])
			|| !fntech3::rtcw::routing::Coordinate(destination[axis])) return false;
		cursor[axis] = source[axis];
		target[axis] = destination[axis];
	}
	int area = BotFuzzyPointReachabilityArea(cursor);
	if (area <= 0 || area >= aasworld.numareas) return false;
	const int goal = BotFuzzyPointReachabilityArea(target);
	if (goal <= 0 || goal >= aasworld.numareas) return false;
	if (area == goal || AAS_AreaVisible(area, goal)) {
		VectorCopy(cursor, position);
		return true;
	}
	// Original SP requires a visibility row for the destination before walking
	// the route, even though each lookup reads the traversed area's row.
	if (!aasworld.rtcwRouteCacheLoaded || !aasworld.rtcwVisibility
		|| !aasworld.rtcwVisibility[goal].data) return false;
	int previousCost = 0;
	for (unsigned step = 0; step < 200; ++step) {
		int cost, link;
		if (!AAS_AreaRouteToGoalArea(area, cursor, goal, travelFlags, &cost, &link)
			|| (step && cost >= previousCost)
			|| link < 0 || link >= aasworld.reachabilitysize || !aasworld.reachability)
			return false;
		previousCost = cost;
		const auto &next = aasworld.reachability[link];
		if (next.areanum <= 0 || next.areanum >= aasworld.numareas) return false;
		// Crossing directly into the destination returns the preceding position.
		// Otherwise, the first visible area contributes its entry point.
		if (next.areanum == goal) {
			VectorCopy(cursor, position);
			return true;
		}
		if (AAS_AreaVisible(next.areanum, goal)) {
			VectorCopy(next.end, position);
			return true;
		}
		area = next.areanum;
		VectorCopy(next.end, cursor);
	}
	return false;
}

#include "rtcw_tactical.hpp"

#if 0
//===========================================================================
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//===========================================================================
float DistancePointToLine(vec3_t v1, vec3_t v2, vec3_t point)
{
	vec3_t vec, p2;

	AAS_ProjectPointOntoVector(point, v1, v2, p2);
	VectorSubtract(point, p2, vec);
	return VectorLength(vec);
} //end of the function DistancePointToLine
#endif
//===========================================================================
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//===========================================================================
#include "wolfet_routing.hpp"

int AAS_NearestHideArea(int srcnum, vec3_t origin, int areanum, int enemynum, vec3_t enemyorigin, int enemyareanum, int travelflags)
{
	int i, j, nextareanum, badtravelflags, numreach, bestarea;
	unsigned short int t, besttraveltime;
	aas_routingupdate_t *updateliststart, *updatelistend, *curupdate, *nextupdate;
	aas_reachability_t *reach;
	float dist1, dist2;
	vec3_t v1, v2, p;
	qboolean startVisible;

	//
	if (!aasworld.hidetraveltimes)
	{
		aasworld.hidetraveltimes = botlib::ClearedAlloc<unsigned short int>(aasworld.numareas);
	} //end if
	else
	{
		std::fill_n(aasworld.hidetraveltimes, aasworld.numareas, static_cast<unsigned short int>(0));
	} //end else
	besttraveltime = 0;
	bestarea = 0;
	//assume visible
	startVisible = qtrue;
	//
	badtravelflags = ~travelflags;
	//
	curupdate = &aasworld.areaupdate[areanum];
	curupdate->areanum = areanum;
	VectorCopy(origin, curupdate->start);
	curupdate->areatraveltimes = aasworld.areatraveltimes[areanum][0];
	curupdate->tmptraveltime = 0;
	//put the area to start with in the current read list
	curupdate->next = nullptr;
	curupdate->prev = nullptr;
	updateliststart = curupdate;
	updatelistend = curupdate;
	//while there are updates in the list
	while (updateliststart)
	{
		curupdate = updateliststart;
		//
		if (curupdate->next) curupdate->next->prev = nullptr;
		else updatelistend = nullptr;
		updateliststart = curupdate->next;
		//
		curupdate->inlist = qfalse;
		//check all reversed reachability links
		numreach = aasworld.areasettings[curupdate->areanum].numreachableareas;
		reach = &aasworld.reachability[aasworld.areasettings[curupdate->areanum].firstreachablearea];
		//
		for (i = 0; i < numreach; i++, reach++)
		{
			//if an undesired travel type is used
			if (AAS_TravelFlagForType_inline(reach->traveltype) & badtravelflags) continue;
			//
			if (AAS_AreaContentsTravelFlags_inline(reach->areanum) & badtravelflags) continue;
			//number of the area the reachability leads to
			nextareanum = reach->areanum;
			// if this moves us into the enemies area, skip it
			if (nextareanum == enemyareanum) continue;
			//time already travelled plus the traveltime through
			//the current area plus the travel time from the reachability
			t = curupdate->tmptraveltime +
						AAS_AreaTravelTime(curupdate->areanum, curupdate->start, reach->start) +
							reach->traveltime;

			//avoid going near the enemy
			AAS_ProjectPointOntoVector(enemyorigin, curupdate->start, reach->end, p);
			for (j = 0; j < 3; j++)
				if ((p[j] > curupdate->start[j] && p[j] > reach->end[j]) ||
					(p[j] < curupdate->start[j] && p[j] < reach->end[j]))
					break;
			if (j < 3)
			{
				VectorSubtract(enemyorigin, reach->end, v2);
			} //end if
			else
			{
				VectorSubtract(enemyorigin, p, v2);
			} //end else
			dist2 = VectorLength(v2);
			//never go through the enemy
			if (dist2 < 40) continue;
			//
			VectorSubtract(enemyorigin, curupdate->start, v1);
			dist1 = VectorLength(v1);
			//
			if (dist2 < dist1)
			{
				t += (dist1 - dist2) * 10;
			}
			// if we weren't visible when starting, make sure we don't move into their view
			if (!startVisible && AAS_AreaVisible(enemyareanum, nextareanum)) {
				continue;
			}
			//
			if (besttraveltime && t >= besttraveltime) continue;
			//
			if (!aasworld.hidetraveltimes[nextareanum] ||
					aasworld.hidetraveltimes[nextareanum] > t)
			{
				//if the nextarea is not visible from the enemy area
				if (!AAS_AreaVisible(enemyareanum, nextareanum))
				{
					besttraveltime = t;
					bestarea = nextareanum;
				} //end if
				aasworld.hidetraveltimes[nextareanum] = t;
				nextupdate = &aasworld.areaupdate[nextareanum];
				nextupdate->areanum = nextareanum;
				nextupdate->tmptraveltime = t;
				//remember where we entered this area
				VectorCopy(reach->end, nextupdate->start);
				//if this update is not in the list yet
				if (!nextupdate->inlist)
				{
					//add the new update to the end of the list
					nextupdate->next = nullptr;
					nextupdate->prev = updatelistend;
					if (updatelistend) updatelistend->next = nextupdate;
					else updateliststart = nextupdate;
					updatelistend = nextupdate;
					nextupdate->inlist = qtrue;
				} //end if
			} //end if
		} //end for
	} //end while
	return bestarea;
} //end of the function AAS_NearestHideArea
