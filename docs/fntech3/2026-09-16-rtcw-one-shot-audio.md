# RTCW one-shot sounds — 2026-09-16

One shared client now implements SP/MP one-shot, extended-flag and local-sound
imports through the legacy and OpenAL backends. The same policy serves native
and QVM callers. This checkpoint does not establish playable RTCW support.

## Contract

The module boundary copies optional fixed positions, checks dynamic entity
indices against RTCW's 1,024-entity limit, validates signed argument widths
and rejects non-finite positions, wrapping pointers and truncated QVM records.
Plain starts read four arguments, extended starts five, and local starts two.
An explicit fixed position permits an arbitrary signed entity identifier, as
in the reference, without indexing an entity-position array. Disabled audio
accepts valid calls without dereferencing a missing backend. Q3/QL retain their
existing public and volume-extension paths.

The shared allocator owns channel state separately from backend resources:

- Cut-all overrides no-cut; ordinary cutoff obeys no-cut, ok-to-cut and
  request-cut flags.
- SP replaces client channels other than auto and weapon, and keeps auto
  starts distinct. MP can reuse the same sample on an auto channel.
- Reuse preserves allocation age. Freed slots join a FIFO queue, initially
  ordered from channel 95 down to 0. SP retains its final free-list node;
  ordinary allocation therefore has 95 slots, versus MP's 96.
- Exhaustion first considers the same entity/sample, then older eligible
  voices of the same entity, then other eligible voices. Listener and
  announcer protection and the strict age comparison are preserved in those
  latter searches. Matching samples can still replace a no-cut voice under
  exhaustion, as observed in both references.

The original exhausted-pool fallback reads beyond its channel array. Original
MP also loses its free-list head after consuming the last node; its later
free operation can overwrite an active channel's bookkeeping. The new policy
never reuses an inactive sentinel through eviction, drops when no defined
candidate exists, and recovers freed slots after exhaustion. Reference-policy
comparisons intentionally retain at least two free nodes; separate engine
tests cover bounded exhaustion, protection, repeated releases and recovery.
These safety differences are explicit, not claims of bug-for-bug parity.
The comparison scope is the original Windows/Linux 96-channel configuration;
the historical `__MACOS__` 64-channel branch is not reproduced or verified here.

One-shots use a master volume of 127 and the original linear 1,250-unit range,
including its 80-unit full-volume radius and integer stereo quantization.
The shared spatial helper now preserves the reference's double subtraction
and pan multiplication, followed by a float store before volume multiplication.
A broader comparison found a one-step difference in the former all-float
calculation. Arbitrary one-shot ranges are not an RTCW import contract; an
experimental Linux comparison at `INT_MAX` range also exposed extended
precision differences. Final one-shot comparisons use the actual fixed range;
loop comparisons continue to cover all six tested ranges.

The legacy backend keeps RTCW entity positions separate from loop submissions
and preserves them across sound clears. Audio updates refresh dynamic gains,
including full-volume listener/menu sounds. OpenAL uses the same allocation
policy and bypasses Q3's duplication limits. It rendered each one-shot with
this gain and pan until 2026-09-24, when it adopted the engine's own features
(see below). Its completed sources release their logical slots, and a
source-allocation failure leaves no occupied orphan slot.
Existing SP effects/master fades apply after spatial gain. Hardware source
limits can still cause OpenAL starts to drop; output-device parity is open.

Entity streaming imports remain gated, so the reference's replacement of a
matching entity stream is not implemented yet. Original MP's queued/threaded
submission timing is also not established by the immediate backend adapter.

## Verification

All production changes are independently authored GPL-2.0-or-later code.
Original GPLv3 functions are compiled only in disposable fixtures with their
notices; no retail module, asset or reference implementation is incorporated.

`scripts/probe_rtcw_one_shots.py` compares 421,778 sound starts per mode with
the original functions and allocators. SP has 438,154 complete channel-state
comparisons; MP has 438,158. These include all 32 defined flag combinations,
client-limit boundaries, every standard channel, same/different samples and
deterministic sequences of starts/releases. Reports are
`shot-reference-{sp,mp}-{x64,x86,sanitizers}.json` in `.tmp/rtcw-manager/`.

The extended loop probe additionally compares 294,912 SP and 32,768 MP
one-shot stereo-gain pairs at the fixed range. Existing loop coverage remains
83,754 SP submissions / 1,769,472 gains and 9,306 MP submissions / 196,608 gains,
with 9,225 SP effects-volume cases. Reports are
`shot-gain-{sp,mp}-{x64,x86,sanitizers}-final.json`.
Both reference probes pass Windows x64, Windows x86 and Linux x86 ASan/UBSan
with leak detection. `shot-source-identities.json` pins the tested policy,
backend, boundary, test/probe and reference files.

