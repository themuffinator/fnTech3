# Reject truncated download chunks before file writes

`CL_ParseDownload` previously called `MSG_ReadData` and immediately proceeded
to signature checks, file writes and `nextdl` acknowledgements. The production
reader marks a truncated read past the message end and zeroes the unread
suffix. The outer server-message parser detects the overread only after the
download handler returns, allowing partial data to reach the temporary file
and advancing download state first.

The handler now checks the message read boundary immediately after reading
the chunk. A truncated body drops the connection before opening/writing the
file, acknowledging the block, incrementing its count, renaming the archive
or starting the next download. This guard applies to both profiles and does
not change their wire encodings. Initial size/UI metadata and recording state
can already have changed while reading the header; this is not a claim that
the entire handler is transactional.

## Regression scope

`tests/download_chunk_tests.py` compiles the production handler against a
controlled byte transport preserving the message reader's overread sentinel
and zero-fill behavior. It reproduced the missing rejection before the fix
and passes on Windows and Linux afterward. It covers every incomplete prefix
of a four-byte body for first and subsequent chunks, complete data, EOF,
stale blocks and the 16-bit block-number wrap under both selected profiles.
File operations, reliable commands and completion calls are instrumented to
assert that rejected chunks do not perform them. This is handler coverage,
not a new Huffman codec or captured malformed-network transcript.

The implementation and test are independently authored; no external source
text was incorporated. Runtime and suite results are recorded in the
[evidence manifest](2026-09-08-download-chunk-bounds.json).

The shared Windows x86 client rebuild passes. The complete Meson suite passes
110 tests, zero failures and one unavailable OpenAL-loopback skip; the aggregate
Python suite runs 1,255 tests with seven skips. A fresh Q3 loopback session
downloads the temporary add-on byte-for-byte, passes pure admission and renders
world/HUD gameplay. QL's pure non-regression session renders its native join UI
over the world. Both clients and servers exit normally. These probes use
isolated writable homes, windowed GLx and disabled input capture on an
unswitched private desktop; screenshots use the engine command. No retail
fixture content is committed or published. No malformed packet was injected
into these live sessions.

Remote fault injection, interrupted-transfer restart behavior and complete
retail-peer coverage remain open. Existing unrelated image/font warnings
remain unresolved.
