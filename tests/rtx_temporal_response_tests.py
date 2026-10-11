"""Execute the shader's response math, including sparse indirect samples."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]


class RtxTemporalResponseTests(unittest.TestCase):
    def test_elapsed_time_visibility_changes_and_missing_samples(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if compiler is None:
            self.skipTest('requires a C++ compiler')
        helpers = (ROOT / 'code/rendererrtx/shaders/rt_temporal_response.glsl').read_text()
        source = r'''
#include <cassert>
#include <cmath>
#include <cstdio>
#include <initializer_list>
static float min(float a, float b) { return a < b ? a : b; }
static float max(float a, float b) { return a > b ? a : b; }
static float clamp(float x, float a, float b) { return min(max(x, a), b); }
''' + helpers + r'''
int main() {
    // An entity's departure must not immediately restore the slow policy.
    volatile float reactivePeriod = 0.15f;
    assert(rt_shadow_reactive_time(0, 1, 1.0f/60) == reactivePeriod);
    assert(rt_shadow_reactive_time(0.15f, 0, 0.05f) > 0.099f);
    assert(rt_shadow_reactive_time(0.15f, 0, 0.2f) == 0);
    assert(rt_shadow_reactive_time(0.15f, 0, 0) == reactivePeriod);
    // A settled shadow edge has a mixed black/white neighborhood. Its
    // wall-clock response is independent of the number of rendered frames.
    float reference = -1;
    for (int fps : {20, 60, 120, 240}) {
        float darkening = 1, brightening = 0;
        for (int frame = 0; frame < fps / 10; ++frame) {
            darkening = rt_temporal_blend(0, darkening, 0, 1, 1.0f/fps, 0.035f, 128);
            brightening = rt_temporal_blend(1, brightening, 0, 1, 1.0f/fps, 0.035f, 128);
        }
        assert(darkening < 0.06f && brightening > 0.94f);
        assert(std::abs(darkening + brightening - 1) < 1e-6f);
        if (reference >= 0) assert(std::abs(darkening - reference) < 2e-6f);
        reference = darkening;
    }
    // A coherent visibility change rejects old light immediately, in both
    // directions. A one-frame revealed highlight must not be clipped away.
    assert(rt_temporal_blend(1, 0, 1, 1, 1.0f/60, 0.035f, 128) == 1);
    assert(rt_temporal_blend(0, 1, 0, 0, 1.0f/60, 0.035f, 128) == 0);
    assert(rt_temporal_blend(1, 0, 0, 1, 1.0f/60, 0.035f, 128) > 0.35f);
    // Frozen capture accumulation is still the exact running mean, even
    // when the current neighborhood does not contain the old observation.
    float mean = 0, sum = 0;
    for (int sample = 1; sample <= 128; ++sample) {
        float current = sample % 7 == 0 ? 7.0f : 0.0f;
        mean = rt_temporal_blend(current, mean, current, current, 0, 0.035f, float(sample));
        sum += current;
        assert(std::abs(mean - sum / sample) < 2e-6f);
    }
    // Missing indirect samples leave history unchanged. Half-rate samples
    // have the same response over two frames as full-rate observations.
    assert(rt_indirect_sample_alpha(0, 0.05f, 128) == 0);
    float full = rt_indirect_sample_alpha(1, 1.0f/60, 128);
    float half = rt_indirect_sample_alpha(2, 1.0f/60, 128);
    assert(std::abs((1-full)*(1-full) - (1-half)) < 1e-6f);
    assert(rt_temporal_alpha(0.25f, 0.035f, 128) > 0.999f);
    assert(rt_temporal_alpha(0, 0.035f, 1) == 1);
    std::puts("temporal response, visibility transitions, frozen mean and missing samples: PASS");
}
'''
        with tempfile.TemporaryDirectory(prefix='rtx-response-') as temp:
            path = Path(temp)
            cpp = path / 'response.cpp'
            exe = path / ('response.exe' if os.name == 'nt' else 'response')
            cpp.write_text(source, encoding='utf-8')
            subprocess.run([compiler, *compiler_target_args(compiler), '-std=c++17', str(cpp), '-o', str(exe)],
                           cwd=path, check=True, capture_output=True, text=True)
            result = subprocess.run([str(exe)], cwd=path, capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertIn('PASS', result.stdout)


if __name__ == '__main__':
    unittest.main()
