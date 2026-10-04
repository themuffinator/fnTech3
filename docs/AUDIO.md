# Audio Guide

fnTech3 plays every game's sound through one audio system. Each game's modules
still decide what to play -- which sample, on which channel, from where, at
what volume -- exactly as its original engine let them. fnTech3 decides how it
sounds: the default OpenAL backend renders every game with positional sources,
distance falloff, geometry occlusion, environmental reverb, audio zones and
HRTF, fed with that game's own sound distances, channels and loops. The
original software mixer remains available as a deterministic fallback, with
each profile's comparison options and limits described below.

This guide covers the player-facing audio controls: backend selection, device
choice, volume and focus muting, spatial audio, how each game's sound is
rendered, codecs and music, legacy mixer tuning, and the commands that show
what the engine actually started. It applies to every game with a client:
Quake III Arena, Quake Live, Return to Castle Wolfenstein, Enemy Territory,
Elite Force, Alice, F.A.K.K.2 and Medal of Honor: Allied Assault. Jedi Academy
and Jedi Outcast have partial developer client adapters; supported calls use
the existing audio services, but their complete spatial audio contracts remain
unverified. Academy's ambient definitions and mover-stage handle queries are
served, and its MP3 assets use the existing Fn decoder. Its optional-sound
registration control suppresses missing-sample warnings while preserving
registration results and playback. Explicit music preloading owns actual Fn
decoder streams for later transfer to the normal intro/loop player; muted
OpenAL probes preload an installed map track. Dynamic music, audible output,
ambient playback and active voice tracking remain open. The measured
Academy voice-volume query returns zero only when the actual audio backend
is absent; an enabled backend refuses until its speech amplitude is adapted
into these same Fn audio services. The IHV Test runs only as a
dedicated server, which plays no sound.

## Overview

If you want the short version first:

- `s_backend openal`: Use the default OpenAL backend with spatial audio support.
- `s_backend legacy`: Use the original software mixer and platform device path instead.
- `s_backendActive`: Read-only cvar that reports which backend actually started.
- `s_alDevice`: Pick a specific OpenAL playback device. Leave it blank for the system default device.
- `s_alHrtf`: Request OpenAL Soft HRTF mode: `auto`, `on`, or `off`. Requires `snd_restart`.
- `s_alHrtfId`: Request a preferred OpenAL Soft HRTF by name or numeric index. Requires `snd_restart`.
- `s_alOutputMode`: Request OpenAL Soft output mode: `auto`, `headphones`, `speakers`, `surround`, `quad`, `5.1`, `6.1`, or `7.1`. Requires `snd_restart`.
- `s_alDistanceModel`: Request the OpenAL world-sound distance model. Requires `snd_restart`.
- `s_alFrequency` / `s_alRefresh`: Request OpenAL mix frequency and refresh-rate hints. Requires `snd_restart`.
- `s_alMonoSources` / `s_alStereoSources`: Request OpenAL source-count hints. Requires `snd_restart`.
- `s_alOutputLimiter`: Request OpenAL Soft output limiting when supported. Requires `snd_restart`.
- `s_alSpatializeStereo`: Optionally route two-channel world samples through OpenAL positional rendering. Default `0`; requires `snd_restart`.
- `s_alAutoRecover 1`: Try OpenAL Soft live device recovery after a playback-device disconnect.
- `s_volume`: Set the master volume for all game audio.
- `s_musicVolume`: Set the music volume independently of gameplay sounds.
- `s_voiceVolume`: Set received Steam voice-chat volume, in Quake Live, independently of music and cinematics.
- `s_pvs`: Opt into retail Quake Live's client-side PVS cull for positional sounds. Default `0`.
- `s_muteWhenUnfocused 1`: Mute audio when the game window loses focus.
- `s_muteWhenMinimized 1`: Mute audio when the game is minimized.
- `s_alReverb 1`: Enable OpenAL environmental reverb when the device supports EFX. Requires `snd_restart`.
- `s_alOcclusion 1`: Enable geometry-based occlusion on the OpenAL backend.
- `s_alReverbGain`: Scale reverb send level from `0` to `2`.
- `s_alOcclusionStrength`: Scale occlusion strength from `0` to `2`.
- `s_alAirAbsorption`: Scale how much distance darkens positional sounds when EFX is available. Default `2.0`.
- `s_alAudioZones 1`: Use `maps/<map>.azb` audio-zone sidecars when present.
- `s_doppler 1`: Enable doppler shift on moving projectiles.
- `s_info`: Print the active backend, device, and runtime audio state.
- `s_alSoundStartDebug 1`: Trace RTCW OpenAL sound requests, rejected starts,
  source state and errors. Use with `logfile 2` for an intermittent sound issue;
  return it to `0` after collecting the log.
- `s_alListDevices`: List OpenAL playback devices, marking the default, requested, and active devices when known.
- `s_alListHrtfs`: List OpenAL Soft HRTF specifiers for the active or requested device.
- `s_alRecoverDevice`: Manually ask OpenAL Soft to reopen or reset the active playback device.
- `s_alConfigHints`: Print OpenAL Soft configuration-file guidance and live capability hints.
- `snd_restart`: Reinitialize the sound system after backend or latched-device changes.

