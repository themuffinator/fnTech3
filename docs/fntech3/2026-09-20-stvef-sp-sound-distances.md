# Elite Force single-player sound distances

## Scope

This record covers how far single player's sounds carry, how loudly they
start and how they are spread between the two channels: the distance each
channel is heard at full volume, the falloff past it, the sounds the player
themselves make being held back, the `s_separation` cvar both of Elite
Force's executables register, and what a mono device hears. Lip-sync and Bink
movies belong to later slices. The profile stays behind
`-Dstvef-development=true`.

The distances live in `code/client/audio/shared/STVEFAmbient.h`, which the
[ambient record](2026-09-20-stvef-sp-ambient.md) already used for its loops;
both of the engine's sound backends now measure every single-player sound
the same way.

## The contract

| Contract | Retail `stvoy.exe` 1.20 | fnTech3 |
| --- | --- | --- |
| How a sound is measured | The distance from the listener, less the distance the sound is heard at full volume, times a falloff, taken from one; what is left multiplies the volume the sound was started with (`S_SpatializeOrigin` `0x420510`) | The same |
| A sound with no channel of its own | Full volume to 256 units, falling by 0.0008 a unit, silent by 1,506 (`0x420578`) | The same. The engine's own is 80 units, so single player's world is louder and carries further |
| A voice | Full volume to 1,024 units, the same falloff, silent by 2,274 (`0x42055b`) | The same, on the channel single player's voices arrive on |
| An attenuated voice | Full volume to 38.4 units and falling five times as fast, silent by about 290 (`0x420568`) | The same. The engine has no channel for it, so it is marshalled to a number of its own above the engine's (`CL_STVEFSPSoundChannel`) and the mixers look for that one |
| The spread | `s_separation`, registered archived at `0.5` by both executables (`0x41fad2`, `stvoyHM.exe` `0x429ab7`): the near channel takes the pan blended towards the cvar, the far channel the cvar less the pan, neither below nothing (`0x4205dd`) | The same cvar, registered for the Elite Force profiles alone and clamped to 0 through 1. Its default is the even spread the engine already has, so no other game is moved. The legacy mixer blends the pan by it; the OpenAL backend spreads placed voices by it above the default ([below](#the-engines-own-features)) |
| A mono device | Both channels at three quarters (`0x4205d1`) | The same in single player's legacy mixer; the engine's own devices stay at one |
| The sounds the player makes | A sound from the listener's own entity starts at three quarters on every channel the game numbers below its ambient one, and at its whole volume on the others (`S_StartSound` `0x4208cc`) | The same, in both backends, on the numbers those channels are marshalled to: the engine's own up to its body channel, and single player's attenuated voice |
| Multiplayer | Quake III's own: 80 units, one falloff for every channel, a mono device at one (`stvoyHM.exe` `0x42a580`) | Unchanged, and the single-player distances are gated on the single-player profile |
| The ambient loops | Measured as sounds with no channel of their own (`0x420d28` passes none) | The same, and now spread by the cvar as well |

## What it touches

`S_SpatializeOrigin` in the legacy mixer takes the channel a sound plays on
and, in single player, asks the header how far that channel carries. The
OpenAL backend takes the same distances, as each channel's span, and renders
them with its own features; see [the engine's own features](#the-engines-own-features)
below. Until 2026-09-24 it measured the law itself, folded retail's gain and
pan into each voice's gain, and left the device's distance model, air
absorption and fade off.

Both backends also hold a sound back as it starts, where it belongs to the
listener's own entity and plays on one of the channels retail holds back, so
that what the player does is quieter than the world around them. Retail's own
`S_StartSound` takes no volume, so it sets those sounds to three quarters of
full rather than scaling what it was asked for; the engine scales instead,
which is the same thing wherever retail can be asked, and keeps a sound the
engine wanted quieter from being made louder.

## The engine's own features

Under the project's rule that every profile runs on fnTech3's own features
(invariant 11 of `AGENTS.md`), retail is authoritative here for the data:
each channel's full-volume and silent distances, the volume a sound starts
at, the sounds the player makes being held back, the volume an ambient set
has faded to, and the separation cvar. Since 2026-09-24 the OpenAL backend
renders that data as it renders any profile's sounds (`ApplySTVEFVoice`,
`code/client/audio/openal/AudioSystemWorld.inl`), for every single-player
voice: the one-shots, the entities' loops and the ambient sets' loops. The
header gives the law as each channel's span (`ChannelSpan` in
`shared/STVEFAmbient.h`):

| Channel | Full volume out to | Silent from |
| --- | --- | --- |
| A voice | 1,024 units | 2,274 |
| An attenuated voice | 38.4 | 288.4 |
| Every other channel, and the ambient loops | 256 | 1,506 |

| Feature | Single player's voices |
| --- | --- |
| Distance model | OpenAL's (`s_alDistanceModel`): the channel's full-volume distance as the reference and its silent distance as the max distance, faded out toward the silent end (`HorizonGainAt`) as every profile's sounds are, because the clamped inverse and exponent models never fall silent. `s_alDistanceModel linear_clamped` plays retail's straight-line law itself, since OpenAL's clamped linear model has that shape. The default inverse model keeps both distances but not the line between them: a sound 1,000 units off plays at 0.26 of its level, where retail's is 0.40. A device without a distance model plays the law's own gain (`SpanGain`) without its pan |
| Air absorption | `s_alAirAbsorption`, beyond the full-volume distance |
| HRTF and output modes | Every voice away from the listener. Single player's sounds are mono |
| Occlusion | The traced occlusion and liquid boundaries, on the direct and wet paths, under `s_alOcclusion`. Nothing is traced for a voice the law has silenced |
| Room reverb | The map's audio zones or the traced room, with the per-voice sends. No Elite Force map has a generated zone file yet, so today it is the traced room |
| Tone classes | By channel. Single player's attenuated voice, which arrives on a number of its own above the engine's channels, is a voice; its ambient and menu channels have no class of their own |
| Spread | `s_separation` above its default half spreads each placed voice toward both channels, through the source's radius (`AL_EXT_SOURCE_RADIUS`, which OpenAL Soft has; `s_info` reports it). Retail keeps (1 − s) / s of a side sound's difference between its near and far channel; a first-order panner keeps (1 + cos(spread / 2)) / 2 of a source's direction (OpenAL Soft's spherical source); `SeparationSpread` sets the two equal, so at 1 a voice is heard alike on both sides. This is an inference from the two formulas, not a listening comparison |
| Doppler | Where `s_doppler` asks for it and the game gave a velocity. The ambient loops have none |
| `s_pvs` | Honoured, as for every profile; off by default |
| Sounds at the listener | A sound from the listener's own entity, and the room tone of the set the player is in, take the room but no occlusion. The cgame hands the room tone the view origin (`S_UpdateAmbientSet` `0x41dd20`), so a loop within a unit of the listener is heard there. A local, announcer or menu channel at the listener is an interface sound, dry and head-relative. Away from the listener, retail measures a local channel by where it is, and so does the backend |
| Levels | A one-shot plays at the volume it was started with, held back or not. An ambient loop plays at its set's volume over a one-shot's 127, as retail hands both to its mixer. The room tone keeps the share of a centred sound that retail's separation leaves on each channel: half at the default, all of it at 1, none at 0 |
| Distant starts | A one-shot is kept if it starts within 1.15 times its channel's silent distance, so a voice started 2,000 units off plays. Quake III's horizon had dropped every sound started beyond 1,530 units |
| Loops out of range | A loop keeps no source past its span, or while its set is faded out, and takes one again when it comes back. Retail merges the loops playing one sound into a single channel; the backend keeps a source for each, as it did before, so each is heard from its own point |

