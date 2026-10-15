# Jedi Academy effects played on an entity, 2026-10-15

The [camera-shake slice](2026-10-15-jka-camera-shake.md) left a
first-person Bryar pistol shot stopping on client-game import 268,
`CG_FX_PLAY_ENTITY_EFFECT_ID`: the view weapon's muzzle flash is played
through it rather than through import 266. This slice serves that import.
Retail hands it the caller's own axis instead of a direction, and every
retail call site passes it the unbolted word; Fn plays the effect through
the same owned path as import 266, at that axis, and refuses a bolted word
by name. The [JSON companion](2026-10-15-jka-entity-effect.json) records
the binaries, retail identities, commands and outcomes. No compatibility
gate closes.

Implementation is independently authored. Retail `jamp.exe` 1.01 and the
1.01 `cgamex86.dll` from `assets3.pk3` were read with capstone. Pinned
OpenJK was used only to name routines (reference observation; no text
incorporated).

## Observed, retail `jamp.exe` 1.01

### The dispatch

The client game's trap dispatcher (`0x412160`) bounds the import number
at `0x15e` and indexes a byte table (`0x414638`) into a jump table
(`0x4142bc`). Import 266 lands at `0x413431` and import 268 at
`0x41346b`.

- Import 266 pushes a false portal flag, args 5 and 4, and the translated
  forward (arg 3) and origin (arg 2) pointers, and calls `0x406d70`. That
  builds an axis from the forward direction (`0x443840`) and calls
  PlayEffect (`0x407110`) with the bolt word -1, no Ghoul2 instance, no
  fxParm (-1) and the two words as the volume and radius.
- Import 268 translates the axis (arg 3) and origin (arg 2) pointers and
  calls `0x401050` with the bolt word (arg 4) in `edx`, arg 6 in `ecx`
  and arg 7 in `eax`. That calls PlayEffect with the effect id, the
  origin, the caller's axis, the bolt word, no Ghoul2 instance, fxParm
  -1, args 6 and 7 as the volume and radius, and no portal, loop or
  relative flag. Arg 5, the entity number, is never read.

### PlayEffect with the caller's axis

- PlayEffect returns at once for an id outside 1..255 or an empty
  template slot (`0x40712c..0x407154`), before it reads the bolt word.
- It stores the volume and radius in each primitive's template
  (`+0x23c`, `+0x240`). No served primitive family reads them (the
  [effect-sound slice](2026-10-15-jka-effect-sound.md) measured the Sound
  case), so the -1 the retail call sites pass has no effect.
- The axis is never normalized or rebuilt. A spawn created at once gets
  the pointer as CreateEffect's axis (`0x407551`); a delayed one copies the
  nine words into its scheduled record (`0x4075d8..0x40760a`).

### The bolt word

- A bolt word above zero is split into a model (bits 10..19), a bolt
  (bits 0..9) and an entity (bits 20..31) (`0x407175..0x4071a5`), and sets
  a flag that schedules every spawn, even an undelayed one, one
  millisecond late (`0x4074cb`, `0x40766f`). The scheduled record keeps
  the model, bolt, entity and the (null) Ghoul2 instance instead of the
  origin and axis (`0x40764e..0x407672`).
- When such a record falls due, AddScheduledEffects (`0x407810`) asks the
  client game for the entity (export 11) and Ghoul2 for the bolt's matrix
  (`0x40c5a0`, `0x4dfa00`); only a bolt that exists creates the spawn
  (`0x4079a9`). With import 268's null Ghoul2 instance the lookup is
  expected to fail and create nothing (inference: the Ghoul2 check at
  `0x4e0c50` was read, not run).
- A bolt word of zero, or below -1, is not decoded. An undelayed spawn is
  created at the caller's origin and axis (`0x40753a`), but a delayed
  one's record is written as bolted, without the origin or axis
  (`0x40764e`), and when it falls due retail asks the client game for
  entity -1's origin (export 10) and passes the record's unwritten axis
  words (`0x4078ab..0x4078fa`).

### The retail client game's call sites

`cgamex86.dll` 1.01 reaches import 268 through one wrapper
(`0x3005efa0`, `push 0x10c`) from four call sites. All four pass -1 as the
bolt word, entity, volume and radius:

- `0x30065a9e` and `0x30065ade`: a weapon's muzzle flash, chosen by the
  alternate-fire flag (`eFlags & 0x400`) between the weapon's two muzzle
  effects (`+0x9c`, `+0x5c`) while the flash is under 30 ms old. The
  same branch calls import 266 instead (`0x3005ef50`) when its view flag
  is set: the third-person path the
  [weapon-fire slice](2026-10-15-jka-weapon-fire.md) played.
- `0x300531a4`: `force/lightning` or, above level 2,
  `force/lightningwide`.
- `0x30053586`: `force/confusion_old`.

## Implemented

- **Kernel** (`code/compat/jka_effect_play.hpp`). `EntityAxis` takes the
  caller's nine words as given, row by row, unnormalized, and refuses a
  word that is not finite or whose size exceeds 1024 (an Fn bound; a
  model scale keeps a bolt matrix far within it). `EntityUnbolted` is the
  -1 word.
