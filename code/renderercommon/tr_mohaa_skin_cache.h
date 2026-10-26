/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_TR_MOHAA_SKIN_CACHE_H
#define FNTECH3_TR_MOHAA_SKIN_CACHE_H

/* Medal of Honor's skinned model surfaces, kept for the rest of the frame.
 *
 * The back end skins a TIKI surface on the CPU each time it draws it: in the
 * view, again in each dynamic light's lit pass, and once more for every face
 * of every dynamic light whose shadow it casts. On m1l1 the trucks in the
 * convoy's headlights were skinned twenty times a frame, half of the frame's
 * time. The pose is this frame's (R_MOHAAPose), and so is every surface
 * skinned by it: the first draw keeps a copy, and the frame's later draws of
 * the same surface at the same scale and detail copy it back instead of
 * skinning it again. A pose generation names the pose pool's contents; when
 * the pool starts over, every copy is dropped with it. A surface the cache
 * has no room for is skinned each time, as before. */

#define MOHAA_SKIN_CACHE_ENTRIES	1024	/* a power of two */
#define MOHAA_SKIN_CACHE_PROBES		16
#define MOHAA_SKIN_CACHE_VERTS		65536
#define MOHAA_SKIN_CACHE_INDEXES	( MOHAA_SKIN_CACHE_VERTS * 3 )

typedef struct {
	unsigned int	generation;		/* the pose generation it was skinned in */
	const float		*bones;
	const void		*surface;
	float			scale;
	float			lod;
	int				lodFlags;
	int				firstVert;
	int				numVerts;
	int				firstIndex;
	int				numIndexes;
} mohaaSkinCacheEntry_t;

typedef struct {
	unsigned int			generation;
	int						usedVerts;
	int						usedIndexes;
	mohaaSkinCacheEntry_t	entries[MOHAA_SKIN_CACHE_ENTRIES];
	vec4_t					xyz[MOHAA_SKIN_CACHE_VERTS];
	vec4_t					normal[MOHAA_SKIN_CACHE_VERTS];
	vec2_t					st[MOHAA_SKIN_CACHE_VERTS];
	unsigned int			indexes[MOHAA_SKIN_CACHE_INDEXES];	/* from the surface's first vertex */
} mohaaSkinCache_t;

static ID_INLINE qboolean R_MOHAASkinCacheKeyMatches( const mohaaSkinCacheEntry_t *entry, unsigned int generation,
	const float *bones, const void *surface, float scale, float lod, int lodFlags ) {
	return entry->generation == generation && entry->bones == bones && entry->surface == surface &&
		entry->scale == scale && entry->lod == lod && entry->lodFlags == lodFlags ? qtrue : qfalse;
}

/* The surface's entry this generation, or the free one it would take (the
 * caller tells them apart by its key), or NULL when its probes are full. */
static ID_INLINE mohaaSkinCacheEntry_t *R_MOHAASkinCacheEntry( mohaaSkinCache_t *cache, unsigned int generation,
	const float *bones, const void *surface, float scale, float lod, int lodFlags ) {
	unsigned int hash;
	int probe;

	if ( cache->generation != generation ) {
		cache->generation = generation;
		cache->usedVerts = 0;
		cache->usedIndexes = 0;
	}
	hash = (unsigned int)( (size_t)bones >> 4 ) * 2654435761u ^ (unsigned int)( (size_t)surface >> 3 ) * 40503u;
	for ( probe = 0; probe < MOHAA_SKIN_CACHE_PROBES; probe++ ) {
		mohaaSkinCacheEntry_t *entry = &cache->entries[( hash + (unsigned int)probe ) & ( MOHAA_SKIN_CACHE_ENTRIES - 1 )];
		if ( entry->generation != generation ||
			R_MOHAASkinCacheKeyMatches( entry, generation, bones, surface, scale, lod, lodFlags ) ) {
			return entry;
		}
	}
	return NULL;
}

/* Copy a kept surface into the caller's arrays, its indexes offset by
 * `baseVertex`. Returns the vertices copied, 0 when the surface is not kept
 * or does not fit. */
static ID_INLINE int R_MOHAASkinCacheCopy( mohaaSkinCache_t *cache, unsigned int generation, const float *bones,
	const void *surface, float scale, float lod, int lodFlags, vec4_t *xyz, vec4_t *normal, vec2_t *st,
	unsigned int *indexes, unsigned int baseVertex, int maxVerts, int maxIndexes, int *numIndexes ) {
	const mohaaSkinCacheEntry_t *entry = R_MOHAASkinCacheEntry( cache, generation, bones, surface, scale, lod, lodFlags );
	int i;

	*numIndexes = 0;
	if ( !entry || !R_MOHAASkinCacheKeyMatches( entry, generation, bones, surface, scale, lod, lodFlags ) ||
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

/* Keep a surface just skinned into the caller's arrays at `baseVertex`, if
 * there is room. */
static ID_INLINE void R_MOHAASkinCacheKeep( mohaaSkinCache_t *cache, unsigned int generation, const float *bones,
	const void *surface, float scale, float lod, int lodFlags, vec4_t *xyz, vec4_t *normal,
	vec2_t *st, const unsigned int *indexes, unsigned int baseVertex, int verts, int numIndexes ) {
	mohaaSkinCacheEntry_t *entry;
	int i;

	if ( verts <= 0 || numIndexes <= 0 ) {
		return;
	}
	/* (a new generation empties the store before its room is measured) */
	entry = R_MOHAASkinCacheEntry( cache, generation, bones, surface, scale, lod, lodFlags );
	if ( !entry || entry->generation == generation || verts > MOHAA_SKIN_CACHE_VERTS - cache->usedVerts ||
		numIndexes > MOHAA_SKIN_CACHE_INDEXES - cache->usedIndexes ) {
		return;
	}
	entry->generation = generation;
	entry->bones = bones;
	entry->surface = surface;
	entry->scale = scale;
	entry->lod = lod;
	entry->lodFlags = lodFlags;
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
