"""What loading every official Enemy Territory map found, held to its fixes."""
from pathlib import Path
import os
import re
import shutil
import subprocess
import sys
import tempfile
import unittest
import zipfile
from compiler_support import compiler_target_args
from mesh_tag_tests import function

ROOT = Path(__file__).resolve().parents[1]
RENDERERS = ('renderer', 'renderervk', 'rendererrtx')
RECORD = ROOT/'docs/fntech3/2026-09-18-wolfet-map-load.md'
sys.path.insert(0, str(ROOT/'scripts'))


def read(relative):
    return (ROOT/relative).read_text(encoding='utf-8')


HOST = r'''
#define _CRT_SECURE_NO_WARNINGS 1
#include "code/qcommon/q_shared.h"
#include <assert.h>
#include <float.h>
#include <limits.h>
#include <math.h>
#include <string.h>
'''

C_CHECKS = r'''
#include "code/renderercommon/tr_wolfet_foliage.h"
#include "code/renderercommon/tr_wolfet_lightmap.h"
#include "code/renderercommon/tr_wolfet_decal.h"

static void Near(float actual, float expected) { assert(fabsf(actual - expected) < 0.001f); }

static void Foliage(void) {
	fnWolfETFoliageCounts_t counts;
	float cull[4] = { 480, 1024, 0.49f, 0 }, axis[4] = { 1, 0, 0, -10 }, point[3] = { 30, 0, 0 };
	float normal[3] = { 1, 0, 0 }, keep[3] = { -63, 0, 0 }, drop[3] = { -65, 0, 0 };
	/* Radar's first rain ripple: four mesh vertices, nineteen instances, one quad. */
	assert(FnWolfET_FoliageCounts(19, 4, 6, 0, 0, 23, 6, 2000, 12000, &counts));
	assert(counts.meshVerts == 4 && counts.instances == 19 && counts.indexes == 6);
	assert(FnWolfET_FoliageCounts(0, 4, 6, 0, 0, 4, 6, 2000, 12000, &counts) && counts.instances == 0);
	/* Instances past the vertex lump, a partial triangle, no mesh, a mesh too
	 * large for one batch, and negative or out-of-range starts are refused. */
	assert(!FnWolfET_FoliageCounts(19, 4, 6, 0, 0, 22, 6, 2000, 12000, &counts));
	assert(!FnWolfET_FoliageCounts(19, 4, 5, 0, 0, 23, 6, 2000, 12000, &counts));
	assert(!FnWolfET_FoliageCounts(19, 0, 6, 0, 0, 23, 6, 2000, 12000, &counts));
	assert(!FnWolfET_FoliageCounts(1, 2001, 6, 0, 0, 3000, 6, 2000, 12000, &counts));
	assert(!FnWolfET_FoliageCounts(-1, 4, 6, 0, 0, 23, 6, 2000, 12000, &counts));
	assert(!FnWolfET_FoliageCounts(19, 4, 6, 0, 1, 23, 6, 2000, 12000, &counts));
	assert(!FnWolfET_FoliageCounts(19, 4, 6, INT_MAX, 0, 23, 6, 2000, 12000, &counts));
	/* r_drawfoliage scales height: negative reads as one, and two is the most. */
	Near(FnWolfET_FoliageHeightScale(-1), 1); Near(FnWolfET_FoliageHeightScale(0), 0);
	Near(FnWolfET_FoliageHeightScale(1.5f), 1.5f); Near(FnWolfET_FoliageHeightScale(3), 2);
	/* A fade keeps the reciprocal of its length; a fade of no length is none. */
	FnWolfET_DistanceCullFinish(cull);
	Near(cull[3], 1.0f / 544.0f);
	{
		float none[4] = { 100, 50, 0.5f, 7 };
		FnWolfET_DistanceCullFinish(none);
		assert(none[0] == 0 && none[1] == 0 && none[2] == 0 && none[3] == 0);
		assert(!FnWolfET_DistanceCullActive(none));
		assert(FnWolfET_FoliageSurfaceVisible(none, 99999, 0, 90));
		assert(FnWolfET_DistanceCullActive(cull));
	}
	/* Depth along the view, widened by the field of view. */
	Near(FnWolfET_FoliageDepth(axis, point, 90), 20);
	Near(FnWolfET_FoliageDepth(axis, point, 45), 10);
	/* A surface goes once even its nearest edge has faded past the alpha. */
	assert(FnWolfET_FoliageSurfaceVisible(cull, 600, 100, 90));
	assert(!FnWolfET_FoliageSurfaceVisible(cull, 1000, 100, 90));
	assert(!FnWolfET_FoliageSurfaceVisible(cull, 1100, 10, 90));
	/* Instances: fully opaque near, faded in between, gone far or behind, and
	 * every second and fourth one a quarter further away. */
	assert(FnWolfET_FoliageInstanceAlpha(cull, 100, 0) == 255);
	assert(FnWolfET_FoliageInstanceAlpha(cull, 700, 0) == (int)(unsigned char)(int)((324.0f / 544.0f) * 255.0f));
	assert(FnWolfET_FoliageInstanceAlpha(cull, 800, 0) == -1);
	assert(FnWolfET_FoliageInstanceAlpha(cull, 700, 1) == -1);
	assert(FnWolfET_FoliageInstanceAlpha(cull, 700, 3) == -1);
	assert(FnWolfET_FoliageInstanceAlpha(cull, 500, 2) == (int)(unsigned char)(int)(((1024.0f - 625.0f) / 544.0f) * 255.0f));
	assert(FnWolfET_FoliageInstanceAlpha(cull, -65, 0) == -1);
	assert(FnWolfET_FoliageInstanceAlpha(cull, -63, 0) == 255);
	/* Each plane gives 64 units of slack. */
	assert(FnWolfET_FoliagePlaneKeeps(normal, 0, keep));
	assert(!FnWolfET_FoliagePlaneKeeps(normal, 0, drop));
}

static void Lightmaps(void) {
	char name[MAX_QPATH];
	assert(FnWolfET_ExternalLightmapName(name, sizeof(name), "maps/oasis.bsp", 0));
	assert(!strcmp(name, "maps/oasis/lm_0000.tga"));
	assert(FnWolfET_ExternalLightmapName(name, sizeof(name), "maps/goldrush.bsp", 14));
	assert(!strcmp(name, "maps/goldrush/lm_0014.tga"));
	assert(FnWolfET_ExternalLightmapName(name, sizeof(name), "maps.v2/custom", 255));
	assert(!strcmp(name, "maps.v2/custom/lm_0255.tga"));
	assert(!FnWolfET_ExternalLightmapName(name, sizeof(name), "maps/oasis.bsp", 256));
	assert(!FnWolfET_ExternalLightmapName(name, sizeof(name), "maps/oasis.bsp", -1));
	assert(!FnWolfET_ExternalLightmapName(name, 8, "maps/oasis.bsp", 0));
	assert(FnWolfET_LightmapSizeValid(256, 256) && FnWolfET_LightmapSizeValid(512, 256));
	assert(!FnWolfET_LightmapSizeValid(300, 256) && !FnWolfET_LightmapSizeValid(0, 256));
}

static int DecalOnto(const fnWolfETDecalProjector_t *dp, const float *a, const float *b, const float *c,
	fnWolfETDecal_t *decal) {
	float points[2][FN_WOLFET_MAX_DECAL_VERTS][3];
	memcpy(points[0][0], a, 12); memcpy(points[0][1], b, 12); memcpy(points[0][2], c, 12);
	return FnWolfET_DecalWinding(dp, 3, points, decal);
}

static void Decals(void) {
	const float quad[4][3] = { { 0, 0, 0 }, { 0, 10, 0 }, { 10, 10, 0 }, { 10, 0, 0 } };
	const float down[4] = { 0, 0, -1, 16 }, white[4] = { 1, 1, 1, 1 };
	fnWolfETDecalProjector_t dp;
	fnWolfETDecalCorner_t corners[4];
	fnWolfETDecal_t decal, slots[3];
	float direction[4];
	int footprint, i;
	/* Requests ET ignores. */
	assert(!FnWolfET_DecalProjectorStart(1, 2, quad, down, white, 1000, 500, 100, &dp, corners, direction, &footprint));
	assert(!FnWolfET_DecalProjectorStart(1, 4, quad, down, white, 0, 500, 100, &dp, corners, direction, &footprint));
	{
		const float flat[4] = { 0, 0, -1, 0 };
		assert(!FnWolfET_DecalProjectorStart(1, 4, quad, flat, white, 1000, 500, 100, &dp, corners, direction, &footprint));
	}
	/* A quad pressed straight down sixteen units. */
	assert(FnWolfET_DecalProjectorStart(7, 4, quad, down, white, 1000, 500, 100, &dp, corners, direction, &footprint));
	assert(dp.numPlanes == 6 && !dp.omnidirectional && footprint == 4);
	assert(dp.fadeStartTime == 600 && dp.fadeEndTime == 1100);
	Near(dp.center[0], 5); Near(dp.center[1], 5); Near(dp.center[2], -8);
	Near(dp.radius, sqrtf(114));
	assert(FnWolfET_DecalProjectorPlanes(&dp, corners, direction, footprint));
	{
		/* A floor facing the projector takes a textured, fully coloured copy. */
		const float a[3] = { 2, 2, -4 }, b[3] = { 2, 8, -4 }, c[3] = { 8, 8, -4 };
		assert(DecalOnto(&dp, a, b, c, &decal));
		assert(decal.inUse && decal.shader == 7 && decal.numVerts == 3);
		Near(decal.verts[0].st[0], 0.2f); Near(decal.verts[0].st[1], 0.2f);
		Near(decal.verts[2].st[0], 0.8f); Near(decal.verts[2].st[1], 0.8f);
		assert(decal.verts[0].modulate[0] == 255 && decal.verts[0].modulate[3] == 255);
		/* Facing away, it takes nothing. */
		assert(!DecalOnto(&dp, a, c, b, &decal));
	}
	{
		/* Over the back half of its depth the projector fades out. */
		const float a[3] = { 2, 2, -12 }, b[3] = { 2, 8, -12 }, c[3] = { 8, 8, -12 };
		assert(DecalOnto(&dp, a, b, c, &decal));
		assert(decal.verts[0].modulate[3] == 127);
	}
	{
		/* A polygon reaching past a side is clipped to it. */
		const float a[3] = { 5, 2, -4 }, b[3] = { 5, 8, -4 }, c[3] = { 15, 8, -4 };
		assert(DecalOnto(&dp, a, b, c, &decal));
		for (i = 0; i < decal.numVerts; ++i) assert(decal.verts[i].xyz[0] <= 10.001f);
		assert(decal.numVerts >= 3);
	}
	{
		/* Wholly outside, nothing. */
		const float a[3] = { 20, 2, -4 }, b[3] = { 20, 8, -4 }, c[3] = { 28, 8, -4 };
		assert(!DecalOnto(&dp, a, b, c, &decal));
	}
	/* A sphere: textured flat along the axis a surface faces most, and faded
	 * with the surface's distance from its centre. */
	{
		const float point[1][3] = { { 0, 0, 0 } }, sphere[4] = { 0, 0, 0, 8 };
		const float a[3] = { -4, -4, 4 }, b[3] = { -4, 4, 4 }, c[3] = { 4, 4, 4 };
		assert(FnWolfET_DecalProjectorStart(3, 1, point, sphere, white, 1000, 0, 0, &dp, corners, direction, &footprint));
		assert(dp.omnidirectional && dp.numPlanes == 6 && footprint == 4);
		assert(FnWolfET_DecalProjectorPlanes(&dp, corners, direction, footprint));
		assert(DecalOnto(&dp, a, b, c, &decal));
		Near(decal.verts[0].st[0], 0.25f); Near(decal.verts[0].st[1], 0.25f);
		Near(decal.verts[2].st[0], 0.75f); Near(decal.verts[2].st[1], 0.75f);
		assert(decal.verts[0].modulate[3] == (unsigned char)(int)((1.0f - 4.0f / dp.radius) * 255));
	}
	/* Ageing compounds, as ET's does, and frees the slot at the end. */
	memset(&decal, 0, sizeof(decal));
	decal.inUse = 1; decal.numVerts = 1; decal.fadeStartTime = 100; decal.fadeEndTime = 200;
	memset(decal.verts[0].modulate, 200, 4);
	assert(!FnWolfET_DecalAge(&decal, 150) && decal.verts[0].modulate[0] == 100);
	assert(!FnWolfET_DecalAge(&decal, 175) && decal.verts[0].modulate[0] == 25);
	assert(FnWolfET_DecalAge(&decal, 200) && !decal.inUse);
	/* A new decal takes the first free slot, else the one fading soonest. */
	memset(slots, 0, sizeof(slots));
	for (i = 0; i < 3; ++i) slots[i].inUse = 1;
	slots[0].fadeEndTime = 300; slots[1].fadeEndTime = 100; slots[2].fadeEndTime = 200;
	assert(FnWolfET_DecalSlot(slots, 3) == 1);
	slots[2].inUse = 0;
	assert(FnWolfET_DecalSlot(slots, 3) == 2);
	/* Touching tests are strict. */
	memset(&dp, 0, sizeof(dp));
	dp.radius = 2; dp.radius2 = 4;
	{
		const float far[3] = { 3, 0, 0 }, close[3] = { 2, 0, 0 };
		const float mins[3] = { 2, -1, -1 }, maxs[3] = { 3, 1, 1 }, nearMins[3] = { 1.9f, -1, -1 };
		assert(!FnWolfET_DecalTouchesSphere(&dp, far, 1) && FnWolfET_DecalTouchesSphere(&dp, close, 1));
		assert(!FnWolfET_DecalTouchesBox(&dp, mins, maxs) && FnWolfET_DecalTouchesBox(&dp, nearMins, maxs));
	}
}

int main(void) {
	Foliage();
	Lightmaps();
	Decals();
	return 0;
}
'''

