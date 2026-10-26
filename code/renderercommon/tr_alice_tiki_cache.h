/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_TR_ALICE_TIKI_CACHE_H
#define FNTECH3_TR_ALICE_TIKI_CACHE_H

/* Alice's and F.A.K.K.2's TIKI model surfaces, kept for the rest of the frame.
 *
 * The back end tessellates a TIKI surface on the CPU (RA_TikiTessellateLod)
 * each time it draws it: in the view, again in each dynamic light's lit pass,
 * for a crossfade's second skin and a custom shader's extra pass, and for the
 * shadows. What it writes depends only on the surface, its pose, the entity's
 * scale and the level of detail. A skeletal model's pose is its skeleton,
 * posed once a frame (RA_TikiPose) and good until the poses start over; a
 * vertex-animated model's is the two frames and the lerp the entity names. So
 * the frame's first draw of a surface keeps a copy, and its later draws of the
 * same surface with the same pose, scale and detail copy it back. A dynamic
 * light's shadow faces draw the whole surface and the views the detail each
 * chose, so one surface may be kept twice. The poses' generation
 * (RA_TikiPoseGeneration) drops every copy when they start over. A surface the
 * store has no room for is tessellated each time, as before. */

#define ALICE_TIKI_CACHE_ENTRIES	1024	/* a power of two */
#define ALICE_TIKI_CACHE_PROBES		16
#define ALICE_TIKI_CACHE_VERTS		65536
#define ALICE_TIKI_CACHE_INDEXES	( ALICE_TIKI_CACHE_VERTS * 3 )

/* Everything a surface's tessellation reads besides the model's own data.
 * The skeleton is named by where this frame's pose put it; the frames are
 * named by value, so a vertex-animated surface does not depend on it. */
typedef struct {
	const void	*surface;		/* the draw surface: the model and its surface */
	const void	*bones;			/* a skeletal model's pose, or NULL */
	int			anim, oldanim;
	int			frame, oldframe;
	float		backlerp;
	float		scale;			/* the entity's own */
	float		lod;			/* the share of its vertices kept */
} aliceTikiCacheKey_t;

typedef struct {
	unsigned int		generation;	/* the pose generation it was tessellated in */
	aliceTikiCacheKey_t	key;
	int					firstVert;
	int					numVerts;
	int					firstIndex;
	int					numIndexes;
} aliceTikiCacheEntry_t;

typedef struct {
	unsigned int			generation;
	int						usedVerts;
	int						usedIndexes;
	aliceTikiCacheEntry_t	entries[ALICE_TIKI_CACHE_ENTRIES];
	vec4_t					xyz[ALICE_TIKI_CACHE_VERTS];
	vec4_t					normal[ALICE_TIKI_CACHE_VERTS];
	vec2_t					st[ALICE_TIKI_CACHE_VERTS];
	unsigned int			indexes[ALICE_TIKI_CACHE_INDEXES];	/* from the surface's first vertex */
} aliceTikiCache_t;

static ID_INLINE qboolean R_AliceTikiCacheKeyMatches( const aliceTikiCacheEntry_t *entry, unsigned int generation,
	const aliceTikiCacheKey_t *key ) {
	const aliceTikiCacheKey_t *kept = &entry->key;

	return entry->generation == generation && kept->surface == key->surface && kept->bones == key->bones &&
		kept->anim == key->anim && kept->oldanim == key->oldanim && kept->frame == key->frame &&
		kept->oldframe == key->oldframe && kept->backlerp == key->backlerp && kept->scale == key->scale &&
		kept->lod == key->lod ? qtrue : qfalse;
}

/* The surface's entry this generation, or the free one it would take (the
 * caller tells them apart by its key), or NULL when its probes are full. */
