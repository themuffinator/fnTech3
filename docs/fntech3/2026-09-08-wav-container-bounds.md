# WAV container bounds

The shared Q3/QL WAV reader now rejects missing RIFF/WAVE identity, short
container/chunk/format reads and chunks outside the declared container or
physical file. It no longer decodes uninitialized integer fields after short
reads or adds padding to an unchecked signed chunk length. Parsed metadata is
published only after the complete supported header succeeds. Both whole-file
loads and streams pass their opened file length into the parser.

The bounded reader checks every payload length before narrowing it to the
filesystem's signed counts. Padding is checked before advancing to another
chunk. Loose-file seeks return zero on success, whereas ZIP seeks return the
requested offset and may conceal a short underlying read. The parser accepts
both conventions and verifies the resulting file position. This avoids imposing
loose-file return conventions on packed assets.

A final data payload need not contain an unused padding byte. The first strict
corpus attempt rejected an installed add-on's `sound/misc/windfly.wav`; further
inspection found the same shape in four stock Q3 `pak0.pk3` sounds: buzzer,
drone6, jumppad and suck1. The corrected parser requires their complete declared
sample payload but does not skip beyond it. Synthetic fixtures preserve that
acceptance and reject a missing pad when an unknown chunk must be skipped.

The [record](2026-09-08-wav-container-bounds.json) pins code, tests, binaries,
logs and corpus hashes. Executable tests run production parser functions over
valid PCM/float/extensible fixtures, every truncation of a basic WAV, short reads,
invalid identities, impossible RIFF/chunk lengths, unsupported scalar values,
data outside the RIFF container, empty data, odd extension/unknown chunks and
both filesystem seek conventions. They also check that rejected parsing leaves
output descriptions unchanged. Removing identity or chunk bounds makes the
regression fail. The recorded mutation limitation makes clear that these are
not exhaustive fault-detection tests.

Both focused header and payload tests pass on Windows and Linux, including
Linux address/undefined-behavior sanitizers. The Windows x86 client rebuilt,
and the complete Meson suite passed 110 checks with one skip and no failures.
Its Python suite passed 1,298 tests with five skips. Documentation packaging and
internal link checks pass. This is an incremental build, not a new clean-build
or audible gameplay claim.

All 631 installed Q3 and 538 installed QL WAV paths still decode byte-for-byte
like Python's independent WAV reader, with exactly the previous checkpoint's
decoded corpus hashes. The Q3 selection contains 613 stock-pak paths and 18
add-on paths. This clarifies the earlier [payload checkpoint](2026-09-08-wav-payload-integrity.md)'s
broad retail-corpus wording: it scanned the installed directory, including its
add-ons. Duplicate paths use the last lexically sorted archive, not an assertion
about engine search-path order. No proprietary input/output samples or external
implementation are committed; the harness uses memory-backed file/allocator
adapters around the production parser and loader.

Extensible GUID/cbSize/valid-bit semantics remain inherited and require further
format-policy work. Broader malformed formats, streaming CRC behavior, audible
backend/device validation and big-endian runtime remain open. The asset/audio
gate stays partial. No additional unrelated engine defect was established;
existing optional-service and UI limitations remain unresolved.
