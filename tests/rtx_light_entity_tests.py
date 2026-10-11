"""Execute the path tracer's light entities and backsplash lights
(code/rendererrtx/rt/rt_lights.c) against a bounded host.

A map's light entities mean what its compiler made them mean. id's q3map
(Quake III): spawnflags 1 is linear, every light has Lambert's cosine, and
"light" is read before "_light". q3map2 (it marks worldspawn with
_q3map2_version): "_light" first, times "scale"; linear lights and lights with
spawnflags 2 lose the cosine. The Wolfenstein compilers (RTCW, ET): a light is
linear without the cosine unless spawnflags 1 (inverse square, with it);
spawnflags 2 adds the cosine and "fade" scales a linear light's distance
term. A spotlight is inverse square with the cosine for all of them. A light
whose target names no entity stays a point light for id's q3map and q3map2;
RTCW's own compiler lit nothing with one, as its retail lightmaps show. A
lightJunior lights only models (the compilers' light grid); a Wolfenstein or
q3map2 light with spawnflags 16 (nogrid) only world surfaces.

Backsplash: id's q3map gives an emitting winding no larger than the light
subdivision a point light 1 + 24 units in front of its centre with 5% of its
photons, and a larger winding none; q3map2 gives every piece one, 1 + 23
units out, but none to a sky surface; q3map_backsplash overrides both.
"""
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args
from mesh_tag_tests import function

ROOT = Path(__file__).resolve().parents[1]


def compile_run_c(test, source):
    """Build and run a C11 probe: the renderer's C, as the renderer compiles it."""
    compiler = shutil.which('clang') or shutil.which('gcc')
    if not compiler:
        test.skipTest('C11 compiler unavailable')
    flags = compiler_target_args(compiler)
    if os.name == 'nt' and not flags:
        flags = ['-DFNTECH3_Q3_ONLY']
    (ROOT / '.tmp').mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='rtx-lights-', dir=ROOT / '.tmp') as temp:
        c, exe = Path(temp) / 'probe.c', Path(temp) / 'probe.exe'
        c.write_text(source, encoding='utf-8')
        built = subprocess.run([compiler, *flags, '-std=c11', '-O2', '-I', str(ROOT), str(c), '-o', str(exe)]
                               + ([] if os.name == 'nt' else ['-lm']),
                               cwd=ROOT, capture_output=True, text=True, timeout=120)
        test.assertEqual(built.returncode, 0, built.stdout + built.stderr)
        ran = subprocess.run([str(exe)], cwd=ROOT, capture_output=True, text=True, timeout=30)
        test.assertEqual(ran.returncode, 0, ran.stdout + ran.stderr)


def between(text, start, end):
    begin = text.index(start)
    return text[begin:text.index(end, begin) + len(end)] + '\n'


def harness():
    lights = (ROOT / 'code/rendererrtx/rt/rt_lights.c').read_text(encoding='utf-8')
    material = (ROOT / 'code/rendererrtx/rt/rt_material.c').read_text(encoding='utf-8')
    defines = '\n'.join(line for line in lights.splitlines() if line.startswith('#define RT_LIGHT_'))
    source = r'''
#define _CRT_SECURE_NO_WARNINGS 1
#include "code/qcommon/q_shared.c"
#include "code/qcommon/q_math.c"
#include "code/qcommon/surfaceflags.h"
#include "code/rendererrtx/rt/rt_gpu.h"
#include <assert.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
void QDECL Com_Printf( const char *fmt, ... ) { (void)fmt; }
void QDECL Com_DPrintf( const char *fmt, ... ) { (void)fmt; }
void NORETURN QDECL Com_Error( errorParm_t level, const char *fmt, ... ) { (void)level; (void)fmt; exit( 3 ); }

typedef struct {
    int surfaceFlags;
    qboolean isSky, rtCompiledSky;
    float surfaceLightSubdivide;
    qboolean rtBacksplashValid;
    float rtBacksplashFraction, rtBacksplashDistance;
} shader_t;
typedef struct mnode_s {
    int contents;
    cplane_t *plane;
    struct mnode_s *children[2];
    int cluster;
} mnode_t;
typedef struct { const char *entityString; mnode_t *nodes; } world_t;
static struct { world_t *world; } tr;
static void *HostMalloc( int bytes ) { return calloc( 1, (size_t)bytes ); }
static void HostFree( void *p ) { free( p ); }
static void HostPrintf( int level, const char *fmt, ... ) { (void)level; (void)fmt; }
#include "code/qcommon/game_profile.h"
static struct {
    void *( *Malloc )( int );
    void ( *Free )( void * );
    void ( *Printf )( int, const char *, ... );
    comGameId_t gameId;
} ri = { HostMalloc, HostFree, HostPrintf, COM_GAME_QUAKE3 };
#define RT_HostAlloc( bytes ) malloc( (size_t)( bytes ) )
#define RT_HostFree( block ) free( block )
typedef struct { float xyz[4]; } rtxRtPackedVertex_t;
typedef struct {
    rtxRtPackedVertex_t *vertices;
    uint32_t *indices;
    uint32_t numVertices, numIndices;
} rtxRtCpuGeometry_t;
'''
    source += defines + '\n'
    source += between(lights, 'typedef struct {\n\tchar\tclassname[64];', '} rtLightEntity_t;')
    source += between(lights, 'typedef struct {\n\trtGpuLight_t\t*lights;', '} rtLightScene_t;')
    source += 'static rtLightScene_t s_rtLights;\n'
    for signature in (
        'static qboolean RT_FloatValid( float v, float limit )',
        'static qboolean RT_Vec3Valid( const vec3_t v, float limit )',
        'static qboolean RT_LightsReserve( uint32_t count )',
        'static const mnode_t *RT_PointInLeaf( const vec3_t p )',
        'static int RT_PointCluster( const vec3_t p )',
        'static int RT_ParseLightEntities( rtLightEntity_t *entities, int maxEntities )',
        'static rtGpuLight_t *RT_AddLight( const vec3_t center, float reach )',
        'static void RT_AddPointLight(',
        'static void RT_WorldspawnLight( const rtLightEntity_t *ent )',
        'static void RT_AddEntityLights( void )',
    ):
        source += function(lights, signature)
    source += function(material, 'static qboolean RT_ShaderIsSky( const shader_t *shader )')
    source += between(lights, '#define RT_BACKSPLASH_BUCKETS', '} rtBacksplash_t;')
    for signature in (
        'static void RT_BacksplashAdd(',
        'static void RT_BacksplashEmit(',
        'static void RT_AddBacksplash(',
    ):
        source += function(lights, signature)
    return source


