"""Medal of Honor's skinned model surfaces are skinned once a frame.

The back end skinned a TIKI surface on the CPU for the view, again for each
dynamic light's lit pass, and again for every face of every dynamic light whose
shadow it casts: m1l1's convoy trucks in their headlights, twenty times a
frame. code/renderercommon/tr_mohaa_skin_cache.h keeps each surface skinned by
this frame's pose and copies it back for the frame's later draws; the pose
pool's generation drops every copy when the pool starts over.

The cache is compiled against stand-in types; the three renderers' surface
functions must copy from it before skinning and keep what they skin.
"""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]
RENDERERS = ('renderer', 'renderervk', 'rendererrtx')

HOST = r'''
#include <cassert>
#include <cstddef>
#include <cstdio>
#include <cstring>
typedef int qboolean;
enum { qfalse, qtrue };
typedef float vec2_t[2];
typedef float vec4_t[4];
#define ID_INLINE inline
#define Com_Memcpy memcpy
#include "code/renderercommon/tr_mohaa_skin_cache.h"

static mohaaSkinCache_t cache;
static vec4_t xyz[4096], normal[4096];
static vec2_t st[4096];
static unsigned int indexes[12288];
static float bones[2][24];
static int surfaces[2048];

/* a surface "skinned" at baseVertex: three vertices and a triangle */
static void Skin( int base, float tag ) {
	for ( int i = 0; i < 3; i++ ) {
		xyz[base + i][0] = tag; xyz[base + i][1] = (float)i; xyz[base + i][2] = 0; xyz[base + i][3] = 1;
		normal[base + i][0] = 0; normal[base + i][1] = 0; normal[base + i][2] = 1; normal[base + i][3] = 0;
		st[base + i][0] = tag; st[base + i][1] = (float)i;
	}
	for ( int i = 0; i < 3; i++ ) indexes[base + i] = (unsigned int)( base + 2 - i );
}

int main() {
	int n = 0;
	const unsigned int gen = 1;
	/* nothing kept yet: zeroed entries are free, never generation 1 */
	assert( R_MOHAASkinCacheCopy( &cache, gen, bones[0], &surfaces[0], 1, -1, 0, xyz, normal, st, indexes,
		0, 4096, 12288, &n ) == 0 && n == 0 );
	/* kept from base 10, copied back at base 100 with its indexes moved */
	Skin( 10, 7.0f );
	R_MOHAASkinCacheKeep( &cache, gen, bones[0], &surfaces[0], 1, -1, 0, xyz + 10, normal + 10, st + 10,
		indexes + 10, 10, 3, 3 );
	assert( cache.usedVerts == 3 && cache.usedIndexes == 3 );
	std::memset( xyz, 0, sizeof( xyz ) );
	std::memset( indexes, 0, sizeof( indexes ) );
	assert( R_MOHAASkinCacheCopy( &cache, gen, bones[0], &surfaces[0], 1, -1, 0, xyz + 100, normal + 100,
		st + 100, indexes + 200, 100, 4096, 12288, &n ) == 3 && n == 3 );
	assert( xyz[100][0] == 7.0f && xyz[102][1] == 2.0f && xyz[101][3] == 1.0f && normal[101][2] == 1.0f );
	assert( st[102][0] == 7.0f && st[102][1] == 2.0f );
	assert( indexes[200] == 102 && indexes[201] == 101 && indexes[202] == 100 );
	/* keeping it again changes nothing */
	R_MOHAASkinCacheKeep( &cache, gen, bones[0], &surfaces[0], 1, -1, 0, xyz + 100, normal + 100, st + 100,
		indexes + 200, 100, 3, 3 );
	assert( cache.usedVerts == 3 );
	/* every part of the key matters: pose, surface, scale, detail, view weapon */
	assert( !R_MOHAASkinCacheCopy( &cache, gen, bones[1], &surfaces[0], 1, -1, 0, xyz, normal, st, indexes, 0, 4096, 12288, &n ) );
	assert( !R_MOHAASkinCacheCopy( &cache, gen, bones[0], &surfaces[1], 1, -1, 0, xyz, normal, st, indexes, 0, 4096, 12288, &n ) );
	assert( !R_MOHAASkinCacheCopy( &cache, gen, bones[0], &surfaces[0], 2, -1, 0, xyz, normal, st, indexes, 0, 4096, 12288, &n ) );
	assert( !R_MOHAASkinCacheCopy( &cache, gen, bones[0], &surfaces[0], 1, 30, 0, xyz, normal, st, indexes, 0, 4096, 12288, &n ) );
	assert( !R_MOHAASkinCacheCopy( &cache, gen, bones[0], &surfaces[0], 1, -1, 1, xyz, normal, st, indexes, 0, 4096, 12288, &n ) );
	/* a copy that would not fit the caller's room is no copy */
	assert( !R_MOHAASkinCacheCopy( &cache, gen, bones[0], &surfaces[0], 1, -1, 0, xyz, normal, st, indexes, 0, 2, 12288, &n ) && n == 0 );
	assert( !R_MOHAASkinCacheCopy( &cache, gen, bones[0], &surfaces[0], 1, -1, 0, xyz, normal, st, indexes, 0, 4096, 2, &n ) );
	/* a new pose generation drops every copy and starts the store over */
	assert( !R_MOHAASkinCacheCopy( &cache, gen + 1, bones[0], &surfaces[0], 1, -1, 0, xyz, normal, st, indexes, 0, 4096, 12288, &n ) );
	assert( cache.usedVerts == 0 && cache.usedIndexes == 0 );
	/* a surface larger than the room left is skinned each time, never kept */
	cache.usedVerts = MOHAA_SKIN_CACHE_VERTS - 2;
	Skin( 0, 3.0f );
	R_MOHAASkinCacheKeep( &cache, gen + 1, bones[0], &surfaces[0], 1, -1, 0, xyz, normal, st, indexes, 0, 3, 3 );
	assert( cache.usedVerts == MOHAA_SKIN_CACHE_VERTS - 2 );
	assert( !R_MOHAASkinCacheCopy( &cache, gen + 1, bones[0], &surfaces[0], 1, -1, 0, xyz, normal, st, indexes, 0, 4096, 12288, &n ) );
	/* more surfaces than entries: the ones kept copy back whole, the rest miss */
	const unsigned int gen3 = gen + 2;
	int kept = 0;
	for ( int s = 0; s < 2048; s++ ) {
		Skin( 0, (float)s );
		R_MOHAASkinCacheKeep( &cache, gen3, bones[0], &surfaces[s], 1, -1, 0, xyz, normal, st, indexes, 0, 3, 3 );
	}
	for ( int s = 0; s < 2048; s++ ) {
		if ( R_MOHAASkinCacheCopy( &cache, gen3, bones[0], &surfaces[s], 1, -1, 0, xyz + 8, normal + 8, st + 8,
			indexes + 8, 8, 4096, 12288, &n ) ) {
			assert( n == 3 && xyz[8][0] == (float)s && indexes[8] == 10 );
			kept++;
		}
	}
	assert( kept > 0 && kept <= MOHAA_SKIN_CACHE_ENTRIES && cache.usedVerts == kept * 3 );
	std::printf( "ok %d\n", kept );
	return 0;
}
'''


