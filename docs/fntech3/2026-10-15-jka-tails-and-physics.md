# Jedi Academy tails, particle physics and spawn shapes, 2026-10-15

The [particle-dynamics slice](2026-10-15-jka-particle-dynamics.md) left the
per-file effect sweep with these first refusals at the top: the Tail family
(30 files), Particle and Electricity `bounce` (25 and 12), the particle
flags (14), Particle `radius` and `height` with the sphere and cylinder
spawn flags (23), and curve words on Line and OrientedParticle (28 and 8).
This slice serves all of them. Of the 376 installed effect files, 190 now
compile completely, up from 68, and none that compiled before refuses. The
[JSON companion](2026-10-15-jka-tails-and-physics.json) records the
binaries, retail identities, commands and outcomes. No compatibility gate
closes.

Implementation is independently authored. Retail `jamp.exe` 1.01 was read
with capstone. Pinned OpenJK was used only to locate routines and name
fields (reference observation; no text incorporated). Retail behaviour wins
where they differ, as noted below.

## Observed, retail `jamp.exe` 1.01

### The primitive families

CreateEffect's switch (`0x40987a`, table `0x40b5e8`) numbers the families
Particle 1, Line 2, Tail 3, Cylinder 4, Emitter 5, Sound 6, Decal 7,
OrientedParticle 8, Electricity 9, FxRunner 10, Light 11, CameraShake 12 and
Flash 13. Each case calls its own creation routine: FX_AddTail is
`0x40fdc0`, FX_AddLine `0x40f810`, FX_AddElectricity `0x40fae0` and
FX_AddOrientedParticle `0x410b70`.

