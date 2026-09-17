/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_WOLFET_ROUTING_HPP
#define FNTECH3_WOLFET_ROUTING_HPP

#include <functional>
#include <queue>
#include <utility>
#include <vector>

/* ET's reachability floods over the area waypoints and visibility built by
 * wolfet_visibility.hpp. Each keeps the original's traversal shape: one
 * breadth-first pass over the reachability links, `hidetraveltimes` used as the
 * visited marker rather than a travel time, and the persistent `areaupdate`
 * array as the work list. Observations:
 * docs/fntech3/2026-09-17-wolfet-visibility.md. */

namespace fntech3 {
namespace wolfet {
namespace routing {

struct Work {
	aas_routingupdate_t *start = nullptr;
	aas_routingupdate_t *end = nullptr;

	bool Begin( int area, const vec3_t origin ) {
		if ( area < 1 || area >= aasworld.numareas || !aasworld.areaupdate
			|| !aasworld.areatraveltimes ) {
			return false;
		}
		auto *update = &aasworld.areaupdate[area];
		update->areanum = area;
		VectorCopy( origin, update->start );
		update->areatraveltimes = aasworld.areatraveltimes[area][0];
		update->tmptraveltime = 0;
		update->next = nullptr;
		update->prev = nullptr;
		start = end = update;
		return true;
	}

	aas_routingupdate_t *Pop() {
		auto *update = start;
		if ( !update ) return nullptr;
		if ( update->next ) update->next->prev = nullptr;
		else end = nullptr;
		start = update->next;
		update->inlist = qfalse;
		return update;
	}

	void Push( aas_routingupdate_t *update ) {
		if ( update->inlist ) return;
		update->next = nullptr;
		update->prev = end;
		if ( end ) end->next = update;
		else start = update;
		end = update;
		update->inlist = qtrue;
	}
};

inline bool Ready() {
	return Com_GameIsWolfET() && aasworld.loaded && aasworld.wolfetVisibilityBuilt
		&& aasworld.wolfetWaypoints && aasworld.areasettings && aasworld.reachability;
}

// The original allocates this lazily and clears it at the top of every query.
inline bool ResetVisited() {
	if ( !aasworld.hidetraveltimes ) {
		aasworld.hidetraveltimes = botlib::ClearedAlloc<unsigned short int>( aasworld.numareas );
	} else {
		std::fill_n( aasworld.hidetraveltimes, aasworld.numareas,
			static_cast<unsigned short int>( 0 ) );
	}
	return aasworld.hidetraveltimes != nullptr;
}

inline bool Links( int area, int &first, int &count ) {
	const auto &settings = aasworld.areasettings[area];
	first = settings.firstreachablearea;
	count = settings.numreachableareas;
	return first >= 0 && count >= 0 && first <= aasworld.reachabilitysize
		&& count <= aasworld.reachabilitysize - first;
}

inline bool Travelable( const aas_reachability_t &reach, int badflags ) {
	if ( reach.traveltype < 0 || reach.traveltype >= MAX_TRAVELTYPES ) return false;
	if ( reach.areanum < 1 || reach.areanum >= aasworld.numareas ) return false;
	if ( aasworld.travelflagfortype[reach.traveltype] & badflags ) return false;
	return !( AAS_AreaContentsTravelFlags_inline( reach.areanum ) & badflags );
}

inline bool Blocked( int area ) {
	const int flags = aasworld.areasettings[area].areaflags;
	return ( flags & AREA_LADDER ) != 0 || ( flags & AREA_DISABLED ) != 0;
}


/* ET floods a cluster of mid-range areas through shared faces. The original
 * recurses; this walks an explicit stack so a large cluster cannot exhaust the
 * server thread's stack. The set of areas collected is the same. */
inline void FloodCluster( int areanum ) {
	aasworld.numclusterareas = 0;
	if ( !aasworld.clusterareas || !aasworld.midrangeareas || !aasworld.faces
		|| !aasworld.faceindex ) {
		return;
	}
	std::vector<int> pending{ areanum };
	while ( !pending.empty() ) {
		const int area = pending.back();
		pending.pop_back();
		if ( area < 1 || area >= aasworld.numareas ) continue;
		if ( !aasworld.midrangeareas[area].valid ) continue;
		if ( aasworld.numclusterareas >= aasworld.numareas ) return;
		aasworld.clusterareas[aasworld.numclusterareas++] = area;
		aasworld.midrangeareas[area].valid = qfalse;
		const aas_area_t &record = aasworld.areas[area];
		for ( int index = 0; index < record.numfaces; ++index ) {
			const int entry = record.firstface + index;
			if ( entry < 0 || entry >= aasworld.faceindexsize ) continue;
			const int faceIndex = abs( aasworld.faceindex[entry] );
			if ( faceIndex < 0 || faceIndex >= aasworld.numfaces ) continue;
			const aas_face_t &face = aasworld.faces[faceIndex];
			const int other = face.frontarea == area ? face.backarea : face.frontarea;
			if ( other < 1 || other >= aasworld.numareas ) continue;
			if ( !aasworld.midrangeareas[other].valid ) continue;
			pending.push_back( other );
		}
	}
}

} // namespace routing
} // namespace wolfet
} // namespace fntech3

