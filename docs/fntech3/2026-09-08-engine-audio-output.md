# Engine audio output and volume control

The same Windows x86 fnTech3 client now has measured audio output evidence for
both profiles. Six isolated launches cover a no-tone control, a synthetic tone
at full volume and the same tone at half volume for each profile. All exit
normally with code zero. Runtime files remain identical before and after all
six runs. The [record](2026-09-08-engine-audio-output.json) pins binaries,
commands, configs, fixture/output hashes and measurements.

A one-second 997 Hz mono PCM16 WAV is written only to each isolated home path.
The engine's existing `play` command reads it through the filesystem and WAV
codec and sends it through the active OpenAL backend. OpenAL Soft's Wave File
Writer records the rendered stereo output at 48 kHz. Its private configuration
selects only the wave backend; logs confirm that device in every run. No speaker
or microphone capture is used. The later half-volume runs additionally select
SDL's dummy driver as a fallback guard. The DLL matches the previously pinned
[source-build evidence](2026-09-08-audio-restart.md#pinned-x86-openal-soft-follow-up).
No library or external implementation is added to the engine or release stage.

Both no-tone controls are exactly silent after the first second. Q3 has a brief
startup transient before that interval; its raw output is retained privately,
not reclassified as silence. The tone begins after the excluded interval in
both profiles. Both outputs have a 997 Hz spectral peak, approximately 1.004
seconds above the analysis threshold, identical left/right samples in the
measured center window, and a normalized peak of 0.151245 at full volume.
The small duration extension includes the runtime's output tail; it is not
claimed as exact one-second sample-for-sample reproduction.

Center-window RMS is approximately 0.105309 per channel at full volume and
0.052654 at half volume. Half/full ratios are 0.499998 for Q3 and 0.499998 for
QL. This establishes gain response and close profile agreement for the bounded
synthetic case. The recorded method excludes the first second, detects frames
above 0.0001, and uses a centered 100 ms Hann-window spectrum padded to one-Hz
bins. It does not establish complete waveform identity across launch timings.

All clients were windowed on unswitched private desktops, with mouse/joystick
capture disabled and isolated writable homes. Installations were read-only.
The audio file is produced by the audio runtime, not OS recording. No rendering
claim or OS screenshot is involved. Raw logs and rendered output stay ignored,
as the Q3 startup transient may contain retail audio; only facts and hashes are
committed. The synthetic source formula is recorded for reproduction.

This is engine filesystem/decoder/backend output evidence, beyond the earlier
standalone decoder corpus and OpenAL loopback harness. It does not exercise
gameplay/module sound traps, positional audio, streaming music, occlusion,
resampling, restart continuity or physical-device behavior. Those gates remain
open. No engine behavior changed and no full-suite rerun is claimed for this
runtime-only checkpoint; documentation packaging/link checks pass. No new
unrelated defect was established. Existing optional-service, image and UI
limitations remain open.
