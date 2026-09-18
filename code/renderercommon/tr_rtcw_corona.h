/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_RTCW_CORONA_H
#define FNTECH3_RTCW_CORONA_H
#include <stdint.h>
#include <string.h>
#include "../qcommon/game_profile.h"

enum { FN_RTCW_MAX_CORONAS = 32, FN_RTCW_MAX_CORONA_FADES = 128 };
/* Owned scene data, never a game-module pointer. */
typedef struct {
	float origin[3], color[3], scale;
	int id, flags;
} fnRTCWCorona_t;

static inline int FnRTCWCoronaFinite( float value ) {
	uint32_t bits;
	memcpy( &bits, &value, sizeof( bits ) );
	return (bits & UINT32_C(0x7f800000)) != UINT32_C(0x7f800000);
}

static inline int FnRTCWCoronaValid( const fnRTCWCorona_t *corona ) {
	int i;
	if ( !corona || !FnRTCWCoronaFinite( corona->scale ) ) return 0;
	for ( i = 0; i < 3; ++i )
		if ( !FnRTCWCoronaFinite( corona->origin[i] ) || !FnRTCWCoronaFinite( corona->color[i] ) ) return 0;
	return 1;
}

/* Enemy Territory's coronas are the other Wolfenstein profile's multiplayer
 * coronas: its cgame decides visibility, and the renderer fades and draws
 * them the same way, with the same flare shader. */
static inline int FnRTCWCoronaProfile( comGameId_t profile ) {
	return profile == COM_GAME_RTCW_SP || profile == COM_GAME_RTCW_MP || profile == COM_GAME_WOLFET;
}

/* The original flags word is a bitmask in SP, a boolean in MP. */
static inline int FnRTCWCoronaVisible( int multiplayer, int flags ) {
	return multiplayer ? flags != 0 : (flags & 1) != 0;
}

static inline float FnRTCWCoronaFade( int64_t elapsed, int visible, float speed ) {
	const float progress = (elapsed / 1000.0f) * speed;
	const float fade = visible ? progress : 1.0f - progress;
	if ( !FnRTCWCoronaFinite( fade ) ) return 0;
	return fade <= 0 ? 0 : fade >= 1 ? 1 : fade;
}

static inline unsigned char FnRTCWCoronaByte( float value ) {
	if ( !FnRTCWCoronaFinite( value ) || value <= 0 ) return 0;
	return value >= 1 ? 255 : (unsigned char)(value * 255.0f);
}

#endif