/*
===========================================================================
AAS_WolfETListAreasInRange

Collects the waypoint of every reachable area within range of the source that
the source area can see, skipping ladder and disabled areas. Returns how many
were written. The original marks each area visited once, so an area reached by
a longer path is not reconsidered.
===========================================================================
*/
int AAS_WolfETListAreasInRange( vec3_t srcpos, int srcarea, float range, int travelflags,
	vec3_t *outareas, int maxareas ) {
	using namespace fntech3::wolfet::routing;

	if ( !Ready() || !outareas || maxareas <= 0 || !ResetVisited() ) return 0;

	Work work;
	if ( !work.Begin( srcarea, srcpos ) ) return 0;

	const int badflags = ~travelflags;
	int count = 0;
	while ( auto *current = work.Pop() ) {
		int first, links;
		if ( !Links( current->areanum, first, links ) ) break;
		for ( int index = 0; index < links; ++index ) {
			const auto &reach = aasworld.reachability[first + index];
			if ( !Travelable( reach, badflags ) ) continue;
			const int next = reach.areanum;
			if ( aasworld.hidetraveltimes[next] ) continue;
			aasworld.hidetraveltimes[next] = 1;
			const float *waypoint = AAS_WolfETWaypoint( next );
			if ( !waypoint || Distance( srcpos, waypoint ) > range ) continue;
			if ( !Blocked( next ) && AAS_WolfETAreaVisible( srcarea, next ) ) {
				VectorCopy( waypoint, outareas[count] );
				if ( ++count >= maxareas ) break;
			}
			auto *update = &aasworld.areaupdate[next];
			update->areanum = next;
			VectorCopy( reach.end, update->start );
			work.Push( update );
		}
		if ( count >= maxareas ) break;
	}
	return count;
}

