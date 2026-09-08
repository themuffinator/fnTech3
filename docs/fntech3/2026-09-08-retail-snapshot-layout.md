# Retail cgame evidence for the native snapshot envelope

Read-only inspection of the installed API-8 `cgamex86.dll` strengthens the
provenance of the existing native snapshot layout. The loose DLL and the member
in retail `baseq3/bin.pk3` are byte-identical, with SHA-256
`310542161ae03cc09a2edf7c5933a3cb5e2f13d791f59d5a33a62721bbf39953`.
The [record](2026-09-08-retail-snapshot-layout.json) pins the module, tool versions,
instruction RVAs, observed values and limits. No retail module is executed,
modified or redistributed by this inspection.

The exported entry stores the incoming import-table pointer. Tracing that
pointer to the call at RVA `0x4c44a` identifies import slot 87, the snapshot
import used by fnTech3. Its output pointer alternates between two static buffers
whose addresses differ by `0x16488`, matching the adapter's total envelope.
This is direct evidence of buffer separation; it is not, by itself, proof of
every field within either buffer.

A copy at RVA `0x44729` takes the current snapshot pointer, advances it by
`0x2c`, and copies `0x94` doublewords into working player-state storage. Thus
the retail consumer copies all `0x250` (592) bytes, including the 88 bytes after
fnTech3's frozen 504-byte prefix. Separately traced accesses to the current
snapshot use entity count at `0x27c`, entity storage at `0x280`, and a `0xec`
entity stride. All match the current native adapter. Sixteen selected
instruction-boundary checks reproduce these observations; broad linear scans
were only candidate locators, not the basis for identifying arbitrary offsets
as snapshot fields.

This does **not** resolve the tail's meaning. Copying bytes does not establish
their field names, initialization, downstream indirect use or wire encoding.
An exploratory absence of direct absolute accesses to the working tail is not
evidence that it is unused. fnTech3 therefore retains the complete envelope,
explicitly marshals the known prefix and zeroes the unresolved tail under its
existing containment policy. That policy remains bounded compatibility evidence,
not a claim that zeroes match all retail states. The next unresolved question is
the retail producer's values and any indirect consumers of those copied bytes.

The [guarded runtime probe](2026-09-08-native-snapshot-containment.md) remains
the separate evidence for fnTech3's output bounds and zero-tail behavior. This
checkpoint makes no new runtime, gameplay, pure or interoperability claim, and
does not close the module ABI gate. No engine behavior changes or full-suite
rerun are required for these provenance annotations. Raw disassembly and the
local verification script remain ignored; the committed record contains facts,
not retail implementation text. No external implementation was incorporated.
No new unrelated defect was established; existing UI/image/font limitations
remain open.
