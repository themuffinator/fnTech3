/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_TR_Q3TEST_SHADER_TEXT_H
#define FNTECH3_TR_Q3TEST_SHADER_TEXT_H

/* Q3Test 1.06-1.08's gfx.shader ends gfx/2d/crosshairc's stage with '{'
 * instead of '}', so the shader never closes. Retail found a shader by the
 * first token of any line (R_FindShader, 1.08 0x4379b0, skips a line at a
 * time, not a braced section), so the defect cost it only crosshairc itself:
 * its parse met the stray '{' as an unknown stage parameter and returned the
 * default shader (1.08 0x439d3e). This engine checks braces before it indexes
 * a file and would drop the whole of gfx.shader. The five bytes are rewritten
 * in the read buffer to the same outcome: an unknown token ends crosshairc's
 * parse, and its stage and body close. Only these exact files are touched:
 * FNV-1a identifies each whole file and the local guard the rewritten bytes. */
static qboolean R_Q3TestRepairShaderText( char *text, long length, const char *filename ) {
	static const struct { long length; long offset; unsigned int hash; } defects[] = {
		{ 20263, 17004, 0x1ea32884u },	/* 1.06 and 1.07 pak0.pk3 */
		{ 20265, 16969, 0x7a08f618u },	/* 1.08 pak0.pk3 */
	};
	static const char stray[] = "\t{\r\n}\r\n";
	unsigned int hash;
	long i;
	size_t d;
	if ( ri.gameId != COM_GAME_Q3TEST || !text || strcmp( filename, "scripts/gfx.shader" ) ) return qfalse;
	for ( d = 0; d < sizeof( defects ) / sizeof( defects[0] ); ++d ) {
		if ( length != defects[d].length ||
			memcmp( text + defects[d].offset, stray, sizeof( stray ) - 1 ) ) continue;
		hash = 2166136261u;
		for ( i = 0; i < length; ++i ) hash = ( hash ^ (unsigned char)text[i] ) * 16777619u;
		if ( hash != defects[d].hash ) continue;
		memcpy( text + defects[d].offset, "?\n}\n}", 5 );
		ri.Printf( PRINT_DEVELOPER, "Repaired Q3Test gfx.shader crosshairc closing brace in memory.\n" );
		return qtrue;
	}
	return qfalse;
}

#endif
