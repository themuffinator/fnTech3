"""Quake Live's host text faces belong to the Quake Live profile alone.

The renderer's FontStash lane reads Quake Live's four bundled TTF faces and,
on Windows, a system Unicode face. Every other profile draws its console and
client text with its own bitmap charset. Before this was gated, starting
Return to Castle Wolfenstein printed "QL fonts: unable to read
fonts/handelgothic.ttf" for each missing face, loaded Windows' segoeui.ttf as
the only face, and drew the RTCW console in it: the console asks for face 2
and the lane substitutes any face it has.

These tests run the renderer's own init, metrics and shutdown functions for
every profile id with FontStash and the file system stubbed.
"""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args
from mesh_tag_tests import function

ROOT = Path(__file__).resolve().parents[1]


def fixture():
    source = (ROOT / 'code/renderercommon/tr_font_stash.c').read_text(encoding='utf-8')
    defines = '\n'.join(line for line in source.splitlines() if line.startswith('#define QL_HOST_'))
    types = source[source.index('typedef struct {\n\tvoid *data;'):
                   source.index('} qlHostFonts_t;') + len('} qlHostFonts_t;')]
    functions = ''.join(function(source, signature) for signature in (
        'static int QL_ResolveHostFaceId( int fontHandle ) {',
        'void R_InitHostFonts( void ) {',
        'void R_DoneHostFonts( void ) {',
        'qboolean RE_GetScaledFontMetrics( int fontHandle, float scale, float *outAscent,',
    ))
    return HARNESS.replace('@DEFINES@', defines).replace('@TYPES@', types) + functions + CHECKS


HARNESS = r'''
#include <cassert>
#include <cstring>
#include <string>
#include <vector>
#include "code/qcommon/game_profile.h"
#define BUILD_FONTSTASH 1
typedef int qboolean;
enum { qfalse, qtrue };
typedef int qhandle_t;
typedef struct image_s image_t;
enum { PRINT_ALL, PRINT_DEVELOPER, PRINT_WARNING };
#define Com_Memset memset
#define FONS_INVALID -1
enum { FONS_ZERO_TOPLEFT = 1 };
struct FONScontext { int unused; };
struct FONSparams {
	int width, height;
	unsigned char flags;
	void *userPtr;
	int ( *renderCreate )( void *, int, int );
	int ( *renderResize )( void *, int, int );
	void ( *renderUpdate )( void *, int *, const unsigned char * );
	void ( *renderDraw )( void *, const float *, const float *, const unsigned int *, int );
	void ( *renderDelete )( void * );
};
@DEFINES@
@TYPES@
static qlHostFonts_t qlHostFonts;

static std::vector<std::string> reads;
static int created, deleted, fallbacks, freed, metrics;
static FONScontext context;
static void Printf( int, const char *, ... ) {}
static void FS_FreeFile( void * ) { ++freed; }
static void Free( void * ) { ++freed; }
static struct {
	comGameId_t gameId;
	void ( *Printf )( int, const char *, ... );
	void ( *FS_FreeFile )( void * );
	void ( *Free )( void * );
} ri = { COM_GAME_AUTO, Printf, FS_FreeFile, Free };

static FONScontext *fonsCreateInternal( FONSparams * ) { ++created; return &context; }
static void fonsDeleteInternal( FONScontext * ) { ++deleted; }
static void fonsSetErrorCallback( FONScontext *, void ( * )( void *, int, int ), void * ) {}
static void fonsSetSize( FONScontext *, float ) {}
static void fonsSetFont( FONScontext *, int ) {}
static void fonsVertMetrics( FONScontext *, float *, float *, float *lineHeight ) {
	++metrics;
	if ( lineHeight ) *lineHeight = 12.0f;
}
static int QL_FonsRenderCreate( void *, int, int ) { return 1; }
static int QL_FonsRenderResize( void *, int, int ) { return 1; }
static void QL_FonsRenderUpdate( void *, int *, const unsigned char * ) {}
static void QL_FonsRenderDraw( void *, const float *, const float *, const unsigned int *, int ) {}
static void QL_FonsRenderDelete( void * ) {}
static void QL_FonsErrorCallback( void *, int, int ) {}
static char faceBytes[QL_HOST_FACE_COUNT];
static qboolean QL_LoadHostFace( int index, const char *, const char *path ) {
	reads.push_back( path );
	qlHostFonts.faceIds[index] = index;
	qlHostFonts.faceData[index].data = &faceBytes[index];
	qlHostFonts.faceData[index].dataLength = 1;
	qlHostFonts.faceData[index].dataFromFileSystem = qtrue;
	return qtrue;
}
#if defined( _WIN32 )
static void QL_LoadWindowsFallbackFace( void ) {
	++fallbacks;
	qlHostFonts.faceIds[4] = 4;
	qlHostFonts.faceData[4].data = &faceBytes[4];
	qlHostFonts.faceData[4].dataLength = 1;
}
#endif
'''

