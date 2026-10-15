/*
Copyright (C) 2026 fnTech3 contributors
GPL version 2 or (at your option) any later version.

A map's shader-record surface flags in the Fn view's Quake III bits, for the
map formats whose compilers wrote other values. The renderers read those
records in two places: every ParseMesh skips a nodraw patch, and RTX takes
the compiler's sky and light filters (ShaderForShaderNum). Raven's maps are
translated by their own reader (tr_raven_bsp_format.h); Quake III, Quake
Live, RTCW, Enemy Territory and Elite Force write Quake III's own values
(each retail renderer's infoparm table), and the IHV Test's and Q3Test's
renderers have no such table.

- Alice and F.A.K.K.2 ("FAKK" 42 and 12): alice.exe's infoparm table
  (0x5294d0) keeps sky, nodraw and the other Quake III words in place but
  has alphashadow at 0x800 and no lightfilter; 0x10000 is its wood and
  0x8000 is unused. F.A.K.K.2's own executable is encrypted; Ritual's
  F.A.K.K.2 SDK surfaceflags.h agrees with alice.exe (reference
  observation).
- Medal of Honor (2015 19): its executable is encrypted. OpenMoHAA's
  surfaceflags.h (reference observation) has alphashadow at 0x200,
  nolightmap 0x100, nosteps 0x400, nonsolid 0x800, nodlight 0x20000000 and
  hint 0x40000000, and 0x8000 and 0x10000 are its metal and rock. The
  installed maps agree for alphashadow: 374 of the 580 records with 0x200
  are shaders whose script says alphashadow.

A meaning Quake III lacks (materials, castshadow, Medal of Honor's backside
and patch bits) is left out. The content words need no translation: the
one bit the renderers read from them, trans (0x20000000), is Quake III's in
every one of these formats.
See docs/fntech3/2026-10-15-map-shader-flag-dialects.md.
*/
#ifndef FN_TR_MAP_SHADER_FLAGS_H
#define FN_TR_MAP_SHADER_FLAGS_H

static inline int R_MapSurfaceFlagsToFn( comGameBspFormat_t format, int flags ) {
	/* bits in the same place as Quake III's in both tables */
	static const int shared = SURF_NODAMAGE | SURF_SLICK | SURF_SKY | SURF_LADDER | SURF_NOIMPACT
		| SURF_NOMARKS | SURF_NODRAW;
	static const int ritual[][2] = {
		{ 0x400, SURF_NOLIGHTMAP }, { 0x800, SURF_ALPHASHADOW }, { 0x2000, SURF_NOSTEPS },
		{ 0x4000, SURF_NONSOLID }, { 0x800000, SURF_NODLIGHT }, { 0x1000000, SURF_HINT },
	};
	static const int mohaa[][2] = {
		{ 0x100, SURF_NOLIGHTMAP }, { 0x200, SURF_ALPHASHADOW }, { 0x400, SURF_NOSTEPS },
		{ 0x800, SURF_NONSOLID }, { 0x20000000, SURF_NODLIGHT }, { 0x40000000, SURF_HINT },
	};
	const int (*table)[2];
	int count, i, fn;

	if ( format == COM_GAME_BSP_FAKK ) {
		table = ritual;
		count = (int)( sizeof( ritual ) / sizeof( ritual[0] ) );
	} else if ( format == COM_GAME_BSP_MOHAA ) {
		table = mohaa;
		count = (int)( sizeof( mohaa ) / sizeof( mohaa[0] ) );
	} else {
		return flags;
	}
	fn = flags & shared;
	for ( i = 0; i < count; i++ ) {
		if ( flags & table[i][0] ) {
			fn |= table[i][1];
		}
	}
	return fn;
}

static inline void R_MapShaderFlagsToFn( comGameBspFormat_t format, dshader_t *shaders, int count ) {
	int i;

	for ( i = 0; i < count; i++ ) {
		shaders[i].surfaceFlags = R_MapSurfaceFlagsToFn( format, shaders[i].surfaceFlags );
	}
}

#endif
