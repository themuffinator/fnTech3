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
| The spread | `s_separation`, registered archived at `0.5` by both executables (`0x41fad2`, `stvoyHM.exe` `0x429ab7`): the near channel takes the pan blended towards the cvar, the far channel the cvar less the pan, neither below nothing (`0x4205dd`) | The same cvar, registered for the Elite Force profiles alone and clamped to 0 through 1. Its default is the even spread the engine already has, so no other game is moved |
| A mono device | Both channels at three quarters (`0x4205d1`) | The same in single player; the engine's own devices stay at one |
| The sounds the player makes | A sound from the listener's own entity starts at three quarters on every channel the game numbers below its ambient one, and at its whole volume on the others (`S_StartSound` `0x4208cc`) | The same, in both backends, on the numbers those channels are marshalled to: the engine's own up to its body channel, and single player's attenuated voice |
| Multiplayer | Quake III's own: 80 units, one falloff for every channel, a mono device at one (`stvoyHM.exe` `0x42a580`) | Unchanged, and the single-player distances are gated on the single-player profile |
| The ambient loops | Measured as sounds with no channel of their own (`0x420d28` passes none) | The same, and now spread by the cvar as well |

## What it touches

`S_SpatializeOrigin` in the legacy mixer takes the channel a sound plays on
and, in single player, asks the header how far that channel carries. The
OpenAL backend measures the same way and then leaves the device's own
distance attenuation off for those voices, because they have already been
measured; the fade that hides the device's residual floor is left off with
it, since it would silence a voice well short of the 2,274 units one
carries. Single player's voices stay positional, so the engine's reverb,
occlusion and HRTF reach them as they reach every other profile's.

Both backends also hold a sound back as it starts, where it belongs to the
listener's own entity and plays on one of the channels retail holds back, so
that what the player does is quieter than the world around them.

## Results

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

## Tests

- `stvef_sp_sound_distance_tests` compiles the distances: the three
  falloffs, each at its full-volume distance, at half volume and past
  silence, every other channel carrying as far as a sound with none, the
  spread at both ends and at its default, the mono device, and which channels
  hold a sound back at the listener. It pins the channel numbers single
  player's voices arrive on against the engine's own enumeration and the
  single-player ABI, the channels held back derived from that same
  marshalling, both backends holding a sound back as it starts, the cvar's
  registration and its Elite Force gate, the channel reaching the legacy
  mixer with the ambient loops measured as channel-less, and the OpenAL
  backend leaving the device's attenuation and horizon fade off for what it
  has measured while keeping those voices positional.
- `stvef_sp_ambient_tests` continues to cover the loops themselves.

## Remaining

Lip-sync, which the game reads from the engine's per-entity voice volumes
(`s_threshold1` through `s_threshold4` against each sample's `#MAXVOL`
peak), and Bink movies.
