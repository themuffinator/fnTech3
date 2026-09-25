# RTCW MP looping sounds and viewpoint — 2026-09-15

MP's two looping-sound imports now submit to a shared RTCW loop policy used by
the legacy and OpenAL backends. The original MP contract takes six arguments,
including range and volume, and ignores the entity number. A frame accepts up
to 128 submissions, including repeated entities and sound handles. It is not
the one-loop-per-entity behavior of Q3 or iortcw's revised backend.

Range zero selects 1,250 units; negative ranges are rejected. The stored range
uses the original binary32 representation. Volume is clamped to 0–255 and
applied with the original divisor of 256 after integer spatialization. The
underwater bit is recorded; original MP never reads that metadata in its mixer.
Zero-volume and full-frame submissions do not load sound handles. Both backends
require an active connection and preserve their disabled/muted behavior.

The legacy mixer consumes separate RTCW loop records and retains duplicate
channels. OpenAL uses submission slots, clears missing voices on the next
update, and virtualizes inaudible voices. Until 2026-09-24 its RTCW loops
used the shared linear range/gain calculation for their gain and pan. They
now use the engine's own features instead (see below). Newly allocated or
resumed sources
join a common sample cycle using the device clock or monotonic fallback.
This timing policy has bounded arithmetic tests; it does not establish original
PCM phase or acoustic parity through an output device.

The module adapter owns position/velocity copies, checks exact integer and
buffer extents, and rejects non-finite vectors before calling a backend.
SP loops remain explicitly unsupported until their clear modes, streaming
interactions and submission-time master-fade quantization are implemented.
The default RTCW release gate remains closed.

MP also supplies its interpolated viewpoint as three scalar float words.
The client now validates their integer encodings and updates all coordinates
atomically, retaining signed zero and clearing the position with map state.
The optional server-requested visible-client policy and its handshake remain
open; this import alone does not establish that policy.

## The engine's own features (2026-09-24)

Under invariant 11 of `AGENTS.md`, the OpenAL backend no longer pans an MP
loop as retail's mixer does. It renders MP's data through the engine's own
audio. The [Wolfenstein OpenAL voices
record](2026-09-24-wolfenstein-openal-voices.md) has the design, tests and
runtime evidence.

Kept from retail:

- the 128 submissions, each on its own voice;
- the range and volume words;
- the virtualization by retail's own gains;
- the common sample cycle;
- the disabled Doppler.

Used:

- **Positional rendering, with HRTF and the output mode.**
- **The OpenAL distance model** across the range's span: full volume to 6.4%
  of it, silent the range beyond, 80 to 1,330 for range 0. It fades out
  toward the silent end, with air absorption. The loop plays at retail's
  level, 90 over 256 of its volume.
- **Traced occlusion and liquid boundaries**, on the direct and wet paths.
- **The room reverb** from the map's audio zone sidecar, 15 of them for MP,
  or from the traced room.
- **The tone classes**, by sample name or `world`.
- **`s_pvs`**, when it is asked for.

Left out:

- **Retail's left/right balance.** It stays in the legacy mixer
  (`s_backend legacy`), the explicit retail-exact option.
- **Doppler.** Retail disables it for loops.
- **The weapon sound shaders.** They have no RTCW rules.
- **Merging duplicates.** Retail's merge loop never runs, so there is none to
  adopt.

On `mp_beach` the loops in range play positionally at 0.70 (89/127) with
`world` tone, traced occlusion up to 0.74 and sends into the 246-zone
sidecar's outdoor-to-hall room.

## Evidence

`scripts/probe_rtcw_loops.py` compiles unchanged original MP submission,
spatialization and vector functions only in disposable fixtures with their
GPLv3 notices. It compares 9,306 submissions and 196,608 stereo/mono gain
calculations. Reports record source and implementation hashes and compiler
arguments under `.tmp/rtcw-manager/loop-reference-{x64,x86,sanitizers}.json`.
They exercise capacity, duplicate entity numbers, range and volume extremes,
underwater flags, listener orientations and deterministic positions. This is
CPU policy evidence, not recorded output or an SP fade comparison.

`tests/rtcw_loop_audio_tests.py` executes the real module adapter, both backend
entrypoints and OpenAL's update loop in controlled hosts. It covers argument
extents, malformed inputs, frame clearing, handle loading, channel limits,
duplicate submissions, disappearing sources, virtualized source restoration,
shared sample offsets and long clocks. The client-state regression additionally
checks the three-word viewpoint import and atomic rejection of every non-finite
component.

The final CPU comparison passes on Windows x64, Windows x86 and Linux x86
with ASan/UBSan. The combined loop/viewpoint, music and spatial-import tests
pass 12 cases on Windows x64 (9.096 s) and x86 (9.640 s). The five loop and
client-state cases pass Linux x86 ASan/UBSan (22.089 s). Logs are
`loop-and-view-{x64,x86,sanitizers}.log` under `.tmp/rtcw-manager/`.

The first development client, SHA-256
`265510eddfe59aa9957a928fd81e379b33247122b3e871928529bdb0c06e4421`,
passes loop submission on installed `mp_beach` and stops at viewpoint import 71
(`CG_SETCLIENTLERPORIGIN`). Audio-disabled, muted OpenAL and zero-volume legacy
runs all reach that boundary. Their reports are `loop-audio-map/manifest.json`,
`loop-audio-openal/manifest.json` and `loop-audio-legacy/manifest.json`. This
executable predates the final range-storage, phase and viewpoint changes;
subsequent build/runtime evidence must identify its own executable.

The subsequent development client, SHA-256
`b4a79983eb4ce577d2840367a9b84c0564fea049e31da729a9716616d79f6254`,
passes both loop and viewpoint imports on installed `mp_beach` and reaches
UI import 93 (`UI_CL_GETLIMBOSTRING`). SP remains at `BOTLIB_LOAD_MAP` (206).
`loop-and-view-map/manifest.json` records that executable and renderer API 23.
The [limbo/cinematic follow-up](2026-09-15-rtcw-limbo-cinematics.md) continues
past the MP chat boundary and records later executable identities.

The preceding [spatial-audio checkpoint](2026-09-15-rtcw-spatial-audio.md)
predates this loop implementation. The subsequent
[limbo/cinematic checkpoint](2026-09-15-rtcw-limbo-cinematics.md) includes it in
a complete regression and six inspected Q3/QL captures. All runtime probes use isolated writable homes,
read-only installations, windowed unswitched private desktops and no input.
No retail sound files or modules are incorporated. Production changes are
independently authored under GPL-2.0-or-later and extend the inherited backends.
