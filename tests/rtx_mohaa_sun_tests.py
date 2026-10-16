"""Execute the path tracer's Medal of Honor sun (code/rendererrtx/rt/rt_lights.c)
against a bounded host.

2015's maps name their sun in worldspawn, and none of the game's shaders has a
q3map_sun (docs/fntech3/2026-10-16-mohaa-worldspawn-sun.md). `suncolor`, or
`sunlight`, is what the compiler stored in the lightmaps times the cosine to
the sun, so it is the path tracer's sun light unscaled; `sundirection` holds
the angles that point toward the sun (AngleVectors). `sundiffuse` (1 when
absent, at most 1) of `sundiffusecolor` (the sun's colour when absent) is what
an open level floor stored from the sky: a dome of suns gives it that much.
The real RT_ParseMOHAASun, RT_SkyDomeFloorShare and RT_FindMOHAASun run here.
"""
import math
from pathlib import Path
import unittest

from mesh_tag_tests import function
from rtx_light_entity_tests import between, compile_run_c

ROOT = Path(__file__).resolve().parents[1]
LIGHTS = ROOT / 'code/rendererrtx/rt/rt_lights.c'
MATERIAL = ROOT / 'code/rendererrtx/rt/rt_material.c'


def harness():
    lights = LIGHTS.read_text(encoding='utf-8')
    material = MATERIAL.read_text(encoding='utf-8')
    defines = '\n'.join(line for line in lights.splitlines() if line.startswith('#define RT_LIGHT_'))
    modes = '\n'.join(line for line in material.splitlines() if line.startswith('#define RT_LIGHTING_'))
    source = r'''
#define _CRT_SECURE_NO_WARNINGS 1
#include "code/qcommon/q_shared.c"
#include "code/qcommon/q_math.c"
#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
void QDECL Com_Printf( const char *fmt, ... ) { (void)fmt; }
void QDECL Com_DPrintf( const char *fmt, ... ) { (void)fmt; }
void NORETURN QDECL Com_Error( errorParm_t level, const char *fmt, ... ) { (void)level; (void)fmt; exit( 3 ); }
typedef struct { const char *entityString; } world_t;
static struct { world_t *world; } tr;
static int developerLines;
static void HostPrintf( int level, const char *fmt, ... ) { (void)fmt; if ( level == PRINT_DEVELOPER ) developerLines++; }
static struct { void ( *Printf )( int, const char *, ... ); } ri = { HostPrintf };
static struct {
    int mode;
    qboolean sunValid;
    vec3_t sunLight, sunDirection;
    vec3_t skyLight, skyDiffuse;
    int skyLightIterations;
} s_rtLights;
'''
    source += defines + '\n' + modes + '\n'
    source += function(lights, 'static qboolean RT_FloatValid( float v, float limit )')
    source += function(lights, 'static qboolean RT_Vec3Valid( const vec3_t v, float limit )')
    source += between(lights, '#define RT_MOHAA_SUN_DEFAULT', '} rtMOHAASun_t;')
    source += function(lights, 'static qboolean RT_ParseMOHAASun( const char *entities, rtMOHAASun_t *sun )')
    source += function(lights, 'static float RT_SkyDomeFloorShare( int iterations )')
    source += function(lights, 'static void RT_FindMOHAASun( void )')
    return source


def dome_floor_share(iterations):
    """rt_sample_sky_light's dome, sun by sun: the mean cosine on a level floor."""
    rows = iterations - 1
    per_row = rows * 4
    suns = per_row * rows + 1
    total = 0.0
    for k in range(suns):
        if k < suns - 1:
            row = k // per_row
            elevation = (0.5 * math.pi / iterations) * (row + 0.5)
            total += math.sin(elevation)
        else:
            total += 1.0
    return total / suns


