/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_RTCW_NAVIGATION_H
#define FNTECH3_RTCW_NAVIGATION_H

// Engine-internal entry points. These do not extend Quake's botlib export POD
// or expose its structures to RTCW modules. Valid before library setup too.
bool AAS_SetCurrentWorld( int index );
void AAS_SetBlockingEntity( const float *mins, const float *maxs, int blocking );

#endif
