# Jedi Academy server bone animation query, 2026-10-07

The retail 1.01 multiplayer game calls import 597 (`G2_GETBONEANIM`) from one
place, and fnTech3's server refused it by name. This record measures what
retail `jamp.exe`'s server answers and writes for it, serves it at the
server's own Ghoul2 clock from the
[pose clock record](2026-10-07-jka-server-pose-clock.md), and compares
fnTech3's dedicated server with retail's through one synthetic game module.
The [JSON companion](2026-10-07-jka-server-bone-anim.json) holds the
identities, addresses, runs, retail's answers and the comparison. No gate
closes.

## Identities

| File | SHA-256 |
| --- | --- |
| `jamp.exe` 1.01 | `e8062d3948ff05917b48e47d8eec0ee43bcc6fb03924410712412404ddfee28b` |
| `base/assets3.pk3` `jampgamex86.dll` | `b9e5ad492d35a3ec2fc7f6933df42ce9d3b05d3ec437e3bf12243a5d0d690850` |
| synthetic module | `2b20e083ab2b6c452f8eff92207623b2821f508bcb9a8b8346973b9eef603a69` |
| `fntech3.ded.exe`, HEAD `6c5c7ed9` plus the pose clock slice (before) | `e1a5b55eb72273717b7f6b5419e5435880be0feb52a3b0a1c501b802f31a7b8d` |
| `fntech3.ded.exe`, the same plus this slice (after) | `377a263e0426a24a7716f33a8058d5e32926621cec4bb423c4d96a87dd0b3369` |

The before server is the pose clock record's confirmation build
(`.tmp/runtime/jka-server-clock-head`). The after server was built from a
clean export of HEAD `6c5c7ed9` with the pose clock slice and then this one
laid over it (MSVC 2022 x86, Meson `debugoptimized`, strict warnings, every
development gate on, no warnings) and run from
`.tmp/runtime/jka-boneanim-after`. Retail's dedicated server is
`jamp.exe +set dedicated 1`. Every run used a private desktop, an isolated
home, no input, `mp/ffa1`, `sv_fps 20` and `fixedtime 75`, so every run
repeats: retail's second run is bit for bit its first, and so is fnTech3's.
The installation's inventory was unchanged after every run.

## Observed in retail

Read with Capstone; no retail code is incorporated.

- **The handler** (0x45cc9c, jump table 0x45d34c) reads ten arguments and
  converts the second and the fourth to ninth from module addresses:

      ghoul2, boneName, time, currentFrame, startFrame, endFrame,
      flags, animSpeed, modelList, modelIndex

  It resolves the model as the instance's vector start plus the index times
  0xc8 (0x4116e0), with no bounds check.
- **The wrapper** (0x4df400) answers 0 and writes nothing when the model's
  setup fails (0x4e0b10), and otherwise replaces the time word with the
  Ghoul2 clock -- the client's (0x10bcb78) when it is set, the server's
  (0x10bcb74) otherwise -- before the core. The model list is passed and
  never read.
- **The core** (0x4e3750) finds the bone among the model's bone records
  (0x4e2be0) or adds a record for it (0x4e2c70); both compare names with
  the C runtime's `_stricmp` (0x539336), so a name matches without regard
  to case. A name the skeleton lacks answers 0 and writes nothing.
- **The answer** (0x4e3640). A record whose flags word (+0x34) has neither
  0x08 nor 0x10 runs no clip: it writes start 0, end 1, frame 0, flags 0 and
  speed 0, in that order, and answers 0 (0x4e3709). Otherwise the timing
  routine (0x497290) gives the frame at the clock, and it writes the whole
  frame plus its fraction, then the record's start, end, flags word and
  speed (+0x38, +0x3c, +0x34, +0x48), and answers 1.
- **The flags word is the record's, not the caller's.** The clip setter
  (0x4df310 into 0x4e32f0) first ORs 0x8000 into the word (0x4e334f), then
  replaces only the animation bits 0xd8 with the flags it was given
  (0x4e355d) -- less the blend bit 0x80 when the record ran no clip
  (0x4e34c7) -- so every other bit it is given stays. A word that has gone
  negative is cleared first (0x4e3566). The angle setter (0x4df460 into
  0x4e3240) replaces only the low three bits with its flags, whole
  (0x4e32a4). Both leave a record carrying 0x2000 untouched (0x4e35de,
  0x4e3346, 0x4e326e). A blend's 0x80 stays until the next clip. The mark
  is cleared only by the collision cache's retransform test (0x4dfd30, at
  0x4dfdd3), for every record whose clip has not passed its end, loops, or
  carries the mark; that test is reached from import 607 (0x45ce4d into
  0x4dfe00) and from the server's own trace code (0x462490, called from
  0x462920), and from nothing this slice serves.

## How the retail game uses it

`scripts/scan_game_import_calls.py --import G_G2_GETBONEANIM` finds one push
of 597 in each retail game module. In `assets3.pk3`'s it is the wrapper at
0x200a6eb0, reached by a jump at 0x200a5580 from one call, at 0x2005904b.
That code resets the angles of `lhumerus` and `lradius` through import 595,
asks 597 for `pelvis` of model 0 at the module's time, then hands the
answer's start, end, flags and speed straight to import 596 for both bones,
with the start frame as the seed and a 300 ms blend. It never reads the
answer. So the flags word, 0x8000 included, comes back to the clip setter.

## Retail's answers

