"""RTX treats r_hdr 1 as display-referred unless scene-linear colour is chosen.

docs/DISPLAY.md defines r_hdr 1 as the high-precision path that, with the
default legacy tone mapper, keeps Quake III's display-referred lighting;
scene-linear colour is an opt-in (a non-legacy tone mapper, colour grading or
HDR output). GLx implements that in FBO_HdrSceneLinearColorMode. RTX's
vk_scene_linear_enabled keyed on r_hdr alone, so once r_hdr defaulted to 1 it
decoded every authored texture from sRGB and blended in linear light. On RTCW
MP's mp_beach that drew the water spray and the translucent edges of the HUD
visibly brighter than GLx and Vulkan at the same demo frame.

These tests run RTX's own gate against GLx's own gate over every combination
of their shared inputs, and check that RTX's fog and underwater compositors,
which must match the output transform, ask the same gate.
"""
from pathlib import Path
import itertools
import os
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args
from mesh_tag_tests import function

ROOT = Path(__file__).resolve().parents[1]


def read(path):
    return (ROOT / path).read_text(encoding='utf-8')


def fixture():
    rtx = function(read('code/rendererrtx/vk.c'), 'qboolean vk_scene_linear_enabled( void )\n{')
    arb = read('code/renderer/tr_arb.c')
    glx = (function(arb, 'static int FBO_HdrSceneLinearMode( void )\n{') +
           function(arb, 'static int FBO_HdrSceneLinearColorMode( void )\n{'))
    cases = ',\n'.join('{%d,%d,%d,%d,%d,%d,%d,%d}' % case for case in itertools.product(
        (0, 1), (-1, 0, 1), (0, 1), (0, 1, 2), (0, 1), (0, 1), (0, 1), (0, 1)))
    return r'''
#include <cassert>
#include <cstdio>
typedef int qboolean;
enum { qfalse, qtrue };
enum { VK_FORMAT_R8G8B8A8_UNORM = 37, VK_FORMAT_R16G16B16A16_UNORM = 91, VK_FORMAT_R16G16B16A16_SFLOAT = 97 };
enum { ROUTPUT_REQUEST_AUTO, ROUTPUT_REQUEST_SDR_SRGB, ROUTPUT_REQUEST_WINDOWS_SCRGB, ROUTPUT_REQUEST_HDR10_PQ };
struct cvar_t { int integer; };
static cvar_t fbo, hdr, srgb, tonemap, grade, output;
static cvar_t *r_fbo = &fbo, *r_hdr = &hdr, *r_srgbTextures = &srgb, *r_tonemap = &tonemap,
	*r_colorGrade = &grade, *r_outputBackend = &output;
static struct { int color_format; qboolean hdrDisplayActive; } vk;
''' + rtx + glx + r'''
int main() {
	// fbo, hdr, srgb, tonemap, grade, hdr display, float scene, null cvars
	static const int cases[][8] = {
''' + cases + r'''
	};
	int checked = 0, linear = 0;
	for ( const auto &c : cases ) {
		fbo.integer = c[0]; hdr.integer = c[1]; srgb.integer = c[2]; tonemap.integer = c[3];
		grade.integer = c[4]; vk.hdrDisplayActive = c[5] ? qtrue : qfalse;
		output.integer = c[5] ? ROUTPUT_REQUEST_HDR10_PQ : ROUTPUT_REQUEST_AUTO;
		vk.color_format = c[6] ? VK_FORMAT_R16G16B16A16_SFLOAT : VK_FORMAT_R16G16B16A16_UNORM;
		if ( c[7] ) { r_tonemap = nullptr; r_colorGrade = nullptr; }
		const bool rtx = vk_scene_linear_enabled() != qfalse;
		r_tonemap = &tonemap; r_colorGrade = &grade;
		// RTX can only be scene-linear on a float scene target with the FBO
		// path on; GLx's function does not see either, so compare there.
		if ( !c[7] && c[0] && c[6] ) {
			const bool glx = FBO_HdrSceneLinearColorMode() != 0;
			if ( rtx != glx ) {
				std::fprintf( stderr, "case %d %d %d %d %d %d: rtx %d glx %d\n",
					c[0], c[1], c[2], c[3], c[4], c[5], rtx, glx );
				return 1;
			}
			++checked;
		}
		if ( !c[0] || !c[6] || c[1] <= 0 || !c[2] ) assert( !rtx );
		if ( c[7] && !c[5] ) assert( !rtx );
		linear += rtx;
	}
	// The shipped defaults: r_fbo 1, r_hdr 1, r_srgbTextures 1, legacy tone
	// mapper, no grade, SDR presentation, float scene storage.
	fbo.integer = 1; hdr.integer = 1; srgb.integer = 1; tonemap.integer = 0; grade.integer = 0;
	vk.hdrDisplayActive = qfalse; vk.color_format = VK_FORMAT_R16G16B16A16_SFLOAT;
	assert( !vk_scene_linear_enabled() );
	for ( int opt = 0; opt < 3; ++opt ) {
		tonemap.integer = opt == 0; grade.integer = opt == 1; vk.hdrDisplayActive = opt == 2 ? qtrue : qfalse;
		assert( vk_scene_linear_enabled() );
	}
	assert( checked == 72 && linear > 0 );
	return 0;
}
'''


class RTXSceneLinearGateTests(unittest.TestCase):
    def compile_and_run(self, source):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++17 compiler required')
        target = compiler_target_args(compiler)
        if os.name != 'nt':
            target = ['-m32']
        (ROOT / '.tmp').mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='rtx-scene-linear-', dir=ROOT / '.tmp') as temp:
            cpp, exe = Path(temp) / 'probe.cpp', Path(temp) / 'probe.exe'
            cpp.write_text(source, encoding='utf-8')
            built = subprocess.run([compiler, *target, '-std=c++17', '-O2', str(cpp), '-o', str(exe)],
                                   cwd=ROOT, capture_output=True, text=True, timeout=120)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            run = subprocess.run([str(exe)], cwd=ROOT, capture_output=True, text=True, timeout=30)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)

    def test_rtx_gate_matches_glx_and_keeps_defaults_display_referred(self):
        self.compile_and_run(fixture())

    def test_fog_and_underwater_colours_follow_the_output_transform(self):
        source = read('code/rendererrtx/vk.c')
        self.assertNotIn('sceneLinear = ( r_hdr', source)
        self.assertEqual(source.count('sceneLinear = vk_scene_linear_enabled();'), 2)
        for consumer in ('frag_spec_data.scene_linear_mode = vk_scene_linear_enabled() ? 1 : 0;',
                         'vk.bloom_format = vk_scene_linear_enabled() ?'):
            self.assertIn(consumer, source)
        image = function(read('code/rendererrtx/tr_image.c'),
                         'static qboolean R_ImageWantsSrgbDecode( imageColorSpace_t colorSpace )\n{')
        self.assertIn('vk_scene_linear_enabled()', image)


if __name__ == '__main__':
    unittest.main()
