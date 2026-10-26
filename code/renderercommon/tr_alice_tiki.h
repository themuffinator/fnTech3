/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_TR_ALICE_TIKI_H
#define FNTECH3_TR_ALICE_TIKI_H

/* The renderer's view of American McGee's Alice's models.
 *
 * A definition is loaded by the engine and shared (see refimport_t's
 * AliceTiki* calls); this is the part of the renderer that reads one and
 * turns an entity's animation state into triangles. It is C++ inside, because
 * the definition and the pose code are, and C outside, because the renderer
 * that calls it is. Every `model` here is the engine's const AliceTikiModel.
 *
 * The rules are retail alice.exe's, shared with the engine's tag queries
 * through code/compat/alice_tiki_pose.hpp: a skeletal surface's vertex is the
 * weighted sum of its bones' matrices applied to bone-local offsets, times the
 * entity's scale and the model's (0x0049ffe0); a vertex-animated one is its
 * quantized position against the frame's scale and offset, lerped between two
 * frames that may come from different animations (0x0049f950). */

#include "tr_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The model's surfaces, in the order its file lists them. */
int RA_TikiNumSurfaces( const void *model );
int RA_TikiSurfaceNumSkins( const void *model, int surface );
const char *RA_TikiSurfaceShader( const void *model, int surface, int skin );
int RA_TikiSurfaceFlags( const void *model, int surface );
/* How many vertices and indexes a surface needs; 0 for a surface with none. */
int RA_TikiSurfaceSize( const void *model, int surface, int *numVerts, int *numIndexes );

/* The model's bounds and radius with its own scale, before the entity's. */
void RA_TikiModelBounds( const void *model, float mins[3], float maxs[3] );
float RA_TikiModelRadius( const void *model );
qboolean RA_TikiIsSkeletal( const void *model );

/* Retail's checks of the animation state an entity names, which its renderer
 * makes before anything reads that state (R_AddTikiSurfaces 0x004a6320,
 * R_AddSkelSurfaces 0x004a3a10): an animation the model does not have
 * becomes the first, with frame 0, and a frame its animation does not have
 * becomes 0 (0x004a6421). The entity is corrected in place, as retail
 * corrects its own copy, and each correction is reported to `print` at
 * PRINT_DEVELOPER in retail's words; `name` is the model's name. */
void RA_TikiCheckAnimation( const void *model, aliceRenderEntity_t *entity, const char *name,
	void ( QDECL *print )( printParm_t level, const char *fmt, ... ) );

/* Pose an entity's skeleton for this frame. The result lives until the next
 * RA_TikiBeginFrame and is what RA_TikiTessellate reads; null when the model
 * has no skeleton or the entity names no animation it can pose. */
const void *RA_TikiPose( const void *model, const aliceRenderEntity_t *entity );

/* Fill one surface's vertices and indexes. `firstVertex` offsets the indexes;
 * nothing is written and 0 is returned if the surface does not fit. This is
 * the whole surface; RA_TikiTessellateLod draws it at a level of detail. */
int RA_TikiTessellate( const void *model, int surface, const aliceRenderEntity_t *entity,
	const void *bones, float ( *xyz )[4], float ( *normal )[4], float ( *st )[2],
	unsigned int *indexes, unsigned int firstVertex, int maxVerts, int maxIndexes,
	int *outVerts, int *outIndexes );

/* Level of detail (code/compat/alice_tiki_lod.hpp). A surface keeps the
 * first numVerts * lodFraction of its vertices, at least its minLod, and its
 * other vertices fold onto them through its collapse map; triangles that
 * fold flat are dropped. 1 draws the whole surface. */
int RA_TikiTessellateLod( const void *model, int surface, const aliceRenderEntity_t *entity,
	const void *bones, float lodFraction, float ( *xyz )[4], float ( *normal )[4], float ( *st )[2],
	unsigned int *indexes, unsigned int firstVertex, int maxVerts, int maxIndexes,
	int *outVerts, int *outIndexes );
/* The sphere retail projects to choose a level of detail: its current
 * frame's radius and the model's origin, both scaled by the model and the
 * entity; the centre is an offset to add to the entity's origin unturned.
 * qfalse when the entity names no frame. */
qboolean RA_TikiLodSphere( const void *model, const aliceRenderEntity_t *entity, float center[3], float *radius );
/* The fraction for a projected radius, from r_lodscale and r_lodbias and the
 * definition's own lod_scale and lod_bias. */
float RA_TikiLodFraction( const void *model, float projectedRadius, float lodScaleCvar, float lodBiasCvar );
/* The point retail samples an entity's light at, in the entity's own frame
 * before its axis turns it: the middle of its current frame's bounds, plus
 * the model's origin and `lightoffset`, scaled. qfalse when there is none. */
qboolean RA_TikiLightingPoint( const void *model, const aliceRenderEntity_t *entity, float point[3] );

void RA_TikiBeginFrame( void );
/* Counts RA_TikiBeginFrame, never 0: a pose, and anything tessellated from
 * one, belongs to the generation it was made in (tr_alice_tiki_cache.h). */
unsigned int RA_TikiPoseGeneration( void );

#ifdef __cplusplus
}
#endif

#endif
