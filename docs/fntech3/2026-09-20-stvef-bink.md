# Elite Force single-player Bink movies

The Windows x86 SP client plays the installation's packed BIKi movies through
its own `binkw32.dll`. The DLL is loaded by an absolute installation path;
neither that runtime nor its headers or movie files are distributed. Other
platforms report the missing Windows x86 boundary explicitly. The SP profile
remains development-gated, as it is for the retail game and UI libraries.

## Observed contract

The following interface facts come from retail `stvoy.exe` 1.20 and the
installed Bink 1 runtime, identified in the evidence manifest. No external
implementation text is incorporated.

| Contract | Observation |
| --- | --- |
| Start and resolution | `stvoy.exe` `0x403cd0` resets `timescale` and `skippingCinematic`; `cl_VideoUsingLoRes` selects the `_lo` filename, with a retry of the other resolution if opening fails |
| In-game playback | `0x4041f0` queues a movie until the client is active, or starts it immediately; `0x40418d` retains the active connection and sets `cl_paused` |
| Stop and keys | `0x4035c0` ignores requested skips for 1,200 ms, closes the movie and clears the pause; `0x4070b1` accepts ASCII keys and mouse 1 when no input catcher owns them |
| German audio | `0x403e08` chooses the second track for `s_language DEUTSCH`; `0x4309f6` falls back to the first track when the second is absent |
| Frames | `0x43092a` copies a frame when `BinkDoFrame` returns zero; `0x430955` requests the 32-bit surface with flags `0x70000004` |
| Memory input | Installed `binkw32.dll` `0x10007f30` uses flag `0x04000000` for an in-memory file; the retail track-selection flag is `0x4000` |
| Low-resolution presentation | `binkw32.dll` `0x1000807d` honors header scaling flags before publishing width/height. Most `_lo` movies double their stored height; `intro_lo` retains 256 by 192 |

## Engine integration

The VFS reads a movie into an owned, bounded buffer. The BIKi validator checks
file length, dimensions, frame rate, track metadata, the complete seek table,
and audio packet lengths before lending that buffer to the installed runtime.
It does not validate proprietary compressed bitstreams; decoding still trusts
the user's native runtime, just as hosting the user's game libraries does.

The movie owns its native handle, pixels and input bytes. Partial loads, failed
first frames, normal completion, explicit stops, loop restarts, held frames and
module teardown release the appropriate resources. Waiting for the first
snapshot retains a queued in-game movie across client-module initialization;
disconnect and explicit stop cancel it. In-game completion unpauses the level
without disconnecting it. Standalone completion runs the existing `nextmap`
continuation. The ordinary RoQ path is retained for the other profiles.

The installed DirectSound factory supplies movie sound. Volume, silent playback
and the engine's unfocused/minimized mute preferences are applied before frame
decoding. Initialization failure is reported, with silent video still available.
The native-resolution image is uploaded by GLx, Vulkan and RTX; only the SP
profile may pass non-power-of-two cinematic dimensions to those renderers.
Invalid or oversized dimensions are rejected before upload for every profile.
Retail's obsolete `cl_VideoQuality` texture-reduction modes are not reproduced;
the engine uses the complete decoded frame, with `_lo` available separately.

## Evidence

`tests/stvef_bink_tests.py` executes the file validator and runtime boundary
against synthetic inputs and recording exports: malformed lengths and indexes,
scaling, track selection, missing exports, failed loads/opens, first-frame
failure, frame skipping, clock wrap, looping, volume and idempotent cleanup.
`stvef_cinematic_tests.py` executes VFS failure cleanup, pending startup,
pause/resume, key policy, hold/loop/error paths and the size checks in all three
renderers. The affected shutdown, RoQ, RTCW, input, key-name and SP cgame tests
also pass: 124 focused tests in total. No keyboard or mouse event was injected.

The full Windows x86 Meson run passed 120 targets with two platform skips;
its recursive Python target ran 1,830 tests with 72 conditional skips. The
final focused run also covers the subsequently added VFS cleanup and explicit
unsupported-platform tests and the resolution-fallback refinement.

The optional `tests/stvef_bink_native_probe.cpp` decoded every frame of all 41
readable installed movies: 10,754 frames in total, including their low variants.
This file probe disables frame pacing and sound; it is decoder evidence, not
audio or playback-timing evidence. ZIP validation rejects the 42nd file,
`video/st_21.bik`, because its installed archive entry has a bad CRC. Its `_lo`
variant decodes. The installation was left unchanged.

Windowed engine probes, with mouse and joystick disabled and no pointer grab,
establish the following:

- `st_01` plays for its 75 frames and runs its `nextmap` continuation. Engine
  captures of the movie and the returned menu are identical across GLx, Vulkan
  and RTX at 640 by 480.
- `st_04` selects track 1 for German and initializes Bink sound. This probe used
  volume zero; it does not establish audible language or mixing parity.
- A movie with only one track selects track 0 for German. Low-resolution
  selection, looping, holding and replacement complete without an error.
- `borg2` stays connected while a movie plays, reports `cl_paused 1` during it
  and `cl_paused 0` afterward, and returns to the gameplay view. A movie queued
  while the map loads also starts and finishes.
- Missing and malformed movies return cleanly. A synthetic damaged header
  shadowing either resolution falls back to the other installed resolution.

[The evidence manifest](evidence/2026-09-20-stvef-bink.json) records executable,
runtime, renderer and fixture identities, commands, selected tracks and capture
hashes. Logs and engine captures remain under `.tmp/stvef-gaps/` in the isolated
`stvef-support` checkout. No operating-system capture was used.

This closes the absent Bink playback path with fixture and live evidence. It
does not establish complete cinematic/audio parity, RDM5/lighting parity, or
Lilium protocol-26 compatibility. Those claims remain separate gates.
