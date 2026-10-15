# Jedi Academy effect sounds, 2026-10-15

The [tails-and-physics slice](2026-10-15-jka-tails-and-physics.md) left
the Sound primitive as the largest first refusal of the per-file effect
sweep. It stopped 85 of the 376 installed effect files, among them every
weapon's muzzle flash, so live weapon fire ended in an `ERR_DROP`. This
slice serves the Sound family. Every Sound primitive in the installed
files now compiles. Of the 376 files, 247 now compile completely, up from
190, and none that compiled before refuses. Each spawn starts its sound
through fnTech3's own spatial audio. The
[JSON companion](2026-10-15-jka-effect-sound.json) records the binaries,
retail identities, commands and outcomes. No compatibility gate closes.

Implementation is independently authored. Retail `jamp.exe` 1.01 was read
with capstone. Pinned OpenJK was used only to locate routines (reference
observation; no text incorporated).

## Observed, retail `jamp.exe` 1.01

### The Sound case

CreateEffect (`0x407c40`) dispatches the Sound family (6) through its
switch table (`0x40b5e8`) to `0x40b0dc`:

- If the portal pass is running (byte `0x12a4ff0`, set and cleared by
  AddScheduledEffects `0x407810`), it picks a handle and calls
  `0x405c70`. That is S_StartLocalSound (its error string is
  `S_StartLocalSound: handle %i out of range`): no origin, the last sound
  listener's entity (`0xd83140`), channel 0.
- Otherwise it picks a handle (`0x405d50`) and calls `0x46ac60`, the
  engine's S_StartSound (`S_StartSound: bad entitynum %i`). It passes the
  spawn origin (`[esp+0x40]`, the point CreateEffect's shared code has
  already offset and shaped), entity `0x3ff` (ENTITYNUM_NONE) in `ebx`,
  channel 0 (CHAN_AUTO) and the handle in `eax`.
- No volume or radius is passed. PlayEffect (`0x407110`) copies its
  caller's volume and radius into the template (`+0x23c`, `+0x240`), but
  the Sound case reads neither. It reads nothing else of the template
  either.

The client game's own S_StartSound import (`0x412910`) reaches the same
`0x46ac60`, so an effect's sound and a client-game sound take one path.

### The list and the pick

- The `sound` and `sounds` keys share one parser (`0x40dad0`). It
  registers each name with S_RegisterSound (`0x46a020`) and appends the
  handle to the template's media list at `+0x48`. The shader and model
  parsers append to the same list.
- The pick (`0x405d50`) returns zero for an empty list. Otherwise it takes
  the global generator (`0x56e760`, multiplier `0x343fd`, increment
  `0x269ec3`), shifts right by 17 and scales by the list's length.
- Retail's handle 0 is the silent `sound/null.wav` its registration starts
  with (recorded in the ambient-sets slice), so an empty list is silent.

### Flag words

Retail's flag (`0x40d1a0`) and spawn-flag (`0x40d4e0`) parsers read up to
seven words (`%s %s %s %s %s %s %s`, `0x565c00`). A word they do not know
sets nothing and returns false (`0x40d79a`, `0x40d4bc`), but the loop goes
on to the next word, and the key loop ignores the result (`0x40edc5`).
`lessAttenuation`, which four installed Sound primitives name, is not a
string in `jamp.exe`: it is such an unknown word, inert in retail
multiplayer.

## Implemented

- **Compile** (`code/compat/jka_effect_play.hpp`). A Sound keeps its list
  of registered sound handles. Its count, delay, origin, the sphere and
  cylinder spawn shapes and their radius and height are served as for the
  other families. Every other field retail parses is accepted and inert,
  because the Sound case never reads it. Any non-sound media in a Sound
  refuses by name, because retail would play those handles from the same
  list as sounds.
- **Flag words.** An unknown flag or spawn-flag word is now read past on
  every family, as retail reads it, instead of refusing. Fn keeps the word
  and names it under `fx_debug`.
- **Instantiate** picks one handle per spawn from Fn's own generator, as
  the shader lists do.
- **Playback** (`code/client/client_jka_cgame_effects.hpp`). A Sound spawn
  creates no instance. When it is created, at once or when its delay falls
  due, the host calls `S_StartJKASound` with the spawn origin, entity
  1023 and Academy's channel 0. That is the entry the client game's own
  S_StartSound import uses: the channel is translated to the engine's
  CHAN_AUTO, and the sound joins Fn's spatial path. Handle 0 plays
  nothing, matching retail's silent null sound and a failed registration.
- `fx_debug 2` names each effect sound as it starts.

### Fn audio features (invariant 11)

Effect sounds are ordinary positional one-shots in fnTech3's audio, on
both backends. On OpenAL they take Fn's distance model, traced occlusion,
EFX room reverb, tone classes and HRTF where the device offers it. The
legacy mixer spatializes them as it spatializes every positional sound.
Nothing of retail's mixer is reproduced.

## Not served, recorded

- The portal pass's local sound. Fn's portal lane plays no effects at all
  yet (recorded with the spawn-effect slice), so no portal Sound plays.
- A paused frame (frame time 0). Fn's PlayEffect returns before creating
  anything, as the earlier slices measured for particles. Retail's Sound
  case has no frame test, so retail probably starts the sound on such a
  frame (inference, not measured live). Fn drops it.
