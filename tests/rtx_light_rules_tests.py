"""Execute the path tracer's choice of a map's sun (code/rendererrtx/rt).

id's q3map lights a map with the q3map_sun of the first draw surface whose
shader is sky, its colour scaled to unit length; q3map2 with the first shader
that has a sun, its largest colour component scaled to 1, unless worldspawn
sets _noshadersun. q3map2's q3map_sunExt spreads the sun over an angular
deviance in a number of samples, and its q3map_skyLight lights the map with
a dome of suns, once per shader, in the shader's light colour. The real
RT_FindSun, RT_FindSkyLight and RT_ShaderIsSky run against a bounded host.
"""
from pathlib import Path
import unittest

from mesh_tag_tests import function
import rtx_surface_owner_tests

ROOT = Path(__file__).resolve().parents[1]


def harness():
    lights = (ROOT / "code/rendererrtx/rt/rt_lights.c").read_text(encoding="utf-8")
    material = (ROOT / "code/rendererrtx/rt/rt_material.c").read_text(encoding="utf-8")
    math = (ROOT / "code/qcommon/q_math.c").read_text(encoding="utf-8")
    shared = (ROOT / "code/qcommon/q_shared.c").read_text(encoding="utf-8")
    source = r'''
#define _CRT_SECURE_NO_WARNINGS 1
#include "code/qcommon/q_shared.h"
#include "code/qcommon/game_profile.h"
#include "code/qcommon/surfaceflags.h"
#include <assert.h>
#include <math.h>
#include <initializer_list>
#define RT_LIGHT_MAX_COORD 1048576.0f
#define RT_LIGHT_MAX_VALUE 1.0e7f
typedef struct {
    const char *name;
    int surfaceFlags;
    qboolean isSky, rtCompiledSky, rtSunValid;
    vec3_t rtSunColor, rtSunDirection;
    float rtSunIntensity;
    float rtSunDeviance;
    int rtSunSamples;
    float rtSkyLightValue;
    int rtSkyLightIterations;
    qboolean rtEmitColorValid;
    vec3_t rtEmitColor;
} shader_t;
typedef struct { shader_t *shader; } msurface_t;
typedef struct { int numsurfaces; msurface_t *surfaces; } world_t;
static struct { world_t *world; } tr;
static void Printf( int, const char *, ... ) {}
static struct { void ( *Printf )( int, const char *, ... ); int gameId; } ri = { Printf, COM_GAME_QUAKE3 };
static struct {
    qboolean q3map2, noShaderSun, sunValid;
    vec3_t sunLight, sunDirection;
    float sunDeviance;
    int sunSamples;
    vec3_t skyLight;
    int skyLightIterations;
} s_rtLights;
'''
    source += function(math, "vec_t VectorNormalize2( const vec3_t v, vec3_t out)")
    source += function(shared, "int Q_stricmpn( const char *s1, const char *s2, int n ) {")
    source += function(shared, "int Q_stricmp( const char *s1, const char *s2 )")
    source += (ROOT / "code/renderercommon/tr_sky_sun_units.h").read_text(encoding="utf-8")
    source += function(lights, "static qboolean RT_FloatValid( float v, float limit )")
    source += function(lights, "static qboolean RT_Vec3Valid( const vec3_t v, float limit )")
    source += function(material, "static qboolean RT_ShaderIsSky( const shader_t *shader )")
    source += function(lights, "static void RT_FindSun( void )")
    source += function(lights, "static void RT_FindSkyLight( void )")
    return source


