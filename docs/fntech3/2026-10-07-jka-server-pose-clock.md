# Jedi Academy server pose clock, 2026-10-07

The [server attachments record](2026-10-06-jedi-server-attachments.md) gave
Jedi Academy's server a Ghoul2 store of its own and left three things open:
the time a bolt query poses at, active transitions between animation clips,
and the fixed fallback retail writes when a query fails. This record measures
all three in retail `jamp.exe`'s server path, serves them, and compares
fnTech3's dedicated server with retail's through one synthetic game module.
It also corrects that record: the 440-query transition measurement the client
game's blends rest on was itself taken through the server path. The
[JSON companion](2026-10-07-jka-server-pose-clock.json) holds the identities,
addresses, clock table, runs and comparison. No gate closes.

## Identities

| File | SHA-256 |
| --- | --- |
| `jamp.exe` 1.01 | `e8062d3948ff05917b48e47d8eec0ee43bcc6fb03924410712412404ddfee28b` |
| synthetic module | `ed6a095e48a787a84736b61843fd8c379079f867c7cc429cf31ec13b620bf483` |
| `fntech3.ded.exe`, HEAD `23ebc087` (before) | `fa8e07df5f731f199a41b56450c57aef8445633d5bd739679e83c832cefcb1b2` |
| `fntech3.ded.exe`, HEAD plus this slice (after) | `6b336850524ead04b513865e21442059dbe80d57996f8062d480ab9972b36120` |
| `fntech3.ded.exe`, HEAD `6c5c7ed9` plus this slice (confirmation) | `e1a5b55eb72273717b7f6b5419e5435880be0feb52a3b0a1c501b802f31a7b8d` |

Both servers were built from one clean export of HEAD `23ebc087` (MSVC 2022
x86, Meson `debugoptimized`, strict warnings, every development gate on,
dedicated server only, no warnings), the second after this slice was laid
over it, and run from `.tmp/runtime/jka-server-clock-before` and `-after`.
After the Jedi Outcast server slice (`6c5c7ed9`) changed the shared store, a
third, from a clean export of that HEAD with this slice, gave the after
server's 1,493 answers bit for bit (`.tmp/runtime/jka-server-clock-head`).
Retail's dedicated server here is `jamp.exe +set dedicated 1`, as in the
attachments record. Every run used a private desktop, an isolated home, no
input, `sv_fps 20` and `fixedtime 75`. The last cvar makes every server frame
add 75 ms of game time whatever the wall clock does, so the frames alternate
between one game frame and two in both engines and every run is
reproducible: retail's second run is bit for bit its first. Both
installations' inventories were unchanged after every run.

## Observed in retail: the clock

Read with Capstone; no retail code is incorporated.

- **Two clocks, one selected.** The engine keeps a server Ghoul2 clock at
  0x10bcb74 and a client one at 0x10bcb78. Its setter (0x4de0d0, index in
  `eax`, value in `ecx`) stores one and clears the client clock when it
  exceeds the server's plus 200 under signed comparison. Every reader takes
  the client clock when it is nonzero and the server's otherwise.
- **The bolt query reads the clock, never the time word.** The core the game
  imports 587 to 589 share (0x4dfa00) selects the clock at 0x4dfa84..0x4dfa94
  and passes it to its rebuild test (0x49a010) and skeleton build
  (0x49a050); the module's seventh word is never read. Import 597's wrapper
  (0x4df400) replaces its time word with the same clock, as the client
  game's 305 does. The clip setter (0x4df310, import 596 at 0x45cc63) and
  the angle setter (0x4df460, import 595 at 0x45cc1b) read no clock: a clip
  starts at the time its caller gives.
- **Where the server sets it.** Only the level's spawn and the server frame
  write the server clock on a dedicated server. The spawn (0x45e840) stores
  the level time before the game starts (0x45e9f3), before each of its three
  settling frames (0x45eb0e) and after them (0x45eb6a), and after the frame
  that follows the baseline (0x45ecf0). The server frame (0x460650) runs its
  game frames (0x460850..0x460879) and only then sets the clock to the level
  time (0x460883). `map_restart` (0x456396..0x4564cf) sets none. The client
  clock is written by the client frame (0x4148b0) and the client game's and
  UI's own imports, none of which run on a dedicated server.
- **So a game frame poses behind its own time.** Inside a server frame every
  game frame's queries pose at the level time the previous server frame
  ended on: one game frame behind, or two for the second of two.
- **Pose reuse at the same clock.** The rebuild test skips the skeleton
  build when the queried model was last built at the same clock and its bone
  cache is current. Both setters reset the stamp (+0x98) of the model they
  change, and only that model's.
- **The no-rotation selector.** Import 589 (0x45cb19) sets the selector
  0x10bcb6c, and the core clears it only when a query succeeds (0x4dfc07): a
  query that fails leaves the next successful one unrotated. Imports 588 and
  589 also set 0x10bcb70, which nothing in `jamp.exe` reads.
