"""FnQ shadows for every model format, and a sun for the games that name
theirs in their entities.

The renderers' point-light shadows and cascaded sun shadows drew only Quake
III's and Return to Castle Wolfenstein's model formats: Enemy Territory's
skeletal meshes and the TIKI models of Alice, F.A.K.K.2 and Medal of Honor
never cast, and no Wolfenstein or TIKI model cast or received the sun. Those
three games had no sun at all for the cascades, which take theirs from a sky
shader's `q3map_sun`; theirs is in their entities. See
docs/fntech3/2026-10-22-fnq-shadow-formats.md.

Here:

* every shadow list in the three renderers, the shared front end and each
  back end's four, admits the same formats;
* each format's surface adder takes a caster context, and the shared caster
  collection reaches it, past the sky and portal entities a view would not
  draw;
* the sun shadows' cache draws a skeletal pose again, since its signature
  reads no pose;
* the back ends' allow-lists, compiled from each renderer;
* the shared entity-sun reader (code/renderercommon/tr_entity_sun.h),
  compiled with the production Ritual key reader, on retail keys, malformed
  ones and other profiles;
* when legitimate installations are present, every installed Alice,
  F.A.K.K.2 and Medal of Honor map's entities through the production readers.
  Set FNTECH3_ALICE_ROOT, FNTECH3_FAKK2_ROOT and FNTECH3_MOHAA_ROOT to point at
  other copies; map bytes are piped to the host and nothing retail is written.
"""
import math
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess
import tempfile
import unittest
import zipfile

from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]
RENDERERS = ('renderer', 'renderervk', 'rendererrtx')
ALICE_ROOT = Path(os.environ.get('FNTECH3_ALICE_ROOT', r'E:\Games\Alice'))
FAKK2_ROOT = Path(os.environ.get('FNTECH3_FAKK2_ROOT', str(ROOT / '.tmp' / 'fakk2-install' / 'game')))
MOHAA_ROOT = Path(os.environ.get('FNTECH3_MOHAA_ROOT', r'E:\Games\Medal of Honor Allied Assault'))

MODELS = {'MOD_BRUSH', 'MOD_MESH', 'MOD_MDS', 'MOD_MDC', 'MOD_MDR', 'MOD_IQM',
          'MOD_WOLFET_MESH', 'MOD_ALICE_TIKI', 'MOD_MOHAA_TIKI'}
SURFACES = {'SF_FACE', 'SF_GRID', 'SF_TRIANGLES', 'SF_MDS', 'SF_MDC', 'SF_MD3', 'SF_MDR',
            'SF_IQM', 'SF_WOLFET', 'SF_ALICE_TIKI', 'SF_MOHAA_TIKI'}


def read(path):
    return (ROOT / path).read_text(encoding='utf-8')


def function(text, signature):
    start = text.index(signature)
    return text[start:text.index('\n}\n', start) + 3]


def sun_reader():
    """tr_entity_sun.h with the units header it includes beside it."""
    reader = read('code/renderercommon/tr_entity_sun.h')
    return reader.replace('#include "tr_sky_sun_units.h"\n',
                          read('code/renderercommon/tr_sky_sun_units.h') + '\n')


def cases(body):
    return set(re.findall(r'case (\w+):', body))


def compile_and_run(test, source, stdin=None, prefix='entity-sun-'):
    compiler = shutil.which('clang++') or shutil.which('g++')
    if not compiler:
        test.skipTest('C++17 compiler required')
    (ROOT / '.tmp').mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix=prefix, dir=ROOT / '.tmp') as temp:
        cpp, exe = Path(temp) / 'probe.cpp', Path(temp) / 'probe.exe'
        cpp.write_text(source, encoding='utf-8')
        built = subprocess.run([compiler, *compiler_target_args(compiler), '-std=c++17', '-O2', '-I', str(ROOT),
                                str(cpp), '-o', str(exe)], cwd=ROOT, capture_output=True, text=True, timeout=120)
        test.assertEqual(built.returncode, 0, built.stdout + built.stderr)
        run = subprocess.run([str(exe)], cwd=ROOT, input=stdin, capture_output=True, timeout=120)
        test.assertEqual(run.returncode, 0, (run.stdout + run.stderr).decode('utf-8', 'replace'))
        return run.stdout.decode('utf-8')


