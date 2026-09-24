"""Alice's patch collision: which patches collide, and how finely they are cut.

Retail alice.exe's collision loader (CMod_LoadPatches, 0x00430683..0x004307a9)
builds nothing for a patch whose shader's contents have neither bit 0 nor any
of bits 16-19 (solid and the four clips, 0x004306a5..0x004306bc), and
subdivides every other patch until no curve midpoint is as far from its chord
as the shader's own subdivision count, raised to 16 when smaller
(0x00430780..0x0043078c; the comparison is >=, CM_NeedsSubdivision
0x00432877). Quake III subdivides every patch at 16. Its CM_AddFacetBevels
reports an invalid bevel only at developer 2 (0x004320b4..0x004320c4).

These fixtures compile the engine's own patch loader and patch generator,
sliced out of code/qcommon/cm_load.c and code/qcommon/cm_patch.c, and feed them
synthetic patches; no retail map is read. See
docs/fntech3/2026-09-22-alice-maps.md.
"""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]


def function(source, signature):
    start = source.index(signature)
    return source[start:source.index('\n}', start) + 2]


def generator_source():
    """cm_patch.c from its own includes to the end of CM_GeneratePatchCollide."""
    text = (ROOT / 'code/qcommon/cm_patch.c').read_text(encoding='utf-8')
    start = text.index('#include "cm_patch.h"\n') + len('#include "cm_patch.h"\n')
    end = text.index('\n}\n', text.index('struct patchCollide_s *CM_GeneratePatchCollide(')) + 3
    return text[start:end]


GENERATOR_PRELUDE = r'''
#include "code/qcommon/cm_local.h"
#include "code/qcommon/cm_patch.h"
#include <assert.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
static cvar_t developer;
cvar_t *com_developer = &developer;
static int game, reports;
int Com_GameIsAlice( void ) { return game == 1; }
int Com_GameIsWolfET( void ) { return game == 2; }
void QDECL Com_Printf( const char *fmt, ... ) { if ( strstr( fmt, "invalid bevel" ) ) reports++; }
void QDECL Com_DPrintf( const char *fmt, ... ) {
	if ( com_developer->integer && strstr( fmt, "invalid bevel" ) ) reports++;
}
void QDECL Com_Error( errorParm_t level, const char *fmt, ... ) { (void)level; (void)fmt; abort(); }
void *Hunk_Alloc( int size, ha_pref preference ) { (void)preference; return calloc( 1, size ? size : 1 ); }
void *Z_Malloc( int size ) { return calloc( 1, size ); }
void Z_Free( void *pointer ) { free( pointer ); }
'''


GENERATOR_CHECKS = r'''
/* A 3x3 patch curved along one axis only: its middle column stands 64 units
 * above the other two, so the curve's midpoint is 32 units from its chord. */
static int ArchFacets( float subdivisions ) {
	vec3_t points[9];
	int i, j;
	for ( j = 0; j < 3; j++ ) {
		for ( i = 0; i < 3; i++ ) {
			VectorSet( points[j * 3 + i], i * 32.0f, j * 32.0f, i == 1 ? 64.0f : 0.0f );
		}
	}
	return CM_GeneratePatchCollide( 3, 3, points, subdivisions )->numFacets;
}

/* Two twisted patches whose facets each lose an edge bevel to the chop, the
 * case the report is about. */
static const float twisted[2][9][3] = {
	{ { -3, 4, 14 }, { 16, 3, 24 }, { 124, -1, 11 }, { -2, 36, 7 }, { 11, 6, 26 },
	  { 129, 60, 20 }, { 0, 126, -19 }, { 12, 36, 30 }, { 34, 65, 18 } },
	{ { 4, 2, 25 }, { 20, 2, -33 }, { 129, 4, 12 }, { 1, 65, -40 }, { 35, 7, -18 },
	  { 29, 28, -31 }, { -4, 124, -5 }, { 16, 14, 4 }, { 61, 30, -8 } },
};

static int Reports( int selectedGame, int developerLevel ) {
	vec3_t points[9];
	int p, i;
	game = selectedGame;
	developer.integer = developerLevel;
	reports = 0;
	for ( p = 0; p < 2; p++ ) {
		for ( i = 0; i < 9; i++ ) VectorCopy( twisted[p][i], points[i] );
		CM_GeneratePatchCollide( 3, 3, points, SUBDIVIDE_DISTANCE );
	}
	return reports;
}

int main( void ) {
	/* Quake III's limit is the one Alice raises smaller counts to. */
	assert( SUBDIVIDE_DISTANCE == 16 );
	assert( ArchFacets( SUBDIVIDE_DISTANCE ) == 2 );
	/* The midpoint is exactly 32 away: 32 still cuts it, anything more
	 * leaves the arch one flat facet, as Alice's large counts do. */
	assert( ArchFacets( 32.0f ) == 2 );
	assert( ArchFacets( 32.1f ) == 1 );
	assert( ArchFacets( 350.0f ) == 1 );
	/* A limit under 16 would cut finer; the loader never passes one. */
	assert( ArchFacets( 4.0f ) == 4 );

	/* The invalid-bevel report: Quake III's at developer 1, none for
	 * Enemy Territory, and Alice's only at developer 2. */
	assert( Reports( 0, 0 ) == 0 );
	assert( Reports( 0, 1 ) > 0 );
	assert( Reports( 2, 2 ) == 0 );
	assert( Reports( 1, 1 ) == 0 );
	assert( Reports( 1, 2 ) == Reports( 0, 1 ) );
	return 0;
}
'''


