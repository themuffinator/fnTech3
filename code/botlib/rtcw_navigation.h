/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_RTCW_NAVIGATION_H
#define FNTECH3_RTCW_NAVIGATION_H

// Engine-internal entry points. These do not extend Quake's botlib export POD
// or expose its structures to RTCW modules. Valid before library setup too.
bool AAS_SetCurrentWorld( int index );
void AAS_SetBlockingEntity( const float *mins, const float *maxs, int blocking );
// SP route query; leaves position unchanged when no visible point is found.
bool AAS_RTCWRouteFirstVisiblePosition( const float *source, const float *destination,
	int travelFlags, float *position );
// Tactical queries publish a waypoint only when a candidate was found.
bool AAS_RTCWHidePosition(const float *source, int sourceEntity, int sourceArea,
	const float *enemy, int enemyEntity, int enemyArea, float *position);
int AAS_RTCWAttackPosition(int sourceEntity, int rangeEntity, int enemyEntity,
	float range, int travelFlags, float *position);
// RTCW movement uses separate flags for the immediate step and its route.
// The original avoid-list and entity parameters do not influence this query.
bool AAS_RTCWMovementRoutingReady();
int AAS_RTCWMovementReachability(const float *origin, int area, int lastGoal,
	int previousArea, int goal, int routeFlags, int movementFlags);
// Original movement look-ahead, with bounded work and unchanged failure output.
bool AAS_RTCWMovementFuturePosition(int reach, const float *goalOrigin, int goalArea,
	int travelFlags, float *position);
bool AAS_RTCWAgainstLadder(const float *origin, int previousArea);
// A failed run-up prediction must not issue a subsequent jump action.
bool AAS_RTCWJumpReachRunStart(const float *from, const float *to, float *runstart);

// Engine callbacks are separate from the frozen Quake botlib import table.
// Availability is explicit: an absent game callback cannot prove a hiding spot.
struct aas_rtcw_cast_callbacks_t {
	bool (*available)() = nullptr;
	int (*visible)(const float *, int, const float *, int, int) = nullptr;
	int (*attack)(int, int, const float *, int, int) = nullptr;
};
void AAS_SetRTCWCastCallbacks(const aas_rtcw_cast_callbacks_t &callbacks);
const aas_rtcw_cast_callbacks_t &AAS_RTCWCastCallbacks();

#endif