- **Host** (`code/client/client_jka_cgame_effects.hpp`). Import 268 enters
  the import 266 path with the caller's axis instead of an axis built
  from a direction. An id without an owned declaration logs and returns
  before the bolt word is read, as retail does. The entity number, volume
  and radius are not read. Every later step is shared with import 266:
  the frame clock and paused-frame checks, compilation of the effect's
  graph, counts, delays and the schedule (which keeps the caller's axis),
  and the owned primitives with their Fn presentation.
- **Refusal.** A bolt word other than -1 stops with
  `Academy entity effect bolt word N is not served yet`. No retail call
  site passes one; serving it needs Ghoul2 bolt matrices for the effects
  schedule.
- The reentry guard of the effects trace and camera shake covers import
  268 too. `fx_debug 2` names each play as `Academy FX entity play`.

### Fn presentation (invariant 11)

The import only places an effect; what it plays is the effects path the
earlier slices adapted: Fn sprites, polygons and dynamic lights with Fn's
shaders, fog and per-pixel lighting, Fn's spatial audio for Sound
primitives, and the client game's own view shake. This slice adds no
presentation of its own and keeps no retail-exact option.

## Not served, recorded

- Bolted entity effects (a bolt word other than -1), named above.
- The Decal primitive of the bolt's wall impact (`bryar/wall_impact`),
  which stops the live shots; the per-file sweep records it.
- The portal pass, as for import 266.

## Live

Two runtimes were built from `git archive`s with only this slice's hunks,
using win32-dev's options and strict warnings:
`.tmp/runtime/jka-entity-effect` on HEAD `8a05e028`, and, after the
[Ghoul2 trace slice](2026-10-15-jka-g2-trace.md) landed,
`.tmp/runtime/jka-entity-effect-g2` on HEAD `15b0766f`. Each run hosts
`mp/ffa2` with the retail modules from the paks, windowed on an unswitched
private desktop, with `net_enabled 0`, an isolated home and no input. The
harness is a copy of the camera-shake slice's `listen.py`, and the steps are
its first-person fire steps (`team free`, `togglemenu`, `give all`,
`+attack`, `viewpos` on every frame); the slow runs add `timescale 0.1` from
the frame before the trigger and engine screenshots after it. The player
spawns at a different point on each run.

| Run | Runtime | Renderer | Entity plays (import 268) | Sounds | Shake records | View samples off the eye after the shot | Stop |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `glx-fire-1` | `8a05e028` | GLx, normal speed | 2 | 2 | 2 | 0 of 3 | game import 28 |
| `glx-slow-1` | `8a05e028` | GLx, timescale 0.1 | 11 | 11 | 11 | 22 of 28 | game import 28 |
| `vk-slow-1` | `8a05e028` | Vulkan, timescale 0.1 | 6 | 6 | 6 | 5 of 7 | game import 28 |
| `glx-g2-1` | `15b0766f` | GLx, timescale 0.1 | 11 | 11 | 11 | 24 of 30 | Decal |
| `vk-g2-1` | `15b0766f` | Vulkan, timescale 0.1 | 9 | 9 | 9 | 24 of 30 | Decal |

- Every first-person shot now plays `effects/bryar/muzzle_flash` through
  import 268, where `glx-fire-1` of the camera-shake slice stopped. The
  client game plays the flash on each frame while it is under 30 ms old,
  so one shot plays it on two frames at normal speed and on more at
  timescale 0.1. Each play starts the flash's sound through Fn's spatial
  audio and hands its shake record (intensity 0.20 to 0.30, radius 60,
  50 ms) to export 31.
- The engine screenshots show the flash's particle and line at the view
  weapon's muzzle, lit by its dynamic light, from the first frame after
  the trigger in GLx and Vulkan, and no flash on the frame before.
- With the view at the muzzle, inside the shake's 60-unit radius, the
  client game's own view now jitters by a unit and a degree after the
  shot in every slow run, and never before it. At normal speed the three
  samples between the shot and the stop stayed on the eye: the shake is
  small (about a quarter of a unit) and `viewpos` truncates.
- On the `8a05e028` runtime the listen server then stops on the game's
  Ghoul2 trace (game import 28, `G_G2TRACE`) when the bolt is first
  traced, so the bolt's own effect is not reached.
- On the `15b0766f` runtime, with that trace served, the bolt flies:
  `bryar/shot` plays on every frame of its flight (100 and 498 plays), and
  `vk-g2-1`'s last capture shows it beyond the muzzle. The client then
  stops on the bolt's wall impact, whose `bryar/wall_impact` holds a Decal
  primitive (`effects primitive family is not served yet (Decal)`), the
  sweep's known refusal.
- The harness ends each run after the refusal; the installation and the
  runtime binaries are unchanged.

## Tests

- `tests/jka_entity_effect_tests.py` (new, registered as
  `fntech3_jka_entity_effect`). The kernel takes a scaled and a skewed
  axis as given and refuses infinities, NaN and words beyond the bound at
  every position, leaving its output unchanged. The host plays through
  the actual import 268 path: a particle, sound and shake offset along
  all three of a turned axis land where that axis puts them, a scaled
  axis scales the offsets where import 266 normalizes its direction, the
  entity number, volume and radius change nothing (same spawns, same
  draws), a delayed spawn keeps the caller's axis in the schedule, a
  paused frame and a missing id create nothing, five bolt words refuse
  by name, and bad axes, origins, pointers and modules refuse, as does
  reentry from the shake export.
- On the export the first runtime was built from, the new test and the
  effects, weapon-fire, module-refusal and generated-docs tests pass (35
  tests), as does `fntech3_jka_client_tests`.
- The full pytest suite on HEAD `33c75dfd` plus this slice: 2,762 passed
  and 131 skipped (retail inputs, runtime probes or symlinks not
  available), none failed. The commit tree on the HEAD it landed on ran the
  new test, the effects tests and the documentation and packaging tests.
