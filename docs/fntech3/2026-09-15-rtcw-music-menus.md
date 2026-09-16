# RTCW SP music and shared-executable menus

The experimental Windows x86 `fntech3.exe` now boots the legitimate SP and MP
main menus through GLX, Vulkan and RTX raster. All six inspected engine PNGs
have textured backgrounds and readable navigation. SP uses English; MP uses
French in these probes. This is menu/initialization evidence, not playable
campaign, multiplayer interoperability or complete rendering/audio parity.

The working base is `c75573c7375e44c908256020ada2d7e12970cacf`, with uncommitted
work. The default RTCW activation gate remains in place. The explicitly enabled
`-Drtcw-development=true` client has SHA256
`d17742fde4ccdb83310d3b0b0aefb2a254a573d4dc4137510ca6b707ce20b608` and renderer
API 22. The [contract foundation](2026-09-14-rtcw-contracts.md) records earlier
manager, ABI, filesystem, renderer and localization work.

## SP music contract

An independently written controller owns music filenames, loop/queue state,
deferred stops and stream fades for both existing audio backends. UI/cgame
adapters read SP's exact three-argument background-track import and its separate
stream/master-fade imports. MP retains its two-argument import. Native and QVM
pointer paths use explicit VM ownership, bounded names and checked integers;
the stock native modules remain external inputs.

The controller preserves the observed original SP rules: source-rate fade
duration against the output sample clock, the zero-clock sentinel, interrupted
fade behavior, `onetimeonly`, queue types -1/-2/-3, retained queued-name fallback,
`s_currentMusic` updates and saved loop metadata after clearing playback.
Queued stops still drain while music volume is zero. Empty loops have bounded
work per service; failed/invalid-rate opens release codec resources. Shutdown
resets process state. Non-finite fade volumes are rejected atomically; accepted
amplification/stop targets are bounded to [-16,16].

OpenAL applies the stream fade to its music source, including buffered PCM,
and the master fade to listener gain. SP effects volume is applied to effects
sources, leaving music independent of `s_volume`. A monotonic sample clock can
use the device clock or host fallback, with rebasing and saturating arithmetic.
The legacy mixer applies music gain during streaming and a bounded master gain
before final output conversion. This preserves the fade contract, but does not
claim original PCM bit identity or identical buffering/quantization.

`scripts/probe_rtcw_music.py` compiles unchanged original GPLv3 functions only
in disposable licensed fixtures. On Windows x86, Windows x64 and Linux x86
with ASan/UBSan/leak detection, it passes **3,000 starts, 3,000 fade values,
26 queue transitions and 2,000 master requests**. Reports
`music-reference-{x86,x64,sanitizers}.json` retain source and implementation
hashes under `.tmp/rtcw-manager/`. Master-request checks compare saved fade
fields; interpolation and backend application have separate unit coverage.

The endpoint explicitly rounds to binary32 before integer conversion. An
initial Linux x87 comparison exposed excess-precision differences, including
an 11,024-versus-11,025 sample endpoint. The Linux source fixture uses SSE2
binary32 arithmetic, recorded in its report, rather than treating compiler-
dependent x87 temporaries as an additional compatibility contract. The normal
Linux sanitizer tests still execute without that fixture-only SSE override.
Legacy Makefile audio compilation disables fast-math so validation remains
effective. A full legacy Makefile build remains unverified.

`music-final-{x86,x64}.log` each records **22 passing focused tests**.
`music-final-sanitizers.log` records **eight passing methods** on Linux x86,
including both backend entrypoints, the actual OpenAL update and legacy master
mix, facade rejection/disabled sound, native/QVM argument widths, and all three
shader-file loaders. These tests use controlled codecs/devices; they do not
substitute for measured output or game-script coverage. Entity streaming sounds
remain unimplemented; unallocated non-music fade slots have no effect.

## Shader-file recovery