static ID_INLINE aliceTikiCacheEntry_t *R_AliceTikiCacheEntry( aliceTikiCache_t *cache, unsigned int generation,
	const aliceTikiCacheKey_t *key ) {
	unsigned int hash;
	int probe;

	if ( cache->generation != generation ) {
		cache->generation = generation;
		cache->usedVerts = 0;
		cache->usedIndexes = 0;
	}
	hash = (unsigned int)( (size_t)key->surface >> 3 ) * 2654435761u ^ (unsigned int)( (size_t)key->bones >> 4 ) * 40503u ^
		(unsigned int)( key->frame * 31 + key->anim ) * 2246822519u;
	for ( probe = 0; probe < ALICE_TIKI_CACHE_PROBES; probe++ ) {
		aliceTikiCacheEntry_t *entry = &cache->entries[( hash + (unsigned int)probe ) & ( ALICE_TIKI_CACHE_ENTRIES - 1 )];
		if ( entry->generation != generation || R_AliceTikiCacheKeyMatches( entry, generation, key ) ) {
			return entry;
		}
	}
	return NULL;
}

/* Copy a kept surface into the caller's arrays, its indexes offset by
 * `baseVertex`. Returns the vertices copied, 0 when the surface is not kept
 * or does not fit. */
static ID_INLINE int R_AliceTikiCacheCopy( aliceTikiCache_t *cache, unsigned int generation,
	const aliceTikiCacheKey_t *key, vec4_t *xyz, vec4_t *normal, vec2_t *st, unsigned int *indexes,
	unsigned int baseVertex, int maxVerts, int maxIndexes, int *numIndexes ) {
	const aliceTikiCacheEntry_t *entry = R_AliceTikiCacheEntry( cache, generation, key );
	int i;

	*numIndexes = 0;
	if ( !entry || !R_AliceTikiCacheKeyMatches( entry, generation, key ) ||
		entry->numVerts > maxVerts || entry->numIndexes > maxIndexes ) {
		return 0;
	}
	Com_Memcpy( xyz, cache->xyz + entry->firstVert, entry->numVerts * sizeof( vec4_t ) );
	Com_Memcpy( normal, cache->normal + entry->firstVert, entry->numVerts * sizeof( vec4_t ) );
	Com_Memcpy( st, cache->st + entry->firstVert, entry->numVerts * sizeof( vec2_t ) );
	for ( i = 0; i < entry->numIndexes; i++ ) {
		indexes[i] = baseVertex + cache->indexes[entry->firstIndex + i];
	}
	*numIndexes = entry->numIndexes;
	return entry->numVerts;
}

/* Keep a surface just tessellated into the caller's arrays at `baseVertex`,
 * if there is room. (The arrays are not const: C does not convert a pointer
 * to an array to a pointer to a const one.) */
static ID_INLINE void R_AliceTikiCacheKeep( aliceTikiCache_t *cache, unsigned int generation,
	const aliceTikiCacheKey_t *key, vec4_t *xyz, vec4_t *normal, vec2_t *st,
	const unsigned int *indexes, unsigned int baseVertex, int verts, int numIndexes ) {
	aliceTikiCacheEntry_t *entry;
	int i;

	if ( verts <= 0 || numIndexes <= 0 ) {
		return;
	}
	/* (a new generation empties the store before its room is measured) */
	entry = R_AliceTikiCacheEntry( cache, generation, key );
	if ( !entry || entry->generation == generation || verts > ALICE_TIKI_CACHE_VERTS - cache->usedVerts ||
		numIndexes > ALICE_TIKI_CACHE_INDEXES - cache->usedIndexes ) {
		return;
	}
	entry->generation = generation;
	entry->key = *key;
	entry->firstVert = cache->usedVerts;
	entry->numVerts = verts;
	entry->firstIndex = cache->usedIndexes;
	entry->numIndexes = numIndexes;
	Com_Memcpy( cache->xyz + entry->firstVert, xyz, verts * sizeof( vec4_t ) );
	Com_Memcpy( cache->normal + entry->firstVert, normal, verts * sizeof( vec4_t ) );
	Com_Memcpy( cache->st + entry->firstVert, st, verts * sizeof( vec2_t ) );
	for ( i = 0; i < numIndexes; i++ ) {
		cache->indexes[entry->firstIndex + i] = indexes[i] - baseVertex;
	}
	cache->usedVerts += verts;
	cache->usedIndexes += numIndexes;
}

#endif
