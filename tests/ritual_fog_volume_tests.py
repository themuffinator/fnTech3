"""Alice's and F.A.K.K.2's fog volumes draw from Ritual's own fog image.

Retail builds its 256 x 32 fog image with Quake III's texel rule but the
image's width where Quake III has its height (fakk2.exe 1.02
0x0046fdbb..0x0046fef1, alice.exe 0x00484b83..0x00484c9a): a texel nearer
than 255/256 of the fog's depth scales the distance by t - 1/256 times
32/254, then by 8, before Quake III's square-root table (0x0046fb50). No row
of the image reaches 255/256, so a point inside a fog volume, at t = 31/32 as
Quake III's texture coordinates give it (0x00494f9a), keeps about an eighth of
the distance Quake III's image gives it, and the volume is that much thinner:
with the far fog off on `over`, retail's canal shows through its black fog
volume at four fifths of its light where fnTech3 had left about half.

The host compiles code/renderercommon/tr_fog_math.h and prints both images
and the table; every texel is checked against an independent transcription of
retail's arithmetic. The renderers' sources must build the image, and answer
the vertex fog, from Ritual's rule under the two Ritual profiles alone.
"""
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]

HOST = r'''
#include <cmath>
#include <cstdio>
#define ID_INLINE inline
#define MIN( a, b ) ( ( a ) < ( b ) ? ( a ) : ( b ) )
#include "code/renderercommon/tr_fog_math.h"
int main() {
	float table[256];
	for ( int i = 0; i < 256; i++ ) table[i] = powf( (float)i / 255, 0.5f );
	for ( int i = 0; i < 256; i++ ) std::printf( "T %d %.9g\n", i, table[i] );
	for ( int y = 0; y < 32; y++ ) {
		for ( int x = 0; x < 256; x++ ) {
			const float s = ( x + 0.5f ) / 256, t = ( y + 0.5f ) / 32;
			std::printf( "I %d %d %d %d\n", x, y, (int)( 255 * R_RitualFogFactor( s, t, table, 256 ) ),
				(int)( 255 * R_LegacyFogFactor( s, t, table, 256 ) ) );
		}
	}
	// past the image: clamped distances, the outside rows and an empty table
	std::printf( "E %.9g %.9g %.9g %.9g\n", R_RitualFogFactor( 9.0f, 31.0f / 32, table, 256 ),
		R_RitualFogFactor( 0.001f, 31.0f / 32, table, 256 ), R_RitualFogFactor( 0.5f, 0.01f, table, 256 ),
		R_RitualFogFactor( 0.5f, 31.0f / 32, nullptr, 0 ) );
	return 0;
}
'''


def retail_texel(x, y, table):
    """fakk2.exe 1.02's texel, transcribed from its arithmetic."""
    s = (x + 0.5) / 256 - 1 / 512
    t = (y + 0.5) / 32
    if s < 0 or t < 1 / 32:
        return 0.0
    if t < 255 / 256:
        s *= (t - 1 / 256) * (32 / 254)
    s = min(s * 8, 1.0)
    return table[int(s * 255)]


class RitualFogVolumeTests(unittest.TestCase):
    def test_image_is_retail_and_thinner_inside_a_volume(self):
        compiler = shutil.which('clang++') or shutil.which('clang')
        if not compiler:
            self.skipTest('Clang unavailable')
        with tempfile.TemporaryDirectory() as temp:
            source = Path(temp) / 'fog.cpp'
            exe = Path(temp) / 'fog.exe'
            source.write_text(HOST, encoding='utf-8')
            built = subprocess.run([compiler, *compiler_target_args(compiler), '-std=c++17', '-O1', '-I', str(ROOT),
                                    str(source), '-o', str(exe)], capture_output=True, text=True, timeout=180)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            lines = subprocess.run([str(exe)], capture_output=True, text=True, timeout=60).stdout.splitlines()
        table = [float(line.split()[2]) for line in lines if line.startswith('T ')]
        texels = {}
        for line in lines:
            if line.startswith('I '):
                x, y, ritual, quake = map(int, line.split()[1:])
                texels[x, y] = (ritual, quake)
        self.assertEqual(len(texels), 256 * 32)
        for (x, y), (ritual, _) in texels.items():
            self.assertEqual(ritual, int(255 * retail_texel(x, y, table)), (x, y))
        # Inside a volume (the rows either side of t = 31/32) Quake III's image
        # reaches full fog an eighth of the way along; Ritual's never does.
        for y in (30, 31):
            quake_full = min(x for x in range(256) if texels[x, y][1] == 255)
            self.assertLessEqual(quake_full, 40)
            self.assertLess(max(texels[x, y][0] for x in range(256)), 255)
            for x in range(8, 64):
                self.assertLess(texels[x, y][0], texels[x, y][1])
        # The outside rows stay clear in both, as Quake III's.
        self.assertTrue(all(texels[x, 0] == (0, 0) for x in range(256)))
        edge = [line for line in lines if line.startswith('E ')][0].split()[1:]
        self.assertEqual([float(v) for v in edge[2:]], [0.0, 0.0])
        self.assertAlmostEqual(float(edge[0]), table[int(min(8.999 * (31 / 32 - 1 / 256) * (32 / 254) * 8, 1) * 255)],
                               places=6)

    def test_renderers_take_the_ritual_image_under_ritual_profiles_alone(self):
        for renderer in ('renderer', 'renderervk'):
            with self.subTest(renderer=renderer):
                text = (ROOT / 'code' / renderer / 'tr_image.c').read_text(encoding='utf-8')
                self.assertIn('#include "../renderercommon/tr_fog_math.h"', text)
                body = text[text.index('float R_FogFactor( float s, float t ) {'):]
                body = body[:body.index('\n}\n')]
                self.assertIn('if ( COM_GAME_ID_IS_RITUAL( ri.gameId ) ) {\n'
                              '\t\treturn R_RitualFogFactor( s, t, tr.fogTable, FOG_TABLE_SIZE );', body)
                # Quake III's rule is kept for every other profile.
                self.assertIn('s *= (t - 1.0f/32.0f) / (30.0f/32.0f);', body)
                image = text[text.index('static void R_CreateFogImage( void ) {'):]
                self.assertIn('d = R_FogFactor( ( x + 0.5f ) / FOG_S, ( y + 0.5f ) / FOG_T );', image)
        text = (ROOT / 'code/rendererrtx/tr_image.c').read_text(encoding='utf-8')
        helper = text[text.index('static float R_CompatibilityFogFactor( float s, float t ) {'):]
        helper = helper[:helper.index('\n}\n')]
        self.assertIn('COM_GAME_ID_IS_RITUAL( ri.gameId )', helper)
        self.assertIn('R_RitualFogFactor( s, t, tr.fogTable, FOG_TABLE_SIZE )', helper)
        self.assertIn('R_LegacyFogFactor( s, t, tr.fogTable, FOG_TABLE_SIZE )', helper)
        image = text[text.index('static void R_CreateFogImage( void ) {'):]
        self.assertIn('d = R_CompatibilityFogFactor( ( x + 0.5f ) / FOG_S, ( y + 0.5f ) / FOG_T );', image)
        self.assertRegex(text, re.compile(r'return R_AnalyticFogFactor\( s, t \);\n\t\}\n\treturn '
                                          r'R_CompatibilityFogFactor\( s, t \);'))


if __name__ == '__main__':
    unittest.main()
