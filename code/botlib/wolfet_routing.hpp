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
 * visited marker (the hide search keeps travel times in it), and the persistent
 * `areaupdate` array as the work list, addressed by area number, so ET sizes it
 * by the world's areas (AAS_InitRoutingUpdate). Observations:
 * docs/fntech3/2026-09-17-wolfet-visibility.md, for the queue marks
 * docs/fntech3/2026-10-15-census-small-gaps.md, and for the hide search
 * docs/fntech3/2026-10-15-wolfet-hide-search.md. */

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

	/* The range and hide searches leave a record that is marked as queued
	 * where it is (etded.exe 2.60b 0x44a663, 0x44ad4d). */
	void Push( aas_routingupdate_t *update ) {
		if ( update->inlist ) return;
		Append( update );
	}

	/* The danger and retreat searches queue without looking at the mark
	 * (0x448166..0x448182, 0x44ba85..0x44ba9e). A search that returned early
	 * leaves its queue marked, and these two lose no area to that. */
	void Append( aas_routingupdate_t *update ) {
		update->next = nullptr;
		update->prev = end;
		if ( end ) end->next = update;
		else start = update;
		end = update;
		update->inlist = qtrue;
	}

	void Drain() {
		while ( Pop() ) {}
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

// The hide search keeps the game's answers in a byte per area, cleared at the
// top of every search (etded.exe 0x44a793..0x44a7c6).
inline byte *ResetHideVisibility() {
	if ( !aasworld.wolfetHideVisibility ) {
		aasworld.wolfetHideVisibility = botlib::ClearedAlloc<byte>( aasworld.numareas );
	} else {
		std::fill_n( aasworld.wolfetHideVisibility, aasworld.numareas, static_cast<byte>( 0 ) );
	}
	return aasworld.wolfetHideVisibility;
}

/* AAS_IsEntityInArea as the hide search asks it (0x43c550, called at 0x44ac30
 * with the searcher and -1 ignored, a number no link carries): a valid, solid
 * entity linked into the area. The links are the world's own, the entity
 * records the entity world's. */
inline bool Occupied( int area, int ignore ) {
	if ( !aasworld.arealinkedentities ) return false;
	const aas_t &world = AAS_EntityWorld();
	int remaining = aasworld.linkheapsize;
	for ( const aas_link_t *link = aasworld.arealinkedentities[area]; link; link = link->next_ent ) {
		// More links than the heap holds is a broken list, not an empty area.
		if ( --remaining < 0 ) return true;
		const int number = link->entnum;
		if ( number == ignore || number < 0 || number >= world.maxentities || !world.entities ) continue;
		const aas_entityinfo_t &info = world.entities[number].i;
		if ( info.valid && info.solid ) return true;
	}
	return false;
}

// The hide search truncates a time it has added to with cvttss2si (0x44ac05),
// which gives 0x80000000 for a value int cannot hold, NaN included: those low
// 16 bits are zero.
inline unsigned short TruncatedTime( float value ) {
	if ( !( value >= -2147483648.0f && value < 2147483648.0f ) ) return 0;
	return static_cast<unsigned short>( static_cast<int>( value ) );
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
	/* Retail goes on flooding once the list is full (etded.exe 0x44a63a),
	 * writing past the caller's array, and so leaves nothing queued. Stopping
	 * here instead, clear the marks a later search would otherwise see. */
	work.Drain();
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
			work.Append( update );
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

The cheapest area the source can reach that the enemy cannot see, searched
for as retail etded.exe 2.60b's AAS_NearestHideArea (0x44a6d0) does. The
flood never enters a ladder or disabled area or the enemy's own, nor takes a
step ending beyond the distance limit. With the enemy more than 32 units from
the source, a step that passes within 256 units of it, nearer to it than the
source, is refused, as is one ending more than halfway from the source towards
it, and a step that comes nearer costs ten time units for each unit gained.
An occupied area costs 1,000 more, and a source the enemy cannot see at the
start never steps into the enemy area's view. An area is hidden when the
enemy's area cannot see it and the game says the enemy cannot see its
waypoint; the game is asked once an area in a search. Every candidate takes a
step, and so does every one that reaches the queue: past 3,000 in a frame the
search queues nothing more but empties its queue, and a later search in that
frame answers none unless a negative source number restarts the count. An
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
		srcnum = ~srcnum; // -srcnum - 1, by retail's not (0x44a6f2)
		lastTime = 0;
	}
	if ( lastTime == AAS_Time() && loopCount >= maxLoops ) return 0;
	if ( lastTime != AAS_Time() ) loopCount = 0;
	lastTime = AAS_Time();

	if ( !ResetVisited() ) return 0;
	byte *const seen = ResetHideVisibility();
	if ( !seen ) return 0;

	// Retail asks for the enemy's travel time and drops the answer (0x44a7df);
	// the query still builds routing caches and counts the frame's updates.
	if ( enemyareanum ) AAS_AreaTravelTimeToGoalArea( areanum, origin, enemyareanum, travelflags );
	vec3_t enemyVec;
	VectorSubtract( enemyorigin, origin, enemyVec );
	// Retail's VectorNormalize returns the length it divided by.
	const float enemyDistance = VectorLength( enemyVec );
	VectorNormalize( enemyVec );
	// Asked before the start area is refused (0x44a832, 0x44a86a).
	const int startVisible = callbacks.visible( enemyorigin, enemynum, origin, srcnum, 0 );
	const int badflags = ~travelflags;

	Work work;
	if ( !work.Begin( areanum, origin ) ) return 0;

	int best = 0;
	unsigned short bestTravel = 0;
	while ( auto *current = work.Pop() ) {
		int first, links;
		if ( !Links( current->areanum, first, links ) ) break;
		for ( int index = 0; index < links; ++index ) {
			const auto &reach = aasworld.reachability[first + index];
			if ( !Travelable( reach, badflags ) ) continue;
			const int next = reach.areanum;
			if ( Blocked( next ) || next == enemyareanum ) continue;
			vec3_t reachStart, reachEnd;
			VectorCopy( reach.start, reachStart );
			VectorCopy( reach.end, reachEnd );
			// The limit is measured to where the step ends (0x44a94e).
			if ( distpos && Distance( reachEnd, distpos ) > maxdist ) continue;
			unsigned short travel = static_cast<unsigned short>( current->tmptraveltime
				+ AAS_AreaTravelTime( current->areanum, current->start, reachStart )
				+ reach.traveltime );
			++loopCount;
			// The time is kept before the costs below; an equal one goes on.
			if ( aasworld.hidetraveltimes[next] && aasworld.hidetraveltimes[next] < travel ) continue;
			aasworld.hidetraveltimes[next] = travel;
			// The best area, reached again as cheaply, must earn its place again.
			if ( best == next ) {
				best = 0;
				bestTravel = 0;
			}
			if ( bestTravel && travel >= bestTravel ) continue;

			// The step's point nearest the enemy: the enemy's projection onto
			// it, or the step's end once that lies more than 0.1 past the step.
			vec3_t projected, toEnemy;
			AAS_ProjectPointOntoVector( enemyorigin, current->start, reachEnd, projected );
			int axis = 0;
			for ( ; axis < 3; ++axis ) {
				if ( ( projected[axis] > current->start[axis] + 0.1 && projected[axis] > reachEnd[axis] + 0.1 )
					|| ( projected[axis] < current->start[axis] - 0.1 && projected[axis] < reachEnd[axis] - 0.1 ) ) {
					break;
				}
			}
			const float *nearest = axis < 3 ? reachEnd : projected;
			VectorSubtract( enemyorigin, nearest, toEnemy );
			const float passDistance = VectorLength( toEnemy );
			const bool distant = enemyDistance > 32;
			// Never past the enemy, and never more than halfway towards it.
			if ( distant && passDistance < enemyDistance && passDistance < 256 ) continue;
			vec3_t step;
			VectorSubtract( reachEnd, origin, step );
			if ( distant && DotProduct( step, enemyVec ) > enemyDistance * 0.5f ) continue;
			vec3_t fromStart;
			VectorSubtract( enemyorigin, current->start, fromStart );
			const float startDistance = VectorLength( fromStart );
			if ( distant && passDistance < startDistance ) {
				travel = TruncatedTime( static_cast<float>( travel ) + ( startDistance - passDistance ) * 10 );
				if ( bestTravel && travel >= bestTravel ) continue;
			}
			if ( Occupied( next, srcnum ) ) travel = static_cast<unsigned short>( travel + 1000 );
			if ( enemyareanum && !startVisible && AAS_WolfETAreaVisible( enemyareanum, next ) ) continue;
			if ( bestTravel && bestTravel <= travel ) continue;

			// Only an area with a waypoint sees itself; only such an area
			// carries visibility to test.
			if ( AAS_WolfETAreaVisible( next, next ) && !AAS_WolfETAreaVisible( enemyareanum, next ) ) {
				if ( seen[next] == 2 || ( !seen[next] && !callbacks.visible( enemyorigin, enemynum,
					aasworld.wolfetWaypoints[next], srcnum, 0 ) ) ) {
					seen[next] = 2;
					bestTravel = travel;
					best = next;
				} else {
					seen[next] = 1;
				}
			}
			// Past the limit nothing more is queued, but the queue still empties.
			if ( loopCount++ > maxLoops ) continue;
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
			work.Append( update );
		}
	}
	return best;
}


#endif
