"""Alice's and F.A.K.K.2's TIKI model surfaces are tessellated once a frame.

The back end tessellated a TIKI surface on the CPU for the view, again for each
dynamic light's lit pass, for a crossfade's second skin and a custom shader's
extra pass, and for the shadows. code/renderercommon/tr_alice_tiki_cache.h
keeps each surface tessellated this frame and copies it back for the frame's
later draws of the same surface with the same pose, scale and detail; the
poses' generation, counted by RA_TikiBeginFrame, drops every copy when they
start over.

The store is compiled against stand-in types; the three renderers' surface
functions must copy from it before tessellating and keep what they tessellate,
and the key must hold every entity field the tessellation reads.
"""
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]
RENDERERS = ('renderer', 'renderervk', 'rendererrtx')
KEY_FIELDS = ('anim', 'oldanim', 'frame', 'oldframe', 'backlerp', 'scale')

HOST = r'''
#include <cassert>
#include <climits>
#include <cstddef>
#include <cstdio>
#include <cstring>
typedef int qboolean;
enum { qfalse, qtrue };
typedef float vec2_t[2];
typedef float vec4_t[4];
#define ID_INLINE inline
#define Com_Memcpy memcpy
#include "code/renderercommon/tr_alice_tiki_cache.h"

static aliceTikiCache_t cache;
static vec4_t xyz[4096], normal[4096];
static vec2_t st[4096];
static unsigned int indexes[12288];
static float bones[2][24];
static int surfaces[2048];

/* a surface "tessellated" at baseVertex: three vertices and a triangle */
static void Tessellate( int base, float tag ) {
	for ( int i = 0; i < 3; i++ ) {
		xyz[base + i][0] = tag; xyz[base + i][1] = (float)i; xyz[base + i][2] = 0; xyz[base + i][3] = 1;
		normal[base + i][0] = 0; normal[base + i][1] = 0; normal[base + i][2] = 1; normal[base + i][3] = 0;
		st[base + i][0] = tag; st[base + i][1] = (float)i;
	}
	for ( int i = 0; i < 3; i++ ) indexes[base + i] = (unsigned int)( base + 2 - i );
}

static aliceTikiCacheKey_t Key( const void *surface, const void *pose ) {
	aliceTikiCacheKey_t key;
	key.surface = surface;
	key.bones = pose;
	key.anim = 2; key.oldanim = 1;
	key.frame = 5; key.oldframe = 4;
	key.backlerp = 0.25f;
	key.scale = 1.0f;
	key.lod = 0.5f;
	return key;
}

static int Copy( unsigned int gen, const aliceTikiCacheKey_t &key, int base, int maxVerts, int maxIndexes, int *n ) {
	return R_AliceTikiCacheCopy( &cache, gen, &key, xyz + base, normal + base, st + base, indexes + base,
		(unsigned int)base, maxVerts, maxIndexes, n );
}

int main() {
	int n = 0;
	const unsigned int gen = 1;
	const aliceTikiCacheKey_t key = Key( &surfaces[0], bones[0] );
	/* nothing kept yet: zeroed entries are free, never generation 1 */
	assert( Copy( gen, key, 0, 4096, 12288, &n ) == 0 && n == 0 );
	/* kept from base 10, copied back at base 100 with its indexes moved */
	Tessellate( 10, 7.0f );
	R_AliceTikiCacheKeep( &cache, gen, &key, xyz + 10, normal + 10, st + 10, indexes + 10, 10, 3, 3 );
	assert( cache.usedVerts == 3 && cache.usedIndexes == 3 );
	std::memset( xyz, 0, sizeof( xyz ) );
	std::memset( indexes, 0, sizeof( indexes ) );
	assert( R_AliceTikiCacheCopy( &cache, gen, &key, xyz + 100, normal + 100, st + 100, indexes + 200, 100,
		4096, 12288, &n ) == 3 && n == 3 );
	assert( xyz[100][0] == 7.0f && xyz[102][1] == 2.0f && xyz[101][3] == 1.0f && normal[101][2] == 1.0f );
	assert( st[102][0] == 7.0f && st[102][1] == 2.0f );
	assert( indexes[200] == 102 && indexes[201] == 101 && indexes[202] == 100 );
	/* keeping it again changes nothing */
	R_AliceTikiCacheKeep( &cache, gen, &key, xyz + 100, normal + 100, st + 100, indexes + 200, 100, 3, 3 );
	assert( cache.usedVerts == 3 );
	/* every part of the key matters: surface, pose, frames, lerp, scale, detail */
	for ( int field = 0; field < 9; field++ ) {
		aliceTikiCacheKey_t other = key;
		switch ( field ) {
		case 0: other.surface = &surfaces[1]; break;
		case 1: other.bones = bones[1]; break;
		case 2: other.anim = 3; break;
		case 3: other.oldanim = 3; break;
		case 4: other.frame = 6; break;
		case 5: other.oldframe = 6; break;
		case 6: other.backlerp = 0.5f; break;
		case 7: other.scale = 2.0f; break;
		case 8: other.lod = 1.0f; break;
		}
		assert( Copy( gen, other, 0, 4096, 12288, &n ) == 0 && n == 0 );
	}
	/* the same frames on another entity are the same vertex-animated surface */
	const aliceTikiCacheKey_t frames = Key( &surfaces[2], nullptr );
	Tessellate( 0, 9.0f );
	R_AliceTikiCacheKeep( &cache, gen, &frames, xyz, normal, st, indexes, 0, 3, 3 );
	assert( Copy( gen, Key( &surfaces[2], nullptr ), 20, 4096, 12288, &n ) == 3 && xyz[20][0] == 9.0f );
	/* a copy that would not fit the caller's room is no copy */
	assert( Copy( gen, key, 0, 2, 12288, &n ) == 0 && n == 0 );
	assert( Copy( gen, key, 0, 4096, 2, &n ) == 0 );
	/* a new pose generation drops every copy and starts the store over */
	assert( Copy( gen + 1, key, 0, 4096, 12288, &n ) == 0 );
	assert( cache.usedVerts == 0 && cache.usedIndexes == 0 );
	/* a surface larger than the room left is tessellated each time, never kept */
	cache.usedVerts = ALICE_TIKI_CACHE_VERTS - 2;
	Tessellate( 0, 3.0f );
	R_AliceTikiCacheKeep( &cache, gen + 1, &key, xyz, normal, st, indexes, 0, 3, 3 );
	assert( cache.usedVerts == ALICE_TIKI_CACHE_VERTS - 2 );
	assert( Copy( gen + 1, key, 0, 4096, 12288, &n ) == 0 );
	/* a surface with no triangles left at its detail is not kept */
	R_AliceTikiCacheKeep( &cache, gen + 2, &key, xyz, normal, st, indexes, 0, 3, 0 );
	assert( Copy( gen + 2, key, 0, 4096, 12288, &n ) == 0 );
	/* more surfaces than entries: the ones kept copy back whole, the rest miss */
	const unsigned int gen4 = gen + 3;
	int kept = 0;
	for ( int s = 0; s < 2048; s++ ) {
		const aliceTikiCacheKey_t each = Key( &surfaces[s], bones[0] );
		Tessellate( 0, (float)s );
		R_AliceTikiCacheKeep( &cache, gen4, &each, xyz, normal, st, indexes, 0, 3, 3 );
	}
	for ( int s = 0; s < 2048; s++ ) {
		if ( Copy( gen4, Key( &surfaces[s], bones[0] ), 8, 4096, 12288, &n ) ) {
			assert( n == 3 && xyz[8][0] == (float)s && indexes[8] == 10 );
			kept++;
		}
	}
	assert( kept > 0 && kept <= ALICE_TIKI_CACHE_ENTRIES && cache.usedVerts == kept * 3 );
	/* Module-supplied frame/animation values must not overflow the hash. */
	for ( int edge = 0; edge < 2; edge++ ) {
		aliceTikiCacheKey_t extreme = key;
		extreme.frame = edge ? INT_MIN : INT_MAX;
		extreme.anim = edge ? INT_MAX : INT_MIN;
		const unsigned int edgeGen = gen4 + 1 + (unsigned int)edge;
		assert( Copy( edgeGen, extreme, 0, 4096, 12288, &n ) == 0 );
		Tessellate( 0, 12.0f );
		R_AliceTikiCacheKeep( &cache, edgeGen, &extreme, xyz, normal, st, indexes, 0, 3, 3 );
		assert( Copy( edgeGen, extreme, 8, 4096, 12288, &n ) == 3 && xyz[8][0] == 12.0f );
	}
	std::printf( "ok %d\n", kept );
	return 0;
}
'''