LOADER_PRELUDE = r'''
#include "code/qcommon/cm_local.h"
#include "code/qcommon/cm_patch.h"
#include "code/qcommon/bsp_fakk.h"
#include <assert.h>
#include <setjmp.h>
#include <stdlib.h>
#include <string.h>
#define MAX_PATCH_VERTS 1024
clipMap_t cm;
static byte *cmod_base;
static jmp_buf errors;
static int generated, allocations;
static float seen[16];
static patchCollide_t collide;
struct patchCollide_s *CM_GeneratePatchCollide( int width, int height, vec3_t *points, float subdivisions ) {
	(void)width; (void)height; (void)points;
	seen[generated++] = subdivisions;
	return &collide;
}
void QDECL Com_Error( errorParm_t level, const char *fmt, ... ) { (void)level; (void)fmt; longjmp( errors, 1 ); }
void *Hunk_Alloc( int size, ha_pref preference ) {
	(void)preference; allocations++; return calloc( 1, size ? size : 1 );
}
'''


LOADER_CHECKS = r'''
/* Six shaders around the mask's edges: solid, water, the player clip, the
 * weapon clip, the shootable-only bit just above the clips, and nothing. */
static const int contents[6] = { 0x1, 0x20, 0x10000, 0x80000, 0x100000, 0 };
static const int32_t subdivisions[6] = { 4, 64, 100, 16, 200, -5 };
static dshader_t shaders[6];
static struct { dsurface_t surfaces[7]; drawVert_t verts[9]; } map;

static void Reset( int shaderNum ) {
	int i;
	memset( &cm, 0, sizeof( cm ) );
	memset( &map, 0, sizeof( map ) );
	for ( i = 0; i < 6; i++ ) shaders[i].contentFlags = contents[i];
	cm.shaders = shaders;
	cm.numShaders = 6;
	for ( i = 0; i < 6; i++ ) {
		map.surfaces[i].surfaceType = MST_PATCH;
		map.surfaces[i].shaderNum = i;
		map.surfaces[i].patchWidth = 3;
		map.surfaces[i].patchHeight = 3;
	}
	map.surfaces[5].shaderNum = shaderNum;
	map.surfaces[6].surfaceType = MST_PLANAR;
	cmod_base = (byte *)&map;
	generated = allocations = 0;
}

static void Load( const fakk_translation_t *fakk ) {
	const lump_t surfs = { 0, (int)sizeof( map.surfaces ) };
	const lump_t verts = { (int)sizeof( map.surfaces ), (int)sizeof( map.verts ) };
	CMod_LoadPatches( &surfs, &verts, fakk );
}

int main( void ) {
	fakk_translation_t fakk;
	int i;

	memset( &fakk, 0, sizeof( fakk ) );
	fakk.shaderSubdivisions = subdivisions;
	fakk.numShaderSubdivisions = 6;
	assert( FAKK_PATCH_COLLISION_CONTENTS == 0x000f0001 && FAKK_PATCH_MIN_SUBDIVISIONS == 16 );

	/* Alice: water, shootable-only and empty patches get no collision, and
	 * the rest are cut by their shader's count, never under 16. */
	Reset( 5 );
	if ( setjmp( errors ) ) assert( 0 );
	Load( &fakk );
	assert( cm.numSurfaces == 7 && generated == 3 );
	assert( cm.surfaces[0] && !cm.surfaces[1] && cm.surfaces[2] && cm.surfaces[3] );
	assert( !cm.surfaces[4] && !cm.surfaces[5] && !cm.surfaces[6] );
	assert( seen[0] == 16.0f && seen[1] == 100.0f && seen[2] == 16.0f );
	assert( cm.surfaces[2]->contents == 0x10000 && cm.surfaces[3]->contents == 0x80000 );

	/* A table shorter than the shaders is read as no count at all. */
	Reset( 5 );
	fakk.numShaderSubdivisions = 2;
	if ( setjmp( errors ) ) assert( 0 );
	Load( &fakk );
	assert( generated == 3 && seen[1] == 16.0f );
	fakk.numShaderSubdivisions = 6;

	/* Every other map loads every patch at Quake III's 16. */
	Reset( 5 );
	if ( setjmp( errors ) ) assert( 0 );
	Load( NULL );
	assert( generated == 6 );
	for ( i = 0; i < 6; i++ ) assert( cm.surfaces[i] && seen[i] == 16.0f );
	assert( !cm.surfaces[6] );

	/* A shader number outside the table is refused before the mask reads it,
	 * with the Alice rules or without. */
	for ( i = 0; i < 2; i++ ) {
		const int bad = i ? 6 : -1;
		Reset( bad );
		if ( !setjmp( errors ) ) { Load( &fakk ); assert( 0 ); }
		assert( generated == 3 );
		Reset( bad );
		if ( !setjmp( errors ) ) { Load( NULL ); assert( 0 ); }
		assert( generated == 5 );
	}
	return 0;
}
'''


