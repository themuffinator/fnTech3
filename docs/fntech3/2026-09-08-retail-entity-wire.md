# Retail QL entity-state wire comparison

At engine commit `a8ac4f4`, fnTech3 and the pinned retail QL entity encoder
produce identical payloads for all 402 synthetic cases in this checkpoint.
Both decoders produce the expected complete entity state for all 400 cases
that emit a payload. The two no-output cases remain empty. The
[record](2026-09-08-retail-entity-wire.json) pins executable and source hashes,
function RVAs, observed field descriptors, corpus identity and outcomes.

## Retail observations

Read-only inspection identifies the entity writer at RVA `0xd5780` through
its entity-number diagnostic. Its field comparisons and corresponding reader
at RVA `0xd5ac0` reference a 58-entry table at RVA `0x142220`. Each descriptor
has a name pointer, byte offset and bit width, occupying twelve bytes. The
observed offsets cover every four-byte word from offset 4 through 232 exactly
once. Entity number is separate and uses ten wire bits. The reader copies
`0x3b` dwords for unchanged state and zeroes `0xec` bytes before setting
entity number to `0x3ff` on removal. These observations agree with the
236-byte entity boundary exercised here.

The probes run actual retail entity and Huffman bit reader/writer code under
Unicorn 2.1.4, using the hash-checked initialization state from the
[player-state checkpoint](2026-09-08-retail-playerstate-wire.md). Each case and
direction gets a fresh emulated address space. Only the zero-fill helper is
controlled, and it must request exactly 236 zero bytes. No retail process,
Steam, network, rendering or user input path is exercised. Local installed
files remain unchanged; executable bytes and disassembly stay in ignored output.

## Corpus and results

The 270 isolated field cases start with a zero state except entity number 17.
Each of the 58 fields takes its unsigned width boundaries, signed boundaries
where applicable, or the seven finite float samples `1.25`, `-4096`, `4095`,
`4096.25`, `-40000.5`, `1e-30` and `1e30`. Another 128 sequential cases use
Python `Random(20260908)`, alternating 90% and 20% selection of fields from
those boundary sets plus zero. Four final cases cover forced unchanged state,
unforced unchanged state, removal and both state pointers null.

A standalone Windows i686 helper builds fnTech3's production `msg.cpp` and
`huffman_static.c`. Its decoded states match the original synthetic inputs;
removal instead matches a zero state with entity number 1023. The retail
decoder reads entity number through the actual retail bit reader before its
delta function consumes the remaining payload. All 400 decoded outputs match
all 236 bytes, retain their guards and consume the same bit count as fnTech3.
The unforced unchanged and both-null cases emit no bytes and are not passed
to either decoder.

The retail writer's 402 outputs are byte-identical to fnTech3's, with intact
packet guards. Windows x86 and Linux i686 versions of the fnTech3 helper also
decode the retail outputs successfully. These are function-level comparisons,
not observations of a live snapshot exchange.

## Regression coverage and validation

`tests/fixtures/ql_retail_entity_fields.inc` preserves one retail-generated
boundary packet for each scalar field. Unsigned fields use their maximum,
signed fields their minimum, and floats use `4096.25`. The fixture expectations
specify observed storage offsets and value bits independently of the engine's
field table and encoder. `TestRetailEntityFieldBoundaries` checks entity number,
complete state, exact bit/byte consumption and adjacent guards.

The complete message-codec executable passes Windows MSVC x86 and Linux GCC
i686 with the new fixtures and existing Q3 tests. Commands were
`cmd /c .tmp\test-retail-wire.cmd` and
`wsl bash /mnt/e/Repositories/fnTech3/.tmp/test-retail-wire-linux.sh`.
The corpus helper was built through `.tmp/build-ql-entity-corpus.cmd` and
`.tmp/ql-entity-corpus-linux.sh`; `verify-retail` selects measured retail
payloads. The emulation harness is `.tmp/emulate-ql-entity-corpus.py`.
Investigation helpers and raw outputs remain ignored, with hashes recorded.

The committed fixtures are synthetic serialized data, not incorporated retail
implementation or game assets. No engine behavior changed and no full-suite
rerun is claimed. Malformed retail input, every bit pattern, all entity numbers,
demo flows and live bidirectional peer interoperability remain outside this
checkpoint. Required compatibility gates stay open. No new unrelated engine
defect was established.