class ShadowFormatListTests(unittest.TestCase):
    def test_every_shadow_list_admits_the_same_formats(self):
        common = read('code/renderercommon/tr_dlight_shadow_casters.h')
        self.assertEqual(cases(function(common, 'qboolean R_DlightShadowFrontendCasterAllowed(')),
                         SURFACES)
        collect = function(common, 'static qboolean R_CollectDlightShadowEntityCasters(')
        filter_switch = collect[collect.index('switch ( model->type )'):collect.index('tr.currentEntityNum = entityNum;')]
        self.assertEqual(cases(filter_switch), MODELS)
        call_switch = collect[collect.index('tr.currentEntityNum = entityNum;'):]
        self.assertEqual(cases(call_switch), MODELS)
        for renderer in RENDERERS:
            backend = read(f'code/{renderer}/tr_backend.c')
            with self.subTest(renderer=renderer):
                self.assertEqual(cases(function(backend, 'static qboolean RB_DlightShadowCasterAllowed(')), SURFACES)
                self.assertEqual(cases(function(backend, 'static qboolean RB_CSMShadowSurfaceAllowed(')), SURFACES)
                self.assertEqual(cases(function(backend, 'static qboolean RB_DlightShadowEntityCasterAllowed(')),
                                 MODELS)
                self.assertEqual(cases(function(backend, 'static qboolean RB_CSMShadowEntityAllowed(')), MODELS)

    def test_each_format_adder_takes_the_caster_context(self):
        common = read('code/renderercommon/tr_dlight_shadow_casters.h')
        collect = function(common, 'static qboolean R_CollectDlightShadowEntityCasters(')
        for call in ('R_AddMD3Surfaces( ent, context );', 'R_MDRAddAnimSurfaces( ent, context );',
                     'R_AddIQMSurfaces( ent, context );', 'R_AddBrushModelSurfaces( ent, context );',
                     'R_AddWolfETSurfaces( ent, context );', 'R_AddAliceTikiSurfaces( ent, context );',
                     'R_AddMOHAATikiSurfaces( ent, context );'):
            self.assertIn(call, collect)
        # an entity the view would not draw casts nothing into it
        self.assertIn('if ( COM_GAME_ID_IS_RITUAL( ri.gameId ) && !R_AliceEntityInView( ent ) ) {', collect)
        self.assertIn('if ( ri.gameId == COM_GAME_MOHAA && !R_MOHAAEntityInView( ent ) ) {', collect)
        self.assertLess(collect.index('R_AliceEntityInView( ent )'), collect.index('switch ( model->type )'))

        wolfet = read('code/renderercommon/tr_wolfet_add_surfaces.h')
        add = function(wolfet, 'void R_AddWolfETSurfaces(')
        caster = add[add.index('if ( casterContext ) {'):add.index('if ( R_WolfETCullModel( ent, bounds ) == CULL_OUT ) {')]
        self.assertIn('R_DlightShadowEntityBoundsCulled( casterContext, ent, bounds[0], bounds[1] )', caster)
        self.assertIn('R_AddDlightShadowEntityCasterSurface( casterContext, &surface->surfaceType,', caster)
        self.assertNotIn('R_CullLocalBox', caster)
        self.assertIn('R_AddDrawSurfFlags( &surface->surfaceType, shader, fogNum, 0, DSF_SHADOW_CASTER_ONLY );', add)
        # the shadow faces draw ET's meshes and Alice's whole
        self.assertIn('if ( view->passFlags & VPF_DLIGHT_SHADOW ) return 1.0f;',
                      read('code/renderercommon/tr_wolfet_surface.h'))

        for renderer in RENDERERS:
            with self.subTest(renderer=renderer):
                header = read(f'code/{renderer}/tr_local.h')
                main = read(f'code/{renderer}/tr_main.c')
                for name in ('R_AddWolfETSurfaces( trRefEntity_t *e, dlightShadowCasterContext_t *',
                             'R_AddAliceTikiSurfaces( trRefEntity_t *ent, dlightShadowCasterContext_t *',
                             'R_AddMOHAATikiSurfaces( trRefEntity_t *ent, dlightShadowCasterContext_t *'):
                    self.assertIn(name, header)
                for call in ('R_AddWolfETSurfaces( ent, NULL );', 'R_AddAliceTikiSurfaces( ent, NULL );',
                             'R_AddMOHAATikiSurfaces( ent, NULL );'):
                    self.assertIn(call, main)
                for path, signature in (('tr_alice.c', 'void R_AddAliceTikiSurfaces('),
                                        ('tr_mohaa.c', 'void R_AddMOHAATikiSurfaces(')):
                    add = function(read(f'code/{renderer}/{path}'), signature)
                    self.assertIn('R_DlightShadowEntityBoundsCulled( casterContext, ent, bounds[0], bounds[1] )', add)
                    self.assertIn('R_AddDlightShadowEntityCasterSurface( casterContext, &surface->surfaceType, '
                                  'shader )', add)
                    self.assertIn('fogNum = casterContext ? 0 :', add)
                    self.assertIn('R_AddDrawSurfFlags( &surface->surfaceType, shader, fogNum, 0, '
                                  'DSF_SHADOW_CASTER_ONLY );', add)
                    # a caster casts before any of the view's own surfaces
                    self.assertLess(add.index('R_AddDlightShadowEntityCasterSurface('),
                                    add.index('R_AddDrawSurf( &surface->surfaceType, shader, fogNum, 0 );'))
                alice = function(read(f'code/{renderer}/tr_alice.c'), 'void R_AddAliceTikiSurfaces(')
                # an entity the view posed keeps its pose; one it did not is posed
                self.assertIn('if ( !casterContext || !ent->e.alice.bones ) {', alice)
                self.assertIn('lod = ( backEnd.viewParms.passFlags & VPF_DLIGHT_SHADOW )',
                              function(read(f'code/{renderer}/tr_alice.c'), 'void RB_SurfaceAliceTiki('))
                mohaa = function(read(f'code/{renderer}/tr_mohaa.c'), 'void R_AddMOHAATikiSurfaces(')
                self.assertLess(mohaa.index('R_MOHAAPose( ent, mod )'), mohaa.index('if ( casterContext ) {\n\t\t\tif ( !R_Add'))
                # a caster's detail is the main view's, never its own
                self.assertLess(mohaa.index('if ( !casterContext ) {'), mohaa.index('LodMetric('))

    def test_the_sun_cache_draws_skeletal_poses_again(self):
        for renderer in RENDERERS:
            backend = read(f'code/{renderer}/tr_backend.c')
            with self.subTest(renderer=renderer):
                unhashed = function(backend, 'static qboolean RB_CSMShadowEntityPoseUnhashed(')
                self.assertEqual(cases(unhashed), {'MOD_MDS', 'MOD_WOLFET_MESH', 'MOD_ALICE_TIKI', 'MOD_MOHAA_TIKI'})
                signature = function(backend, 'static qboolean RB_CSMShadowCacheSignature(')
                call = signature.index('RB_CSMShadowEntityPoseUnhashed( &backEnd.refdef.entities[entityNum] )')
                self.assertLess(signature.index('if ( shader->numDeforms > 0 ) {'), call)
                self.assertLess(call, signature.index('hash = RB_DlightShadowCacheHashUInt( hash, drawSurf->sort );'))