- **The fallback.** A query whose model or bolt index is outside the
  instance's writes the world placement built from the caller's angles and
  position times the fixed axes at 0x54e69c and answers 0 (0x4dfc1d), the
  failure output the client game already serves. The instance check
  (0x4e0c50) dereferences the instance, so a null or foreign one has no
  answer, and a null matrix would be written through.

## Retail's answers through the server path

A synthetic game module, independently written and carrying no game logic
(source `.tmp/jka-server-clock/server-clock-probe.cpp`, not committed), loads
eight Kyle bodies, each holding a blaster pistol on `*r_hand` and `saber_1`
on `*l_hand` as in the attachments record. It gives each body a clip at time
0 and then a transition: in a settling frame, where the clock equals the
caller's time, and in server frames where it lags it by 50 or 100 ms; one
interrupting an active transition, two in one frame, one with a fractional
seed, and one on `thoracic` beside one on `model_root`. It asks for the
bodies' hand tags and `rhand` bone and the pistols' and sabers' tags and
roots in every frame. In the hash-pinned retail engine only (its PE stamp,
size, checksum and base), it also reads both clocks read-only and asks import
597 for a clip playing one frame per millisecond. It then fails queries on
purpose, runs the selector sequence, bends a body between two weapon queries
at one clock, cleans up and asks for `map_restart`, after which one body is
asked about for ten more frames. The same module ran in all three servers:

`python .tmp/jka-server-clock/run-clock-probe.py --engine retail|fntech3 [--executable ...] --output ...`

and `compare-clock.py` compares the answers.

**The clock**, as retail's game frames saw it (level time, clock):

| Frames | Clock |
| --- | --- |
| `GAME_INIT` and the three settling frames: 0, 0, 100, 200 | the level time |
| The frame after the baseline: 300 | 300 |
| Server frames: 450, 500, 550, 600, 650, 700 ... 1500 | 400, 450, 450, 550, 600, 600 ... 1450 |
| `map_restart`'s four frames: 1500, 1600, 1700, 1800 | 1500 |
| The server frame after it: 1950, 2000 | 1500 |
| Then: 2050, 2100, 2150, 2200 | 2000, 2050, 2050, 2150 |

Import 597's frame equals the server clock in all 38 readouts and the client
clock stays 0. The same queries asked again in one frame with time words 0,
the level time plus 1234 and 999999 answer bit for bit as before, 48 times.

**Transitions.** 231 of the 1,468 answered queries fall inside an active
transition at retail's clock.

**Failures.** The 25 failed queries (model 9, bolt 99, bolt -1, a weapon's
bolt 5, through each of 587 to 589, unplaced and placed) answer 0. Unplaced
they write the fixed axes, columns (0, 1, 0), (-1, 0, 0), (0, 0, 1); placed
at angles (15, 25, 35) and position (7, -11, 13), those axes carried by that
placement. The query's scale is not applied.

**The selector.** After a failed 589, the next 587 answers bit for bit as a
successful 589 does, and the one after it rotated.

**Pose reuse.** With every model built at one clock, a bend of the body's
`thoracic` left the saber's and pistol's next answers bit for bit their
answers before the bend; the body's own answer moved, and after it so did
the weapons'.

## fnTech3 against retail

| Server | Answered | Within tolerance | Inside a transition | Failure output | Selector |
| --- | --- | --- | --- | --- | --- |
| Retail, second run | 1,468 of 1,468 | bit for bit | 231 bit for bit | 25 bit for bit | retail's |
| fnTech3, before | 615 | 375 | none answered: 231 refused | none written | not kept |
| fnTech3, after | 1,468 | 1,466 | 231 of 231 | 25, 12 bit for bit | retail's |

Tolerances are 0.00005 for rotation and 0.0002 units for translation, as in
the attachments record. After the change the 1,466 agree to within
0.0000134 in rotation and 0.000126 units, the largest on a pistol's tag
mid-transition; placed failure output parts from retail's x87 arithmetic
in the last bit or two (0.00000006). The two answers that differ are the
pose reuse: fnTech3 answers the bent body's weapons afresh, 5.0 and 11.4
units from retail's stale answers.

**Before this change** fnTech3's server refused every query during an active
transition, and the 853 refusals wrote nothing. It posed the rest at the
module's time word, a frame or more ahead of retail's clock, so 240 answers
on moving bodies differed by up to 1.9 in rotation and 43 units. Failed
queries wrote nothing, and its selector lasted one query.

## What this slice serves

- **Academy's server has its own pose clock**, set where retail's server sets
  it: before its game starts, after each settling frame, after the frame that
  follows the baseline and after each server frame's game frames
  (`sv_init.cpp`, `sv_main.cpp`), and nowhere in `map_restart`. Every
  setting of retail's spawn falls on the same level time as one of these.
  Bolt queries pose at it and ignore the time word. Every other profile's
  setting is a no-op.
- **Active forward transitions** are served on the server: its store now
  opts into the blends the client game serves, through the same setter and
  evaluator, so a saber or weapon in a hand answers mid-transition too. The
  clip setter takes the setter's bounds (0x4df326..0x4df36f) as the client
  game's does.
