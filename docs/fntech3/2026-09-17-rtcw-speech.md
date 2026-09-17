# RTCW speech amplitude and default-sound checkpoint

RTCW SP's `CG_S_GETVOICEAMPLITUDE` now measures playing audio through the
selected legacy or OpenAL backend. It no longer drops the client when sound
is enabled. Both backends complete the installed `escape1` introduction and
reach the first-person knife/health-HUD view with nonzero speech observations.
Full RTCW compatibility remains gated.

## Contract and implementation

The shared measurement keeps SP's 100-sample window, signed accumulation of
PCM16 samples whose magnitude exceeds 5000, arithmetic-shift rounding,
0–255 result and cutoff below 25. Streamed dialogue instead uses the peak of
the left raw channel divided by 8000, before spatial/master gains. A valid
later one-shot measurement overrides stream speech, including a measured zero;
an incomplete one-shot tail does not overwrite the stream result. MP's original
disabled talking-animation query remains zero. Invalid pointer-width/entity
arguments, non-RTCW imports and incomplete sound interfaces remain rejected;
nonclient SP entities and disabled/uninitialized sound return zero.

The legacy backend reads bounded windows across PCM, ADPCM, wavelet and mu-law
chunks without modifying the mixer's shared decode cache. It checks partial
chains and wavelet lengths and handles sample-clock wrap without signed
overflow. The query follows current backend playback state, so stopping or
clearing a sound cannot retain a cached mouth amplitude.

