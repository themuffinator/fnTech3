# Q3Test collision and linking against retail servers

The Q3Test gate matrix said its map and collision row had no trace comparison
against retail. This record asks every release's retail dedicated server and
fnTech3's the same questions through one synthetic game module: 78,840
traces, 64,800 through the world and 14,040 around brush models linked as
entities, plus 18,000 point contents, 156 entity links and 2,400 box queries
over both maps of all six releases. Three differences came out, and each is
now corrected for Q3Test alone:

- a trace that starts and ends inside a brush keeps the contents it already
  had, where fnTech3 wrote the brush's;
- a link whose box reaches only solid leafs, with no area among them, leaves
  the entity unlinked, where fnTech3 linked it;
- a link asks whether the game has the entity in use, and links nothing it
  has not, where fnTech3 linked it anyway.

After them every hit, flag, contents value, entity number, link and box
query agrees. The
[machine-readable record](2026-10-08-q3test-traces.json) holds identities,
bounds, seeds and every run's tallies.

## Method

A small game module (C, built with MSVC for x86, kept in `.tmp` and not
committed) implements each release family's own `GetGameAPI` tables from
[`code/compat/q3test_game_abi.h`](../../code/compat/q3test_game_abi.h):
family A for 1.02 and 1.03 and family B for 1.05, which load IBSP 44 maps,
and family C for 1.06 and 1.07 and family D for 1.08, which load IBSP 45.
When the level spawns (`SpawnEntities` in family A, `Init` in the others) it
asks the engine, through the release's own imports:

- **World:** for three hulls -- a point, the player box (-15,-15,-24 to
  15,15,32) and a random box up to 32 units -- and two masks (solid, and every
  bit), 300 each of three shapes: a long trace between random points in the
  world's bounds, a vertical drop through the whole world, and a short trace
  of at most 48 units. Then 1,500 point contents at random points.
- **In use:** a 32-unit box entity linked three times, with `inuse` clear,
  set, then clear again; its `linked` and `linkcount` after each.
- **Brush models:** every brush model `*k` of the map set on entity 300 + k
  with contents solid, moved by (8k, -4k, 2k) and, for even k, turned as well,
  then linked; its link flags and bounds. 90 traces through the space each
  model occupies, with a point and the player box, and 200 box queries
  (`ContactEntities` in families A and B, `EntitiesInBox` in C and D) around
  the models.

All inputs come from a seeded generator and from the module's own
arithmetic, never from values an engine wrote back, so both engines receive
the same floats. The module prints every answer as raw float bits and asks
the server to quit. The world's bounds and brush-model count are read from
each map and handed to the module as cvars.

