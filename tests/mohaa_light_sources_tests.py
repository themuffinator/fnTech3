"""Medal of Honor's lights reaching a point, for its client game's shadows.

1.00's client game asks, through its import 54, which lights reach a point,
and casts a character's foot shadows from each under "Complex" shadows
(`cg_shadows 2`; its one call, 0x30029b98). fnTech3 had answered none, so
Complex shadows fell back to the simple ones. The collision model now keeps
the map's sphere lights and each leaf's list of them
(code/qcommon/cm_mohaa_lights.c) and answers as OpenMoHAA's renderer does
(reference only): in a leaf with lights, the sun when the leaf's list names it
first and the sky is in reach, then the leaf's lights whose area the view can
see; elsewhere every light; each counted when intensity times 7,500 over its
squared distance comes to 5, the strongest first, at most eight.

The fixture compiles the loader and the query sliced from the file, with the
collision model's lookups stubbed; no retail file is read."""
import unittest
from pathlib import Path

from mohaa_permanent_decal_tests import build_and_run, function, read

ROOT = Path(__file__).resolve().parents[1]

HOST = r'''
#include "code/qcommon/q_shared.h"
#include "code/qcommon/bsp_mohaa.h"
#include "code/qcommon/cm_mohaa_lights.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void QDECL Com_Error( errorParm_t level, const char *fmt, ... ) { (void)level; (void)fmt; exit( 3 ); }
void QDECL Com_Printf( const char *fmt, ... ) { (void)fmt; }

typedef struct { int area; } cLeaf_t;
static struct {
	int numLeafs;
	cLeaf_t *leafs;
	qboolean vised;
	char *entityString;
	struct cmMOHAALights_s *mohaaLights;
} cm;
void *Hunk_Alloc( int size, ha_pref preference ) { (void)preference; return calloc( 1, size ); }

static int pointLeaf, traces;
static qboolean skyInReach;
static int CM_PointLeafnum( const vec3_t p ) { (void)p; return pointLeaf; }
static void CM_BoxTrace( trace_t *results, const vec3_t start, const vec3_t end, const vec3_t mins,
	const vec3_t maxs, int model, int brushmask, qboolean capsule ) {
	(void)start; (void)end; (void)mins; (void)maxs; (void)model; (void)capsule;
	assert( brushmask == CONTENTS_SOLID );
	traces++;
	memset( results, 0, sizeof( *results ) );
	results->surfaceFlags = skyInReach ? SURF_SKY : 0;
}
#define MOHAA_GATHERED_LIGHTS	8
#define MOHAA_SUN_DISTANCE		16384.0f
#define MOHAA_SUN_OVERBRIGHT	2.0f
'''