OpenAL retains one amplitude byte per possible one-shot window, computed in
linear time before discarding uploaded PCM. Stream buffers retain one peak
byte per left-channel frame only for SP entity streams. Those entries follow
successful queue/unqueue operations and clear with the stream. Source state
and sample offsets select the current window; failed offset queries return
zero rather than inventing a level. OpenAL offsets refer to the entire buffer
queue, including processed buffers until they are removed, as specified in
the [OpenAL 1.1 specification](https://www.openal.org/documentation/openal-1.1-specification.pdf).
SP voice lanes now allow eight approximately 50 ms chunks within the existing
global buffer budget, providing the future samples needed for facial animation.
Other entity lanes retain their two-buffer target.

Both backends use a quarter-second lookahead at their actual sample rate and
playback cursor. Original SP used its paint cursor and integer `s_khz * 1000`;
iortcw SP's inspected mixer uses a 0.01-second lead and a fixed 22 kHz factor.
The source comparison establishes measurement agreement on identical sample
windows, **not** clock alignment or audible/facial synchronization parity.
OpenAL also measures its converted queue while the legacy backend measures
its resampled raw ring. These distinctions remain explicit runtime gates.

## Default-sound failure found during runtime testing

The first legacy intro run stopped when a loop used sound handle zero. That
handle had been seeded with Q3's `sound/feedback/hit.wav`, which is absent from
the RTCW installation, leaving a zero-length sample. The RTCW sound manager
now reserves its own internal `***DEFAULT***` handle on both backends. It is
silent by default: eight mono frames for SP and 512 for MP. `s_defaultsound`
enables the original small diagnostic ramp; Q3/QL retain their existing paths.

An intermediate attempt exposed the shared public loader's rejection of
leading-asterisk player aliases. Legacy initialization now reserves the
internal handle directly, and reloads it without attempting filesystem access.
The bounded default-sound fixture exercises first registration, repeated
registration, reload, upload failure, both RTCW modes and the Q3/QL path.
Failed runtime evidence is retained separately from the successful final runs.

## Evidence

The base commit is `1c2b716a5bd21f1c17ea9368c98717af220e7f7f`; this checkpoint
includes the uncommitted character-material and speech changes. Source hashes,
ordinary client/server/renderer identities, compiler reports, command lines,
installed-module identities and capture hashes are retained under
`.tmp/rtcw-manager/speech/`. `source-identities.json` freezes the production
inputs, and `runtime-audit.json` checks all eight final runs and twelve images.
The final ordinary Windows x86 development client has SHA-256
`c8ff8241e52c52bf4b945a19681474a7ce146011acfb335844ad132bf2b91554`.

- `scripts/probe_rtcw_speech.py` compares 299,264 cases per original/iortcw SP
  reference with zero mismatches on Windows x86, Windows x64 and Linux x86
  with AddressSanitizer/UndefinedBehaviorSanitizer. The corpus includes every
  PCM16 value, threshold occupancy, cancellation, stream peaks/partial tails
  and chunk traversal. Compressed-reader fixtures supply decoded PCM; these
  comparisons do not validate the compression codecs themselves.
- The 28 focused audio tests pass on Windows x64 and sanitized Linux x86.
  They exercise frontend bounds/lifecycle, actual legacy readers and backend
  queries, OpenAL queue offsets/failures/clears, stream-versus-one-shot
  precedence, missing-sound handling and existing music/loop/channel behavior.
  Windows x86 validation is included in the complete Meson source suite.
- Both existing Windows x86 default and RTCW-development builds succeed.
  The complete development suite passes 111 Meson tests with two skips and
  zero failures, including 1,475 Python tests with eleven skips (581.536
  seconds). The detailed log is
  `.tmp/rtcw-manager/build-rtcw-dev/meson-logs/speech-final.txt`.

The final private runtime probe changes only intro advancement/capture hooks
and aggregate speech-query logging; it does not alter audio decisions. Both
production source files were restored and the ordinary client rebuilt before
runtime validation. `cl_main.cpp` retains SHA-256
`1af70946515c465024f044255667eea759f98a5c3914d6c98c010a12a2259505`.
The private executable's identity and exact instrumentation are retained in
`probe-v3/`. Earlier incomplete/tainted validation attempts are not final
evidence: one x64 fixture run overlapped temporary instrumentation, and an
initial full-suite run was stopped before correcting placeholder registration.

Each final SP process receives exactly one Ready click under the user's
standing intro-button authorization in `AGENTS.md`. Engine PNGs are taken at
about 2, 35 and 90 seconds; the process exits at about 95 seconds. No movement,
combat, other menu input, operating-system input or OS capture is used.

| Final SP backend | Last sampled queries | Nonzero results | Maximum amplitude | Outcome |
| --- | ---: | ---: | ---: | --- |
| Legacy | 16,520 | 557 | 255 | Three inspected PNGs; valid first-person snapshot; normal exit |
| OpenAL | 16,530 | 518 | 255 | Three inspected PNGs; valid first-person snapshot; normal exit |

The differing sample counts are observations from separate real-time runs,
not an amplitude-parity comparison. Both runs use an active backend with
focus-muted output. Audible playback was not assessed. All six frames are
1600×1200 PNGs with CSM disabled and `r_picmip 0`/`r_picmip2 0`. Byte-identical
copies are delivered in `.tmp/screenies/rtcw-sp/audio-enabled/`, alongside an
index and hash manifest. The earlier four-game menu/join gallery is retained.

The final ordinary executable also passes six windowed, input-free startup
checks: Q3 `q3dm6`, QL `campgrounds` at its Join Match menu and RTCW MP
`mp_beach` at its deployment/briefing screen, each on both audio backends.
Their six engine PNGs were inspected. These tests disable Steam and do not
replace the earlier signed-in QL menu captures or establish online admission.
Every final test uses an isolated writable home; installation inventories are
unchanged and homes are removed. All final runs exit without engine errors.

## Remaining gates and provenance

Measured device output, original-engine timing/PCM comparisons, facial
interpolation, complete dialogue/codec coverage, device recovery and resource
pressure remain open. Campaign movement/combat/AI, save restoration, map
transitions, MP classes/objectives, sustained sessions, demos and bidirectional
interop remain open. Vulkan/RTX still guard active RTCW scene fog.

The original [SP mixer](https://github.com/id-Software/RTCW-SP/blob/master/src/client/snd_mix.c),
[SP sound system](https://github.com/id-Software/RTCW-SP/blob/master/src/client/snd_dma.c),
[MP sound system](https://github.com/id-Software/RTCW-MP/blob/master/src/client/snd_dma.c)
and [iortcw](https://github.com/iortcw/iortcw) provide observed contracts.
Their GPLv3-or-later notices are preserved with unchanged reference functions
only in disposable comparison fixtures. Production is independently written
GPLv2-or-later code; no retail modules, sound data or other proprietary content
are committed.

An unrelated source issue was found: the existing legacy
`S_PaintChannelFromWavelet` initially invokes the ADPCM decoder before later
wavelet chunks use `decodeWavelet`. Wavelet loading is currently disabled by
`#if 0`; this dormant playback issue was not changed or runtime-validated here.
Previously recorded Q3 menu-logo gaps, routing-cache bounds work, generic
OpenAL big-endian PCM16 conversion and the local Steam-provider manifest
mismatch also remain open.