The first SP menu reached normal exit but had default texture boxes. Read-only
inspection found two extra closing braces at the end of the installed SP UI
shader file, and an unfinished definition in `common.shader`. The inherited
loader discarded each entire file. Original RTCW instead indexes earlier usable
definitions without whole-file rejection.

All three renderers now share an RTCW-only filter that retains complete
definitions and their uncompressed text. Stray root tokens and unfinished tails
are discarded within the individual file, so malformed input cannot consume
another file's definitions. Q3/QL retain their existing rejection and compression
behavior. Synthetic tests exercise duplicate order, comments/quotes, orphan
tokens, malformed tails, empty input and repeated file isolation. No installed
shader text or reference implementation is incorporated into the engine/tests.

## Installed menu and audio observations

`scripts/probe_rtcw_boot.py` records exact commands, configs, module diagnostics,
client/renderer hashes, normal exits and engine screenshots. Evidence is under
`.tmp/rtcw-manager/`:

| Bundle | Renderer | SP and MP observations |
| --- | --- | --- |
| `mus-glx/manifest.json` | GLX | Inspected textured menus; OpenAL initialized, focus-muted, four decoded music buffers queued; SP reports `sound/music/l_theme.wav`, MP `sound/music/x_action.wav` |
| `mus-vk/manifest.json` | Vulkan | Inspected textured menus; legacy audio initialized with effects/music volume zero; no decoded-output claim |
| `mus-rtx/manifest.json` | RTX raster | Inspected textured menus; sound disabled |

All six use the same unchanged executable and the installation at
`E:/SteamLibrary/steamapps/common/Return to Castle Wolfenstein`. Each runs at
640x480 windowed on an unswitched private desktop, with no input control or OS
capture. Writable homes are isolated and removed after containment/link checks;
the installation inventory remains unchanged. Visual inspection is recorded
separately from automated boot success.

The earlier `music-ui` bundle retains the SP texture failure. `mus-al` retains
an initial probe false failure: an optional audio-capability description matched
an overly broad error classifier. The system OpenAL32 runtime also ignored the
requested OpenAL Soft null driver; the corrected report records focus muting,
not null output. `mus-glx` repeats both runs with explicit, verified focus-muted
semantics. The system runtime lacks OpenAL Soft extensions; this existing
capability limitation is not a newly introduced RTCW failure.

## Combined validation and next map blockers

The default Windows x86 client, dedicated server and all three renderers build.
The completed suite records **111 passing Meson targets, zero failures and two
configured skips** in `music-full-suite.log` / `music-full-testlog.txt`. Its Python
aggregate passes **1,357 methods in 325.109 seconds, with 11 skips**. The first
attempt found two isolated dispatch fixtures missing the new audio-host stub;
those fixtures were repaired and passed on Windows x86/x64 and Linux x86
sanitizers before this full rerun. The initial failure logs remain as evidence.

`music-live/manifest.json` records six further inspected Q3/QL captures using
the default client with SHA256
`555b90b7b6ddcf77c794fc318c9dd5243bdde6314c6211351c627cc31b72d1c6`.
Q3 `q3dm6` and QL `campgrounds` retain world, weapon/HUD or native join UI
rendering in GLX, Vulkan and RTX raster. These are bounded smoke checks, not
new bidirectional interoperability evidence.

An exploratory `mus-map/manifest.json` run loads both legitimate native game
modules but stops at SP `G_UNLINKENTITY` (33) and MP `G_SET_BRUSH_MODEL` (24).
The SP attempt used generic `devmap escape1` with game type zero: it did **not**
enable campaign AI. Campaign startup must use `spmap`/`spdevmap` and its separate
client-slot policy. Both failed runs were stopped by the bounded probe, with
isolated homes removed and the installation unchanged.

Open work includes RTCW `nofog`/`nocompress` material semantics visible in these
logs, remaining renderer/fog/cinematic/entity-stream imports, full sound-trigger
arguments, measured fade/queue playback, source-engine image/audio comparison,
SP campaign/AI/save-load, MP gameplay and bidirectional interop. The existing
Q3/QL release gates remain separate. No full compatibility claim is made.
