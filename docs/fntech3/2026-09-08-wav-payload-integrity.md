# WAV payload integrity and retail corpus

The shared WAV codec now rejects incomplete whole-file sample payloads.
Previously PCM8/16 returned an allocated buffer even when the file read was
short, exposing unread bytes. PCM24/float32 instead synthesized zeroes for
missing samples and reported a successful load. Both paths now return null
and release their temporary allocations and file handle on incomplete reads.

PCM streams now advance and report only bytes actually read. Previously they
reported the requested count at short reads or EOF. Nonpositive requests return
zero before file access in both direct and converted stream paths. Conversion,
valid-file output and profile selection are unchanged. This codec is shared by
Q3 and QL; no new profile-specific behavior is introduced.

The [record](2026-09-08-wav-payload-integrity.json) pins the source, executable,
tests and log hashes. The executable regression compiles the production load,
stream and conversion functions with bounded file/allocator adapters. It covers
8-bit and 16-bit PCM, 24-bit PCM and 32-bit IEEE float, mono and stereo, every
payload truncation of an eight-frame fixture, read errors, repeated streaming,
EOF, invalid requests and resource cleanup. Header parsing is substituted in
this focused regression. The old implementation fails it; restoring only the
old stream function also fails. The corrected test passes on Windows and Linux.

An additional private harness uses the production header parser and whole-load
function with memory-backed filesystem/allocator adapters. All 631 Q3 and 538
QL WAV paths selected from the installed PK3 archives decode byte-for-byte like
Python's independent WAV reader. Q3 contains 608 mono PCM16, 17 stereo PCM16 and
six mono PCM8 files; QL contains 538 mono PCM16 files. The structural scan found
no short RIFF headers, wrong RIFF/WAVE identity or chunk payloads extending past
their file. Duplicate names use the last lexically sorted PK3; this inventory
does not prove the engine's search-path order. No proprietary samples or decoded
sound are committed. Corpus bytes remain in process memory; retained artifacts
contain independently written harnesses, counts and hashes. The harness required
binary Windows stdio and a larger bounded allocation limit for Q3 music before
passing; those preliminary failures were harness constraints.

The Windows x86 release client rebuilt successfully. The complete Meson suite
passed 110 checks with one skip and no failures; its recursive Python target
passed 1,297 tests with five skips. Documentation package/link validation also
passes. Linux evidence here is the focused executable regression, not a fresh
full engine build. Neither the corpus probe nor the tests establish audible
output, backend/device behavior or big-endian correctness.

A separate header audit found unchecked integer/header reads, missing RIFF/WAVE
identity validation and unchecked chunk seeks/padding arithmetic. Those remain
open malformed-input work and are not repaired by this payload correction.
The asset/audio gate stays partial. No external implementation was incorporated.
No other new unrelated defect was established; existing optional-service and
UI limitations remain open.
