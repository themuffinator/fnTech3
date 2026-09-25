# RTCW's and Enemy Territory's voices on the engine's own audio

## Scope

Every Return to Castle Wolfenstein voice, single player and multiplayer,
and every Enemy Territory voice, on the OpenAL backend: the one-shots
`StartRTCWSound` and `StartWolfETSound` start, their local forms, and the
loops `AddRTCWLoopingSound` and `AddWolfETLoopingSound` submit. The legacy
mixer is unchanged.

Until this change `Q3SoundWorld::ApplyVoice`
(`code/client/audio/openal/AudioSystemWorld.inl`) rendered these voices as
retail's mixer panned them. Each voice was head-relative, at the gain and
left/right balance of retail's law (`SpatializeOrigin` and `SpatializeLoop`
in `code/client/audio/shared/RTCWLoops.h`). None was positional, so HRTF never
applied. None was traced for occlusion or given a tone class, and none sent
anything to the room reverb. The 47 RTCW and 6 ET audio zone sidecars the
package ships (`pkg/games/{rtcw-sp,rtcw-mp,wolfet}/*/maps/*.azb`) loaded and
were never heard.

Under invariant 11 of `AGENTS.md`, retail is authoritative for the data these
voices carry, not for how the engine presents it. The change follows the one
made for American McGee's Alice the same day ([Alice audio
record](2026-09-23-alice-audio.md), "The engine's own features").

## Retail's data, kept

Observed in the original sources (`RTCW-SP-master` and `RTCW-MP-master`
`src/client/snd_dma.c`, `Enemy-Territory-original` `src/client/snd_dma.c`)
and already pinned by the source-compared fixtures of the [one-shot
record](2026-09-16-rtcw-one-shot-audio.md) and the loop records:

- **Allocation.** Which slot a one-shot takes, what it cuts and what
  exhaustion evicts (`RTCWOneShots.h`), unchanged.
- **Loops.** Each frame's up to 128 submissions, each on its own voice. Retail
  RTCW's merge of duplicate loops never runs: its inner loop starts past
  `numLoopChannels` (SP `snd_dma.c` 1049, MP 1051). A loop's volume is
  captured at submission with SP's master fade, and its phase comes from the
  device clock or ET's submitted start time. It is virtualized when retail's
  law gives it nothing on either channel. All of this is unchanged.
- **The law.** A sound is at full volume out to 6.4% of its range and silent
  the range beyond that (`dist_fullvol = range * 0.064f`: MP `snd_dma.c` 595,
  ET 601). The range is 1,250 for every one-shot (`SOUND_RANGE_DEFAULT`) and
  the submitted range for a loop, 0 meaning 1,250. `RTCWLoops.h` now names
  these as `OneShotRange` and `Distances( range )`.
- **Levels.** A one-shot plays at 127, a loop at 90 over 256 of its submitted
  volume, capped at 255 per channel ("sphere": SP 1040, MP 1042, ET 1243; ET's
  127 "3d" branch is for loops marked `kill`, which neither ET loop entry
  sets). `LoopLevel( loop )` is that figure. An ET one-shot plays at the
  volume it was submitted with, 127 meaning unchanged.
- **Single player's effects stage.** `s_volume` and the master fade,
  quantized as retail quantizes them (`EffectsMixVolume`), scale each source,
  including its sends. The cap is 255/127 for an SP loop and 2 otherwise.
- **Doppler.** RTCW's retail mixer disables it for loops ("RF, disabled
  doppler for looping sounds": SP 1088, MP 1091). One-shots carry no velocity.

## The engine's own features

| Feature | RTCW and ET voices |
| --- | --- |
| Positional rendering | Every voice away from the listener that is mono, or two-channel with `s_alSpatializeStereo` |
| Distance model | OpenAL's active model (`s_alDistanceModel`, `inverse_clamped` by default) across the law's span: the full-volume distance as the reference, the silent distance as the far end, rolloff 1 (`OpenALDevice::ConfigureSourceDistanceRange`). The voice fades out toward the silent end (`HorizonGainAt`), because a clamped model keeps a floor there. A one-shot's span, 80 to 1,330, is a Quake III world sound's. `linear_clamped` plays retail's law itself. Without a distance model, the law's own gain plays, without its pan |
| Air absorption | `s_alAirAbsorption`, beyond the reference |
| HRTF and output modes | Every positional voice, where the runtime offers them |
| Occlusion | The traced occlusion and liquid boundaries, on the direct and wet paths, under `s_alOcclusion`. Nothing is traced for a voice past the law's silent end |
| Room reverb | The map's audio zones or the traced room, with the per-voice send that grows with distance and occlusion |
| Tone classes | By channel. RTCW's and ET's channels 0 to 7 are Quake III's numbers (`soundChannel_t` in each `q_shared.h`), so weapon, voice, item, body and announcer apply as they are. A voice with no channel class takes its sample name's or `world` |
| `s_pvs` | Honoured; off by default for every profile |
| Sound shaders | Gain, pitch and wet scale are honoured, but no rules ship for these profiles (`sound/fnql-weapon-sounds.sndshd` is packaged for Quake III and Quake Live), so nothing applies. Their distance scales do not apply, since the span is retail's |
| Sounds at the listener | Head-relative at full level. They take the room but no occlusion, and a tone class by channel |
| Interface sounds | A local channel (1, 6 or 7) at the listener is dry and head-relative, with the local or announcer tone. Elsewhere it is attenuated where it is, as retail's mixer attenuates every sound but the listener's own |