CHECKS = r'''
static dsphereLightMOHAA_t lights[10];
static int32_t vis[64];
static cLeaf_t leafs[4];

static void Light( int i, float x, float intensity, float r, float g, float b, int leaf ) {
	memset( &lights[i], 0, sizeof( lights[i] ) );
	lights[i].origin[0] = x;
	lights[i].color[0] = r;
	lights[i].color[1] = g;
	lights[i].color[2] = b;
	lights[i].intensity = intensity;
	lights[i].leaf = leaf;
}

int main( void ) {
	mohaa_translation_t translation;
	vec3_t positions[8], intensities[8];
	const vec3_t point = { 0, 0, 0 };
	byte areamask[32];
	int n = 0, count, i;

	/* nine lights along x; light 8 is dark, light 9 names no leaf */
	for ( i = 0; i < 8; i++ ) {
		Light( i, 100.0f + 20.0f * i, 100.0f, 0.5f, 0.25f, 1.0f, i < 4 ? 1 : 2 );
	}
	Light( 8, 50.0f, -1.0f, 1, 1, 1, 1 );
	Light( 9, 60.0f, 100.0f, 1, 1, 1, 99 );
	/* leaf 0: the sun and lights 0, 5, 8, 9 and a bad 42; leaf 1: none;
	 * leaf 2: every light; leaf 3: only the sun */
	vis[n++] = -2; vis[n++] = 0; vis[n++] = 5; vis[n++] = 8; vis[n++] = 9; vis[n++] = 42; vis[n++] = -1;
	vis[n++] = -1;
	for ( i = 0; i < 10; i++ ) vis[n++] = i;
	vis[n++] = -1;
	vis[n++] = -2; vis[n++] = -1;
	leafs[0].area = 0; leafs[1].area = 1; leafs[2].area = 2; leafs[3].area = 0;
	cm.numLeafs = 4;
	cm.leafs = leafs;
	cm.vised = qtrue;
	cm.entityString = (char *)"{\n\"classname\" \"worldspawn\"\n\"suncolor\" \"65 60 45\"\n\"sundirection\" \"0 90 0\"\n"
		"\"sunlight\" \"255 102 51\"\n}\n{\n\"classname\" \"light\"\n\"suncolor\" \"1 2 3\"\n}\n";

	memset( &translation, 0, sizeof( translation ) );
	CMod_LoadMOHAALights( &translation );
	assert( cm.mohaaLights == NULL );
	translation.sphereLights = lights;
	translation.numSphereLights = 10;
	translation.sphereLightVis = vis;
	translation.numSphereLightVis = n;
	CMod_LoadMOHAALights( &translation );
	assert( cm.mohaaLights && cm.mohaaLights->numLights == 10 );
	assert( cm.mohaaLights->lights[9].leaf == -1 );
	assert( cm.mohaaLights->leafSun[0] && !cm.mohaaLights->leafSun[1] && !cm.mohaaLights->leafSun[2] );
	assert( cm.mohaaLights->leafSun[3] && cm.mohaaLights->leafCount[3] == 0 );
	assert( cm.mohaaLights->leafCount[0] == 4 && cm.mohaaLights->leafCount[1] == 0 && cm.mohaaLights->leafCount[2] == 10 );
	/* the worldspawn's last sun colour, doubled by 2015's overbright, and
	 * its direction; the next entity's keys are not the world's */
	assert( cm.mohaaLights->sun && cm.mohaaLights->sunColor[0] == 510.0f && cm.mohaaLights->sunColor[2] == 102.0f );
	assert( fabsf( cm.mohaaLights->sunDirection[1] - 1.0f ) < 1e-5f );

	/* in leaf 0 with the sky in reach: the sun, then lights 9 and 0 by
	 * strength (light 5 at 200 units comes to 18.75); 8 is dark */
	pointLeaf = 0;
	skyInReach = qtrue;
	count = CM_MOHAAGatherLights( point, NULL, positions, intensities, 8 );
	assert( count == 4 );
	assert( fabsf( positions[0][1] - 16384.0f ) < 0.01f && fabsf( intensities[0][0] - 1.0f ) < 1e-6f
		&& fabsf( intensities[0][1] - 0.4f ) < 1e-6f );
	assert( positions[1][0] == 60.0f && positions[2][0] == 100.0f && positions[3][0] == 200.0f );
	/* 100 * 7500 / 60^2 = 208.3, over 200 times white, scaled down to 1 */
	assert( fabsf( intensities[1][0] - 1.0f ) < 1e-6f && fabsf( intensities[1][1] - 1.0f ) < 1e-6f );
	/* 75 over 200 times (0.5, 0.25, 1) */
	assert( fabsf( intensities[2][2] - 0.375f ) < 1e-6f && fabsf( intensities[2][0] - 0.1875f ) < 1e-6f );
	/* no sky, no sun */
	skyInReach = qfalse;
	count = CM_MOHAAGatherLights( point, NULL, positions, intensities, 8 );
	assert( count == 3 && positions[0][0] == 60.0f );
	/* the view cannot see light 5's area: it goes */
	memset( areamask, 0, sizeof( areamask ) );
	areamask[0] = 1 << 2;
	count = CM_MOHAAGatherLights( point, areamask, positions, intensities, 8 );
	assert( count == 2 && positions[1][0] == 100.0f );
	/* the caller's limit holds */
	count = CM_MOHAAGatherLights( point, NULL, positions, intensities, 1 );
	assert( count == 1 && positions[0][0] == 60.0f );
	/* leaf 1 lists nothing, so every light counts, the strongest eight */
	pointLeaf = 1;
	skyInReach = qtrue;
	traces = 0;
	count = CM_MOHAAGatherLights( point, areamask, positions, intensities, 8 );
	assert( count == 8 && traces == 0 && positions[0][0] == 60.0f && positions[7][0] == 220.0f );
	/* leaf 3: only the sun */
	pointLeaf = 3;
	count = CM_MOHAAGatherLights( point, NULL, positions, intensities, 8 );
	assert( count == 1 && fabsf( positions[0][1] - 16384.0f ) < 0.01f );
	/* without vis, every light and never the sun */
	cm.vised = qfalse;
	count = CM_MOHAAGatherLights( point, NULL, positions, intensities, 8 );
	assert( count == 8 && positions[0][0] == 60.0f );
	cm.vised = qtrue;
	/* a light out of reach: 100 * 7500 / 1225^2 is under 5 */
	{
		const vec3_t far = { -1125.0f, 0, 0 };
		pointLeaf = 1;
		count = CM_MOHAAGatherLights( far, NULL, positions, intensities, 8 );
		assert( count == 0 );
	}
	/* what is refused */
	assert( CM_MOHAAGatherLights( point, NULL, positions, intensities, 0 ) == 0 );
	assert( CM_MOHAAGatherLights( point, NULL, NULL, intensities, 8 ) == 0 );
	cm.mohaaLights = NULL;
	assert( CM_MOHAAGatherLights( point, NULL, positions, intensities, 8 ) == 0 );
	puts( "lights ok" );
	return 0;
}
'''


