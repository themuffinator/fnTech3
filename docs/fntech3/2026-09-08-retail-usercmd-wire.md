# QL user-command movement preservation

The shared decoder previously applied Q3's `-128` to `-127` movement clamp
to QL commands too. Actual retail QL decoding preserves `-128` in all three
movement bytes, including inherited movement values when another command
field changes. fnTech3 now applies this clamp only to Q3 layouts. The
[record](2026-09-08-retail-usercmd-wire.json) pins the source baseline, executable,
probe inputs, observed results and the separate unresolved key-mask difference.

## Direct retail evidence

Read-only inspection locates the keyed command writer at RVA `0xd51a0` and
reader at `0xd54a0` in the same pinned executable used by the
[entity-wire checkpoint](2026-09-08-retail-entity-wire.md). Command fields occupy
offsets 0 through 25 within the 28-byte POD: time, three four-byte angle values,
four-byte buttons, weapon/primary weapon/FOV bytes, then three signed movement
bytes. The last two bytes are padding and are initialized to zero in this probe.

The functions execute in fresh Unicorn x86 address spaces with actual retail
Huffman bit IO and hash-checked prior Huffman initialization state. This probe
substitutes no helper calls. It does not launch retail processes or exercise
Steam, network, input or rendering. Installed files remain unchanged.

There are eight scenarios for each of three keys (`0`, `0x12345678`, and
`0xffffffff`): one changed `-128` movement axis at a time; all three axes;
inherited `-128` axes in an otherwise unchanged command; inherited axes with
changed angles, buttons and weapon/FOV fields; a signed timestamp wrap; and
a 256-millisecond timestamp delta. All 24 writer outputs are byte-identical
between fnTech3 and retail. The retail reader preserves all supplied movement
bytes in every case. Packet and output guards remain intact.

Before the fix, fnTech3's local decoder changed movement bytes in 15 of the
24 cases. Afterward, Windows x86 and Linux i686 helpers decode all 24 measured
retail payloads into the original synthetic inputs. One retail decoder result
still differs from those inputs in other fields, as detailed below; this is
not presented as complete bidirectional command parity.

## Regression and validation

`TestUserCommandMovementMinimum` checks both changed and inherited minimum
movement bytes under Q3 protocols 68/71 and QL protocol 91. It expects Q3's
existing clamp and QL preservation. It also decodes a five-byte retail-generated
fixture without using the engine encoder as an oracle. That fixture changes
all three axes to `-128`, consumes 32 bits and uses five framed bytes.
The new regression failed before the production fix and passes afterward.

The complete message-codec executable passes on Windows MSVC x86 and Linux
GCC i686. The Windows client/dedicated incremental build and complete automated
suite are recorded in the companion JSON. Standalone helpers are
`.tmp/ql-usercmd-corpus.cpp` and `.tmp/emulate-ql-usercmd-corpus.py`; raw outputs
and executable inspection remain ignored. Only independently authored code,
synthetic fixture bytes and measured facts are committed.

The first full Windows run passed 109 tests, skipped one, and failed the Python
regression group because five packaging checks encountered this report before
its companion JSON existed. After completing the evidence pair, the entire
Python group passed all 1,292 tests with five skips. Together, the original
run and that rerun give 110 passed, zero remaining failures and one skipped
Meson test. No engine code changed between those runs. Q3's three clamps were
also checked against the pinned FnQuake3 commit, not its dirty worktree.

## Open key-mask discrepancy

This was open at this checkpoint. The subsequent
[delivery and key-mask correction](2026-09-08-retail-usercmd-key-mask.md)
resolves the observed decoder difference after tracing it to module output.

With key `0xffffffff`, target timestamp 1 and changed angle/button values
`0xffff`, the retail reader produces `0x1ffff` in all four four-byte fields.
fnTech3 produces `0xffff`. The observed retail reader uses mask constants
`0x1ffff` for these 16-bit reads and `0x1ff` for byte reads; the latter extra
bit disappears when stored as a byte. This was independently observed in the
executed result, not inferred solely from the constants. All 24 writers still
emit identical bytes, and the other 23 retail-decoded states match the supplied
inputs completely.

The current correction leaves key-mask behavior unchanged. Trace the retail
caller's key construction and downstream field handling before resolving that
separate contract discrepancy. The ledger records it as open. The corpus also
does not cover backward timestamps, every key, malformed retail reads, live
command batches or gameplay interoperability. Required compatibility gates
remain open. No unrelated engine defect was established in this work.
