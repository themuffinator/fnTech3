# Enemy Territory pure admission

## Scope and authority

This work follows `b0d8e52`, with a clean worktree at the start. It removes the
explicit refusal of `sv_pure` for ET, restores retail ET's pure defaults, admits
ET wherever the RTCW native pure pair was already handled, and publishes the
loaded-pak names that retail ET clients require. The ET client and
default-release gates are unchanged; ET remains experimental and dedicated-only
behind `-Dwolfet-development=true`.

Interoperability facts come from the pinned original
[ET pure verification](https://github.com/id-Software/Enemy-Territory/blob/40342a9e3690cb5b627a433d4d5cbf30e3c57698/src/server/sv_client.c),
[server systeminfo publication](https://github.com/id-Software/Enemy-Territory/blob/40342a9e3690cb5b627a433d4d5cbf30e3c57698/src/server/sv_init.c),
[client pure checksums](https://github.com/id-Software/Enemy-Territory/blob/40342a9e3690cb5b627a433d4d5cbf30e3c57698/src/client/cl_main.c),
[client systeminfo handling](https://github.com/id-Software/Enemy-Territory/blob/40342a9e3690cb5b627a433d4d5cbf30e3c57698/src/client/cl_parse.c),
[filesystem pak lists](https://github.com/id-Software/Enemy-Territory/blob/40342a9e3690cb5b627a433d4d5cbf30e3c57698/src/qcommon/files.c)
and the shifted module-name constants in
[qcommon.h](https://github.com/id-Software/Enemy-Territory/blob/40342a9e3690cb5b627a433d4d5cbf30e3c57698/src/qcommon/qcommon.h).
The reference is GPL-3.0-or-later with additional terms. No implementation text
is incorporated into fnTech3.

## Contracts

**Proof shape.** ET's `SV_VerifyPaks_f` is Q3's with two substitutions: the
module names come from `FS_ShiftStr` over obfuscated constants, and a stale `cp`
is one whose serverId precedes the checksum feed rather than one that differs
from the current serverId. Decoding those constants yields `cgame_mp_x86.dll`
and `ui_mp_x86.dll` on Windows and `cgame.mp.i386.so` / `ui.mp.i386.so` on
Linux, which is exactly what fnTech3's ET profile already generated. The
obfuscation is a naming device in the reference engine, not a wire contract; the
proof is the two module checksums followed by the referenced pak list, as for
RTCW MP. ET's client applies a similar two-byte shift to spell the `cp` command
itself, so the command that reaches the server is unchanged.

The three server-side places that recognised only `COM_PURE_PROOF_RTCW_NATIVE`
— the expected module checksums, the stale-`cp` rule and the second module slot
— now use one `Com_PureProofIsNativeModulePair` predicate that also covers ET,
as does the module-pak referencing in server startup and the client's own proof
builder. RTCW single player keeps no pure admission, and Q3 and QL keep their
QVM and single-binary proofs.

**Defaults.** Retail ET starts with `sv_pure 1` and does not latch it. The ET
profile previously defaulted to `0` and latched it so that development probes
could not enable a path that would fail; both values now match retail.

**Loaded-pak names.** Retail ET's pure server publishes `sv_pakNames` alongside
`sv_paks`, and its client stores those names next to the checksums it receives.
fnTech3 inherited a Q3 lineage that publishes an empty `sv_pakNames` because Q3
and QL clients ignore it. A retail ET client connecting to a pure fnTech3 server
therefore received checksums with no names and faulted before reaching the world
(exit `0xC0000005`, immediately after `Connected to a pure server`). ET now
publishes `FS_LoadedPakPureNames`, a new dedicated-safe function that lists
`gamedir/basename` for exactly the paks `FS_LoadedPakChecksums` counts, in the
same order and with the same exclusions, and both strings are measured against
the gamestate budget before either is set.

This is scoped to ET. Retail RTCW MP reads the same key from its own server, so
that profile very likely has the same defect; it is left unchanged here because
there is no retail RTCW client run to confirm it, and changing a second
profile's systeminfo without evidence would be a silent cross-profile change.
(Followed up in the [RTCW pure-name record](2026-09-17-rtcw-mp-pure-names.md):
RTCW MP stores the names but never reads them back, a retail MP client admits
without them, and the "very likely" above did not hold.)

## Verification

Clean Windows x86 RelWithDebInfo build with `-Dwolfet-development=true` and
`-Drtcw-development=true`; no errors or warnings. The complete automated suite
passes.

New source and executable regressions check that the two ET pure module names
are produced for slots 0 and 1 and nothing for slot 2, that the shifted retail
constants decode to those names, that only RTCW MP and ET are treated as the
native pure pair while Q3, QL and RTCW single player are not, that ET's profile
now defaults `sv_pure` to `1` unlatched, that the startup refusal is gone, that
the loaded-pak name list is built from the same paks as the checksum list and
lives outside the dedicated-build guard, and that only ET publishes it.

A retail ET client now passes pure admission against the experimental fnTech3 ET
dedicated server: it reaches `CS_ACTIVE`, joins Axis and Allies, and both
processes exit with status 0 in 55.5 seconds with the installation unchanged and
no unpure drop. The same run against the previous binary faulted in the client.
`scripts/probe_wolfet_retail_client.py` gained a `--pure` option for this.

| Artifact | SHA-256 |
| --- | --- |
| `fntech3.ded.exe` | `d82f60dd648eaac49734da6f33ec54276737593e69d7e247686ab098c36a3c39` |
| retail `ET.exe` | `4d1326c5936b61c5f878fc017e4aa0abddf13e008139e31fce2f13d57b26243e` |
| retail `qagame_mp_x86.dll` | `160a6218397865ad61071faa6316053713c03b9be80e5d05c7a5be2cab7f630b` |

A non-pure control run with the same binary still passes, so nothing in the
existing admission path regressed.

All six stock maps repeat this with the later `41cbb678` server, each joining
Axis and Allies under `sv_pure 1` with both processes exiting 0 and the
installation unchanged:

| Map | Pure admission | Axis | Allies | Elapsed |
| --- | --- | --- | --- | --- |
| goldrush | yes | yes | yes | 55.7 s |
| battery | yes | yes | yes | 55.6 s |
| oasis | yes | yes | yes | 55.6 s |
| radar | yes | yes | yes | 55.4 s |
| railgun | yes | yes | yes | 55.3 s |
| fueldump | yes | yes | yes | 55.4 s |

Two existing fixtures needed matching updates: the pure-profile contract probe
gained the new predicate, and the server-policy probe now expects ET's retail
`sv_pure` default instead of the development refusal it previously asserted.

## Remaining gates

- One client and one installation. All six stock maps pass, but mods with
  their own paks, gamestate-overflow fallback and the degraded no-`sv_paks`
  path are unverified, as is any map larger than the stock set.
- Downloads of missing paks under pure ET, and the reverse direction with
  fnTech3 as the ET client, remain open; ET client startup is still gated.
- Retail RTCW MP's `sv_pakNames` requirement is resolved: it has none. The
  [RTCW pure-name record](2026-09-17-rtcw-mp-pure-names.md) shows the fault is
  ET-specific and the scoping here is correct, so nothing changed for that profile.
- Everything else in the [ET matrix](COMPATIBILITY.md#wolfet-gate-matrix)
  remains as recorded.

## Unrelated observations

`FS_LoadedPakNames` remains a console-only helper compiled out of dedicated
builds and reporting bare basenames; it is unchanged and is not the function
servers publish. The RTCW MP pure-name question described above is the one
product issue this slice found outside its scope; it was investigated separately
and turned out to need no change.
