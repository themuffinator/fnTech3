# Jedi Academy's skin gore, 2026-10-17

When a hit lands on a Jedi Academy multiplayer (`jka-mp`) player, the client
game asks the engine to leave a mark on the player's Ghoul2 model:
client-game import 314, `CG_G2_ADDSKINGORE`. Before it does, it asks how many
marks the model already has, import 313, `CG_G2_GETNUMGOREMARKS`, and adds
none past its own limit. On respawn it clears them, import 315 (the
[skin-gore clear slice](2026-10-16-jka-clear-skin-gore.md)). fnTech3 refused
313 and 314 and kept nothing for 315 to clear. This slice serves all three
as retail `jamp.exe` 1.01 does, as data: the marks are made, counted, shared,
expired and cleared, but Fn's renderer does not draw them yet. The
[JSON companion](2026-10-17-jka-skin-gore.json) records the binaries, retail
identities, commands and outcomes. No compatibility gate closes.

Implementation is independently authored. Retail `jamp.exe` 1.01 and the
1.01 `cgamex86.dll` in `assets3.pk3` were read with capstone and pefile.
Pinned OpenJK was used to name routines and as a map of the gore code, each
step then checked in the retail binary (reference observation; no text
incorporated).

## Retail

### Observed, `jamp.exe` 1.01: the imports

- **313.** The dispatcher (`0x412170`) takes import 313 to index 189 and
  `0x413c9d`. It indexes the instance's 0xc8-byte model records through the
  client game's word and `args[2]` with no bound (`0x4116e0`; a null word
  faults at `0x4116e9`), then `0x4e07a0` reads the record's gore-set word
  (`+0x4c`), finds the set in the set map (`0x12a5048`) and answers its mark
  count (`+0x10`), or 0 with no word or no set.
- **314.** Index 190, `0x413cb6`: maps `args[2]`, the client game's
  0x90-byte gore record, calls `0x4e08f0` with the word and answers 0.
- **315.** `0x413cd5` and `0x4e07f0` delete every model's gore set
  (`0x4e9310`) and zero its word.

### Observed: adding gore (`0x4e08f0`)

1. A shot direction (`+0x20`) shorter than 0.1 (`[0x5661ec]`) adds nothing;
   only then is the instance read.
2. The skeleton is built (`0x49a050`) and the world matrix made from the
   record's angles and position (`0x4ec580`: forward, left and up from
   `AngleVectors` `0x443f50` as columns), then inverted (`0x4ec620`:
   transposed, translation carried back). The hit (`+0x2c`) and the direction
   go through it into model space.
3. The per-call tag map is emptied and a new tag group begun (`0x4e91a0`:
   tags restart at the group counter `[0x56ce64]`, which steps by 0x100;
   the counters start at 0x101 and 0x100).
