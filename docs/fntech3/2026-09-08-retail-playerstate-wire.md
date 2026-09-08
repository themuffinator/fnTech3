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