BACKEND_PRELUDE = r'''
#include <cassert>
#include <initializer_list>
typedef int qboolean;
enum { qfalse, qtrue };
enum surfaceType_t { SF_BAD, SF_SKIP, SF_FACE, SF_GRID, SF_TRIANGLES, SF_POLY, SF_MD3, SF_MDR, SF_IQM, SF_FLARE,
    SF_ENTITY, SF_MDC, SF_MDS, SF_WOLFET, SF_WOLFET_FOLIAGE, SF_WOLFET_DECAL, SF_IHV_PARTICLES, SF_ALICE_TIKI,
    SF_ALICE_SWIPE, SF_MOHAA_TIKI };
enum { MOD_BAD, MOD_BRUSH, MOD_MESH, MOD_MDR, MOD_IQM, MOD_MDC, MOD_MDS, MOD_WOLFET_MESH, MOD_WOLFET_SKELETON,
    MOD_IHV_BEAM, MOD_ALICE_TIKI, MOD_ALICE_SPRITE, MOD_MOHAA_TIKI };
enum { SS_OPAQUE = 3, GLS_ATEST_BITS = 0x70000000, SURF_SKY = 0x4, SURF_NODLIGHT = 0x20000,
    RT_MODEL = 0, RT_SPRITE = 2, REFENTITYNUM_WORLD = 1023, RF_NOSHADOW = 0x40, RF_FIRST_PERSON = 0x4,
    RF_DEPTHHACK = 0x8, RF_THIRD_PERSON = 0x2 };
struct cvar_t { int integer; };
struct shaderStage_t { unsigned stateBits; };
struct shader_t { shader_t *remappedShader; int sort, isSky, polygonOffset, numDeforms, surfaceFlags,
    numUnfoggedPasses; shaderStage_t *stages[2]; };
static cvar_t curves{}, correctness{};
static cvar_t *r_nocurves = &curves, *r_shadowCorrectness = &correctness;
struct model_t { int type; } model;
struct trRefEntity_t { struct { int reType, renderfx, hModel; } e; } entity;
static struct { struct { int num_entities; trRefEntity_t *entities; } refdef; } backEnd;
static const model_t *R_GetModelByHandle( int handle ) { return handle == 1 ? &model : nullptr; }
static bool runtimeMDR;
static qboolean R_ModelIsRuntimeMDR( int handle ) { return handle == 1 && runtimeMDR; }
'''

BACKEND_CHECKS = r'''
int main() {
    shader_t shader{}; shader.sort = SS_OPAQUE;
    shader_t remap{}; remap.sort=SS_OPAQUE+1;
    surfaceType_t face=SF_FACE;
    shader.remappedShader=&remap;
    assert(!RB_CSMShadowSurfaceAllowed(&shader,&face));
    remap.sort=SS_OPAQUE;
    assert(RB_CSMShadowSurfaceAllowed(&shader,&face));
    shader.remappedShader=nullptr;
    for ( surfaceType_t type : { SF_FACE, SF_GRID, SF_TRIANGLES, SF_MDS, SF_MDC, SF_MD3, SF_MDR, SF_IQM, SF_WOLFET,
            SF_ALICE_TIKI, SF_MOHAA_TIKI } ) {
        assert( RB_DlightShadowCasterAllowed( &shader, &type ) && RB_CSMShadowSurfaceAllowed( &shader, &type ) );
        shader.sort = SS_OPAQUE + 1;
        assert( !RB_DlightShadowCasterAllowed( &shader, &type ) && !RB_CSMShadowSurfaceAllowed( &shader, &type ) );
        shader.sort = SS_OPAQUE;
    }
    for ( surfaceType_t type : { SF_POLY, SF_FLARE, SF_ENTITY, SF_WOLFET_FOLIAGE, SF_WOLFET_DECAL, SF_IHV_PARTICLES,
            SF_ALICE_SWIPE } ) {
        assert( !RB_DlightShadowCasterAllowed( &shader, &type ) && !RB_CSMShadowSurfaceAllowed( &shader, &type ) );
    }
    entity.e.reType = RT_MODEL; entity.e.hModel = 1;
    backEnd.refdef.num_entities = 1; backEnd.refdef.entities = &entity;
    for ( int kind : { MOD_BRUSH, MOD_MESH, MOD_MDS, MOD_MDC, MOD_MDR, MOD_IQM, MOD_WOLFET_MESH, MOD_ALICE_TIKI,
            MOD_MOHAA_TIKI } ) {
        model.type = kind;
        assert( RB_DlightShadowEntityCasterAllowed( 0 ) && RB_CSMShadowEntityAllowed( 0 ) );
        /* a view weapon casts neither shadow */
        entity.e.renderfx = RF_DEPTHHACK;
        assert( !RB_DlightShadowEntityCasterAllowed( 0 ) && !RB_CSMShadowEntityAllowed( 0 ) );
        entity.e.renderfx = RF_NOSHADOW;
        assert( !RB_DlightShadowEntityCasterAllowed( 0 ) && !RB_CSMShadowEntityAllowed( 0 ) );
        entity.e.renderfx = 0;
        const bool skeletal = kind == MOD_MDS || kind == MOD_WOLFET_MESH || kind == MOD_ALICE_TIKI ||
            kind == MOD_MOHAA_TIKI;
        assert( !!RB_CSMShadowEntityPoseUnhashed( &entity ) == skeletal );
        if ( kind == MOD_MDR ) {
            runtimeMDR = true;
            assert( RB_CSMShadowEntityPoseUnhashed( &entity ) );
            runtimeMDR = false;
        }
    }
    for ( int kind : { MOD_BAD, MOD_WOLFET_SKELETON, MOD_IHV_BEAM, MOD_ALICE_SPRITE } ) {
        model.type = kind;
        assert( !RB_DlightShadowEntityCasterAllowed( 0 ) && !RB_CSMShadowEntityAllowed( 0 ) );
        assert( !RB_CSMShadowEntityPoseUnhashed( &entity ) );
    }
    entity.e.hModel = 0;
    assert( !RB_CSMShadowEntityPoseUnhashed( &entity ) );
    assert( RB_DlightShadowEntityCasterAllowed( REFENTITYNUM_WORLD ) && RB_CSMShadowEntityAllowed( REFENTITYNUM_WORLD ) );
    return 0;
}
'''