4. The first LOD is max(model 0's bias `+0x44`, `r_lodbias`)
   (`[0xfe30f4]`, registered at `0x4a23e5` with default "0"), lowered to
   model 0's LOD count less one, then kept within 0.0 and 2.0. The LODs run
   up to min(model 0's LOD count, 3.0) (`[0x566270]`).
5. For each LOD the models are skinned (`0x4e9ce0`) and traced
   (`0x4ec1c0`). The trace skips empty, invalid and no-collide models and
   any whose LOD is not this one, and, unless `cg_g2MarksAllModels`
   (made at first use, `0x4ec1c6`, default "0") is set, stops after the first
   model it processes. It walks the surfaces from the model's root surface
   (`+0x40`) down (`0x4ec0a0`): a surface whose flags (its override in the
   model's list, else its file flags) are zero takes gore, and a surface with
   NODESCENDANTS (0x100) ends that branch.

### Observed: one surface (`0x4ea460`)

- Two axes come from the model-space shot: the cross product with
  (0, 0, 1), or with (0, 1, 0) when that one's squared length is under 0.1,
  and the cross of the shot with that; both normalized. With c and s the
  cosine and sine of theta (`+0x4c`), the T axis is the first times
  0.5c/TSize plus the second times 0.5s/TSize, and the S axis the first
  times -0.5s/SSize plus the second times 0.5c/SSize (`+0x44`, `+0x48`).
- Every vertex of the LOD's surface (five floats each) gets
  s = (vertex - hit) . S + 0.5 and t likewise, and four bits: s > 0, s < 1,
  t > 0, t < 1, with no margin. If every vertex misses the same edge, the
  surface is skipped.
- A triangle is kept unless all three corners miss the same edge. When
  front or back faces are excluded (`+0x58`, `+0x5c`), its normal
  (edge 1 x edge 2) is dotted with the shot: positive asks for front faces,
  otherwise back faces.
- The kept corners are numbered in first-use order. If any triangle is
  kept, the first such surface of the model in this call takes a new record
  (`0x4e9070`) and a mark in the model's set (made if the word names none,
  `0x4e9250`, one reference). The mark keeps the shader (`+0x84`), a delete
  time (the Ghoul2 clock, `[0x10bcb78]` else `[0x10bcb74]`, plus the
  lifetime `+0x64`, or 0 for none), the fade time (`+0x68`), fade-RGB
  (`+0x8c`), the growth start and end (end -1 for a duration of -1, `+0x50`)
  and growth rate (1 - start fraction `+0x54`) / duration. Later LODs of the
  same call reuse the record and fill their own slot.

### Observed: records and sets

- Records live in a tag map (`0x12a5030`). Before a new one is made, while
  more than 500 exist (`0x1f4`), the oldest tag's whole group (same bits
  above 0xff) is deleted.
- A set's reference count is a byte at `+4`. Deleting a set drops one
  reference, or, at one or none, deletes the set and its records.
- Duplicating an instance (`0x4e03b0`) copies every model's word and adds a
  reference to each set: the copy shares the marks. Copying one model
  (`0x4dea40`) copies the word with no new reference. Cleaning an instance
  (`0x4de520`) clears its gore first.

### Observed: the renderer (`0x498740..0x4989ba`)

When a surface of a drawn model is not switched off and the model is not
the personal model (`[ebp+0x10]`), each of that surface's marks whose record
is gone, or whose delete time has come on the Ghoul2 clock, leaves the set.
An expired mark's record has its data freed (`0x4966a0`) but keeps its entry
in the record map, so it still counts toward the 500.

### Observed: `cgamex86.dll` 1.01

- Import 313's wrapper (`0x3005f550`) and 314's (`0x3005f570`) each have
  one caller, in `0x3004c580`. It asks 313 for model 0, returns once the
  count reaches the cvar at `[0x306f056c]`, then zeroes a 0x90-byte record,
  sets both faces, a growth duration of -1 and a start fraction of 1, and
  calls 314 with the hit, shot, size, theta, lifetime and shader.
- `0x3004c580` is called from `0x30036f80` (twice), `0x3004c700` (twice)
  and `0x3003d7ed`. The first two trace with the Ghoul2 flag set
  (`0x30056c30` and `0x30056ba0` to `0x30056760`, and `0x3004c2d0`), and
  that trace reaches import 299 (wrapper `0x3005f2a0`, push 299 at
  `0x3005f2dc`).

### Inferred, from OpenJK

- `0x3004c580` is `CG_AddGhoul2Mark`, the cvar is `cg_ghoul2Marks`,
  `0x3004c700` is `CG_G2MarkEvent` and `0x3004c2d0` `CG_G2TraceCollide`.
- The renderer draws each mark as a second pass over the surface's kept
  triangles with the mark's shader (or `gfx/damage/burnmark1`), its texture
  coordinates scaled while growing and its colour or alpha faded over the
  fade time.

## fnTech3

### Before

The client's Academy Ghoul2 store kept no gore. Imports 313 and 314 were
refused by number, and 315 only validated its word.

### Now

- `code/client/client_jka_cgame_gore.hpp` keeps retail's tables for the
  client game: records by tag with their per-LOD vertex lists, coordinates
  and triangles; sets with a byte reference count and surface-keyed marks;
  each model's set word by instance and place; the per-call tag map and the
  tag counters. Allocation, the 500 cap, set deletion, duplication, copy,
  clear, the per-surface projection, the surface walk and render expiry
  follow the retail rules above.
- **313** requires the x86 native client game, refuses a null, stale or
  foreign word and a place outside the instance (retail reads past its
  records), and answers the set's mark count.
- **314** requires the native client game in its cgame context, refuses a
  null gore record and any non-finite angle, position, shot, hit, scale,
  size, theta or start fraction, and adds nothing for a shot shorter than
  0.1 without reading the instance. It then refuses a null, stale or foreign
  word, an instance without a loaded model 0 (retail reads it unchecked) and
  a missing frame clock. It poses each model it traces with the store's
  linked palette at the Ghoul2 clock (refusing a pose it cannot serve),
  loads and skins that LOD's geometry with Fn's Ghoul2 geometry, applies the
  record's scale (zero counting as one) and walks from surface 0 with the
  same override flags the renderer and import 321 use. Fn models carry no
  LOD bias, since no served import sets one. `r_lodbias` is read as retail
  reads it; Fn's default of -2 gives the same first LOD as retail's 0.
- **315** now deletes every model's set, as retail does.
- Duplicate (309), single-model copy (308) and clean (301) carry, share or
  clear the words as retail does. Clearing the client game drops the tables.
- The render flush expires marks on each drawn surface of every model it
  draws, except on an entity with `RF_THIRD_PERSON`, at the Ghoul2 clock.

### Differences kept

- The marks are not drawn. Fn's skeletal models are registered as runtime
  MDR models, one renderer model slot each, and registrations are never
  freed; drawing a mark per hit that way would exhaust the slots. Drawing
  needs a renderer path of its own.
- Retail expires marks only for models that survive its frustum cull and
  treats a third-person model as personal only outside portal views. Fn's
  flush runs before culling and does not know the portal state, so it
  expires marks on every submitted model that is not third-person. A model
  out of view may therefore lose expired marks earlier than in retail, which
  can let the client game add marks sooner.
- The arithmetic is single precision with Fn's own rounding, not retail's
  x87 evaluation; a vertex lying exactly on a splotch edge may fall the
  other way.
- Retail's tables are engine-wide and survive a client-game restart; Fn's
  belong to the client game and are cleared with it.

### Presentation

Nothing is drawn yet (see above). When the marks are drawn, invariant 11
applies: the mark's shader and its kept triangles and coordinates are the
data; Fn's skeletal renderer, lighting and materials present them.

## Evidence

### Unit

`tests/jka_cgame_skin_gore_tests.py` compiles the production Ghoul2, gore
and render headers into the client-game render fixture of
`jka_cgame_render_tests.py`, drives imports 313, 314, 315, 321, 308, 309 and
301 through the production dispatcher, and draws through the production
render flush:

- The projection on a hand-made surface: exact splotch coordinates for six
  vertices, a triangle near the s = 1 edge kept, one with corners past
  different edges kept, one wholly past an edge cut, a second LOD sharing
  the mark, and a surface wholly past an edge filing nothing.
- On a posed three-surface, four-LOD mesh: the marks and every LOD's record
  against an independent double-precision computation of the same rules;
  LODs 0 to 2 of 4, `r_lodbias` 1 and 9, and a two-LOD model; surfaces off by
  file flags, by import 321 and under NODESCENDANTS; a shot straight down z;
  front and back faces; scale, zero scale and a moved, turned entity;
  growth fields; per-call tag groups.
- Model 0 alone by default and every model with `cg_g2MarksAllModels`;
  duplicates sharing a set with two references; a single-model copy without
  one; clean; refusals for null, stale and foreign words, a null record,
  non-finite values, a missing model 0, a missing frame clock, a non-native
  client game and an out-of-range place; a short shot that reads no word.
- Expiry when drawn, none on a personal model, none on a switched-off
  surface, none without a lifetime, the record entry kept empty; the 500 cap
  evicting the oldest call's records and the draw then dropping their marks;
  the client-game clear dropping the tables.
- 42 mutations, each failing at run time, across the cases, the gore
  header and the render hook. A second test pins the retail addresses.

`tests/jka_cgame_clear_skin_gore_tests.py` now expects 313 to answer 0
where it expected 313 and 314 to be unserved. The shared Ghoul2 fixture
gained `r_lodbias` and `cg_g2MarksAllModels` stubs, and the clock test a
gore-clear stub.

### Live

Each probe ran the client in an 800x600 window on a private desktop with
`net_enabled 0`, sound off, no input and engine screenshots only, through
`.tmp/round-2026-10-15/jka-local-sets/probe_live.py`. The runtimes were built
from a `git archive` export advanced path for path to `f4794432`, without
(before) and with (after) this slice, Windows x86 debugoptimized with strict
warnings. The retail installation and the binaries were unchanged after
every run.

| Run | Runtime | Steps | Outcome |
| --- | --- | --- | --- |
| `gore-before-korriban-bot` | before | `mp/siege_korriban` (`g_gametype 7`): `team red`, `addbot Tavion 1 blue`, a capture every 400 frames | Tavion dies, `ircg 1 32 0 0`, `rcg 1`; six captures; "Academy cgame unsupported Raven renderer entity flags" |
| `gore-after-korriban-bot` | after | the same | the same sequence and stop |
| `gore-before-ffa3-npc` | before | `mp/ffa3`: `g_forcerespawn 1`, `team free`, `togglemenu`, two `npc spawn stormtrooper` | "Unsupported jka-mp cgame import 299 (CG_G2_COLLISIONDETECT)" |
| `gore-after-ffa3-npc` | after | the same | the same stop |

Neither runtime reached import 313 or 314: every client-game path to them
traces through import 299 first. These runs show only that the slice
changes nothing on the paths that do run.

## Validation

- The export at `f4794432` with this slice built every target of the client
  configuration with strict warnings and no warning. Its native suite
  passed 274 of 276 tests and skipped two (OpenAL loopback and the optional
  Quake Live factory probe).
- 76 Python test files, one file to a process, passed 258 tests and 1,365
  subtests with one skipped: every `jka_*` file, both render-record adapter
  files, and the changelog, generated-docs, release-packaging,
  release-workflow, version and player-manual files.

## Remaining and unrelated findings

- Client-game import 299 (`CG_G2_COLLISIONDETECT`) stands before every use
  of 313 and 314; serving it would let the marks be made live. It shares
  this slice's posing, skinning and surface walk.
- The marks need a renderer path; runtime MDR registrations are never freed,
  which also affects the existing topology re-registrations of
  `client_jka_cgame_render.hpp`.
- Raven's `RF_DISINTEGRATE2` (0x40000) on a corpse is still refused.