The synthetic module plays clips on eight Kyle bodies, a pistol on one, and
asks 597 of them every game frame, before and after `map_restart`. In
retail's runs:

| Case | Answer |
| --- | --- |
| A name the skeleton lacks, or an empty one | 0, nothing written |
| A bone the skeleton has, no clip, or a bend alone | 0; frame 0, 0..1, flags 0, speed 0 |
| `MODEL_ROOT`, `Model_Root` | `model_root`'s answer |
| A loop, a freeze past its end, a plain clip mid-way | 1; the frame at the clock; flags `0x8058`, `0x8048`, `0x8008` |
| A blend from a running clip | flags `0x80c8`, kept long after the blend ends |
| A blend asked of a bone with no clip | flags `0x8048` |
| A bend, then a clip; bent again with REPLACE | flags `0x805a`, then `0x805c` |
| A clip given `0x8058` | flags `0x8058` |
| Seeds | the truncated start clock: seed 512.25 at speed 7.77 answers 512.121216 |
| Time words 0, 999999, -5 and ahead | the same answer, at the clock |
| The pistol's engine skeleton | 0 and the defaults; after a clip, 1 with `0x8048` |
| A bone that model lacks | 0, nothing written |
| After `map_restart` | the clock, unchanged by the restart |
| An answer at `0x80c8` given to `lhumerus` as the IK release gives it | `0x8048` from its start frame |

The model-list canary was never touched. On one more instance, inside retail
only, the clips fnTech3 does not time:

| Case | Answer |
| --- | --- |
| A plain clip past its end | 1 once, with the frame left as stack contents (2.2e-29) and flags `0x8000`: the timing cleared the animation bits; then 0 and the defaults |
| A reverse clip, 20 down to 9 at speed -1 | 1; 20, 19, 17, 16 ... |
| A clip given `0x1048` | 1; flags `0x9048` |
| `0x2000` through the angle setter, then a clip | 0 and the defaults |
| `0x48` through the angle setter, no clip | 1; frame 0, 0..0, flags `0x48`, speed 0 |

## fnTech3 against retail

| Server | Answers | Bit for bit | Fractional frames | Setter results | Bolts |
| --- | --- | --- | --- | --- | --- |
| Retail, second run | 723 | its first run's | -- | -- | -- |
| fnTech3, before | none: the first 597 call stops the server by name | -- | -- | -- | -- |
| fnTech3, after | 678 of retail's 678 outside the retail-only instance | 678 | 124 of 124 | 22 of 22 | 50, within 0.0000063 and 0.0000115 |

The 678 are 369 running clips, 306 defaults and 3 answers that write
nothing. The bolts include those posed through the IK release's two bones.
fnTech3's second run repeats its first, and matches retail's second run the
same way.

## What this slice serves

- **Import 597** on Academy's server (`server_jka_game.hpp`), at the server's
  pose clock, ignoring the time word and the model list, writing in retail's
  order, answering 0 or 1 as retail does.
- **Retail's flags word** (`ghoul2_server.hpp`, `BoneFlags`): the store
  keeps it for every bone either setter reaches, by both setters' rules,
  lock and negative word included. Only Academy's server reads it.
- **Names without case** for this import, as retail's `_stricmp`.
- **The mark given back.** Import 596 drops 0x8000 from the flags before the
  store's clip, since retail's setter marks the word with it anyway; the
  retail game's IK release would otherwise leave both arm bones outside the
  served clip flags, and every bolt of that body refused.

Refused by name, with nothing written: a running clip the store does not
time (a plain clip past its end, whose retail frame is stack contents; a
reverse clip; flag bits outside 0xd8), a word whose animation bits came
through the angle setter or that 0x2000 has locked, an instance this engine
never handed out, and an output retail would write through null. A model
index past the instance answers 0 and writes nothing, where retail reads
past its vector.

## Differences recorded, not reproduced

- **Names in the setters.** Retail's setters match bone names without case
  too; the store's match them exactly, so a clip set as `PELVIS` is refused
  where retail sets it. Only this import folds case in this slice. The
  [subsequent bone names slice](2026-10-07-jka-bone-names.md) measures and
  corrects Academy's setters and AddBolt, leaving Outcast unchanged.
- **Clips the store's setter refuses**, such as a non-finite speed or a seed
  outside the clip, are kept by retail; the store keeps the previous clip and
  word, and answers for those.
- **The added record.** Retail's query adds an empty record for a bone no
  setter reached; no served import tells it from none.
- **The mark after a collision cache.** The retail 1.01 game calls import
  607 once, and retail's server trace reaches the same test; either clears
  0x8000 from the words it passes. fnTech3 refuses 607 and does not follow
  that trace path, so after one, its words keep the mark where retail's lose
  it. The game's only 597 call hands the word back to the clip setter, where
  the mark changes nothing; serving 607 must clear the store's words the
  same way.

## Presentation (invariant 11)

None. A dedicated server draws nothing; this slice changes only the answers
a server's game module receives.

## Tests

- `tests/jka_server_attachment_tests.py` compiles, in ordinary and
  fast-math builds, the production clip and angle setters and the import-597
  case over the production store, and checks every retail case above, the
  IK release, the refusals and the store's word rules
  (`CheckAcademyBoneAnim`). Thirteen faults laid into the production code one
  at a time are each caught.
- `tests/jka_module_refusal_tests.py` counts 157 served imports.

## Open

The listen server's shared clock and pose reuse at one clock stay as the
pose clock record left them. Live interoperability, demos and the rest of
Academy's gates stay open.
