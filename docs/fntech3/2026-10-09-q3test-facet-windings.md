# Q3Test curve facets against each release's own windings

A curve's facets are checked by cutting a large square in the facet's plane
down to the facet's borders and measuring what is left. Every Q3Test
release starts that square 8192 units out from the plane's point nearest the
origin and measures it from +-99999. fnTech3 used Quake III's 65535 for both,
so a facet far along its plane from that point was kept where retail drops
it, and a long facet the square clips was dropped where retail keeps it.
fnTech3's Q3Test profile now uses the retail sizes. No other profile changes.

The rule changes no facet of any shipped map. To see it act, each release's
own patch generator was run in-process beside fnTech3's on synthetic patches
placed where the sizes decide. Retail and the corrected fnTech3 agree on all
36 synthetic patches and all 1,948 shipped ones; fnTech3 before the change
differed on 14 of the synthetic ones. The trace sweep of the
[trace record](2026-10-08-q3test-traces.md), rerun on every release with
q3tourney added, gives the same answers before and after the change. The
[machine-readable record](2026-10-09-q3test-facet-windings.json) holds the
addresses, cases, hashes and every run's tallies.

## Retail facts

Observed in each release's `quake3.exe`. `CM_ValidateFacet` builds the
facet's square with `BaseWindingForPlane`, chops it by each border with an
epsilon of 0.1, bounds it with `WindingBounds` and keeps it when it spans no
more than 4096 units on every axis. `BaseWindingForPlane` scales both of the
square's axes by 8192 (six multiplies by the float 8192.0), and
`WindingBounds` starts its bounds at +-99999 (`mov eax, 0x47c34f80`, then its
negation).

| Release | `CM_ValidateFacet` | calls `BaseWindingForPlane` | 8192 multiplies | calls `WindingBounds` | +-99999 start | 4096 test |
| --- | --- | --- | --- | --- | --- | --- |
| 1.02 | `0x0040a4a0` | `0x0040a4f0` -> `0x0040b070` | `0x0040b178`..`0x0040b1c0` | `0x0040a5cf` -> `0x0040af70` | `0x0040af79` | `0x0040a5ec` |
| 1.03 | `0x0040a8a0` | `0x0040a8f0` -> `0x0040b470` | `0x0040b578`..`0x0040b5c0` | `0x0040a9cf` -> `0x0040b370` | `0x0040b379` | `0x0040a9ec` |
| 1.05 | `0x0040aca0` | `0x0040acf0` -> `0x0040b950` | `0x0040ba58`..`0x0040baa0` | `0x0040adcf` -> `0x0040b850` | `0x0040b859` | `0x0040adec` |
| 1.06 | `0x0040c280` | `0x0040c2d0` -> `0x0040cc00` | `0x0040cd08`..`0x0040cd50` | `0x0040c3af` -> `0x0040cb70` | `0x0040cb79` | `0x0040c3cc` |
| 1.07 | `0x0040c2c0` | `0x0040c310` -> `0x0040cc40` | `0x0040cd48`..`0x0040cd90` | `0x0040c3ef` -> `0x0040cbb0` | `0x0040cbb9` | `0x0040c40c` |
| 1.08 | `0x0040cc30` | `0x0040cc80` -> `0x0040d5b0` | `0x0040d6b8`..`0x0040d700` | `0x0040cd5f` -> `0x0040d520` | `0x0040d529` | `0x0040cd7c` |

The only other caller of `BaseWindingForPlane` draws curves for debugging
(it registers `cm_debugSize`); no release adds bevels to a facet, so nothing
else in collision uses the square.

## Retail's generator run in-process

**Method.** Retail `quake3.exe` has its relocations stripped, so its code
runs only at its own base, 0x400000. A small host program, linked fixed at
that base with an 8 MB zero-filled section, holds the range; a harness
library loaded beside it copies the retail executable's sections over that
range in memory and calls its `CM_GeneratePatchCollide` (width, height,
points) directly. The generator's own code runs as shipped, and the
installation is only read. The five engine services it reaches are
redirected, by a jump written at each one's entry in the copy in memory, to
the harness's own: `Com_Error`, `Com_Printf`, `Hunk_Alloc`, `Z_Malloc` and
`Z_Free`. The x87 runs with Windows' default control word for a new thread
(53-bit precision, round to nearest). fnTech3's
generator, sliced from `code/qcommon/cm_patch.c` as the unit test does and
run as the Q3Test profile, is compiled into the same process, once from
HEAD and once with the change. The harness and its scripts stay in `.tmp`
and are not committed.

