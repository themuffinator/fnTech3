# PCM16 endian conversion across all WAV channels

The shared WAV loader previously multiplied its swap count only for stereo.
A six-channel, seven-frame PCM16 load therefore swapped seven scalar samples
instead of all 42 on a big-endian host. Its streaming path also rounded reads
down to complete interleaved frames: a stereo read of three scalar samples
swapped only the first two.

The loader now includes every channel. The stream reader swaps every complete
16-bit scalar sample returned, including samples within a partial final frame.
The existing little-endian early return and 8-bit behavior remain unchanged.
Header validation already bounds decoded byte counts before this multiplication.
This is a shared codec correction for both profiles, with no module ABI change.

Executable regressions extract the production helper and load/stream functions.
They simulate both endian branches; cover 1, 2, 3, 4, 6 and 8 channels, zero and
nonzero frame counts, and sentinel boundaries; and expand payload truncation and
conversion checks to multichannel sources. The original helper fails the new
regression. A separate mutant restoring only the old frame-rounded stream count
also fails. No external implementation was incorporated.

This is fixture evidence, not a complete big-endian engine build or hardware
runtime claim. Odd-byte short reads that divide an individual PCM16 sample are
outside the endian reconstruction proof. Extensible valid-bit/channel-mask
semantics and broader audio/device runtime gates remain open. No new unrelated
issue was established in this checkpoint.

Windows x86 builds and passes the complete configured suite: 110 targets, zero
failures and one skip, including 1,301 Python tests with five skips. The three WAV
regressions also pass under Linux AddressSanitizer and UndefinedBehaviorSanitizer.
The [structured evidence](2026-09-08-wav-multichannel-endian.json) records source and log hashes.
