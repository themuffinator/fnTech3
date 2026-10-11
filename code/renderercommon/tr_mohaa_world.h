/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_TR_MOHAA_WORLD_H
#define FNTECH3_TR_MOHAA_WORLD_H

/* Medal of Honor's terrain, static models and permanent decals, built into its
 * translated world as ordinary surfaces; see tr_mohaa_world.c. */

#include "../qcommon/q_shared.h"
#include "../qcommon/bsp_mohaa.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The built surfaces the renderers treat apart: one a terrain patch, in the
 * map's order, which decals are cut against (tr_mohaa_marks_surface.h), and
 * one a permanent decal fragment, which r_drawstaticdecals hides. */
typedef struct {
	int		firstTerrain;
	int		numTerrain;
	int		firstDecal;
	int		numDecals;
	/* All appended world surfaces, including static models when there is
	 * no terrain. They lie after the original world AND inline models;
	 * extending model zero's contiguous range would include moving doors. */
	int		firstBuiltSurface;
	int		numBuiltSurfaces;
} mohaaBuiltSurfaces_t;

/* From a "2015" v19 map already translated to IBSP 46 (MOHAA_TranslateToV46),
 * a new IBSP 46 buffer whose surfaces also hold every terrain patch and
 * every static model, listed in the leafs the map places them in, and the
 * fragments of the map's decal file beside `mapName` (maps/<name>.dcl), with
 * the full appended range and the terrain and decal subranges. The caller frees the
 * result with ri.Free. False, with nothing allocated and no ranges, when the
 * map has none of them or the build fails. */
qboolean R_MOHAABuildWorld( const mohaa_translation_t *translation, const char *mapName, void **data, int *length,
	mohaaBuiltSurfaces_t *built );

#ifdef __cplusplus
}
#endif

#endif