CPP_CHECKS = r'''
#include "code/compat/wolfet_gameinfo.hpp"
#include "code/compat/rtcw_match_state.hpp"
#include <cassert>
#include <cstring>
int main() {
    using namespace fntech3::wolfet::gameinfo;
    // Retail's gameinfo.dat, byte for byte.
    const char retail[] = "//spEnabled\r\nspGameTypes 0\r\ndefaultSPGameType 0\r\ncoopGameTypes 1\r\n"
        "defaultCoopGameType 1\r\ndefaultGameType 4\r\nusesProfiles 1\r\n";
    GameInfo info = Parse(retail, std::strlen(retail));
    assert(!info.spEnabled && info.spGameTypes == 1u && info.defaultSPGameType == 0);
    assert(info.coopGameTypes == 2u && info.defaultCoopGameType == 1 && info.defaultGameType == 4);
    assert(info.usesProfiles == 1);
    assert(IsSinglePlayer(info, 0) && !IsSinglePlayer(info, 2) && !IsSinglePlayer(info, 4));
    assert(IsCoop(info, 1) && !IsCoop(info, 0) && !IsSinglePlayer(info, -1) && !IsSinglePlayer(info, 40));
    // Enabled, several types on one line, block comments, and a value only on
    // its keyword's own line.
    const char custom[] = "spEnabled\n/* a note\n spGameTypes 9 */ spGameTypes 0 3\ndefaultSPGameType\n5\n";
    info = Parse(custom, std::strlen(custom));
    assert(info.spEnabled && info.spGameTypes == ((1u << 0) | (1u << 3)) && info.defaultSPGameType == 0);
    // No file: nothing is single player and nothing is enabled.
    info = Parse(nullptr, 0);
    assert(!info.spEnabled && !info.spGameTypes && !info.coopGameTypes);

    // ET's restart keeps RTCW multiplayer's transitions. From intermission a
    // multiplayer restart goes to warmup; a single-player or cooperative one
    // is played straight through, which intermission refuses.
    int result = 77;
    assert(fntech3::rtcw::RestartMatchState(FNTECH3_RTCW_MP_GS_INTERMISSION, FNTECH3_RTCW_MP_GS_PLAYING,
        false, result) && result == FNTECH3_RTCW_MP_GS_WARMUP);
    result = 77;
    assert(!fntech3::rtcw::RestartMatchState(FNTECH3_RTCW_MP_GS_INTERMISSION, FNTECH3_RTCW_MP_GS_PLAYING,
        false, result, false) && result == 77);
    assert(fntech3::rtcw::RestartMatchState(FNTECH3_RTCW_MP_GS_PLAYING, FNTECH3_RTCW_MP_GS_RESET,
        false, result, false) && result == FNTECH3_RTCW_MP_GS_WARMUP);
    assert(fntech3::rtcw::RestartMatchState(FNTECH3_RTCW_MP_GS_WARMUP, FNTECH3_RTCW_MP_GS_PLAYING,
        false, result, false) && result == FNTECH3_RTCW_MP_GS_PLAYING);
    return 0;
}
'''