- The template's volume and radius stay unread, as in retail.

## Sweep

`.tmp/jka-sound/sweep/run_sweep.py`, a copy of the tails slice's sweep,
compiles every installed effect file through the adapter at HEAD
`fbde92d4` and in this tree. Both sides now read past `forcefeedback`
groups, as HEAD does.

| | HEAD | This slice |
| --- | --- | --- |
| Files compiling completely | 190 | 247 |
| Newly served | | 57 |
| Newly refused | | 0 |

All 85 files that held a Sound primitive pass it now. Their 57 complete
files include the concussion rifle's, thermal detonator's, trip mine's,
detonation pack's and turret's muzzle flashes, the saber clash and
cut, punch and kick impacts, and the material splashes. The other 28 stop
later:

- 24 on CameraShake, including the muzzle flashes of the Bryar pistol,
  blaster, bowcaster, DEMP2, disruptor, flechette, repeater and rocket
  launcher;
- 2 on Decal;
- 1 on Flash;
- 1 on Emitter media.

The next first refusals across the sweep: CameraShake 46, Light curve
words 21, Decal 21, Emitter media 20.

## Live

The runtime under `.tmp/runtime/jka-effect-sound` was built from a
`git archive` of HEAD `fbde92d4` with this slice's two code files, using
win32-dev's options and strict warnings. Each run hosts `mp/ffa2` with the
retail modules from the paks. It runs windowed on an unswitched private
desktop, with `net_enabled 0`, an isolated home, no input, and sound on
through OpenAL Soft. A client that is not on the input desktop is
offscreen-muted, so the evidence is the voice dumps (`s_alDebugDump`), not
listening. The harness is `.tmp/jka-sound/listen.py`.

### Effect runners

An entity override in the run's home adds `fx_runner`s next to the
spawn point. `setviewpos -2014 1536 90 180` puts the listener about 120
units from them.

- `glx-runners-1` (sparks with a sound, and a rock smash). The run plays
  to its scripted end. 11 effect sounds start, and the dumps show them as
  OpenAL one-shots on entity 1023 at their own places:
  - `sound/ambience/spark1.wav` at 123.4 units, straight ahead (pan 0.00);
  - `sound/effects/stone_explosion.mp3` and `stone_drop_large.mp3` at
    155.3 units, to one side (pan -0.49);
  - each with the reverb send of the outdoor zone (wet 0.11) and no
    occlusion on the open line.
- `glx-weapons-1` and `vk-weapons-1` (the concussion rifle's and thermal
  detonator's muzzle flashes, a saber clash, a punch impact and the
  sparks). Both play to their scripted end with 28 effect sounds each. In
  both dumps:
  - `concussion/fire.wav` at 123.2 units, ahead;
  - `thermal/fire.wav` at 138.9 units, pan 0.46;
  - `saberhitwall3.mp3` at 142.8 units, pan -0.46;
  - `punch2.mp3` at 99.5 units, ahead.

  The weapon sounds take Fn's `weapon` tone class (high-pass) and the
  sparks the `world` class.

### Scripted weapon fire

The task's scripted shot does not get as far as an effect. Each step
stopped on something outside this slice:

- `+altattack` is not a command under the `jka-mp` profile (`Unknown
  command`). `cl_input.cpp` binds it only for Elite Force, so the probes
  used `+button7`.
- A local player starts as a spectator. `team free` joins the game, and
  the spawn effect plays.
- `give all` stops the client on the existing refusal `Jedi Academy
  cgame Ghoul2 sparse model-copy destinations are not served yet`
  (`glx-conc-alt-2`). `give weapon_concussion_rifle` gives nothing.
- After the join, the menu module keeps the key catcher (0x0002, printed
  by `togglemenu`), so every command carries `BUTTON_TALK` and the game
  fires nothing (`glx-conc-alt-3`: `+attack` and `+button7` start no
  effect).
- `togglemenu` would hand the menu module Escape, but that stops on
  `Unsupported Jedi Academy native UI export 3 with 3 arguments`, the
  module's key event (`glx-conc-alt-4`).

Once those are served, the sweep predicts where fire stops:

- **Concussion rifle.** Its muzzle flashes, shot, alternate ring, hit and
  miss compile completely. Its explosion stops on Flash.
- **Bryar pistol, blaster and the other guns.** Their muzzle flashes stop
  on CameraShake, and their wall impacts on Decal.

The [weapon-fire slice](2026-10-15-jka-weapon-fire.md) serves the four
blockers above and the sound mute a weapon switch needs; the concussion
rifle's muzzle flashes now play live, and its shots stop on the server's
Ghoul2 trace.

## Tests

- `tests/jka_effect_sound_tests.py` (new, registered as
  `fntech3_jka_effect_sound`). The kernel compiles the list, the inert
  fields and unknown words, and checks the named refusals and the pick
  over many spawns. The host plays through the actual import 266 path. It
  checks immediate, delayed and nested sounds, a zero count, a silent
  registration, and a refusal for mixed media, with the entity, channel,
  origin and handle handed to `S_StartJKASound`.
- `tests/jka_cgame_effects_tests.py` gives its fixture an
  `S_StartJKASound` stub.
- `tests/jka_spawn_effect_tests.py` now expects an unknown Emitter flag
  word to be read past, not refused.