Left out, and why:

- The weapon sound shaders (`sound/fnql-weapon-sounds.sndshd`) tune Quake
  III's and Quake Live's weapon samples by name and have no Elite Force
  rules, so there is nothing for them to apply. Their distance scales would
  also replace retail's spans.
- A separation below the default half cannot narrow a placed voice, because
  OpenAL has no source narrower than a point. Only the room tone's level and
  the legacy mixer follow it there.
- Retail's straight-line falloff is exact only under
  `s_alDistanceModel linear_clamped`; the default is the inverse model every
  profile uses.

Retail's exact gain and pan remain the legacy mixer's (`s_backend legacy`),
the explicit retail-exact option: the law, the pan blended by
`s_separation`, and three quarters on a mono device.

## Results

The runs below, from 2026-09-20, describe the legacy mixer and the OpenAL
backend as it was before [the engine's own features](#the-engines-own-features);
the OpenAL runs after that change are under [2026-09-24](#2026-09-24).

A developer client with the `stvef-sp` profile, 1,024 by 768 in a window,
with an isolated writable home path, on `voy1`, where the bridge crew speak
across the room:

- With the OpenAL backend, the crew are heard as positional voices at their
  own distances: `sound/voice/janeway/cin/03/theydontrespond.wav` at 568.8
  units with gain 0.57 and pan −0.16, and
  `sound/voice/seven/cin/03/weresponded.wav` at 622.3 units with gain 0.69
  and pan −0.38. Both keep their full volume: a voice carries to 1,024
  units, so at 568.8 the mix is the pan alone.
- The legacy mixer plays the same scene at 73 and 53, and 87 and 39 — the
  pan alone, as the header computes it (127 × 0.58 and 127 × 0.42; 127 ×
  0.69 and 127 × 0.31), and the same numbers the OpenAL backend reports as
  gains (73 ÷ 127 = 0.57, 87 ÷ 127 = 0.69). Measured the way the engine
  measured them before, the same two sounds would have been 44 and 32, and
  49 and 22.
- With `s_separation 1` the same scene plays every voice at 127 on both
  channels, which is the spread turned off, so the cvar reaches the mixer.
- The room's ambient loop stays a sound at the listener — the OpenAL dump
  holds `sound/ambience/voyager/voy1bridge.wav` at no distance and full
  gain. The bridge's own machinery is heard at its distance and panned:
  `sound/ambience/voyager/lcars1.wav` at 0 and 113, and
  `sound/movers/doors/door1.wav` at 39 and 47.
- A sound the player themselves makes on one of the held-back channels plays
  at three quarters: `kill` sounds `sound/voice/munro/misc/death3.wav` on the
  voice channel at 95 on each side, where the whole of it is 127. The
  cinematic's own explosions, which the game plays on a channel above
  ambient, stay at 127.
- Holomatch was started on `hm_voy1` with the same build and the same mixer
  trace, and reached the map with nothing sounding in an empty server.
- The installation was left unchanged.

### 2026-09-24

A developer client with the `stvef-sp` profile, 1,024 by 768 in a window on
an unswitched private desktop (`tests/windows_private_process.py`), with no
input, an isolated writable home path, `+devmap`, `s_backend openal`,
`s_muteWhenUnfocused 0`, `s_volume 0.02`, `s_musicvolume 0` and `net_enabled
0`, dumping the world with `s_alDebugDump` and `s_alSourceClassDebug 1`.
Every run left the installation unchanged. The client was linked from
`meson/build/win32-dev`'s objects of the shared working tree into
`.tmp/runtime/stvef-audio/fntech3.exe` (SHA-256 `5b5f0068…12f1ce4`, over
commit `b68782b`), because another session's probes held the developer
executable. Another session's unfinished Ritual HUD change left one
function undefined, so the link added a stub for it, which the `stvef-sp`
profile never reaches. The system `OpenAL32.dll` is the Creative router's
generic software device, which has none of OpenAL Soft's extensions, so the
runs put an OpenAL Soft 1.25.2 `soft_oal.dll` beside the executable:
`s_info` reported `AL_EXT_SOURCE_RADIUS: available`, EFX with four sends and
`distance inverse_clamped`.

- On `voy1` the room tones play at the listener, head-relative, at 1.00:
  first `sound/ambience/voyager/voy1bridge.wav`, then
  `sound/ambience/voyager/bridgeamb.wav` as the sets change. Each takes the
  room's send, 0.09 in the traced "outdoors", 0.20 in a room and 0.22 in a
  hallway, and neither is occluded.
- The crew's lines are placed in the world: Kim's `conduitsrupturing.wav` at
  673.8 units, Paris's `warpdriveoffline.wav` at 551.7 and Janeway's
  `theydontrespond.wav` at 568.8. Each is `3d` with tone class `voice` at
  gain 1.00, since a voice holds its full volume to 1,024 units. Late in the
  scene Torres's `theresaleak.wav` at 28.5 units is unoccluded, with dry 1.00
  and a room send of 0.23.
- A door is placed and toned as the world: `sound/movers/doors/door1.wav` at
  60.2 units, unoccluded, dry 1.00, send 0.24, and the door's loop
  `classicdoorsmove.wav` at 647.2 units. The cinematic's explosions belong to
  the player's entity and play head-relative in the room.
- During the opening, the listener the cgame places is traced as outdoors,
  and the crew's lines 550 to 670 units off are fully occluded there (dry
  0.28, band-passed, send 0.51). An engine screenshot at 30 seconds shows
  the bridge while Janeway's line plays so, so the listener the cgame hands
  over is not where the camera looks from. The 2026-09-20 backend traced the
  same occlusion. Which origin retail's cgame passes during a cinematic is
  still open.
- On `tutorial` Tuvok's lines at 149.8 units are `3d`, class `voice`,
  unoccluded, dry 1.00 with a send of 0.27, and the room tone
  `conferenceamb.wav` plays at the listener at 1.00. A run with
  `s_alHrtf on` and `s_separation 1` reported `output stereo-hrtf` and
  placed the same voices the same way, with the tone at 2.00, both of
  retail's full channels. The dump does not show a source's radius; the
  spread is checked by `stvef_sp_openal_voice_tests`.

A screenshot taken with the `screenshot` command at `voy1`'s start dropped
the level with `Hunk_AllocateTempMemory: failed on 2360072` at the default
64 MB hunk; `com_hunkMegs 128` took it.

## Tests

- `stvef_sp_sound_distance_tests` compiles the distances: the three
  falloffs, each at its full-volume distance, at half volume and past
  silence, every other channel carrying as far as a sound with none, the
  spread at both ends and at its default, the mono device, and which channels
  hold a sound back at the listener. It pins the channel numbers single
  player's voices arrive on against the engine's own enumeration and the
  single-player ABI, the channels held back derived from that same
  marshalling, both backends holding a sound back as it starts, the cvar's
  registration and its Elite Force gate, and the channel reaching the legacy
  mixer with the ambient loops measured as channel-less. It also compiles
  each channel's span against retail's measure every 7.5 units out past
  silence, the attenuated voice's tone channel and the menu channels, whose
  numbers it pins against the ABI, and checks that the OpenAL backend takes
  the spans rather than retail's gain and pan.
- `stvef_sp_openal_voice_tests` compiles the real `ApplySTVEFVoice` against a
  recording device: each channel's span handed to the distance model, the
  fade toward the silent end with nothing traced past it, a voice still heard
  at 2,000 units, the tone classes, sounds and interface sounds at the
  listener, the ambient loops' levels, the room tone at the listener with
  the separation's share, the spread above the default half read back through
  OpenAL Soft's own formula, a device without a distance model, a stereo
  sample, `s_pvs`, the focus mute, and which loops keep no source. It also
  checks that `ApplyVoice` hands every single-player voice over before
  measuring anything, the virtual loops, the start-time cull, and the
  device's radius extension and pool reset.
- `stvef_sp_ambient_tests` continues to cover the loops themselves, and
  `rtcw_loop_audio_tests` compiles the virtual-loop pass with the Elite Force
  arm in it.

## Remaining

Bink movies. Lip-sync, which measures the same voices these distances carry,
has its own [record](2026-09-20-stvef-sp-lipsync.md).

For the OpenAL backend: Elite Force's maps carry no generated audio zones
(`voy1` looked for `maps/voy1.azb`), so the traced room is their only
reverb. The listener origin the cgame hands over during a cinematic, which
decides what is occluded, is not yet compared with retail's. No listening
test has been made.