def c_fixture():
    math = read('code/qcommon/q_math.c')
    source = HOST
    for signature in ('vec_t VectorNormalize( vec3_t v ) {', 'qboolean PlaneFromPoints('):
        source += function(math, signature)
    return source + C_CHECKS


def build_and_run(test, name, source, extension):
    compiler = shutil.which('clang++') or shutil.which('g++')
    if not compiler:
        test.skipTest('C++17 compiler unavailable')
    scratch = ROOT/'.tmp/wolfet-map-load-tests'
    scratch.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=scratch) as directory:
        folder = Path(directory)
        path = folder/(name + extension)
        executable = folder/(name + ('.exe' if os.name == 'nt' else ''))
        path.write_text(source, encoding='utf-8')
        target = compiler_target_args(compiler)
        if os.name == 'nt' and not target:
            target = ['-DFNTECH3_Q3_ONLY']
        elif os.name != 'nt':
            target = ['-m32']
        language = ['-x', 'c++'] if extension == '.cpp' else ['-x', 'c++']
        built = subprocess.run([compiler, *target, *language, '-std=c++17', '-I', str(ROOT), str(path),
                                '-o', str(executable)], cwd=ROOT, capture_output=True, text=True, timeout=180)
        test.assertEqual(built.returncode, 0, built.stdout + built.stderr)
        run = subprocess.run([str(executable)], cwd=ROOT, capture_output=True, text=True, timeout=30)
        test.assertEqual(run.returncode, 0, run.stdout + run.stderr)


