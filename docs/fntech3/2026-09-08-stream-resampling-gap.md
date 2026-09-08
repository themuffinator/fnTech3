# Stream resampling partition defect

Sample-rate conversion remains an open audio correctness issue. The production
`AudioSystem::ConvertStreamToPCM16` independently rounds each chunk's output
length up and begins interpolation at source position zero. `QueueStreamChunk`
applies it to music and raw PCM without carrying resampling phase or history
between chunks. The path is shared by Q3 and QL.

A private executable harness runs the production converter and interpolation
helpers on ten seconds of stereo PCM16, comparing one complete conversion with
8192-frame chunks (the normal 32768-byte stereo PCM16 music reads). Bounded type,
clamp and format helpers stand in for surrounding engine services. The observed
values are recorded in the [evidence](2026-09-08-stream-resampling-gap.json):

| Input rate | Whole output frames at 48 kHz | Chunked output frames | Extra frames |
| --- | --- | --- | --- |
| 22050 | 480000 | 480002 | 2 |
| 44100 | 480000 | 480029 | 29 |
| 48000 | 480000 | 480000 | 0 |

The 48 kHz control also matches every sample byte. The converted cases differ
in sample values as well as length; the record retains maximum and RMS differences
for the fixed two-tone input. This establishes partition-dependent timing and
phase, not a measured claim about subjective audibility. Simply removing the
extra frames would not restore interpolation history across chunk boundaries.

Four separate current-client runs stream 22.05 and 44.1 kHz stereo extensible
PCM16 intros and loops through OpenAL Soft's 48 kHz Wave File Writer, one of
each under both profiles. Each still exhibits the intended left/right dominant
frequencies, intro-to-loop order and stop-to-silence behavior, and exits normally.
Interior 10 ms RMS windows remain nonzero. Those coarse checks must not be used
to claim phase-accurate conversion: the executable partition comparison exposes
a defect that the dominant-frequency checks do not catch.

The setup follows the [streaming checkpoint](2026-09-08-engine-music-streaming.md):
windowed clients on unswitched private desktops, disabled input capture,
read-only installations, isolated writable homes, wave-only OpenAL output and
SDL dummy fallback. Runtime files remain unchanged. Raw wave files/logs stay
ignored because startup may contain retail audio. No external implementation
was incorporated. Documentation packaging and link checks pass; no engine
changes or full-suite rerun are claimed for this investigation.

Correction requires stream-local phase and sample history, explicit handling of
startup/end, loops and rate/channel changes, and preservation of clear/recovery
and queue-fallback behavior. It must compare arbitrary partitions and one-shot
conversion, retain the equal-rate path, and repeat both-profile runtime checks.
This defect is not fixed in this checkpoint. The compatibility ledger now records
it as an explicit blocker. No additional unrelated defect was established;
existing optional-service and UI limitations remain open.