CHECKS = r'''
static shader_t shaders[4];
static msurface_t surfaces[4];
static world_t world = { 4, surfaces };

static void Reset( qboolean q3map2 ) {
    memset( shaders, 0, sizeof( shaders ) );
    memset( &s_rtLights, 0, sizeof( s_rtLights ) );
    for ( int i = 0; i < 4; ++i ) surfaces[i].shader = &shaders[i];
    s_rtLights.q3map2 = q3map2;
    tr.world = &world;
    ri.gameId = COM_GAME_QUAKE3;
}
// a shader's q3map_sun as the renderer's parser records it: colour scaled so
// its largest component is 1
static void Sun( shader_t *s, float r, float g, float b, float intensity, float yaw, float elevation ) {
    const float m = fmaxf( r, fmaxf( g, b ) );
    s->rtSunValid = qtrue;
    s->rtSunColor[0] = r / m; s->rtSunColor[1] = g / m; s->rtSunColor[2] = b / m;
    s->rtSunIntensity = intensity;
    yaw *= (float)M_PI / 180.0f; elevation *= (float)M_PI / 180.0f;
    s->rtSunDirection[0] = cosf( yaw ) * cosf( elevation );
    s->rtSunDirection[1] = sinf( yaw ) * cosf( elevation );
    s->rtSunDirection[2] = sinf( elevation );
}
static bool Near( const vec3_t v, float x, float y, float z ) {
    return fabsf( v[0] - x ) < 1e-3f && fabsf( v[1] - y ) < 1e-3f && fabsf( v[2] - z ) < 1e-3f;
}

int main() {
    /* id's q3map: the first sky surface's sun, unit-length colour
     * (q3tourney2's q3map_sun 1 1 0.5 150 75 60); a sun on a non-sky shader
     * before it does not count */
    Reset( qfalse );
    Sun( &shaders[0], 1, 0, 0, 500, 0, 90 );
    shaders[1].surfaceFlags = SURF_SKY;
    Sun( &shaders[1], 1, 1, 0.5f, 150, 75, 60 );
    RT_FindSun();
    assert( s_rtLights.sunValid && Near( s_rtLights.sunLight, 100, 100, 50 ) );
    assert( fabsf( s_rtLights.sunDirection[2] - sinf( 60.0f * (float)M_PI / 180.0f ) ) < 1e-5f );
    /* a white sun is 1/sqrt(3) of its intensity in each channel (q3dm17's
     * q3map_sun 1 1 1 32 90 90) */
    Reset( qfalse );
    shaders[2].surfaceFlags = SURF_SKY;
    Sun( &shaders[2], 1, 1, 1, 32, 90, 90 );
    RT_FindSun();
    const float white = 32.0f / sqrtf( 3.0f );
    assert( s_rtLights.sunValid && Near( s_rtLights.sunLight, white, white, white ) );
    assert( Near( s_rtLights.sunDirection, 0, 0, 1 ) );
    /* the first sky surface has no sun: the map has none, whatever later
     * sky surfaces say */
    Reset( qfalse );
    shaders[0].surfaceFlags = SURF_SKY;
    shaders[1].surfaceFlags = SURF_SKY;
    Sun( &shaders[1], 1, 1, 1, 100, 0, 45 );
    RT_FindSun();
    assert( !s_rtLights.sunValid );
    /* sky by skyparms, and sky as the map's BSP recorded it though a later
     * script dropped surfaceparm sky */
    for ( int kind = 0; kind < 2; ++kind ) {
        Reset( qfalse );
        if ( kind ) shaders[3].rtCompiledSky = qtrue; else shaders[3].isSky = qtrue;
        Sun( &shaders[3], 0.5f, 0.6f, 0.8f, 100, 70, 60 );
        RT_FindSun();
        const float length = sqrtf( 0.25f + 0.36f + 0.64f );
        assert( s_rtLights.sunValid && Near( s_rtLights.sunLight, 50 / length, 60 / length, 80 / length ) );
    }
    /* no sky at all */
    Reset( qfalse );
    Sun( &shaders[0], 1, 1, 1, 100, 0, 45 );
    RT_FindSun();
    assert( !s_rtLights.sunValid );

    /* Wolfenstein's own compiler picks id's sun but keeps its largest
     * component at 1, as its lightmaps show (tr_sky_sun_units.h); so do
     * Quake Live's and Enemy Territory's maps without q3map2's keys */
    {
        const int games[] = { COM_GAME_RTCW_SP, COM_GAME_RTCW_MP, COM_GAME_QUAKELIVE, COM_GAME_WOLFET };
        for ( int game : games ) {
            Reset( qfalse );
            ri.gameId = game;
            Sun( &shaders[0], 1, 0, 0, 500, 0, 90 );
            shaders[1].surfaceFlags = SURF_SKY;
            Sun( &shaders[1], 1, 1, 0.5f, 150, 75, 60 );
            RT_FindSun();
            assert( s_rtLights.sunValid && Near( s_rtLights.sunLight, 150, 150, 75 ) );
        }
        /* Raven's and id's own keep unit length */
        const int unit[] = { COM_GAME_QUAKE3, COM_GAME_Q3TEST, COM_GAME_STVEF_MP, COM_GAME_JKA_MP, COM_GAME_JK2_SP };
        for ( int game : unit ) {
            Reset( qfalse );
            ri.gameId = game;
            shaders[1].surfaceFlags = SURF_SKY;
            Sun( &shaders[1], 1, 1, 0.5f, 150, 75, 60 );
            RT_FindSun();
            assert( s_rtLights.sunValid && Near( s_rtLights.sunLight, 100, 100, 50 ) );
        }
    }
    /* q3map2: the first shader with a sun, sky or not, largest component 1 */
    Reset( qtrue );
    Sun( &shaders[1], 2, 1, 0, 100, 0, 30 );
    shaders[2].surfaceFlags = SURF_SKY;
    Sun( &shaders[2], 1, 1, 1, 500, 0, 30 );
    RT_FindSun();
    assert( s_rtLights.sunValid && Near( s_rtLights.sunLight, 100, 50, 0 ) );
    /* ... unless worldspawn sets _noshadersun */
    s_rtLights.sunValid = qfalse;
    s_rtLights.noShaderSun = qtrue;
    RT_FindSun();
    assert( !s_rtLights.sunValid );

    /* malformed suns light nothing */
    for ( int bad = 0; bad < 4; ++bad ) {
        Reset( qfalse );
        shaders[0].surfaceFlags = SURF_SKY;
        Sun( &shaders[0], 1, 1, 1, 100, 0, 45 );
        if ( bad == 0 ) shaders[0].rtSunDirection[1] = NAN;
        if ( bad == 1 ) shaders[0].rtSunIntensity = INFINITY;
        if ( bad == 2 ) VectorClear( shaders[0].rtSunDirection );
        if ( bad == 3 ) shaders[0].rtSunIntensity = 0;
        RT_FindSun();
        assert( !s_rtLights.sunValid );
        assert( Near( s_rtLights.sunLight, 0, 0, 0 ) );
    }
    /* no world surfaces, and a surface without a shader */
    Reset( qfalse );
    world.numsurfaces = 0;
    RT_FindSun();
    assert( !s_rtLights.sunValid );
    world.numsurfaces = 4;
    surfaces[0].shader = NULL;
    shaders[1].surfaceFlags = SURF_SKY;
    Sun( &shaders[1], 1, 0, 0, 10, 0, 90 );
    RT_FindSun();
    assert( s_rtLights.sunValid && Near( s_rtLights.sunLight, 10, 0, 0 ) );
    /* a plain sun has no spread */
    assert( s_rtLights.sunSamples == 0 && s_rtLights.sunDeviance == 0.0f );

    /* q3map_sunExt: Quake Live's common 3 degrees in 16 samples, in radians */
    Reset( qtrue );
    Sun( &shaders[0], 1, 1, 1, 100, 0, 45 );
    shaders[0].rtSunDeviance = 3.0f;
    shaders[0].rtSunSamples = 16;
    RT_FindSun();
    assert( s_rtLights.sunValid && s_rtLights.sunSamples == 16 );
    assert( fabsf( s_rtLights.sunDeviance - 3.0f * (float)M_PI / 180.0f ) < 1e-6f );
    /* one sample, no deviance, or a deviance that is not a number: the
     * compiler's single sun */
    for ( int bad = 0; bad < 4; ++bad ) {
        Reset( qtrue );
        Sun( &shaders[0], 1, 1, 1, 100, 0, 45 );
        shaders[0].rtSunDeviance = bad == 1 ? 0.0f : bad == 2 ? NAN : bad == 3 ? -2.0f : 3.0f;
        shaders[0].rtSunSamples = bad == 0 ? 1 : 16;
        RT_FindSun();
        assert( s_rtLights.sunValid && s_rtLights.sunSamples == 0 && s_rtLights.sunDeviance == 0.0f );
    }

    /* q3map_skyLight: each shader with one, once however many surfaces use
     * it, in its light colour (white without one); the domes add up and the
     * densest stands in for them */
    Reset( qtrue );
    shaders[0].rtSkyLightValue = 100; shaders[0].rtSkyLightIterations = 3;
    shaders[0].rtEmitColorValid = qtrue; VectorSet( shaders[0].rtEmitColor, 1, 0.5f, 0.25f );
    shaders[2].rtSkyLightValue = 20; shaders[2].rtSkyLightIterations = 4;
    surfaces[1].shader = &shaders[0];
    RT_FindSkyLight();
    assert( Near( s_rtLights.skyLight, 120, 70, 45 ) && s_rtLights.skyLightIterations == 4 );
    /* none, a value that is not a number, or too many rows */
    Reset( qtrue );
    RT_FindSkyLight();
    assert( s_rtLights.skyLightIterations == 0 && Near( s_rtLights.skyLight, 0, 0, 0 ) );
    shaders[0].rtSkyLightValue = NAN; shaders[0].rtSkyLightIterations = 3;
    RT_FindSkyLight();
    assert( s_rtLights.skyLightIterations == 0 );
    Reset( qtrue );
    shaders[3].rtSkyLightValue = 50; shaders[3].rtSkyLightIterations = 1000;
    RT_FindSkyLight();
    assert( Near( s_rtLights.skyLight, 50, 50, 50 ) && s_rtLights.skyLightIterations == 64 );
    return 0;
}
'''


class RtxLightRulesTests(unittest.TestCase):
    def test_the_sun_is_the_one_the_maps_compiler_used(self):
        rtx_surface_owner_tests.compile_run(self, harness() + CHECKS)


if __name__ == "__main__":
    unittest.main()
