# RTCW entity queries: state origins and the carried prop

The [entity box contents record](2026-09-23-entity-box-contents.md) left two
RTCW differences open in the server's entity queries. Both are now followed,
and the synthetic game module of the [RTCW trace record](2026-10-08-rtcw-traces.md)
shows each against the retail `WolfSP.exe` 1.32 and `WolfMP.exe` 1.41
dedicated servers: every answer that had differed now matches. The
[machine-readable record](2026-10-08-rtcw-entity-queries.json) holds the runs.

## What the engines do

Observed in the retail executables:

- **Point contents.** `SV_PointContents` places each entity at its
  `s.origin`, turned by its `s.angles`, boxes included (`WolfSP.exe`
  `0x43a1ba..0x43a1c4`, `WolfMP.exe` `0x43d60a..0x43d614`), as the Quake III
  IHV Test's does. fnTech3 placed it at `r.currentOrigin`, with
  `r.currentAngles` and none for a box, as it still does for the other
  profiles.
- **The carried prop.** `WolfSP.exe`'s `SV_ClipMoveToEntities`, after the
  owner and contents tests, passes over an entity whose `s.eType` is 32, RTCW
  SP's `ET_PROP`, and whose `s.otherEntityNum` is the pass entity's number
  plus one (`0x439f07..0x439f16`). `WolfMP.exe` goes from the contents test
  straight to the clip handle (`0x43d31d..0x43d32f`).

`code/server/sv_world.cpp` now does both, by profile.

## Runtime check

The module gained a battery: a player-sized box entity and a capsule-flagged
one whose `s.origin` lies 48 and 40 units off `r.currentOrigin`, with turned
`s.angles`, and brush model `*2` moved and turned the same way, then 400
point contents around them; then an `ET_PROP` entity whose
`s.otherEntityNum` is pass entity 290 plus one, and one that names 290
itself, swept 60 times each with box and point hulls, with 290 as the pass
entity and with none. Seed 20261008, 100 traces of each world shape:

| Map | Build | Traces equal | Prop sweeps differing | Entity point contents equal |
| --- | --- | --- | ---: | --- |
| escape1, village1 (SP) | before | 7,610 of 7,620 each | 10 each | 597 of 600 each |
| escape1, village1 (SP) | after | 7,620 of 7,620 each | 0 | 600 of 600 each |
| mp_beach, mp_assault (MP) | before | 7,620 of 7,620 each | 0 | 597 of 600 each |
| mp_beach, mp_assault (MP) | after | 7,620 of 7,620 each | 0 | 600 of 600 each |

"Equal" counts answers bit-exact or within 0.01 unit with every flag the
same. Before, fnTech3 stopped single-player sweeps on the carried prop that
`WolfSP.exe` passed over, and in both modes reported a body at three points
where the entities stood by `r.currentOrigin` but not by `s.origin`.

`tests/entity_box_contents_tests.py` checks both rules through the engine's
real traces for every profile it runs.

Inference: the carried prop is the chair or other object an RTCW SP player
picks up, so a sweep from the carrier does not stop on what it carries.

## Limits

- The point-contents rule is shown at three points per map; more would need
  points drawn where the two placements part.
- Enemy Territory's `et.exe` places an entity at `r.currentOrigin` and
  `r.currentAngles` (entity box contents record) and is unchanged, as are
  Quake III and Quake Live.
