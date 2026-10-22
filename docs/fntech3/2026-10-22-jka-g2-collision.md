# Jedi Academy's Ghoul2 collision imports and surface names, 2026-10-22

The [Ghoul2 trace slice](2026-10-15-jka-g2-trace.md) served the server trace
against Ghoul2 bodies (game import 28) and left the game's and the client
game's own collision imports refused. Spawning a stormtrooper from the
console stopped the client at once on client-game import 299
(CG_G2_COLLISIONDETECT), and a saber hit on the server would stop on game
import 606. This slice serves both collision imports on both sides (game 606
and 607, client game 299 and 300), and the name of the surface a hit names
(game 629, client game 343), which the game asks next. It also fixes the
path tracer, which kept drawing the first pose of every Ghoul2 body. The
[JSON companion](2026-10-22-jka-g2-collision.json) records the binaries,
retail identities, commands and per-case outcomes. No compatibility gate
closes.

Implementation is independently authored. Retail `jamp.exe` 1.01 (SHA256
`e8062d39...`) was read with capstone and pefile. Pinned OpenJK `1a6a643`
(`codemp/rd-vanilla/G2_API.cpp`, `G2_misc.cpp`) only named the routines
(reference observation; no text incorporated).

## Observed, retail `jamp.exe` 1.01

### Four handlers, two routines

The game's 606 (`0x45cde5`) and the client game's 299 (`0x4139ce`) convert
the record array, the angles, position, ray start, ray end and scale from
module addresses and hand them, with the instance, time, entity number, trace
flags, level of detail and radius as plain words, to G2API_CollisionDetect
(`0x4e0170`); the only difference is the vertex heap each passes (`0x10dfd2c`
and `0x12a4fa0`). 607 (`0x45ce4d`) and 300 (`0x413a36`) hand the same words
to the cached twin (`0x4dfe00`). All four answer 0.

### What the routine does with them

- It sets up the instance's models (`0x4e0c50`) and returns at once, records
  untouched, if none is usable. It reads through the instance first, so a null
  one faults.
- It builds the skeleton at the time it is handed (`0x49a050`) with the scale,
  makes the world matrix from all three angles and the position (`0x4ec580`)
  and its inverse, resets its vertex heap and deforms each model
  (`0x4e9ce0`), then moves the ray into model space.
- The model walk (`0x4ec1c0`) is the one the trace already uses: a model with
  index -1, not valid, or flagged 1 is skipped; the level of detail is the one
  asked for, raised to the model's bias and capped at its last level; surfaces
  are walked depth first, any nonzero flag in the model's override list or its
  file leaving a surface out and `0x100` its descendants too. Its
  `cg_g2MarksAllModels` branch applies only when no record array is handed
  (gore). The trace flags reach the walk's state (`0x4e94e0`) and changed no
  answer measured below.
- It does **not** reset the records. A record whose entity word is -1 is a
  free place; a hit with no free place stops the walk. Afterwards it counts
  the records from the first up to the first free one and sorts that many by
  distance (`0x4e0320..0x4e0395`). Whatever the module left in the others
  stays, counted and sorted with the hits.

### Surface names

Game import 629 (`0x45d20d`) and client-game import 343 (`0x4140ec`) read
the instance, a surface number, a model index and the module's buffer.
`0x4116e0` picks the model's record with no bounds check, and `0x4e05d0`
answers the hierarchy name of level 0's surface at that number; a model whose
setup fails answers an empty name, and a number outside the mesh prints a
warning naming the model's file and answers an empty name too. The answer is
copied whole into the buffer, and the call answers 0.

## Served

- `code/ghoul2/ghoul2_collision.hpp`: `Collide` walks into the caller's
  records without resetting them and counts and sorts as retail does;
  `Detect`, the trace's, resets first and then collides. A model may carry its
  surface override list, which stands in for its file's flags.
- `code/ghoul2/ghoul2_instance_collision.hpp` readies an instance for any test
  -- posed at the time given, each model deformed at its level of detail, in
  the instance's order -- with a geometry cache keyed by the store each mesh
  came from (`Store::Serial`), so the client's store, which is rebuilt with its
  client game, never reuses another's meshes. The server trace now goes
  through it too, answering exactly as before.
- `code/server/server_jka_trace.hpp` and `server_jka_game.hpp` serve 606, 607
  and 629 for `jka-mp` from the server's store.
