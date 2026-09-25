"""Medal of Honor: the sun a level's worldspawn gives its lens flare, read
through the Alice flare code (code/renderer/tr_alice.c and the Vulkan and RTX
copies).

The key reader is production's, compiled against a stub that records the
sun it is handed; the other renderers' copies must match it."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]
RENDERERS = ('code/renderer/tr_alice.c', 'code/renderervk/tr_alice.c', 'code/rendererrtx/tr_alice.c')


def function(source, signature):
    start = source.index(signature)
    return source[start:source.index('\n}\n', start) + 3]


def sun_keys(source):
    start = source.index('static struct {\n\tqboolean\tsun;')
    end = source.index('\n}\n', source.index('void R_MOHAAEndSunKeys( void ) {')) + 3
    return source[start:end]


class MohaaSunFlareTests(unittest.TestCase):
    def test_the_worldspawn_names_the_sun_and_its_flare(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++ compiler unavailable')
        source = (ROOT / RENDERERS[0]).read_text()
        harness = r'''
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
typedef float vec_t;
typedef vec_t vec3_t[3];
typedef int qboolean;
enum { qfalse, qtrue };
#define MAX_QPATH 64
static void Com_Memset( void *p, int v, size_t n ) { std::memset( p, v, n ); }
static void Q_strncpyz( char *d, const char *s, int n ) { std::strncpy( d, s, n - 1 ); d[n - 1] = 0; }
static void AngleVectors( const vec3_t angles, vec3_t forward, vec3_t, vec3_t ) {
	const float p = angles[0] * 3.14159265f / 180.0f, y = angles[1] * 3.14159265f / 180.0f;
	forward[0] = std::cos( p ) * std::cos( y );
	forward[1] = std::cos( p ) * std::sin( y );
	forward[2] = -std::sin( p );
}
static int calls;
static bool sunSet;
static vec3_t sunDirection;
static std::string sunSection;
void R_MOHAASetSunFlare( const vec3_t direction, const char *section ) {
	calls++;
	sunSet = direction && section;
	if ( sunSet ) {
		for ( int i = 0; i < 3; i++ ) sunDirection[i] = direction[i];
		sunSection = section;
	}
}
''' + sun_keys(source) + r'''
static bool Near( float a, float b ) { return std::fabs( a - b ) < 1e-4f; }
int main() {
	/* A sun from its direction alone flares from the "sun" section, along
	 * the angles' forward vector. */
	R_MOHAABeginSunKeys();
	assert( calls == 1 && !sunSet );
	assert( R_MOHAASunKey( "sundirection", "-90 0 0" ) );
	assert( !R_MOHAASunKey( "message", "Battle in the Bocage" ) );
	assert( !R_MOHAASunKey( "sunflare", "4096 -5604 1200" ) );
	R_MOHAAEndSunKeys();
	assert( calls == 2 && sunSet && sunSection == "sun" );
	assert( Near( sunDirection[2], 1.0f ) );
	/* sunflaredirection moves the flare alone, sunflarename picks the
	 * section, and names are matched exactly. */
	R_MOHAABeginSunKeys();
	R_MOHAASunKey( "suncolor", "70 70 70" );
	R_MOHAASunKey( "sundirection", "0 90 0" );
	R_MOHAASunKey( "sunflaredirection", "0 180 0" );
	R_MOHAASunKey( "sunflarename", "sunset1" );
	assert( !R_MOHAASunKey( "SunDirection", "0 0 0" ) );
	R_MOHAAEndSunKeys();
	assert( sunSet && sunSection == "sunset1" && Near( sunDirection[0], -1.0f ) );
	/* "none" is handed on for the renderer to refuse; a level with no sun
	 * key leaves the begin's clear standing. */
	R_MOHAABeginSunKeys();
	R_MOHAASunKey( "sunlight", "90 90 90" );
	R_MOHAASunKey( "sunflarename", "none" );
	R_MOHAAEndSunKeys();
	assert( sunSection == "none" );
	calls = 0;
	R_MOHAABeginSunKeys();
	R_MOHAASunKey( "sunflarename", "sun" );
	R_MOHAAEndSunKeys();
	assert( calls == 1 && !sunSet );
	return 0;
}
'''
        with tempfile.TemporaryDirectory() as directory:
            cpp = Path(directory) / 'sun.cpp'
            exe = Path(directory) / 'sun.exe'
            cpp.write_text(harness)
            result = subprocess.run([compiler, *compiler_target_args(compiler), '-std=c++17', str(cpp), '-o', str(exe)],
                                    capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            result = subprocess.run([str(exe)], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_both_renderers_read_the_same_sun_and_flare_it_the_same_way(self):
        texts = [(ROOT / path).read_text() for path in RENDERERS]
        for text in texts[1:]:
            self.assertEqual(sun_keys(text), sun_keys(texts[0]))
            for signature in ('static const char *R_AliceLensFlareFile( void ) {',
                              'void R_MOHAASetSunFlare( const vec3_t direction, const char *section ) {',
                              'void R_MOHAARegisterLensFlareCvars( void ) {',
                              'static void RB_MOHAASunFlare( void ) {'):
                self.assertEqual(function(text, signature), function(texts[0], signature))
        for text in texts:
            draw = text[text.index('static void RB_MOHAASunFlare( void ) {'):]
            draw = draw[:draw.index('\n}\n')]
            self.assertIn('VectorMA( backEnd.viewParms.or.origin, 128.0f, mohaaSunDirection, flare->position );', draw)
            self.assertIn('trace.surfaceFlags & SURF_SKY', draw)
            self.assertIn('return ri.gameId == COM_GAME_MOHAA ? "global/lensflaredefs.txt" : "scripts/lensflare2.txt";',
                          text)
        for path in ('code/renderer/tr_bsp.c', 'code/renderervk/tr_bsp.c', 'code/rendererrtx/tr_bsp.c'):
            bsp = (ROOT / path).read_text()
            self.assertIn('if ( ri.gameId == COM_GAME_MOHAA && R_MOHAASunKey( keyname, value ) ) {', bsp)
            self.assertIn('R_MOHAABeginSunKeys();', bsp)
            self.assertIn('R_MOHAAEndSunKeys();', bsp)

    def test_a_light_flares_for_its_lens_flare_or_additive_type(self):
        for path in ('code/renderer/tr_scene.c', 'code/renderervk/tr_scene.c', 'code/rendererrtx/tr_scene.c'):
            scene = (ROOT / path).read_text()
            self.assertIn('dl->aliceFlare = ri.gameId == COM_GAME_MOHAA ? ( ( type & 5 ) ? 1 : 0 ) : type & 3;', scene)
        cgame = (ROOT / 'code/client/cl_mohaa_cgame.cpp').read_text()
        self.assertIn('re.AddAliceLightToScene( origin, intensity, r, g, b, type );', cgame)


if __name__ == '__main__':
    unittest.main()
