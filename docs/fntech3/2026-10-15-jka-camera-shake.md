# Jedi Academy effect camera shake, 2026-10-15

The [effect-sound slice](2026-10-15-jka-effect-sound.md) left the
CameraShake primitive as the largest first refusal of the per-file effect
sweep. It stopped 46 of the 376 installed effect files, among them the
muzzle flashes of the Bryar pistol, blaster, bowcaster, DEMP2, disruptor,
flechette, repeater and rocket launcher. This slice serves the
CameraShake family. All 73 CameraShake primitives in the installed files
now compile. Of the 376 files, 290 now compile completely, up from 247,
and none that compiled before refuses. Each spawn hands its shake to the
retail client game, which shakes its own view; fnTech3 draws that view
and places its listener there. The
[JSON companion](2026-10-15-jka-camera-shake.json) records the binaries,
retail identities, commands and outcomes. No compatibility gate closes.

Implementation is independently authored. Retail `jamp.exe` 1.01 and the
1.01 `cgamex86.dll` from `assets3.pk3` were read with capstone. Pinned
OpenJK was used only to locate routines (reference observation; no text
incorporated).

## Observed, retail `jamp.exe` 1.01

### The CameraShake case

CreateEffect (`0x407c40`) indexes its switch table (`0x40b5e8`) by the
family number less one: the Sound family (6) is entry 5 (`0x40b0dc`), the
FxRunner (10) entry 9 (`0x40b11e`), and CameraShake (12) entry 11,
`0x40b2ca`:

- It reads three of the template's ranges: the life (`+0x3c`, `+0x40`),
  the radius (`+0xec`, `+0xf0`) and the intensity (`+0x234`, `+0x238`).
  Each is drawn between its ends (`0x444230`) only when the ends differ;
  otherwise the first end is used.
- The life and radius are truncated to integers (`_ftol`, `0x539584`). The
  intensity stays a float. The intensity range is the one the shared
  `bounce`/`intensity` parser (`0x40cd90`) fills, so either key sets it and
  the later one wins.
- It calls `0x40c560` with the spawn origin (`[esp+0x40]`, the same point
  the Sound case uses, after CreateEffect's shared offsets and spawn
  shapes), the intensity, the radius and the life. The helper address
  pushed first (`0x12d0850`) is not read.
- `0x40c560` writes a 24-byte record to the client game's registered
  shared buffer (`[0xb25c98]`): the origin at `+0`, the intensity at
  `+0xc`, the radius at `+0x10` and the life at `+0x14`. It then calls the
  engine's VM call (`0x44ae90`) on the client game (`[0x8af0fc]`) with
  export 31.
- The case reads nothing else of the template and creates no primitive.
  It has no portal or frame test of its own.

### The client game's shake, retail `cgamex86.dll` 1.01

`vmMain` (`0x300425a0`) sends export 31 through its table (`0x30042854`)
to `0x3004281c`. That passes the four fields from its shared buffer
(`0x30740800`) to `0x30062c70`:

- It measures the distance from the view origin of its last computed view
  (`0x306f9f7c`) to the shake's origin (`0x30068d60`). Beyond the radius it
  does nothing.
- Within it, the intensity is scaled by `1 - distance / radius`, and a
  result above 16 is held at 16 (`0x30086088`).
- It stores that intensity, the life as the duration and the client
  game's current time as the start (`0x304fe268..0x304fe270`). A newer
  shake replaces the one before it.

Once a frame, after the view is computed, `CG_DrawActiveFrame` calls
`0x30062990` on the view origin and angles (`0x30063b9e`):

- Past its end the shake clears itself.
- Otherwise the scale falls linearly from 1 to 0 over the duration. Two
  field-of-view terms enter the scale, but both are set to 90 first, so
  their factor is 1.
- Each axis of the origin moves by a fresh draw in -1..1 (`rand & 0x7fff`
  over 32767, less a half, doubled) times the intensity and scale. Pitch
  and yaw take two fresh draws; roll takes the third origin draw.
- The client game then hands the shaken origin and axis to S_Respatialize
  (import 44, `0x30063cf7`) and draws its scene from them. No cvar gates
  the shake.

`viewpos` (`0x30022390`) prints that view origin and yaw, each truncated
to an integer.

### Values in the installed effects