/*
===========================================================================
AAS_WolfETAvoidDangerArea

Finds the cheapest reachable area that is far enough from the danger position
and has a waypoint of its own. A candidate reached quickly enough is taken
immediately; otherwise the cheapest within the time limit wins.
===========================================================================
*/
int AAS_WolfETAvoidDangerArea( vec3_t srcpos, int srcarea, vec3_t dangerpos, int dangerarea,
	float range, int travelflags ) {
	using namespace fntech3::wolfet::routing;
	(void)dangerarea;

	if ( !Ready() || !srcarea || !ResetVisited() ) return 0;

	Work work;
	if ( !work.Begin( srcarea, srcpos ) ) return 0;
	aasworld.hidetraveltimes[srcarea] = 1;

	constexpr int maxTime = 5000;
	constexpr int goodTime = 1000;
	const int badflags = ~travelflags;
	int best = 0;
	int bestTravel = 999999;

	while ( auto *current = work.Pop() ) {
		int first, links;
		if ( !Links( current->areanum, first, links ) ) break;
		for ( int index = 0; index < links; ++index ) {
			const auto &reach = aasworld.reachability[first + index];
			if ( !Travelable( reach, badflags ) ) continue;
			if ( Blocked( reach.areanum ) ) continue;
			const int next = reach.areanum;
			if ( aasworld.hidetraveltimes[next] ) continue;
			aasworld.hidetraveltimes[next] = 1;
			vec3_t reachStart;
			VectorCopy( reach.start, reachStart );
			const int travel = current->tmptraveltime
				+ AAS_AreaTravelTime( current->areanum, current->start, reachStart )
				+ reach.traveltime;
			if ( travel > maxTime || travel > bestTravel ) continue;
			const float *waypoint = AAS_WolfETWaypoint( next );
			if ( waypoint && Distance( dangerpos, waypoint ) > range ) {
				if ( travel < goodTime ) return next;
				if ( travel < bestTravel ) {
					bestTravel = travel;
					best = next;
				}
			}
			auto *update = &aasworld.areaupdate[next];
			update->areanum = next;
			VectorCopy( reach.end, update->start );
			/* The original never assigns tmptraveltime here, so a successor
			 * inherits whatever the shared work record held. Reproducing that
			 * keeps the same areas reachable within the time limits. */
			work.Push( update );
		}
	}
	return best;
}