CHECKS = r'''
static bool Near( const vec3_t v, float x, float y, float z, float e ) {
    return fabsf( v[0] - x ) < e && fabsf( v[1] - y ) < e && fabsf( v[2] - z ) < e;
}
static rtMOHAASun_t Parse( const char *entities, qboolean expected ) {
    rtMOHAASun_t sun;
    assert( RT_ParseMOHAASun( entities, &sun ) == expected );
    return sun;
}
static void Find( const char *entities, int mode ) {
    static world_t world;
    memset( &s_rtLights, 0, sizeof( s_rtLights ) );
    s_rtLights.mode = mode;
    world.entityString = entities;
    tr.world = &world;
    developerLines = 0;
    RT_FindMOHAASun();
}

int main( void ) {
    /* DM/mohdm6: a white sun straight overhead ("-90 90 0"), its diffuse sun
     * the same colour at 1 */
    const char *mohdm6 = "{\n\"classname\" \"worldspawn\"\n\"ambientlight\" \"20 19 18\"\n\"sundiffuse\" \"1\"\n"
        "\"sundirection\" \"-90 90 0\"\n\"suncolor\" \"85 85 85\"\n\"ambient\" \"50\"\n\"sundiffusecolor\" \"85 85 85\"\n}\n"
        "{\n\"classname\" \"info_player_deathmatch\"\n\"origin\" \"0 0 0\"\n}\n";
    rtMOHAASun_t sun = Parse( mohdm6, qtrue );
    assert( Near( sun.light, 85, 85, 85, 1e-4f ) && Near( sun.direction, 0, 0, 1, 1e-5f ) );
    assert( Near( sun.diffuse, 85, 85, 85, 1e-4f ) );

    /* m4l2: pitch -35 above the horizon at yaw 10; sundiffuse .4 of its own
     * diffuse colour */
    sun = Parse( "{ \"sundirection\" \"-35 10 0\" \"suncolor\" \"42 64 95\" \"sundiffusecolor\" \"42 64 95\" "
        "\"sundiffuse\" \".4\" }", qtrue );
    {
        const float p = DEG2RAD( -35.0f ), y = DEG2RAD( 10.0f );
        assert( Near( sun.direction, cosf( p ) * cosf( y ), cosf( p ) * sinf( y ), -sinf( p ), 1e-5f ) );
        assert( sun.direction[2] > 0.5f );
    }
    assert( Near( sun.light, 42, 64, 95, 1e-4f ) && Near( sun.diffuse, 16.8f, 25.6f, 38, 1e-3f ) );

    /* m3l2: `sunlight` is the colour's other name; pitch 315 is -45 */
    sun = Parse( "{ \"sundiffuse\" \"3\" \"sundirection\" \"315 315 0\" \"sunlight\" \"90 90 90\" }", qtrue );
    assert( Near( sun.light, 90, 90, 90, 1e-4f ) && fabsf( sun.direction[2] - sinf( DEG2RAD( 45.0f ) ) ) < 1e-5f );
    /* ... and a sundiffuse above 1 stores no more than 1: the sun's colour,
     * there being no sundiffusecolor */
    assert( Near( sun.diffuse, 90, 90, 90, 1e-4f ) );

    /* DM/mohdm1 names both: the later key wins, as in 2015's renderer */
    sun = Parse( "{ \"suncolor\" \"65 60 45\" \"sundirection\" \"320 150 0\" \"sunlight\" \"255 243 171\" \"sundiffuse\" \"1.3\" }", qtrue );
    assert( Near( sun.light, 255, 243, 171, 1e-4f ) && Near( sun.diffuse, 255, 243, 171, 1e-4f ) );

    /* training: no sundiffuse is 1 */
    sun = Parse( "{ \"ambientlight\" \"9 8 6\" \"sundirection\" \"330 70 0\" \"suncolor\" \"70 65 60\" }", qtrue );
    assert( Near( sun.diffuse, 70, 65, 60, 1e-4f ) );

    /* a direction without a colour is 70 in each channel, as the renderer's */
    sun = Parse( "{ \"sundirection\" \"-90 0 0\" }", qtrue );
    assert( Near( sun.light, 70, 70, 70, 1e-4f ) && Near( sun.direction, 0, 0, 1, 1e-5f ) );
    /* a colour that is not three numbers is no colour */
    sun = Parse( "{ \"suncolor\" \"1\" \"sundirection\" \"-90 0 0\" }", qtrue );
    assert( Near( sun.light, 70, 70, 70, 1e-4f ) );
    sun = Parse( "{ \"suncolor\" \"10 20 nan\" \"sundirection\" \"-90 0 0\" }", qtrue );
    assert( Near( sun.light, 70, 70, 70, 1e-4f ) );

    /* no direction, or none that parses: no sun */
    Parse( "{ \"suncolor\" \"85 85 85\" }", qfalse );
    Parse( "{ \"suncolor\" \"85 85 85\" \"sundirection\" \"-90\" }", qfalse );
    Parse( "{ \"suncolor\" \"85 85 85\" \"sundirection\" \"-90 inf 0\" }", qfalse );
    /* keys are matched exactly, and only the worldspawn's count */
    Parse( "{ \"SunDirection\" \"-90 90 0\" }", qfalse );
    Parse( "{ \"classname\" \"worldspawn\" }\n{ \"sundirection\" \"-90 90 0\" }", qfalse );
    /* no entities, no worldspawn brace, a cut-off worldspawn */
    Parse( NULL, qfalse );
    Parse( "", qfalse );
    Parse( "\"sundirection\" \"-90 90 0\"", qfalse );
    Parse( "{ \"sundirection\" \"-90 90 0\" \"suncolor\"", qtrue );

    /* a black sun with no diffuse sky lights nothing; a black sun with a
     * diffuse colour lights from the sky alone */
    Parse( "{ \"sundirection\" \"-90 0 0\" \"suncolor\" \"0 0 0\" }", qfalse );
    sun = Parse( "{ \"sundirection\" \"-90 0 0\" \"suncolor\" \"0 0 0\" \"sundiffusecolor\" \"10 10 10\" }", qtrue );
    assert( Near( sun.light, 0, 0, 0, 1e-6f ) && Near( sun.diffuse, 10, 10, 10, 1e-5f ) );
    /* negative colours and diffuse factors light nothing below zero; a
     * diffuse factor that is not a number is none */
    sun = Parse( "{ \"sundirection\" \"-90 0 0\" \"suncolor\" \"-5 10 20\" \"sundiffuse\" \"-2\" }", qtrue );
    assert( Near( sun.light, 0, 10, 20, 1e-5f ) && Near( sun.diffuse, 0, 0, 0, 1e-6f ) );
    sun = Parse( "{ \"sundirection\" \"-90 0 0\" \"suncolor\" \"5 10 20\" \"sundiffuse\" \"nan\" }", qtrue );
    assert( Near( sun.diffuse, 0, 0, 0, 1e-6f ) );

    /* the dome's share on a level floor, as rt_sample_sky_light places its suns */
    assert( fabsf( RT_SkyDomeFloorShare( RT_MOHAA_SKY_ITERATIONS ) - (float)DOME_SHARE ) < 1e-5f );
    assert( fabsf( RT_SkyDomeFloorShare( 2 ) - (float)DOME_SHARE_2 ) < 1e-5f );

    /* the scene: the sun as given, and a dome that gives an open floor the
     * diffuse sun */
    Find( mohdm6, RT_LIGHTING_LEVEL );
    assert( s_rtLights.sunValid && Near( s_rtLights.sunLight, 85, 85, 85, 1e-4f ) );
    assert( Near( s_rtLights.sunDirection, 0, 0, 1, 1e-5f ) );
    assert( s_rtLights.skyLightIterations == RT_MOHAA_SKY_ITERATIONS );
    {
        const float floor = s_rtLights.skyLight[0] * RT_SkyDomeFloorShare( RT_MOHAA_SKY_ITERATIONS );
        assert( fabsf( floor - 85.0f ) < 1e-3f );
    }
    assert( Near( s_rtLights.skyDiffuse, 85, 85, 85, 1e-4f ) && developerLines == 0 );
    /* the dome adds to one a shader asked for, and keeps the denser */
    memset( &s_rtLights, 0, sizeof( s_rtLights ) );
    s_rtLights.mode = RT_LIGHTING_COMPILERS;
    VectorSet( s_rtLights.skyLight, 1, 2, 3 );
    s_rtLights.skyLightIterations = 20;
    {
        static world_t world;
        world.entityString = mohdm6;
        tr.world = &world;
    }
    RT_FindMOHAASun();
    assert( s_rtLights.skyLightIterations == 20 );
    assert( fabsf( s_rtLights.skyLight[2] - ( 3.0f + 85.0f / RT_SkyDomeFloorShare( RT_MOHAA_SKY_ITERATIONS ) ) ) < 1e-3f );
    /* the emitters alone: no dome, the even sky asks for the diffuse sun */
    Find( mohdm6, RT_LIGHTING_EMITTERS );
    assert( s_rtLights.sunValid && s_rtLights.skyLightIterations == 0 && Near( s_rtLights.skyLight, 0, 0, 0, 1e-6f ) );
    assert( Near( s_rtLights.skyDiffuse, 85, 85, 85, 1e-4f ) );
    /* a black sun with a diffuse sky: no sun, a dome */
    Find( "{ \"sundirection\" \"-90 0 0\" \"suncolor\" \"0 0 0\" \"sundiffusecolor\" \"10 10 10\" }", RT_LIGHTING_LEVEL );
    assert( !s_rtLights.sunValid && s_rtLights.skyLightIterations == RT_MOHAA_SKY_ITERATIONS );
    /* no sun named, or no world: nothing, said to a developer */
    Find( "{ \"classname\" \"worldspawn\" }", RT_LIGHTING_LEVEL );
    assert( !s_rtLights.sunValid && s_rtLights.skyLightIterations == 0 && developerLines == 1 );
    memset( &s_rtLights, 0, sizeof( s_rtLights ) );
    tr.world = NULL;
    developerLines = 0;
    RT_FindMOHAASun();
    assert( !s_rtLights.sunValid && developerLines == 1 );
    puts( "PASS" );
    return 0;
}
'''


