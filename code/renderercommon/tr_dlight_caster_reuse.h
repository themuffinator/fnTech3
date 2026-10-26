/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_TR_DLIGHT_CASTER_REUSE_H
#define FNTECH3_TR_DLIGHT_CASTER_REUSE_H

/* A dynamic light's shadow casters, built once for all six of its faces.
 *
 * Each face of a point light's shadow cube walked the light's caster chain,
 * tessellated every caster again and pushed each vertex off its surface along
 * its normal on the CPU, though all six faces get the same vertices: the push
 * reads only the light's origin and the entity's own orientation, and a face,
 * a triangle soup, an MD3 or a TIKI model tessellates without regard to the
 * face it is drawn into. Medal of Honor's m1l1 rebuilt the canyon and the
 * convoy's trucks in their headlights twelve times a frame that way, most of
 * the frame on Vulkan. Now the first face to draw such a caster keeps its
 * pushed vertices and indexes, and the light's other faces copy them. A grid,
 * whose detail is measured along the face's axis (LodErrorForVolume), and
 * RTCW's and Enemy Territory's skeletal meshes, whose detail is projected on
 * it, are still built per face, as is any caster the store has no room for.
 * r_dlightShadowCasterReuse 0 builds every caster per face, for comparison;
 * the faces are the same either way. */

#define DLIGHT_CASTER_REUSE_SURFACES	8192	/* the light's caster chain, by position */
#define DLIGHT_CASTER_REUSE_VERTS		65536
#define DLIGHT_CASTER_REUSE_INDEXES		( DLIGHT_CASTER_REUSE_VERTS * 3 )

/* Set while a caster batch holds vertices the caster pass has already pushed,
 * so the stage iterator leaves them as they are (tr_shade.c). */
extern qboolean rb_dlightCasterOffsetApplied;
/* Counts RB_BeginSurface: a caster that overflows tess starts a new batch
 * before it writes, which this tells the caster pass. */
extern unsigned int rb_surfaceBegins;

typedef struct {
	unsigned int	generation;		/* the light it was kept for */
	int				firstVert;
	int				numVerts;
	int				firstIndex;
	int				numIndexes;
} dlightCasterKept_t;

typedef struct {
	unsigned int		generation;
	int					usedVerts;
	int					usedIndexes;
	dlightCasterKept_t	kept[DLIGHT_CASTER_REUSE_SURFACES];
	vec4_t				xyz[DLIGHT_CASTER_REUSE_VERTS];
	unsigned int		indexes[DLIGHT_CASTER_REUSE_INDEXES];	/* from the caster's first vertex */
} dlightCasterReuse_t;

/* Push vertices first..end-1 off their surfaces for a shadow caster: along
 * each unit normal, toward the light where it faces the light and away where
 * it faces away, by up to normalBias, most where the light grazes it. The
 * light is a point at lightOrigin, or, for the sun's cascades, a direction
 * (towardVertex, from the light to every vertex). */
static ID_INLINE void R_DlightShadowCasterOffset( vec4_t *xyz, vec4_t *normal, int first, int end,
	const orientationr_t *orient, const float *lightOrigin, const float *towardVertex, float normalBias ) {
	int i;

	for ( i = first; i < end; i++ ) {
		vec3_t world;
		vec3_t normalWorld;
		vec3_t lightToVertex;
		float normalLength;
		float lightSide;
		float normalScale;
		float scale;

		normalWorld[0] = normal[i][0] * orient->axis[0][0] +
			normal[i][1] * orient->axis[1][0] +
			normal[i][2] * orient->axis[2][0];
		normalWorld[1] = normal[i][0] * orient->axis[0][1] +
			normal[i][1] * orient->axis[1][1] +
			normal[i][2] * orient->axis[2][1];
		normalWorld[2] = normal[i][0] * orient->axis[0][2] +
			normal[i][1] * orient->axis[1][2] +
			normal[i][2] * orient->axis[2][2];

		normalLength = VectorNormalize( normalWorld );
		if ( normalLength <= 0.0f ) {
			continue;
		}

		if ( towardVertex ) {
			VectorCopy( towardVertex, lightToVertex );
		} else {
			world[0] = orient->origin[0] +
				xyz[i][0] * orient->axis[0][0] +
				xyz[i][1] * orient->axis[1][0] +
				xyz[i][2] * orient->axis[2][0];
			world[1] = orient->origin[1] +
				xyz[i][0] * orient->axis[0][1] +
				xyz[i][1] * orient->axis[1][1] +
				xyz[i][2] * orient->axis[2][1];
			world[2] = orient->origin[2] +
				xyz[i][0] * orient->axis[0][2] +
				xyz[i][1] * orient->axis[1][2] +
				xyz[i][2] * orient->axis[2][2];

			VectorSubtract( world, lightOrigin, lightToVertex );
			if ( VectorNormalize( lightToVertex ) <= 0.0f ) {
				continue;
			}
		}
		lightSide = DotProduct( normalWorld, lightToVertex );
		normalScale = 0.25f + 0.50f * ( 1.0f - Com_Clamp( 0.0f, 1.0f, fabsf( lightSide ) ) );
		scale = ( normalBias * normalScale ) / normalLength;
		if ( lightSide < 0.0f ) {
			scale = -scale;
		}
		VectorMA( xyz[i], scale, normal[i], xyz[i] );
	}
}