CHECKS = r'''
static world_t world;

static void Load( const char *entities, qboolean wolf ) {
    if ( s_rtLights.lights ) free( s_rtLights.lights );
    if ( s_rtLights.reach ) free( s_rtLights.reach );
    if ( s_rtLights.center ) free( s_rtLights.center );
    if ( s_rtLights.cluster ) free( s_rtLights.cluster );
    memset( &s_rtLights, 0, sizeof( s_rtLights ) );
    s_rtLights.scale = 2.0f / 255.0f;
    s_rtLights.wolf = wolf;
    world.entityString = entities;
    world.nodes = NULL;
    tr.world = &world;
    RT_AddEntityLights();
}
static uint32_t Type( int i ) { return (uint32_t)s_rtLights.lights[i].color[3] & RT_LIGHT_TYPE_MASK; }
static uint32_t Skip( int i ) { return (uint32_t)s_rtLights.lights[i].color[3] & ~RT_LIGHT_TYPE_MASK; }
static bool Angle( int i ) { return s_rtLights.lights[i].extra[3] < 0.5f; }
static bool Near( float a, float b ) { return fabsf( a - b ) <= 1e-3f * fmaxf( 1.0f, fabsf( b ) ); }
// a linear light's photons * linearScale, an inverse-square light's colour
static float Linear( int i ) { return s_rtLights.lights[i].extra[1]; }
static float Photons( int i ) { return s_rtLights.lights[i].color[1] / s_rtLights.scale; }

int main() {
    /* id's q3map: "light" before "_light"; spawnflags 1 linear; the cosine always */
    Load( "{ \"classname\" \"worldspawn\" } "
          "{ \"classname\" \"light\" \"origin\" \"0 0 0\" \"light\" \"100\" \"spawnflags\" \"1\" } "
          "{ \"classname\" \"light\" \"origin\" \"0 0 0\" \"_light\" \"200\" } "
          "{ \"classname\" \"light\" \"origin\" \"0 0 0\" \"light\" \"50\" \"_light\" \"200\" \"spawnflags\" \"2\" } "
          "{ \"classname\" \"light\" \"origin\" \"0 0 0\" \"light\" \"80\" \"spawnflags\" \"16\" } ", qfalse );
    assert( s_rtLights.numLights == 4 );
    assert( Type( 0 ) == RT_LIGHT_POINT_LINEAR && Angle( 0 ) && Near( Linear( 0 ), 100 * 7500.0f / 8000.0f ) );
    assert( s_rtLights.lights[0].extra[2] == 1.0f );
    assert( Type( 1 ) == RT_LIGHT_POINT && Angle( 1 ) && Near( Photons( 1 ), 200 * 7500.0f ) );
    assert( Type( 2 ) == RT_LIGHT_POINT && Angle( 2 ) && Near( Photons( 2 ), 50 * 7500.0f ) );
    /* nogrid means nothing to id's q3map */
    assert( Skip( 3 ) == 0 );

    /* q3map2: "_light" first, times "scale"; linear and spawnflags 2 drop the cosine */
    Load( "{ \"classname\" \"worldspawn\" \"_q3map2_version\" \"2.5.17\" } "
          "{ \"classname\" \"light\" \"origin\" \"0 0 0\" \"light\" \"50\" \"_light\" \"200\" \"scale\" \"2\" } "
          "{ \"classname\" \"light\" \"origin\" \"0 0 0\" \"light\" \"100\" \"spawnflags\" \"1\" } "
          "{ \"classname\" \"light\" \"origin\" \"0 0 0\" \"light\" \"100\" \"spawnflags\" \"2\" } "
          "{ \"classname\" \"light\" \"origin\" \"0 0 0\" \"light\" \"100\" \"spawnflags\" \"16\" } "
          "{ \"classname\" \"lightJunior\" \"origin\" \"0 0 0\" \"light\" \"100\" } ", qfalse );
    assert( s_rtLights.q3map2 && s_rtLights.numLights == 5 );
    assert( Type( 0 ) == RT_LIGHT_POINT && Angle( 0 ) && Near( Photons( 0 ), 400 * 7500.0f ) );
    assert( Type( 1 ) == RT_LIGHT_POINT_LINEAR && !Angle( 1 ) );
    assert( Type( 2 ) == RT_LIGHT_POINT && !Angle( 2 ) );
    assert( Skip( 3 ) == RT_LIGHT_NO_MODELS );
    assert( Skip( 4 ) == RT_LIGHT_NO_SURFACES );
    /* a q3map2 build that wrote no _q3map2_version: keys only it reads */
    Load( "{ \"classname\" \"worldspawn\" \"_lightmapscale\" \"0.25\" } "
          "{ \"classname\" \"light\" \"origin\" \"0 0 0\" \"light\" \"50\" \"_light\" \"200\" } ", qfalse );
    assert( s_rtLights.q3map2 && Near( Photons( 0 ), 200 * 7500.0f ) );
    Load( "{ \"classname\" \"worldspawn\" \"_blocksize\" \"0\" } ", qfalse );
    assert( s_rtLights.q3map2 );
    /* _farplanedist marks q3map2 but for the Wolfenstein profiles, whose own
       tools set it too (RTCW's mp_tram, beside "fogclip", keeps its lights) */
    Load( "{ \"classname\" \"worldspawn\" \"_farplanedist\" \"3000\" } ", qfalse );
    assert( s_rtLights.q3map2 );
    ri.gameId = COM_GAME_RTCW_MP;
    Load( "{ \"classname\" \"worldspawn\" \"_farplanedist\" \"3000\" \"fogclip\" \"3000\" } ", qtrue );
    assert( !s_rtLights.q3map2 );
    ri.gameId = COM_GAME_QUAKE3;
    /* every light entity counts, lit or not: q3map2 leaves none in a map it
       lit unless -keeplights, and then only the lightmaps hold their light */
    Load( "{ \"classname\" \"worldspawn\" \"_q3map2_version\" \"2.5.17\" } "
          "{ \"classname\" \"info_null\" \"origin\" \"0 0 0\" } ", qfalse );
    assert( s_rtLights.q3map2 && s_rtLights.entityLights == 0 && s_rtLights.numLights == 0 );
    Load( "{ \"classname\" \"worldspawn\" \"_q3map2_version\" \"2.5.17\" } "
          "{ \"classname\" \"light\" } "
          "{ \"classname\" \"lightJunior\" \"origin\" \"0 0 0\" } ", qfalse );
    assert( s_rtLights.entityLights == 2 && s_rtLights.numLights == 1 );
    /* ... or its shaders, whatever the worldspawn says */
    Load( "{ \"classname\" \"worldspawn\" } ", qfalse );
    assert( !s_rtLights.q3map2 );
    memset( &s_rtLights, 0, sizeof( s_rtLights ) );
    s_rtLights.q3map2Shaders = qtrue;
    RT_AddEntityLights();
    assert( s_rtLights.q3map2 );

    /* the Wolfenstein model: linear without the cosine by default */
    Load( "{ \"classname\" \"worldspawn\" } "
          "{ \"classname\" \"light\" \"origin\" \"0 0 0\" \"light\" \"250\" } "
          "{ \"classname\" \"light\" \"origin\" \"0 0 0\" \"light\" \"75\" \"spawnflags\" \"19\" } "
          "{ \"classname\" \"light\" \"origin\" \"0 0 0\" \"light\" \"100\" \"spawnflags\" \"2\" \"fade\" \"2\" } "
          "{ \"classname\" \"lightJunior\" \"origin\" \"0 0 64\" \"light\" \"250\" } "
          "{ \"classname\" \"light\" \"origin\" \"0 0 0\" \"light\" \"100\" \"target\" \"t\" } "
          "{ \"classname\" \"info_null\" \"origin\" \"0 0 -128\" \"targetname\" \"t\" } "
          "{ \"classname\" \"light\" \"origin\" \"0 0 0\" \"light\" \"100\" \"fade\" \"nan\" } ", qtrue );
    assert( !s_rtLights.q3map2 && s_rtLights.numLights == 6 );
    assert( Type( 0 ) == RT_LIGHT_POINT_LINEAR && !Angle( 0 ) && Skip( 0 ) == 0 );
    assert( Near( Linear( 0 ), 250 * 7500.0f / 8000.0f ) && s_rtLights.reach[0] == Linear( 0 ) );
    /* spawnflags 19 = 16 | 2 | 1: inverse square, the cosine, surfaces only (mp_ice) */
    assert( Type( 1 ) == RT_LIGHT_POINT && Angle( 1 ) && Skip( 1 ) == RT_LIGHT_NO_MODELS );
    /* fade scales the distance term, and so the reach */
    assert( Type( 2 ) == RT_LIGHT_POINT_LINEAR && Angle( 2 ) && s_rtLights.lights[2].extra[2] == 2.0f );
    assert( Near( s_rtLights.reach[2], 100 * 7500.0f / 8000.0f / 2.0f ) );
    /* a junior lights models only; it is still linear */
    assert( Type( 3 ) == RT_LIGHT_POINT_LINEAR && Skip( 3 ) == RT_LIGHT_NO_SURFACES );
    /* a spotlight is inverse square with the cosine */
    assert( Type( 4 ) == RT_LIGHT_SPOT && Angle( 4 ) );
    /* a fade that is not a number falls back to 1 */
    assert( Type( 5 ) == RT_LIGHT_POINT_LINEAR && s_rtLights.lights[5].extra[2] == 1.0f );

    /* a light whose target names no entity: RTCW's compiler lit nothing with
       it (mp_depot's Cargo Bay), id's q3map and q3map2 a point light */
    Load( "{ \"classname\" \"worldspawn\" } "
          "{ \"classname\" \"light\" \"origin\" \"0 0 0\" \"light\" \"300\" \"target\" \"gone\" } "
          "{ \"classname\" \"light\" \"origin\" \"0 0 0\" \"light\" \"300\" \"target\" \"t\" } "
          "{ \"classname\" \"info_null\" \"origin\" \"0 0 -128\" \"targetname\" \"t\" } ", qtrue );
    assert( s_rtLights.numLights == 1 && Type( 0 ) == RT_LIGHT_SPOT && s_rtLights.untargetedLights == 1 );
    Load( "{ \"classname\" \"worldspawn\" \"_q3map2_version\" \"2.5.17\" } "
          "{ \"classname\" \"light\" \"origin\" \"0 0 0\" \"light\" \"300\" \"target\" \"gone\" } ", qtrue );
    assert( s_rtLights.numLights == 1 && Type( 0 ) == RT_LIGHT_POINT_LINEAR && s_rtLights.untargetedLights == 0 );
    Load( "{ \"classname\" \"worldspawn\" } "
          "{ \"classname\" \"light\" \"origin\" \"0 0 0\" \"light\" \"300\" \"target\" \"gone\" } ", qfalse );
    assert( s_rtLights.numLights == 1 && Type( 0 ) == RT_LIGHT_POINT && s_rtLights.untargetedLights == 0 );
    /* ... but only once every entity was read: past the parse's limit the
       target may be among those it dropped */
    {
        static const char one[] = "{ \"classname\" \"light\" \"origin\" \"0 0 0\" \"target\" \"gone\" } ";
        const size_t count = RT_LIGHT_MAX_ENTITIES + 8;
        char *many = malloc( count * sizeof( one ) + 64 ), *at = many;
        at += sprintf( at, "{ \"classname\" \"worldspawn\" } " );
        for ( size_t i = 0; i < count; i++ ) {
            memcpy( at, one, sizeof( one ) - 1 );
            at += sizeof( one ) - 1;
        }
        *at = 0;
        Load( many, qtrue );
        assert( s_rtLights.untargetedLights == 0 && s_rtLights.numLights == RT_LIGHT_MAX_ENTITIES - 1 );
        free( many );
    }

    /* backsplash */
    shader_t shader;
    float total = 0;
    rtxRtPackedVertex_t square[4] = { { { 0, 0, 0 } }, { { 100, 0, 0 } }, { { 100, 100, 0 } }, { { 0, 100, 0 } } };
    rtxRtPackedVertex_t strip[4] = { { { 0, 0, 0 } }, { { 3000, 0, 0 } }, { { 3000, 100, 0 } }, { { 0, 100, 0 } } };
    uint32_t quad[6] = { 0, 1, 2, 0, 2, 3 };
    rtxRtCpuGeometry_t small = { square, quad, 4, 6 };
    rtxRtCpuGeometry_t large = { strip, quad, 4, 6 };
    const vec3_t up = { 0, 0, 1 }, white = { 1, 1, 1 };

    /* id's q3map: a face within the subdivision, one light at its centre */
    Load( "", qfalse );
    memset( &shader, 0, sizeof( shader ) );
    RT_AddBacksplash( &shader, qtrue, &small, up, white, 1000 );
    assert( s_rtLights.numLights == 1 );
    assert( Near( s_rtLights.lights[0].position[0], 50 ) && Near( s_rtLights.lights[0].position[1], 50 ) );
    assert( Near( s_rtLights.lights[0].position[2], 25 ) );
    assert( Near( Photons( 0 ), 1000 * 10000 * 0.25f * 0.05f ) && Angle( 0 ) );
    /* ... a larger face was subdivided and gets none */
    Load( "", qfalse );
    RT_AddBacksplash( &shader, qtrue, &large, up, white, 1000 );
    assert( s_rtLights.numLights == 0 );
    /* ... unless the shader's q3map_lightsubdivide takes it whole */
    shader.surfaceLightSubdivide = 4096;
    RT_AddBacksplash( &shader, qtrue, &large, up, white, 1000 );
    assert( s_rtLights.numLights == 1 );
    shader.surfaceLightSubdivide = 0;
    /* q3map_backsplash <percent> <distance> */
    Load( "", qfalse );
    shader.rtBacksplashValid = qtrue;
    shader.rtBacksplashFraction = 0.10f;
    shader.rtBacksplashDistance = 8;
    RT_AddBacksplash( &shader, qtrue, &small, up, white, 1000 );
    assert( s_rtLights.numLights == 1 && Near( s_rtLights.lights[0].position[2], 9 ) );
    assert( Near( Photons( 0 ), 1000 * 10000 * 0.25f * 0.10f ) );
    shader.rtBacksplashFraction = 0;
    Load( "", qfalse );
    RT_AddBacksplash( &shader, qtrue, &small, up, white, 1000 );
    assert( s_rtLights.numLights == 0 );
    shader.rtBacksplashValid = qfalse;

    /* q3map2: every piece of a large face, the photons conserved */
    Load( "{ \"classname\" \"worldspawn\" \"_q3map2_version\" \"2.5.17\" } ", qfalse );
    s_rtLights.numLights = 0;
    RT_AddBacksplash( &shader, qtrue, &large, up, white, 1000 );
    assert( s_rtLights.numLights >= 2 );
    for ( uint32_t i = 0; i < s_rtLights.numLights; ++i ) {
        total += Photons( (int)i );
        assert( Near( s_rtLights.lights[i].position[2], 24 ) );
    }
    assert( Near( total, 1000 * 300000 * 0.25f * 0.05f ) );
    /* ... and none on a sky surface */
    s_rtLights.numLights = 0;
    shader.surfaceFlags = SURF_SKY;
    RT_AddBacksplash( &shader, qtrue, &small, up, white, 1000 );
    assert( s_rtLights.numLights == 0 );
    return 0;
}
'''