class WolfETMapArithmeticTests(unittest.TestCase):
    def test_foliage_lightmaps_and_decals(self):
        build_and_run(self, 'arithmetic', c_fixture(), '.cpp')

    def test_gameinfo_and_restart_rules(self):
        build_and_run(self, 'rules', CPP_CHECKS, '.cpp')


class WolfETRendererWiringTests(unittest.TestCase):
    def test_et_surface_type_ordinals_and_dispatch_order_are_preserved(self):
        for renderer in RENDERERS:
            with self.subTest(renderer=renderer):
                local = read(f'code/{renderer}/tr_local.h')
                types = local[local.index('SF_BAD'):local.index('SF_NUM_SURFACE_TYPES')]
                order = re.findall(r'^\s*(SF_\w+)\s*,', types, re.M)
                self.assertEqual(order[13:16], ['SF_WOLFET', 'SF_WOLFET_FOLIAGE', 'SF_WOLFET_DECAL'])
                table = read(f'code/{renderer}/tr_surface.c')
                order = [table.index(entry) for entry in (
                    'RB_SurfaceWolfET,', 'RB_SurfaceWolfETFoliage,', 'RB_SurfaceWolfETDecal')]
                self.assertEqual(order, sorted(order))

    def test_foliage_is_loaded_culled_and_only_for_et(self):
        for renderer in RENDERERS:
            with self.subTest(renderer=renderer):
                bsp = read(f'code/{renderer}/tr_bsp.c')
                case = bsp[bsp.index('case MST_FOLIAGE:'):]
                case = case[:case.index('break;')]
                self.assertIn('ri.gameId != COM_GAME_WOLFET', case)
                self.assertIn('ParseWolfETFoliage(', case)
                world = read(f'code/{renderer}/tr_world.c')
                self.assertIn('R_CullWolfETFoliage( (const srfWolfETFoliage_t *)surface )', world)
                init = read(f'code/{renderer}/tr_init.c')
                self.assertIn('r_drawfoliage = ri.gameId == COM_GAME_WOLFET ? ri.Cvar_Get( "r_drawfoliage", "1", CVAR_CHEAT ) : NULL;', init)
        self.assertIn('MST_FOLIAGE', read('code/qcommon/qfiles.h'))

    def test_external_lightmaps_load_and_a_missing_one_falls_back_to_vertex_light(self):
        for renderer in RENDERERS:
            with self.subTest(renderer=renderer):
                bsp = read(f'code/{renderer}/tr_bsp.c')
                self.assertLess(bsp.index('R_LoadLightmaps( &header->lumps[LUMP_LIGHTMAPS] );'),
                                bsp.index('R_LoadWolfETExternalLightmaps( &header->lumps[LUMP_SURFACES] );'))
                shader = read(f'code/{renderer}/tr_shader.c')
                self.assertIn('( lightmapIndex >= tr.numLightmaps || !tr.lightmaps[lightmapIndex] )', shader)
        loader = read('code/renderercommon/tr_wolfet_lightmap_load.h')
        self.assertIn('ri.gameId != COM_GAME_WOLFET || tr.numLightmaps', loader)
        self.assertIn('R_ColorShiftLightingBytes( texel, texel, qfalse );', loader)

    def test_large_faces_and_lit_triangle_soups_are_et_only(self):
        for renderer in RENDERERS:
            with self.subTest(renderer=renderer):
                bsp = read(f'code/{renderer}/tr_bsp.c')
                self.assertIn('maxPoints = ri.gameId == COM_GAME_WOLFET ? SHADER_MAX_VERTEXES : MAX_FACE_POINTS;', bsp)
                self.assertIn('ri.gameId == COM_GAME_WOLFET ? lightmapNum : LIGHTMAP_BY_VERTEX', bsp)
                self.assertIn('ri.gameId == COM_GAME_WOLFET ) tr.sunShader = NULL;', bsp)

    def test_the_shader_dialect_is_read_for_et_alone(self):
        for renderer in RENDERERS:
            with self.subTest(renderer=renderer):
                shader = read(f'code/{renderer}/tr_shader.c')
                self.assertIn('ri.gameId == COM_GAME_WOLFET && !Q_stricmpn( token, "implicit", 8 )', shader)
                self.assertIn('ri.gameId == COM_GAME_WOLFET && !Q_stricmp( token, "distancecull" )', shader)
                self.assertIn('ri.gameId == COM_GAME_WOLFET && !Q_stricmp( token, "lightmap" )', shader)
                self.assertEqual(shader.count('R_WolfETFixShaderName( strippedName );'), 2)
                self.assertIn('R_CreateWolfETDefaultShading( image );', shader)
                self.assertIn("!(shader.contentFlags & CONTENTS_FOG ) && !s_wolfetImplicitMap[0] )", shader)
                self.assertIn('( ri.gameId == COM_GAME_RTCW_MP || ri.gameId == COM_GAME_WOLFET ) && !Q_stricmp( token, "nomipmap" )', shader)
                self.assertIn('ri.gameId == COM_GAME_WOLFET ? NULL : R_FindShader( "sun", LIGHTMAP_NONE, qtrue )', shader)
        dialect = read('code/renderercommon/tr_wolfet_shader.h')
        # implicitMask and implicitBlend draw two sided and blend or mask.
        self.assertIn('GLS_DEPTHMASK_TRUE | GLS_ATEST_GE_80', dialect)
        self.assertIn('GLS_DEPTHMASK_TRUE | GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA', dialect)
        self.assertIn('GLS_DEFAULT | GLS_SRCBLEND_DST_COLOR | GLS_DSTBLEND_ZERO | GLS_DEPTHFUNC_EQUAL', dialect)
        material = read('code/renderercommon/tr_rtcw_shader_material.h')
        self.assertIn('ri.gameId == COM_GAME_WOLFET ? qtrue : qfalse;', material)
        fog = read('code/renderercommon/tr_rtcw_shader_fog.h')
        self.assertIn('if ( wolfet ) VectorScale( color, tr.identityLight, color );', fog)
        self.assertIn('profile != COM_GAME_WOLFET', read('code/renderercommon/tr_rtcw_world_shader.h'))

    def test_models_skins_coronas_and_diagnostics(self):
        for renderer in RENDERERS:
            with self.subTest(renderer=renderer):
                model = read(f'code/{renderer}/tr_model.c')
                block = model[model.index('if ( ri.gameId == COM_GAME_WOLFET && ( !Q_stricmp( ext, "md3" )'):]
                self.assertLess(block.index('".mdc"'), block.index('".md3"'))
                backend = read(f'code/{renderer}/tr_backend.c')
                self.assertLess(backend.index('RB_WolfETGlobalFogClearColor('),
                                backend.index('backEnd.refdef.rtcwFog.fogEnabled &&'))
        skin = read('code/renderercommon/tr_skin.h')
        self.assertIn('wolfet && !Q_stricmpn( token, "md3_", 4 )', skin)
        self.assertIn('wolfet ? !Q_stricmpn( token, "tag_", 4 )', skin)
        self.assertIn('skinFile = wolfet ||', skin)
        corona = read('code/renderercommon/tr_rtcw_corona.h')
        self.assertIn('profile == COM_GAME_WOLFET', corona)
        backend = read('code/renderercommon/tr_rtcw_corona_backend.h')
        self.assertIn('FnRTCWCoronaVisible( ri.gameId != COM_GAME_RTCW_SP, fade->corona.flags )', backend)
        self.assertIn('ri.gameId != COM_GAME_WOLFET', read('code/renderercommon/tr_image_tga.c'))
        self.assertIn('if ( !Com_GameIsWolfET() ) {', read('code/qcommon/cm_patch.c'))

    def test_decals_are_pressed_kept_and_drawn(self):
        for renderer in RENDERERS:
            with self.subTest(renderer=renderer):
                world = read(f'code/{renderer}/tr_world.c')
                self.assertIn('tr.numWolfETDecalProjectors && tr.currentEntityNum == REFENTITYNUM_WORLD', world)
                self.assertIn('R_AddWolfETDecalSurfaces( tr.world->bmodels );', world)
                self.assertEqual(world.count('R_AddWolfETDecalSurfaces( bmodel );'), 2)
                self.assertLess(world.index('R_CullWolfETDecalProjectors();'),
                                world.index('if ( !r_drawworld->integer ) {'))
                scene = read(f'code/{renderer}/tr_scene.c')
                self.assertIn('R_CopyWolfETDecalProjectorsToScene();', scene)
                self.assertIn('R_ResetWolfETDecalSurfaces();', scene)
                bsp = read(f'code/{renderer}/tr_bsp.c')
                self.assertIn('i == 0 ? FN_WOLFET_MAX_WORLD_DECALS : FN_WOLFET_MAX_ENTITY_DECALS', bsp)
        client = read('code/client/client_wolfet_render_compat.hpp')
        self.assertIn('re.ProjectWolfETDecal(', client)
        self.assertIn('re.ClearWolfETDecals();', client)
        scene = read('code/renderercommon/tr_wolfet_decal_scene.h')
        self.assertIn('ET decal projected outside its game profile', scene)


