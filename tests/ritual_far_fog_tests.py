"""Alice's and F.A.K.K.2's far-plane fog fogs an opaque surface once.

Retail draws a surface's stages unfogged, then one pass of the fog colour,
blended GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA, its alpha min( |v - eye| /
distance, 1 ) at each vertex (fakk2.exe 1.02 0x004948a0 and 0x00494910;
alice.exe 0x0049ebb0): FinishShader appends it to a copy of the stages
(0x004792bc..0x0047937b), which RB_BeginSurface draws (0x0047c5ee). So a
lightmap and the textures over it fog as one colour. This engine had fogged
each stage with RTCW's neutral colours, which made `fog`'s distant rock
brighter with the fog on than off; giving every stage the fog colour instead
darkened `swamp1`'s water and `fog`'s ivy to black, where retail's show the
haze. A shader whose first stage is blended keeps the per-stage fog, and so
does every Medal of Honor stage.
"""
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]

PREFIX = r'''
#include <cassert>
#include <initializer_list>
typedef int qboolean;
typedef unsigned char byte;
enum { qfalse, qtrue };
'''
PREFIX += '\n'.join(re.findall(
    r'^#define GLS_(?:SRCBLEND|DSTBLEND|BLEND_BITS|DEPTHTEST_DISABLE|DEPTHFUNC_EQUAL|DEPTHMASK_TRUE|ATEST)[^\n]*',
    (ROOT / 'code/renderer/tr_local.h').read_text(encoding='utf-8'), re.M)) + '\n'

CHECKS = r'''
#include "code/renderercommon/tr_ritual_far_fog.h"
int main(){
 const unsigned opaque=GLS_DEPTHMASK_TRUE,tested=GLS_DEPTHMASK_TRUE|GLS_ATEST_GT_0,
  noDepth=GLS_DEPTHMASK_TRUE|GLS_DEPTHTEST_DISABLE,
  modulate=GLS_SRCBLEND_DST_COLOR|GLS_DSTBLEND_ZERO,additive=GLS_SRCBLEND_ONE|GLS_DSTBLEND_ONE,
  alpha=GLS_SRCBLEND_SRC_ALPHA|GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA;
 // An opaque or alpha-tested first stage takes the pass; a blended one, a
 // sky or an empty shader keeps the per-stage fog.
 for(unsigned bits:{opaque,tested,noDepth})assert(FnRitualFarFogShaderTakesPass(qfalse,qtrue,bits));
 for(unsigned bits:{modulate,additive,alpha})assert(!FnRitualFarFogShaderTakesPass(qfalse,qtrue,bits));
 assert(!FnRitualFarFogShaderTakesPass(qtrue,qtrue,opaque));
 assert(!FnRitualFarFogShaderTakesPass(qfalse,qfalse,opaque));
 // Blended over the stages, on the surface's own depth.
 assert(FnRitualFarFogState(opaque)==(GLS_SRCBLEND_SRC_ALPHA|GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA|GLS_DEPTHFUNC_EQUAL));
 assert(FnRitualFarFogState(tested)==FnRitualFarFogState(opaque));
 assert(FnRitualFarFogState(noDepth)==(GLS_SRCBLEND_SRC_ALPHA|GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA|GLS_DEPTHTEST_DISABLE));
 // Linear from the eye, truncated, full at and past the distance.
 assert(FnRitualFarFogAlpha(0,2000)==0&&FnRitualFarFogAlpha(1000,2000)==127);
 assert(FnRitualFarFogAlpha(1999.9f,2000)==254&&FnRitualFarFogAlpha(2000,2000)==255&&FnRitualFarFogAlpha(9e9f,2000)==255);
 assert(FnRitualFarFogAlpha(500,0)==0&&FnRitualFarFogAlpha(-1,2000)==0);
 return 0;
}
'''


def read(relative):
    return (ROOT / relative).read_text(encoding='utf-8').replace('\r\n', '\n')