def grid_harness(listed):
    """The light grid's build, with every cell's candidates listed from the
    lights' reach (listed) or found by scanning every light (not listed)."""
    lights = (ROOT / 'code/rendererrtx/rt/rt_lights.c').read_text(encoding='utf-8')
    defines = [line for line in lights.splitlines() if line.startswith('#define RT_LIGHT_')]
    if not listed:
        defines = [re.sub(r'^(#define RT_LIGHT_MAX_LISTED\s+).*$', r'\g<1>0u', line) for line in defines]
    source = r'''
#define _CRT_SECURE_NO_WARNINGS 1
#include "code/qcommon/q_shared.c"
#include "code/qcommon/q_math.c"
#include "code/rendererrtx/rt/rt_gpu.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
void QDECL Com_Printf( const char *fmt, ... ) { (void)fmt; }
void QDECL Com_DPrintf( const char *fmt, ... ) { (void)fmt; }
void NORETURN QDECL Com_Error( errorParm_t level, const char *fmt, ... ) { (void)level; (void)fmt; exit( 3 ); }

typedef struct mnode_s {
    int contents;
    cplane_t *plane;
    struct mnode_s *children[2];
    int cluster;
} mnode_t;
typedef struct { vec3_t bounds[2]; } bmodel_t;
typedef struct {
    mnode_t *nodes;
    bmodel_t *bmodels;
    const byte *vis;
    int numClusters, clusterBytes;
} world_t;
static struct { world_t *world; } tr;
static cvar_t *r_novis;
static void *HostMalloc( int bytes ) { return calloc( 1, (size_t)bytes ); }
static void HostFree( void *p ) { free( p ); }
static void HostPrintf( int level, const char *fmt, ... ) { (void)level; (void)fmt; }
static struct {
    void *( *Malloc )( int );
    void ( *Free )( void * );
    void ( *Printf )( int, const char *, ... );
} ri = { HostMalloc, HostFree, HostPrintf };
#define RT_HostAlloc( bytes ) malloc( (size_t)( bytes ) )
#define RT_HostFree( block ) free( block )
'''
    source += '\n'.join(defines) + '\n'
    source += between(lights, 'typedef struct {\n\tchar\tclassname[64];', '} rtLightEntity_t;')
    source += between(lights, 'typedef struct {\n\trtGpuLight_t\t*lights;', '} rtLightScene_t;')
    source += 'static rtLightScene_t s_rtLights;\n'
    for signature in (
        'static qboolean RT_FloatValid( float v, float limit )',
        'static const mnode_t *RT_PointInLeaf( const vec3_t p )',
        'static int RT_PointCluster( const vec3_t p )',
        'static qboolean RT_ClusterSees( int from, int to )',
        'static float RT_BoxDistanceSquared( const vec3_t p, const vec3_t mins, const vec3_t maxs )',
        'static float RT_LightImportance( const rtGpuLight_t *light, float d2 )',
    ):
        source += function(lights, signature)
    source += between(lights, 'typedef struct {\n\tfloat importance;', '} rtCellCandidate_t;')
    for signature in (
        'static int RT_CompareCandidates( const void *a, const void *b )',
        'static qboolean RT_LightCellRange( uint32_t l, float cellSize, int lo[3], int hi[3] )',
        'static uint32_t *RT_ListCellLights( uint32_t numCells, float cellSize, uint32_t **outStart )',
        'static qboolean RT_Vec3Valid( const vec3_t v, float limit )',
        'static qboolean RT_BuildLightGrid( void )',
    ):
        source += function(lights, signature)
    return source + GRID_CHECKS