class WolfETEngineWiringTests(unittest.TestCase):
    def test_the_map_command_reaches_the_game_intact(self):
        spawn = read('code/server/sv_init.cpp')
        body = spawn[spawn.index('void SV_SpawnServer( const char *mapname, qboolean killBots ) {'):]
        self.assertLess(body.index('Q_strncpyz( spawnCommand, Cmd_Cmd(), sizeof( spawnCommand ) );'),
                        body.index('CL_MapLoading();'))
        self.assertLess(body.index('FS_PureServerSetReferencedPaks( "", "" );'),
                        body.index('Cmd_TokenizeString( spawnCommand );'))
        self.assertLess(body.index('Cmd_TokenizeString( spawnCommand );'), body.index('SV_InitGameProgs();'))

    def test_et_game_types_follow_its_gameinfo(self):
        main = read('code/server/sv_main.cpp')
        self.assertEqual(main.count('SV_GameTypeIsSinglePlayer( Cvar_VariableIntegerValue( "g_gametype" ) )'), 2)
        self.assertIn('SV_GameTypeIsSinglePlayer( Cvar_VariableIntegerValue( "g_gametype" ) )',
                      read('code/server/sv_client.cpp'))
        commands = read('code/server/sv_ccmds.cpp')
        self.assertIn('if( !Com_GameIsWolfET() && sv_gametype->integer == GT_SINGLE_PLAYER ) {', commands)
        self.assertIn('singlePlayerCommand && Com_GameIsWolfET() && !SV_WolfETSinglePlayerEnabled()', commands)
        self.assertIn('Cvar_Set("g_currentRound", "0");', commands)
        self.assertIn('Cvar_Set("g_nextTimeLimit", "0");', commands)
        self.assertIn('false, nextState, !solo) ) return;', commands)
        spawn = read('code/server/sv_init.cpp')
        self.assertLess(spawn.index('FS_Restart( sv.checksumFeed );'), spawn.index('SV_WolfETLoadGameInfo();'))
        self.assertIn('Cvar_Set( "sv_serverRestarting", "1" );', spawn)
        self.assertIn('Cvar_Set( "sv_serverRestarting", "0" );', spawn)

    def test_client_art_sound_and_memory(self):
        client = read('code/client/cl_main.cpp')
        for name in ('"gfx/2d/consolechars"', '"console-16bit"', '"ui/assets/3_cursor3"'):
            self.assertIn(name, client)
        self.assertIn('cls.recordShader = wolfet || Com_GameIsSTVEF() ? 0 :', client)
        self.assertRegex(read('code/client/cl_scrn.cpp'),
                         r'Com_GameIsWolfET\(\)[^;]*\? 0\s*: re\.RegisterShaderNoMip\( "ui/assets/backscreen_smoke" \)')
        sound = read('code/client/audio/snd_main.cpp')
        volume = sound[sound.index('static int S_WolfETVolume( int volume ) {'):]
        volume = volume[:volume.index('\n}')]
        self.assertNotIn('Com_Error', volume)
        common = read('code/qcommon/common.c')
        self.assertIn('#define MIN_WOLFET_COMHUNKMEGS\t128', common)
        self.assertIn('Cvar_Set2( "com_hunkMegs", XSTRING( MIN_WOLFET_COMHUNKMEGS ), qtrue );', common)