def build_and_run(test, name, sources):
    cc = shutil.which('clang')
    if not cc:
        test.skipTest('Clang C compiler unavailable')
    target = compiler_target_args(cc)
    abi = (['-DFNTECH3_Q3_ONLY'] if not target else []) if os.name == 'nt' else ['-m32']
    with tempfile.TemporaryDirectory() as directory:
        paths = []
        for index, text in enumerate(sources):
            if isinstance(text, Path):
                paths.append(str(text))
                continue
            path = Path(directory) / f'{name}{index}.c'
            path.write_text(text, encoding='utf-8')
            paths.append(str(path))
        exe = Path(directory) / f'{name}.exe'
        command = [cc, *target, *abi, '-std=c11', '-I', str(ROOT), '-I', str(ROOT / 'code/qcommon'),
                   *paths, '-o', str(exe)]
        if os.name != 'nt':
            command.append('-lm')
        run = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, timeout=120)
        test.assertEqual(run.returncode, 0, run.stdout + run.stderr)
        options = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
        run = subprocess.run([str(exe)], cwd=directory, capture_output=True, text=True, timeout=30, **options)
        test.assertEqual(run.returncode, 0, run.stdout + run.stderr)


class AlicePatchCollisionTests(unittest.TestCase):
    def test_patch_generator_takes_the_limit_and_reports_like_each_engine(self):
        # not "patch": Windows asks to elevate an executable named like one
        build_and_run(self, 'facets', [
            GENERATOR_PRELUDE + generator_source() + GENERATOR_CHECKS,
            ROOT / 'code/qcommon/cm_polylib.c',
            ROOT / 'code/qcommon/q_math.c',
        ])

    def test_alice_loader_skips_non_colliding_patches_and_uses_shader_counts(self):
        load = (ROOT / 'code/qcommon/cm_load.c').read_text(encoding='utf-8')
        build_and_run(self, 'loader', [
            LOADER_PRELUDE + function(load, 'static void CMod_LoadPatches(') + LOADER_CHECKS,
        ])

    def test_collision_loader_asks_for_the_counts_only_for_alice_maps(self):
        load = (ROOT / 'code/qcommon/cm_load.c').read_text(encoding='utf-8')
        loader = function(load, 'void CM_LoadMap(')
        self.assertIn('CMod_LoadPatches( &header.lumps[LUMP_SURFACES], &header.lumps[LUMP_DRAWVERTS],\n'
                      '\t\tbspFormat == COM_GAME_BSP_FAKK ? &fakkTranslated : NULL );', loader)
        # the table lives in the translation, so it must be read before the
        # translation is freed
        self.assertLess(loader.index('CMod_LoadPatches('), loader.index('Z_Free( fakkTranslated.data );'))
        fakk = (ROOT / 'code/qcommon/bsp_fakk.h').read_text(encoding='utf-8')
        collision = function(fakk, 'static qboolean FAKK_TranslateToV46(\n')
        self.assertIn('FAKK_TRANSLATE_SHADER_SUBDIVISIONS', collision)
        # the renderers ask for the light grid alone
        for renderer in ('renderer', 'renderervk', 'rendererrtx'):
            bsp = (ROOT / 'code' / renderer / 'tr_bsp.c').read_text(encoding='latin-1')
            self.assertNotIn('FAKK_TRANSLATE_SHADER_SUBDIVISIONS', bsp)


if __name__ == '__main__':
    unittest.main()