class ShadowBackendTests(unittest.TestCase):
    def test_each_back_end_admits_every_format_and_redraws_skeletal_poses(self):
        for renderer in RENDERERS:
            backend = read(f'code/{renderer}/tr_backend.c')
            body = ''.join(function(backend, name) for name in (
                'static qboolean RB_ShadowShaderHasAlphaTest(',
                'static qboolean RB_ShadowCorrectnessRejectsAlphaTest(',
                'static const shader_t *RB_DlightShadowEffectiveShader(',
                'static qboolean RB_DlightShadowCasterAllowed(',
                'static qboolean RB_DlightShadowEntityCasterAllowed(',
                'static qboolean RB_CSMShadowEntityAllowed(',
                'static qboolean RB_CSMShadowSurfaceAllowed(',
                'static qboolean RB_CSMShadowEntityPoseUnhashed('))
            with self.subTest(renderer=renderer):
                compile_and_run(self, BACKEND_PRELUDE + body + BACKEND_CHECKS, prefix='shadow-formats-')


SUN_PRELUDE = r'''
#include <cassert>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include "code/renderercommon/tr_ritual_model_light.h"
typedef int qboolean;
enum { qfalse, qtrue };
typedef float vec3_t[3];
#define MAX_QPATH 64
#define MAX( a, b ) ( ( a ) > ( b ) ? ( a ) : ( b ) )
enum { COM_GAME_QUAKE3, COM_GAME_ALICE, COM_GAME_FAKK2, COM_GAME_MOHAA, COM_GAME_QUAKELIVE, COM_GAME_RTCW_SP,
    COM_GAME_RTCW_MP, COM_GAME_WOLFET };
#define ID_INLINE inline
#define ARRAY_LEN( x ) ( sizeof( x ) / sizeof( *( x ) ) )
static int Q_stricmpn( const char *a, const char *b, int n ) {
    for ( ; n > 0; a++, b++, n-- ) {
        const int x = std::tolower( (unsigned char)*a ), y = std::tolower( (unsigned char)*b );
        if ( x != y ) return x < y ? -1 : 1;
        if ( !x ) return 0;
    }
    return 0;
}
static int Q_stricmp( const char *a, const char *b ) { return Q_stricmpn( a, b, 1 << 30 ); }
#define COM_GAME_ID_IS_RITUAL( id ) ( ( id ) == COM_GAME_ALICE || ( id ) == COM_GAME_FAKK2 )
static void Com_Memset( void *p, int v, size_t n ) { std::memset( p, v, n ); }
static void Com_Memcpy( void *d, const void *s, size_t n ) { std::memcpy( d, s, n ); }
static float Com_Clamp( float lo, float hi, float v ) { return v < lo ? lo : v > hi ? hi : v; }
static void Q_strncpyz( char *d, const char *s, int n ) { std::strncpy( d, s, n - 1 ); d[n - 1] = 0; }
#define VectorCopy( a, b ) ( ( b )[0] = ( a )[0], ( b )[1] = ( a )[1], ( b )[2] = ( a )[2] )
#define VectorScale( v, s, o ) ( ( o )[0] = ( v )[0] * ( s ), ( o )[1] = ( v )[1] * ( s ), ( o )[2] = ( v )[2] * ( s ) )
#define VectorLengthSquared( v ) ( ( v )[0] * ( v )[0] + ( v )[1] * ( v )[1] + ( v )[2] * ( v )[2] )
static float VectorNormalize2( const vec3_t v, vec3_t out ) {
    const float length = std::sqrt( VectorLengthSquared( v ) );
    if ( length > 0.0f ) { out[0] = v[0] / length; out[1] = v[1] / length; out[2] = v[2] / length; }
    else { out[0] = out[1] = out[2] = 0.0f; }
    return length;
}
struct worldSun_t { qboolean valid; char shaderName[MAX_QPATH]; vec3_t color, directionToSun, lightDirection, light;
    float intensity; qboolean unitColour; };
static struct { worldSun_t worldSun; vec3_t sunLight, sunColor, sunDirection; float sunIntensity;
    qboolean sunParmsValid; } tr;
static struct { char *entityString; } s_worldData;
static struct { int gameId; } ri;
static bool mohaaSun; static vec3_t mohaaLight, mohaaToward;
static qboolean R_MOHAAWorldSun( vec3_t light, vec3_t towardSun ) {
    if ( !mohaaSun ) return qfalse;
    VectorCopy( mohaaLight, light ); VectorCopy( mohaaToward, towardSun ); return qtrue;
}
'''

