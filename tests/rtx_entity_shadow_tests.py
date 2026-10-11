"""Entity shadow flags must not alter primary/reflection visibility."""
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]
SHADERS = ROOT / 'code/rendererrtx/shaders'


class RtxEntityShadowTests(unittest.TestCase):
    def test_shadow_policy_and_ray_class_masks(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if compiler is None:
            self.skipTest('requires a C++ compiler')
        common = (SHADERS / 'rt_common.glsl').read_text()
        masks = '\n'.join(re.findall(r'^const uint RT_MASK_.*?;', common, re.M))
        geometry = (SHADERS / 'rt_geometry.glsl').read_text()
        casts = re.search(r'bool rt_casts_shadow\(.*?\n\}', geometry, re.S).group(0)
        source = r'''
#include <cassert>
using uint = unsigned;
struct V { float w; };
struct Vertex { V normal; };
struct RtTriangle { bool dynamic; Vertex v0; };
''' + masks + '\n' + casts + r'''
int main() {
    for (float marker : {0.0f, -1.0f}) {
        RtTriangle world={false,{{marker}}}, entity={true,{{marker}}};
        assert(rt_casts_shadow(world));
        assert(rt_casts_shadow(entity)==(marker>=0));
        assert((RT_MASK_PRIMARY&RT_MASK_DYNAMIC)!=0);
        assert((RT_MASK_SECONDARY&RT_MASK_DYNAMIC)!=0);
        assert((RT_MASK_PRIMARY&RT_MASK_BODY)==0);
        assert((RT_MASK_SECONDARY&RT_MASK_BODY)!=0);
        assert((RT_MASK_SHADOW&RT_MASK_BODY)!=0);
        assert((RT_MASK_SHADOW&RT_MASK_WEAPON)==0);
        assert((RT_MASK_SECONDARY&RT_MASK_WEAPON)==0);
    }
}
'''
        source = '#include <initializer_list>\n' + source
        with tempfile.TemporaryDirectory(prefix='rtx-shadow-') as temp:
            path = Path(temp)
            cpp = path / 'policy.cpp'
            exe = path / ('policy.exe' if os.name == 'nt' else 'policy')
            cpp.write_text(source, encoding='utf-8')
            built = subprocess.run([compiler, *compiler_target_args(compiler), '-std=c++17', str(cpp), '-o', str(exe)],
                                   cwd=path, capture_output=True, text=True)
            self.assertEqual(built.returncode, 0, built.stderr)
            self.assertEqual(subprocess.run([str(exe)], cwd=path).returncode, 0)

    def test_all_shadow_queries_enforce_the_policy(self):
        sw = (SHADERS / 'rt_trace_sw.glsl').read_text()
        hw = (SHADERS / 'rt_trace_hw.glsl').read_text()
        shadow = (SHADERS / 'rt_shadow.rahit').read_text()
        primary = (SHADERS / 'rt_main.rahit').read_text()
        host = (ROOT / 'code/rendererrtx/rt/rt_backend.c').read_text()
        self.assertIn('(firstHit || solid) && !rt_casts_shadow(shaded)', sw)
        self.assertIn('!rt_casts_shadow(tri) || !rt_hit_accepted', shadow)
        self.assertNotIn('rt_casts_shadow', primary)
        # Both first-hit point-light rays and nearest-sky queries use SBT 2.
        self.assertIn('mask & RT_MASK_MOVING, 2, 0, 0,', hw)
        self.assertIn('mask & RT_MASK_MOVING, 2, 0, 1,', hw)
        self.assertIn('groups[6].closestHitShader = 4;', host)
        self.assertIn('groups[6].anyHitShader = 7;', host)
        self.assertIn('v->normal[3] = ( ent->e.renderfx & RF_NOSHADOW ) ? -1.0f : 0.0f;', host)
        self.assertIn('!world && geometry->vertices[i0].normal[3] < 0.0f', host)


if __name__ == '__main__':
    unittest.main()
