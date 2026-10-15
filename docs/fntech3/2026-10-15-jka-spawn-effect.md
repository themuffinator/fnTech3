# Jedi Academy spawn effect: Emitter, Light, FxRunner and Electricity, 2026-10-15

Joining a local `mp/ffa1` game stopped as the player spawned, on the
Emitter primitive of `effects/mp/spawn.efx`
([second Jedi round](2026-10-07-jedi-compatibility-round2.md),
[October 14 round](2026-10-14-compatibility-round.md#jedi-academy-saber-glow-and-lines)).
This slice serves the Emitter and everything the effect plays through it, so
the join now reaches play in GLx, Vulkan and RTX. The
[JSON companion](2026-10-15-jka-spawn-effect.json) records the binaries,
retail identities, commands and outcomes. No compatibility gate closes.

Implementation is independently authored. Retail `jamp.exe` 1.01 was read
with capstone. Pinned OpenJK was used only to locate routines and name
fields (reference observation; no text incorporated). Retail behaviour wins
where they differ: for example, retail draws the emitter's endpoint offsets
once per component, and its `intensity` key also turns on forked bolts.

## The effect

The installed `spawn.efx` has two traced Lines (already served) and an
Emitter. The Emitter climbs at 3,000 units a second for up to 8 s with
physics that kill it on impact. Every 100 units it plays
`env/beam_lights`, which holds a 40 ms teal Light and an FxRunner. With a
one-in-six chance, the FxRunner plays `env/beam_bolts`: two Electricity
bolts, each spawned one time in six, traced from the emission point
sideways and upward. `mp/jedispawn.efx` has the same structure.

## Observed, retail `jamp.exe` 1.01

- **Primitive types** (`0x406a53..0x406ba6`): Particle 1, Line 2, Tail 3,
  Cylinder 4, Emitter 5, Sound 6, Decal 7, OrientedParticle 8, Electricity
  9, FxRunner 10, Light 11, CameraShake 12, Flash 13. Template fields:
  count `+0x34`, life `+0x3c`, emitFx list `+0x78`, playFx list `+0x88`,
  origin2 `+0xd4`, velocity `+0x144`, density `+0x17c`, variance `+0x184`,
  intensity `+0x234`.
- **Flag words**, matched without case. `flags` (`0x40d2ad..0x40d4b7`):
  useModel `0x1000000`, useBBox `0x4000000`, usePhysics `0x2000000`,
  expensivePhysics `0x800000`, impactKills `0x40000000`, impactFx
  `0x80000000`, deathFx `0x20000000`, emitFx `0x10000000`, depthHack
  `0x100000`, relative `0x200000`, and others. `spawnflags`
  (`0x40d5ed..0x40d78e`): org2fromTrace `0x10`, traceImpactFx `0x20`,
  org2isOffset `0x40`, cheapOrg2Calc `0x200`, absoluteVel `0x400`, and
  others. Curve flags (`0x40cb10`): linear 1, random 2, nonlinear 4,
  wave 8, clamp 12. The `intensity` parser (`0x40cd90`) also ORs usePhysics
  into the flags.
- **Count** (`0x4072b1..0x4073ba`): a range is drawn only when its ends
  differ. A spread wider than one is multiplied by `fx_countScale`, clamped
  to at most 1. The value then rounds as `floor(v + 0.5)`, and a minimum of
  at least one always spawns one. `count 0 0.6` therefore spawns one time in
  six. Life is drawn the same way and truncated (`_ftol`).
- **CreateEffect** (`0x407c40`, type switch `0x409887`). Velocity
  (`0x408739..0x4089f0`) draws each range once per axis component. Traced
  endpoints (`0x408f39..0x4093e2`) start at `origin + 16384 * axis[0]`; with
  org2isOffset, each offset range is again drawn once per component. The
  trace is the 104-byte cgame export 15 already served for Lines.
  - **FxRunner** (`0x40b11e`) plays one effect picked from its list
    immediately, at its origin and axis.
  - **Light** (`0x40b155`) creates a `CLight` (`0x410960`).
  - **Electricity** (`0x40a16e`) creates a `CElectricity` (`0x40fae0`).
  - **Emitter** (`0x40a7bc`) creates a `CEmitter` (`0x410550`). Its angles
    are `vectoangles(axis[0])` plus its template angles (`0x4435e0`), and it
    picks one emitFx effect.
- **Primitive pass** (`0x40f320`, pool of 1,800 at `0x572608`): an entry
  past its end time (`now > start + life`) dies; otherwise `Update` runs. A
  full pool frees its first slot (`0x40f2c0`). Nothing is created on a
  paused frame (`0x410960`, `0x410550`).
- **CEmitter::Update** (`0x403c30`). It records this frame's start
  position and velocity. If it started before this frame, `UpdateOrigin`
  (`0x401a50`) moves it by `realTime * velocity`. With usePhysics and
  expensivePhysics, it traces that step (`0x401130`: point trace, mask
  `0x1001`, skip `-1`, export 15). A solid start stops it and clears
  usePhysics. A hit with impactKills kills it before it emits.
- **Emission walk** (`0x4039a4..0x403c0c`). From the last emission time it
  steps in 12 ms ticks, predicting `start + t*v + t²/2*v` (retail uses the
  velocity in the half-square term). It emits whenever the squared distance
  from the start reaches `(density + rand(-variance, variance))²`. The start
  resets to the actual position each frame, so emissions run ahead of the
  emitter. `AnglesToAxis` (`0x403e90`) rebuilds its axis each frame from its
  angles, with roll zero.
- **CLight** (`0x403f90`, draw `0x403f50`) calls `re.AddLightToScene(origin,
  radius, r, g, b)`. The size curve gives the radius, the RGB curve the
  colour. It has no alpha and no cull. The linear size law (`0x4040a0`,
  `0x401e50`) is `start*p + end*(1-p)` with `p = 1 - age/life` in binary32.
- **CElectricity** (`0x402b60`) maps usePhysics to forks (`0x4000`),
  useModel to taper (`0x8000`) and useBBox to grow (`0x10000`). It draws as
  the renderer's electricity entity (`0x402c10`), with the intensity as its
  jaggedness.

## Change

`code/compat/jka_effect_play.hpp` compiles and instantiates the four
families with these rules: flag words by name, count and life ranges,
per-component velocity and endpoint offsets, the emitter's angle-built axis,
the linear size curve, and the emission walk. `code/client/client_jka_cgame_effects.hpp`:

- compiles an effect's whole graph (every effect it can emit or play)
  before any callback, so an unserved descendant refuses the play;
- runs FxRunners' nested plays at once, at most eight deep;
- advances live emitters in the normal effects pass: motion, the physics
  trace, death on impact, and emissions;
- publishes the children in the same frame.

Unserved controls still refuse by name: cheap physics, bounce, models on
emitters, acceleration, angles, delays and the other flag words.

**Fn presentation (invariant 11):**

- A Light enters Fn's dynamic lights (`RE_AddLightToScene`, per-pixel
  lighting, shadow-eligible), in GLx, Vulkan and RTX.
- Electricity is a Fn-authored strip in the polygon path. It is a seeded
  jagged bolt of 16 segments whose jag scales with intensity. A grown bolt
  reaches its end at half life, a tapered bolt narrows to nothing, and a
  forked bolt adds two branches at segments 5 and 10. Fn's shaders, fog,
  sorting and post-processing apply.
- An emitter without a model draws nothing.

Fn adds no retail electricity renderer and no retail-exact option.

**Engine choices that differ from retail:**

- A full pool evicts the instances nearest their end, not the first slot.
  Retail never refuses, and Fn now never refuses either; the old
  "owned lifetime pool is full" error is gone. The change was needed: the
  client game replays `effects/mp/itemcone.efx` every frame for each item,
  and at short frame times that filled the pool.
- Children are drawn in the frame they are created. Retail's slot order
  sometimes delays them a frame.
- A paused frame advances no emitter.
- `fx_debug 1` prints retail's active/drawn/scheduled counts each pass.
  `fx_debug 2` also names each played and nested effect, a Fn diagnostic.

## Unit (Source)

`tests/jka_spawn_effect_tests.py` covers:

- the four families' declarations and flag words;
- named refusals;
- the count rule, including the one-in-six case;
- the emitter's axis;
- the emission walk, matched against a restatement of `0x4038f0`;
- the linear size law;
- traced offset endpoints;
- the bolt, branch, grow and taper geometry;
- the actual imports: a climbing emitter shedding lights and traced bolts
  at the reference positions, its death on impact with no emission that
  frame, a solid start, the nesting bound, a whole-graph refusal before any
  trace, and a missing light service.

Each runs plain and with `-ffast-math`. The existing effect tests were
updated where the subset grew:

- count, traced Line offsets, and a pool that now evicts;
- the registration fixture's renderer gains `AddLightToScene`.

## Live (subset)

Runtime `.tmp/runtime/jka-spawn-effect`. It was built from the shared
working tree, which also held other sessions' uncommitted Alice renderer
work. Each run is a windowed jka-mp client hosting `mp/ffa1` with
`+devmap`, on a private desktop, on loopback only, with no input. The
harness is `.tmp/jka-emitter/live/listen.py`, a copy of the October 14
harness with configurable capture spacing.

- **Before the change**, the join stopped at the Emitter. Fixing it
  exposed a model entity with Raven renderfx `0x2400`: distortion plus
  forced alpha. That is the invulnerability sphere's refraction copy, which
  the retail client game adds only while `cg_renderToTextureFX` is on
  (`cgamex86.dll` 1.01 at `0x3004a719`/`0x3004daf5`, default 1, sites
  `0x3004a98a`/`0x3004db31`). The probes set that player option to 0. The
  distortion flag stays refused.
- **At normal speed** (GLx), the player joins the battle and ten engine
  captures are written in play. About 12.5 s in, play stops on the known
  Kyle Ghoul2 bolt-pose refusal (bone 52, an unknown clip), unrelated to
  effects.
- **With `timescale 0.1`** (cheats on under devmap), captures land about
  22 ms of game time apart. GLx, Vulkan and RTX each ran to their scripted
  end with no refusal: 16 captures each, third person, HUD hidden,
  `fx_debug 2` to the log only. Each played `spawn.efx` once. Its emitter
  shed `env/beam_lights` 9, 6 and 4 times before it hit a ceiling, and the
  FxRunners played `env/beam_bolts` 1, 2 and 0 times.
- **A traced GLx diagnostic** confirmed the measured timings:
  - the Lines run vertically from the floor to the ceiling and retire at
    199 ms;
  - the emitter climbs 3 units per 1 ms frame and dies at the ceiling;
  - Lights come every 24 ms of game time at these frame lengths;
  - the bolts' endpoints are traced and offset as measured.
- **The captures show the Light.** The 40 ms teal Light visibly washes the
  wall above the player in Vulkan and RTX during the climb and is gone
  afterwards; a GLx capture diff shows the same tint.
- **`r_showtris`** shows the Lines' quads drawn through the player. With
  the third-person camera within tens of units of the beam, and the client
  game's green invulnerability shell around the player
  (`g_spawnInvulnerability` set at run time did not apply), no capture
  isolates the beam or a bolt's appearance.

Retail installations were unchanged and the binaries kept their hashes in
every run.

## Validation

The commit's tree alone (HEAD `9a15b30e` plus this slice's files, with no
other session's work) was exported and built: 1,541 Windows x86 targets
with strict warnings at four jobs. Its Meson suite passed 265 tests, with 2
skipped and none failed, including the wrapper that runs the Python suite.
Its release-packaging tests passed 48, with one symlink case skipped for
lack of privilege. The 22 Python test files around the Academy client host,
renderer adapters and generated docs passed 75 tests and 38 subtests in
both the export and the shared tree.

## Remaining and unrelated findings

- Raven renderfx `0x2000` (distortion) refuses the invulnerability sphere
  whenever `cg_renderToTextureFX` is on, its retail default.
- Kyle's Ghoul2 bolt pose refuses about 12 s into play (bone 52 with an
  unknown clip); full play is not ready.
- No capture yet isolates a Line's or bolt's appearance. A far camera
  without the shell is needed.
- The flag words not served (impactFx, deathFx, cheap physics, bounce,
  relative, model emitters, delays) refuse by name if another effect uses
  them.
- Academy's `glow` stage keyword is still an unknown shader parameter.