SUN_CHECKS = r'''
static bool Near( float a, float b ) { return std::fabs( a - b ) < 1e-4f; }
static void Forward( float pitch, float yaw, vec3_t out ) {
    const double p = pitch * 3.14159265358979323846 / 180.0, y = yaw * 3.14159265358979323846 / 180.0;
    out[0] = (float)( std::cos( p ) * std::cos( y ) ); out[1] = (float)( std::cos( p ) * std::sin( y ) );
    out[2] = (float)( -std::sin( p ) );
}
/* a sky shader's sun, which an entity sun replaces, and the sky's own
 * direction, which it never touches */
static void SkySun() {
    std::memset( &tr, 0, sizeof( tr ) );
    tr.worldSun.valid = qtrue; Q_strncpyz( tr.worldSun.shaderName, "textures/sky/q3", MAX_QPATH );
    tr.worldSun.intensity = 100.0f; tr.sunIntensity = 100.0f; tr.sunParmsValid = qtrue;
    tr.sunDirection[0] = 0.25f; tr.sunDirection[1] = 0.5f; tr.sunDirection[2] = 0.75f;
}
static bool SkySunKept() {
    return tr.worldSun.valid && !std::strcmp( tr.worldSun.shaderName, "textures/sky/q3" ) && tr.sunIntensity == 100.0f;
}
static bool SkyDirectionKept() {
    return tr.sunDirection[0] == 0.25f && tr.sunDirection[1] == 0.5f && tr.sunDirection[2] == 0.75f;
}
static void Load( int game, const char *entities ) {
    static std::string text;
    SkySun();
    ri.gameId = game;
    text = entities ? entities : "";
    s_worldData.entityString = entities ? &text[0] : nullptr;
    R_LoadEntitySun();
}
static void ExpectSun( const char *source, float r, float g, float b, float intensity, float pitch, float yaw ) {
    vec3_t toward;
    Forward( pitch, yaw, toward );
    assert( tr.worldSun.valid && !std::strcmp( tr.worldSun.shaderName, source ) );
    assert( Near( tr.worldSun.color[0], r ) && Near( tr.worldSun.color[1], g ) && Near( tr.worldSun.color[2], b ) );
    assert( Near( tr.worldSun.intensity, intensity ) && Near( tr.sunIntensity, intensity ) && tr.sunParmsValid );
    for ( int i = 0; i < 3; i++ ) {
        assert( Near( tr.worldSun.directionToSun[i], toward[i] ) );
        assert( Near( tr.worldSun.lightDirection[i], -toward[i] ) );
        assert( tr.sunColor[i] == tr.worldSun.color[i] && tr.sunLight[i] == tr.worldSun.light[i] );
    }
    /* the planner's own test (R_PlanCascadedShadows) */
    assert( tr.worldSun.valid && tr.sunParmsValid && tr.sunIntensity > 0.0f );
    assert( SkyDirectionKept() );
}
int main() {
    /* Alice's grounds1 and F.A.K.K.2's fakkhouse, as their worldspawns give them */
    Load( COM_GAME_ALICE, "{\n\"classname\" \"worldspawn\"\n\"suncolor\" \"224 192 192\"\n"
        "\"sundirection\" \"-45 -26.9 0\"\n\"ambientlight\" \"32 8 8\"\n}\n" );
    ExpectSun( "entities", 1.0f, 192.0f / 224.0f, 192.0f / 224.0f, 224.0f, -45.0f, -26.9f );
    assert( tr.worldSun.directionToSun[2] > 0.7f );
    Load( COM_GAME_FAKK2, "{\n\"ambientlight\" \"4 4 3\"\n\"suncolor\" \"56 56 50\"\n\"sundirection\" \"-65.0 22.5 0\"\n"
        "\"sunflare\" \"-952 72 -1280\"\n\"classname\" \"worldspawn\"\n}\n" );
    ExpectSun( "entities", 1.0f, 1.0f, 50.0f / 56.0f, 56.0f, -65.0f, 22.5f );
    /* retail reads the keys from anywhere in the text, the last winning */
    Load( COM_GAME_FAKK2, "{\n\"classname\" \"worldspawn\"\n\"suncolor\" \"56 56 50\"\n\"sundirection\" \"-65 22.5 0\"\n}\n"
        "{\n\"classname\" \"info_null\"\n\"suncolor\" \"10 20 40\"\n}\n" );
    ExpectSun( "entities", 0.25f, 0.5f, 1.0f, 40.0f, -65.0f, 22.5f );
    /* no direction, no colour, nothing or no text: the sky's sun stands */
    Load( COM_GAME_ALICE, "{\n\"classname\" \"worldspawn\"\n\"suncolor\" \"224 192 192\"\n}\n" );
    assert( SkySunKept() && SkyDirectionKept() );
    Load( COM_GAME_FAKK2, "{\n\"classname\" \"worldspawn\"\n\"sundirection\" \"-65 22.5 0\"\n}\n" );
    assert( SkySunKept() );
    Load( COM_GAME_FAKK2, "{\n\"classname\" \"worldspawn\"\n\"suncolor\" \"0 0 0\"\n\"sundirection\" \"-65 22.5 0\"\n}\n" );
    assert( SkySunKept() );
    Load( COM_GAME_FAKK2, "{\n\"classname\" \"worldspawn\"\n\"sunflare\" \"648 6056 712\"\n}\n" );
    assert( SkySunKept() );
    Load( COM_GAME_ALICE, nullptr );
    assert( SkySunKept() );
    /* a sun at or below the horizon (F.A.K.K.2's otto) casts nothing */
    Load( COM_GAME_FAKK2, "{\n\"suncolor\" \"32 33 35\"\n\"sundirection\" \"65.0 -22.5 0.0\"\n}\n" );
    assert( SkySunKept() );
    Load( COM_GAME_FAKK2, "{\n\"suncolor\" \"32 33 35\"\n\"sundirection\" \"0 45 0\"\n}\n" );
    assert( SkySunKept() );
    /* a negative channel is no colour of its own */
    Load( COM_GAME_ALICE, "{\n\"suncolor\" \"-50 100 100\"\n\"sundirection\" \"-90 0 0\"\n}\n" );
    ExpectSun( "entities", 0.0f, 1.0f, 1.0f, 100.0f, -90.0f, 0.0f );
    /* Medal of Honor's comes from its worldspawn reader */
    mohaaSun = true;
    mohaaLight[0] = mohaaLight[1] = mohaaLight[2] = 85.0f;
    Forward( -60.0f, 30.0f, mohaaToward );
    Load( COM_GAME_MOHAA, "{\n\"suncolor\" \"1 1 1\"\n\"sundirection\" \"0 0 0\"\n}\n" );
    ExpectSun( "worldspawn", 1.0f, 1.0f, 1.0f, 85.0f, -60.0f, 30.0f );
    mohaaToward[0] = mohaaToward[1] = mohaaToward[2] = 0.0f;
    Load( COM_GAME_MOHAA, "" );
    assert( SkySunKept() );
    mohaaToward[2] = NAN;
    Load( COM_GAME_MOHAA, "" );
    assert( SkySunKept() );
    Forward( -60.0f, 30.0f, mohaaToward );
    mohaaLight[1] = INFINITY;
    Load( COM_GAME_MOHAA, "" );
    assert( SkySunKept() );
    mohaaSun = false;
    Load( COM_GAME_MOHAA, "" );
    assert( SkySunKept() );
    /* every other profile keeps its sky's sun, entity keys or none */
    mohaaSun = true; mohaaLight[1] = 85.0f;
    Load( COM_GAME_QUAKE3, "{\n\"suncolor\" \"224 192 192\"\n\"sundirection\" \"-45 -26.9 0\"\n}\n" );
    assert( SkySunKept() && SkyDirectionKept() );
    /* the sky sun's units (tr_sky_sun_units.h): id's q3map at unit length,
     * q3map2's and Wolfenstein's at a brightest channel of 1 */
    assert( tr.worldSun.unitColour );
    Load( COM_GAME_QUAKE3, "{\n\"classname\" \"worldspawn\"\n\"_q3map2_version\" \"2.5.17n\"\n}\n" );
    assert( SkySunKept() && !tr.worldSun.unitColour );
    Load( COM_GAME_RTCW_SP, "{\n\"classname\" \"worldspawn\"\n}\n" );
    assert( SkySunKept() && !tr.worldSun.unitColour );
    Load( COM_GAME_QUAKE3, nullptr );
    assert( SkySunKept() && tr.worldSun.unitColour );
    /* an entity sun is in lightmap units already */
    Load( COM_GAME_ALICE, "{\n\"classname\" \"worldspawn\"\n\"suncolor\" \"224 192 192\"\n"
        "\"sundirection\" \"-45 -26.9 0\"\n}\n" );
    assert( !SkySunKept() && !tr.worldSun.unitColour );
    return 0;
}
'''