//===========================================================================
// AAS_WolfETAlternativeRouteGoals
//
// ET's own alternative routing. It derives both area numbers itself, retrying
// the goal 30 units higher, selects route and cluster portals rather than the
// cluster and view portals Quake picks, widens the travel-time window to
// 500 + 3 * goal, publishes each goal at the area's waypoint rather than its
// centre, and drops a goal whose first reachability towards the start and
// towards the goal lead to the same area.
//===========================================================================
int AAS_WolfETAlternativeRouteGoals(vec3_t start, vec3_t goal, int travelflags,
									aas_altroutegoal_t *altroutegoals, int maxaltroutegoals)
{
	int i, j, startareanum, goalareanum, bestareanum;
	int numaltroutegoals, nummidrangeareas;
	int starttime, goaltime, goaltraveltime;
	float dist, bestdist;
	vec3_t mid, dir;
	int reachnum, time;

	if (!Com_GameIsWolfET() || !aasworld.loaded || !altroutegoals || maxaltroutegoals <= 0)
		return 0;
	if (!aasworld.midrangeareas || !aasworld.clusterareas) return 0;

	startareanum = AAS_PointAreaNum(start);
	if (!startareanum) return 0;
	goalareanum = AAS_PointAreaNum(goal);
	if (!goalareanum)
	{
		// The original lifts the goal once before giving up on it.
		VectorCopy(goal, dir);
		dir[2] += 30;
		goalareanum = AAS_PointAreaNum(dir);
		if (!goalareanum) return 0;
	}

	goaltraveltime = AAS_AreaTravelTimeToGoalArea(startareanum, start, goalareanum, travelflags);
	std::fill_n(aasworld.midrangeareas, aasworld.numareas, aas_midrangearea_t{});
	numaltroutegoals = 0;
	nummidrangeareas = 0;

	for (i = 1; i < aasworld.numareas; i++)
	{
		const int contents = aasworld.areasettings[i].contents;
		if (!(contents & AREACONTENTS_ROUTEPORTAL) && !(contents & AREACONTENTS_CLUSTERPORTAL))
			continue;
		if (!AAS_AreaReachability(i)) continue;
		starttime = AAS_AreaTravelTimeToGoalArea(startareanum, start, i, travelflags);
		if (!starttime) continue;
		if (starttime > 500 + 3.0 * goaltraveltime) continue;
		goaltime = AAS_AreaTravelTimeToGoalArea(i, nullptr, goalareanum, travelflags);
		if (!goaltime) continue;
		if (goaltime > 500 + 3.0 * goaltraveltime) continue;
		aasworld.midrangeareas[i].valid = qtrue;
		aasworld.midrangeareas[i].starttime = static_cast<unsigned short>(starttime);
		aasworld.midrangeareas[i].goaltime = static_cast<unsigned short>(goaltime);
		nummidrangeareas++;
	} //end for

	for (i = 1; i < aasworld.numareas; i++)
	{
		if (!aasworld.midrangeareas[i].valid) continue;
		fntech3::wolfet::routing::FloodCluster(i);
		if (!aasworld.numclusterareas) continue;
		VectorClear(mid);
		for (j = 0; j < aasworld.numclusterareas; j++)
			VectorAdd(mid, aasworld.areas[aasworld.clusterareas[j]].center, mid);
		VectorScale(mid, 1.0 / aasworld.numclusterareas, mid);
		bestdist = 999999;
		bestareanum = 0;
		for (j = 0; j < aasworld.numclusterareas; j++)
		{
			VectorSubtract(mid, aasworld.areas[aasworld.clusterareas[j]].center, dir);
			dist = VectorLength(dir);
			if (dist < bestdist)
			{
				bestdist = dist;
				bestareanum = aasworld.clusterareas[j];
			} //end if
		} //end for
		if (!bestareanum) continue;
		// An area with no waypoint cannot be published, and the original would
		// route from a cleared vector here.
		const float *waypoint = AAS_WolfETWaypoint(bestareanum);
		if (!waypoint) continue;
		vec3_t from;
		VectorCopy(waypoint, from);
		if (!AAS_AreaRouteToGoalArea(bestareanum, from, goalareanum, travelflags, &time, &reachnum))
			continue;
		if (reachnum < 0 || reachnum >= aasworld.reachabilitysize) continue;
		const int towardsGoal = aasworld.reachability[reachnum].areanum;
		if (!AAS_AreaRouteToGoalArea(bestareanum, from, startareanum, travelflags, &time, &reachnum))
			continue;
		if (reachnum < 0 || reachnum >= aasworld.reachabilitysize) continue;
		// Both directions leaving through the same area is not an alternative.
		if (towardsGoal == aasworld.reachability[reachnum].areanum) continue;

		VectorCopy(waypoint, altroutegoals[numaltroutegoals].origin);
		altroutegoals[numaltroutegoals].areanum = bestareanum;
		altroutegoals[numaltroutegoals].starttraveltime = aasworld.midrangeareas[bestareanum].starttime;
		altroutegoals[numaltroutegoals].goaltraveltime = aasworld.midrangeareas[bestareanum].goaltime;
		altroutegoals[numaltroutegoals].extratraveltime = static_cast<unsigned short>(
			(aasworld.midrangeareas[bestareanum].starttime
				+ aasworld.midrangeareas[bestareanum].goaltime) - goaltraveltime);
		numaltroutegoals++;
		if (numaltroutegoals >= maxaltroutegoals) break;
	} //end for
	(void)nummidrangeareas;
	return numaltroutegoals;
} //end of the function AAS_WolfETAlternativeRouteGoals


