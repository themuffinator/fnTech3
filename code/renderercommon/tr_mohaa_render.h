/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_TR_MOHAA_RENDER_H
#define FNTECH3_TR_MOHAA_RENDER_H

/* Medal of Honor: Allied Assault's model definitions, as the renderers reach
 * them.
 *
 * A TIKI definition is the engine's: it loads it once for the game, the
 * client game and the renderer, and poses its skeleton with the skeletor
 * that 2015's game and client game both drive. So the renderer does not read
 * the definition's records itself; it asks the engine, through this table,
 * for each surface's shaders and size, for an entity's pose, and for a
 * surface skinned by that pose into its own vertex arrays. Every pointer the
 * table hands out stays valid while the definition stays registered.
 *
 * The pose is the skeletor's own bone frame: for each of the definition's
 * channels, three rows of the bone's rotation and a fourth of its origin
 * (twelve floats), followed by one weight per morph target, which moves the
 * face's vertices for lip sync. See code/mohaa/mohaa_render_api.cpp. */

#include "../qcommon/q_shared.h"
#include "tr_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MOHAA_RENDER_MAX_SKINS			4
#define MOHAA_RENDER_MAX_FRAMEINFOS		16
#define MOHAA_RENDER_BONE_CONTROLLERS	5
#define MOHAA_RENDER_MAX_SURFACES		32

/* A definition surface's flags the renderer reads (OpenMoHAA tiki_shared.h). */
#define MOHAA_TIKI_SURF_SKIN1		0x01
#define MOHAA_TIKI_SURF_NODRAW		0x04
#define MOHAA_TIKI_SURF_CROSSFADE	0x40
#define MOHAA_TIKI_SURF_NOMIPMAPS	0x100
#define MOHAA_TIKI_SURF_NOPICMIP	0x200

/* Skin's level-of-detail flags: a view weapon, which 2015 draws at
 * r_lodviewmodelcap's detail whatever its size on screen. */
#define MOHAA_LOD_VIEWMODEL			0x01

typedef struct {
	const char	*name;
	int			numSkins;
	const char	*shaders[MOHAA_RENDER_MAX_SKINS];
	int			flags;
	int			numVerts;
	int			numIndexes;
} mohaaTikiSurface_t;

/* An entity's pose is computed from a mohaaPoseInput_t (tr_types.h), one
 * mohaaFrameInfo_t for each animation blended into it. */

typedef struct mohaaRenderImport_s {
	/* The definition's surfaces, over all of its meshes, in the order 2015's
	 * renderer walks them and an entity's surface bytes are numbered in. */
	int			(*NumSurfaces)( const void *tiki );
	qboolean	(*Surface)( const void *tiki, int surface, mohaaTikiSurface_t *out );
	/* How many floats a pose of this definition takes. */
	int			(*PoseFloats)( const void *tiki );
	/* Pose the entity's skeleton and write its bone frame into `pose`,
	 * which holds PoseFloats() floats. Returns false for no pose. */
	qboolean	(*Pose)( const mohaaPoseInput_t *input, float *pose );
	/* Skin one surface with a pose, scaled, into the caller's arrays: four
	 * floats a position and a normal, two a texture coordinate, and indexes
	 * offset by `baseVertex`. `lod` is the entity's level-of-detail metric
	 * (LodMetric), which drops the vertices and triangles 2015 drops at it,
	 * or negative to skin the whole surface; `lodFlags` holds
	 * MOHAA_LOD_VIEWMODEL for a view weapon. Returns the vertices written,
	 * 0 when the surface does not fit, has none, or is too small on screen
	 * to draw; `numIndexes` gets the indexes. The arrays must hold the
	 * whole surface. */
	int			(*Skin)( const void *tiki, int surface, const float *pose, float scale, float lod, int lodFlags,
					float (*xyz)[4], float (*normal)[4], float (*st)[2], unsigned int *indexes,
					unsigned int baseVertex, int maxVerts, int maxIndexes, int *numIndexes );
	/* The definition's bounds at a scale, its radius, and its load scale,
	 * which every position it skins is multiplied by already. */
	void		(*Bounds)( const void *tiki, float scale, vec3_t mins, vec3_t maxs );
	float		(*Radius)( const void *tiki );
	float		(*LoadScale)( const void *tiki );
	/* A static model's definition, registered by the path its map names
	 * (with or without the leading `models/`), or NULL; and its pose, the
	 * first frame of its first animation, into PoseFloats() floats. */
	const void	*(*RegisterStatic)( const char *path );
	qboolean	(*StaticPose)( const void *tiki, float *pose );
	/* An entity's level-of-detail metric seen from `viewOrigin` with a
	 * horizontal field of `fovX` degrees: its model's radius over its
	 * distance, times 100 over the field. `axis` is the entity's three
	 * rows, and its pose must be this frame's (Pose). Negative for a
	 * definition with no level-of-detail record. */
	float		(*LodMetric)( const mohaaPoseInput_t *input, float scale, const vec3_t origin, const float *axis,
					const vec3_t viewOrigin, float fovX );
} mohaaRenderImport_t;

#ifdef __cplusplus
}
#endif

#endif