/* The casters no face's axis changes: their tessellation, and so their pushed
 * vertices, are the same in every face. Each checks its room in tess once,
 * before it writes. */
static ID_INLINE qboolean R_DlightCasterReusable( int surfaceType ) {
	switch ( surfaceType ) {
	case SF_FACE:
	case SF_TRIANGLES:
	case SF_MD3:
	case SF_ALICE_TIKI:
	case SF_MOHAA_TIKI:
		return qtrue;
	default:
		return qfalse;
	}
}

/* A new light: nothing kept. */
static ID_INLINE void R_DlightCasterReuseBeginLight( dlightCasterReuse_t *reuse ) {
	if ( ++reuse->generation == 0 ) {
		reuse->generation = 1;
	}
	reuse->usedVerts = 0;
	reuse->usedIndexes = 0;
}

/* The caster at a position in the light's chain, if kept for this light. */
static ID_INLINE const dlightCasterKept_t *R_DlightCasterReuseKept( const dlightCasterReuse_t *reuse, int chainIndex ) {
	const dlightCasterKept_t *kept;

	if ( chainIndex < 0 || chainIndex >= DLIGHT_CASTER_REUSE_SURFACES ) {
		return NULL;
	}
	kept = &reuse->kept[chainIndex];
	return kept->generation == reuse->generation ? kept : NULL;
}

/* Keep a caster's pushed vertices and indexes, written to tess from
 * firstVert and firstIndex, if there is room. */
static ID_INLINE void R_DlightCasterReuseKeep( dlightCasterReuse_t *reuse, int chainIndex, vec4_t *xyz,
	const unsigned int *indexes, int firstVert, int numVerts, int firstIndex, int numIndexes ) {
	dlightCasterKept_t *kept;
	int i;

	if ( chainIndex < 0 || chainIndex >= DLIGHT_CASTER_REUSE_SURFACES || numVerts <= 0 || numIndexes <= 0 ||
		numVerts > DLIGHT_CASTER_REUSE_VERTS - reuse->usedVerts ||
		numIndexes > DLIGHT_CASTER_REUSE_INDEXES - reuse->usedIndexes ) {
		return;
	}
	for ( i = 0; i < numIndexes; i++ ) {
		/* a caster that names vertices outside its own is not kept */
		if ( indexes[firstIndex + i] < (unsigned int)firstVert ||
			indexes[firstIndex + i] >= (unsigned int)( firstVert + numVerts ) ) {
			return;
		}
	}
	kept = &reuse->kept[chainIndex];
	kept->generation = reuse->generation;
	kept->firstVert = reuse->usedVerts;
	kept->numVerts = numVerts;
	kept->firstIndex = reuse->usedIndexes;
	kept->numIndexes = numIndexes;
	Com_Memcpy( reuse->xyz + kept->firstVert, xyz + firstVert, numVerts * sizeof( vec4_t ) );
	for ( i = 0; i < numIndexes; i++ ) {
		reuse->indexes[kept->firstIndex + i] = indexes[firstIndex + i] - (unsigned int)firstVert;
	}
	reuse->usedVerts += numVerts;
	reuse->usedIndexes += numIndexes;
}

/* Copy a kept caster into tess after its numVertexes and numIndexes, which
 * must have room for it. Only the positions: a caster is drawn for depth
 * alone. */
static ID_INLINE void R_DlightCasterReuseCopy( const dlightCasterReuse_t *reuse, const dlightCasterKept_t *kept,
	vec4_t *xyz, unsigned int *indexes, int *numVertexes, int *numIndexes ) {
	const unsigned int base = (unsigned int)*numVertexes;
	int i;

	Com_Memcpy( xyz + *numVertexes, reuse->xyz + kept->firstVert, kept->numVerts * sizeof( vec4_t ) );
	for ( i = 0; i < kept->numIndexes; i++ ) {
		indexes[*numIndexes + i] = base + reuse->indexes[kept->firstIndex + i];
	}
	*numVertexes += kept->numVerts;
	*numIndexes += kept->numIndexes;
}

#endif