Some games add settings of their own; see [Each Game's Sound](#each-games-sound).

## Backend Selection

fnTech3 exposes two audio paths on the client side.

- `openal`: The default backend. This is the modern path and the only one that provides the current spatial audio controls such as reverb, occlusion, audio zones, device selection, and the spatial debug tools.
- `legacy`: The original id Tech 3 software mixer and output-device path, extended with profile-specific gain and pan rules and with the retail Quake Live controls described below. Use it when OpenAL is unavailable or when you need the classic fallback. Check [Each Game's Sound](#each-games-sound) for the supported comparison options and remaining limits.

Behavior notes:

- `s_backend` is latched, so change it and then run `snd_restart`.
- `s_backendActive` tells you what actually initialized, not just what you asked for.
- If OpenAL fails to initialize, fnTech3 falls back to the legacy backend automatically instead of leaving you without sound.
- `s_info` is the quickest way to confirm the active backend and inspect the runtime state after a restart.

Example:

```cfg
seta s_backend "openal"
snd_restart
```

If you want the classic mixer instead:

```cfg
seta s_backend "legacy"
snd_restart
```

## Defaults, Migration, And Fallback

The modern audio path is designed to be an additive default rather than a new content requirement. Each game's existing assets, modules, demos, and sound commands keep working; the OpenAL backend changes how the client renders audio after the game has already chosen which sounds to play.

Fresh configs start from these intended defaults:

- `s_backend openal`: Start the OpenAL backend first.
- `s_alDevice ""`: Use the system default playback device.
- `s_alHrtf auto`: Let OpenAL Soft decide whether HRTF fits the current output.
- `s_alHrtfId ""`: Use the runtime's default HRTF.
- `s_alOutputMode auto`: Let OpenAL Soft choose the active output mode.
- `s_alDistanceModel inverse_clamped`: Use the modern OpenAL world-sound falloff.
- `s_alFrequency 48000` and `s_alRefresh 100`: Request a stable low-latency context profile.
- `s_alMonoSources 64` and `s_alStereoSources 8`: Request enough sources for a normal mix without assuming unlimited hardware.
- `s_alOutputLimiter 1`: Request OpenAL Soft output limiting when supported.
- `s_alSpatializeStereo 0`: Keep authored stereo samples on the stable direct path.
- `s_alReverb 1` and `s_alOcclusion 1`: Enable the environmental layer on capable OpenAL devices.
- `s_alReverbGain 1.0` and `s_alOcclusionStrength 1.0`: Use the neutral spatial tuning scale.
- `s_alAutoRecover 1`: Let the OpenAL backend attempt a live device reopen if the runtime reports a disconnect.

If you are upgrading from an older config, keep your existing `s_volume`, `s_musicVolume`, focus muting, and `s_doppler` choices. Then change OpenAL startup requests one at a time and run `snd_restart` after each latched change. `s_info` is the source of truth for the active result, because several OpenAL settings are requests that a device or runtime may decline.

Fallback is intentionally layered:

- Invalid text values for HRTF, output mode, or distance model are normalized to safe defaults and reported in the console.
- fnTech3 tries the requested OpenAL device first, or the system default when `s_alDevice` is blank.
- If a saved OpenAL device name cannot be opened, startup prints a warning and tries the system default before falling back to the legacy backend.
- If a modern OpenAL context cannot be created, the engine retries with simpler attributes before giving up on OpenAL.
- If an active OpenAL Soft device later disconnects and the runtime supports live reopen, fnTech3 attempts a conservative reconnect before asking you to use `snd_restart`.
- If OpenAL still cannot start, the sound system falls back to the legacy backend instead of leaving the client silent.
- To force the old path from the start, set `s_backend legacy` and run `snd_restart`.

When troubleshooting an upgrade, check `s_backendActive` and the requested/active lines in `s_info` before editing a large config. That usually tells you whether the issue is a latched setting waiting for restart, a runtime capability mismatch, a device selection problem, or an intentional fallback to the legacy backend.

## OpenAL Device Selection

When `s_backend` is set to `openal`, `s_alDevice` lets you choose which playback device OpenAL should open.

- Leave `s_alDevice` blank to use the system default device.
- Set `s_alDevice` to a device name if you want fnTech3 to target a specific headset, speakers, or virtual audio device.
- `s_alDevice` is latched, so you must run `snd_restart` after changing it.
- `s_info` prints both the requested device and the active device, which is useful when the system falls back to a different output.
- `s_alListDevices` prints the devices reported by OpenAL and marks the default, requested, and active device when those names are known.
- `s_alRecoverDevice` asks OpenAL Soft to reopen or reset the active playback device without rebuilding the whole sound backend. This is most useful after a USB headset, Bluetooth device, HDMI output, or default-device route disappears and returns.
- If a saved device name stops working after hardware or driver changes, set `s_alDevice ""`, run `snd_restart`, and then use `s_alListDevices` to pick a current name.

Example:

```cfg
seta s_backend "openal"
seta s_alDevice ""
snd_restart
```

## OpenAL Startup Requests

fnTech3 exposes modern OpenAL startup controls as latched request cvars. They describe what the engine should ask the OpenAL runtime for during initialization. The active result can differ when the device, driver, or OpenAL implementation does not support a request, so `s_info` prints requested and active values side by side.

Headphone and HRTF controls:

- `s_alHrtf auto`: Let OpenAL decide whether HRTF is appropriate for the current device and output.
- `s_alHrtf on`: Request HRTF rendering and report if the device denies or cannot provide it.
- `s_alHrtf off`: Request non-HRTF rendering.
- `s_alHrtfId`: Optional HRTF name or numeric index. Leave blank for the runtime default.
- `s_alOutputMode auto`: Let OpenAL choose the output mode.
- `s_alOutputMode headphones`: Request headphone/stereo-HRTF-oriented output where supported.
- `s_alOutputMode speakers`: Request ordinary speaker output where supported.
- `s_alOutputMode surround` or `5.1`: Request 5.1 surround output where supported.
- `s_alOutputMode quad`, `6.1`, or `7.1`: Request those speaker layouts where supported.

Distance and context hints:

- `s_alDistanceModel`: Defaults to `inverse_clamped`. Accepted names are `none`, `inverse`, `inverse_clamped`, `linear`, `linear_clamped`, `exponent`, and `exponent_clamped`. Some games give `linear_clamped` and `none` a retail-exact meaning; see [Each Game's Sound](#each-games-sound).
- `s_alFrequency`: Requested mix frequency in Hz. Default `48000`.
- `s_alSourceResampler`: Source conversion quality. Default `balanced` selects
  the widest advertised sinc filter up to 24 taps; `fast` selects the smallest,
  and `best` selects the largest. `library` retains OpenAL's configured default.
  Requires `snd_restart`. Missing or rejected optional support retains the
  library default, which `s_info` reports.
- `s_alRefresh`: Requested context refresh rate in Hz. Default `100`.
- `s_alMonoSources`: Requested mono/3D source count. Default `64`.
- `s_alStereoSources`: Requested stereo source count. Default `8`.
- `s_alOutputLimiter`: Request OpenAL Soft output limiting. Default `1`.
- `s_alSpatializeStereo`: Optional two-channel world-sample positional routing when `AL_SOFT_source_spatialize` is available. Default `0`; stereo UI, announcer, music, raw audio, and surround samples remain direct.

These settings are intentionally requests, not guarantees. For example, an OpenAL runtime may use a different source count or disable HRTF for a device format it cannot render with HRTF. If a requested OpenAL context cannot be created, fnTech3 retries with safer context attributes before falling back to the legacy backend. Run `s_info` after `snd_restart` to see the actual state.

### OpenAL Latency Diagnostics

`s_alFrequency` and `s_alRefresh` are fnTech3's standard OpenAL context hints for mix rate and refresh cadence. The defaults, `48000` and `100`, are intended to be a stable low-latency starting point rather than an aggressive tuning profile.

Run `s_info` after `snd_restart` to compare requested and active context values. On OpenAL Soft devices that expose `ALC_SOFT_device_clock`, `s_info` also reports a live device-clock snapshot and output latency in milliseconds. If latency is unavailable, fnTech3 reports that explicitly instead of guessing.

For OpenAL Soft installations, deeper latency tuning is usually done in the OpenAL Soft config rather than through game cvars. Smaller `period_size` or `periods` values can reduce latency, but they can also cause crackle on unstable devices, so change them gradually. Run `s_alConfigHints` to print the relevant config-file locations, useful option names, and the live OpenAL capabilities fnTech3 detected.

Example OpenAL Soft test profile:

```ini
[general]
frequency = 48000
period_size = 256
periods = 3
output-limiter = true
```

OpenAL Soft options worth knowing about include `stereo-mode`, `stereo-encoding`, `hrtf-mode`, `hrtf-size`, `default-hrtf`, `resampler`, `channels`, `sample-type`, and decoder options such as `hq-mode`, `distance-comp`, `nfc`, and `speaker-dist`. fnTech3 does not need to own every one of those settings; it reports the active HRTF/output/limiter/device state and leaves advanced library-global policy in the OpenAL Soft config.

Static sounds and pitch-shifted voices use OpenAL's per-source converter.
`s_alSourceResampler balanced` requests band-limited conversion for this path
when `AL_SOFT_source_resampler` is available. Higher quality takes more CPU,
particularly with many active voices. Use `s_alSourceResampler library` to let
the OpenAL Soft `resampler` config option choose that filter.

## Volume, Music, And Focus Muting

The main day-to-day controls are intentionally simple.

- `s_volume`: Master volume for game audio. Range `0` to `2`. Default `0.8`, or `1.0` in Alice and F.A.K.K.2, as their original engine shipped.
- `s_musicVolume`: Music-only volume. Range `0` to `2`. Default `0.25`, or `0.60` in Alice and F.A.K.K.2.
- `s_voiceVolume`: Received Steam voice-chat volume. Range `0` to `2`. Default `1.0`. Only Quake Live has Steam voice.
- `s_muteWhenUnfocused`: Mute audio when the window is no longer focused. Default `1`.
- `s_muteWhenMinimized`: Mute audio when the game is minimized. Default `1`.
- On Windows, a client whose window is on a desktop other than the one on screen, such as a test probe's private desktop, is always silent (`s_info` reports `offscreen-muted`). It still mixes; only the output is muted: OpenAL through the listener gain, so source gains are unchanged, and the legacy backend into its secondary buffer. While the session is locked, no desktop is on screen and a client keeps the state it last had; one started during a lock counts as on screen only on the normal desktop, so a probe started then on its private desktop is silent from the start.

Practical guidance:

- Lower `s_musicVolume` first if the soundtrack is stepping on gameplay clarity.
- Use `s_volume` when the whole mix is too loud or too quiet.
- If the game seems to "lose" audio after task switching, check the two focus-muting cvars before assuming the backend is broken.
- Focus muting applies to both backends.
- In Quake Live, remote speakers use five independent retail-style queues, so
  simultaneous speakers do not overwrite one another or the raw PCM stream used
  by cinematics. `s_voiceStep` controls the small packet prebuffer and is
  normally best left at its `0.02` default.

Examples:

```cfg
seta s_volume "0.65"
seta s_musicVolume "0.15"
```

Keep audio playing while alt-tabbed:

```cfg
seta s_muteWhenUnfocused "0"
seta s_muteWhenMinimized "0"
```

## Spatial Audio

Spatial audio lives on the OpenAL backend. It is enabled by default, but the individual features remain adjustable so you can tune the mix instead of treating it as all-or-nothing.

Mono world sounds are submitted to OpenAL as true positional sources, using the current listener position and orientation so HRTF-capable devices can render head turns and source movement naturally. These sources also use the selected OpenAL distance model, with each game's own distances (see [Each Game's Sound](#each-games-sound)). Local sounds, announcer/UI sounds, music, raw samples, and authored multi-channel assets stay on the non-spatial path to avoid shifting centered or speaker-authored content. When OpenAL exposes both `AL_SOFT_direct_channels` and `AL_SOFT_direct_channels_remix`, stereo and surround samples plus music/raw streams request direct routing with unmatched speakers remixed into the output layout. Older implementations retain normal mixing so missing output speakers do not silently discard authored channels; their virtualization and gain can differ.

`s_alSpatializeStereo 1` is an opt-in diagnostic/compatibility escape hatch for mods with two-channel world samples that were authored expecting positional movement. It only applies to two-channel world samples on OpenAL runtimes with `AL_SOFT_source_spatialize`; UI, announcer, music, raw audio, and authored surround layouts remain direct. Leave it at `0` for retail-style content.

Use `s_alDistanceModel inverse_clamped` for the default modern falloff. `linear_clamped` is closest to the old Quake-style full-volume/max-distance shape, while `none` disables OpenAL distance attenuation and leaves the compatibility gain path in place.

### Each Game's Sound

Every game's sounds go through the same OpenAL pipeline -- positional sources, the distance model, traced occlusion, zone or traced room reverb, tone classes and HRTF -- but each game supplies its own data to it. The legacy mixer's comparison behavior and omissions differ by profile.

| Game | How OpenAL places its sounds | Comparison option and fallback limits |
| --- | --- | --- |
| Quake III Arena, Quake Live, Elite Force Holomatch | Quake III's law: full volume to 80 units, silent by 1,330. | `s_backend legacy` |
| Return to Castle Wolfenstein, Enemy Territory | Retail's straight-line law, full volume to 6.4% of a sound's range and silent at its end, carried by the distance model with a fade toward the horizon. One-shots span 80 to 1,330 units; loops use the range the game gives them. | `s_alDistanceModel linear_clamped` plays retail's law exactly and `none` plays its gain without its pan; `s_backend legacy` keeps retail's gain and pan. |
| Elite Force single player | Retail's span for each channel: most sounds full volume to 256 units and silent by 1,506, a voice 1,024 to 2,274, an attenuated voice 38.4 to 288.4. Your own sounds play at three quarters. Ambient sets become loops placed at their own points, and the room tone plays at the listener. | `s_alDistanceModel linear_clamped` plays retail's law exactly; `s_backend legacy` keeps retail's gain and pan. |
| Alice, F.A.K.K.2 | Each sound's own minimum distance is the reference distance, and the far end is 64 times that, under the default inverse model. That this matches the original engine is an inference. | `s_backend legacy` keeps retail's gain and obstruction; its left-right spread is even, because the original pan law was never read. |
| Medal of Honor: Allied Assault | Sound aliases supply volume, minimum and maximum distances, and pitch to the OpenAL pipeline. An entity's one-shot replaces its previous sound on the same channel except on the automatic channel; the game's stop request ends it. | No retail-exact mode is established. The legacy fallback ignores alias distances and pitch, loop volume, and the game's entity-channel replacement and stop behavior. |

MOHAA's server-driven `fadesound` commands are currently unhandled, including
on the OpenAL path. Campaign audio-fade behavior remains an open requirement;
see the [October 2 warning audit](fntech3/2026-10-02-ubertools-round.md).

Game-specific settings and commands:

- **RTCW:** `s_defaultsound 1` plays RTCW's placeholder sound for a missing sample instead of silence (requires `snd_restart`). Single player keeps retail's speech amplitude for lip movement, its music controller (see [Codecs And Music](#codecs-and-music)) and its entity streams: `streamingsound` and `stopstreamingsound` start and stop a stream. `s_volume` and the master fade are quantized as retail quantizes them. Loops carry no Doppler.
- **Enemy Territory:** each one-shot's own volume, loop start times, stop-after-fade and ET's master fade are kept. Loop Doppler and ET's "no attenuation" flag are not carried yet.
- **Elite Force:** `s_separation` (default `0.5`) is the original engine's channel spread. On OpenAL, values above the default spread placed voices through OpenAL Soft's source radius; on the legacy mixer, and in Holomatch, it sets the pan as retail did. `s_language DEUTSCH` plays the German voices where the installation has them. Single player adds lip-sync, tuned by `s_threshold1` to `s_threshold4`, the loudness steps at which a speaker's mouth opens wider.
- **Enemy Territory:** the player aliases which its module returns as sound handle zero, including jumping, stay silent on OpenAL and the legacy mixer. `s_defaultsound` defaults to 0; setting it to 1 enables the diagnostic placeholder ([input and launch fixes](fntech3/2026-10-02-input-launch-fixes.md)). Spatial sounds continue through the shared distance, occlusion, EFX and zone systems.
- **Alice and F.A.K.K.2:** `s_separation` (default `0.3`) spreads only the two-dimensional dialog voices. `s_reverb 1` (the default) lets a room the game names -- a map's reverb trigger -- replace the zone or traced room; no shipped map names one. `s_show_sounds 1` prints every sound as it starts and stops. Stereo WAV files are refused, as the original engine refused them. The commands are the original engine's: `play`, `sounddump`, `playsong`, `loadsoundtrack`, `playmp3` and `stopmp3`; there is no `music` or `stopmusic`.

### HRTF And Output Mode

`s_alHrtf`, `s_alHrtfId`, and `s_alOutputMode` control the HRTF/output mode requested from OpenAL Soft at startup. Use `auto` unless you are deliberately testing a headset, speaker path, or specific HRTF.

Important details:

- HRTF availability depends on the OpenAL runtime and the active output device.
- Surround playback is optional and capability-gated. Authored quad/5.1/6.1/7.1 PCM is submitted natively when `AL_EXT_MCFORMATS` is available; otherwise it is mixed down to stereo so the asset still plays.
- Advanced immersive assets are opt-in by filename tag. WAV files tagged with `uhj`, `uhj2`, `uhj3`, or `uhj4` use OpenAL Soft UHJ formats when `AL_SOFT_UHJ` is available. Files tagged with `bformat2d`, `bformat3d`, or `ambisonic` use first-order B-Format when `AL_EXT_BFORMAT` is available. Untagged files keep the ordinary PCM interpretation.
- If UHJ or B-Format support is unavailable, fnTech3 falls back to a safe stereo render path instead of failing the sound. Two-channel UHJ remains stereo-compatible; B-Format uses its omni channel for fallback.
- `s_info` reports the requested HRTF mode, active HRTF status, output-mode request, and active output mode when the runtime exposes that information.
- `s_alListHrtfs` prints the HRTF names/indices exposed by OpenAL Soft. If OpenAL is active, it uses the live device; otherwise it opens the requested/default device temporarily for diagnostics.
- If HRTF sounds worse on a particular headset or listener, use `s_alHrtf off` and run `snd_restart`.

### Reverb

- `s_alReverb 1`: Enable environmental reverb sends on OpenAL devices that expose EFX support.
- `s_alReverb 0`: Disable the OpenAL reverb path.
- `s_alReverbGain`: Scale the wet reverb level from `0` to `2`. Default `1.0`.

Important details:

- Reverb requires an OpenAL device with EFX support.
- `s_alReverb` is latched and requires `snd_restart` to apply cleanly.
- `s_info` reports whether EFX support is available and whether the reverb send is active on the current device.
- fnTech3 smooths environment changes instead of hard-switching the active EFX preset. The debug overlay and `s_alDebugDump` show the active-to-target environment, transition blend, wet send, and direct/wet high-frequency values.
- The room comes from the map's audio zone where one is loaded, and otherwise from traced listener probes. In Alice and F.A.K.K.2, a room the game names takes precedence while `s_reverb` is on.

### Occlusion

- `s_alOcclusion 1`: Enable world-geometry occlusion checks.
- `s_alOcclusion 0`: Disable occlusion.
- `s_alOcclusionStrength`: Scale how strongly occluded sounds are muffled. Range `0` to `2`. Default `1.0`.

Occlusion is useful when you want walls, doors, and level structure to affect how remote sounds read. fnTech3 smooths occlusion changes over time and applies separate direct-path attenuation and tone filtering, so moving behind a wall should sound like a transition rather than a hard step. The guaranteed dry-path gain change is intentionally audible even on devices without EFX filters; EFX-capable devices add the stronger low-pass/band-pass tone shift on top. If the result feels too dull, reducing `s_alOcclusionStrength` is usually a better first move than disabling the feature entirely.

Weapon impacts and explosions are emitted flush against the surface they hit, so the occlusion test samples the direct path from just clear of that surface rather than from the raw event origin, and it discards probes that land inside world geometry instead of counting them as obstructions. A detonation you can see is therefore measured against what is actually between you and it, not against the wall, floor, or corner it went off on. A sound genuinely emitted inside geometry still reads as occluded.

`s_alAirAbsorption` (default `2.0`, EFX only) darkens distant positional sounds: `0` disables it, `1` is physically neutral, and higher values darken distant sounds more strongly.

### Audio Visibility And Culling

fnTech3 plays the transient and looping sounds requested by the client module, then applies source budgets, distance attenuation, and optional trace-based occlusion. `s_pvs 1`, from retail Quake Live, additionally mutes positional sounds whose origins are outside the listener's current BSP PVS. It is off by default, available in every game on the OpenAL backend, and fails open when no valid collision world is loaded; it does not affect local/UI audio, music, raw/cinematic PCM, or remote voice chat. The legacy mixer applies it everywhere except in RTCW, Alice and F.A.K.K.2.

- Snapshot visibility is handled before audio sees most world entities. The server builds snapshots from `CM_ClusterPVS`, area-portal connectivity, broadcast/single-client flags, and the portal-style second-view PVS paths (`SVF_PORTAL` and `SVF_SELF_PORTAL2`).
- There is no engine-wide `CM_ClusterPHS` sound visibility path in this tree. The only PHS reference is in bot AAS helpers, not in client sound dispatch.
- One-shot sounds are limited by duplicate suppression, per-entity concurrency caps, and voice-source eviction. Looping sounds must be refreshed by the client game each frame; unrefreshed transient loops are stopped.
- Once a voice reaches OpenAL, audibility is mainly distance gain and source availability. Occlusion is a separate `CM_BoxTrace` test against solid/slime/lava contents between listener and source, with a small source-side probe fan so narrow or edge obstructions become partial occlusion instead of a binary on/off gate.

Servers can opt into a compatibility-preserving audio visibility expansion without changing the protocol or relaxing visual PVS:

- `sv_audioPVS 0`: Disabled. This is the default and preserves retail snapshot behavior.
- `sv_audioPVS 1`: Add nearby sound-only emitters, such as speaker entities, audio-only general sound events, and non-visual `loopSound` entities, when they are outside visual PVS but still area-connected.
- `sv_audioPVS 2`: Also add nearby non-visually culled entities that carry `loopSound`. This can make more gameplay sounds audible through walls, but it intentionally remains an explicit server choice.
- `sv_audioPVSRange`: Maximum expansion distance in game units. Default `1024`.
- `sv_audioPVSMaxEntities`: Maximum extra sound emitters per client snapshot. Default `16`.

The expansion is deliberately server-side, capped, and off by default. It sends ordinary entity records that existing clients already understand, then the client-side occlusion path handles the muffling. Every game's server has it except Alice's and F.A.K.K.2's. It recognizes emitters by Quake III's entity and event types, so outside Quake III Arena and Quake Live its choice of entities has not been checked.

### Tone Shaping

fnTech3 uses a small set of EFX filter presets for source classes rather than a full user-facing EQ. The goal is clarity and plausibility with no custom sound assets:

- World sounds use low-pass shaping from the current environment and occlusion state.
- Heavily occluded and underwater sounds can use band-pass shaping so blocked sounds lose some low-end weight as well as high-frequency detail.
- Local UI, chat, and announcer-style sounds can use light high-pass shaping so they stay separate from the world mix.
- Sounds emitted by your own player, such as a held weapon's fire loop, keep that same dry close-up character but are still in the level with you: when you go under water they are muffled along with everything else. Only announcer, UI, and feedback audio is exempt from the environment.
- Each game's channels choose the class: a voice channel is shaped as a voice, a local channel as local audio.
- Stereo samples, music, and raw audio keep their authored direct path by default; they are not spatialized or re-EQed for HRTF. A two-channel world sample that opts into positional playback with `s_alSpatializeStereo 1` is treated as a world source and shaped like one.

### Audio Zone Sidecars

An audio zone sidecar, `maps/<mapname>.azb`, divides a map into room-scale volumes, each with its own reverb, tone, occlusion multiplier and transition behavior. Sidecars are never required for normal gameplay: if a sidecar is missing, invalid, disabled, or does not contain the listener's current position, the OpenAL backend keeps using the generic trace-based environment heuristics described above.

fnTech3's engine package ships a geometry-derived sidecar for every stock map of seven games, each under its own game's namespace:

| Game | Sidecars | Package folder |
| --- | --- | --- |
| Quake III Arena | 35, from FnQuake3 | `games/quake3/baseq3/maps/` |
| Quake Live | one for each of the 149 BSP maps in the retail Steam package | `games/quakelive/baseq3/maps/` |
| Return to Castle Wolfenstein | 32 single-player maps, including cinematics and the ending, and 15 multiplayer maps | `games/rtcw-sp/main/maps/`, `games/rtcw-mp/main/maps/` |
| Enemy Territory | the six stock maps | `games/wolfet/etmain/maps/` |
| American McGee's Alice | all 36 maps | `games/alice/base/maps/` |
| Heavy Metal: F.A.K.K.2 | all 30 maps | `games/fakk2/fakk/maps/` |
| Medal of Honor: Allied Assault | all 54 maps, each beside its map in the pak's own folder | `games/mohaa/main/maps/` |

These are generated acoustic estimates, pending listening checks; the generation records, such as the [RTCW record](fntech3/2026-09-17-rtcw-audio-zones.md), list their audit warnings. Elite Force and the Jedi games ship no sidecars. Menu-only optional or holiday Workshop entries whose BSPs are not installed, other Workshop maps, and custom maps use the heuristic fallback unless their own sidecar is supplied.

- `s_alAudioZones 1`: Enable `.azb` sidecars when present. This is the default.
- `s_alAudioZones 0`: Ignore sidecars and use generic environment heuristics only.
- `s_info`: Reports whether audio zones are disabled, missing, loaded, and which zone is currently active.
- `s_alDebugOverlay 2` and `s_alDebugDump`: Include active zone name, material metadata, portal blend target when one is active, plus the zone-adjusted wet, low/high-frequency, occlusion, and transition values.
- fnTech3 checks `fntech3-pkg.fnz` next to the executable before normal game data, using `games/<profile>/<gamedir>/maps/<mapname>.azb`; for example, `games/quakelive/baseq3/maps/campgrounds.azb` is available only to Quake Live. The active profile's mod directories precede its base directory. No game ever reads another game's sidecars.
- Only the OpenAL backend reads sidecars; the legacy mixer never does.

Sidecars are compiled with the repo tool target `fnql-audiozonesc`. Maintainers can write `maps/<mapname>.audiozones` by hand or generate a first-pass sidecar from an existing `.bsp` -- Quake III's and Quake Live's, RTCW's and ET's, Ritual's FAKK maps, or Medal of Honor's 2015 maps -- then layer small overrides on top. Current sidecars can carry material metadata, portal hints, and optional per-portal blend tuning while older version 1 and version 2 sidecars remain readable. The compiler workflow, authoring syntax and each game's generation script are documented in [`code/tools/audiozones/README.md`](../code/tools/audiozones/README.md).

Generated zones are built by merging BSP leaves back into room-scale volumes along their real open contact area, so a zone corresponds to a place in the map rather than to whichever leaves happened to share a bounding box. Environment presets follow the merged volume's size and shape, `underwater` is only set for volumes actually inside a liquid brush, `outdoor` follows how much sky the volume can see, and portal hints and their blend tuning are derived from the size and openness of the real connection between two zones.

### Weapon Sound Shaders

The OpenAL backend also reads a small fnTech3 sound shader file, `sound/fnql-weapon-sounds.sndshd`, from `fntech3-pkg.fnz`. The format intentionally follows the idTech4/Quake 4 declaration style: `sound <name> { minDistance ... maxDistance ... volumeDb ... shakes ... sample }`. Its tuning is imported from FnQuake3, preserving the same attack, distance, pitch, shake, and reverb treatment without replacing the retail samples.

Like audio zones, the package stores it under `games/<profile>/<gamedir>/sound/fnql-weapon-sounds.sndshd`. Quake III Arena (with a separate copy for Team Arena's `missionpack`) and Quake Live ship one; the other games do not. A mod can supply an entry in its own game's namespace when it needs different tuning. If one is supplied for RTCW or Enemy Territory, their sounds take its gain, pitch and wet-send tuning but keep their own distances; Elite Force single player and the Ritual games do not apply it. See the [package layout](fntech3/PACKAGES.md).

### Doppler

- `s_doppler 1`: Enable doppler shift on moving projectiles.
- `s_doppler 0`: Disable doppler shift.
- `s_alDopplerFactor`: Scales the OpenAL doppler effect from source and listener motion, from `0` to `10`. Default `1.0`.
- `s_alDopplerSpeed`: Speed of sound in world units per second for OpenAL doppler, from `1000` to `20000`. Default `6000`; lower values exaggerate pitch shifts, and about `13500` matches real-world acoustics at Quake III scale.

`s_doppler` affects both backends, but it matters most when the OpenAL path is doing the rest of the spatial work.

### Recommended OpenAL Starting Point

This is a solid default if you want fnTech3's modern audio path without pushing the mix into something exaggerated:

```cfg
seta s_backend "openal"
seta s_alDevice ""
seta s_alHrtf "auto"
seta s_alOutputMode "auto"
seta s_alDistanceModel "inverse_clamped"
seta s_alFrequency "48000"
seta s_alRefresh "100"
seta s_alMonoSources "64"
seta s_alStereoSources "8"
seta s_alOutputLimiter "1"
seta s_alSpatializeStereo "0"
seta s_volume "0.8"
seta s_musicVolume "0.25"
seta s_doppler "1"
seta s_alReverb "1"
seta s_alReverbGain "1.0"
seta s_alOcclusion "1"
seta s_alOcclusionStrength "1.0"
seta s_alAudioZones "1"
snd_restart
```

## Codecs And Music

Sound formats:

- **WAV** in every game, tried first when a sound name has no extension.
- **Ogg Vorbis** in every game, when built with `-Dogg-vorbis=true` (the default).
- **MP3** for the games whose data is MP3: Elite Force, whose MP3s carry Raven's size and lip-sync tags, and Alice and F.A.K.K.2, whose songs are decoded in memory with their own start and loop points. It is built by default with `-Dmp3=true`.

OpenAL's shared stream converter keeps resampling continuous across decoded chunks.
Upsampling and downsampling use a band-limited filter to suppress unwanted images
and aliasing; equal-rate PCM stays unchanged. Ritual tracks with non-native rates
use the same filter before their 44.1 kHz score mix.
Streamed volume changes preserve the resampler's timing and interpolation history.
Raw-stream idle drains also retain phase across short bursts; explicit clearing
or a format change starts a fresh stream.
Music drains its buffered ending when a file finishes, and retries a decoded
chunk when the stream buffer pool is temporarily full. Explicit stop commands
still cancel playback immediately. The [audio system audit](fntech3/2026-10-02-audio-system-audit.md)
records the playback, quality and allocation checks.
Alice and F.A.K.K.2's score mixer also drains its last decoded frame, including
when the tail spans more than one mixing update.

Music follows each game's own engine:

- **Quake III Arena, Quake Live, RTCW multiplayer and Elite Force** use `music <intro> [loop]` and `stopmusic`. In RTCW a one-argument `music` loops the same file.
- **Quake Live**'s WebUI owns `music/fla_mp05` while the main menu is visible. Entering a match stops that menu-owned track so game and map music retain normal ownership; returning to the WebUI starts the menu track again.
- **RTCW single player** keeps its original music controller: fades, queued tracks (`queuemusic`) and the `s_currentMusic` readout. Enemy Territory's music start uses the same controller.
- **Alice and F.A.K.K.2** play the game's musical moods through two crossfading song players, and a cinematic's `video/<name>.mp3` soundtrack, mixed into one stream. `s_musicVolume` scales the songs; a cinematic's soundtrack plays at full scale. Alice's menu soundtrack is not played yet.

## Legacy Backend Tuning

The legacy mixer remains available when you want the original software path or when OpenAL is not appropriate for a given machine. It keeps the implemented profile-specific gain and pan rules, has no distance model, occlusion, reverb or audio zones, and ignores weapon sound shaders. Its profile-specific comparison limits are listed in [Each Game's Sound](#each-games-sound).

For Medal of Honor: Allied Assault, legacy one-shots keep their requested volume, but the fallback ignores alias distances and pitch, loop volume, and the game's entity-channel replacement and stop requests. Keep OpenAL active when those sound contracts are needed; an automatic fallback to legacy carries the same limits. See the [Medal of Honor audio evidence](fntech3/2026-09-24-mohaa-contracts.md#client).

The main legacy-specific controls are:

- `s_khz`: Output sampling rate for the legacy backend. Valid values are `8`, `11`, `22`, `44`, and `48`. Default `22`. Requires `snd_restart`.
- `s_mixAhead`: Amount of audio to pre-mix ahead of playback. Default `0.2`.
- `s_mixPreStep`: Retail Quake Live-compatible DMA cursor lead. Default `0.05`.
- `s_mixOffset`: Developer-facing timing offset for the legacy mixer. Range `0` to `0.5`.
- `s_device`: ALSA output device selector on Linux builds that use the non-SDL ALSA path.

Practical guidance:

- Higher `s_khz` values can improve fidelity, but they are not automatically the best choice on every machine.
- `s_mixAhead` is a stability-vs-latency control. Higher values can help with crackle or starvation on unstable systems, but they also push the mixer further ahead.
- `s_mixOffset` is an advanced knob and is usually best left at `0` unless you are deliberately testing mixer timing behavior.

Example legacy profile:

```cfg
seta s_backend "legacy"
seta s_khz "44"
seta s_mixAhead "0.2"
snd_restart
```

## Useful Commands

fnTech3 keeps each game's classic sound commands and adds a few OpenAL-oriented inspection tools.

- `s_info`: Print the current backend, device, EFX support, reverb state, occlusion state, source counts, audio zones, weapon sound shader rule count, sample counts, and background-track state.
- On OpenAL, `s_info` also compares requested and active HRTF, output mode, distance model, mix frequency, refresh rate, source counts, output limiter state, and the stereo-spatialization request. It reports device clock and latency when OpenAL Soft exposes that timing extension, and whether the device offers a source radius.
- `s_alListDevices`: List OpenAL playback devices. This can still be useful if the legacy backend is active and you are preparing an OpenAL device setting.
- `s_alListHrtfs`: List HRTFs for the active OpenAL device, or for the requested/default OpenAL device when OpenAL is not active.
- `s_list`: List registered samples and whether each one is loaded, unloaded, missing, or using the generated fallback tone. Loaded OpenAL samples also show channel layout, sample rate, and duration.
- `s_stop`: Stop active sounds.
- `play <soundfile>`: Play one or more sound files locally for a quick spot check.
- `music <intro> [loop]`: Start background music playback, in the games that have it.
- `stopmusic`: Stop the current background track.
- `snd_restart`: Restart the whole sound system after backend or latched setting changes.
- `s_alDebugDump`: Print a spatial-audio debug snapshot on the OpenAL backend.

The game-specific commands are listed under [Each Game's Sound](#each-games-sound) and [Codecs And Music](#codecs-and-music).

Examples:

```cfg
play sound/player/jump1.wav
```

```cfg
music music/fla22k_02.wav
```

## Spatial Debug Tools

If you are tuning the OpenAL path and want more than a yes-or-no answer, fnTech3 exposes both console and overlay debug tools.

- `s_alDebugDump`: Prints a spatial snapshot for the current environment transition and inspected voice, and for each voice its routing mode, tone class, occlusion, and dry and wet gains. When `AL_SOFT_source_latency` is available, the selected source also reports its playback offset and latency.
- `s_alDebugOverlay 0`: Disable the OpenAL debug overlay.
- `s_alDebugOverlay 1`: Show summary environment transition and selected voice state.
- `s_alDebugOverlay 2`: Add environment wet/filter values, active audio-zone information, plus selected-voice sample, gain, and tone-filter information.
- `s_alDebugVoice -1`: Automatically inspect the nearest active voice.
- `s_alDebugVoice <entityNum>`: Lock the debug tools to a specific entity when you want to inspect a known source.
- `s_alSourceClassDebug 1`: In developer mode, add source-class aggregate routing and gain summaries to `s_alDebugDump`.

These tools are only meaningful on the OpenAL backend.

## Troubleshooting

### There is no sound at all

- Open the console and run `s_info`. It reports the configured backend,
  active backend, master volume, and focus/registration mute state; when no
  backend started, it says so explicitly. A registration mute in the initial
  startup report is expected and should become `audible` once registration
  finishes.
- Confirm `s_initsound` is `1` and `s_volume` is above `0`. Then run
  `snd_restart`.
- Leave `s_alDevice ""` while diagnosing. A saved headset or HDMI device name
  can become stale after hardware or driver changes.
- If OpenAL is unavailable, fnTech3 automatically tries the legacy platform
  backend. SDL builds use the SDL3 default output route; native Windows builds
  try WASAPI and then DirectSound. The console records each failed stage
  instead of reporting only a generic initialization failure.
- To bypass OpenAL deliberately, set `s_backend legacy`, run `snd_restart`,
  then run `s_info` again.
- If both backends fail, attach the `------ Initializing Sound ------` section
  from the console log. It contains the runtime/library, device, format, and
  fallback failure that is needed for diagnosis.

### I changed a setting and nothing happened

Some audio cvars are latched. If you change any of the following, run `snd_restart` before judging the result:

- `s_backend`
- `s_alDevice`
- `s_alHrtf`
- `s_alHrtfId`
- `s_alOutputMode`
- `s_alDistanceModel`
- `s_alFrequency`
- `s_alSourceResampler`
- `s_alRefresh`
- `s_alMonoSources`
- `s_alStereoSources`
- `s_alOutputLimiter`
- `s_alSpatializeStereo`
- `s_alReverb`
- `s_khz`
- `s_defaultsound` (RTCW)

`s_alAudioZones` is not latched. Toggle it and use `s_info` or the spatial debug overlay to confirm whether the active map's sidecar is loaded or ignored. `s_language` applies to sounds loaded after it changes.

### Spatial audio sounds flat

Check the basics in this order:

- Run `s_info` and confirm `s_backendActive` is `openal`.
- Compare the requested and active HRTF/output/distance lines in `s_info`.
- If you expect reverb, confirm that `s_info` reports `EFX support: enabled`.
- Confirm `s_alReverb 1` and `s_alOcclusion 1`.
- If the mix is too subtle, try increasing `s_alReverbGain` or `s_alOcclusionStrength` in small steps instead of jumping straight to extremes.

### The wrong output device started

- Run `s_alListDevices` and copy the device name exactly as OpenAL reports it.
- Set `s_alDevice` explicitly.
- Run `snd_restart`.
- Run `s_info` again and compare the requested device against the active device.
- If the requested device is no longer listed or startup warns that it could not be opened, set `s_alDevice ""` to return to the system default.

### Audio does not return after unplugging a device

- Run `s_info` and check the `Device state` line.
- Leave `s_alAutoRecover 1` enabled if your OpenAL Soft runtime supports live reopen.
- Run `s_alRecoverDevice` after reconnecting the headset, HDMI output, or Bluetooth device.
- Use `s_alRecoverDevice force` if the device reports connected but you want OpenAL Soft to reopen it anyway.
- If live reopen is unavailable or fails, run `snd_restart`; if the device name changed, set `s_alDevice ""` first.

### HRTF stays disabled or sounds wrong

- Run `s_info` and compare the requested and active HRTF/output lines.
- Run `s_alListHrtfs` to see whether the active or requested OpenAL device exposes any HRTF specifiers.
- If you are testing headphones, try `s_alOutputMode headphones`, leave `s_alHrtfId ""`, and run `snd_restart`.
- If HRTF still reports disabled, the active runtime or output path probably declined the request. Use `s_alHrtf auto` or `s_alHrtf off` for that device.
- If front/back cues feel worse on your setup, `s_alHrtf off` is a valid preference rather than a failure.

### `s_info` shows different active values than I requested

That usually means OpenAL accepted the context but adjusted one or more requested attributes. This is normal for HRTF, output mode, source counts, output limiting, and timing hints.

- Prefer `auto` for HRTF and output mode unless you are testing a specific device.
- Leave `s_alHrtfId` blank if a named or numeric HRTF no longer appears in `s_alListHrtfs`.
- Use the default `s_alMonoSources 64` and `s_alStereoSources 8` unless you are diagnosing source starvation.
- Treat `s_alFrequency` and `s_alRefresh` as hints; use OpenAL Soft config files for deeper latency tuning.

### OpenAL keeps falling back to legacy

- Run `s_info` and confirm whether `s_backendActive` is `legacy`.
- Set `s_alDevice ""`, `s_alHrtf auto`, `s_alHrtfId ""`, and `s_alOutputMode auto`, then run `snd_restart`.
- Check that the packaged OpenAL runtime is present on Windows builds, or that a system OpenAL runtime is available on non-Windows builds.
- If the machine still cannot start OpenAL cleanly, keep `s_backend legacy` for that install.

### A map sounds different than expected

- Run `s_info` and check the `Audio zones` line.
- If a sidecar is loaded, use `s_alDebugOverlay 2` or `s_alDebugDump` to see the active zone name, material, portal blend target, and tuned environment values.
- Set `s_alAudioZones 0` to compare the same location against the generic environment heuristics.
- Invalid `.azb` files are ignored with a console warning; missing files are harmless.
- To compare the supported classic gain and pan behavior, use the option and check the limits for that profile in [Each Game's Sound](#each-games-sound).

### The game goes silent after alt-tabbing

That may be intentional rather than a failure:

- `s_muteWhenUnfocused 1` mutes audio when the window is not focused.
- `s_muteWhenMinimized 1` mutes audio when the game is minimized.
- `offscreen-muted` in `s_info` means the window is on a desktop that is not on screen; neither cvar changes that.

If you want audio to continue while the game is in the background, disable one or both.

### I want the safest fallback path

Use the legacy backend:

```cfg
seta s_backend "legacy"
snd_restart
```

## Recommended Starting Points

### Modern Default

Use this if you want fnTech3's intended current audio path:

- `s_backend openal`
- `s_alDevice ""`
- `s_alHrtf auto`
- `s_alOutputMode auto`
- `s_alDistanceModel inverse_clamped`
- `s_alSpatializeStereo 0`
- `s_alReverb 1`
- `s_alOcclusion 1`
- `s_volume 0.8`
- `s_musicVolume 0.25`

### Competitive And Dry

Use this if you want less ambience and more direct positional cues:

- `s_backend openal`
- `s_alReverb 0`
- `s_alOcclusion 1`
- `s_alOcclusionStrength 0.5`
- `s_musicVolume 0`

### Conservative Compatibility

Use this if you need the original software mixer path. Check the profile's limits in [Each Game's Sound](#each-games-sound) before using it for a retail comparison:

- `s_backend legacy`
- `s_khz 22`
- `s_mixAhead 0.2`

## Related Guides

- [Console Guide](CONSOLE.md) for command entry, completion, and console-side workflow.
- [Games Guide](GAMES.md) for which games run and where their files live.
- [Modern Audio Engine Notes](fnql/AUDIO_ENGINE.md) for architecture, source layout, compatibility boundaries, and validation expectations.
- [Architecture overview](fntech3/ARCHITECTURE.md) for how the audio adapters fit the rest of the engine.