Their update routines (from the classes' tables at `0x565ea8..0x565f5c`):

- **Particle** (`0x401730`), **OrientedParticle** (`0x402770`) and **Tail**
  (`0x402e30`) move through UpdateOrigin (`0x401a50`), then run the size,
  RGB and alpha curves (`0x401e50`, `0x402040`, `0x402390`). Particles and
  oriented particles also turn (`0x402640`).
- **Line** and **Electricity** share one update (`0x402c80`). It never
  moves them, but runs the same three curves. Their death routine is an
  empty `ret` (`0x5271f0`).
- Every creation routine converts its curve parms the same way: a wave's
  by `0.0031415901`, any other's to an absolute time.

### Tails

FX_AddTail builds a 0x140-byte primitive whose render entity has type 8
(Academy's `RT_LINE`). Each update:

1. keeps the origin it starts the frame with (`+0x124`);
2. moves, if it was created before this frame (`0x403170`);
3. runs size, then **length** (`0x4031c0`), RGB and alpha;
4. sets its far end (`0x4033b0`): from the origin, back towards the kept
   origin, by this frame's length;
5. draws, with `depthHack` setting render flag 8 (`0x402df0`).

The length curve is the same law over bits 12..15 of the flags: linear
`0x1000`, nonlinear `0x4000`, wave `0x8000`, clamp `0xc000` and random
`0x2000`. Its ends and parm sit at template `+0x20c`, `+0x214` and
`+0x21c`. Retail's effects use negative lengths, which point a tail along
its motion. A tail that has not moved has no direction and draws as a
point.

### Template and parser

- `bounce` and `intensity` share one parser (`0x40cd90`). It stores the
  elasticity (`+0x234`) and ORs `usePhysics` (`0x2000000`) into the flags.
  For Electricity the same value is its bolt's chaos.
- `radius` (`+0xec`), `height` (`+0xf4`) and `wind` (`+0xfc`) are scalars.
- The group parser reads `width` as `size`, `width2` as `size2` and a
  `height` group as `length` (`0x40ee34..0x40ef13`). Every group also
  reads `parms` as `parm`.
- An unknown key is printed ("Unknown key parsing an effect primitive",
  `0x40eda1`) and the parser reads on; so is an unknown group (`0x40eee4`).
- The file parser skips any top-level group that is not a primitive name
  silently (`0x406a51..0x406c0f`), so `forcefeedback` groups are inert.

### Physics (UpdateOrigin `0x401a50`)

Velocity takes the frame's acceleration, and the step's end is predicted.
Then:

- Nothing is tested without `usePhysics`.
- `expensivePhysics` (`0x800000`) always traces the step.
- Any other physics particle first asks cgame export 9 for the point's
  contents (`0x401b2e`, mask `0x1001`), but only while the cvar at
  `0xb43cd0` is set. That cvar is `RMG`, which the engine registers as
  `Cvar_Get("RMG", "0", 0)` in its startup (`0x4386e7`). So on ordinary
  maps such a particle never collides.
- The trace is the effects trace (`0x401130`, export 15, skip -1, mask
  `0x1001`), with the primitive's box under `useBBox`, or Ghoul2's under
  flag `0x20000` (`0x4011e0`).

Outcomes:

- **Starting in solid** stops the particle where it is: velocity and
  acceleration are cleared, and so are `usePhysics` and `impactFx`.
- **A hit** plays the impact effect unless the surface carries `0x80000`
  (`0x401bff`), at the hit and facing its plane (`0x406d70`, which builds
  the axis with MakeNormalVectors `0x443840`).
- With `impactKills` the particle then dies.
- Otherwise the velocity takes the fraction's acceleration, reflects about
  the plane (factor `-2.0`, `0x5663b8`) and is scaled by the elasticity,
  which then halves (`0x565f90`). Below a squared speed of 100
  (`0x566220`) it comes to rest and its physics ends. The particle is
  placed one unit off the plane.
- **No hit** moves it to the predicted end.

### Impact and death effects

- Each spawn picks its impact and death effect once, at creation, from the
  template's lists at `+0x58` and `+0x68` (Tail: `0x40a0e9`, `0x40a0f2`).
- The primitive pass (`0x40f34c..0x40f371`) clears `impactKills` from a
  primitive past its end, then runs its death routine. A primitive whose
  update fails (an impact kill) dies with the flag still set.
- Die (`0x401500`) plays the death effect only when `deathFx` is set and
  `impactKills` is not. It plays at the origin, facing a random normal:
  three draws between -1 and 1, normalized.

### Spawn shapes and absolute motion (CreateEffect `0x407cea..0x408e29`)

- **`cheapOrgCalc`** (`0x100`) adds the origin offsets along the world's
  axes instead of the play axis (`0x408038`).
- **`orgOnSphere`** (`1`) adds a point on an ellipsoid, in world axes. Two
  angles are drawn: 0..360 degrees and 0..180 degrees (`0x408124`). The
  radius and height are drawn once. The point is (sin θ sin φ r,
  cos θ sin φ r, cos φ h).
- **`orgOnCylinder`** (`4`) draws, for each component, the radius along
  the axis's second row and the height times a signed draw times 0.5 along
  its first. The point is then rotated about the first row by a drawn
  0..360 degrees (`0x4433f0`).
- **`axisFromSphere`** (`2`) points the axis out through that point.
  - On a sphere the other rows come from MakeNormalVectors.
  - On a cylinder (`0x40856a`) they are up × forward, unnormalized, then
    forward × that row. Up is world Z, or world Y when forward is exactly
    world Z.
- Velocity and acceleration (`0x408722..0x408e29`) draw each range once per
  component. They run along the axis, or along the world's with
  `absoluteVel` (`0x400`) and `absoluteAccel` (`0x800`). Only Particle,
  OrientedParticle, Tail and Emitter compute them (`0x40870a`).
- No read of `wind` (`+0xfc`) or test of `affectedByWind` (`0x10000`) was
  found in CreateEffect (`0x407c40..0x40bb00`) or UpdateOrigin. They are
  treated as inert (inference).

### Endpoints of lines and bolts

- `cheapOrg2Calc` (`0x200`) adds `origin2`'s offsets along the world's
  axes: on the traced path (`0x40922f`), and from the placement on the
  untraced path (`0x409765`).
- `org2isOffset` is read only on the traced path (`0x408f47`).
- `traceImpactFx` (`0x20`, `0x4093d9..0x409493`) picks an impact effect and
  plays it where the endpoint trace ends, facing the trace's plane, whether
  or not the trace hit.

### Sprites

- `depthHack` sets render flag 8 on a particle (`0x401620`).
- `setShaderTime` (`0x400000`) sets the entity's shader time to its
  creation time in seconds (`0x40f440..0x40f467`).

## Change

`code/compat/jka_effect_play.hpp`:

- serves the Tail family: its motion, curves (size, length, RGB, alpha),
  physics, impact and death effects, and its far end;
- runs the curve law, with the `random` word, on every moving and segment
  family (Particle, OrientedParticle, Tail, Line, Electricity);
- moves and turns oriented particles;
- serves `bounce` and `intensity` as one elasticity, which is Electricity's
  chaos;
- serves the particle flags: `usePhysics`, `expensivePhysics`,
  `impactKills`, `impactFx`, `deathFx`, and for sprites `depthHack` and
  `setShaderTime`;
- serves spawn shapes and absolute motion: `orgOnSphere`, `orgOnCylinder`,
  `axisFromSphere`, `cheapOrgCalc`, `absoluteVel`, `absoluteAccel`, and on
  lines and bolts `cheapOrg2Calc` and `traceImpactFx`;
- accepts `wind`, `affectedByWind` and an untraced `org2isOffset`, inert as
  in retail;
- reads keys and groups retail does not know past, as retail does, and
  keeps their names for the host's warning;
- raises Fn's spawn-count bound from 100 to 500, so retail's 150- and
  200-particle effects compile;
- accepts signed sizes and lengths on the curved families.

`code/compat/jka_effects.hpp` reads the `width`, `width2` and `height`
groups and the `parms` field as retail's parser does.

`code/client/client_jka_cgame_effects.hpp`:

- moves Particles, OrientedParticles and Tails each frame. A traced step
  goes through export 15; the outcome may play an impact effect or kill
  the particle.
- keeps each tail's starting origin, paused frames included, and draws its
  segment;
- plays death effects as particles end;
- plays the impact effect of a traced endpoint (`traceImpactFx`);
- registers `RMG` with retail's default and reads it;
- prints retail's warning for an ignored key;
- reads past non-primitive groups such as `forcefeedback`;
- includes impact and death effects in the graph compiled before a play.

## Fn presentation (invariant 11)

- A tail is an Fn polygon. It is a camera-facing quad from the origin to
  its far end, the size on either side, through Fn's shaders, fog, sorting
  and post-processing. Fn has no `RT_LINE` entity.
- Particles stay Fn sprites. `depthHack` is Fn's `RF_DEPTHHACK`.
  `setShaderTime` sets the Fn entity's float shader clock.
- Lines, bolts and oriented particles draw each frame's colour, alpha and
  size from their curves, through the same polygon path as before.
- Impact and death effects are ordinary nested plays through the same paths.

Fn adds no retail-exact option.

## Engine choices that differ from retail

- Fn draws from its own generator, as before.
- A cylinder point is rotated with Fn's own exact rotation. Retail's
  RotatePointAroundVector is only ever given a uniformly drawn angle here,
  so spawns spread the same way.
- With `RMG` set, Fn asks export 15 for a zero-length trace at the point
  instead of export 9's contents. The trace starts in solid exactly when
  the point lies in the same mask (`0x1001`).
- A tail that has not moved is skipped rather than drawn as a point.
- The unknown-key warning prints when an effect first compiles for play,
  not at registration.
- A death effect plays in the pass that retires its particle, either a play
  or the draw pass of the same frame.
- Lines and bolts now take their colour from the curve law each frame,
  rounded as UpdateRGB rounds. The earlier subset truncated a creation
  colour; retail's update rounds.
- Fn bounds the elasticity at ±100, the spawn radius and height at ±16384,
  and the spawn count at 500. Retail has no bounds.
- A step that would produce a non-finite position kills the particle.

## Still refused by name

- particle flags `ghoul2Collision` and `relative` (Ghoul2 traces, bolted
  effects);
- `depthHack` and `setShaderTime` on oriented particles, tails, lines and
  bolts. Fn's polygons carry no depth range or entity clock.
- the spawn flag `randRotAroundFwd`;
- `min` and `max` (box traces);
- `size2` and `length` controls on families that do not use them.

## Unit (Source)

New `tests/jka_tails_physics_tests.py`, plain and with `-ffast-math`. Its
kernel covers:

- a Tail declaration with retail's aliases;
- ignored keys and groups, the inert controls and thirteen named refusals;
- MakeNormalVectors;
- sphere points on the ellipsoid, with the axis through them;
- cylinder points at the radius and within the height, with their axis;
- cheap origins and absolute velocity;
- an untraced `cheapOrg2Calc` endpoint;
- UpdateOrigin's outcomes (bounce, the acceleration term, rest, miss,
  start in solid, impact kill, no-impact surfaces) and the `RMG` gate;
- the length curve with clamp, and a tail's far end;
- the random word;
- a Line's alpha and nonlinear width curves.

Through the actual imports, against a fake floor:

- a falling tail draws nothing while still, then a segment back to where it
  was by its length;
- it hits the floor, plays its impact effect there and bounces at half its
  elasticity, one unit off the plane;
- at its end it plays its death effect, facing a unit normal;
- an `impactKills` dart plays its impact and dies without a death effect,
  and a no-impact surface kills it silently;
- a plain `usePhysics` drop passes through the floor while `RMG` is 0, and
  with `RMG` set probes, traces and bounces;
- a Line with `traceImpactFx` plays its impact where its endpoint trace
  ends.

The existing effect tests were updated where the subset grew (their refusal
cases now use controls still refused), and the registration fixture serves
`RMG`. These pass in the shared tree:

- the five existing effect test files and the new one;
- the thirteen other fixtures that read or compile the client-game host.

`cl_cgame.cpp` compiles with `win32-dev`'s `/W4 /WX` command.

## Sweep (Source)

Every primitive of the 376 installed effect files was compiled through the
adapter at HEAD and with this change, using placeholder media handles
(`.tmp/jka-tails/sweep`). The tree side also reads past non-primitive
groups, as the host now does. This is a per-file check: it does not follow
an effect's children.

- 68 files compiled completely before; 190 do now.
- No file that compiled before refuses now.

The first refusals left are, by count:

- the Sound family (85, among them every weapon muzzle flash);
- the CameraShake family (22);
- Light curve words (21);
- the Decal family (19, among them most wall impacts);
- Emitter media (19);
- Cylinder curves, flags and spawn flags (14);
- the named refusals above (5);
- the Flash family (1).

## Live (Live subset)

Runtime `.tmp/runtime/jka-spawn-effect`, rebuilt in place. Its binaries
come from a `git archive` of HEAD `dd6383c6` with this slice's three code
files, built with `win32-dev`'s options (strict warnings). HEAD now carries
client-game import 51, so no overlay is needed.

The harness is `.tmp/jka-tails/listen.py`: the earlier harness, plus an
optional entity override. It is written into the run's isolated home as
`maps/<map>.ent`, which `sv_entityOverrides` reads through the virtual
filesystem. The override is `mp/ffa2`'s own entity text with five added
`fx_runner`s. Each run is windowed, loopback only, with no input, on a
private desktop, with `cg_renderToTextureFX 0`, `cl_menuBlur 0` and an
isolated home.

- **Before.** `before-glx` runs the previous runtime with the first
  override. It stops on `effects/sparks/spark_exp_nosnd.efx` ("effects
  curve controls are not served yet").
- **After, five effects.** `glx-probe-1..4`, `vk-probe-4` and `rtx-probe-4`
  play `spark_exp_nosnd`, `env/impact_dust`, `env/waterspray`,
  `env/tasbolt` and `saber/saber_block`, with the bolt's traced impact
  effect `env/btend` and `saber_block`'s nested `volumetric/smoke`. Every
  run plays to its scripted end with no error.
  - `waterspray`'s drops carry `usePhysics` without `expensivePhysics`, so
    on this ordinary map they pass through the floor and play no impact
    effect, as retail's would.
  - In `glx-probe-4` the sparks and the water spray draw as streaks along
    their motion.
  - The dust effect's debris (`expensivePhysics`, `bounce 0.2 0.3`) lies
    on the floor around its cloud.
  - The traced bolt runs down to the floor.
  - All three renderers draw the scene. The RTX room is again much darker.
- **After, curves.** `glx-probe-5` adds `env/red_cyc` and
  `mp/crystalbeamred` (Line curves with `random` and `linear wave`, and a
  traced beam), `env/water_steam2`, `sparks/bluesparks` and
  `chunks/sparkexplode`. All play and draw.
- **A shader gap.** One sprite in `glx-probe-5` draws as the engine's
  default image. `shaderlist` (`glx-probe-5-shaders`) names
  `gfx/exp/explosion1` and five other explosion shaders DEFAULTED. Each
  uses Academy's `oneShotAnimMap` stage keyword, which the renderers do not
  read. This is a renderer gap, like `glow` in the last slice, not an
  effects refusal.
- **siege_korriban.** `glx-korriban-1` runs `mp/siege_korriban` unmodified
  to its end. Its two newly served runners are not started outside siege
  play, so it shows only that nothing else regresses.

Retail installation `E:\Games\Jedi Academy` was read only; its inventory is
unchanged after every run.

## Open

- Weapon fire stays unverified. Every muzzle flash also holds a Sound
  primitive, and most wall impacts a Decal.
- The renderers' `oneShotAnimMap` keyword, which defaults six explosion
  shaders.
- No capture compares any of these effects with retail's.
- Physics on `RMG` maps, Ghoul2 collision, box traces and the remaining
  families are not served or not checked live.
- The RTX darkness noted in the last slice is not investigated.