**Synthetic patches.** Each is one flat facet, so the rule alone decides.
Every release kept exactly the same facets:

| Patch | Retail (1.02 to 1.08) | fnTech3 before | fnTech3 after |
| --- | ---: | ---: | ---: |
| 64-unit square standing in the x = 0 plane, 8000 to 8191 units out along y (7 cases) | 1 | 1 | 1 |
| the same square 8192, 8193, 8200, 8300, 10000, 20000, 60000 or 65000 units out | 0 | 1 | 0 |
| square on z = 0, 4000, 4095 or 4096 units wide | 1 | 1 | 1 |
| square on z = 0, 4097, 4200, 5000 or 8000 units wide | 0 | 0 | 0 |
| strip 64 by 5000 units on z = 0, from y = 5000 (the 8192-unit square clips it to 3192) | 1 | 0 | 1 |
| the same strip from y = 3000 (inside the square, 5000 units long) | 0 | 0 | 0 |
| 64-unit square in the x = 65000 or 66000 plane | 1 | 1 | 1 |
| the same in the x = 70000, 99000, 100000, 103999 or 104090 plane | 1 | 0 | 1 |
| the same in the x = 104100, 104200 or 110000 plane | 0 | 0 | 0 |
| square on a 45-degree plane, 5000 or 9000 units out along y | 1 | 1 | 1 |

A square in the x = 104090 plane spans 4091 units across x when bounds start
at 99999 and is kept; at x = 104100 it spans 4101 and is dropped. The bounds
start therefore lies between 99994 and 100004 in every release, as the
`0x47c34f80` store says.

**Shipped patches.** Every patch of every shipped map, through Q3Test's own
translation of IBSP 44 and 45: each release's generator and the corrected
fnTech3's keep the same number of facets, each with the same number of
borders, and their planes agree within 0.0023 units.

| Release | Maps | Patches | Facets, retail = fnTech3 |
| --- | --- | ---: | ---: |
| 1.02 | Q3TEST1, Q3TEST2 | 217 | 1,320 |
| 1.03 | Q3TEST1, Q3TEST2 | 217 | 1,320 |
| 1.05 | Q3TEST1, Q3TEST2 | 209 | 1,190 |
| 1.06 | q3test1, q3test2, Q3TOURNEY | 435 | 1,800 |
| 1.07 | q3test1, q3test2, q3tourney (pak1) | 435 | 1,800 |
| 1.08 | q3test1, q3test2, q3tourney | 435 | 1,800 |

fnTech3's generator from before the change keeps exactly the same facets on
these maps, bit for bit, as after it: no map's world bounds reach farther
than 5,000 units from the origin, so every facet lies well inside the
square.

## Trace sweep before and after

**Method.** The synthetic game module, batteries and seed of the
[trace record](2026-10-08-q3test-traces.md), run on every shipped map of
every release, now including q3tourney in 1.06 to 1.08. Each map's retail
server ran once, and two fnTech3 dedicated servers ran the same module: one
built from an export of `e53ba098` and one from the same export with this
change (`cm_patch.c` only). The fnTech3 servers ran with networking off;
nothing connects to them. All on a private desktop; 1.02, 1.03 and 1.05
under the existing `NET_GetLocalAddress` breakpoint.

**Results.** The two fnTech3 servers wrote identical answers, every line and
bit, on all fifteen maps. Against retail:

