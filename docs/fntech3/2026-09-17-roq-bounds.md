# RoQ stream bounds, startup recovery and retail movie decoding

This checkpoint resolves bounded cinematic decoder and lifecycle gaps shared by
Q3, QL and both RTCW profiles. It does not establish complete cinematic, audio,
renderer or game compatibility. RTCW remains development gated.

## Corrections

The reader now counts bytes actually consumed, checks complete file/chunk reads,
and validates the full 32-bit chunk length. Packed chunks cannot borrow bytes
from another chunk or exceed their declared parent/count. Quad dimensions must
fit the two fixed 512×512 RGBA buffers and the decoder's 16-pixel grouping.
Codebooks, stereo sample pairs and complete VQ instruction streams are checked
before decoding. Motion sources must stay inside the previous frame. Malformed
looping movies stop instead of repeatedly reopening themselves.

Codebook counts require a compatibility detail: a zero large-entry count can
mean either no large-entry update or all 256 entries. Payload length distinguishes
the forms. The installed Q3 logo/end movies and RTCW menu/intro movies contain
1,536-byte, zero-flag codebooks that update only the small entries. The old reader
read beyond those payloads. The first strict corpus run rejected them; that
failed investigation is retained separately from the corrected results.

Valid terminal frames no longer need an extra lookahead header. The installed
corpus contains 17,391 video-frame chunks, and the corrected decoder emits exactly
17,391 frames. Compared with the previous decoder, sixteen movies gain their
previously skipped final frame, and Q3's logo loses 28 spurious frames produced
from stale data after EOF. The 17,375 shared frame hashes match the previous
decoder. This is per-frame RGBA FNV-1a comparison against the fnTech3 baseline,
not an independent original-engine visual comparison.

Movies started after client initialization now decode through setup chunks to
their first frame even while the synchronous startup loop holds the client clock
fixed. A setup-only looping movie terminates. Failed initial reads, aborted first
frames and held movies release their file ownership; the console startup loop
checks its handle again after a decoder shutdown. The actual blitter callback
type replaces an incompatible function-pointer cast found by Linux UBSan.

The original [RTCW SP decoder](https://github.com/id-Software/RTCW-SP/blob/master/src/client/cl_cin.c)
and [RTCW MP decoder](https://github.com/id-Software/RTCW-MP/blob/master/src/client/cl_cin.c)
both ignore `ROQ_QUAD_JPEG` without publishing a frame. The previous ledger's
description of that branch as a missing implementation was therefore not an
established original-engine compatibility defect. fnTech3 preserves the no-op
policy; JPEG image presentation is not newly claimed.

## Evidence

The [machine-readable record](2026-09-17-roq-bounds.json) identifies the dirty
worktree based on `0b6af9ba7a17d73bf27c5c016c00772a1a93899a`, source hashes,
Windows x86 executable/renderer hashes, installation/member identities and
commands. Local logs and captures are under `.tmp/roq-bounds/`. No proprietary
movie payloads, modules or credentials are retained in this evidence.

- Fifty focused Windows x86 tests pass. The new executable regression runs the
  actual reader, codebook and VQ decoder bodies; its short-read assertion fails
  against the pre-change source. It covers short headers/payloads, dimensions,
  both codebook count forms, high length bytes, VQ truncation/motion ranges,
  odd stereo payloads, packet containment/counts, terminal frames, looping,
  restart and partial shutdown. Presentation tests exercise all four profiles.
- Seven focused Linux i686 tests pass; the complete decoder regression runs
  with AddressSanitizer and UndefinedBehaviorSanitizer, including function-type
  checking. The strict MSVC development preset builds the shared client/server
  and all three renderer modules.
- The final full configured Meson suite passes 112 checks with zero failures
  and two skips. Its Python suite runs 1,541 tests with 62 skips and no failures.
  This is a local dirty-worktree build; it does not close clean-build, hosted CI
  or unsupported-platform gates.
- Read-only inspection and decoder execution cover 30 archive-member/profile
  occurrences representing 27 distinct installed RoQ files: eleven Q3 members,
  three SP members and sixteen MP members. The inspected QL archive directory
  contains no RoQ members. Retail payloads are sent to the compiled decoder in
  memory, never staged on disk.
- Four windowed GL runs use one unchanged Windows x86 client. Each rejects nine
  synthetic malformed movies, reaches all nine recovery markers, then displays
  a generated green frame and exits normally. The four engine PNGs were inspected.
  These establish same-process recovery under `q3`, `ql`, `rtcw-sp` and `rtcw-mp`.
- Installed SP intro probes use GL, Vulkan and RTX raster. Initial short captures
  show a dark transition and are not counted as visible-scene evidence. Longer
  probes inspect the letterboxed opening film scene. Sound is disabled; this
  does not establish audible playback, A/V synchronization or native ray tracing.

The first live recovery attempt reproduced the synchronous first-frame hang
after a valid quad-info chunk and needed termination. The retained `runtime/`
failure records are superseded by `runtime-final/` recovery runs and
`runtime-film/` installed-film runs. Every completed run uses an isolated home,
windowed mode, disabled input grabbing, engine `screenshotPNG`, no input events
or desktop switching, and an unchanged installation inventory. Writable homes
are removed after collecting non-proprietary evidence.

## Provenance and remaining gates

The zero-count interpretation is independently implemented from installed chunk
metadata and corroborated by [FFmpeg's RoQ decoder](https://github.com/FFmpeg/FFmpeg/blob/master/libavcodec/roqvideodec.c).
Its LGPL-2.1-or-later notice and the RTCW references' GPLv3 notices were inspected;
no external implementation text is incorporated. Existing engine license headers
remain unchanged.

Complete malformed-codec/fuzz coverage, nested packet compatibility, broader
dimension/format support, original-engine timing/visual parity, audio buffer
continuity and device lifecycle, cinematic game-script fades and full campaign
transitions remain open. In particular, this does not remove the existing audio
chunk clamps or claim their PCM/output timing is correct.

Unrelated diagnostics observed: QL cvar/default-description conflicts, RTCW MP's
missing console image and successful RTCW RTX MD3-to-MDC fallbacks. The first full
suite also encountered linker exit 1104 in the existing profile-restart config
test; the isolated Windows x86 rerun passed. These are separate from the decoder
corrections. Full-suite results are recorded in the accompanying manifest.
