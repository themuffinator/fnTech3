"""Medal of Honor's TIKI models in the RTX path tracer.

code/renderercommon/tr_mohaa_ray_geometry.h hands the trace each surface of
a TIKI entity as RB_SurfaceMOHAATiki draws it. The RTX renderer's real
tr_mohaa.c (registration, front end, back end and the trace's helpers) is
compiled with the appender against a stand-in engine import whose pose and
skinning answer from every argument, so the traced vertices, normals,
texture coordinates, triangles and shaders can be checked against what the
raster passes drew of the same entity in the same frame: visible, at a
measured detail, as a view weapon, culled from the view (posed by the
trace alone), unposable, and with every surface state the front end reads.
"""
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]

STUB = r'''
#include <assert.h>
#include <ctype.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "code/qcommon/q_shared.h"
#include "code/renderercommon/tr_types.h"
#include "code/renderercommon/tr_mohaa_render.h"

typedef enum { SF_BAD, SF_MOHAA_TIKI } surfaceType_t;
typedef enum { MOD_BAD, MOD_MOHAA_TIKI } modtype_t;
typedef struct shader_s { int index, sort, masked; qboolean hasScreenMap, defaultShader, mipmaps, picmip; char name[64]; } shader_t;
@MOHAA_SURFACE@
typedef struct model_s {
    char name[MAX_QPATH]; modtype_t type; int index, dataSize;
    const void *mohaaTiki; struct mohaaSurface_s *mohaaSurfaces; int numMohaaSurfaces;
} model_t;
typedef struct { refEntity_t e; qboolean lightingCalculated; } trRefEntity_t;
typedef struct dlightShadowCasterContext_s dlightShadowCasterContext_t;
typedef struct { vec3_t bounds[2]; } fog_t;
typedef struct { int numfogs; fog_t *fogs; } world_t;
typedef struct { int passFlags, portal; float fovX; struct { vec3_t origin; } or; } viewParms_t;
typedef struct { int rdflags; float fovXAuthored; } trRefdef_t;
#define VPF_ALICE_SKY_PORTAL 0x40
#define SS_OPAQUE 3
#define CULL_OUT 2
#define DSF_SHADOW_CASTER_ONLY 1u
#define SHADER_MAX_VERTEXES 1000
#define SHADER_MAX_INDEXES 6000
#define RB_CHECKOVERFLOW( v, i ) assert( tess.numVertexes + ( v ) <= SHADER_MAX_VERTEXES && tess.numIndexes + ( i ) <= SHADER_MAX_INDEXES )

static struct {
    int frameCount, numModels; model_t *models[8]; model_t *currentModel; shader_t *defaultShader;
    qboolean needScreenMap; viewParms_t viewParms; trRefdef_t refdef; world_t *world;
} tr;
static struct { trRefEntity_t *currentEntity; } backEnd;
static struct {
    vec4_t xyz[SHADER_MAX_VERTEXES], normal[SHADER_MAX_VERTEXES]; vec2_t texCoords[2][SHADER_MAX_VERTEXES];
    unsigned int indexes[SHADER_MAX_INDEXES]; int numVertexes, numIndexes;
} tess;
static void Printf( int level, const char *fmt, ... ) { (void)level; (void)fmt; }
static byte hunk[1 << 16]; static int hunkUsed;
static void *Hunk_Alloc( int size, ha_pref preference ) {
    void *block = hunk + hunkUsed; (void)preference; hunkUsed += ( size + 15 ) & ~15; assert( hunkUsed <= (int)sizeof( hunk ) );
    memset( block, 0, size ); return block;
}
static struct { const mohaaRenderImport_t *MOHAARender; void (*Printf)( int, const char *, ... ); void *(*Hunk_Alloc)( int, ha_pref ); } ri;
static cvar_t shadowsCvar; static cvar_t *r_shadows = &shadowsCvar;

vec_t VectorNormalize( vec3_t v ) {
    const float length = sqrtf( DotProduct( v, v ) );
    if ( length ) { v[0] /= length; v[1] /= length; v[2] /= length; }
    return length;
}
int Q_stricmp( const char *a, const char *b ) {
    for ( ; *a && *b; a++, b++ ) { int x = tolower( (unsigned char)*a ), y = tolower( (unsigned char)*b ); if ( x != y ) return x - y; }
    return tolower( (unsigned char)*a ) - tolower( (unsigned char)*b );
}
void Q_strncpyz( char *dest, const char *src, int size ) { strncpy( dest, src, size - 1 ); dest[size - 1] = 0; }

static model_t models[8];
static model_t *R_AllocModel( void ) {
    model_t *m = &models[tr.numModels]; memset( m, 0, sizeof( *m ) ); m->index = tr.numModels; tr.models[tr.numModels++] = m; return m;
}
static model_t *R_GetModelByHandle( qhandle_t h ) { return h > 0 && h < tr.numModels ? tr.models[h] : NULL; }
static shader_t shaders[32]; static int numShaders = 1;
static shader_t *R_FindAliceSurfaceShader( const char *name, qboolean mipmaps, qboolean picmip ) {
    int i;
    for ( i = 1; i < numShaders; i++ ) if ( !strcmp( shaders[i].name, name ) ) return &shaders[i];
    assert( numShaders < 32 );
    shaders[numShaders].index = numShaders; shaders[numShaders].mipmaps = mipmaps; shaders[numShaders].picmip = picmip;
    shaders[numShaders].sort = SS_OPAQUE; strcpy( shaders[numShaders].name, name );
    return &shaders[numShaders++];
}
static shader_t *R_GetShaderByHandle( qhandle_t h ) { return h > 0 && h < numShaders ? &shaders[h] : tr.defaultShader; }
static qboolean R_ViewPassIsPortal( const viewParms_t *v ) { return v->portal ? qtrue : qfalse; }
static int cullAll;
static int R_CullLocalBox( vec3_t bounds[2] ) { (void)bounds; return cullAll ? CULL_OUT : 0; }
static void R_SetupEntityLighting( const trRefdef_t *refdef, trRefEntity_t *ent ) { (void)refdef; ent->lightingCalculated = qtrue; }
static struct { surfaceType_t *surface; shader_t *shader; unsigned int flags; } draws[64]; static int numDraws;
static void R_AddDrawSurfFlags( surfaceType_t *surface, shader_t *shader, int fog, int dlight, unsigned int flags ) {
    (void)fog; (void)dlight; assert( numDraws < 64 );
    draws[numDraws].surface = surface; draws[numDraws].shader = shader; draws[numDraws++].flags = flags;
}
static void R_AddDrawSurf( surfaceType_t *surface, shader_t *shader, int fog, int dlight ) { R_AddDrawSurfFlags( surface, shader, fog, dlight, 0 ); }
'''

