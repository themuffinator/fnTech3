/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_TR_Q3_IHV_SHADER_TEXT_H
#define FNTECH3_TR_Q3_IHV_SHADER_TEXT_H

/* The supplied IHV common.shader omits light5's outer closing brace before
 * light6. Repair only that exact file, in the read buffer, without growing it
 * or relaxing validation of arbitrary shader text. FNV-1a identifies all 8672
 * bytes; the local guard also bounds and identifies the five rewritten bytes.
 * Its SHA-256 and original installation remain in the evidence ledger. */
static qboolean R_Q3IHVRepairShaderText( char *text, long length, const char *filename ) {
	unsigned int hash = 2166136261u;
	long i;
	if ( ( ri.gameId != COM_GAME_QUAKE3 && ri.gameId != COM_GAME_QUAKE3_IHV ) || length != 8672 ||
		strcmp( filename, "scripts/common.shader" ) ||
		memcmp( text + 6169, "\r\n}\r\ntextures/common/light6\r\n",
			sizeof( "\r\n}\r\ntextures/common/light6\r\n" ) - 1 ) ) return qfalse;
	for ( i = 0; i < length; ++i ) hash = ( hash ^ (unsigned char)text[i] ) * 16777619u;
	if ( hash != 0x87e8f157u ) return qfalse;
	memcpy( text + 6169, "\n}\n}\n", 5 );
	ri.Printf( PRINT_DEVELOPER, "Repaired IHV common.shader light5 closing brace in memory.\n" );
	return qtrue;
}

#endif
