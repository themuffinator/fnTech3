/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
/* Included by vk.c after the model extractors. Entries are model-space poses,
 * not world-space entities. Every append applies THIS frame's transform, and
 * RT_ExtractDynamic subsequently stamps current colour, lighting and motion.
 * Exact pose keys deliberately never quantize backlerp or throttle BODY. */
#define RT_GEOMETRY_CACHE_SLOTS 16
#define RT_GEOMETRY_CACHE_ENTRY_BYTES (1024u * 1024u)

typedef struct {
	const model_t *model;
	int handle, frame, oldFrame, skin, skinNum, shader, wrap, blink, skinning;
	float backlerp;
} rtGeometryKey_t;
typedef struct {
	rtGeometryKey_t key;
	rtxRtCpuGeometry_t geometry;
	uint64_t used;
} rtGeometryCacheEntry_t;
static rtGeometryCacheEntry_t s_rtGeometryCache[RT_GEOMETRY_CACHE_SLOTS];
static uint64_t s_rtGeometryClock, s_rtGeometryHits, s_rtGeometryMisses;

static void RT_GeometryCacheClear( void )
{
	int i;
	for ( i = 0; i < RT_GEOMETRY_CACHE_SLOTS; ++i ) {
		vk_rt_cpu_geometry_free( &s_rtGeometryCache[i].geometry );
	}
	Com_Memset( s_rtGeometryCache, 0, sizeof( s_rtGeometryCache ) );
	s_rtGeometryClock = s_rtGeometryHits = s_rtGeometryMisses = 0;
}

static qboolean RT_GeometryKey( const trRefEntity_t *ent, rtGeometryKey_t *key )
{
	const model_t *model;
	if ( !ent || ent->e.reType != RT_MODEL || ent->e.hModel <= 0 ||
		ri.gameId == COM_GAME_QUAKE3_IHV ) return qfalse;
	model = R_GetModelByHandle( ent->e.hModel );
	/* MDS/ET have torso, bone and procedural pose inputs beyond frame/backlerp.
	 * They keep their shared skeletal pose routines until a complete key exists. */
	if ( !model || ( model->type != MOD_BRUSH && model->type != MOD_MESH &&
		model->type != MOD_MDR && model->type != MOD_IQM ) ) return qfalse;
	Com_Memset( key, 0, sizeof( *key ) );
	key->model = model;
	key->handle = ent->e.hModel;
	key->frame = ent->e.frame;
	key->oldFrame = ent->e.oldframe;
	key->backlerp = ent->e.backlerp;
	key->skin = ent->e.customSkin;
	key->skinNum = ent->e.skinNum;
	key->shader = ent->e.customShader;
	key->wrap = ent->e.renderfx & RF_WRAP_FRAMES;
	key->blink = ent->e.rtcw.blink;
	key->skinning = rtx_rt_iqm_skinning ? rtx_rt_iqm_skinning->integer : 1;
	if ( model->type == MOD_BRUSH ) {
		key->frame = key->oldFrame = 0;
		key->backlerp = 0.0f;
	}
	return qtrue;
}

static qboolean RT_AppendCachedGeometry( rtxRtCpuGeometry_t *dst, const rtxRtCpuGeometry_t *src,
	const trRefEntity_t *ent )
{
	uint32_t v, i, first = dst->numVertices, material = dst->numMaterials;
	if ( !vk_rt_cpu_geometry_reserve( dst, src->numVertices, src->numIndices ) ||
		!vk_rt_cpu_geometry_reserve_materials( dst, src->numMaterials ) ) return qfalse;
	for ( i = 0; i < src->numMaterials; ++i ) {
		dst->materials[material + i] = src->materials[i];
		dst->materialShaders[material + i] = src->materialShaders[i];
	}
	for ( v = 0; v < src->numVertices; ++v ) {
		dst->vertices[first + v] = src->vertices[v];
		dst->vertices[first + v].materialIndex += material;
	}
	for ( i = 0; i < src->numIndices; ++i ) dst->indices[dst->numIndices + i] = first + src->indices[i];
	dst->numMaterials += src->numMaterials;
	dst->numVertices += src->numVertices;
	dst->numIndices += src->numIndices;
	dst->sourceTriangleCount += src->sourceTriangleCount;
	dst->maskedTriangleCount += src->maskedTriangleCount;
	dst->anyHitTriangleCount += src->anyHitTriangleCount;
	dst->sourceEntityCount += src->sourceEntityCount;
	vk_rt_transform_geometry_range( dst, first, ent );
	return qtrue;
}

static qboolean vk_rt_append_dynamic_model_entity_geometry( rtxRtCpuGeometry_t *geometry,
	const trRefEntity_t *ent, uint32_t *outModelCount )
{
	rtGeometryKey_t key;
	rtGeometryCacheEntry_t *entry = NULL;
	uint64_t oldest = UINT64_MAX;
	size_t bytes;
	int i;
	qboolean result;
	if ( !geometry || !RT_GeometryKey( ent, &key ) ) {
		return vk_rt_extract_dynamic_model_entity_geometry( geometry, ent, outModelCount );
	}
	for ( i = 0; i < RT_GEOMETRY_CACHE_SLOTS; ++i ) {
		rtGeometryCacheEntry_t *e = &s_rtGeometryCache[i];
		if ( e->used && !memcmp( &e->key, &key, sizeof( key ) ) ) {
			e->used = ++s_rtGeometryClock;
			++s_rtGeometryHits;
			if ( outModelCount ) ++*outModelCount;
			return RT_AppendCachedGeometry( geometry, &e->geometry, ent );
		}
		if ( e->used < oldest ) { oldest = e->used; entry = e; }
	}
	++s_rtGeometryMisses;
	vk_rt_cpu_geometry_free( &entry->geometry );
	entry->used = 0;
	{
		trRefEntity_t local = *ent;
		VectorClear( local.e.origin );
		AxisClear( local.e.axis );
		if ( !vk_rt_extract_dynamic_model_entity_geometry( &entry->geometry, &local, NULL ) ) {
			vk_rt_cpu_geometry_free( &entry->geometry );
			return qfalse;
		}
	}
	result = RT_AppendCachedGeometry( geometry, &entry->geometry, ent );
	if ( result && outModelCount ) ++*outModelCount;
	bytes = (size_t)entry->geometry.vertexCapacity * sizeof( rtxRtPackedVertex_t ) +
		(size_t)entry->geometry.indexCapacity * sizeof( uint32_t ) +
		(size_t)entry->geometry.materialCapacity * ( sizeof( rtxRtMaterial_t ) + sizeof( shader_t * ) );
	if ( bytes > RT_GEOMETRY_CACHE_ENTRY_BYTES || !result ) {
		vk_rt_cpu_geometry_free( &entry->geometry );
	} else {
		entry->key = key;
		entry->used = ++s_rtGeometryClock;
	}
	return result;
}