def read(*parts):
    return (ROOT.joinpath(*parts)).read_text(encoding='utf-8', errors='replace').replace('\r\n', '\n')


def function(text, signature):
    start = text.index(signature)
    return text[start:text.index('\n}\n', start) + 2]


class AliceTikiCacheTests(unittest.TestCase):
    def test_cache(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++17 compiler required')
        (ROOT / '.tmp').mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='alice-tiki-cache-', dir=ROOT / '.tmp') as temp:
            cpp, exe = Path(temp) / 'host.cpp', Path(temp) / 'host.exe'
            cpp.write_text(HOST, encoding='utf-8')
            built = subprocess.run([compiler, *compiler_target_args(compiler), '-std=c++17', '-ftrapv', '-I', str(ROOT),
                                    str(cpp), '-o', str(exe)], capture_output=True, text=True, timeout=120)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            run = subprocess.run([str(exe)], capture_output=True, text=True, timeout=60)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)
            self.assertTrue(run.stdout.startswith('ok '), run.stdout)

    def test_the_key_holds_everything_the_tessellation_reads(self):
        tiki = read('code', 'renderercommon', 'tr_alice_tiki.cpp')
        tessellate = function(tiki, 'int RA_TikiTessellateLod( const void *model, int surface, '
                                    'const aliceRenderEntity_t *e,')
        scale = function(tiki, 'float EntityScale( const Model *m, const aliceRenderEntity_t *e ) {')
        self.assertIn('EntityScale( m, e )', tessellate)
        read_fields = set(re.findall(r'\be->(\w+)', tessellate + scale))
        self.assertTrue(read_fields)
        self.assertLessEqual(read_fields, set(KEY_FIELDS))
        # the entity reaches nothing else that reads it
        self.assertEqual(re.findall(r'\(\s*m,\s*e\s*\)', tessellate), ['( m, e )'])
        # the poses' generation is counted where they start over
        begin = function(tiki, 'void RA_TikiBeginFrame( void ) {')
        self.assertIn('\tposeBlock = 0;\n\tposeUsed = 0;\n\tif ( ++poseGeneration == 0 ) {', begin)
        self.assertIn('unsigned int RA_TikiPoseGeneration( void );', read('code', 'renderercommon', 'tr_alice_tiki.h'))

    def test_every_renderer_tessellates_a_surface_once_a_frame(self):
        for renderer in RENDERERS:
            with self.subTest(renderer=renderer):
                model = read('code', renderer, 'tr_alice.c')
                self.assertIn('#include "../renderercommon/tr_alice_tiki_cache.h"', model)
                key = function(model, 'static void RB_AliceTikiCacheKey( aliceTikiCacheKey_t *key, '
                                      'const aliceSurface_t *surface,')
                self.assertIn('\tkey->surface = surface;\n\tkey->bones = alice->bones;\n', key)
                for field in KEY_FIELDS:
                    self.assertIn(f'\tkey->{field} = alice->{field};\n', key)
                self.assertIn('\tkey->lod = lod;\n', key)
                surface = function(model, 'void RB_SurfaceAliceTiki( void *data ) {')
                self.assertIn('const unsigned int generation = RA_TikiPoseGeneration();', surface)
                self.assertIn('RB_AliceTikiCacheKey( &key, surface, &ent->e.alice, lod );', surface)
                copy = surface.index('verts = reuse ? R_AliceTikiCacheCopy( &aliceTikiCache, generation, &key,')
                tessellate = surface.index('if ( !RA_TikiTessellateLod(')
                keep = surface.index('R_AliceTikiCacheKeep( &aliceTikiCache, generation, &key,')
                self.assertLess(surface.index('RB_CHECKOVERFLOW('), copy)
                self.assertLess(copy, tessellate)
                self.assertLess(tessellate, keep)
                self.assertIn('\t\tif ( reuse ) {\n\t\t\tR_AliceTikiCacheKeep(', surface)
                # the shadow faces' full detail and the views' own are both part of the key
                self.assertIn('const float lod = ( backEnd.viewParms.passFlags & VPF_DLIGHT_SHADOW )\n'
                              '\t\t? 1.0f : ent->aliceLod[R_AliceLodSlot( &backEnd.viewParms )];', surface)
                # the comparison switch is registered with the Ritual profiles' own cvars
                cvars = function(model, 'void R_AliceRegisterCvars( void ) {')
                self.assertIn('r_ritualModelReuse = ri.Cvar_Get( "r_ritualModelReuse", "1", CVAR_ARCHIVE_ND );',
                              cvars)
                self.assertIn('const qboolean reuse = !r_ritualModelReuse || r_ritualModelReuse->integer '
                              '? qtrue : qfalse;', surface)


if __name__ == '__main__':
    unittest.main()
