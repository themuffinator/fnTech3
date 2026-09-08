# Retail snapshot production and delta-reader tail behavior

The installed retail executable supplies stronger evidence for the native
snapshot tail than the [consumer copy alone](2026-09-08-retail-snapshot-layout.md).
Its SHA-256 is
`c926fe9f6c851e00b3b9332e88903ad01f28fdd60454873891c0158f5ded1299`.
The [record](2026-09-08-retail-snapshot-producer.json) identifies selected static
checks and four controlled CPU-emulation cases. The installed executable is
read-only; its startup, Steam integration and rendering are not run.

The snapshot producer at RVA `0xaf570`, identified through its snapshot error
diagnostic and layout, selects a history record with stride `0x298`. It copies
`0x250` bytes from record offset `0x3c` into output offset `0x2c`, then writes
the entity count at `0x27c` and entity data at `0x280`. Its entity limit is 384.
This agrees with fnTech3's envelope and the separately inspected retail consumer.

The snapshot parser calls the player-state delta reader at RVA `0xd66c0`.
Without a prior state, that reader creates a zero-filled 592-byte baseline.
It then copies all 592 bytes of the baseline or supplied prior state to its
destination. The scalar table has 58 entries; even accounting for four-byte
stores at the movement-byte offsets, its highest write ends at byte 482.
Four arrays each contain sixteen four-byte values in ranges 192–255,
256–319, 384–447 and 320–383. These scalar and array destinations lie below
the unresolved range 504–591. Seventeen selected instruction-operand checks
reproduce the relevant sizes, offsets and call targets.

## Controlled execution

An independently written local harness uses [Unicorn CPU emulation](https://www.unicorn-engine.org/docs/tutorial.html)
2.1.4 to execute the pinned delta-reader bytes in isolated memory. The bit-reader
returns controlled values; the zero-fill and stack-cookie helper calls are
stubbed. No retail bytes are embedded in committed source or artifacts.
This is execution of a selected function under controlled dependencies, not a
full retail process or an encoded network-stream test.

Four cases cover a null baseline and a prior state containing a distinct
nonzero tail, each with either no changes or all 58 scalar fields and all 64
array entries updated. Every case reaches its expected return address, consumes
the complete expected bit-reader call sequence, matches the full expected
592-byte result and preserves eight-byte destination guards. A write hook
attributes all writes in bytes 504–591 to the initial baseline copy at RVA
`0xd670b`; no subsequent field update touches the tail in these cases.

The null-baseline cases leave the tail zero. The supplied-prior cases preserve
the nonzero tail exactly. This establishes **inheritance, not unconditional
zeroing**, in the tested retail decoder paths. fnTech3's current zero-tail policy
matches the tested no-prior result, but this does not prove that every retail
history entry stays zero. Other writers, indirect consumers, tail field meaning,
all scalar encodings, malformed packets and complete live flows remain outside
this evidence. The tail must still not be labelled padding or discarded.

No fnTech3 behavior changes in this checkpoint. The module ABI gate remains
open, with the [guarded native probe](2026-09-08-native-snapshot-containment.md)
providing separate containment evidence. No full engine-suite rerun is claimed
for this analysis-only change. Raw disassembly, executable bytes, emulator
dependencies and local harnesses remain ignored; committed output records
observations and hashes. No external implementation was incorporated into the
engine. No new unrelated fnTech3 defect was established; existing UI/image/font
limitations remain unresolved.
