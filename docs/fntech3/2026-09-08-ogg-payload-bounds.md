# Bound Ogg metadata and reject incomplete whole loads

Ogg stream initialization previously narrowed the decoder's sample count before
checking whether decoded PCM fit the engine's signed integer size fields. It
now validates positive rate/channel values, nonnegative sample counts and the
decoded-byte bound before narrowing or multiplying. Every rejection clears
the decoder and closes the file. Empty streams remain representable; whole
loads still reject an empty decode.

Whole-file loading also previously accepted any positive decoded length while
advertising the full metadata size, exposing an undecoded buffer tail. It now
requires exactly the advertised byte count and publishes caller metadata only
after success. Partial positive decoder returns remain supported when they
eventually fill the buffer. Short or failed loads release temporary memory and
leave the caller's metadata unchanged.

The executable regression extracts the production open/read/close/load functions
and injects decoder metadata and read faults. It checks integer boundaries,
negative/zero rate and channel values, decoder setup failures, every truncated
length of a 16-byte fixture, exact successful output and resource cleanup.
The predecessor fails the new rejection case. The corrected test passes on
Windows and under Linux AddressSanitizer/UndefinedBehaviorSanitizer. A missing
standard-library include in the first Linux test harness was corrected before
the successful run.

Both Windows x86 build variants pass. The complete native-Windows suite passes
110 targets, zero failures and one skip. Two private-desktop runtime probes use
the same native client binary with Q3 and QL profiles. An unmodified local QL
Ogg fixture is exercised as a loaded sound and streaming music, through OpenAL
Soft's file writer with no speaker output. Both processes exit zero normally,
produce nonzero post-startup output, and end with a silent final second. This is
valid-input smoke evidence, not exact decoded-waveform or routing proof. The Q3
installation has no Ogg paths; the QL installation has 559. Only one local fixture
was exercised, and no claim is made for the entire Ogg corpus.

Probes use windowed input-disabled clients on private unswitched desktops,
isolated writable homes, disabled online services and the diagnostic front-buffer
control. No input control or OS recording was used. Retail data and raw output
remain ignored, and runtime stage hashes remain unchanged. The
[structured record](2026-09-08-ogg-payload-bounds.json) records source/binary
identities, commands, fixture hashes, output measurements and validation logs.

Ogg callback arithmetic/seek behavior, broader malformed-container coverage and
normal presentation-related audio continuity remain open. No unrelated issue
was found; the callback concerns are within this media audit. No external
implementation was incorporated.