class WolfETMapCheckerTests(unittest.TestCase):
    def test_content_is_read_from_the_installation_itself(self):
        import probe_wolfet_maps as checker
        with tempfile.TemporaryDirectory() as tmp:
            game = Path(tmp)/'etmain'
            game.mkdir()
            with zipfile.ZipFile(game/'pak0.pk3', 'w') as pak:
                pak.writestr('models/m.mdc', b'm')
                pak.writestr('sound/s.wav', b's')
                pak.writestr('textures/t.jpg', b't')
                pak.writestr('scripts/x.shader', 'textures/foo/bar\n{\n\timplicitMap -\n}\n')
            content = checker.Content(Path(tmp))
            self.assertTrue(content.ships('model', 'models/m.md3'))
            self.assertTrue(content.ships('image', 'textures/t.tga'))
            self.assertTrue(content.ships('shader', 'textures/foo/bar'))
            self.assertTrue(content.ships('image', 'textures\\foo\\bar.tga'))
            self.assertTrue(content.ships('sound', 'sound/s.wav'))
            self.assertFalse(content.ships('sound', 'sound/x.wav'))
            self.assertFalse(content.ships('model', 'models/absent.md3'))

    def test_every_line_is_sorted_and_only_engine_issues_fail(self):
        import probe_wolfet_maps as checker

        class Content:
            @staticmethod
            def ships(kind, name):
                return name == 'models/shipped.md3'

        text = '\n'.join((
            "^5^3R_RegisterMD3: couldn't load models/shipped.md3",
            "^5^3R_RegisterMD3: couldn't load models/absent.md3",
            "^5^3WARNING: couldn't load sound: sound/absent.wav",
            "^5WARNING: shader 'x' has lightmap but no lightmap stage!",
            "^1Warning: setstate called and no entities found",
            'WARNING: NET_IP6Socket: bind: WSAEADDRNOTAVAIL',
            'Parsing menu file: ui/hostgame_dedicated_warning.menu',
            'Audio zones: 12 loaded from maps/oasis.azb',
            'ERROR: something stopped',
            '^3WARNING: something new',
        ))
        result = checker.classify(text, Content())
        self.assertEqual(sorted(result['engine']), ['engine-error', 'missing-model-shipped', 'other-warning'])
        self.assertEqual(result['absent'], {'model': ['models/absent.md3'], 'sound': ['sound/absent.wav']})
        self.assertEqual(len(result['retailIdentical']), 2)
        self.assertEqual(len(result['environment']), 1)


class WolfETMapLoadRecordTests(unittest.TestCase):
    def test_the_record_states_what_is_and_is_not_claimed(self):
        record = RECORD.read_text(encoding='utf-8')
        self.assertIn('All six official maps load, draw and quit cleanly on every renderer.', record)
        self.assertIn('No gameplay is claimed.', record)
        self.assertIn('ray-traced mode off', record)
        self.assertIn('no implementation text from any of them is incorporated', record)


if __name__ == '__main__':
    unittest.main()