class EntitySunTests(unittest.TestCase):
    def test_the_reader_takes_each_games_own_keys(self):
        compile_and_run(self, SUN_PRELUDE + sun_reader() + SUN_CHECKS)

    def test_every_renderer_loads_the_sun_after_its_entities(self):
        for renderer in RENDERERS:
            with self.subTest(renderer=renderer):
                bsp = read(f'code/{renderer}/tr_bsp.c')
                # after s_worldData, as the Ritual light loader is
                self.assertLess(bsp.index('#include "../renderercommon/tr_ritual_model_light_bsp.h"'),
                                bsp.index('#include "../renderercommon/tr_entity_sun.h"'))
                load = function(bsp, 'void RE_LoadWorldMap( const char *name ) {')
                self.assertIn('\tR_LoadEntities( &header->lumps[LUMP_ENTITIES] );\n\tR_LoadEntitySun();\n', load)
                self.assertLess(load.index('R_ClearWorldSun();'), load.index('R_LoadEntitySun();'))
                self.assertLess(load.index('R_LoadShaders( &header->lumps[LUMP_SHADERS] );'),
                                load.index('R_LoadEntitySun();'))
                self.assertIn('qboolean R_MOHAAWorldSun( vec3_t light, vec3_t towardSun );',
                              read(f'code/{renderer}/tr_local.h'))
                # the planner still reads the sun it always read
                planner = read(f'code/{renderer}/tr_main.c')
                planner = planner[planner.index('static void R_PlanCascadedShadows( void )'):]
                self.assertIn('if ( !tr.worldSun.valid || !tr.sunParmsValid || tr.sunIntensity <= 0.0f ) {',
                              planner[:planner.index('\n}\n')])


RT_PRELUDE = r'''
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include "code/renderercommon/tr_ritual_model_light.h"
typedef int qboolean;
enum { qfalse, qtrue };
typedef float vec3_t[3];
typedef union { float f; uint32_t u; } floatint_t;
#define MAX( a, b ) ( ( a ) > ( b ) ? ( a ) : ( b ) )
enum { COM_GAME_QUAKE3, COM_GAME_ALICE, COM_GAME_FAKK2, COM_GAME_MOHAA };
#define COM_GAME_ID_IS_RITUAL( id ) ( ( id ) == COM_GAME_ALICE || ( id ) == COM_GAME_FAKK2 )
static void Com_Memset( void *p, int v, size_t n ) { std::memset( p, v, n ); }
#define VectorCopy( a, b ) ( ( b )[0] = ( a )[0], ( b )[1] = ( a )[1], ( b )[2] = ( a )[2] )
#define VectorClear( v ) ( ( v )[0] = ( v )[1] = ( v )[2] = 0.0f )
#define VectorLengthSquared( v ) ( ( v )[0] * ( v )[0] + ( v )[1] * ( v )[1] + ( v )[2] * ( v )[2] )
static float VectorNormalize2( const vec3_t v, vec3_t out ) {
    const float length = std::sqrt( VectorLengthSquared( v ) );
    if ( length > 0.0f ) { out[0] = v[0] / length; out[1] = v[1] / length; out[2] = v[2] / length; }
    else { out[0] = out[1] = out[2] = 0.0f; }
    return length;
}
struct world_t { char *entityString; };
static struct { world_t *world; } tr;
static struct { int gameId; } ri;
static struct { qboolean sunValid, entitySun; vec3_t sunLight, sunDirection; } s_rtLights;
'''

