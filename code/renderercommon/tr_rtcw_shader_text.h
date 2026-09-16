/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_RTCW_SHADER_TEXT_H
#define FNTECH3_RTCW_SHADER_TEXT_H

/* RTCW indexes usable definitions even when a file has stray top-level tokens
 * (including the retail SP UI's trailing braces). Keep complete definitions,
 * without letting an unfinished body consume definitions from another file.
 * The filesystem supplies writable, NUL-terminated text. This only shrinks it.
 */
static int R_RTCWNormalizeShaderText( char *text, const char *filename ) {
	const char *cursor = text;
	char *output = text;
	int discarded = 0;
	COM_BeginParseSession( filename );
	while ( cursor ) {
		const char *start = cursor;
		const char *token = COM_ParseExt( &cursor, qtrue );
		const char *afterName;
		if ( !token[0] ) break;
		if ( token[1] == '\0' && ( token[0] == '{' || token[0] == '}' ) ) {
			++discarded;
			if ( token[0] == '{' && !SkipBracedSection( &cursor, 1 ) ) break;
			continue;
		}
		afterName = cursor;
		token = COM_ParseExt( &cursor, qtrue );
		if ( token[0] != '{' || token[1] != '\0' ) {
			++discarded;
			cursor = afterName; // The next token may name a valid definition.
			continue;
		}
		if ( !SkipBracedSection( &cursor, 1 ) ) {
			++discarded;
			break;
		}
		memmove( output, start, cursor - start );
		output += cursor - start;
	}
	*output = '\0';
	if ( discarded ) ri.Printf( PRINT_DEVELOPER,
		"RTCW shader file %s: retained complete definitions; discarded %d malformed sections\n",
		filename, discarded );
	return (int)( output - text ); // Preserve RTCW's uncompressed shader text.
}

#endif