class MohaaLightSourcesTests(unittest.TestCase):
    def test_the_lights_reaching_a_point_are_the_leafs_strongest(self):
        source = read('code/qcommon/cm_mohaa_lights.c')
        body = HOST
        for signature in ('static void CM_MOHAASunKeys(', 'void CMod_LoadMOHAALights(',
                          'static float CM_MOHAALightFalloff(', 'static void CM_MOHAAKeepLight(',
                          'int CM_MOHAAGatherLights('):
            body += '\n' + function(source, signature)
        output = build_and_run(self, 'lights', [
            body + CHECKS,
            ROOT / 'code/qcommon/q_shared.c',
            ROOT / 'code/qcommon/q_math.c',
        ])
        self.assertIn('lights ok', output)

    def test_the_client_game_is_answered_from_the_last_view(self):
        cgame = read('code/client/cl_mohaa_cgame.cpp')
        gather = function(cgame, 'int QDECL GatherLightSources(')
        self.assertIn('return CM_MOHAAGatherLights( position, sceneAreamaskSet ? sceneAreamask : nullptr,', gather)
        render = function(cgame, 'void QDECL RenderScene(')
        self.assertIn('std::memcpy( sceneAreamask, fd->areamask, sizeof( fd->areamask ) );', render)
        load = read('code/qcommon/cm_load.c')
        self.assertLess(load.index('CMod_LoadMOHAATerrain( &mohaaTranslated );'),
                        load.index('CMod_LoadMOHAALights( &mohaaTranslated );'))
        self.assertLess(load.index('CMod_LoadEntityString (&header.lumps[LUMP_ENTITIES]);'),
                        load.index('CMod_LoadMOHAALights( &mohaaTranslated );'))
        imports = read('code/compat/mohaa_cgame_imports.def')
        self.assertRegex(imports, r'FNTECH3_MOHAA_CGAME_IMPORT\(\s+54, R_GatherLightSources,\s+4,')
        for build in ('meson.build', 'Makefile'):
            self.assertIn('cm_mohaa_lights', read(build))


if __name__ == '__main__':
    unittest.main()