GRID_CHECKS = r'''
static uint32_t seed = 12345u;
static float Random( float lo, float hi ) {
    seed = seed * 1664525u + 1013904223u;
    return lo + ( hi - lo ) * (float)( seed >> 8 ) / 16777216.0f;
}
static uint64_t Hash( uint64_t h, const void *data, size_t bytes ) {
    const unsigned char *p = data;
    for ( size_t i = 0; i < bytes; i++ ) {
        h = ( h ^ p[i] ) * 1099511628211ull;
    }
    return h;
}

int main( void ) {
    /* two clusters split at x = 0, each seeing only itself */
    static cplane_t split = { { 1, 0, 0 }, 0 };
    static mnode_t leaves[2], root;
    static const byte vis[2] = { 1, 2 };
    static bmodel_t model = { { { -3000, -2000, -500 }, { 3000, 2000, 700 } } };
    static world_t world;
    const uint32_t count = 3000;
    uint32_t *start = NULL, *listed;
    uint64_t h = 1469598103934665603ull;

    leaves[0].cluster = 0;
    leaves[1].cluster = 1;
    root.contents = -1;
    root.plane = &split;
    root.children[0] = &leaves[1];
    root.children[1] = &leaves[0];
    world.nodes = &root;
    world.bmodels = &model;
    world.vis = vis;
    world.numClusters = 2;
    world.clusterBytes = 1;
    tr.world = &world;

    s_rtLights.lights = calloc( count, sizeof( rtGpuLight_t ) );
    s_rtLights.reach = calloc( count, sizeof( float ) );
    s_rtLights.center = calloc( count, sizeof( vec3_t ) );
    s_rtLights.cluster = calloc( count, sizeof( int ) );
    for ( uint32_t l = 0; l < count; l++ ) {
        rtGpuLight_t *light = &s_rtLights.lights[l];
        const uint32_t type = l % 3 == 0 ? RT_LIGHT_POINT : l % 3 == 1 ? RT_LIGHT_POINT_LINEAR : RT_LIGHT_TRIANGLE;
        s_rtLights.center[l][0] = Random( -3500, 3500 );
        s_rtLights.center[l][1] = Random( -2500, 2500 );
        s_rtLights.center[l][2] = Random( -800, 1000 );
        s_rtLights.reach[l] = l % 149 == 0 ? Random( 4000, 65536 ) : Random( 16, 1000 );
        s_rtLights.cluster[l] = RT_PointCluster( s_rtLights.center[l] );
        light->color[0] = Random( 0, 4 );
        light->color[1] = Random( 0, 4 );
        light->color[2] = Random( 0, 4 );
        light->color[3] = (float)type;
        light->dir[3] = Random( 1, 400 );
        light->extra[1] = Random( 1, 5000 );
        light->extra[2] = Random( 0.5f, 2 );
    }
    /* a reach that is not a finite number reaches every cell in both builds */
    s_rtLights.reach[5] = NAN;
    s_rtLights.reach[6] = INFINITY;
    s_rtLights.numLights = count;

    if ( !RT_BuildLightGrid() ) {
        return 4;
    }
    const uint32_t cells = (uint32_t)( s_rtLights.dims[0] * s_rtLights.dims[1] * s_rtLights.dims[2] );
    float previous = 0.0f;
    for ( uint32_t l = 0; l < count; l++ ) {
        floatint_t cdf, pdf;
        cdf.u = s_rtLights.cells[(cells+l)*2];
        pdf.u = s_rtLights.cells[(cells+l)*2+1];
        if ( !( cdf.f > previous ) || pdf.f != cdf.f-previous ) return 7;
        previous = cdf.f;
    }
    if ( previous != 1.0f || s_rtLights.overflowCells == 0 || s_rtLights.overflowEntries == 0 ||
         !( s_rtLights.overflowImportance > 0 && s_rtLights.candidateImportance > s_rtLights.overflowImportance ) ) return 8;
    /* which way the build found each cell's candidates */
    listed = RT_ListCellLights( cells, s_rtLights.cellSize, &start );
    printf( "listed %d\n", listed != NULL );
    free( listed );
    free( start );
    h = Hash( h, s_rtLights.dims, sizeof( s_rtLights.dims ) );
    h = Hash( h, &s_rtLights.cellSize, sizeof( s_rtLights.cellSize ) );
    h = Hash( h, s_rtLights.cells, cells * 2 * sizeof( uint32_t ) );
    h = Hash( h, s_rtLights.indices, s_rtLights.numIndices * sizeof( uint32_t ) );
    uint32_t full = 0;
    for ( uint32_t c = 0; c < cells; c++ ) {
        full += s_rtLights.cells[c * 2 + 1] == RT_LIGHT_CELL_MAX;
    }
    printf( "cells %u entries %u full %u hash %016llx\n", cells, s_rtLights.numIndices, full, (unsigned long long)h );
    return 0;
}
'''


