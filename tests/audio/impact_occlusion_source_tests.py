from __future__ import annotations

import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
SHARED = ROOT / "code/client/audio/openal/AudioSystemShared.inl"
POLICY = ROOT / "code/client/audio/shared/AudioOcclusion.h"


def shared_source() -> str:
    return SHARED.read_text(encoding="utf-8")


def slice_between(source: str, start: str, end: str) -> str:
    begin = source.index(start)
    return source[begin:source.index(end, begin)]


class ImpactOcclusionSourceTests(unittest.TestCase):
    """Explosions and impacts detonate flush against world geometry.

    Sampling the direct path at the raw event origin made the surface the sound
    was resting on count as an obstruction, so wall, floor, and corner
    detonations lost several dB of dry gain and gained reverb send for no real
    obstruction. These gates keep the sampling geometry honest.
    """

    def test_policy_exposes_the_surface_escape_constants(self) -> None:
        policy = POLICY.read_text(encoding="utf-8")

        self.assertIn("constexpr float kSurfaceBias", policy)
        self.assertIn("constexpr float kSurfaceEscape", policy)

    def test_direct_path_is_sampled_clear_of_the_emitting_surface(self) -> None:
        body = slice_between(
            shared_source(),
            "static float ComputeOcclusionFactor(",
            "static float AudibilityHorizonDistance(",
        )

        self.assertIn("ResolveOcclusionSamplePoint( sourceOrigin, toSource, samplePoint )", body)
        self.assertIn("TraceBlocked( listenerOrigin, samplePoint )", body)
        self.assertNotIn("TraceBlocked( listenerOrigin, sourceOrigin )", body)

    def test_probe_fan_is_offset_from_the_resolved_sample_point(self) -> None:
        body = slice_between(
            shared_source(),
            "static float ComputeOcclusionFactor(",
            "static float AudibilityHorizonDistance(",
        )

        self.assertEqual(4, body.count("AddOcclusionProbe( listenerOrigin, shifted, blocked, total )"))
        self.assertEqual(4, len(re.findall(r"VectorMA\( samplePoint, -?spread,", body)))
        self.assertNotIn("VectorMA( sourceOrigin, spread,", body)
        # The sample count is now whatever survived rejection, never a constant.
        self.assertNotIn("blocked += TraceBlocked(", body)
        self.assertNotIn("total = 5;", body)

    def test_buried_probes_are_discarded_instead_of_counted_as_blocked(self) -> None:
        probe = slice_between(
            shared_source(),
            "static void AddOcclusionProbe(",
            "static float MoveFloatTowards(",
        )

        # A buried probe must not reach the trace and must not inflate the
        # denominator either, or rejection would read as partial occlusion.
        self.assertLess(
            probe.index("if ( PointInOccludingSolid( probeOrigin ) ) {"),
            probe.index("++total;"),
        )
        self.assertLess(probe.index("++total;"), probe.index("TraceBlocked("))

    def test_embedded_sources_still_fall_back_to_the_raw_origin(self) -> None:
        source = shared_source()
        solid = slice_between(
            source,
            "static bool PointInOccludingSolid(",
            "static void ResolveOcclusionSamplePoint(",
        )
        resolve = slice_between(
            source,
            "static void ResolveOcclusionSamplePoint(",
            "static void AddOcclusionProbe(",
        )

        self.assertIn("CM_PointContents( origin, 0 ) & kOcclusionMask", solid)
        self.assertIn("-occ::kSurfaceBias", resolve)
        self.assertIn("-occ::kSurfaceEscape", resolve)
        # A source genuinely inside geometry keeps the raw origin so the trace
        # still reports it blocked.
        self.assertIn("VectorCopy( sourceOrigin, sample );", resolve)


if __name__ == "__main__":
    unittest.main()
