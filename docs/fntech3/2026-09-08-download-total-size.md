# Reject UDP downloads that disagree with their declared size

The client already rejected truncated chunks and short storage writes, but did
not compare accumulated bytes with the first block's declared file size. An
otherwise valid zero-length EOF block could finalize a short file; excess data
could be written and acknowledged, with signed count overflow possible near the
integer limit.

CL_ParseDownload now checks the remaining declared size after rejecting
out-of-order blocks and before opening, writing, acknowledging or finalizing the
file. It rejects excess bytes and premature EOF with ERR_DROP. Subtracting the
validated count from the declared size avoids signed addition overflow. Existing
sequence handling, 16-bit block rollover and complete transfers remain intact.
Both profiles use this parser. No wire layout or server behavior changes.

The expanded executable test compiles the production parser with controlled
message and filesystem boundaries. It covers excess initial and later blocks,
premature EOF, ignored stale excess blocks, integer-limit rejection and exact
completion, alongside existing truncation, short-write and block-order cases.
The old parser fails the new rejection assertion; the fixed parser passes on
Windows and WSL Ubuntu. The malformed cases are executable parser evidence, not
a full client/server malformed-packet transcript.

The Windows x86 rebuild and full suite pass: 110 checks pass, one is skipped,
and none fail. The Python aggregate runs 1,284 tests with five skips.

The rebuilt Windows x86 client also completed the existing loopback UDP fixture
probe for both profiles against their reference dedicated servers. In both runs
the downloaded 32,916-byte fixture pak matches the server SHA-256
`9cc5e33aac0b21cb32c5e618fa137ad0b448acd507120558ebc56310197b3161`,
and the client reaches CS_ACTIVE after download and pure admission. Both client
and server exit normally with no forced termination. Total probe lifetimes were
35.437 seconds for Q3 and 35.625 seconds for QL; these are not transfer timings.
Engine screenshots show Q3 world/HUD and QL native join UI over the world.

Q3 uses the clean pinned FnQuake3 reference at
`7d8c9629cdc7979f09697266470359101ffd9eda`; QL uses the existing FnQL reference
binary whose exact source revision remains unverified. These are client-side
non-regression runs, not new bidirectional or independent-retail evidence. The
same merged client binary was used for both. Clients ran windowed on private,
unswitched desktops with input disabled, isolated writable homes and legitimate
read-only asset installations. The transferred pak contains only generated test
data. Proprietary assets and screenshots remain outside tracked evidence.

The [evidence record](2026-09-08-download-total-size.json) contains commands,
configuration, artifact/fixture hashes, log hashes and observations. No external
implementation was incorporated. This does not close HTTP downloads, archive
integrity, interrupted-file disposal, disk-full runtime behavior or the full
compatibility gate. No additional unrelated issue was found in this slice.