class RtxMohaaSunTests(unittest.TestCase):
    def test_the_worldspawn_sun_is_the_compilers(self):
        checks = CHECKS.replace('(float)DOME_SHARE_2', repr(dome_floor_share(2)) + 'f').replace(
            '(float)DOME_SHARE', repr(dome_floor_share(8)) + 'f')
        compile_run_c(self, harness() + checks)

    def test_the_mohaa_profile_alone_takes_it(self):
        lights = LIGHTS.read_text(encoding='utf-8')
        self.assertIn('#define RT_MOHAA_SKY_ITERATIONS\t8', lights)
        build = function(lights, 'static qboolean RT_BuildLights( void )')
        branch = ('if ( ri.gameId == COM_GAME_MOHAA ) {\n\t\t\tRT_FindMOHAASun();\n\t\t} else {\n'
                  '\t\t\tRT_FindSun();\n\t\t}\n')
        # both the emitting modes and the baked ones
        self.assertEqual(build.count(branch), 2)
        self.assertEqual(build.count('RT_FindSun();'), 2)
        self.assertIn('ri.gameId == COM_GAME_MOHAA ? "worldspawn" : s_rtLights.q3map2 ? "q3map2" : "q3map",', build)
        self.assertIn('ri.gameId == COM_GAME_MOHAA ? "worldspawn diffuse" : "q3map2",', build)
        # the emitters' even sky asks for the diffuse sun, zero elsewhere
        sky = function(lights, 'static void RT_FindSkyLightColor(')
        self.assertIn('VectorMA( wanted, s_rtLights.scale, s_rtLights.skyDiffuse, wanted );', sky)
        self.assertLess(sky.index('s_rtLights.skyDiffuse'), sky.index('luminance = '))
        # the dome is the one the shader samples
        glsl = (ROOT / 'code/rendererrtx/shaders/rt_lighting.glsl').read_text(encoding='utf-8')
        sample = function(glsl, 'vec3 rt_sample_sky_light(')
        self.assertIn('float elevation = (0.5 * PI / iterations) * (float(row) + 0.5);', sample)
        self.assertIn('uint perRow = rows * 4u;', sample)
        self.assertIn('uint suns = perRow * rows + 1u;', sample)
        # no Medal of Honor shader has a sun of its own, and the shared rules
        # for the other profiles' suns are unchanged
        sun = function(lights, 'static void RT_FindSun( void )')
        self.assertNotIn('MOHAA', sun)


if __name__ == '__main__':
    unittest.main()