MAP_CHECKS = r'''
static uint32_t seed = 99u;
static float Random( float lo, float hi ) {
    seed = seed * 1664525u + 1013904223u;
    return lo + ( hi - lo ) * (float)( seed >> 8 ) / 16777216.0f;
}

int main( void ) {
    static mnode_t leaf;
    static world_t world;
    enum { LIGHTS = 700, TRACED = 1400, WORLD = 1500, LOOSE = 20 };
    static rtxRtPackedVertex_t vertices[WORLD * 3];
    static uint32_t indices[WORLD * 3];
    static int lightOf[WORLD];
    const rtxRtCpuGeometry_t geometry = { vertices, indices, WORLD * 3, WORLD * 3 };
    const vec3_t radiance = { 1, 2, 3 }, white = { 1, 1, 1 }, origin = { 0, 0, 64 }, up = { 0, 0, 1 };
    uint32_t t, k, matched = 0, generation, *map;

    leaf.contents = 0;
    leaf.cluster = 0;
    world.nodes = &leaf;
    tr.world = &world;

    /* a point light first: only triangle lights are matched */
    RT_AddPointLight( origin, white, 7500.0f, qfalse, qtrue, 1.0f, NULL, 0.0f, 0 );
    assert( s_rtLights.numLights == 1 );

    /* world triangles anywhere; the filter partition after the first TRACED
       repeats the first ones, lights among them */
    for ( t = 0; t < WORLD; t++ ) {
        float *a = vertices[t * 3 + 0].xyz, *b = vertices[t * 3 + 1].xyz, *c = vertices[t * 3 + 2].xyz;
        if ( t < TRACED ) {
            for ( k = 0; k < 3; k++ ) {
                a[k] = Random( -4000, 4000 );
                b[k] = a[k] + Random( -64, 64 );
                c[k] = a[k] + Random( -64, 64 );
            }
        } else {
            memcpy( &vertices[t * 3], &vertices[( t - TRACED ) * 3], 3 * sizeof( vertices[0] ) );
        }
        for ( k = 0; k < 3; k++ ) {
            indices[t * 3 + k] = t * 3 + k;
        }
        lightOf[t] = -1;
    }
    /* every other traced triangle emits, a light made from its corners */
    for ( t = 0; t < 2 * LIGHTS; t += 2 ) {
        const float *a = vertices[t * 3 + 0].xyz, *b = vertices[t * 3 + 1].xyz, *c = vertices[t * 3 + 2].xyz;
        const uint32_t before = s_rtLights.numLights;
        vec3_t e1, e2, n;
        VectorSubtract( b, a, e1 );
        VectorSubtract( c, a, e2 );
        CrossProduct( e1, e2, n );
        VectorNormalize( n );
        RT_AddTriangleLight( a, b, c, n, radiance, 100.0f, qfalse, 0u );
        assert( s_rtLights.numLights == before + 1 );
        lightOf[t] = (int)before;
    }
    /* and lights no world triangle carries */
    for ( t = 0; t < LOOSE; t++ ) {
        vec3_t a, b, c;
        for ( k = 0; k < 3; k++ ) {
            a[k] = Random( 5000, 6000 );
            b[k] = a[k] + Random( 8, 64 );
            c[k] = a[k] + ( k == 0 ? Random( 8, 64 ) : 0.0f );
        }
        RT_AddTriangleLight( a, b, c, up, radiance, 100.0f, qfalse, 0u );
    }
    assert( s_rtLights.triangleLights == LIGHTS + LOOSE );

    generation = s_rtLights.generation;
    map = RT_MapPrimitiveLights( &geometry, TRACED );
    assert( map );
    /* the lights' flags changed: the frame copies them again */
    assert( s_rtLights.generation == generation + 1 );
    for ( t = 0; t < WORLD; t++ ) {
        if ( lightOf[t] >= 0 ) {
            assert( map[t] == (uint32_t)lightOf[t] );
            matched++;
        } else {
            assert( map[t] == ~0u );
        }
    }
    for ( t = 0; t < s_rtLights.numLights; t++ ) {
        const uint32_t word = (uint32_t)s_rtLights.lights[t].color[3];
        const qboolean traced = ( word & RT_LIGHT_TRACED ) ? qtrue : qfalse;
        assert( traced == ( t >= 1 && t <= LIGHTS ) );
        assert( ( word & RT_LIGHT_TYPE_MASK ) == ( t == 0 ? RT_LIGHT_POINT : RT_LIGHT_TRIANGLE ) );
    }
    printf( "matched %u of %u triangle lights\n", matched, s_rtLights.triangleLights );
    RT_HostFree( map );
    return 0;
}
'''