The 16 muzzle flashes of the eight weapons shake within 60 units, except
the repeater's alternate fire (200). Their lives run from 50 to 200 ms and
their intensities from 0.2 to 1.4, so they shake only the firer's own view,
and only slightly. `explosions/explosion1` shakes up to 300 units away at
intensity 2 to 4. Every installed CameraShake declares `usePhysics`, which
`intensity` also sets; the case never reads it.

## Implemented

- **Compile** (`code/compat/jka_effect_play.hpp`). A CameraShake keeps its
  life, radius and intensity (`bounce` or `intensity`, the later key
  winning). Its count, delay, origin, sphere and cylinder spawn shapes and
  their height are served as for the other families. Every other field,
  flag word, list and curve group retail parses is accepted and inert,
  because the case never reads it. An intensity beyond 100 either way
  refuses by name, as do the shared bounds on life, radius, count and
  origin.
- **Instantiate** draws the intensity and radius from Fn's own generator,
  after the shared origin and shape, and truncates the radius and life
  toward zero as `_ftol` does.
- **Export** (`code/cgame/cg_public.h`, `code/compat/jka_cgame_exports.h`,
  `code/qcommon/vm.c`). A new engine export, `CG_RAVEN_CAMERA_SHAKE`, maps
  to Academy's export 31 with no arguments. Any other profile's client game
  refuses it before its module runs, as with the trace and map-change
  exports.
- **Playback** (`code/client/client_jka_cgame_effects.hpp`). A CameraShake
  spawn creates no instance. When it is created, at once or when its delay
  falls due, the host writes the 24-byte record
  (`fntech3_jka_cgame_camera_shake_t`, layout pinned by static assertions
  in `code/compat/jka_client_abi.h`) to the registered shared buffer and
  calls export 31. The effect context is closed to reentry during the call,
  as during an effects trace. If the module frees or resets the effects
  context, moves its shared buffer or changes the client game behind the
  call, the host stops with a named `ERR_DROP`.
- `fx_debug 2` prints each record as it is handed over.

### Fn view features (invariant 11)

The shake is computed by the retail client game in its own view, from
data the engine hands it, so the engine reproduces none of it. Fn renders
that view with its own renderer features, including field-of-view
correction, lighting and fog, and Fn's spatial audio follows the shaken
listener the module passes to S_Respatialize. fnTech3 has no engine view-shake setting
or reduced-motion option in any profile (none in its code or guides, nor in
the pinned FnQuake3 and FnQL), so none applies here. Such an option could
scale the intensity the engine hands export 31; it is not added by this
slice.

## Not served, recorded

- A paused frame (frame time 0). Fn's PlayEffect returns before creating
  anything, as the earlier slices measured for particles. Retail's
  CameraShake case has no frame test, so retail probably hands the shake
  over on such a frame (inference, not measured live). Fn drops it.
- The portal pass. Fn's portal lane plays no effects yet (recorded with
  the spawn-effect slice).
- Effects played on an entity (client-game import 268,
  `CG_FX_PLAY_ENTITY_EFFECT_ID`). The first-person view weapon's muzzle
  flash uses it, so a first-person shot stops there (below).

## Sweep

`.tmp/jka-camshake/sweep/run_sweep.py`, a copy of the sound slice's sweep,
compiles every installed effect file through the adapter at HEAD
`750f070f` and in this tree, both sides reading past `forcefeedback`
groups.

| | HEAD | This slice |
| --- | --- | --- |
| Files compiling completely | 247 | 290 |
| Newly served | | 43 |
| Newly refused | | 0 |

A second tool (`shake_only.cpp`) compiles every CameraShake primitive,
whatever precedes it in its file: all 73, in 73 files, compile. Of the 46
files whose first refusal was CameraShake, 43 compile completely,
including all 16 muzzle flashes of the eight weapons. The other three
(`emplaced/explode`, `explosions/hugeexplosion3`,
`explosions/probeexplosion1`) now stop on Emitter media. The next first
refusals across the sweep: Emitter media 23, Light curve words 21, Decal
21, Cylinder curve flags 6.

## Live

The runtime under `.tmp/runtime/jka-camera-shake` was built from a
checkout of HEAD `750f070f` with this slice, using win32-dev's options and
strict warnings. Each run hosts `mp/ffa2` with the retail modules from the
paks. It runs windowed on an unswitched private desktop, with
`net_enabled 0`, an isolated home and no input. The harness is
`.tmp/jka-camshake/listen.py`.