The 18 focused tests exercise actual imports, legacy start/free/update paths,
OpenAL start/cleanup/gain selection, facade profile dispatch, disabled/muted
backends, resource failure, and the existing music/loop/mix contracts. Final
Windows x64 and x86 results are in `shot-final-x64.log` and
`shot-final-x86.log`; sanitizer results are in `shot-final-sanitizers.log`.
All 18 pass: x64 in 11.744 seconds, x86 in 12.939 seconds and Linux x86
ASan/UBSan with leak detection in 82.724 seconds.

Both Windows x86 client/server builds with GLX, Vulkan and RTX targets pass.
Default client SHA-256:
`684c2f4098cdcebc31638146e5aec0480bfa84c13bab587e22d0c0e3d11ef735`.
Experimental client SHA-256:
`1b31e577e46fc21a5f8fb0581eed29e3246d5a1a54efc5cb87487ac809dd19e1`.
Build logs are `shot-final-default-build.log` and `shot-final-dev-build.log`.
The worktree remains uncommitted at
`c75573c7375e44c908256020ada2d7e12970cacf`.

The final complete suite passes 111 Meson checks with two skips. The Python
aggregate runs 1,397 tests in 373.419 seconds with 11 skips. Results are
`shot-final-suite.log` and `shot-final-testlog.txt`. The earlier full run also
passed before the final gain/dynamic-position corrections; it is retained as
`shot-full-tests.log` / `shot-full-testlog.txt` and does not replace final evidence.
Intermediate logs retain the corrected array-pointer build error and the
dynamic-position regression caught by the new backend test. Skipped OpenAL
loopback checks supply no measured output evidence.

Final installed-module probes are `shot-final-openal/manifest.json` and
`shot-final-legacy/manifest.json`. Both SP runs observe pure mode 0, audio
initialization, server `CS_ACTIVE` and `CL_FirstSnapshot` on `escape1`, then
stop honestly at game import 207 (`BOTLIB_UPDATENTITY`). No SP playable-map
capture is produced. Both MP runs observe pure mode 1, active local admission,
clean exit and an inspected engine PNG of the deployment/briefing UI. Their
movies/panels are readable; the UI obscures the world and no input was supplied.

The final default executable passes six Q3/QL map probes across GLX, Vulkan
and RTX raster. All six engine PNGs were inspected for readable worlds and
Q3 HUD/weapon or native QL join UI. Module and renderer identities, windowed
mode and clean exits are audited in `shot-final-live/manifest.json`. These
are startup/scene checks, not broader gameplay or audio-output parity.

All runtime probes use isolated writable homes on a private, unswitched
desktop, force 640×480 windowed mode, disable input capture and leave installed
files unchanged. The probes request muted audio. Captures come from the engine
render target; temporary homes were removed after checking containment/links.

## The engine's own features (2026-09-24)

Under invariant 11 of `AGENTS.md`, the OpenAL backend no longer pans a
one-shot as retail's mixer does. It renders RTCW's data through the engine's
own audio, as it renders every profile's sounds. The [Wolfenstein OpenAL
voices record](2026-09-24-wolfenstein-openal-voices.md) has the design, tests
and runtime evidence. The allocation above and SP's effects stage are
unchanged.

Used:

- **Positional rendering, with HRTF and the output mode**, for a one-shot
  away from the listener.
- **The OpenAL distance model** across the fixed range's span: full volume to
  80 units, the reference, and silent at 1,330, the far end. It fades out
  toward that end, with air absorption. This is a Quake III world sound's
  span. `s_alDistanceModel linear_clamped` plays retail's line exactly.
- **Traced occlusion and liquid boundaries**, on the direct and wet paths. A
  one-shot past 1,330 is silent and not traced.
- **The room reverb** from the map's audio zone sidecar, 32 of them for SP
  and 15 for MP, or from the traced room.
- **Tone classes** by channel. RTCW's channel numbers are Quake III's.
- **`s_pvs`**, when it is asked for.

At the listener:

- A one-shot is head-relative at full level. It takes the room, not
  occlusion.
- A local channel (1, 6 or 7) there is an interface sound: dry, local or
  announcer tone.
- Elsewhere a local channel is attenuated where it is, as retail attenuates
  every sound but the listener's own.

Left out:

- **Retail's left/right balance.** The device places the voice. Retail's
  exact gain and pan stay in the legacy mixer (`s_backend legacy`), the
  explicit retail-exact option.
- **Doppler.** A one-shot carries no velocity.
- **The weapon sound shaders.** They ship rules for Quake III and Quake Live
  samples only, so none apply.

## Remaining work

SP AI entity updates must retain RTCW's shared entity registry while respecting
the selected navigation world's link ownership. Source inspection confirms
that the stock SP bot update selects world 0 before submitting entities; other
queries can select world 1. Forwarding directly into Quake III's selected-world
entity storage would be unsafe because only world 0 owns the entity table.
That implementation, further AI imports, entity streaming, sustained gameplay,
save/load, peer interoperability and measured audio output remain open.

The unrelated, previously recorded Q3/QL routing-cache reader audit also
remains open. RTCW continues to use its separate checked cache reader.
