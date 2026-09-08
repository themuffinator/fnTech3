# QL player-state field corpus against the retail codec

At engine commit `f74e5df`, 478 synthetic state transitions match in both
directions between fnTech3's production codec and selected functions from the
pinned retail QL executable. The [machine-readable record](2026-09-08-retail-playerstate-field-corpus.json)
pins identities, corpus digest, helper hashes, counts and byte-different cases.
This extends the [six-state payload checkpoint](2026-09-08-retail-playerstate-wire.md).
It is function-level differential evidence, not live client/server interop.

## Corpus and independent expectations

The corpus uses the 58 retail field descriptors observed at RVA `0x1424d8`
in the [snapshot producer investigation](2026-09-08-retail-snapshot-producer.md).
Every isolated case starts with a zero 504-byte baseline and changes one field.
There are 222 scalar cases: unsigned fields use one, their maximum and high bit;
signed fields use minus one, minimum and maximum. Float fields use `1.25`,
`-4096`, `4095`, `4096.25`, `-40000.5`, `1e-30` and `1e30`, covering compact
boundaries and full finite encodings. Movement values occupy their one-byte
storage; other scalar values occupy four bytes.

Another 128 cases exercise all 16 slots in each of stats, persistant, ammo and
powerups, using the two width-boundary values per slot. The first three arrays
use signed 16-bit minimum/maximum; powerups use all 32 bits set and the high bit.
Finally, 128 sequential cases use Python's `Random(20260908)`: alternating
90%/20% scalar selection and 30% array-slot selection, with values selected
from the same boundary sets plus zero. Each transition inherits its predecessor.
These are synthetic representable states, not a claim that every combination
is a valid gameplay state. NaN and infinity are outside this corpus.

A standalone Windows i686 helper compiles production `msg.cpp` and
`huffman_static.c`. It serializes each input and decodes it locally; all 478
complete decoded states equal the original synthetic input bytes. Thus the
retail comparison does not silently accept a local round-trip normalization.

## Retail execution and results

The retail reader and writer execute in fresh Unicorn x86 instances for each
case. They use real retail Huffman/bit IO and the hash-checked initializer state
produced by the prior checkpoint. Only the zero-fill and stack-cookie helper
calls are controlled. No retail process is launched and no network, Steam,
input or rendering path is involved. Installed files remain unchanged.

The retail reader accepts all 478 fnTech3 payloads with both zero and patterned
88-byte prior tails: 956 runs match the full expected prefix, preserve the tail
and output guards, and consume the exact number of bits written by fnTech3.
The retail writer serializes all 478 original target states with intact packet
guards. Windows x86 and Linux i686 builds of the fnTech3 helper decode every
retail payload into the original complete target state.

There are 431 byte-identical payloads and 47 byte-different payloads. State
equality, rather than identical choice of change flags, is the acceptance
criterion. The record retains the differing case labels. The previous checkpoint
established one source of such differences in retail movement-field comparisons;
this run does not claim to have separately attributed each difference.

## Permanent regression coverage

`tests/fixtures/ql_retail_player_fields.inc` retains 58 measured retail payloads,
one per scalar field: unsigned maximum, signed minimum, or float `4096.25`.
`TestRetailPlayerFieldBoundaries` checks the complete expected state, exact bit
and byte consumption, and adjacent guards. Expectations use observed storage
offsets and value bits independently of the engine's field table or encoder.
The fixtures contain synthetic serialized data only; no retail implementation
text, executable bytes, assets or credentials are incorporated.

The complete codec regression executable, including existing Q3 coverage,
passed with Windows MSVC x86 and Linux GCC i686 after adding these fixtures.
Commands were `cmd /c .tmp\test-retail-wire.cmd` and
`wsl bash /mnt/e/Repositories/fnTech3/.tmp/test-retail-wire-linux.sh`.
The standalone corpus helper was built by `.tmp/build-ql-field-corpus.cmd`,
then run with `verify-retail`; Linux built and verified it through
`.tmp/ql-field-corpus-linux.sh`. Investigation helpers and raw output remain
in ignored `.tmp/`; their hashes and the ordered corpus digest are recorded.
No engine implementation changed and no full-suite rerun is claimed.

The network and module ABI gates remain open: live peer interoperability,
other snapshot-tail writers, tail semantics, malformed retail input behavior,
and exhaustive protocol/demo coverage are not proved by this corpus. No new
unrelated engine defect was established.