/*
===========================================================================
AAS_WolfETNearestHideArea

Floods outward from the source area and keeps the cheapest area the enemy
cannot see, penalising candidates that move towards the enemy and rejecting
those that end too close to it. The original bounds the work per frame and
lets a negative source number restart that budget; both are reproduced, and an
absent game callback means no candidate can be proved hidden.
===========================================================================
*/
int AAS_WolfETNearestHideArea( int srcnum, vec3_t origin, int areanum, int enemynum,
	vec3_t enemyorigin, int enemyareanum, int travelflags, float maxdist, vec3_t distpos ) {
	using namespace fntech3::wolfet::routing;

	constexpr int maxLoops = 3000;
	static float lastTime;
	static int loopCount;

	if ( !Ready() ) return 0;
	const auto &callbacks = AAS_WolfETCallbacks();
	if ( !callbacks.available || !callbacks.visible || !callbacks.available() ) return 0;

	if ( srcnum < 0 ) {
		srcnum = -srcnum - 1;
		lastTime = 0;
	}
	if ( lastTime == AAS_Time() && loopCount >= maxLoops ) return 0;
	if ( lastTime != AAS_Time() ) loopCount = 0;
	lastTime = AAS_Time();

	if ( !ResetVisited() ) return 0;

	Work work;
	if ( !work.Begin( areanum, origin ) ) return 0;

	vec3_t enemyVec;
	VectorSubtract( enemyorigin, origin, enemyVec );
	const float enemyDistance = VectorNormalize( enemyVec );
	(void)enemyDistance;
	const int startVisible = callbacks.visible( enemyorigin, enemynum, origin, srcnum, 0 );
	const int badflags = ~travelflags;
	int best = 0;
	unsigned short bestTravel = 0;

	while ( auto *current = work.Pop() ) {
		int first, links;
		if ( !Links( current->areanum, first, links ) ) break;
		for ( int index = 0; index < links; ++index ) {
			if ( ++loopCount > maxLoops ) return best;
			const auto &reach = aasworld.reachability[first + index];
			if ( !Travelable( reach, badflags ) ) continue;
			const int next = reach.areanum;
			if ( next == enemyareanum ) continue;

			vec3_t reachStart, reachEnd, projected, toEnemy;
			VectorCopy( reach.start, reachStart );
			VectorCopy( reach.end, reachEnd );
			unsigned short travel = static_cast<unsigned short>( current->tmptraveltime
				+ AAS_AreaTravelTime( current->areanum, current->start, reachStart )
				+ reach.traveltime );

			AAS_ProjectPointOntoVector( enemyorigin, current->start, reachEnd, projected );
			int axis = 0;
			for ( ; axis < 3; ++axis ) {
				if ( ( projected[axis] > current->start[axis] && projected[axis] > reachEnd[axis] )
					|| ( projected[axis] < current->start[axis] && projected[axis] < reachEnd[axis] ) ) {
					break;
				}
			}
			if ( axis < 3 ) VectorSubtract( enemyorigin, reachEnd, toEnemy );
			else VectorSubtract( enemyorigin, projected, toEnemy );
			const float endDistance = VectorLength( toEnemy );
			if ( endDistance < 40 ) continue;

			vec3_t fromStart;
			VectorSubtract( enemyorigin, current->start, fromStart );
			const float startDistance = VectorLength( fromStart );
			if ( endDistance < startDistance ) {
				travel = static_cast<unsigned short>( travel + ( startDistance - endDistance ) * 10 );
			}

			// A distance limit is measured from the caller's reference point.
			if ( maxdist > 0 && distpos ) {
				const float *waypoint = AAS_WolfETWaypoint( next );
				if ( waypoint && Distance( distpos, waypoint ) > maxdist ) continue;
			}

			if ( !startVisible && AAS_WolfETAreaVisible( enemyareanum, next ) ) continue;
			if ( bestTravel && travel >= bestTravel ) continue;
			if ( aasworld.hidetraveltimes[next] && aasworld.hidetraveltimes[next] <= travel ) continue;

			if ( !AAS_WolfETAreaVisible( enemyareanum, next ) ) {
				bestTravel = travel;
				best = next;
			}
			aasworld.hidetraveltimes[next] = travel;
			auto *update = &aasworld.areaupdate[next];
			update->areanum = next;
			update->tmptraveltime = travel;
			VectorCopy( reach.end, update->start );
			work.Push( update );
		}
	}
	return best;
}