The retail server runs from its release's fixed runtime copy
(`scripts/probe_q3test.py`'s slots), with the synthetic module in place of
`qagamex86.dll` for that one run; the retail module goes back afterwards.
fnTech3's dedicated server runs from a scratch folder holding the release's
own client modules and paks beside the synthetic module. The module carries
the retail module's PE timestamp, which is how fnTech3 names the release;
retail engines never read it. Both run `bot_enable 0`; retail 1.08 tests it
before asking the module for `GetBotAIAPI` (0x41b494), which the family C and
D builds export as a stub. All servers ran on loopback on a private desktop.
1.02, 1.03 and 1.05 need the existing `NET_GetLocalAddress` breakpoint under
cdb; nothing is patched.

## Results

Each release's retail `quake3.exe` against the corrected fnTech3 dedicated
server, seed 20261008:

| Release | Map | Format | World traces: bit-exact / last bits | Entity traces: bit-exact / last bits | Links: same flags | Box queries: same entities | Point contents equal |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 1.02 | q3test1 | IBSP 44 | 4,680 / 720 | 564 / 66 | 7 of 7 | 200 of 200 | 1,500 |
| 1.02 | q3test2 | IBSP 44 | 4,913 / 487 | 1,349 / 361 | 19 of 19 | 200 of 200 | 1,500 |
| 1.03 | q3test1 | IBSP 44 | 4,680 / 720 | 564 / 66 | 7 of 7 | 200 of 200 | 1,500 |
| 1.03 | q3test2 | IBSP 44 | 4,913 / 487 | 1,349 / 361 | 19 of 19 | 200 of 200 | 1,500 |
| 1.05 | q3test1 | IBSP 44 | 4,677 / 723 | 548 / 82 | 7 of 7 | 200 of 200 | 1,500 |
| 1.05 | q3test2 | IBSP 44 | 4,958 / 442 | 1,274 / 436 | 19 of 19 | 200 of 200 | 1,500 |
| 1.06 | q3test1 | IBSP 45 | 4,252 / 1,148 | 528 / 102 | 7 of 7 | 200 of 200 | 1,500 |
| 1.06 | q3test2 | IBSP 45 | 4,468 / 932 | 1,202 / 508 | 19 of 19 | 200 of 200 | 1,500 |
| 1.07 | q3test1 | IBSP 45 | 4,252 / 1,148 | 528 / 102 | 7 of 7 | 200 of 200 | 1,500 |
| 1.07 | q3test2 | IBSP 45 | 4,468 / 932 | 1,202 / 508 | 19 of 19 | 200 of 200 | 1,500 |
| 1.08 | q3test1 | IBSP 45 | 4,252 / 1,148 | 528 / 102 | 7 of 7 | 200 of 200 | 1,500 |
| 1.08 | q3test2 | IBSP 45 | 4,468 / 932 | 1,202 / 508 | 19 of 19 | 200 of 200 | 1,500 |

"Last bits" means the solid flags, surface flags, contents, entity and
whether the trace hit all agree, and the end position is within 0.01 unit:
every such trace was in fact within 0.0026 units, and its fraction within
0.0000044. No trace differs in anything else. 3,744 of the entity traces
stopped on a moved model. Every box query returned the same entities in the
same order. 150 of the 156 links have bit-identical bounds; the other six,
turned models on q3test2, differ by at most 0.0005 units. The in-use
sequence ends the same on every server: not linked, linked with `linkcount`
1, then unlinked with `linkcount` still 1. 1.02 and 1.03 share one pak, and
1.06 to 1.08 share their maps, so their rows match.

### Before the corrections

A first sweep, before the contents rule, ran the world battery on 1.05 and
1.08:

| Release | Map | Bit-exact | Last bits | Differ |
| --- | --- | ---: | ---: | ---: |
| 1.05 | q3test1 | 4,568 | 722 | 110 |
| 1.05 | q3test2 | 4,868 | 442 | 90 |
| 1.08 | q3test1 | 4,142 | 1,148 | 110 |
| 1.08 | q3test2 | 4,378 | 932 | 90 |

All 400 differing traces were all-solid: they started and ended inside a
brush. Retail's contents was 0 on 395 of them; on the other 5 it was a
different brush's, from an earlier hit in the same trace. fnTech3 had written
the containing brush's contents on all 400.

A second sweep, with the contents rule but not the link rules, ran both
batteries on every release. On q3test1 of 1.05 to 1.08, model `*7`, moved
into a wall, was linked by fnTech3 but not by retail: 15 of the 200 box
queries also returned it from fnTech3, and one trace stopped on it 12 units
short of where retail's went on. q3test1 of 1.02 and 1.03 is a different map,
and q3test2's models all stay in open space; they agreed. A separate run of
the in-use sequence on 1.02 and 1.08 left the entity linked all three times
on fnTech3, with `linkcount` 1, 2, 3.

## Retail facts

- Retail 1.08's `CM_TraceThroughBrush` (`0x0040ece0`) stores startsolid,
  fraction 0 and allsolid when a trace starts and ends inside the brush
  (`0x0040eea9`..`0x0040eed3`), and nothing else; it writes the contents only
  on a hit (`0x0040ef4d`). Its brush position test does write them
  (`0x0040ea3b`..`0x0040ea54`). The same three stores, with no contents
  store beside them, are in 1.02 (`0x0040d494`..), 1.03 (`0x0040d784`..),
  1.05 (`0x0040dc64`..), 1.06 (`0x0040e504`..) and 1.07 (`0x0040e544`..).
- Retail 1.08's `SV_LinkEntity` (`0x0041f740`, the game's `LinkEntity`
  import) unlinks the entity, returns if its `inuse` is clear
  (`0x0041f76b`..`0x0041f773`), and after asking for the box's leafs returns
  unless one of them had an area (`0x0041fa80`..`0x0041fa88`), before it
  counts the link or marks the entity linked (`0x0041fb6b`). 1.02's
  (`0x0041de30`) unlinks, skips the world and returns on a clear `inuse` the
  same way (`0x0041de66`..`0x0041de6e`), and every release has the same
  area test (1.02 `0x0041e183`, 1.03 `0x0041e7a3`, 1.05 `0x0041e643`, 1.06
  `0x0041f118`, 1.07 `0x0041f1b8`).
- Quake III 1.32's GPL source writes the brush's contents on that trace
  branch, links a box whose leafs are all solid, and does not look at
  `inuse`.

## Corrections

- A Q3Test map sets a collision flag beside its curve rules
  (`CM_Q3TestCurveRules`), and `CM_TraceThroughBrush` leaves the contents
  unchanged on that branch while it is set. Q3Test's curves already kept them
  when a start was found inside one, and position tests still write them.
- `SV_LinkEntity` leaves a Q3Test entity unlinked when its leafs give no area,
  as it already did when there were no leafs, without counting the link.
- The Q3Test `LinkEntity` import unlinks an entity whose `inuse` is clear and
  links nothing more.

The flag and both tests are Q3Test's alone; every other profile's traces and
links are unchanged.

Inferences: the remaining last-bit differences come from arithmetic, not
rules. Retail computes in x87 registers; fnTech3 computes in SSE with
double-precision dot products. They are far below what the wire carries.
The curve rules that differ between releases -- 1.06 and 1.07's surface flags
read from a curve's shader contents, and 1.02 to 1.05's contents forced from
one shader -- agree on every trace that met a curve, though the battery does
not separate those traces from the rest.

## Limits

- No zero-length trace, the position test, is compared.
- Entities are brush models and one box; no client entity, owner or pass
  entity is exercised.
- The IHV Test's collision was not compared.

## Tests

- [`tests/collision_capsule_trace_tests.c`](../../tests/collision_capsule_trace_tests.c)
  links the real `cm_trace.c` with a 128-unit cube in its stub world. With
  the flag set, a sweep inside the cube is all-solid and keeps contents 0;
  without it, the sweep reports the cube's contents. A sweep into the cube and
  a position test report the cube's contents either way.
- [`tests/q3test_link_rules_tests.py`](../../tests/q3test_link_rules_tests.py)
  runs the real area test and `LinkEntity` import against stubs: a link with
  leafs but no area stays unlinked in Q3Test alone, an entity not in use is
  unlinked and never linked, and families A and B still never link the world.
