/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_RTCW_WORLD_SHADER_H
#define FNTECH3_RTCW_WORLD_SHADER_H
#include "../qcommon/q_shared.h"
#include "../qcommon/game_profile.h"
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

/* Independently implemented from original RTCW renderer contract observations.
 * Retain metadata on the material so cached shaders still configure a new map.
 */
typedef struct {
	char sunShader[MAX_QPATH];
	float ambient, directed;
} fnRTCWWorldShader_t;

static inline int FnRTCWWorldShaderDirective( comGameId_t profile, const char *token,
	const char **text, fnRTCWWorldShader_t *state ) {
	const char *value, *before;
	int kind;
	/* Enemy Territory keeps RTCW's world directives unchanged. */
	if ( profile != COM_GAME_RTCW_SP && profile != COM_GAME_RTCW_MP && profile != COM_GAME_WOLFET ) return 0;
	if ( !Q_stricmp(token, "sunshader") ) kind = 0;
	else if ( !Q_stricmp(token, "lightgridmulamb") ) kind = 1;
	else if ( !Q_stricmp(token, "lightgridmuldir") ) kind = 2;
	else return 0;
	before = *text;
	value = COM_ParseExt(text, qfalse);
	if ( !*value || !strcmp(value, "{") || !strcmp(value, "}") ) {
		*text = before; return -1;
	}
	if ( !kind ) {
		if ( strlen(value) >= sizeof(state->sunShader) ) return -1;
		Q_strncpyz(state->sunShader, value, sizeof(state->sunShader));
	} else {
		char *end;
		const float scale = (float)strtod(value, &end);
		uint32_t bits;
		memcpy(&bits, &scale, sizeof(bits));
		if ( end == value || *end || (bits & UINT32_C(0x7f800000)) == UINT32_C(0x7f800000) ) return -1;
		/* Original nonpositive values leave the existing multiplier alone. */
		if ( scale > 0 ) {
			if ( kind == 1 ) state->ambient = scale;
			else state->directed = scale;
		}
	}
	return 1;
}

static inline void FnRTCWWorldShaderMerge( fnRTCWWorldShader_t *world,
	const fnRTCWWorldShader_t *material ) {
	if ( material->sunShader[0] ) memcpy(world->sunShader, material->sunShader, sizeof(world->sunShader));
	if ( material->ambient > 0 ) world->ambient = material->ambient;
	if ( material->directed > 0 ) world->directed = material->directed;
}

static inline void FnRTCWWorldGridScale( const fnRTCWWorldShader_t *world,
	float *ambient, float *directed ) {
	int i;
	for ( i = 0; i < 3; ++i ) {
		if ( world->ambient > 0 ) ambient[i] *= world->ambient;
		if ( world->directed > 0 ) directed[i] *= world->directed;
	}
}
#endif