class RitualFarFogTests(unittest.TestCase):
    def test_an_opaque_surface_takes_one_fog_pass(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if compiler is None:
            self.skipTest('requires a C++17 compiler')
        scratch = ROOT / '.tmp'
        scratch.mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='ritual-fog-', dir=scratch) as folder:
            cpp = Path(folder) / 'fog.cpp'
            exe = Path(folder) / 'fog.exe'
            cpp.write_text(PREFIX + CHECKS, encoding='utf-8')
            build = subprocess.run([compiler, *compiler_target_args(compiler), '-std=c++17',
                                    '-I', str(ROOT), str(cpp), '-o', str(exe)],
                                   cwd=ROOT, capture_output=True, text=True, timeout=120)
            self.assertEqual(build.returncode, 0, build.stdout + build.stderr)
            run = subprocess.run([str(exe)], capture_output=True, text=True, timeout=15)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)

    def test_the_pass_is_ritual_and_needs_the_view_fog(self):
        render = read('code/renderercommon/tr_ritual_far_fog_render.h')
        self.assertIn('if ( !COM_GAME_ID_IS_RITUAL( ri.gameId ) || !shader || backEnd.projection2D ) {', render)
        self.assertIn('if ( !backEnd.viewParms.aliceFarFog || !( backEnd.viewParms.aliceFarDistance > 0.0f ) ) {',
                      render)
        self.assertIn('VectorSubtract( tess.xyz[i], backEnd.or.viewOrigin, delta );', render)

    def test_a_far_plane_replaces_the_surfaces_fog_volume(self):
        # retail's RB_BeginSurface gives every 3D surface but an uncut sky the
        # far fog's number in place of its fog volume (0x0047c553..0x0047c583)
        render = read('code/renderercommon/tr_ritual_far_fog_render.h')
        self.assertIn('if ( fogNum && shader && COM_GAME_ID_IS_RITUAL( ri.gameId ) && '
                      'backEnd.viewParms.aliceFarFog &&\n'
                      '\t\t!backEnd.projection2D && ( !shader->isSky || backEnd.viewParms.aliceFarCull ) ) {\n'
                      '\t\treturn 0;', render)
        for renderer in ('renderer', 'renderervk', 'rendererrtx'):
            with self.subTest(renderer=renderer):
                source = read(f'code/{renderer}/tr_shade.c')
                begin = source.index('void RB_BeginSurface( shader_t *shader, int fogNum ) {')
                body = source[begin:source.index('tess.fogNum = fogNum;', begin)]
                self.assertIn('\tfogNum = RB_RitualSurfaceFogNum( shader, fogNum );\n', body)

    def test_each_renderer_fogs_the_stages_or_the_pass(self):
        gl = read('code/renderer/tr_rtcw_fog_gl.h')
        self.assertIn('rtcwStageFog = RB_AliceFarFogDraw();\n'
                      '\t\tif ( RB_RitualFarFogTakesPass( tess.shader ) ) {\n'
                      '\t\t\trtcwStageFog.color[3] = 0;', gl)
        vk = read('code/renderercommon/tr_rtcw_fog_vk.h')
        self.assertIn('\tif ( RB_RitualFarFogTakesPass( tess.shader ) ) {\n'
                      '\t\tdraw.color[3] = 0;', vk)
        shade = read('code/renderer/tr_shade.c')
        self.assertIn('\tif ( RB_RitualFarFogTakesPass( tess.shader ) )\n\t{\n\t\tRB_RitualFarFogPass();\n\t}', shade)
        for renderer in ('renderervk', 'rendererrtx'):
            with self.subTest(renderer=renderer):
                source = read(f'code/{renderer}/tr_shade.c')
                self.assertIn('\tif ( RB_RitualFarFogTakesPass( tess.shader ) ) {\n'
                              '\t\tRB_RitualFarFogPass( rebindIndex );\n\t}', source)
                # the pass reads tess.xyz, so its surfaces stay out of static VBOs
                self.assertIn('!RB_RitualFarFogTakesPass( shader ) ) {\n\t\ttess.allowVBO = qtrue;', source)


if __name__ == '__main__':
    unittest.main()