RT_CHECKS = r'''
static bool Near( float a, float b ) { return std::fabs( a - b ) < 1e-4f; }
static bool Find( int game, const char *entities ) {
    static std::string text;
    static world_t world;
    std::memset( &s_rtLights, 0, sizeof( s_rtLights ) );
    ri.gameId = game;
    text = entities ? entities : "";
    world.entityString = entities ? &text[0] : nullptr;
    tr.world = &world;
    return RT_FindRitualSun() != qfalse;
}
int main() {
    /* fakkhouse's keys: the sun as given, toward the angles' forward vector */
    assert( Find( COM_GAME_FAKK2, "{\n\"suncolor\" \"56 56 50\"\n\"sundirection\" \"-65.0 22.5 0\"\n}\n" ) );
    assert( s_rtLights.sunValid && s_rtLights.entitySun );
    assert( Near( s_rtLights.sunLight[0], 56.0f ) && Near( s_rtLights.sunLight[2], 50.0f ) );
    const double p = -65.0 * 3.14159265358979323846 / 180.0, y = 22.5 * 3.14159265358979323846 / 180.0;
    assert( Near( s_rtLights.sunDirection[0], (float)( std::cos( p ) * std::cos( y ) ) ) );
    assert( Near( s_rtLights.sunDirection[2], (float)( -std::sin( p ) ) ) );
    assert( Find( COM_GAME_ALICE, "{\n\"suncolor\" \"224 192 192\"\n\"sundirection\" \"-45 -26.9 0\"\n}\n" ) );
    /* otto's sun below the horizon, a colour or a direction alone, a black
     * sun, no text, and every other profile leave the sky's sun to RT_FindSun */
    assert( !Find( COM_GAME_FAKK2, "{\n\"suncolor\" \"32 33 35\"\n\"sundirection\" \"65.0 -22.5 0.0\"\n}\n" ) );
    assert( !s_rtLights.sunValid && !s_rtLights.entitySun );
    assert( !Find( COM_GAME_FAKK2, "{\n\"suncolor\" \"56 56 50\"\n}\n" ) );
    assert( !Find( COM_GAME_FAKK2, "{\n\"sundirection\" \"-65 22.5 0\"\n}\n" ) );
    assert( !Find( COM_GAME_FAKK2, "{\n\"suncolor\" \"0 0 0\"\n\"sundirection\" \"-65 22.5 0\"\n}\n" ) );
    assert( !Find( COM_GAME_ALICE, nullptr ) );
    assert( !Find( COM_GAME_QUAKE3, "{\n\"suncolor\" \"56 56 50\"\n\"sundirection\" \"-65 22.5 0\"\n}\n" ) );
    assert( !Find( COM_GAME_MOHAA, "{\n\"suncolor\" \"56 56 50\"\n\"sundirection\" \"-65 22.5 0\"\n}\n" ) );
    /* a negative channel is no light */
    assert( Find( COM_GAME_ALICE, "{\n\"suncolor\" \"-20 100 100\"\n\"sundirection\" \"-90 0 0\"\n}\n" ) );
    assert( s_rtLights.sunLight[0] == 0.0f && Near( s_rtLights.sunDirection[2], 1.0f ) );
    return 0;
}
'''


class RTXRitualSunTests(unittest.TestCase):
    def test_the_path_tracer_takes_the_ritual_entity_sun(self):
        lights = read('code/rendererrtx/rt/rt_lights.c')
        defines = '\n'.join(line for line in lights.splitlines() if line.startswith('#define RT_LIGHT_MAX_'))
        body = '\n'.join((defines, function(lights, 'static qboolean RT_FloatValid( float v, float limit )'),
                          function(lights, 'static qboolean RT_Vec3Valid( const vec3_t v, float limit )'),
                          function(lights, 'static qboolean RT_FindRitualSun( void )')))
        compile_and_run(self, RT_PRELUDE + body + RT_CHECKS, prefix='rtx-ritual-sun-')

    def test_only_the_ritual_profiles_take_it_and_mohaa_keeps_its_own(self):
        lights = read('code/rendererrtx/rt/rt_lights.c')
        build = function(lights, 'static qboolean RT_BuildLights( void )')
        self.assertEqual(build.count('} else if ( !RT_FindRitualSun() ) {\n\t\t\tRT_FindSun();'), 2)
        self.assertNotIn('RitualModelLight', function(lights, 'static void RT_FindSun( void )'))
        self.assertIn('if ( !COM_GAME_ID_IS_RITUAL( ri.gameId ) || !tr.world || !tr.world->entityString ) {',
                      function(lights, 'static qboolean RT_FindRitualSun( void )'))


def pak_maps(roots):
    for root in roots:
        for pak in sorted(root.glob('*.pk3')):
            with zipfile.ZipFile(pak) as archive:
                for info in archive.infolist():
                    if info.filename.lower().endswith('.bsp'):
                        yield info.filename, archive.read(info)


def entity_text(data):
    # FAKK 42/12 and 2015's version 19 both keep three header words before
    # their lumps, the entities fifteenth
    offset, length = struct.unpack_from('<ii', data, 12 + 14 * 8)
    return data[offset:offset + length].split(b'\0', 1)[0]


RITUAL_SWEEP = SUN_PRELUDE + '%READER%' + r'''
#include <iostream>
#include <iterator>
int main() {
    /* one map's entity text a record: its game id, its length, its bytes */
    int game, length;
    while ( std::cin >> game >> length ) {
        std::cin.get();
        std::string text( length, '\0' );
        std::cin.read( &text[0], length );
        std::memset( &tr, 0, sizeof( tr ) );
        ri.gameId = game;
        s_worldData.entityString = &text[0];
        R_LoadEntitySun();
        if ( tr.worldSun.valid ) {
            std::printf( "sun %s %.6f %.6f %.6f %.3f\n", tr.worldSun.shaderName, tr.worldSun.directionToSun[0],
                tr.worldSun.directionToSun[1], tr.worldSun.directionToSun[2], tr.worldSun.intensity );
        } else {
            std::printf( "none\n" );
        }
    }
    return 0;
}
'''


