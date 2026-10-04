# Audio system audit — 2026-10-02

The shared audio review found and corrected stream loss, truncated music endings,
an unsafe surround fallback, native PCM byte-order inconsistencies, unchecked
numeric conversion, ALSA short-write loss, fade-induced sample drift, and aliasing
during sample-rate conversion. Follow-up probes also corrected non-native Ritual
score conversion, MOHAA's looping/position handling, nonfinite score gains,
raw idle-drain timing and static-source conversion quality.
The [JSON companion](2026-10-02-audio-system-audit.json) pins the checks, build
identities, measurements and limitations. The workspace already contained other
compatibility work; this record covers the audio changes described here.

## Reliability and portability

An OpenAL source left in `AL_STOPPED` reports its queued buffers as processed,
including newly submitted buffers awaiting a deferred start. The production
stream player could therefore reclaim PCM before it played. Clearing a source
now rewinds it to `AL_INITIAL`. A source exhausted during an upload reclaims only
the older buffers, retains the new buffer and rewinds before restarting. A failed
queue query during a clear also resets duration metadata, and nonfinite or
negative stream gains become silence before reaching OpenAL.
The state and buffer semantics are documented in the
[OpenAL Programmer's Guide](https://www.openal.org/documentation/OpenAL_Programmers_Guide.pdf).

Natural music EOF now closes the decoder and lets queued PCM drain. Explicit stop
still cancels playback. A failed interpolation-tail submission retries without
closing the decoder. Music also retains one decoded chunk, at most 32,768 bytes,
until it can queue successfully; it does not advance the decoder again while
that chunk is pending. Empty loops and failed loop opens stop boundedly while
allowing accepted audio to finish. Closing or replacing the stream clears its
pending chunk.

A multichannel queue failure could previously clear already accepted audio
before attempting stereo fallback. That fallback now requires an empty queue.
A busy queue retains its PCM and conversion state for retry. The existing
stereo fallback remains available when a fresh stream's multichannel upload is
rejected. An empty OpenAL queue can still have input held in the resampler's
lookahead; if both uploads fail, that history is restored for retry as well.

WAV and Vorbis already decode PCM16 into native byte order. MP3 now follows that
same internal contract, and the OpenAL converter and compressed legacy loader
no longer swap decoded shorts again. RTCW's paired stream adapter submits native
shorts directly. File decoding retains its own endian conversion, and the retail
module/raw PCM8 signedness rules stay intact. Fault fixtures exercise unaligned
native reads and the big-endian MP3 compile branch; actual big-endian hardware
has not been tested.

Float-to-PCM conversion rejects nonfinite values and clamps before converting to
an integer. PCM8 expansion multiplies by 256 rather than shifting negative signed
values. Both native ALSA write paths advance their ring cursors by frames actually
accepted, including partial writes across wraparound; zero/error returns stop the
current transfer rather than skipping data or spinning.

The Ritual score/cinematic mixer had a separate one-frame EOF loss: exhausting
the decoder stopped the player while its last sample was still in the interpolator.
Decoder exhaustion and playback completion are now separate states, preserved
when a track is parked or moved. The cached final frame drains for its full output
duration across mix calls. Fixtures check empty, one-frame and longer streams at
22.05, 44.1 and 48 kHz, with one-frame and larger mix requests. Its PCM writer also
rejects null output buffers, silences nonfinite values before integer conversion,
and writes native PCM without requiring alignment. Sanitizers reproduced the null
write and NaN conversion before those fixes.

MOHAA's separate score adapter could reopen an empty or completely skipped loop
indefinitely. Its loop restart also discarded the interpolation tail and fractional
phase; saved positions and remaining time followed the decoder's chunk read-ahead
rather than mixed frames. Executable production fixtures reproduced these failures.
Playable loops now preserve conversion history and phase, empty/skipped loops stop,
and natural EOF or a failed/incompatible reopen drains accepted PCM before stopping.
Playback positions subtract read-ahead, including filter support. Triggered loops
restart the whole file; soundtrack loops retain their existing 0.063-second start
offset. The triggered-loop interpretation is supported by the pinned OpenMoHAA
reconstruction, not a retail runtime measurement. Both score PCM writers now
handle null/unaligned output and nonfinite values safely. A nonfinite score gain
becomes silence without poisoning subsequent gain ramps.
Both score mix codecs previously kept returning silent PCM after every track
ended. They now report EOF, allowing the backend to drain accepted audio and stop
idle decoding/conversion. A later song or movie uses the existing reattachment
path. Bounded PCM fixtures reproduced the silent-pump behavior before this fix.

## Quality and efficiency

The previous cubic stream converter had no anti-alias filter. A 30 kHz tone at
96 kHz survived conversion to 48 kHz at approximately full amplitude, becoming
an audible alias. Cubic upsampling also produced an audible image at about -14.7 dB
and lost 19% of an 8 kHz fixture tone's amplitude. Rate conversion now uses an independently written Blackman-windowed
sinc filter, with 128 interpolated phases, normalized coefficients and support
scaled to the rate ratio. The coefficients are immutable and shared by conversion
transactions. They are built once per configuration; processing performs no
trigonometry or table copies per chunk. Interleaved channels share each weight.
Equal-rate PCM remains unchanged. The common mono, stereo and six-channel loops
are specialized, with contiguous support avoiding per-tap bounds checks away from
endpoints; other channel counts retain the generic path.
This is the shared final stream converter and MOHAA's per-track converter. Ritual
now uses it for tracks differing from its 44.1 kHz score mix: the previous linear
converter lost 34% of the fixture tone's amplitude and produced an image at -14.4 dB.
Its observed retail Alice songs match the mix rate and keep native PCM. Start/loop
offsets, gain ramps and parked decoders retain their state. Non-native fixtures
cover tone/image/alias rejection, exact EOF counts, DC, chunking, moves, loops,
playback-based remaining time and recovery from nonfinite gain.

Changing only a raw stream's gain previously flushed and restarted its resampler,
rounding up each segment independently and introducing extra frames during fades.
Gain now belongs to each input sample, with the retained interpolation history
and integer phase preserved. A fixture alternates gain across 100 chunks of 17
frames and verifies byte-identical PCM against the same envelope submitted in one
buffer, at both 48 kHz and 24 kHz output. Format changes still finish their own
segments, and rejected uploads still leave the conversion transaction unchanged.

Raw audio has no explicit EOF. An idle drain previously reset the converter,
rounding each short burst independently even with a constant gain. It now drains
known PCM while retaining phase and left support. A production-update fixture
exhausts the queue between 100 bursts of 17 frames and verifies exact DC PCM and
frame counts against one buffer at both output rates. After draining, 10,000 idle
updates allocate nothing; retained history is not mistaken for pending PCM.

The filter has an intentional transition band below the lower Nyquist limit.
Its cutoff is 95% of that limit, with 64 lobes of support per rate ratio and
a maximum radius of 2,048 input frames. The initial 90%/32-lobe filter halved
the amplitude at 90% of the lower Nyquist limit; a new high-band fixture exposed
this loss before widening it. For the tested 8--192 kHz range, filter lookahead
is about 1.34 ms during 48 kHz downsampling, 1.46 ms at 44.1 → 48 kHz, and at most
8.13 ms for 8 kHz input/output. This is filter support, not end-to-end device latency.
The existing integer phase keeps chunk boundaries and total frame counts exact.
Constant signals and one-frame streams remain exact, and EOF flushes the tail
using clamped endpoint samples. Each input/output chunk is bounded to 4,194,304
float samples (16 MiB) before reading or reserving memory, preventing extreme
sample-rate metadata from requesting multi-gigabyte conversions. The coefficient
table has a separate bound of 2,113,536 bytes.

Spectral fixtures compare 1 kHz and 90%-of-Nyquist passband tones with a tone at
51% of the output sample rate, just above its Nyquist limit. Upsampling fixtures
jointly fit the wanted tone and its image to avoid finite-window leakage bias.
Passband amplitude differs by less than 0.1%; rejection must exceed 60 dB:

| Conversion | Measured alias rejection |
| --- | ---: |
| 96,000 → 48,000 Hz | 90.78 dB |
| 48,000 → 24,000 Hz | 90.77 dB |
| 48,000 → 8,000 Hz | 90.76 dB |
| 44,100 → 22,050 Hz | 90.78 dB |
| 48,000 → 44,100 Hz | 94.08 dB |
| 192,000 → 8,000 Hz | 90.76 dB |

The 8, 16, 22.05 and 44.1 kHz → 48 kHz upsampling fixtures reject the tested images
by 93--125 dB. A 20 kHz tone at 44.1 → 48 kHz has amplitude 0.999749 and its image
is about -93.7 dB. Ritual's quantized MP3 fixtures reject the tested upsampling
images by over 123 dB and the 48 → 44.1 kHz alias by about 85.6 dB. These are
specified synthetic frequencies, not a complete continuous-frequency guarantee.

The output vector reserves its validated chunk size once. In the Windows x86
allocation benchmark, 1,000 transactional chunks of 8,192 frames at
44,100 → 48,000 Hz required 27,003 allocations before and 3,000 after for stereo;
six channels required 30,006 before and 3,000 after, including EOF flush.
Output sample counts matched: 17,832,926 and 53,498,778 respectively.
This is about 89--90% fewer allocations. Timing measurements are observations
from one machine under concurrent workload, not general performance guarantees.
The JSON companion records rate-conversion timings separately.

The JSON companion records each final conversion's initialization and processing
time across 8, 22.05, 44.1 and 96 kHz → 48 kHz and 48 → 44.1 kHz, for stereo and
six channels, including transactional copies and EOF flush. Concurrent builds
affected wall time, so those observations have no machine-independent timing gate.
The reproducible witnesses are `tests/audio/resampler_allocation_benchmark.cpp`
and `tests/audio/resampler_downsample_benchmark.cpp`.

Static samples and pitch-shifted sources use OpenAL's own converter. The measured
OpenAL Soft 1.25.2 runtime defaulted to cubic spline, consistent with its
[upstream default](https://github.com/kcat/openal-soft/blob/1.25.2/core/voice.h).
Its optional API advertises runtime-defined indexes and
[filter names](https://github.com/kcat/openal-soft/blob/1.25.2/al/state.cpp).
The engine now resolves those names and validates the setter/query on a temporary
source before selecting a filter. `s_alSourceResampler balanced` requests the
widest sinc up to 24 taps; `fast` chooses the smallest and `best` the largest.
`library` restores the runtime's queried default, including on reused sources.
Unavailable, malformed or rejected optional support keeps a diagnosed fallback.
This does not pre-convert static assets or disable their HRTF, Doppler or effects.

The x86 mono-float loopback probe compares an 8 kHz, 2.8 kHz tone and its 5.2 kHz
image with native-rate reference PCM. A separate 96 → 48 kHz probe measures the
18 kHz alias of a 30 kHz input. Mixing cost uses 32 voices at 22.05 → 48 kHz for
ten seconds. These observations apply to the specified runtime and frequencies:

| Source filter | Wanted gain | Image rejection | Alias rejection | Mixing wall time / audio duration |
| --- | ---: | ---: | ---: | ---: |
| Library cubic spline | 0.7732 | 15.24 dB | 0 dB | 0.51% |
| Balanced 24-tap sinc | 0.9892 | 70.30 dB | 67.67 dB | 3.25% |
| Best 48-tap sinc | 0.9928 | 87.67 dB | 104.55 dB | 4.71% |

The balanced filter took 12.72% of the audio duration for 128 voices. A real-runtime
regression requires at least 60 dB image/alias rejection and less than 2% wanted
gain error. Older runtimes offering only the narrower 12-tap filter are measured
in its supported band. Fault fixtures cover index reordering, quality tiers,
missing APIs, bad inventories, source allocation/set/query failures, cleanup and
restoring the default on reuse. Source policy is latched until `snd_restart`,
and `s_info` reports the selected filter and library default.

## FnQ3/FnQL features and profile scope

The review preserves the modern OpenAL pipeline: HRTF and output-mode negotiation,
authored stereo and multichannel routing, optional UHJ/B-Format interpretation,
source budgets and pooling, distance attenuation, cached occlusion, per-voice tone
shaping, EFX filters/reverb, audio zones and portal blending, output limiting,
deferred updates, timing diagnostics and device-reopen policy. No feature is
disabled by these corrections and no retail-exact presentation mode is added.
The legacy fallback retains its documented feature limitations.

Shared corrections apply wherever the profiles use these paths. Profile fixtures
cover RTCW music, paired entity streams, speech and loops; ET's sound semantics;
Elite Force's voice/distance rules; Alice's backend and score conversion; MOHAA's
score streams; and common speaker routing.
These are subsystem and synthetic-output checks. They do not establish every
profile's gameplay, listening, cinematic synchronization or physical-device
compatibility, and no compatibility gate is promoted to complete.

## Validation and remaining work

New executable fixtures reproduced the stream, music, byte-order, surround retry,
ALSA, MOHAA loop/position, score gain and spectral failures before their respective fixes. Linux x64 fixtures also
passed AddressSanitizer, UndefinedBehaviorSanitizer and leak checks for the revised
stream, music, PCM, ALSA and resampler paths. The production stream
is additionally compiled into the OpenAL loopback harness: two buffers survive
delayed playback, PCM has nonzero RMS, underruns restart, and two buffer objects
are recycled through 200 restart cycles without OpenAL errors.

A Windows x86 silent-output probe also ran the production stream player on the
WASAPI backend, using `Headphones (Realtek(R) Audio)` at 48 kHz. It completed 100
delayed-start/exhaustion cycles, alternating explicit clears with restart after
natural drain, with two buffer objects, no OpenAL errors and successful device
shutdown. This checks a real playback driver; it does not measure listening
quality, end-to-end latency or unplug/replug behavior. The local probe source and
driver diagnostic log are under `.tmp/audio-review/`.

Final builds, focused regressions, runtime measurements, sanitizer results and
artifact/source hashes are recorded separately in the JSON companion. The final
Windows x86 `/W4 /WX` client build and 83 audio/profile regressions passed.
Windows x86 OpenAL Soft 1.25.2 and Linux x64 1.23.1 each passed 47 loopback checks,
including source conversion. Linux x86 syntax checks cover the shared backend,
ALSA, both score adapters and cvar registration. GCC ASan/UBSan/float-cast-overflow
checks passed for wideband conversion and scores, with subsequent focused reruns
for idle score EOF, raw burst timing/allocation and source selection/fallback.

An earlier complete Meson suite passed 180 registered tests and skipped the
optional retail factory probe. Its recursive source suite passed 2,422 tests,
with 121 platform/environment skips, in 3,067.483 seconds. That run preceded the
final audio corrections and is historical evidence. Two later complete-suite
attempts were stopped to fix the score-idle and raw-idle issues; neither is a
passing gate. Only verified task-owned wrapper/descendant processes were stopped.
The final non-recursive registered rerun is recorded separately; it does not
claim a fresh complete recursive suite or complete profile compatibility.
It passed 202 registered tests and skipped the optional retail factory probe.

Unrelated work changed during the audit. Earlier builds encountered `/WX` failures
in JKA bridge, font-host and string-host tests; a subsequent all-target build
passed after independent corrections. An interrupted suite also failed
`fntech3_application_icons` at its committed-PNG/embedded-frame equality check.
The earlier Alice input source assertion failed before its source/test changed
independently. Final registered outcomes and exact observations are in the JSON.
Ninja repeatedly reported `premature end of file; recovering`, forcing rebuilds.
WSL briefly failed to start but recovered without service/distribution changes.
One Clang sanitizer fixture compilation timed out; the equivalent GCC checks
passed. WSL OpenAL reported missing PipeWire configuration while loopback passed.
No build-cache or host audio configuration was changed by this task.

Hardware unplug/replug, suspend/resume, long running physical output, subjective
speaker/headset listening, all-profile retail acceptance and actual big-endian
hardware remain unverified. Linux x86 OpenAL loopback was unavailable because
that architecture's runtime is absent. No game, fullscreen window, input control
or retail-installation write was used.

Useful logs and reproduction sources are retained under `.tmp/audio-review/`.
Disposable probe binaries and temporary files remain because command execution
was rejected before cleanup, including one explicitly named executable, with
`blocked by policy`. The cleanup verification records the exact project-local
root, link checks and task-process inventory. Reusable performance witnesses
are in `tests/audio/`.

On 2026-10-03, a resumed review confirmed that all 31 recorded audio source and
test fingerprints still match. The shared client and loopback binaries had been
rebuilt by other work; their earlier test identities remain historical evidence.
The current Windows x86 loopback binary separately passed all 47 checks with
OpenAL Soft 1.25.2, with its hash unchanged during the run. The resumed result and
identity are recorded in the JSON companion and
`.tmp/audio-review/windows-loopback-resumed-2026-10-03.log`. Cleanup verification
again found no links or active task processes, but command execution rejected
the exact disposable `downsample-bench.exe` deletion before execution. No new
audio defect or unrelated source defect was found during this resumed review.

The rejection was initially described as automatic approval review, but the
error only establishes a command-policy block. A settings inspection on
2026-10-03 found `approval_policy = "never"`, `sandbox_mode = "danger-full-access"`
and `approvals_reviewer = "user"`. All 35 user rules allow commands, and both the
app's bundled executable and the CLI found no matching rule for the rejected
command. [Official Auto-review documentation](https://learn.chatgpt.com/docs/sandboxing/auto-review)
says that review does not run with approvals set to `never`. The exact source of
the execution-policy rejection remains undetermined; no permission settings or
command rules were changed during this inspection.

No external implementation text was incorporated. OpenAL's documentation and
its maintainer's [resampler discussion](https://openal.org/pipermail/openal/2016-March/000485.html)
were consulted as reference observations; the filter, fixes and fixtures are
project-owned code. The pinned [OpenMoHAA triggered-stream reconstruction](https://github.com/openmoh/openmohaa/blob/ab43a0def6c4feccedae36fdb58c2e007d7e4c35/code/client/snd_openal_new.cpp)
supports whole-file triggered loops. This was an observation; no implementation
text was incorporated from it.
