# Profile candidate path conversion — 2026-09-08

At `72f04a7`, the real filesystem marker probe terminated on Windows when
given an unrepresentable UTF-8 candidate path. Although regular-file queries
already use error codes, `std::filesystem::u8path` can throw before those
queries. The C-facing marker probe now contains conversion system errors and
returns no evidence for that candidate. Allocation failures are not swallowed.

The new executable test links the production profile implementation and
exercises missing/null paths, directory markers, regular-file markers, mixed
Q3/QL markers, a valid Unicode directory, invalid UTF-8 and oversized paths.
It failed on Windows before the patch and passes after it on Windows and
Linux. Linux may represent byte sequences that Windows rejects; the test's
invalid-byte candidate is absent in its isolated fixture on either platform.

The Windows MSVC x86 client/dedicated rebuild, existing profile tests and Steam
path tests pass. The same rebuilt dedicated binary also automatically selects
Q3 and QL against their legitimate installations, loads q3dm6/campgrounds and
shuts down normally using isolated writable homes. The
[startup record](evidence/2026-09-08-profile-path-conversion.json) pins its
identity, commands, configs and module-loading excerpts. No game client,
input control or screenshot capture was used for these dedicated probes.

This does not validate archive content: a regular file with a marker filename
still supplies preliminary evidence, even if it is not a valid pak. That
remaining detection gate is explicit. No new unrelated issue was found.