class InstalledMapSunTests(unittest.TestCase):
    def ritual_sweep(self, game, roots):
        maps = list(pak_maps(roots))
        if not maps:
            self.skipTest('installation not present')
        stdin = b''.join(b'%d %d\n' % (game, len(entity_text(data))) + entity_text(data) for _, data in maps)
        out = compile_and_run(self, RITUAL_SWEEP.replace('%READER%', sun_reader()), stdin=stdin).splitlines()
        self.assertEqual(len(out), len(maps))
        result = {}
        for (name, data), line in zip(maps, out):
            text = entity_text(data).decode('latin-1')
            keys = dict(re.findall(r'"(suncolor|sundirection)"\s+"([^"]*)"', text))
            toward = None
            if 'suncolor' in keys and 'sundirection' in keys:
                pitch, yaw = (float(v) for v in keys['sundirection'].split()[:2])
                p, y = math.radians(pitch), math.radians(yaw)
                toward = (math.cos(p) * math.cos(y), math.cos(p) * math.sin(y), -math.sin(p))
            if toward and toward[2] > 0.0:
                fields = line.split()
                self.assertEqual(fields[:2], ['sun', 'entities'], name)
                for got, want in zip(fields[2:5], toward):
                    self.assertAlmostEqual(float(got), want, places=4, msg=name)
                # every one is above the horizon
                self.assertGreater(float(fields[4]), 0.0, name)
                self.assertAlmostEqual(float(fields[5]), max(float(v) for v in keys['suncolor'].split()), places=3)
            else:
                self.assertEqual(line, 'none', name)
            result[name] = line
        return result

    def test_alice_maps(self):
        # 36 maps; grounds1, rchess1, wchess1 and wchess2 name a sun
        result = self.ritual_sweep(1, [ALICE_ROOT / 'base'])
        self.assertEqual(sorted(name for name, line in result.items() if line != 'none'),
                         ['maps/grounds1.bsp', 'maps/rchess1.bsp', 'maps/wchess1.bsp', 'maps/wchess2.bsp'])

    def test_fakk2_maps(self):
        # 20 of 30 maps name a sun, 19 above the horizon: otto's points 65
        # degrees down. intro, oracle and oracleway place only a flare, which
        # is no light.
        result = self.ritual_sweep(2, [FAKK2_ROOT / 'fakk'])
        suns = sorted(name for name, line in result.items() if line != 'none')
        self.assertEqual(len(suns), 19)
        for name in ('maps/otto.bsp', 'maps/intro.bsp', 'maps/oracle.bsp', 'maps/oracleway.bsp'):
            self.assertEqual(result[name], 'none')

    def test_mohaa_maps(self):
        maps = list(pak_maps([MOHAA_ROOT / 'main']))
        if not maps:
            self.skipTest('installation not present')
        from mohaa_sun_flare_tests import sun_keys
        alice = read('code/renderer/tr_alice.c')
        source = r'''
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>
typedef float vec_t;
typedef vec_t vec3_t[3];
typedef int qboolean;
enum { qfalse, qtrue };
#define MAX_QPATH 64
static void Com_Memset( void *p, int v, size_t n ) { std::memset( p, v, n ); }
static void Q_strncpyz( char *d, const char *s, int n ) { std::strncpy( d, s, n - 1 ); d[n - 1] = 0; }
#define VectorSet( v, x, y, z ) ( ( v )[0] = ( x ), ( v )[1] = ( y ), ( v )[2] = ( z ) )
#define VectorCopy( a, b ) ( ( b )[0] = ( a )[0], ( b )[1] = ( a )[1], ( b )[2] = ( a )[2] )
static void AngleVectors( const vec3_t angles, vec3_t forward, vec3_t, vec3_t ) {
	const double p = angles[0] * 3.14159265358979323846 / 180.0, y = angles[1] * 3.14159265358979323846 / 180.0;
	forward[0] = (float)( std::cos( p ) * std::cos( y ) );
	forward[1] = (float)( std::cos( p ) * std::sin( y ) );
	forward[2] = (float)( -std::sin( p ) );
}
void R_MOHAASetSunFlare( const vec3_t, const char * ) {}
''' + sun_keys(alice) + r'''
/* worldspawn's quoted key-value pairs, as R_LoadEntities hands them over */
int main() {
	int length;
	while ( std::cin >> length ) {
		std::cin.get();
		std::string text( length, '\0' );
		std::cin.read( &text[0], length );
		R_MOHAABeginSunKeys();
		size_t at = text.find( '{' ), end = text.find( '}' );
		std::string key, value;
		bool haveKey = false;
		for ( size_t i = at; i < end; i++ ) {
			if ( text[i] != '"' ) continue;
			const size_t close = text.find( '"', i + 1 );
			std::string token = text.substr( i + 1, close - i - 1 );
			i = close;
			if ( !haveKey ) { key = token; haveKey = true; }
			else { R_MOHAASunKey( key.c_str(), token.c_str() ); haveKey = false; }
		}
		R_MOHAAEndSunKeys();
		vec3_t light, toward;
		if ( R_MOHAAWorldSun( light, toward ) ) {
			std::printf( "sun %.6f %.6f %.6f %.3f %.3f %.3f\n", toward[0], toward[1], toward[2], light[0], light[1], light[2] );
		} else {
			std::printf( "none\n" );
		}
	}
	return 0;
}
'''
        stdin = b''.join(b'%d\n' % len(entity_text(data)) + entity_text(data) for _, data in maps)
        out = compile_and_run(self, source, stdin=stdin).splitlines()
        self.assertEqual(len(out), len(maps))
        suns = 0
        for (name, data), line in zip(maps, out):
            text = entity_text(data).decode('latin-1')
            world = text[text.index('{'):text.index('}')]
            direction = re.findall(r'"sundirection"\s+"([^"]*)"', world)
            if direction:
                suns += 1
                pitch, yaw = (float(v) for v in direction[-1].split()[:2])
                p, y = math.radians(pitch), math.radians(yaw)
                toward = (math.cos(p) * math.cos(y), math.cos(p) * math.sin(y), -math.sin(p))
                fields = line.split()
                self.assertEqual(fields[0], 'sun', name)
                for got, want in zip(fields[1:4], toward):
                    self.assertAlmostEqual(float(got), want, places=4, msg=name)
                # above the horizon on every map (the worldspawn-sun record)
                self.assertGreater(float(fields[3]), 0.0, name)
                colours = re.findall(r'"(?:suncolor|sunlight)"\s+"([^"]*)"', world)
                self.assertTrue(colours, name)
                given = colours[-1].split()
                # 70 in each channel for one that is not three numbers
                want = [float(v) for v in given] if len(given) == 3 else [70.0] * 3
                for got, value in zip(fields[4:7], want):
                    self.assertAlmostEqual(float(got), value, places=2, msg=name)
            else:
                self.assertEqual(line, 'none', name)
        # 54 maps, 46 suns (docs/fntech3/2026-10-16-mohaa-worldspawn-sun.md)
        self.assertEqual((len(maps), suns), (54, 46))


if __name__ == '__main__':
    unittest.main()
