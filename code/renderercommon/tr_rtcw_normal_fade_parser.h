/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_RTCW_NORMAL_FADE_PARSER_H
#define FNTECH3_RTCW_NORMAL_FADE_PARSER_H

#include <float.h>
#include <math.h>

/* Optional arguments are alpha alone, or alpha/lower/upper. A missing set
 * keeps the original defaults. Reject malformed numbers atomically and leave
 * braces to the enclosing parser rather than treating them as zeroes.
 */
static qboolean R_RTCWParseNormalZFade( const char **text, byte *alpha, float bounds[2] ) {
	const char *cursor = *text;
	float values[3] = { 1, -1, 1 };
	int count;
	for ( count = 0; count < 3; ++count ) {
		const char *before = cursor;
		const char *token = COM_ParseExt( &cursor, qfalse );
		char *end;
		double value;
		if ( !token[0] ) break;
		if ( !strcmp( token, "{" ) || !strcmp( token, "}" ) ) {
			cursor = before;
			break;
		}
		value = strtod( token, &end );
		if ( end == token || *end || !isfinite( value ) || value < -FLT_MAX || value > FLT_MAX ) return qfalse;
		values[count] = (float)value;
	}
	if ( count == 2 || values[0] < 0 || values[0] > 1 ) return qfalse;
	*alpha = (byte)( values[0] * 255.0f );
	bounds[0] = values[1];
	bounds[1] = values[2];
	*text = cursor;
	return qtrue;
}
#endif
