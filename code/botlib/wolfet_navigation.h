/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_WOLFET_NAVIGATION_H
#define FNTECH3_WOLFET_NAVIGATION_H
#include "wolfet_prediction_record.h"

/* Engine-internal ET navigation entry points. They do not extend Quake's
 * botlib export POD and do not expose its structures to ET modules.
 * Contract observations: docs/fntech3/2026-09-17-wolfet-navigation.md. */

/* ET resets routing with a negative blocking value on an empty box, keeps the
 * caller's remaining bits when it blocks, and can restrict a block to mover
 * areas, retrying without that restriction when nothing matched. */
void AAS_WolfETSetBlockingEntity( const float *mins, const float *maxs, int blocking );
/* Writes the area's stored centre. False leaves the output untouched. */
bool AAS_WolfETAreaCenter( int area, float *center );
/* Nonzero when the area carries ladder faces. */
int AAS_WolfETAreaLadder( int area );
/* ET publishes a waypoint only when its route table built one. This engine
 * has no waypoint table, which the original reports the same way: no point. */
bool AAS_WolfETAreaWaypoint( int area, float *center );
/* Derived from the loaded world at routing initialization, and released with
 * it. Visibility answers false for any area without a waypoint. */
void AAS_WolfETBuildVisibility( void );
void AAS_WolfETFreeVisibility( void );
int AAS_WolfETAreaVisible( int srcarea, int destarea );
const float *AAS_WolfETWaypoint( int area );

/* Reachability floods over those waypoints. Each returns zero when the world
 * has no visibility, as the original does. */
int AAS_WolfETListAreasInRange( vec3_t srcpos, int srcarea, float range, int travelflags,
	vec3_t *outareas, int maxareas );
int AAS_WolfETAvoidDangerArea( vec3_t srcpos, int srcarea, vec3_t dangerpos, int dangerarea,
	float range, int travelflags );
/* ET derives both area numbers itself and publishes goals at area waypoints. */
int AAS_WolfETAlternativeRouteGoals( vec3_t start, vec3_t goal, int travelflags,
	aas_altroutegoal_t *altroutegoals, int maxaltroutegoals );

/* ET asks the game module whether one position sees another during its hide
 * search. The callback stays out of the frozen botlib import table; an absent
 * one means no hiding spot can be proved, which the search reports as none. */
struct aas_wolfet_callbacks_t {
	bool (*available)() = nullptr;
	int (*visible)(const float *, int, const float *, int, int) = nullptr;
};
void AAS_SetWolfETCallbacks( const aas_wolfet_callbacks_t &callbacks );
const aas_wolfet_callbacks_t &AAS_WolfETCallbacks();

/* Nearest area the source can stand in without the enemy seeing it. Zero means
 * none was found, or that the work budget for this frame was already spent. */
int AAS_WolfETNearestHideArea( int srcnum, vec3_t origin, int areanum, int enemynum,
	vec3_t enemyorigin, int enemyareanum, int travelflags, float maxdist, vec3_t distpos );
/* First area far enough from both the source and every danger spot. */
int AAS_WolfETRetreat( int *dangerSpots, int dangerSpotCount, vec3_t srcpos, int srcarea,
	vec3_t dangerpos, int dangerarea, float range, float dangerRange, int travelflags );

#endif