Left out, and why:

- **Loop Doppler.** RTCW's retail mixer disables it, and that is kept. ET's
  retail mixer pitches a moving loop (`S_AddLoopingSound`, `dopplerScale`, 1115),
  but fnTech3's ET loop path has never carried a loop's motion. This is left
  open, below.
- **Retail's pan.** The OpenAL backend places voices itself. Retail's exact
  gain and left/right balance remain an explicit option in the legacy mixer
  (`s_backend legacy`), which is source-compared with the original.
- **Elite Force single player's ambient loops.** They shared the retail-pan
  arm of `ApplyVoice`. Another session adapts them on its own path
  (`ApplySTVEFVoice`), which this change leaves alone.

### Where fnTech3 differs from retail's presentation

- The default curve is the engine's clamped inverse model over retail's
  span, not retail's straight line. It is the curve every Quake III world
  sound has, and `s_alDistanceModel linear_clamped` restores the line.
- The direction is OpenAL's panner or HRTF. Retail gives each channel half
  of a centred sound, which OpenAL's equal-power centre plays about 3 dB
  louder, as it does for Quake III's sounds.
- SP's 255/127 cap applies to a loop's source gain before the device
  attenuates it; retail caps each channel after. The two differ only when a
  master fade above one pushes a loop past the cap.

## Tests

- `tests/wolfenstein_openal_voice_tests.py` (new) compiles the real
  `Q3SoundWorld::ApplyVoice`, with the tone, occlusion, horizon and filter
  helpers it calls, against a device that records every call. Its seven cases
  cover:
  - one-shots: RTCW SP and MP placed through `ConfigureSourceDistanceRange(
    80, 1330 )`, traced, sent to the room, classed by channel, faded toward
    1,330 and untraced past it; ET's volume; `s_pvs` and the focus mute;
  - sounds and local channels at the listener;
  - loops: a submitted range's span, the retail level 89/127 for volume 255,
    no Doppler, range 0 as 1,250, and the cap under a master fade of 4;
  - a device without a distance model, which plays retail's measured gain
    (124/127 at 100 units, 87/127 for a loop there, 0 past the span);
  - SP's quantized effects stage;
  - a Quake III world sound unchanged, and an Elite Force SP ambient loop
    either retail-panned or handed to its own path.
- Run against `git show HEAD:code/client/audio/openal/AudioSystemWorld.inl`,
  six of the seven fail on the old retail-pan path. Only the other-profiles
  case passes.
- `tests/rtcw_one_shot_tests.py`'s spatial-gain fixture now expects the law
  without its pan: 124/127 at 100 units on every channel away from the
  listener, 89/127 for a loop at its origin, and that loop's full level.
- `tests/stvef_sp_ambient_tests.py` and `tests/stvef_sp_sound_distance_tests.py`
  no longer name the removed `rtcwSpatial` line. The Elite Force session has
  since rewritten them around its own path.
- The files that read the changed sources all pass:
  - `rtcw_one_shot_tests`, `rtcw_loop_audio_tests`, `rtcw_music_tests`,
    `rtcw_speech_tests`, `rtcw_spatial_audio_tests`;
  - `stvef_sp_ambient_tests`, `stvef_sp_sound_distance_tests`,
    `stvef_sp_lipsync_tests`;
  - `cgame_native_bridge_source_tests`, `wolfet_audio_tests`,
    `audio_speaker_layout_tests`, `audio_direct_channel_fallback_tests`,
    `frame_clock_source_tests` and `tests/audio`.
  `AudioSystem.cpp` compiles with `win32-dev`'s flags (`/W4 /WX`).

## Results

A developer client, `meson/build/win32-dev/fntech3.exe` SHA-256
`f99efe43e00020c4b81fbbabe94d44b976432185f7916ad9050ba9bd1e79bc89`, was
built at 15:59 on 2026-09-24. It came from the shared working tree at
`b68782b`, with this change and other sessions' uncommitted work, so no
commit reproduces it. The GL renderer is `2315eca2...` and the package
`758ebd16...`. Each run was windowed at 800 by 600 on an unswitched private
desktop (`tests/windows_private_process.py`), with no input, an isolated home
under `.tmp/wolf-audio/`, and `+devmap`. Each ran `s_info`, then
`s_alDebugDump` with `s_alSourceClassDebug 1`, at `s_volume 0.02` and
`s_musicvolume 0`, with `s_muteWhenUnfocused 0` and `s_muteWhenMinimized 0`,
so the unfocused window's soft mute did not hide the gains. Every run quit
cleanly and left its installation unchanged.

