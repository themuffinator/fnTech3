"""Execute the direct-light support policy at shadow edges and noisy receivers."""
from pathlib import Path
import unittest

from rtx_surface_owner_tests import compile_run
from rtx_transport_tests import function

ROOT = Path(__file__).resolve().parents[1]


class RtxDirectFilterTests(unittest.TestCase):
    def test_uncertain_light_gets_bounded_exposure_invariant_support(self):
        shader = (ROOT / 'code/rendererrtx/shaders/rt_denoise_atrous.comp').read_text()
        source = r'''
#include <cassert>
#include <cstdint>
#include <initializer_list>
using uint = uint32_t;
''' + function(shader, 'rt_direct_filter_support') + r'''
int main() {
    // A tenfold exposure change scales variance by one hundred. It must
    // never change whether the same noisy signal receives reconstruction.
    for (float exposure : {0.0001f, 0.01f, 1.0f, 10.0f, 1000.0f}) {
        float settledVariance = 0.01f * exposure * exposure;
        float noisyVariance = 0.25f * exposure * exposure;
        for (float departure : {0.001f, 0.075f, 0.15f}) {
            for (uint pass : {0u, 1u}) {
                assert(rt_direct_filter_support(pass, departure, 0, exposure));
            }
            assert(!rt_direct_filter_support(2, departure, settledVariance, exposure));
            assert(rt_direct_filter_support(2, departure, noisyVariance, exposure));
            // Neither extreme variance nor a long configured pass count
            // may spread a moving shadow through the widest passes.
            for (uint pass : {3u, 4u, 5u, 6u}) {
                assert(!rt_direct_filter_support(pass, departure, 1e20f, exposure));
            }
        }
    }
    // Both sides of a settled black/white shadow edge have zero temporal
    // variance. Their contrast alone must not unlock wider filtering.
    for (float mean : {0.0f, 1.0f, 64.0f}) {
        assert(!rt_direct_filter_support(2, 0.15f, 0, mean));
    }
    // Quiet receivers retain their existing wider reconstruction policy;
    // a dark but uncertain receiver is still eligible for the extra pass.
    for (uint pass : {0u, 1u, 2u, 3u, 4u, 5u}) {
        assert(rt_direct_filter_support(pass, 0, 0, 0));
    }
    assert(rt_direct_filter_support(2, 0.15f, 0.001f, 0));
}
'''
        compile_run(self, source)


if __name__ == '__main__':
    unittest.main()
