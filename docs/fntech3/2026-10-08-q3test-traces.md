# Q3Test collision against retail servers

The Q3Test gate matrix said its map and collision row had no trace comparison
against retail. This record asks every release's retail dedicated server and
fnTech3's the same world traces and point contents through one synthetic
game module: 64,800 traces and 18,000 point contents over both maps of all
six releases. One difference came out: a trace that starts and ends inside a
brush. Retail leaves its contents alone there, and fnTech3 wrote the brush's.
That is now corrected for Q3Test maps, after which every hit, flag and
contents value agrees. The
[machine-readable record](2026-10-08-q3test-traces.json) holds identities,
bounds, seeds and every run's tallies.

## Method

A small game module (C, built with MSVC for x86, kept in `.tmp` and not
committed) implements each release family's own `GetGameAPI` tables from
[`code/compat/q3test_game_abi.h`](../../code/compat/q3test_game_abi.h):
family A for 1.02 and 1.03 and family B for 1.05, which load IBSP 44 maps,
and family C for 1.06 and 1.07 and family D for 1.08, which load IBSP 45. It
spawns nothing. When the level spawns (`SpawnEntities` in family A, `Init`
in the others) it asks the engine, through the release's `Trace` and
`PointContents` imports:

- for three hulls -- a point, the player box (-15,-15,-24 to 15,15,32) and a
  random box up to 32 units -- and two masks (solid, and every bit), 300 each
  of three shapes: a long trace between random points in the world's bounds,
  a vertical drop through the whole world, and a short trace of at most 48
  units;
- then 1,500 point contents at random points.

The points come from a seeded generator inside the module, so both engines
receive the same float inputs. The module prints every answer as raw float
bits and asks the server to quit. The world's bounds are read from each map's
model 0 and handed to the module as cvars.

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
server, seed 20261008, 5,400 traces and 1,500 point contents per map:

| Release | Map | Format | Bit-exact | Last bits | Differ | Largest position difference | Point contents equal |
| --- | --- | --- | ---: | ---: | ---: | ---: | ---: |
| 1.02 | q3test1 | IBSP 44 | 4,680 | 720 | 0 | 0.0026 | 1,500 |
| 1.02 | q3test2 | IBSP 44 | 4,913 | 487 | 0 | 0.0013 | 1,500 |
| 1.03 | q3test1 | IBSP 44 | 4,680 | 720 | 0 | 0.0026 | 1,500 |
| 1.03 | q3test2 | IBSP 44 | 4,913 | 487 | 0 | 0.0013 | 1,500 |
| 1.05 | q3test1 | IBSP 44 | 4,677 | 723 | 0 | 0.0024 | 1,500 |
| 1.05 | q3test2 | IBSP 44 | 4,958 | 442 | 0 | 0.0007 | 1,500 |
| 1.06 | q3test1 | IBSP 45 | 4,252 | 1,148 | 0 | 0.0020 | 1,500 |
| 1.06 | q3test2 | IBSP 45 | 4,468 | 932 | 0 | 0.0005 | 1,500 |
| 1.07 | q3test1 | IBSP 45 | 4,252 | 1,148 | 0 | 0.0020 | 1,500 |
| 1.07 | q3test2 | IBSP 45 | 4,468 | 932 | 0 | 0.0005 | 1,500 |
| 1.08 | q3test1 | IBSP 45 | 4,252 | 1,148 | 0 | 0.0020 | 1,500 |
| 1.08 | q3test2 | IBSP 45 | 4,468 | 932 | 0 | 0.0005 | 1,500 |

"Last bits" means the solid flags, surface flags, contents, entity and
whether the trace hit all agree, and the end position is within 0.01 unit.
Every such trace was in fact within 0.0026 units, and its fraction within
0.0000044. 1.02 and 1.03 share one pak, and 1.06 to 1.08 share their maps,
so their rows match.

A first sweep, before the correction, ran 1.05 and 1.08 only:

| Release | Map | Bit-exact | Last bits | Differ |
| --- | --- | ---: | ---: | ---: |
| 1.05 | q3test1 | 4,568 | 722 | 110 |
| 1.05 | q3test2 | 4,868 | 442 | 90 |
| 1.08 | q3test1 | 4,142 | 1,148 | 110 |
| 1.08 | q3test2 | 4,378 | 932 | 90 |

Facts:

- All 400 differing traces were all-solid: they started and ended inside a
  brush. Retail's contents was 0 on 395 of them; on the other 5 it was a
  different brush's, from an earlier hit in the same trace. fnTech3 had
  written the containing brush's contents on all 400.
- Retail 1.08's `CM_TraceThroughBrush` (`0x0040ece0`) stores startsolid,
  fraction 0 and allsolid on that branch and nothing else
  (`0x0040eea9`..`0x0040eed3`); it writes the contents only on a hit
  (`0x0040ef4d`). Its brush position test does write them
  (`0x0040ea3b`..`0x0040ea54`).
- The same three stores, with no contents store beside them, are in 1.02
  (`0x0040d494`..), 1.03 (`0x0040d784`..), 1.05 (`0x0040dc64`..), 1.06
  (`0x0040e504`..) and 1.07 (`0x0040e544`..). The JSON lists each address.
  Quake III 1.32's GPL source writes the brush's contents there.

## Correction

A Q3Test map now sets a collision flag beside its existing curve rules
(`CM_Q3TestCurveRules`), and `CM_TraceThroughBrush` leaves the trace's
contents unchanged on that branch while the flag is set. Q3Test's curves
already kept them when a start was found inside one. Position tests still
write the contents, as every release's do. The flag is zero on every other
profile's map, so their traces are unchanged.

Inferences: the remaining last-bit differences come from arithmetic, not
rules. Retail computes in x87 registers; fnTech3 computes in SSE with
double-precision dot products. They are far below what the wire carries.
The curve rules that differ between releases -- 1.06 and 1.07's surface flags
read from a curve's shader contents, and 1.02 to 1.05's contents forced from
one shader -- agree on every trace that met a curve, though the battery does
not separate those traces from the rest.

## Limits

- The battery traces the world alone: no entities, brush models or
  zero-length position tests are compared.
- The IHV Test's collision was not compared.

## Tests

[`tests/collision_capsule_trace_tests.c`](../../tests/collision_capsule_trace_tests.c)
links the real `cm_trace.c` with a 128-unit cube in its stub world. With
the flag set, a sweep inside the cube is all-solid and keeps contents 0.
Without it, the sweep reports the cube's contents. A sweep into the cube and
a position test report the cube's contents either way.
