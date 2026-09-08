# QL keyed commands at the game-module boundary

fnTech3 now matches the retail QL key mask for changed user-command fields.
QL includes one key bit beyond the serialized width: bit 16 survives in the
four-byte angle/button destinations, while the extra bit for eight-bit values
is discarded by byte storage. Q3 retains its existing width-limited mask.
This resolves the decoder discrepancy reported by the
[movement checkpoint](2026-09-08-retail-usercmd-wire.md) for the observed cases.
The [record](2026-09-08-retail-usercmd-key-mask.json) pins identities, function
addresses, controlled dependencies and validation.

## Retail delivery evidence

Read-only tracing connects the actual keyed reader at RVA `0xd54a0` to the
retail command-batch routine at `0xe0320`. The caller XORs a server value,
message acknowledgement and reliable-command hash to form the key; it does
not clear bit 16 before calling the decoder. Accepted commands are copied as
seven dwords into client storage at offset `0x1041c`, immediately before the
game ClientThink callback. The retail `SV_GetUsercmd` routine at `0xe1630`
copies the same seven dwords to module output without masking those fields.

A controlled Unicorn probe executes this chain with one active synthetic
client and one command. Actual retail code encodes the command count and keyed
command, computes the empty reliable-command hash, decodes the batch, accepts
the timestamp, copies the last command and reaches the game callback. The
callback records the command and returns; no real game module is executed.
The probe then calls actual retail `SV_GetUsercmd` and checks all 28 output
bytes and adjacent guards. Zero-fill and stack-cookie helpers are controlled.
The server globals and active-client branch are synthetic fixture inputs,
not observations of a live authenticated session.

For timestamp 1 and supplied angle/button words `0xffff`, keys `0x10000` and
`0xffffffff` deliver `0x1ffff` in all four words at both the callback and
GetUsercmd boundaries. Key zero delivers `0xffff`. All three cases consume the
full written bit count and retain guards. Thus the extra bit is part of the
observed engine-to-module contract; this probe does not establish how every
gameplay consumer interprets it.

## Change and validation

The keyed field reader receives the selected layout and expands its key mask
only for QL. Only fields whose change bit is present apply that mask; unchanged
fields inherit their complete baseline words. The encoder is unchanged.
The regression crosses Q3 protocols 68/71 and QL protocol 91 with key/timestamp
bit-16 toggles and inherited versus changed words. It also decodes an eight-byte
retail-generated fixture, checking the complete command and 60 consumed bits.
The old mask fails the regression. The corrected Windows x86 helper matches
all 24 measured retail decoder results from the earlier command corpus,
including its formerly mismatching case.

Final build and test outcomes are recorded in the companion JSON. Investigation
helpers and raw retail output remain ignored. No executable bytes, retail
implementation text, assets or credentials are incorporated. These are selected
retail-function and module-boundary observations, not full retail process,
gameplay or bidirectional live peer interoperability. Compatibility gates
remain open. No unrelated engine defect was established.

The Windows x86 build succeeds; both Windows x86 and Linux i686 helpers match
all 24 retail-decoded corpus states, and the Linux codec executable passes.
The initial complete Windows suite passed 108 targets and skipped one. Its
two failures were the same stale FOV call-signature assertion, reported by
the standalone bridge test and the complete Python group. Updating that
assertion to require the new profile argument preserves its wiring check.
Both targets then pass, including 1,292 Python tests with five skips. Combined
coverage is 110 passed, zero remaining failures and one skipped Meson target;
the engine code was unchanged between the full run and the affected-target rerun.