An entity override adds `fx_runner`s on an arc behind a first-person
spectator view. `setviewpos -2014 1536 90 180` settles the client game's
view at (-2076, 1536, 127), yaw 180, on every run without shakes. After a
240-frame settle, the run samples `viewpos` on 300 consecutive frames,
then takes engine screenshots 40 frames apart. Because `viewpos` truncates
and the eye sits on whole units, any shake of the origin shows as a
change of one unit. The screenshots are compared by phase correlation.

| Run | Runners | Records handed over | `viewpos` samples off the eye | Distinct views | Largest shot-to-shot shift |
| --- | --- | --- | --- | --- | --- |
| `glx-calib-1` | none | 0 | 0 of 5 | 1 | 0.00 px |
| `glx-near-3` | 16 muzzle flashes at 25 units | 492 | 294 of 300 | 8 (yaw 180 and 179) | 0.13 px |
| `vk-near-1` | the same, Vulkan | 491 | 211 of 300 | 10 (yaw 180 and 179) | 0.39 px |
| `glx-far-2` | 15 muzzle flashes at 90 units | 472 | 0 of 300 | 1 | 0.00 px |
| `glx-control-2` | 16 concussion muzzle flashes at 25 units | 0 | 0 of 300 | 1 | 0.00 px |
| `glx-explosion-1` | 3 `explosions/explosion1` at 60 units | 129 | 282 of 300 | 87 (yaw 177 to 182) | 1.76 px |

- In the near runs every muzzle flash is played and handed over, and the
  view jitters by up to a unit and a degree while the shakes last.
- In the far run the same records are handed over, but beyond the
  60-unit radius the client game does nothing: the view and frames stay
  still. The repeater's alternate fire, which shakes within 200 units, is
  left out of that run.
- The control places a muzzle flash that has no CameraShake at the same
  points; its view is as still as the calibration run's.
- The explosions shake the origin by up to 3 units and the yaw by up to
  3 degrees.
- `glx-near-1` and `glx-near-2` were first attempts. In `near-1`
  (timescale 0.1) the view was still settling when the runners began. In
  `near-2` the spectator came to rest 4 units short of the usual eye, at
  a whole-unit point, and jittered there; the repeat, `near-3`, rested on
  the usual eye.
- Every run ended normally, with no error, the installation unchanged and
  the runtime binaries unchanged.

### Weapon fire

The [weapon-fire slice](2026-10-15-jka-weapon-fire.md) landed while this
slice was under test; its Bryar pistol shot had stopped on CameraShake. A
second runtime, `.tmp/runtime/jka-camera-shake-fire`, was built the same
way from HEAD `aa0e7049` with this slice, and the Bryar was fired with that
slice's steps (`team free`, `togglemenu`, `give all`, `+attack`), sampling
`viewpos` on every frame around the shot:

- `glx-fire3p-1`, third person, as that slice fired. Two shots play
  `bryar/muzzle_flash` in full: its sound starts and its shake record is
  handed over (intensity 0.21 and 0.22, radius 60, 50 ms). The view stays
  still, as expected while the third-person camera is beyond the 60-unit
  radius (inference: the camera's distance from the muzzle was not
  measured). The listen server then stops on the game's Ghoul2 trace
  (game import 28), as the concussion rifle's shots do.
- `glx-fire-1`, first person. The view weapon's muzzle flash goes through
  client-game import 268 (`CG_FX_PLAY_ENTITY_EFFECT_ID`), which is not
  served, so the client stops there before any shake.

The guns' wall impacts still stop on Decal in the sweep.

## Tests

- `tests/jka_camera_shake_tests.py` (new, registered as
  `fntech3_jka_camera_shake`). The kernel compiles the life, radius and
  intensity, the shared parser, the constructor's defaults and the inert
  fields, and checks the named refusals, the draws, the truncation and the
  spawn shape. The host plays through the actual import 266 path. It
  checks immediate, delayed and nested shakes, a zero count, a paused
  frame, the record handed to export 31, and refusals when the module
  reenters, frees or resets the effects context or drops the shared
  buffer. Its Windows fixture links an 8 MB stack: clang keeps the effects
  pool's 540 KB reset in the import dispatcher's frame, and the reentry
  cases nest two of them. The engine's own MSVC frames there are under
  5 KB.
- `tests/jka_map_change_export_tests.py` reaches export 31 under
  `jka-mp` and checks that every other profile refuses it.
- `tests/jka_client_tests.cpp` checks the export's mapping and argument
  count; `tests/jka_sp_vm_export_tests.py`'s fixture names the new export.