- **Retail's fallback** is written for retail's own failures: a model or bolt
  the instance lacks. This is reproduced because it is the data retail hands
  the module, and the retail game reads the matrix at all 52 of its query
  sites whatever the answer (attachments record); the client game already
  serves it. Every refusal that is this engine's own still writes nothing:
  a model retail's order never poses, a link to a missing bolt, a pose
  outside the served clips, and an instance this store never handed out,
  which retail would dereference.
- **The no-rotation selector** lasts until a query succeeds, as retail's
  does.

**Gameplay effect.** On fnTech3's Academy server a held saber's or weapon's
bolt now answers where retail's server puts it in every frame measured here,
mid-transition and one frame behind included. In the pinned OpenJK game code
the blade traces start from those answers every frame
(`WP_SaberPositionUpdate`, `codemp/game/w_saber.c` 8892), so saber hits are
the likely consumer; that is inference from the reference, not observation
of the retail game.

## Differences recorded, not reproduced

- **Pose reuse at one clock.** A model set playing or bent between two
  queries on another model of its instance, at one clock, leaves retail's
  second answer at the earlier pose; fnTech3 poses afresh. Reproducing it
  needs per-model build stamps over kept poses. The Jedi Outcast server
  slice adds kept builds to the store for its own engine, which judges by
  model 0, where Academy's judges by the queried model.
- **A listen server.** Retail poses its server's queries at the client clock
  whenever the client has set one within 200 ms ahead of the server's, and
  its client and server share one selector. fnTech3 keeps both per module;
  its Academy client game refuses bolt queries while a local server runs.
- **Malformed calls.** A null matrix, which retail writes through, is
  refused with nothing written.

## Correction to the server attachments record

That record said the server kept blends off because they "were measured
through the client game only". The October 3 measurement of 440 queries
(`.tmp/jka-compat/animation-blend-probe.cpp`, run `animation-blend-retail-02`)
ran its module as a game module in retail `jamp.exe +set dedicated 1`,
through imports 596 and 587, reading the server clock 0x10bcb74: the server
path. It set its transitions only in settling frames, where the clock equals
the caller's time, and asked in one long first server frame whose clock
stayed at 400 while the level ran from 450 to 750. It could not tell the
setter's caller time from the clock; this module sets transitions where the
two differ, and fnTech3's store, sampling the outgoing clip at the caller's
time and posing at the clock, matches all of them.

## Presentation (invariant 11)

None. A dedicated server draws nothing; this slice changes only the answers
a server's game module receives.

## Tests

- `tests/jka_server_attachment_tests.py` (Meson's
  `fntech3_jka_server_attachment`, now under `jka_test_timeout`) compiles,
  in ordinary and fast-math builds, the production clip setter, bolt-query
  cases, pose clock, selector and clock setter of `server_jka_game.hpp` with
  the production stores. Through the real imports, a transition is answered
  at the server's clock exactly as a store built like the client game's
  answers it: before it starts, while it blends (where it differs from the
  same clip change without one) and after it ends. That holds for the bone
  and a weapon on it, whatever the time word. Failed queries write retail's
  measured fallback, placed within 0.000001 and unplaced bit for bit, and
  only Academy sets the clock. A foreign instance gets nothing. The selector
  survives failures and clears on success, and import 596 bounds its clip. A
  second test compiles `SV_Frame`'s production run of game frames and its
  clock setting and replays retail's 32 measured (level time, clock) pairs
  under `fixedtime 75`, across a `map_restart` that sets no clock. A third
  checks the spawn's three settings and `map_restart`'s none against the
  source.
- `tests/jka_pose_tests.py`'s bolt-import fixture now expects the fallback
  and the selector a failed 589 leaves.
- `tests/wolfet_map_load_settle_tests.py`, `tests/stvef_sp_startup_tests.py` and
  `tests/stvef_sp_cadence_tests.py` stub the new clock setting in the blocks
  they compile.

Thirteen deliberate mutations fail them (`.tmp/jka-server-clock/mutate.py`):
the store without blends; the query at the time word; no fallback; a
fallback for a foreign instance; a selector lasting one query, or cleared by
a failure; the clock set for every profile; unbounded clip frames or seed;
the frame's clock set before its game frames, or before each; the spawn's
settling setting removed; and `map_restart` setting the clock.

On a clean export of HEAD `3f694633` with this slice, built with the client
and every development gate (1,499 targets, no warnings), Meson's 250 project
tests other than the pytest wrapper give 248 passes and 2 skips for this
machine, and the complete Python suite passes 2,571 tests with 129 skipped.
On a clean export of HEAD `6c5c7ed9` with this slice, Meson gives the same
248 and 2, and the 28 Python files for the Jedi games, the Raven stores, the
fixtures that slice the server's spawn and frame, and the documentation and
packaging checks pass 194 tests with 1 skip.

## What stays open

- Pose reuse at one clock on Academy's server.
- A listen server's shared client clock and selector.
- Reverse clips, angle blends and the other controls outside the served
  clips, which still refuse with nothing written.
- Live play with sabers on fnTech3's Academy server.
