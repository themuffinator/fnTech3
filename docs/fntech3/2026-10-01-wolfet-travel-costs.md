# Enemy Territory area travel costs, types and endpoint restrictions

Date: 2026-10-01. Source/Unit/Fixture correction; runtime regressions are
recorded separately in the [JSON companion](2026-10-01-wolfet-travel-costs.json).
Alice remains deferred by the user. RTCW SP's `escape1` listening check is
still pending; this round changes neither its audio path nor its evidence.

## Findings and correction

The [AAS8 adapter](2026-10-01-wolfet-aas8-loading.md) loads ET's own area
records, and the [area-cache update](2026-10-01-wolfet-area-routing.md) uses
its goal costs and avoidance flags. The calculation that supplies those
costs still followed Quake III: it truncated distances and used a separate
swimming speed. The original ET client and dedicated binary instead use
normal speed in liquid areas and round their final binary32 cost to the
nearest even integer. A 150-unit normal crossing costs 50; the former path
charged 49 on dry ground and 150 in liquid. Crouch is selected by the absence
of the normal-presence bit. Ground steepness does not multiply ET's cost.

The independent ET helper now preserves the observed binary32 subtraction,
squares, ordered additions and distance, followed by the original double
speed constants 0.33 and 1.3 and a binary32 cost. It expresses the nearest-even
integer decision explicitly. Minimum one applies before the unsigned-short
storage wraps, including the measured wrapped zero. It accepts coordinates
in the existing EAAS reader's bounded domain, rejects nonfinite or excessive
coordinates before conversion, and leaves an output parameter untouched on
failure. The engine checks the area number and partially initialized tables
before accessing area settings. ET does not require a slope sidecar to price
a crossing.

ET's route query also omitted its large-area entry exception. The original
query adds travel bit `0x02000000` when the start or goal has contents bit
`1024`; the production query now does that for ET as well as RTCW. A
generated route to such a goal previously returned no route and now succeeds.
Intermediate entry restrictions retain their cache-update filtering.

The travel-type lookup also interpreted Quake III's high team annotations
as valid ET types. ET's original bounded table rejects those annotated values
as invalid; the adapter now does the same, preserving every valid table entry.
Quake III and Quake Live retain their annotations. The independently measured
ET lookup cases include all 32 low table indices, negatives, upper bounds and
high-bit annotations.

## Original observations

The legitimate Windows x86 `et.exe` and `etded.exe` remain read-only. The
[bounded observer](../../scripts/probe_wolfet_travel_cost.py) maps each
hash-pinned image into an emulator and executes only the cost routine,
its vector, square-root and integer math, and type table initialization/lookup.
Generated area records, vectors,
stack and world pointer replace runtime state. No entry point, platform
import, game input or renderer runs. The explicit normal floating-point
state is x87 `0x037f`, MXCSR `0x1f80`, with nearest-even rounding. This is
numeric Fixture evidence rather than an original-engine gameplay run.

Client cost entry is `0x0047d470`; dedicated entry is `0x00447dd0`. Their
261 base/vector/presence combinations plus four cases at a wrapped-zero distance produce
265 identical cost results and 42 identical travel-type results. The fixture
includes ties, neighboring binary32
values, all presence-bit combinations of interest, liquid states, three
slopes, translations, reversed endpoints, large distances and minimum costs.
The [fixture](../../tests/fixtures/wolfet-travel-cost.json) contains only
generated numbers, executable identities and observed results. The original
query's contents-bit test is visible at `et.exe` `0x0047d081..0x0047d0a4` and
the corresponding dedicated range `0x004479e1..0x00447a04`.

The [original ET source release](https://github.com/id-Software/Enemy-Territory/tree/40342a9e3690cb5b627a433d4d5cbf30e3c57698)
supports the absence of a swimming/slope multiplier and the use of `myftol`.
Its GPLv3 license was checked before use. It is reference observation only:
no implementation text is incorporated. The helper, generated fixture,
emulator harness and regressions are independently authored.

## Validation and limits

Using the corrected liquid fixtures, the pre-fix production bodies differ
from 142 of 265 original results and reject the restricted goal. All five
new production regressions pass after the correction, including generated
cost rows, origin-to-first-link costs, all affected profile controls, null
vectors, nonfinite/excessive coordinates, invalid areas and cleanup.
The travel-type regression also fails before its correction on an annotated
walk type and passes every original table result afterward.
Quake III, Quake Live, RTCW SP/MP and Holomatch keep their own swimming,
rounding and slope behavior. Twenty-five related loader, cache, route,
goal-link and visibility tests pass. A separate MSVC x86 `/O2 /fp:fast /W4
/WX` probe matches all 265 costs too.

On the rebuilt stable binaries, Q3 `q3dm1`, QL `campgrounds` and Holomatch
`hm_borg1` initialize their installed navigation and list a bot before a
normal exit. The ET client joins both teams with pure admission and visibility
filtering on original `railgun` and fnTech3 `oasis` servers. A short ET
`goldrush` team recording passes normal playback and timedemo. Elite Force
SP `borg1` and MP `hm_borg1` reach their first snapshot and write engine
captures at 1024x768 with Vulkan and initialized, muted OpenAL. All tests
use isolated writable homes, read-only installations and disabled input on
private desktops; no fullscreen or input-control action is performed.
An extended `borg1` probe writes early and late engine captures while its
opening text and background advance. These validate intro progress rather
than campaign gameplay. The first delayed-capture attempt queued its final
image immediately before `quit`; renderer shutdown correctly discarded that
pending image. The corrected probe waits for capture before exiting. Both
attempts are retained with their distinct outcomes.

Full build, complete suite, final executable identities, isolated runtime
commands and outcomes are pinned in the JSON companion. The runtime checks
exercise installed maps and modules; the stock ET installation has no AAS
or RCD navigation, so they cannot establish live ET cost or bot behavior.
The first completed suite found 18 release-fixture failures because its
documentation copy ran before this record's JSON companion was created.
The missing-document error also prevented checks of deliberately invalid
release inputs. Both evidence files are present before the corrected suite;
the earlier result is retained. No engine or packaging implementation
change was required to correct this task setup.
The targeted packaging rerun passes 49 tests with one platform capability
skip, including every previously failing case.
The corrected complete suite passes 179 Meson checks with two capability
skips and no failures. Its recursive Python suite runs 2,379 tests in
1,340.163 seconds, with 121 skips and no failures or errors. The strict
Windows x86 developer build includes all sixteen profiles and all three
renderer modules; the recorded runtime binaries remain unchanged throughout
the final checks.
Cache query/eviction/budget comparisons, loaded retail navigation and live
bot paths remain open. In particular the inspected ET source uses a 16 MiB
default routing budget and a configurable frame limit, while the current
ET path still inherits the 12 MiB Quake default and does not defer cache
misses at ET's frame limit. These are recorded follow-up gaps, not closed
by the scalar cost correction.

No presentation path or retail-exact option is added. Existing fnTech3
rendering, spatial audio, input and interface features retain their scope.
Earlier documentation debt remains: 155 dated compatibility records lack
JSON companions, as listed by the previous round's retained audit. This
record has its own companion; historical evidence is not reconstructed.
