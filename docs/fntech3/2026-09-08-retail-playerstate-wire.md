# Bidirectional QL player-state payload checks

This checkpoint extends the [controlled delta-reader probe](2026-09-08-retail-snapshot-producer.md)
to actual Huffman-compressed payloads. A standalone Windows i686 helper compiles
fnTech3's production `msg.cpp` and `huffman_static.c`, writes six sequential QL
player-state deltas and verifies their complete decoded states. The pinned
retail executable's Huffman initializer, bit reader/writer and delta
reader/writer execute under isolated CPU emulation. Bit-reader values are no
longer substituted. The [record](2026-09-08-retail-playerstate-wire.json) identifies
sources, tools, executable hashes, function RVAs, commands and payloads.

The retail Huffman initializer runs its own table-building code; only its
zero-fill helper is controlled. Delta-function probes additionally control the
stack-cookie helper. The existing Unicorn 2.1.4 investigation dependency stays
in ignored local output. Neither retail process startup nor Steam, rendering,
network sockets or user input are exercised. No retail executable bytes or
implementation text are committed.

## Results

The six states cover no changes; mixed signed values, integral/fractional
floats, QL-specific fields and all four array types; an incremental update with
a large fractional float and cleared entries; reset to zero; a rightmove-only
change; and an upmove-only change. The retail decoder accepts all six
fnTech3 payloads with both zero and nonzero prior tails: twelve cases match the
entire expected 504-byte prefix, preserve all 88 tail bytes and retain output
guards. Its consumed bit counts match fnTech3's writer/reader.

The retail encoder also serializes all six states, and fnTech3 decodes every
result into the complete expected state. Four payloads are byte-identical
between writers. Two movement-only payloads differ: the retail writer's
four-byte comparisons at movement-byte offsets can mark adjacent fields
changed, while fnTech3 compares these fields as individual signed bytes. Both
encodings are accepted and produce the same state. This difference is retained
in the record; it is not hidden by claiming universal byte equality.

The two observed retail movement-only payloads now live in
`TestRetailMovementBytePayloads` in the message-codec regression executable.
They are serialized synthetic inputs, not retail game assets or implementation
code. The test checks complete state, exact consumed bits/bytes and guards.
The complete message-codec executable passes under Windows MSVC x86 and Linux
GCC i686, including its existing Q3 checks. No engine implementation changes
were necessary, and no full engine-suite rerun is claimed for this fixture-only
addition.

### Alignment and truncated-input follow-up

At source baseline `2cc39cd`, the regression now relocates each observed
payload's compressed bits to every starting offset from zero through seven,
without using fnTech3's encoder to produce the expected bytes. For each offset,
it tests every shorter byte prefix and the complete framed payload, both with
a separate destination and with baseline/destination aliasing. A distinct
destination sentinel detects premature baseline copying on a rejected input.
All failures must preserve the entire destination and its adjacent guards;
successful reads must match the full expected state and exact final bit offset.

The 392 combinations comprise 32 complete reads and 360 rejected prefixes.
Bytes beyond `cursize` remain physically present in the backing buffer, so the
prefix cases also check that available storage does not override the declared
message length. Compressed framing uses `(finalBit / 8) + 1` bytes, including
the trailing byte at an exact byte boundary, as required by the existing
`MSG_WriteBits`/reader cursor convention. An initial harness using a rounded-up
bit count failed at that boundary; correcting its framing made both runs pass.
This was a test-construction error, not an observed engine defect.

Validation: `cmd /c .tmp\test-retail-wire.cmd` rebuilt and passed the complete
Windows MSVC x86 message-codec test (one Meson test, zero failures); `wsl bash
/mnt/e/Repositories/fnTech3/.tmp/test-retail-wire-linux.sh` rebuilt and ran the
same executable successfully with GCC i686. Existing Q3 cases ran in both.
This follow-up exercises fnTech3's decoder only; it adds no claim about retail
malformed-input handling or live peer interoperability.

## Scope

These are bidirectional **payload** checks against selected retail functions,
not bidirectional live client/server interoperability. Six states do not cover
every bit pattern, malformed packet, protocol variant, demo flow or network
impairment. Tail inheritance is confirmed for these payloads, but other tail
writers and its full meaning remain unresolved. The network and module ABI
gates therefore stay open.

The helper was explicitly built as i686 after its host-default x64 attempt
correctly hit the shared-engine architecture guard. Raw disassembly, emulator
state, local harnesses and intermediate files stay ignored. The record and two
regression vectors contain only synthetic inputs and measured results. No
external implementation was incorporated. No new unrelated engine defect was
established; existing UI/image/font limitations remain unresolved.