PROBE = r'''
#include "tr_mohaa.c"

/* ---- a stand-in engine: every answer follows from every argument ---- */
#define FAKE_SURFACES 6
static const struct { const char *name; int numSkins; const char *shaders[2]; int flags, numVerts, numIndexes; } fake[FAKE_SURFACES] = {
    { "body", 2, { "skin/body", "skin/body_dirty" }, 0, 12, 30 },
    { "hidden", 1, { "skin/nodraw" }, MOHAA_TIKI_SURF_NODRAW, 6, 12 },
    { "helmet", 2, { "skin/helmet", "skin/helmet2" }, MOHAA_TIKI_SURF_NOMIPMAPS, 8, 18 },
    { "lod", 1, { "skin/lod" }, 0, 40, 114 },
    { "empty", 1, { "skin/empty" }, 0, 0, 0 },
    { "tiny", 1, { "skin/tiny" }, 0, 5, 9 },
};
static int fakeTiki, otherTiki, poseCalls, skinCalls, badIndex;
static float fakeLod = -1.0f;
static int NumSurfaces( const void *tiki ) { return tiki == &fakeTiki ? FAKE_SURFACES : 0; }
static qboolean Surface( const void *tiki, int i, mohaaTikiSurface_t *out ) {
    if ( tiki != &fakeTiki || i < 0 || i >= FAKE_SURFACES ) return qfalse;
    memset( out, 0, sizeof( *out ) );
    out->name = fake[i].name; out->numSkins = fake[i].numSkins; out->shaders[0] = fake[i].shaders[0];
    out->shaders[1] = fake[i].shaders[1]; out->flags = fake[i].flags; out->numVerts = fake[i].numVerts;
    out->numIndexes = fake[i].numIndexes;
    return qtrue;
}
static int PoseFloats( const void *tiki ) { (void)tiki; return 24; }
static qboolean Pose( const mohaaPoseInput_t *input, float *pose ) {
    int i;
    if ( !input || input->tiki != &fakeTiki || input->entityNumber == 99 ) return qfalse;
    poseCalls++;
    for ( i = 0; i < 24; i++ ) pose[i] = input->frameInfo[0].time + 0.25f * i + (float)input->entityNumber;
    return qtrue;
}
static int Skin( const void *tiki, int index, const float *pose, float scale, float lod, int lodFlags,
    float (*xyz)[4], float (*normal)[4], float (*st)[2], unsigned int *indexes, unsigned int baseVertex,
    int maxVerts, int maxIndexes, int *numIndexes ) {
    int drawn, v, k, n = 0;
    *numIndexes = 0;
    assert( tiki == &fakeTiki && pose );
    if ( index < 0 || index >= FAKE_SURFACES || fake[index].numVerts > maxVerts || fake[index].numIndexes > maxIndexes ) return 0;
    drawn = fake[index].numVerts;
    if ( drawn <= 0 ) return 0;
    if ( lod >= 0.0f && index == 5 ) return 0;      /* too small on screen */
    if ( lod >= 0.0f && index == 3 && !( lodFlags & MOHAA_LOD_VIEWMODEL ) ) drawn -= (int)lod;
    skinCalls++;
    for ( v = 0; v < drawn; v++ ) {
        xyz[v][0] = ( pose[0] * v + index ) * scale; xyz[v][1] = ( pose[12] + 0.5f * v ) * scale;
        xyz[v][2] = ( pose[1] * index - v + lod ) * scale; xyz[v][3] = 1.0f;
        normal[v][0] = (float)( v + 1 ); normal[v][1] = (float)( index + 2 ) + lodFlags; normal[v][2] = 3.0f; normal[v][3] = 0.0f;
        st[v][0] = 0.1f * v; st[v][1] = 0.2f * index;
    }
    for ( k = 1; k + 1 < drawn; k++ ) {
        indexes[n++] = baseVertex; indexes[n++] = baseVertex + k; indexes[n++] = baseVertex + k + 1;
    }
    if ( badIndex ) indexes[n - 1] = baseVertex + (unsigned int)drawn;
    *numIndexes = n;
    return drawn;
}
static void Bounds( const void *tiki, float scale, vec3_t mins, vec3_t maxs ) {
    (void)tiki; VectorSet( mins, -16 * scale, -16 * scale, -16 * scale ); VectorSet( maxs, 16 * scale, 16 * scale, 16 * scale );
}
static float Radius( const void *tiki ) { (void)tiki; return 32.0f; }
static float LoadScale( const void *tiki ) { (void)tiki; return 1.0f; }
static const void *RegisterStatic( const char *path ) { (void)path; return NULL; }
static qboolean StaticPose( const void *tiki, float *pose ) { (void)tiki; (void)pose; return qfalse; }
static float LodMetric( const mohaaPoseInput_t *input, float scale, const vec3_t origin, const float *axis,
    const vec3_t viewOrigin, float fovX ) {
    (void)input; (void)scale; (void)origin; (void)axis; (void)viewOrigin; (void)fovX; return fakeLod;
}
static const mohaaRenderImport_t engine = { NumSurfaces, Surface, PoseFloats, Pose, Skin, Bounds, Radius, LoadScale,
    RegisterStatic, StaticPose, LodMetric };

/* ---- the RTX transport's geometry helpers, bounded ---- */
enum { RTX_RT_MATFLAG_MASKED = 1 };
#define MAXV 512
typedef struct { int flags; } rtxRtMaterial_t;
typedef struct {
    uint32_t numVertices, numIndices, numMaterials, sourceTriangleCount, maskedTriangleCount, sourceEntityCount;
    struct { vec3_t xyz, normal; vec2_t st; uint32_t color, material; } vertices[MAXV];
    uint32_t indices[MAXV * 3];
    rtxRtMaterial_t materials[16]; const shader_t *shaders[16];
} rtxRtCpuGeometry_t;
static int failure, allocations;
#define RT_HostAlloc( bytes ) ( allocations++, malloc( (size_t)( bytes ) ) )
#define RT_HostFree( block ) free( block )
static qboolean vk_rt_cpu_array_growth( uint32_t current, uint32_t required, uint32_t initial, size_t size,
    const char *label, uint32_t *capacity, size_t *bytes ) {
    uint32_t c = current ? current : initial;
    (void)label;
    if ( failure == 5 ) return qfalse;
    while ( c < required ) c *= 2;
    *capacity = c; *bytes = (size_t)c * size;
    return qtrue;
}
static qboolean vk_rt_cpu_geometry_find_or_add_material( rtxRtCpuGeometry_t *g, const shader_t *shader, uint32_t *out ) {
    uint32_t i;
    if ( failure == 1 ) return qfalse;
    for ( i = 0; i < g->numMaterials; i++ ) if ( g->shaders[i] == shader ) { *out = i; return qtrue; }
    assert( g->numMaterials < 16 );
    g->shaders[g->numMaterials] = shader; g->materials[g->numMaterials].flags = shader->masked ? RTX_RT_MATFLAG_MASKED : 0;
    *out = g->numMaterials++;
    return qtrue;
}
static qboolean vk_rt_cpu_geometry_reserve( rtxRtCpuGeometry_t *g, uint32_t verts, uint32_t indexes ) {
    assert( g->numVertices + verts <= MAXV && g->numIndices + indexes <= MAXV * 3 );
    return failure == 2 ? qfalse : qtrue;
}
static void vk_rt_transform_point( const trRefEntity_t *ent, const vec3_t local, vec3_t world ) {
    VectorCopy( ent->e.origin, world );
    VectorMA( world, local[0], ent->e.axis[0], world );
    VectorMA( world, local[1], ent->e.axis[1], world );
    VectorMA( world, local[2], ent->e.axis[2], world );
}
static void vk_rt_transform_direction( const trRefEntity_t *ent, const vec3_t local, vec3_t world ) {
    int i;
    for ( i = 0; i < 3; i++ ) world[i] = local[0] * ent->e.axis[0][i] + local[1] * ent->e.axis[1][i] + local[2] * ent->e.axis[2][i];
    if ( VectorNormalize( world ) == 0.0f ) VectorSet( world, 0, 0, 1 );
}
static qboolean vk_rt_cpu_geometry_add_vertex( rtxRtCpuGeometry_t *g, const vec3_t xyz, const vec3_t normal,
    const vec4_t tangent, const vec2_t st, uint32_t color, uint32_t material, uint32_t *out ) {
    assert( !out && tangent[0] == 1 && tangent[3] == 1 && material < g->numMaterials );
    if ( failure == 3 ) return qfalse;
    VectorCopy( xyz, g->vertices[g->numVertices].xyz ); VectorCopy( normal, g->vertices[g->numVertices].normal );
    g->vertices[g->numVertices].st[0] = st[0]; g->vertices[g->numVertices].st[1] = st[1];
    g->vertices[g->numVertices].color = color; g->vertices[g->numVertices++].material = material;
    return qtrue;
}
static qboolean vk_rt_cpu_geometry_add_triangle( rtxRtCpuGeometry_t *g, uint32_t a, uint32_t b, uint32_t c ) {
    assert( a < g->numVertices && b < g->numVertices && c < g->numVertices );
    if ( failure == 4 ) return qfalse;
    g->indices[g->numIndices++] = a; g->indices[g->numIndices++] = b; g->indices[g->numIndices++] = c;
    return qtrue;
}
#include "code/renderercommon/tr_mohaa_ray_geometry.h"

/* ---- what the raster passes draw of an entity, surface by surface ---- */
typedef struct {
    const shader_t *shader; const surfaceType_t *surface; int numVerts, numIndexes;
    vec4_t xyz[64], normal[64]; vec2_t st[64]; unsigned int indexes[192];
} drawn_t;
static model_t *model;
static int Raster( trRefEntity_t *ent, drawn_t *out ) {
    int i, n = 0;
    numDraws = 0;
    tr.currentModel = model;
    R_AddMOHAATikiSurfaces( ent, NULL );
    for ( i = 0; i < numDraws; i++ ) {
        if ( draws[i].flags & DSF_SHADOW_CASTER_ONLY ) continue;
        memset( &tess, 0x7f, sizeof( tess ) ); tess.numVertexes = tess.numIndexes = 0;
        backEnd.currentEntity = ent;
        RB_SurfaceMOHAATiki( draws[i].surface );
        out[n].shader = draws[i].shader; out[n].surface = draws[i].surface;
        out[n].numVerts = tess.numVertexes; out[n].numIndexes = tess.numIndexes;
        memcpy( out[n].xyz, tess.xyz, sizeof( vec4_t ) * tess.numVertexes );
        memcpy( out[n].normal, tess.normal, sizeof( vec4_t ) * tess.numVertexes );
        memcpy( out[n].st, tess.texCoords[0], sizeof( vec2_t ) * tess.numVertexes );
        memcpy( out[n].indexes, tess.indexes, sizeof( unsigned int ) * tess.numIndexes );
        if ( tess.numVertexes ) n++;
    }
    return n;
}
/* the trace's geometry is raster's, vertex for vertex, in world space */
static void Same( const trRefEntity_t *ent, const drawn_t *drawn, int count, const rtxRtCpuGeometry_t *g ) {
    uint32_t first = 0, firstIndex = 0, triangles = 0;
    int s, i, j;
    for ( s = 0; s < count; s++ ) {
        const drawn_t *d = &drawn[s];
        for ( i = 0; i < d->numVerts; i++ ) {
            vec3_t world, normal;
            vk_rt_transform_point( ent, d->xyz[i], world );
            vk_rt_transform_direction( ent, d->normal[i], normal );
            for ( j = 0; j < 3; j++ ) {
                assert( g->vertices[first + i].xyz[j] == world[j] );
                assert( g->vertices[first + i].normal[j] == normal[j] );
            }
            assert( g->vertices[first + i].st[0] == d->st[i][0] && g->vertices[first + i].st[1] == d->st[i][1] );
            assert( g->vertices[first + i].color == 0xffffffffu );
            assert( g->shaders[g->vertices[first + i].material] == d->shader );
        }
        for ( i = 0; i < d->numIndexes; i++ ) assert( g->indices[firstIndex + i] == first + ( d->indexes[i] ) );
        first += d->numVerts; firstIndex += d->numIndexes; triangles += d->numIndexes / 3;
    }
    assert( g->numVertices == first && g->numIndices == firstIndex );
    assert( g->sourceTriangleCount == triangles && g->sourceEntityCount == (uint32_t)count );
}
static rtxRtCpuGeometry_t geometry;
static drawn_t drawn[16];
static void Entity( trRefEntity_t *ent, int number ) {
    memset( ent, 0, sizeof( *ent ) );
    ent->e.reType = RT_MODEL; ent->e.hModel = model->index;
    VectorSet( ent->e.origin, 100, -50, 25 );
    VectorSet( ent->e.axis[0], 0, 1, 0 ); VectorSet( ent->e.axis[1], -1, 0, 0 ); VectorSet( ent->e.axis[2], 0, 0, 1 );
    ent->e.mohaa.pose.tiki = &fakeTiki; ent->e.mohaa.pose.entityNumber = number;
    ent->e.mohaa.pose.frameInfo[0].time = 0.5f; ent->e.mohaa.pose.frameInfo[0].weight = 1.0f;
    ent->e.mohaa.scale = 1.25f; ent->e.mohaa.lod = -1.0f;
    ent->e.mohaa.surfaces[0] = 1;	/* the body's second skin */
    ent->e.mohaa.surfaces[2] = 2;	/* past the helmet's last skin: its first */
}
static void NewFrame( void ) { tr.frameCount++; }

/* OpenMoHAA's R_AddSkelSurfaces, as this file's tr_mohaa.c describes it */
static const shader_t *Expected( const trRefEntity_t *ent, const mohaaSurface_t *s, const shader_t **second, const shader_t **pass ) {
    const int state = ent->e.mohaa.surfaces[s->index];
    const int customPass = ( ent->e.mohaa.renderfx & ( 1 << 16 ) ) != 0;
    int skin;
    *second = *pass = NULL;
    if ( ( s->flags & MOHAA_TIKI_SURF_NODRAW ) || ( state & 4 ) || !s->numVerts || !s->numIndexes ) return NULL;
    if ( ent->e.customShader && !customPass ) return R_GetShaderByHandle( ent->e.customShader );
    if ( ent->e.customShader ) *pass = R_GetShaderByHandle( ent->e.customShader );
    if ( ( state & 0x40 ) && s->numSkins > 1 ) {
        skin = ( state & 2 ) + ent->e.skinNum + 1;
        *second = s->shaders[skin >= 0 && skin < s->numSkins ? skin : 0];
        skin = ( state & 2 ) + ent->e.skinNum;
    } else {
        skin = ( state & 3 ) + ent->e.skinNum;
    }
    if ( s->numSkins <= 0 ) return tr.defaultShader;
    return s->shaders[skin >= 0 && skin < s->numSkins ? skin : 0];
}

int main( void ) {
    trRefEntity_t ent, culled, reference;
    shader_t defaultShader = { 0 };
    world_t world = { 0, NULL };
    int count, calls, poses, i, state, skinNum, custom;

    ri.MOHAARender = &engine; ri.Printf = Printf; ri.Hunk_Alloc = Hunk_Alloc;
    tr.defaultShader = &defaultShader; strcpy( defaultShader.name, "<default>" );
    tr.world = &world; tr.numModels = 1; tr.frameCount = 1;
    model = R_GetModelByHandle( RE_RegisterMOHAATiki( "models/test.tik", &fakeTiki ) );
    assert( model && model->type == MOD_MOHAA_TIKI && model->numMohaaSurfaces == FAKE_SURFACES );
    assert( model->mohaaSurfaces[2].shaders[0]->mipmaps == qfalse && model->mohaaSurfaces[0].shaders[0]->mipmaps == qtrue );

    /* A visible entity: the front end poses it, the base pass skins and
     * keeps its surfaces, and the trace copies them from the cache. The
     * body takes its second skin, the helmet its first, the nodraw and
     * empty surfaces are drawn by neither. */
    Entity( &ent, 3 );
    count = Raster( &ent, drawn );
    assert( count == 4 && ent.e.mohaa.bones && ent.lightingCalculated );
    assert( !strcmp( drawn[0].shader->name, "skin/body_dirty" ) && !strcmp( drawn[1].shader->name, "skin/helmet" ) );
    assert( !strcmp( drawn[2].shader->name, "skin/lod" ) && drawn[2].numVerts == 40 && !strcmp( drawn[3].shader->name, "skin/tiny" ) );
    calls = skinCalls; poses = poseCalls;
    memset( &geometry, 0, sizeof( geometry ) );
    assert( vk_rt_append_mohaa_entity_geometry( &geometry, &ent ) );
    Same( &ent, drawn, count, &geometry );
    assert( skinCalls == calls && poseCalls == poses );
    assert( R_MOHAATraceable( &ent, model ) );

    /* At the detail the main view measured: the trace drops what raster
     * drops, the level-of-detail surface's vertices and the tiny surface. */
    NewFrame(); fakeLod = 25.0f;
    Entity( &ent, 3 );
    count = Raster( &ent, drawn );
    assert( count == 3 && ent.e.mohaa.lod == 25.0f && drawn[2].numVerts == 15 );
    memset( &geometry, 0, sizeof( geometry ) );
    assert( vk_rt_append_mohaa_entity_geometry( &geometry, &ent ) );
    Same( &ent, drawn, count, &geometry );

    /* A view weapon keeps its view-model detail in both. */
    NewFrame();
    Entity( &ent, 3 ); ent.e.renderfx = RF_DEPTHHACK | RF_FIRST_PERSON;
    count = Raster( &ent, drawn );
    assert( count == 3 && drawn[2].numVerts == 40 );
    memset( &geometry, 0, sizeof( geometry ) );
    assert( vk_rt_append_mohaa_entity_geometry( &geometry, &ent ) );
    Same( &ent, drawn, count, &geometry );

    /* An entity culled from the view has no pose and no measured detail:
     * the trace poses it whole into the frame's pool, leaves the entity as
     * it was, and keeps nothing in the raster passes' cache. */
    NewFrame(); fakeLod = -1.0f;
    Entity( &culled, 4 ); cullAll = 1;
    assert( Raster( &culled, drawn ) == 0 && !culled.e.mohaa.bones );
    poses = poseCalls; calls = skinCalls;
    memset( &geometry, 0, sizeof( geometry ) );
    assert( vk_rt_append_mohaa_entity_geometry( &geometry, &culled ) );
    assert( poseCalls == poses + 1 && skinCalls == calls + 4 && !culled.e.mohaa.bones && culled.e.mohaa.lod == -1.0f );
    assert( mohaaSkinCache.generation == mohaaPoseGeneration && mohaaSkinCache.usedVerts == 0 );
    cullAll = 0;
    reference = culled;
    count = Raster( &reference, drawn );
    assert( count == 4 );
    Same( &culled, drawn, count, &geometry );
    calls = skinCalls;
    memset( &geometry, 0, sizeof( geometry ) );
    assert( vk_rt_append_mohaa_entity_geometry( &geometry, &culled ) && skinCalls == calls + 4 );
    Same( &culled, drawn, count, &geometry );

    /* An entity the engine cannot pose has nothing to trace, as nothing is drawn. */
    NewFrame();
    Entity( &ent, 99 );
    assert( Raster( &ent, drawn ) == 0 );
    memset( &geometry, 0, sizeof( geometry ) );
    assert( vk_rt_append_mohaa_entity_geometry( &geometry, &ent ) && !geometry.numVertices && !geometry.sourceEntityCount );

    /* A custom shader replacing every skin is traced with it. */
    NewFrame();
    Entity( &ent, 3 ); ent.e.customShader = model->mohaaSurfaces[3].shaders[0]->index;
    count = Raster( &ent, drawn );
    assert( count == 4 );
    for ( i = 0; i < count; i++ ) assert( drawn[i].shader == model->mohaaSurfaces[3].shaders[0] );
    memset( &geometry, 0, sizeof( geometry ) );
    assert( vk_rt_append_mohaa_entity_geometry( &geometry, &ent ) && geometry.numMaterials == 1 );
    Same( &ent, drawn, count, &geometry );
    assert( R_MOHAATraceable( &ent, model ) );

    /* One shader a surface: a custom shader over the skins, a crossfade's
     * second skin, and a sky entity leave the entity to raster. */
    ent.e.mohaa.renderfx = 1 << 16;
    assert( !R_MOHAATraceable( &ent, model ) );
    ent.e.customShader = 0;
    assert( R_MOHAATraceable( &ent, model ) );
    ent.e.mohaa.renderfx = 0; ent.e.mohaa.surfaces[0] = 0x40;
    assert( !R_MOHAATraceable( &ent, model ) );
    ent.e.mohaa.surfaces[0] = 0x44;	/* hidden: draws nothing at all */
    assert( R_MOHAATraceable( &ent, model ) );
    ent.e.mohaa.surfaces[0] = 0; ent.e.mohaa.surfaces[3] = 0x40;	/* one skin: no second */
    assert( R_MOHAATraceable( &ent, model ) );
    ent.e.mohaa.renderfx = 1 << 14;
    assert( !R_MOHAATraceable( &ent, model ) );

    /* The eye inside a model's bounds (here 20 units each way, at scale
     * 1.25), along the entity's own turned axes. */
    {
        static const float steps[][4] = {
            { 19, 0, 0, 1 }, { 21, 0, 0, 0 }, { 0, -19, 0, 1 }, { 0, -21, 0, 0 },
            { 0, 0, 19.5f, 1 }, { 0, 0, -20.5f, 0 }, { 20, 20, 20, 1 }, { NAN, 0, 0, 0 } };
        Entity( &ent, 3 );
        for ( i = 0; i < (int)( sizeof( steps ) / sizeof( steps[0] ) ); i++ ) {
            vec3_t eye;
            VectorCopy( ent.e.origin, eye );
            VectorMA( eye, steps[i][0], ent.e.axis[0], eye );
            VectorMA( eye, steps[i][1], ent.e.axis[1], eye );
            VectorMA( eye, steps[i][2], ent.e.axis[2], eye );
            assert( R_MOHAAViewInside( &ent, model, eye ) == (qboolean)steps[i][3] );
        }
        ri.MOHAARender = NULL;
        assert( !R_MOHAAViewInside( &ent, model, ent.e.origin ) );
        ri.MOHAARender = &engine;
    }

    /* Every surface state, skin number and custom shader: the shader and
     * the passes are the documented rule's. */
    for ( custom = 0; custom < 3; custom++ ) {
        for ( skinNum = -1; skinNum <= 3; skinNum++ ) {
            for ( state = 0; state < 256; state++ ) {
                for ( i = 0; i < FAKE_SURFACES; i++ ) {
                    const shader_t *wantSecond, *wantPass, *want;
                    shader_t *second, *pass;
                    Entity( &ent, 3 );
                    ent.e.skinNum = skinNum; ent.e.mohaa.surfaces[i] = (byte)state;
                    ent.e.customShader = custom ? model->mohaaSurfaces[0].shaders[1]->index : 0;
                    ent.e.mohaa.renderfx = custom == 2 ? 1 << 16 : 0;
                    want = Expected( &ent, &model->mohaaSurfaces[i], &wantSecond, &wantPass );
                    assert( R_MOHAASurfaceShader( &ent, &model->mohaaSurfaces[i], &second, &pass ) == want );
                    assert( second == wantSecond && pass == wantPass );
                    assert( R_MOHAASurfaceShader( &ent, &model->mohaaSurfaces[i], NULL, NULL ) == want );
                }
            }
        }
    }

    /* A triangle naming a vertex past the surface is not traced. */
    NewFrame(); badIndex = 1;
    Entity( &culled, 5 ); cullAll = 1;
    memset( &geometry, 0, sizeof( geometry ) );
    assert( vk_rt_append_mohaa_entity_geometry( &geometry, &culled ) );
    assert( geometry.sourceTriangleCount == ( 10 - 1 ) + ( 6 - 1 ) + ( 38 - 1 ) + ( 3 - 1 ) );
    for ( i = 0; i < (int)geometry.numIndices; i++ ) assert( geometry.indices[i] < geometry.numVertices );
    badIndex = 0; cullAll = 0;

    /* A failing helper fails the extraction, which leaves the frame to raster. */
    for ( failure = 1; failure <= 5; failure++ ) {
        RT_MOHAAScratchFree();
        memset( &geometry, 0, sizeof( geometry ) );
        assert( !vk_rt_append_mohaa_entity_geometry( &geometry, &culled ) );
        assert( !geometry.sourceEntityCount );
    }
    failure = 0;

    /* The scratch grows once for the largest surface and is given back. */
    RT_MOHAAScratchFree();
    allocations = 0;
    memset( &geometry, 0, sizeof( geometry ) );
    assert( vk_rt_append_mohaa_entity_geometry( &geometry, &culled ) && allocations == 2 );
    assert( s_rtMohaaScratchVerts >= 40 && s_rtMohaaScratchIndexes >= 114 );
    RT_MOHAAScratchFree();
    assert( !s_rtMohaaXyz && !s_rtMohaaIndexes && !s_rtMohaaScratchVerts );

    /* Not a TIKI model, or no engine: nothing, and no failure. */
    ri.MOHAARender = NULL;
    memset( &geometry, 0, sizeof( geometry ) );
    assert( vk_rt_append_mohaa_entity_geometry( &geometry, &culled ) && !geometry.numVertices );
    ri.MOHAARender = &engine;
    model->type = MOD_BAD;
    assert( vk_rt_append_mohaa_entity_geometry( &geometry, &culled ) && !geometry.numVertices );
    model->type = MOD_MOHAA_TIKI;
    (void)otherTiki;
    printf( "ok\n" );
    return 0;
}
'''


