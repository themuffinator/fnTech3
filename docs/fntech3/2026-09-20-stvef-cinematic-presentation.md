# Elite Force single-player movie presentation

**Live-evidence correction (2026-09-21):** These probes used `r_renderer`,
which does not select fnTech3's backend. All fnTech3 live runs in this record
used GLx; Vulkan/RTX labels in the historical manifest identify the requested
test cases only. Source, native-vector and compiled-fixture evidence is
unchanged. See the [renderer audit](2026-09-21-stvef-renderer-verification.md).

The Windows x86 SP client now carries retail's one-second movie fades and
the central crop selected by `cl_VideoQuality >= 2`. The installed Bink
runtime still owns decoding; no runtime, SDK header, movie or retail
implementation text is incorporated. The SP development gate remains.

## Observed contract

The local mathematical probe calls `stvoy.exe` 1.20's cinematic drawing
routine at `0x403790` with synthetic pixels and a recording draw callback.
It substitutes a millisecond clock and leaves the native movie handle null.
This executes the retail calculations without launching the game or using
mouse, keyboard or operating-system capture.

| Contract | Retail observation |
| --- | --- |
| Defaults | `cl_VidFadeUp 1` and `cl_VidFadeDown 1`, both temporary; `cl_VideoQuality 0`, archived |
| Fade-in | Mute before first decode, hold that frame with `BinkPause`, then unpause after 1,000 ms; draw intensity increases from zero to one |
| Fade-out | Keep the final frame for 1,000 ms, decreasing intensity from one to zero before returning to the level or continuation |
| Quality 0 | A complete image in a padded power-of-two texture, stretched and clipped to remove the pad |
| Quality 1 | A complete image drawn in native-size tiles; the synthetic 640 by 480 image produces 12 tiles |
| Quality 2 | Copy the central 256 by 192 pixels into a 256 by 256 texture and draw a rectangle one third taller, clipping its bottom |

Start/pause sequencing is observed at `0x403cd0` and `0x404151`; end
sequencing is at `0x4036b0`. Fourteen native fade vectors cover 0, 1, 250,
500, 999, 1,000 and 1,001 ms in both directions. The independently written
fade calculation agrees within 0.0000002; retail's extended x87 precision
accounts for the final-bit differences.

## Implementation and bounds

`stvef_cinematic_presentation.hpp` owns the fade clock and optional crop
buffer. The clock begins when the first frame is decoded, handles unsigned
millisecond wrap, and never delays a failed first decode. Bink's installed
`_BinkPause@8` pauses the decoder. Pausing a one-frame movie also preserves
its final-frame duration in the engine's clock.

The client mutes sound during transitions and holds its in-game pause until
fade-out completes. Explicit stop, replacement, failure and module teardown
release resources immediately. Looped movies do not fade at every wrap;
held movies keep the final image. Shader movies are an engine extension and
continue without screen-movie transitions or crop.

Modes 0 and 1 use the full native image in one texture. Mode 2 reproduces the
central crop and draw dimensions while zeroing the unused rows, which retail
leaves uninitialized. Frames smaller than the crop retain their full image.
Null pixels, non-positive or oversized dimensions and short source buffers
are rejected before reading pixels. The crop is rebuilt only on a dirty
frame. All three renderers apply fades with a black blended quad and restore
the drawing colour afterward.

## Validation

Executable fixtures check native fade vectors, wraparound, decoder pause,
mute/resume, delayed completion, loop/hold/error/stop behavior, crop contents
and bounds, dirty-frame caching, shader exemptions and the actual client
drawing function. Existing RoQ/RTCW presentation and bulk shutdown tests
pass alongside them: 18 focused Windows tests, including RoQ bounds and
packaged-document validation. The Windows x86 MSVC client build passes.
Seventeen cinematic tests also pass under Ubuntu/Clang, with the malformed
file, crop and RoQ probes enabling address/undefined-behavior sanitizers.
The final full Windows x86 Meson suite passes 120 targets with two conditional
skips; its Python target passes 1,841 tests with 72 conditional skips.

A separate installed-DLL probe pauses the first and last decoded frames of
`st_01`, resumes, and still produces all 75 frames with the same aggregate
pixel hash as the original corpus run. This tests the pause interface without
sound or real-time pacing.

Three windowed GLx runs use isolated writable homes under
`E:\Games\fnTech3-tests`, disabled mouse/joystick input, and the registered
engine `screenshot` command. Each run produces 32 sequential fade
captures and three held-frame quality captures:

- The first frame increases in brightness while held; the final frame
  decreases before the menu returns. After fitting a brightness multiplier,
  mean per-channel residuals in the sampled transitions are below 0.15 on
  the 8-bit scale. Capture times are not synchronized between runs.
- Quality 0 and 1 held images are byte-identical. Quality 2 held images are
  byte-identical across those GLx runs and show the central crop.
- Six further live sessions cover standalone completion and in-game playback
  in GLx. Every standalone movie runs its `nextmap` continuation.
  Every in-game movie reports `cl_paused 1` during playback and
  `cl_paused 0` on return, with the level still connected.

The [evidence manifest](evidence/2026-09-20-stvef-cinematic-presentation.json)
records source and executable identities, native observations, commands and
capture hashes. Logs and engine images remain in
`.tmp/stvef-gaps/bink-presentation-captures` and
`.tmp/stvef-gaps/bink-fade-lifecycle` in the isolated worktree. Proprietary
pixels and movie inputs are not committed.

This establishes the measured fade/crop slice on Windows x86. It does not
establish audible movie parity, complete renderer/effect parity, or broader
installation/platform coverage. The installed `st_21.bik` CRC defect noted
in the original Bink record is unchanged.