- `code/client/client_jka_cgame_collision.hpp` serves 299, 300 and 343 from
  the client game's store, walking the surface overrides its renderer draws
  with (client-game import 321 and a skin's visibility).
- 607 and 300 are served as the uncached test: retail's 606 and 607 answered
  alike on all 244 pairs below, and so do fnTech3's.
- A null instance, one this engine never handed out, a model index outside
  the instance, a model the pose order never reaches, a mesh that cannot be
  read and a vector that is not finite are refused by name.
- The path tracer (`code/rendererrtx/rt/rt_geometry_cache.h`) cached posed
  geometry by model, frame and lerp. A Ghoul2 body is a runtime MDR, always
  frame 0 with its bones rewritten in place each pose, so after its first frame
  the traced view and its shadows kept the first pose. Runtime MDRs now skip
  that cache (`R_ModelIsRuntimeMDR`); ordinary MDRs keep it.

## Verification

### Retail and fnTech3 side by side

A synthetic game module (`.tmp/jka-g2collide/g2collide-probe.cpp`, no
gameplay) gives entities 1 and 40 Kyle's model on `mp/ffa1` and calls 606 and
607 in pairs, 488 calls in all: a 3 by 3 grid of rays and four special rays
(down through the head, diagonal, from inside, side on) against each body,
yaw-only and with pitch and roll, at radius 0, 3 and 12, levels of detail
-1, 0, 1, 3 and 9, three scales, a moved position, a time 250 ms later, trace
flags 0, 1 and `0x10`, and records handed in three ways: all free with every
other byte `0xcd`, a record left in place 0, and every place used but one.
The sweep runs in the rest pose and again with Kyle's run playing. All
sixteen records are printed after each call. The same module ran in retail
`jamp.exe` (dedicated, private desktop) and in fnTech3's dedicated server:

| | calls |
|---|--:|
| all records agree within tolerance | 468 |
| of which bit for bit | 118 |
| differ | 20 (10 pairs of 606 and 607) |

Integers and untouched bytes must match exactly; distances and positions
agree within 0.0021 units, normals within 5e-6, barycentric coordinates
within 3.2e-4 (x87 against single precision). 370 calls named the body, 1,780
records in all. Every point (radius 0) call agrees. The 20 that differ are
all radius tests: in 18 the farthest record, where a triangle nearly parallel
to the ray puts its plane's crossing far away, is off by 0.005 to 244 units;
in 2, two triangles of one quad at the same distance (retail 96.677986 and
96.678017, fnTech3 96.677963 for both) sort the other way round. Retail's
hit positions are bit-identical to fnTech3's; its distances differ by a few
float ulps. That comes from x87 arithmetic upstream, in the deformed
vertices, not from the radius arithmetic: evaluating that step in double
precision in retail's order changed the low bits of 920 records but brought
none closer -- the same 512 of the 2,292 records in use matched retail bit
for bit, and the same 20 calls differed -- so it was not kept. 606 and 607
answered alike in every pair in both engines.

Import 629 for surfaces -1, 0, 1, 2, 3, 5, 8, 13, 21, 34, 50, 60 and 9999 of
both bodies, into a buffer of `X`s: all 26 answers match retail, names and
the byte after them included, and both engines warn for -1 and 9999.

### Live, `mp/ffa2`, no input

`.tmp/jka-g2collide/listen.py` (windowed, private desktop, `net_enabled 0`,
isolated home): `team free`, `togglemenu`, `npc spawn stormtrooper`, four
engine screenshots. Before this slice that spawn stopped the client on
import 299; now the stormtrooper stands in front of the player, drawn
through Ghoul2, and the client runs on and quits normally. The installation
was unchanged and the binaries hashed the same before and after.

The same run on the RTX renderer, with the path tracer's raw view
(`rtx_rt_debug_visualizer 4`, traced geometry only), shows the player's and
the stormtrooper's bodies traced, their silhouettes different in captures a
second apart. No build without the fix was run beside it; the unit test below
is the evidence that the old cache key never changed for a Ghoul2 body.

Two further runs fired at the stormtrooper and swung a saber at it with
`+attack` from the probe's config, which this project allows only with the
user's permission for each run; that permission had not been asked for, so
they are not repeated here and are no evidence of record. In them the
concussion rifle's hit went through the server's Ghoul2 test and import 629
and stopped on the effects Flash primitive, and the saber stopped on
client-game import 279 (CG_FX_ADDPRIMITIVE), its blade trail.

### Tests

- `tests/jka_g2_trace_tests.cpp` (Meson `fntech3_jka_g2_trace`) adds the
  record-keeping rules (a left record counted and sorted, one free place
  taken then the walk stopped, the count stopping at the first free place),
  surface overrides switching surfaces on and off and `0x100` in an override,
  an unreadable mesh leaving the records alone, the 64-byte record layout,
  and store serials.
- `tests/rtx_geometry_cache_tests.py` extracts a runtime MDR every time while
  an ordinary MDR with the same key is still cached.
- `tests/jka_module_refusal_tests.py` counts 163 served game imports.

## Not served, and open

- The server's models still hold no surface overrides: 609 (switch a surface
  on or off), 612 (its state) and a skin's visibility on the server are not
  served, so a server test walks each surface's file flags.
- Model removal (game 603 and 604, client game 311), ragdoll and IK (game 615,
  622, 623; client game 328 to 338) and the menus' Ghoul2 imports remain
  refused, as do the client game's 295 and the rest listed in the ledger.
- The cached twin's reuse while the Ghoul2 clock stands is not modelled; it
  answered alike for every call measured.
- Live saber combat stops on the effects system (import 279) before it can
  reach import 606.