| Release | Map | Traces: bit-exact / last bits / differ | Point contents equal | Links: same flags | Box queries: same entities |
| --- | --- | --- | --- | --- | --- |
| 1.02 | q3test1 | 5,244 / 786 / 0 | 1,500 | 7 of 7 | 200 of 200 |
| 1.02 | q3test2 | 6,262 / 848 / 0 | 1,500 | 19 of 19 | 200 of 200 |
| 1.03 | q3test1 | 5,244 / 786 / 0 | 1,500 | 7 of 7 | 200 of 200 |
| 1.03 | q3test2 | 6,262 / 848 / 0 | 1,500 | 19 of 19 | 200 of 200 |
| 1.05 | q3test1 | 5,225 / 805 / 0 | 1,500 | 7 of 7 | 200 of 200 |
| 1.05 | q3test2 | 6,232 / 878 / 0 | 1,500 | 19 of 19 | 200 of 200 |
| 1.06 | q3test1 | 4,780 / 1,250 / 0 | 1,500 | 7 of 7 | 200 of 200 |
| 1.06 | q3test2 | 5,670 / 1,440 / 0 | 1,500 | 19 of 19 | 200 of 200 |
| 1.06 | q3tourney | 5,039 / 1,259 / 2 | 1,500 | 10 of 10 | 200 of 200 |
| 1.07 | q3test1 | 4,780 / 1,250 / 0 | 1,500 | 7 of 7 | 200 of 200 |
| 1.07 | q3test2 | 5,670 / 1,440 / 0 | 1,500 | 19 of 19 | 200 of 200 |
| 1.07 | q3tourney | 5,044 / 1,254 / 2 | 1,500 | 10 of 10 | 200 of 200 |
| 1.08 | q3test1 | 4,780 / 1,250 / 0 | 1,500 | 7 of 7 | 200 of 200 |
| 1.08 | q3test2 | 5,670 / 1,440 / 0 | 1,500 | 19 of 19 | 200 of 200 |
| 1.08 | q3tourney | 5,045 / 1,255 / 0 | 1,500 | 10 of 10 | 200 of 200 |

In all, 97,740 traces (81,000 through the world, 16,740 around brush models),
22,500 point contents, 186 links and 3,000 box queries. The q3test1 and
q3test2 rows repeat the trace record's exactly. "Last bits" has the trace
record's meaning: every flag, contents value and entity agrees and the end
position is within 0.01 unit; the largest gap is 0.0026 units.

**Open: two traces on q3tourney.** The same two traces differ in 1.06 and in
1.07, before the change and after it, and in surface flags alone: retail
reports 1 where fnTech3 reports 0 for a player box, and 0 where fnTech3
reports 1 for a random box. Both start in solid and stop at bit-identical
fractions. 1.07 loads q3tourney from its `pak1.pk3`, the same file as
1.08's (1.06 has an earlier build), and on that file 1.08's retail server
and fnTech3 both report 0 for both traces. Inference, not investigated
further: each trace meets a curve and a brush at the same fraction and the
two engines report different ones; 1.06 and 1.07 give a curve the surface
flags of its shader's contents (1) where 1.08 does not, so the choice shows
only in those two releases.

## Correction

`CM_FacetSizes` (`code/qcommon/cm_patch.c`) gains a Q3Test entry: a square
reaching 8192 units out, bounded from +-99999, as for RTCW multiplayer.
`CM_ValidateFacet` keeps its own Q3Test extent test unchanged. Q3Test adds no
bevels, so `CM_AddFacetBevels`, which also reads the sizes, returns before
reading them.

## Limits

- No shipped map has a facet the rule changes, so no retail server trace
  shows it; the in-process runs are the evidence that retail applies it.
- The in-process runs build facets only. How a trace meets a facet is the
  sweep's evidence, on shipped geometry.
- The in-process runs use Windows' default x87 control word; a retail
  process whose startup chose another precision was not reproduced.
- Every retail engine runs in its restricted mode and loads no other map;
  that restriction was not lifted.

## Tests

- [`tests/alice_patch_collision_tests.py`](../../tests/alice_patch_collision_tests.py)
  builds a 64-unit square standing in the x = 0 plane: 10000 units out along
  y it is dropped for Q3Test and kept for Quake III, and 8000 units out both
  keep it. A square in the x = 70000 plane is kept for Q3Test, which bounds
  it from +-99999; from +-65535 it would span 4465 units and be dropped. Both
  checks fail with the previous sizes.
