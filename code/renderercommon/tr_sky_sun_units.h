/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_TR_SKY_SUN_UNITS_H
#define FNTECH3_TR_SKY_SUN_UNITS_H

/* How a map's compiler stored a sky shader's q3map_sun (or Raven's `sun`)
 * in its lightmaps: colour x intensity x cos, with the colour scaled to unit
 * length by id's q3map and Raven's, and so its brightest channel is 1 by
 * q3map2 and Return to Castle Wolfenstein's own compiler. The renderers'
 * sky-sun parser keeps the brightest channel at 1.
 *
 * Measured from the shadow edges in the installed maps' lightmaps
 * (docs/fntech3/2026-10-22-fnq-shadow-formats.md, "The sun's units"):
 * against unit length Quake III read 1.04, Q3Test 1.07, Elite Force 0.95 and
 * Jedi Academy 0.98; against a brightest channel of 1 Quake Live (q3map2)
 * read 0.83 and Wolfenstein 0.88. Enemy Territory's maps were compiled by
 * q3map2 and keep their lightmaps in images; all of them carry its
 * worldspawn keys. A map of any game that q3map2 compiled is known by those
 * keys, which RTX's light parser (RT_ParseLightEntity) reads too.
 *
 * Shared by the raster renderers' sun shadows (csmPlan_t.sunLightmapColor)
 * and RTX's path tracer (RT_FindSun). Entity suns -- Alice's, F.A.K.K.2's,
 * Medal of Honor's -- are given in lightmap units already and never pass
 * through here. */

/* A worldspawn key only q3map2 writes or reads. Wolfenstein's own tools read
 * _farplanedist too (RTCW's mp_tram keeps its 828 light entities, which
 * q3map2 would have left out). */
static ID_INLINE qboolean R_SkySunQ3map2Key( const char *key, int gameId ) {
	static const char *const keys[] = {
		"_ambient", "_minlight", "_minvertexlight", "_mingridlight", "_noshadersun",
		"_lightmapscale", "_ls", "_blocksize", "_celshader", "_foghull",
		"_keepLights", "_floodlight", "_ignoreleaks"
	};
	int i;

	if ( !Q_stricmpn( key, "_q3map2_", 8 ) ) {
		return qtrue;	// _q3map2_version, _q3map2_cmdline
	}
	if ( !Q_stricmp( key, "_farplanedist" ) ) {
		return ( gameId != COM_GAME_RTCW_SP && gameId != COM_GAME_RTCW_MP &&
			gameId != COM_GAME_WOLFET ) ? qtrue : qfalse;
	}
	for ( i = 0; i < (int)ARRAY_LEN( keys ); i++ ) {
		if ( !Q_stricmp( key, keys[i] ) ) {
			return qtrue;
		}
	}
	return qfalse;
}

/* Whether the worldspawn -- the entity text's first entity -- carries a key
 * only q3map2 writes or reads. The text is quoted pairs, as compilers write
 * it; anything else ends the scan. */
static ID_INLINE qboolean R_SkySunEntitiesQ3map2( const char *entities, int gameId ) {
	char key[MAX_QPATH];
	const char *p = entities;
	int length;

	if ( !p ) {
		return qfalse;
	}
	while ( *p && *p != '{' ) {
		p++;
	}
	if ( *p != '{' ) {
		return qfalse;
	}
	p++;
	for ( ;; ) {
		while ( *p == ' ' || *p == '\t' || *p == '\r' || *p == '\n' ) {
			p++;
		}
		if ( *p != '"' ) {
			return qfalse;	// the closing brace, the end or something malformed
		}
		p++;
		for ( length = 0; *p && *p != '"'; p++ ) {
			if ( length < (int)sizeof( key ) - 1 ) {
				key[length++] = *p;
			}
		}
		key[length] = '\0';
		if ( *p != '"' ) {
			return qfalse;
		}
		p++;
		while ( *p == ' ' || *p == '\t' || *p == '\r' || *p == '\n' ) {
			p++;
		}
		if ( *p != '"' ) {
			return qfalse;
		}
		p++;
		while ( *p && *p != '"' ) {
			p++;	// the value
		}
		if ( *p != '"' ) {
			return qfalse;
		}
		p++;
		if ( R_SkySunQ3map2Key( key, gameId ) ) {
			return qtrue;
		}
	}
}

/* Whether a sky sun's colour was stored at unit length: by id's q3map and
 * Raven's, not by q3map2 nor by the compilers of Quake Live, Return to
 * Castle Wolfenstein and Enemy Territory. */
static ID_INLINE qboolean R_SkySunUnitLengthColour( int gameId, qboolean q3map2 ) {
	if ( q3map2 ) {
		return qfalse;
	}
	switch ( gameId ) {
		case COM_GAME_QUAKELIVE:
		case COM_GAME_RTCW_SP:
		case COM_GAME_RTCW_MP:
		case COM_GAME_WOLFET:
			return qfalse;
		default:
			return qtrue;
	}
}

#endif
