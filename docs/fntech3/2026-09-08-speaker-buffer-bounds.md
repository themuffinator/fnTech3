# Bound loaded-sound speaker conversion

Review of the [speaker-routing change](2026-09-08-wav-speaker-routing.md) found an
OpenAL metadata overflow path. The mixer rejects frame counts whose stereo output
would exceed signed byte sizes, but its caller still evaluated `info.samples * 4`
before handling the empty result. The caller now derives size from the returned
vector, whose size is already bounded. The legacy loader uses the same rule.

Loaded-sound routing also bypassed the ordinary PCM converter's buffer-range
checks. A shared wrapper now validates byte length, offset, sample width and
frame/channel counts before adding the offset to the source pointer. It checks
required capacity using division, without an overflowing intermediate product.
Negative offsets, offsets past the end, truncated frames and oversized counts
return an empty result through existing load-failure handling. Stream conversion
and speaker gains are unchanged.

An executable regression extracts the actual OpenAL metadata-routing fragment.
The original committed fragment fails its oversized-count case. The corrected
fragment and shared wrapper cover oversized counts, short buffers, invalid
widths/offsets, and valid nonzero offsets. Synthetic metadata exercises the size
limit without allocating a multi-gigabyte file. This is bounds evidence, not an
observed retail-asset failure or an allocation-exhaustion runtime test.

Both updated Windows x86 profiles still play a loaded front-center sound with
equal left/right signal, stop to a silent final second and exit zero normally.
Runtime files remain unchanged. Probes use private unswitched desktops, windowed
input-disabled clients, isolated homes and OpenAL Soft file output. No speakers,
OS recording, input control or retail installation writes are used. Raw output
and logs remain ignored. Legacy DMA output and hardware surround remain outside
this checkpoint. No external implementation was incorporated or unrelated issue
discovered.

The Windows x86 build passes 110 configured targets with no failures and one
skip, including 1,303 Python tests with five skips. Five speaker/WAV executable
regressions pass Linux ASan/UBSan. The [structured evidence](2026-09-08-speaker-buffer-bounds.json)
records source, binary and log hashes, commands and measured output.
