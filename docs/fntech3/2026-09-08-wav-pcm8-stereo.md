# Unsigned PCM8 WAV normalization

PCM8 stereo WAV files now enter the audio pipeline as signed PCM16. Previously
the codec passed their unsigned bytes into the legacy signed stereo raw-sample
convention used by both audio backends. A byte value of 128, meaning WAV silence,
therefore became a large negative sample. The correction stays at the WAV codec
boundary and leaves the game-module/raw PCM contract unchanged.

[Microsoft's PCM format description](https://learn.microsoft.com/en-us/windows/win32/multimedia/devices-and-data-types)
specifies unsigned eight-bit samples with midpoint 128. The independently written
conversion maps each byte with `(value - 128) * 256`. It applies to PCM8 WAVs with
more than one channel, for both whole loads and streams. Mono PCM8 retains its
existing representation, which the backends already interpret as unsigned.
Decoded metadata reports width two, and expanded size is checked before signed
multiplication. No external implementation was incorporated.

The [record](2026-09-08-wav-pcm8-stereo.json) pins source, builds, fixtures,
commands, output hashes and measurements. Four baseline launches establish the
defect in the running client under Q3 and QL: PCM8 stereo silence produces a
normalized center-window mean/RMS magnitude of approximately 0.999969, while the
quarter-amplitude tone measures approximately 0.844260 RMS. Output is written
only through OpenAL Soft's Wave File Writer, with SDL dummy fallback; no physical
speaker receives these signals.

Four corrected launches exit normally. Both silence outputs are exactly zero
after the excluded startup second. Tone output contains the intended intro and
loop frequencies in their separate channels, with interior RMS near 0.1766,
followed by silence after stopping. The same unsigned input bytes are used before
and after. This is measured engine streaming evidence for the OpenAL backend;
the shared PCM16 codec output also avoids the signed-byte interpretation in the
legacy backend, but no new physical or legacy-backend output claim is made.

Executable codec tests cover all 256 input byte values, whole and streamed
conversion, truncation/error cleanup and expanded-size overflow. The previous
header path fails the new normalization regression. Focused Windows tests and
Linux address/undefined-behavior sanitizer tests pass. The Windows x86 client
builds, and the full Meson suite passes 110 checks with one skip and no failures;
its Python target passes 1,300 tests with five skips.

The installed 631 Q3 and 538 QL WAV paths still match the previous decoded corpus
hashes and Python's reader. That corpus includes Q3 add-ons and has no stereo
PCM8 cases, so it establishes non-regression while synthetic files establish
the correction. Documentation packaging/link checks pass.

All clients are windowed on unswitched private desktops, input capture is
disabled, installations are read-only and homes are isolated. No OS audio or
screen capture is used. Raw output/logs remain ignored because Q3 startup may
contain retail audio. Broader multichannel layouts, device behavior and gameplay
sound parity remain open. No new unrelated defect was established; existing
optional-service and UI limitations remain unresolved.
