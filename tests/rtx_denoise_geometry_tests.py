"""Execute the denoiser's plane guide on perspective slopes and depth edges."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]


class RtxDenoiseGeometryTests(unittest.TestCase):
    def test_sloping_receivers_and_discontinuities(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if compiler is None:
            self.skipTest('requires a C++ compiler')
        source = r'''
#include <cassert>
#include <cmath>
#include <initializer_list>
using std::abs;
''' + (ROOT / 'code/rendererrtx/shaders/rt_denoise_geometry.glsl').read_text() + r'''
int main() {
    // Perspective depth varies sharply across a flat grazing surface.
    // Every tap on that plane must remain eligible at every filter scale.
    for (float z : {8.0f, 100.0f, 4000.0f}) {
        float slope = 0.02f / z;
        for (int step : {1, 2, 4, 8, 16, 32}) for (int sign : {-1, 1}) {
            float delta = float(step * sign) * slope;
            float qz = 1.0f / (1.0f/z + delta);
            assert(rt_depth_plane_distance(z, qz, delta) < 1e-5f * z);
            // A parallel surface at twice the depth is not on this plane.
            assert(rt_depth_plane_distance(z, qz * 2, delta) > 0.99f * z);
        }
        // Choose the within-surface derivative at either edge orientation.
        assert(rt_depth_slope(slope, slope*20, true, true) == slope);
        assert(rt_depth_slope(slope*20, slope, true, true) == slope);
        assert(rt_depth_slope(slope, 0, true, false) == slope);
        assert(rt_depth_slope(0, slope, false, true) == slope);
        assert(rt_depth_slope(slope, slope, false, false) == 0);
    }
    assert(rt_depth_plane_distance(0, 100, 0) > 1e20f);
    assert(rt_depth_plane_distance(100, -1, 0) > 1e20f);
    assert(rt_depth_plane_distance(100, 100, -0.02f) > 1e20f);
}
'''
        with tempfile.TemporaryDirectory(prefix='rtx-depth-plane-') as temp:
            path = Path(temp)
            cpp = path / 'plane.cpp'
            exe = path / ('plane.exe' if os.name == 'nt' else 'plane')
            cpp.write_text(source, encoding='utf-8')
            result = subprocess.run([compiler, *compiler_target_args(compiler), '-std=c++17', str(cpp), '-o', str(exe)],
                                    cwd=path, capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual(subprocess.run([str(exe)], cwd=path).returncode, 0)


if __name__ == '__main__':
    unittest.main()