- **RTCW MP, `mp_beach`** (`run-mp_beach`):
  - `Audio zones: 246 loaded from maps/mp_beach.azb`; the environment is
    outdoors blending into a hall, `wet 0.28`.
  - Every loop within its range is `mode=3d tone=world` at gain 0.70, which
    is retail's 89/127. They fade out toward the silent end: `amb_war3.wav` at
    1,287 units plays 0.08.
  - One behind geometry is traced to `occ=0.74` and shaped `world/bandpass`.
  - Class summary: `class=world voices=8 ... route 3d=8 ... wet=2.48`. The
    other 66 loops are out of range and virtual, and their unset debug fields
    count them as `neutral`/`relative`.
- **RTCW SP, `escape1`** (`run-escape1`, `run-escape1-kill`):
  - `Audio zones: 397 loaded from maps/escape1.azb`; a stone room, `wet 0.55`.
  - The prisoner's sob, the drip and the fires are `mode=3d tone=world/bandpass`
    behind the cell walls (`occ=0.84` to `1.00`), with `wet=3.33` across the
    four. Their gain, 0.01, is `s_volume 0.02` through SP's effects stage.
  - `kill` played `sound/player/bj2/death2.wav` through RTCW's allocator at
    the listener: `mode=relative tone=voice/bandpass ... wet=0.30`, no
    occlusion.
- **ET, `oasis`** (`run-oasis`, and with `bot_enable 1` and four `spawnbot`s
  in `run-oasis-bots2`):
  - `Audio zones: 512 loaded from maps/oasis.azb`; a hall, `wet 0.53`.
  - Its loops are `mode=3d tone=world` at 0.70 with sends (`wet=0.95` across
    two).
  - Two map one-shots came up as `mode=3d tone=item/bandpass`, and both were
    silent past the span: `beeper.wav` at 1,336 units and `parrot.wav` at
    4,827. The bots never came in range.
- The runtime on this machine is the system `OpenAL32.dll`, "Generic Software
  Compatibility Mode". It has EFX, but not `ALC_SOFT_HRTF` or
  `AL_SOFT_source_spatialize`, so HRTF could not be observed.

### Before and after

A snapshot of the same working tree, copied to `.tmp/wolf-audio/snap/` and
configured like `win32-dev` with the GL renderer only, was built twice. The
first build was as it was: `fntech3.exe` SHA-256 `bb6d3d96...`. The second
had this change's `ApplyVoice` hunks reverted by exact match: `af7fd783...`.
The reverted `ApplyVoice` equals HEAD's apart from the Elite Force session's
early branch, which both builds share. The only other change to the
snapshot is a no-op `CL_RitualHudMenusFrame`, in both builds, because
another session had declared that function without defining it yet. Both
builds ran the same three probes (`ab-before-*`, `ab-after-*`, with
`net_enabled 0`).

| Map | Before | After |
| --- | --- | --- |
| `mp_beach` | 74 loops `neutral`, all `relative`, `wet=0.00`; `amb_war2.wav` at 951 units `gain=0.20 pan=-1.00` | 8 in range `world`, `route 3d=8`, `wet=2.48`; the same loop `mode=3d tone=world/lowpass gain=0.70 pan=0.00` |
| `escape1` | 13 loops and the death cry `neutral`, `relative`, `wet=0.00`, no occlusion (`prisoner_sob.wav occ=0.00 pan=0.30`) | 4 in range `world`, `route 3d=4`, `wet=3.33`, `prisoner_sob.wav occ=1.00`; the death cry `tone=voice/bandpass wet=0.30` |
| `oasis` | 37 loops `neutral`, `relative`, `wet=0.00` | 2 in range `world`, `route 3d=2`, `wet=0.95` |

Both builds loaded the same sidecars (`246`, `397` and `512` zones) and the
same environments. Only the after build's voices are sent to them.

No listening test has been made.

## Still open

- No remote one-shot within its range sounded in these runs, so the audible
  positional one-shot has fixture evidence only.
- HRTF on a runtime that offers it.
- ET's retail loop data that fnTech3's shared Wolfenstein loop path does not
  carry, recorded here while reading the source:
  - the pitch Doppler of a moving loop;
  - the merge of same-sample, same-phase loops into one channel (ET
    `snd_dma.c` 1252 to 1273);
  - a submitted volume up to 65,535 before the 255 channel cap, where
    `LoopVolume` clamps it to 255 first.
  None of these was introduced by this change.
- ET's one-shot flag `SND_NO_ATTENUATION` (0x20), which retail's mixer
  honours, is ignored by both backends. No shipped ET module passes it.