def compile_run_c_output(test, source):
    compiler = shutil.which('clang') or shutil.which('gcc')
    if not compiler:
        test.skipTest('C11 compiler unavailable')
    flags = compiler_target_args(compiler)
    if os.name == 'nt' and not flags:
        flags = ['-DFNTECH3_Q3_ONLY']
    (ROOT / '.tmp').mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='rtx-grid-', dir=ROOT / '.tmp') as temp:
        c, exe = Path(temp) / 'probe.c', Path(temp) / 'probe.exe'
        c.write_text(source, encoding='utf-8')
        built = subprocess.run([compiler, *flags, '-std=c11', '-O2', '-I', str(ROOT), str(c), '-o', str(exe)]
                               + ([] if os.name == 'nt' else ['-lm']),
                               cwd=ROOT, capture_output=True, text=True, timeout=120)
        test.assertEqual(built.returncode, 0, built.stdout + built.stderr)
        ran = subprocess.run([str(exe)], cwd=ROOT, capture_output=True, text=True, timeout=60)
        test.assertEqual(ran.returncode, 0, ran.stdout + ran.stderr)
        return ran.stdout


class RtxLightEntityTests(unittest.TestCase):
    def test_lights_follow_the_maps_compiler_and_lighting_model(self):
        compile_run_c(self, harness() + CHECKS)

    def test_a_reflection_finds_the_light_its_world_triangle_emits_as(self):
        # A glossy reflection ray that meets a surface light weighs it against
        # the light sampler (rt_path.glsl's rt_reflected_light): the world
        # triangle it hit names its light by their shared centroid, only among
        # the triangles reflections meet, and only those lights are weighed.
        lights = (ROOT / 'code/rendererrtx/rt/rt_lights.c').read_text(encoding='utf-8')
        source = harness()
        for signature in (
            'static void RT_TriangleCentroid(',
            'static void RT_AddTriangleLight(',
            'static uint32_t RT_CentroidHash(',
            'static uint32_t *RT_MapPrimitiveLights(',
        ):
            source += function(lights, signature)
        output = compile_run_c_output(self, source + MAP_CHECKS)
        self.assertIn('matched 700 of 720 triangle lights', output)

    def test_the_light_grid_from_each_lights_reach_matches_a_scan_of_every_light(self):
        # A cell lists the lights whose reach may touch it (RT_ListCellLights)
        # rather than testing every light in the map; candidates still take
        # the exact test in light order, so the grid is the same, bit for bit.
        listed = compile_run_c_output(self, grid_harness(True))
        scanned = compile_run_c_output(self, grid_harness(False))
        self.assertIn('listed 1', listed)
        self.assertIn('listed 0', scanned)
        grid = listed.splitlines()[-1]
        self.assertEqual(grid, scanned.splitlines()[-1])
        cells, entries, full = (int(v) for v in re.search(r'cells (\d+) entries (\d+) full (\d+)', grid).groups())
        # cells of every kind: short lists, and full ones that had to be sorted
        self.assertGreater(entries, cells)
        self.assertGreater(full, 0)
        self.assertLess(full, cells)


if __name__ == '__main__':
    unittest.main()
