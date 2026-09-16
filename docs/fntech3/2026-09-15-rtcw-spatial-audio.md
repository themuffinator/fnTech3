# RTCW spatial-audio boundary — 2026-09-15

The shared client now dispatches RTCW SP and MP entity-position and listener
imports through one checked adapter. Positions and the nine listener-axis
components are copied into engine-owned storage before reaching either audio
backend. Module memory cannot be modified by a backend accepting mutable axes.

Entity indices must fit the original 1,024-entity sound contract. Null, wrapped,
truncated QVM and non-finite input is rejected before any backend call. The
listener's water argument preserves its signed 32-bit encoding. The adapter
does not route Q3 or QL through RTCW validation. The original position setter's
off-by-one acceptance of index 1,024 is deliberately rejected to avoid indexing
beyond the sound entity array.

This is import and memory-boundary evidence. It does not establish original
attenuation, PCM output, underwater effects, looping sounds, entity streaming
or full gameplay audio. The default RTCW release gate remains closed.

## Evidence

`tests/rtcw_spatial_audio_tests.py` executes the production header with exact
native and QVM argument/buffer extents. It covers both profiles, unaligned
records, all valid entity extremes, invalid entity/word ranges, wrapped and
truncated buffers, infinity and quiet/signaling NaN in every component,
unchanged module storage and Q3/QL isolation. Its normal and optimized
fast-math runs both pass; validation reads the foreign memory's integer bits.

The combined music, fog and spatial-audio run passes ten tests on Windows x64
(6.430 seconds), Windows x86 (8.460 seconds) and Linux x86 with ASan/UBSan
(67.434 seconds). Logs are `.tmp/rtcw-manager/spatial-audio-{x64,x86,sanitizers}.log`.
The fog controller's four original/iortcw comparisons were also renewed on
all three targets, each passing 80,643 complete state comparisons.

The Windows x86 default client/server build passes for GLX, Vulkan and RTX.
The complete default suite passes 111 Meson checks with two skips, including
the full Python regression aggregate (322.91 seconds). The retained results are
`spatial-audio-full-tests.log` and `spatial-audio-full-testlog.txt` under
`.tmp/rtcw-manager/`.
Its client SHA-256 is
`4b223035aca06e27a13b192a8aacd3a7d3615c9f3a3a58fe8b21cc03e406884d`.
All six Q3/QL renderer probes pass with inspected engine-written captures:
Q3 renders its world, weapon and HUD; QL renders its native join UI over the
world without joining the match. Commands, module/renderer identities,
captures and visual audit are in `spatial-audio-live/manifest.json` under
`.tmp/rtcw-manager/`.

The development client SHA-256 is
`11fc7e24aefeaa383877ff2714c04d655f8616dcdaab40a0471527b249819301`.
Both mode probes in `spatial-audio-map/manifest.json` use this executable.
MP passes position import 36 and stops at looping-sound import 35
(`CG_S_ADDLOOPINGSOUND`) on `mp_beach`. This does not yet prove the listener
import executes in a complete retail frame. SP remains at game import 206
(`BOTLIB_LOAD_MAP`) on `escape1`. Neither produces a map screenshot.

All runtime probes use windowed, unswitched private desktops, disabled input,
isolated writable homes and read-only installations. Test homes are removed
and installation metadata remains unchanged. The recorded commit is
`c75573c7375e44c908256020ada2d7e12970cacf` with an uncommitted worktree.

The adapter is independently authored from original SP/MP module and sound
contract observations under GPL-2.0-or-later. No reference implementation text,
retail modules or game assets are incorporated.