CHECKS = r'''
int main() {
	int profiles = 0;
	for ( unsigned id = COM_GAME_AUTO; id < COM_GAME_COUNT; ++id ) {
		reads.clear();
		created = deleted = fallbacks = freed = metrics = 0;
		std::memset( &qlHostFonts, 0x5a, sizeof( qlHostFonts ) );
		ri.gameId = static_cast<comGameId_t>( id );
		R_InitHostFonts();
		float lineHeight = -1.0f;
		const qboolean console = RE_GetScaledFontMetrics( 2, 12.0f, nullptr, nullptr, &lineHeight );
		if ( id == COM_GAME_QUAKELIVE ) {
			const std::vector<std::string> faces = { "fonts/handelgothic.ttf",
				"fonts/notosans-regular.ttf", "fonts/droidsansmono.ttf",
				"fonts/droidsansfallbackfull.ttf" };
			assert( reads == faces && created == 1 && qlHostFonts.ready );
#if defined( _WIN32 )
			assert( fallbacks == 1 );
#endif
			assert( console && lineHeight == 12.0f && metrics == 1 );
		} else {
			// Nothing is read or substituted, so the console keeps its charset.
			assert( reads.empty() && !created && !fallbacks && !metrics );
			assert( !qlHostFonts.ready && !qlHostFonts.stash );
			for ( int face : qlHostFonts.faceIds ) assert( face == FONS_INVALID );
			for ( int face : qlHostFonts.fallbackIds ) assert( face == FONS_INVALID );
			assert( !console && lineHeight == 0.0f );
			++profiles;
		}
		R_DoneHostFonts();
		if ( id == COM_GAME_QUAKELIVE ) {
			assert( deleted == 1 && freed == 4 + ( fallbacks ? 1 : 0 ) );
		} else {
			assert( !deleted && !freed );
		}
		assert( !qlHostFonts.stash && !qlHostFonts.ready );
	}
	assert( profiles == COM_GAME_COUNT - 1 );
	return 0;
}
'''


class HostFontProfileTests(unittest.TestCase):
    def compile_and_run(self, source):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++17 compiler required')
        target = compiler_target_args(compiler)
        if os.name != 'nt':
            target = ['-m32']
        (ROOT / '.tmp').mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='host-font-', dir=ROOT / '.tmp') as temp:
            cpp, exe = Path(temp) / 'probe.cpp', Path(temp) / 'probe.exe'
            cpp.write_text(source, encoding='utf-8')
            built = subprocess.run([compiler, *target, '-std=c++17', '-O2', '-I', str(ROOT), str(cpp), '-o', str(exe)],
                                   cwd=ROOT, capture_output=True, text=True, timeout=120)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            run = subprocess.run([str(exe)], cwd=ROOT, capture_output=True, text=True, timeout=30)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)

    def test_only_quake_live_reads_or_substitutes_host_faces(self):
        self.compile_and_run(fixture())

    def test_profile_gate_precedes_every_face_read(self):
        source = (ROOT / 'code/renderercommon/tr_font_stash.c').read_text(encoding='utf-8')
        init = function(source, 'void R_InitHostFonts( void ) {')
        gate = init.index('if ( ri.gameId != COM_GAME_QUAKELIVE ) return;')
        # The reset comes first so a gated profile leaves no stale face ids.
        self.assertLess(init.index('qlHostFonts.fallbackIds[i] = FONS_INVALID;'), gate)
        for call in ('fonsCreateInternal(', 'QL_LoadHostFace(', 'QL_LoadWindowsFallbackFace()'):
            self.assertLess(gate, init.index(call), call)


if __name__ == '__main__':
    unittest.main()
