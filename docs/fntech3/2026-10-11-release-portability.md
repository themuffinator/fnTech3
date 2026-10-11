# Release portability follow-up

The 0.1.0 release checks exposed differences between MSVC, MinGW i686 GCC
15.2 and Linux x86 compilers. This record is **Source and Unit** evidence;
it establishes no new live-interoperability or complete compatibility gate.
The [machine-readable companion](2026-10-11-release-portability.json) names
the affected boundaries and regression tests.

## Engine corrections

- The shared UI stretch path now rejects a positive subnormal scale from its
  IEEE representation before fast-math can treat it as zero. The existing
  malformed-viewport probe rejected this input under MSVC but accepted it
  under MinGW. Normal, zero and aspect-locked paths retain their behavior.
- Jedi Academy's effects clock materializes the measured retail `0.001f`
  multiplier as a float, as it already did for the integer frame delta.
  MinGW otherwise retained a wider literal and missed the existing exact
  `0x3ca3d70b` result for a 20 ms step. The fixture also checks negative
  deltas, word wrap, pauses and the large-delta rounding boundary.
- Jedi Outcast's server animation age materializes the product as the
  stored float described by the existing retail readings in
  `code/ghoul2/ghoul2_server.hpp`. This prevents x87 temporaries from carrying
  extra precision across that boundary.
- The IHV Test renderer materializes its binary32 model-table degree step
  and portal-roll seconds multiplier before wider arithmetic. The original
  model digest and exact portal-roll vectors now agree under MinGW too.
- RTCW movement prediction classifies a duration's sign from its float
  representation before validating the reciprocal. MinGW fast-math could
  otherwise flush a positive subnormal to zero and accept it. The existing
  invalid-duration fixture now rejects it and verifies a cleared result.
- RTCW skin player scales use `strtod` before the existing stored-bit
  validation. In the tested MinGW runtime, `atof` read `nan`, `inf` and
  `-inf` as zero, while `strtod` preserved their nonfinite values. All three
  renderer fixtures now reject these inputs consistently.
- RTX's lightmap-mode model-light stamp retains the evaluated lighting
  expression until byte conversion, matching the raster path. An extra
  float rounding produced 213 where raster produced 212 in one seeded
  x87 case. The existing comparison checks 192,000 vertices exactly.
- Jedi Outcast SP's allocation pool now aligns its header and payload within
  an overallocated block and retains the original pointer for release.
  MinGW's `max_align_t` can require 16-byte alignment where Win32 `malloc`
  supplies eight. The regression fixture injects deliberately shifted
  allocations, checks alignment, zeroing and payload writes, and verifies
  that both individual frees and pool reset release the original pointer.

These paths keep fnTech3's shared widescreen UI, model posing and effects
presentation, including the existing per-pixel lighting and shadow paths.
The IHV renderer retains the same shared renderer integration. These fixes
add no retail-exact presentation option and disable no FnQ3/FnQL
rendering, audio or interface feature. Experimental game gates stay closed.

## Probe corrections

Several fixtures compared stored floats against constants or expressions
that MinGW retained at wider precision. The corrected expectations still
compare exact binary32 values; they do not add broad numeric tolerances.
Other harness corrections provide missing stubs, avoid a C runtime symbol
collision, separate misleadingly indented statements, recognize the MinGW
x86 target and select its GUI linker options. The relaunch fixture builds
under an ASCII filename before moving to its Unicode runtime filename, so
the existing Unicode-path and descriptor-isolation checks still execute.

The synthetic server-discovery host changes profiles inside a process and
now makes its selector volatile so compiler analysis cannot cache it across
those changes. Production profile selection remains immutable.

Local checks use independent standalone fixtures, not retail engine copies.
The release workflow remains responsible for the complete source suite,
MSVC and MinGW Windows builds and suites, Linux Meson suite, and Ubuntu
20.04 x86 Make distribution checks. Release runtime evidence is separate
from these unit results.