def read(path):
    return (ROOT / path).read_text(encoding='utf-8').replace('\r\n', '\n')


class MOHAARayGeometryTests(unittest.TestCase):
    def test_traced_surfaces_are_the_raster_passes_surfaces(self):
        compiler = shutil.which('clang') or shutil.which('gcc')
        if not compiler:
            self.skipTest('C compiler unavailable')
        flags = compiler_target_args(compiler)
        if os.name == 'nt' and not flags:
            flags = ['-DFNTECH3_Q3_ONLY']
        surface = re.search(r'typedef struct mohaaSurface_s \{.*?\} mohaaSurface_t;',
                            read('code/rendererrtx/tr_local.h'), re.S)[0]
        (ROOT / '.tmp').mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='mohaa-ray-', dir=ROOT / '.tmp') as temp:
            folder = Path(temp)
            # the RTX renderer's own file, beside a stand-in for its tr_local.h
            (folder / 'tr_mohaa.c').write_text(read('code/rendererrtx/tr_mohaa.c'), encoding='utf-8')
            (folder / 'tr_local.h').write_text(STUB.replace('@MOHAA_SURFACE@', surface), encoding='utf-8')
            (folder / 'probe.c').write_text(PROBE, encoding='utf-8')
            exe = folder / 'probe.exe'
            built = subprocess.run([compiler, *flags, '-std=gnu11', '-Wall', '-Wno-unused-function',
                                    '-I', str(folder), '-I', str(ROOT), '-I', str(ROOT / 'code' / 'rendererrtx'),
                                    str(folder / 'probe.c'), '-o', str(exe)],
                                   cwd=ROOT, capture_output=True, text=True, timeout=120)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            ran = subprocess.run([str(exe)], cwd=ROOT, capture_output=True, text=True, timeout=60)
            self.assertEqual(ran.returncode, 0, ran.stdout + ran.stderr)
            self.assertEqual(ran.stdout.strip(), 'ok')

    def test_the_tracer_takes_tiki_entities_and_their_surfaces(self):
        vk = read('code/rendererrtx/vk.c')
        backend = read('code/rendererrtx/rt/rt_backend.c')
        self.assertIn('#include "../renderercommon/tr_mohaa_ray_geometry.h"', vk)
        extract = vk[vk.index('static qboolean vk_rt_extract_dynamic_model_entity_geometry('):]
        extract = extract[:extract.index('\n}\n')]
        self.assertIn('case MOD_MOHAA_TIKI:\n\t\tif ( !vk_rt_append_mohaa_entity_geometry( geometry, ent ) ) '
                      'return qfalse;', extract)
        entity = backend[backend.index('static int RT_EntityClass( const trRefEntity_t *ent )'):]
        entity = entity[:entity.index('\n}\n')]
        self.assertIn('case MOD_MOHAA_TIKI:', entity)
        self.assertIn('if ( !R_MOHAATraceable( ent, model ) ) {', entity)
        owner = backend[backend.index('rtSurfaceOwner_t vk_rt_surface_owner('):]
        owner = owner[:owner.index('\n}\n')]
        self.assertLess(owner.index('case SF_MOHAA_TIKI:'), owner.index('// the model formats RT_ExtractDynamic poses'))
        # the skinning scratch goes with the tracer's other host arrays
        self.assertIn('s_rtIqmScratchCount = 0;\n\tRT_MOHAAScratchFree();', backend)
        # a frame's pose is no geometry-cache key
        cache = read('code/rendererrtx/rt/rt_geometry_cache.h')
        key = cache[cache.index('static qboolean RT_GeometryKey('):]
        key = key[:key.index('\n}\n')]
        self.assertNotIn('MOD_MOHAA_TIKI', key)
        # the front end draws each surface with the same choice the trace takes
        model = read('code/rendererrtx/tr_mohaa.c')
        add = model[model.index('void R_AddMOHAATikiSurfaces('):]
        add = add[:add.index('\n}\n')]
        self.assertIn('shader_t *shader = R_MOHAASurfaceShader( ent, surface, &second, &pass );', add)


if __name__ == '__main__':
    unittest.main()
