/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_WOLFET_NAVIGATION_H
#define FNTECH3_WOLFET_NAVIGATION_H

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

#endif
