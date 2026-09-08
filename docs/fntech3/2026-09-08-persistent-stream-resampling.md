# Persistent stream resampling

The shared OpenAL music/raw conversion path now carries integer resampling phase
and cubic sample history between chunks. It fixes the [partition defect](2026-09-08-stream-resampling-gap.md):
converting ten seconds from either 22.05 or 44.1 kHz now yields exactly 480,000
48 kHz output frames, with identical sample values regardless of chunk size.
The 48 kHz equal-rate path preserves input samples exactly.

A bounded resampler retains the previous cubic sample and pending lookahead.
Phase is an integer numerator relative to that retained history, preventing
per-chunk ceiling drift and avoiding a growing whole-stream counter. Fractional
positions wait for the required future samples; finishing a segment clamps only
the true endpoint. Stream state is separate for music and raw PCM. The existing
PCM interpretation, channel conversion and gain operation remain at their
boundaries. No external implementation was incorporated.

Queue submission commits new conversion state only after success. Failed flushes
retain the pending tail for retry. Input-rate, width, channel, gain or requested
layout changes finish the previous conversion segment before starting another.
A changed device output clock clears the incompatible OpenAL queue. Stereo
fallback retains the resampling timeline and remains selected for that segment,
instead of repeatedly rejecting the same multichannel layout and clearing it.
Explicit stop/clear/shutdown discards retained samples even when the device is
unavailable. RawSamples supplies no EOF callback, so its short interpolation tail
is drained when submitted buffers become idle. Timing at real hardware/device
failure boundaries still needs broader validation.

The [record](2026-09-08-persistent-stream-resampling.json) pins source, tests,
build and runtime hashes. Executable tests compare one-shot conversion against
1/2/3/7/257/8192-frame partitions across five input rates, three output rates and
mono/stereo/six-channel inputs. They check endpoints, empty finish, invalid
configuration, reset, bounded history, gain/layout transitions, queue failures,
retry, device-clock changes and stereo fallback. Resetting phase at chunk
boundaries makes the regression fail. Downmix/device services are bounded test
adapters; the production resampler and queue/flush functions execute in the tests.

Both tests pass on Windows and Linux, including Linux address/undefined-behavior
sanitizers. The Windows x86 release client builds, and the full Meson suite passes
110 checks with one skip and no failures. The recursive Python target passes
1,300 tests with five skips. This is an incremental build; focused Linux testing
is not a new complete Linux engine build.

Eight current-client launches also exit normally with unchanged runtime files.
Both profiles stream 22.05, 44.1 and 48 kHz intros/loops into the Wave File Writer,
retaining intended channel frequencies and stop-to-silence behavior. Two further
runs change from a 44.1 kHz intro to a 22.05 kHz loop, stop, and replay in the same
process. Each output contains both expected intro/loop sequences and final
silence. The file-output analysis verifies those runtime behaviors; exact
partition invariance comes from the executable comparison, not coarse FFT bins.

Clients remain windowed on unswitched private desktops with input capture disabled,
read-only installations and isolated writable homes. Wave-only OpenAL output and
SDL dummy fallback avoid speaker output. No OS audio capture, microphone capture
or screenshot is used. Raw output/logs remain ignored because startup can contain
retail UI audio. Documentation packaging/link checks pass.

The identified same-format chunk phase/count defect is corrected. This does not
close physical-device behavior, prolonged stress, arbitrary layout transitions,
all cinematic/raw producer lifecycles, performance or broader gameplay-audio
parity. The audio gate remains partial. No new unrelated defect was established;
existing optional-service and UI limitations remain open.
