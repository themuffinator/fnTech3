/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_TR_Q3_IHV_SKIN_H
#define FNTECH3_TR_Q3_IHV_SKIN_H

#include <stddef.h>
#include <string.h>

/* Independently observed R_RegisterSkin (0x439930): names after commas,
 * in order, without whitespace/quote/comment processing. A tag_ prefix at
 * the current cursor skips through the next LF. This is not CommaParse.
 * Native storage has 32 shader pointers and a 1024-byte token buffer. */
#define FNTECH3_IHV_SKIN_SHADERS 32
#define FNTECH3_IHV_SKIN_TOKEN 1024
typedef struct {
	int count;
	char names[FNTECH3_IHV_SKIN_SHADERS][FNTECH3_IHV_SKIN_TOKEN];
} fntech3IhvSkinNames_t;

static inline int FnQ3IHV_ParseSkin( const char *text, size_t length, fntech3IhvSkinNames_t *output ) {
	fntech3IhvSkinNames_t result;
	const char *cursor, *end, *zero;
	if ( !text || !output ) return 0;
	memset( &result, 0, sizeof( result ) );
	zero = (const char *)memchr( text, 0, length );
	end = zero ? zero : text + length;
	cursor = text;
	while ( cursor < end ) {
		const char *comma, *start;
		size_t bytes;
		if ( end - cursor >= 4 && !memcmp( cursor, "tag_", 4 ) ) {
			cursor = (const char *)memchr( cursor, '\n', (size_t)( end - cursor ) );
			if ( !cursor ) break;
			++cursor;
			continue;
		}
		comma = (const char *)memchr( cursor, ',', (size_t)( end - cursor ) );
		if ( !comma ) break;
		cursor = comma + 1;
		if ( cursor < end && ( *cursor == '\r' || *cursor == '\n' ) ) {
			do { ++cursor; } while ( cursor < end && ( *cursor == '\r' || *cursor == '\n' ) );
			continue;
		}
		start = cursor;
		while ( cursor < end && *cursor != '\r' && *cursor != '\n' ) ++cursor;
		bytes = (size_t)( cursor - start );
		if ( result.count == FNTECH3_IHV_SKIN_SHADERS || bytes >= FNTECH3_IHV_SKIN_TOKEN ) return 0;
		memcpy( result.names[result.count++], start, bytes );
	}
	*output = result;
	return 1;
}

#endif
