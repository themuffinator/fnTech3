# RTCW SP sound reset and loop mixing — 2026-09-16

The same client now dispatches SP's sound-reset modes and looping sounds to
both audio backends. This extends the shared RTCW loop policy already used by
MP. It does not establish playable SP/MP support or close the release gates.

## Contract

SP clear mode 0 (and other values except 1 and 2) clears the submitted loop
frame. Mode 1 also clears one-shots, raw/effect streams and voice lanes while
retaining music, its buffered audio and its queued successor. Mode 2 clears
music and its queue as well. A disabled backend accepts these imports safely.
The SP clear import consumes one argument; MP consumes none. Both original
stop-loop imports are deliberately inactive and consume no arguments.

SP loop submission copies position/velocity and captures the last serviced
master fade into a truncated integer volume. Spatialization then applies the
range and volume factors and caps each channel at 255. The effects mixer has
its own quantization: `int(s_volume * 256)`, followed by a second truncated
master fade only below one. A master gain above one amplifies submitted loops
and streams, but does not amplify that second effects stage. Repeated entity
numbers and handles remain separate submissions, with a 128-submission limit.
The stored underwater flag is unused by the original mixer; loop Doppler is
disabled. MP retains its existing volume policy.

The legacy backend separates SP music into its own PCM ring so a reset or raw
cinematic producer cannot discard/overwrite it. Stream master gain is applied
before mixing effects. OpenAL applies the quantized effects gain to sources
and the stream gain separately; its listener stays at unity for SP. Source
maximum gains allow the supported SP amplification range. The corresponding
[OpenAL Soft property validation](https://github.com/kcat/openal-soft/blob/master/al/source.cpp)
accepts finite, nonnegative maximum gains; this observation is not an
output-device parity claim.

The shared legacy PCM multiplications now widen before scaling and saturate
accumulation. The prior signed 32-bit product could overflow with an ordinary
loud sample at `s_volume 2`. Raw conversion accepts unaligned PCM16 safely,
bounds writes to ring capacity, handles wrapping cursors and rejects malformed
rates/formats and non-finite gains. Ordinary Q3/QL/MP volume calculations and
the inherited signed-stereo/unsigned-mono raw PCM8 convention are retained.

## Evidence

The original SP/MP sound code is GPL-3.0-or-later. Production changes are
independently authored under GPL-2.0-or-later; no GPLv3 implementation or retail
assets/modules are incorporated. `scripts/probe_rtcw_loops.py` compiles original
functions only in disposable fixtures with their notices. SP comparisons cover
83,754 submissions, 1,769,472 spatial-gain pairs and 9,225 effects-volume cases.
MP comparisons cover 9,306 submissions and 196,608 spatial-gain pairs.
Both pass Windows x64, Windows x86 and Linux x86 ASan/UBSan. Reports are
`sp-loop-reference-{x64,x86,sanitizers}.json` and
`sp-audio-mp-reference-{x64,x86,sanitizers}.json` in `.tmp/rtcw-manager/`.

The 12 focused tests pass on Windows x64 (8.754 s), Windows x86 (9.200 s) and
Linux x86 ASan/UBSan with leak detection (50.726 s). The fixtures execute actual
module dispatch, backend entrypoints, reset paths, legacy channel painters,
raw/music rings, final mix stages and OpenAL gain routing. They exercise
preserved music/queues, full resets, disabled/muted services, malformed native
and QVM arguments, duplicate loops, fade quantization, virtualized voices,
partial blocks, cursor wrap and arithmetic bounds. Logs are
`sp-audio-{x64,x86,sanitizers}.log`.

The manager now also owns the `sv_pure` default and registration flags. Original
SP and iortcw SP register zero without `CVAR_LATCH`; original/iortcw MP register
one without that flag. Q3/QL retain their inherited default one and latch flag.
Both server initialization and map setup use the same profile registration;
explicit user values survive. The engine still freezes the effective pure
mode for the current map and advertises that effective value. Broader live
pure-mode transitions are not verified here. SP proof generation/verification
remains explicitly unavailable when pure mode is requested.

This corrects a separate discovery: fnTech3 had still registered the inherited
default one for SP, and the boot helper also forced one. The helper now leaves
the profile default alone, records observed values and offers `--pure` for an
explicit override. The upstream SP default described in the earlier pure
checkpoint was not yet implemented by fnTech3 at that time.

The server/default/proof/module checks and corrected source regression pass
34 tests on Windows x64 (2.383 s) and x86 (1.914 s). The three executable
default/proof/module tests pass Linux x86 ASan/UBSan (15.648 s). Logs are
`sp-pure-defaults-{x64,x86,sanitizers}.log`.

Both Windows x86 client/server builds with GLX, Vulkan and RTX targets pass.
The final default client SHA-256 is
`53f95fed85dba0ff732ac806b12d84a0f7ce4eb9f44e55b6ef39f0fa27c9b3b1`;
the experimental client is
`202b18e7b637c18d2a0eff9ce11b35de2534a967e6cdfc544382777faaa529e9`.
Build logs are `sp-pure-defaults-build.log` and `sp-pure-defaults-dev-build.log`.
`sp-audio-source-identities.json` pins production, fixtures and reference files
at commit `c75573c7375e44c908256020ada2d7e12970cacf` with an uncommitted worktree.

The final full suite passes 111 Meson checks with two skips. Its Python
aggregate runs 1,391 tests in 365.562 seconds with 11 skips. Results are
`sp-audio-full-recheck.log` and `sp-audio-full-recheck-testlog.txt`. The first
run found a source assertion tied to the old OpenAL gain expression; the
updated assertion and executable gain-routing fixture cover the new SP bound
and the preserved Q3/QL bound. The original failure logs remain available as
`sp-audio-full-tests.log` and `sp-audio-full-testlog.txt`. The skipped OpenAL
loopback check does not provide device-output evidence for this checkpoint.

## Installed-module probes

`sp-audio-final-openal/manifest.json` observes SP pure mode zero, initialized
OpenAL, local `escape1` admission and its first client snapshot. SP then stops
at cgame import 31 (`CG_S_STARTSOUND`).
`sp-audio-final-legacy/manifest.json` also observes SP pure mode zero and an
initialized legacy backend; that run reaches server `CS_ACTIVE` before stopping
at game import 207 (`BOTLIB_UPDATENTITY`). It has no client-first-snapshot log.
These are distinct run observations, not evidence that audio backend selection
causes the different first unsupported import.

Both manifests show MP pure mode one, clean shutdown and inspected textured
deployment/briefing captures on `mp_beach`. The menu obscures the game world.
`sp-pure-explicit/manifest.json` verifies that an explicit SP `--pure 1` survives
and reaches the honest unsupported-verification guard. There are no SP map
captures and no playable-campaign claim.

All six final Q3 `q3dm6` / QL `campgrounds` probes pass, with inspected engine
PNGs across GLX, Vulkan and RTX raster. Their same-executable identities and
visual audit are in `sp-audio-final-live/manifest.json`. Q3 shows a textured
world, weapon and HUD; QL shows its native join menu and world. Spawn variation
does not establish image parity. All probes used windowed unswitched private
desktops with input disabled, isolated writable homes and read-only installs.
The RTCW OpenAL runs were focus-muted; legacy volumes were zero. Homes were
removed and installation metadata remained unchanged. No OS captures or input
injection were used.

## The engine's own features (2026-09-24)

Under invariant 11 of `AGENTS.md`, the OpenAL backend no longer pans an SP
loop as retail's mixer does. It renders SP's data through the engine's own
audio. The [Wolfenstein OpenAL voices
record](2026-09-24-wolfenstein-openal-voices.md) has the design, tests and
runtime evidence.

Kept from retail:

- the reset modes, the 128 separate submissions and the volume captured
  with the master fade;
- the effects stage, which still scales each source and its sends, with
  `s_volume` quantized as above and the listener at unity;
- the 255/127 cap and the disabled Doppler.

Used:

- **Positional rendering, with HRTF and the output mode.**
- **The OpenAL distance model** across the submitted range's span: full
  volume to 6.4% of it, the reference, and silent the range beyond, the far
  end. It fades out toward that end, with air absorption. The loop plays at
  its retail level, 90 over 256 of its captured volume, on a one-shot's
  scale of 127.
- **Traced occlusion and liquid boundaries**, on the direct and wet paths.
- **The room reverb** from the level's audio zone sidecar or the traced room.
- **The tone classes**: a loop has no channel, so it takes its sample
  name's class or `world`.
- **`s_pvs`**, when it is asked for.

Left out:

- **Retail's left/right balance.** Retail's exact gain and pan stay in the
  legacy mixer (`s_backend legacy`), the explicit retail-exact option.
- **Doppler.** Retail disables it for loops.
- **A merge of duplicate loops.** Retail's inner merge loop never runs, so
  there is none to adopt.

On `escape1` the cell block's loops are positional behind the walls:
occlusion 0.84 to 1.00, a bandpass tone, and stone-room sends from the 397
zones of its sidecar.

## Limits

These CPU/backend checks do not establish PCM phase, acoustic output or exact
original stream-buffer timing. Entity streaming, script-driven sound triggers,
one-shot channel policy, save/load restoration and output-device parity remain open. Broader RTCW
campaign, AI, rendering, demo and peer-interoperability gates remain open in
the [compatibility ledger](COMPATIBILITY.md).

No new unrelated issue was found. The previously recorded inherited Q3/QL
RCD-reader audit remains open; RTCW uses its separate checked reader.