/*
===========================================================================
AAS_WolfETRetreat

Measures how far every area is from the nearest danger spot, then floods out
from the source and takes the first reachable area that is far enough from both
the source and the danger. The original walks the danger distances with its own
priority queue; a standard heap gives the same distances.
===========================================================================
*/
int AAS_WolfETRetreat( int *dangerSpots, int dangerSpotCount, vec3_t srcpos, int srcarea,
	vec3_t dangerpos, int dangerarea, float range, float dangerRange, int travelflags ) {
	using namespace fntech3::wolfet::routing;
	(void)dangerpos;
	(void)dangerarea;

	if ( !Ready() || !ResetVisited() ) return 0;
	if ( dangerSpotCount < 0 || ( dangerSpotCount > 0 && !dangerSpots ) ) return 0;

	constexpr unsigned short definitelySafe = 0xFFFF;
	if ( !aasworld.wolfetDangerDistance ) {
		aasworld.wolfetDangerDistance =
			botlib::ClearedAlloc<unsigned short>( aasworld.numareas );
	}
	if ( !aasworld.wolfetDangerDistance ) return 0;
	std::fill_n( aasworld.wolfetDangerDistance, aasworld.numareas, definitelySafe );

	// Shortest walking distance from any danger spot, bounded by dangerRange.
	using Entry = std::pair<unsigned, int>;
	std::priority_queue<Entry, std::vector<Entry>, std::greater<Entry>> queue;
	for ( int index = 0; index < dangerSpotCount; ++index ) {
		const int area = dangerSpots[index];
		if ( area < 1 || area >= aasworld.numareas ) continue;
		aasworld.wolfetDangerDistance[area] = 0;
		queue.emplace( 0u, area );
	}
	while ( !queue.empty() ) {
		const auto entry = queue.top();
		queue.pop();
		const int area = entry.second;
		if ( entry.first != aasworld.wolfetDangerDistance[area] ) continue;
		const float *from = AAS_WolfETWaypoint( area );
		if ( !from ) continue;
		int first, links;
		if ( !Links( area, first, links ) ) continue;
		for ( int index = 0; index < links; ++index ) {
			const auto &reach = aasworld.reachability[first + index];
			const int next = reach.areanum;
			if ( next < 1 || next >= aasworld.numareas ) continue;
			const float *to = AAS_WolfETWaypoint( next );
			if ( !to ) continue;
			const unsigned distance = entry.first + static_cast<unsigned>( Distance( from, to ) );
			if ( static_cast<float>( distance ) > dangerRange ) continue;
			if ( distance >= aasworld.wolfetDangerDistance[next] ) continue;
			aasworld.wolfetDangerDistance[next] = static_cast<unsigned short>( distance );
			queue.emplace( distance, next );
		}
	}

	Work work;
	if ( !work.Begin( srcarea, srcpos ) ) return 0;
	aasworld.hidetraveltimes[srcarea] = 1;

	constexpr int maxTime = 5000;
	const int badflags = ~travelflags;
	int best = 0;
	float bestDistance = 0;

	while ( auto *current = work.Pop() ) {
		int first, links;
		if ( !Links( current->areanum, first, links ) ) break;
		for ( int index = 0; index < links; ++index ) {
			const auto &reach = aasworld.reachability[first + index];
			if ( !Travelable( reach, badflags ) ) continue;
			if ( Blocked( reach.areanum ) ) continue;
			const int next = reach.areanum;
			if ( aasworld.hidetraveltimes[next] ) continue;
			aasworld.hidetraveltimes[next] = 1;
			vec3_t reachStart;
			VectorCopy( reach.start, reachStart );
			const int travel = current->tmptraveltime
				+ AAS_AreaTravelTime( current->areanum, current->start, reachStart )
				+ reach.traveltime;
			if ( travel > maxTime ) continue;
			const unsigned short danger = aasworld.wolfetDangerDistance[next];
			const float *waypoint = AAS_WolfETWaypoint( next );
			if ( waypoint && Distance( srcpos, waypoint ) > range
				&& ( static_cast<float>( danger ) > dangerRange || danger == definitelySafe ) ) {
				return next;
			}
			if ( static_cast<float>( danger ) > bestDistance ) {
				best = next;
				bestDistance = static_cast<float>( danger );
			}
			auto *update = &aasworld.areaupdate[next];
			update->areanum = next;
			VectorCopy( reach.end, update->start );
			work.Push( update );
		}
	}
	return best;
}


#endif
