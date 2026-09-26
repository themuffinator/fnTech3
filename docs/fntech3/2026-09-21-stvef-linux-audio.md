# Elite Force Linux audio output

The Linux i686 Holomatch client renders sound through OpenAL in six isolated
runs: a no-tone control, a synthetic tone at full and half volume, English and
German retail MP3 paths, and a sound restart followed by the tone. All complete
normally with the same production client and GLx module used in the
[Linux client sessions](2026-09-21-stvef-linux-client.md).

The system has only an x86-64 OpenAL runtime. For this probe, the already pinned
OpenAL Soft 1.25.2 dependency is built privately for i686; all 461 upstream
files match the cached archive. Its LGPL terms were recorded in the earlier
[dependency evidence](2026-09-08-audio-restart.md#pinned-x86-openal-soft-follow-up).
No system package, production binary or upstream source changes. This private
runtime contains only null and wave backends, and every run explicitly selects
wave. Runtime logs verify the backend independently of the generic device name
`OpenAL Soft`.

The Wave Writer produces stereo PCM16 at 48 kHz. A one-second, 997 Hz mono
source uses `round(8192 * sin(2*pi*997*i/48000))` for 48,000 samples. After
excluding the first three seconds, which contain menu speech and its decaying
tail, the no-tone control is exactly silent. The other measured results are:

| Case | Observation |
| --- | --- |
| Full-volume tone | 997 Hz peak; identical left/right center samples; normalized peak 0.151245 and center RMS 0.105309 |
| Half-volume tone | 997 Hz peak; center RMS 0.052654; half/full ratio 0.499998 |
| English voice | The engine opens `sound/voice/impannouncer/cin/16/redalert.mp3` and produces nonzero output |
| German voice | The same request with `s_language DEUTSCH` opens `sound/vox_d/impannouncer/cin/16/redalert.mp3` and produces nonzero output |
| Sound restart | Both initializations succeed; the final device opening renders the same measured tone level and frequency |

The centered spectral window is 100 ms with a Hann window, padded to one-Hz
bins. The tone's measured active span is 1.003979 seconds, including the output
tail. This is not a sample-for-sample timing claim. The two voice checks establish
path selection and delivery, not complete-utterance or perceptual equivalence.
The restart file contains the final opening because the writer replaces its
file when the device reopens.

An initial diagnostic run exposed an unrelated logging issue: leaving
`fs_debug 1` during shutdown reopened and truncated `qconsole.log` after it
closed. The final probes restore `fs_debug 0` before quitting and retain stdout
independently. The production logging code is unchanged.

The [manifest](2026-09-21-stvef-linux-audio.json) records the source,
ELF/runtime identities, build commands, configuration, selected paths, output
hashes and measurements. Logs remain in `.tmp/stvef-gaps/linux-audio-*` and the
`E:\Games\fnTech3-tests\linux-audio-*` homes. No proprietary audio is committed.
Only the test engine's own stream is written; there is no microphone, operating
system audio capture, physical playback device or user input control. Windows
x86 remains the SP/Bink boundary. Listening and physical-device gates remain
open.