def read(*parts):
    return (ROOT.joinpath(*parts)).read_text(encoding='utf-8', errors='replace').replace('\r\n', '\n')


def function(text, signature):
    start = text.index(signature)
    return text[start:text.index('\n}\n', start) + 2]


class MOHAASkinCacheTests(unittest.TestCase):
    def test_cache(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++17 compiler required')
        (ROOT / '.tmp').mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='mohaa-skin-cache-', dir=ROOT / '.tmp') as temp:
            cpp, exe = Path(temp) / 'host.cpp', Path(temp) / 'host.exe'
            cpp.write_text(HOST, encoding='utf-8')
            built = subprocess.run([compiler, *compiler_target_args(compiler), '-std=c++17', '-I', str(ROOT),
                                    str(cpp), '-o', str(exe)], capture_output=True, text=True, timeout=120)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            run = subprocess.run([str(exe)], capture_output=True, text=True, timeout=60)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)
            self.assertTrue(run.stdout.startswith('ok '), run.stdout)

    def test_every_renderer_skins_a_surface_once_a_frame(self):
        for renderer in RENDERERS:
            with self.subTest(renderer=renderer):
                model = read('code', renderer, 'tr_mohaa.c')
                self.assertIn('#include "../renderercommon/tr_mohaa_skin_cache.h"', model)
                pose = function(model, 'static const float *R_MOHAAPose( trRefEntity_t *ent, const model_t *mod ) {')
                # the pool starting over is a new generation
                self.assertIn('\t\tmohaaPosePoolUsed = 0;\n\t\tmohaaPoseGeneration++;', pose)
                surface = function(model, 'void RB_SurfaceMOHAATiki( void *data ) {')
                copy = surface.index('verts = R_MOHAASkinCacheCopy( &mohaaSkinCache, mohaaPoseGeneration, '
                                     'ent->e.mohaa.bones, surface, scale,')
                skin = surface.index('verts = ri.MOHAARender->Skin(')
                keep = surface.index('R_MOHAASkinCacheKeep( &mohaaSkinCache, mohaaPoseGeneration, '
                                     'ent->e.mohaa.bones, surface, scale,')
                self.assertLess(copy, skin)
                self.assertLess(skin, keep)
                self.assertLess(surface.index('RB_CHECKOVERFLOW('), copy)
                # the view weapon's detail rule is part of the key and of the skin
                self.assertIn('const int lodFlags = ( ent->e.renderfx & RF_DEPTHHACK ) ? MOHAA_LOD_VIEWMODEL : 0;',
                              surface)
                self.assertEqual(surface.count('ent->e.mohaa.lod, lodFlags,'), 3)


if __name__ == '__main__':
    unittest.main()
