# RTCW MP loaded-pak names under pure

## Scope and authority

The [ET pure record](2026-09-17-wolfet-pure.md) left one question open. A pure
fnTech3 server publishes `sv_paks` but sets `sv_pakNames` to an empty string, a
Quake 3 lineage optimization. Retail ET faulted on that, so ET now publishes
`FS_LoadedPakPureNames()`, scoped by `const bool publishNames =
Com_GameIsWolfET();` in
[`code/server/sv_init.cpp`](../../code/server/sv_init.cpp).
Retail RTCW MP sets the same key from its own server and reads it on its client,
so that profile was left unchanged pending evidence.

This slice answers whether a retail RTCW MP client needs those names. **It does
not.** The ET-scoped publication stays as it is; no engine behaviour changed.

The source references are:

- [id Software RTCW MP](https://github.com/id-Software/RTCW-MP), local
  `E:\_SOURCE\_CODE\RTCW-MP-master`.
- [id Software Enemy Territory](https://github.com/id-Software/Enemy-Territory),
  local `E:\_SOURCE\_CODE\Enemy-Territory-original`, for the contrast.
- [iortcw](https://github.com/iortcw/iortcw), local
  `E:\_SOURCE\_CODE\iortcw-master`, because the RTCW MP profile also advertises
  `COM_PROTOCOL_FAMILY_IORTCW_MP`.

These releases are GPL-3.0-or-later with id Software's additional terms. They
were read only. No implementation text is incorporated into fnTech3.

## Observed facts

**The two storage functions are the same.** `FS_PureServerSetLoadedPaks` is
character-for-character equivalent in RTCW MP `src/qcommon/files.c` and ET
`src/qcommon/files.c`, including the `if ( pakNames && *pakNames )` guard. With
an empty `sv_pakNames` both engines leave every `fs_serverPakNames[i]` as `NULL`
rather than storing anything. Storage is therefore not what distinguishes them.

**ET has one unguarded reader; RTCW MP has none.** Across each tree, the
non-definition references to `fs_serverPakNames` are:

| Tree | References outside `FS_PureServerSetLoadedPaks` |
| --- | --- |
| RTCW MP `src/` | none |
| iortcw `MP/code/` | none |
| ET `src/` | `files.c:2999` and `files.c:3000`, both inside `FS_VerifyOfficialPaks` |

ET's `FS_VerifyOfficialPaks` loops `i < fs_numServerPaks` — a count taken from
`sv_paks`, not from the names — and passes `fs_serverPakNames[i]` straight to
`FS_idPak`, which calls `FS_FilenameCompare` on it. Neither function checks for
`NULL`. `src/client/cl_parse.c:637` calls it from `CL_ParseGamestate` on the
statement after `CL_SystemInfoChanged()`, gated on `cl_connectedToPureServer`.
RTCW MP's `CL_ParseGamestate` goes from `CL_SystemInfoChanged()` directly to
`FS_ConditionalRestart`; it has no `FS_VerifyOfficialPaks`, and the function
does not exist anywhere in that tree. In RTCW MP `FS_idPak` is reached only
from `files.c:2830` over `fs_serverReferencedPakNames` — fed by
`sv_referencedPakNames`, which fnTech3 publishes for every profile — and from a
server-side download check in `sv_client.c:816`.

**The retail binaries agree with their sources.** Both literals belonging to
`FS_VerifyOfficialPaks` are present in retail `et.exe` and absent from retail
`WolfMP.exe`:

| String | retail `et.exe` | retail `WolfMP.exe` |
| --- | --- | --- |
| `sv_pakNames` | present | present |
| `Connected to a pure server.` | present | present |
| `ERROR: Missing/corrupt official pak file %s` | present | absent |
| `Couldn't load an official pak file; verify your installation…` | present | absent |

The `et.exe` scanned is SHA-256 `4d1326c5…6243e`, the same binary the ET record
faulted. `sv_pakNames` is also absent from all three retail RTCW MP modules
(`cgame_mp_x86.dll`, `ui_mp_x86.dll`, `qagame_mp_x86.dll`), so no module reads
the cvar the client sets from systeminfo either.

**A retail RTCW MP client admits without the names.** The fnTech3 dedicated
server published exactly the ET-fatal condition for this run — `sv_pure 1`,
fifteen checksums in `sv_paks`, `sv_pakNames` empty — and the retail client
reached the world and exited cleanly:

| Observation | Value |
| --- | --- |
| `sv_pure` / `sv_paks` / `sv_pakNames` published | `1` / 15 checksums / empty |
| Client printed `Connected to a pure server.` | yes |
| Paks the client put on its pure list | 15 |
| `CS_PRIMED` → `CS_ACTIVE`, `ClientBegin: 0` | yes |
| Unpure drop, official-pak error | none |
| Client exit / server exit | `0` / `0` |
| Access violation (`0xC0000005`) | no |
| Installation unchanged | yes |

## Inference

The observed facts above are what was measured. The reading that follows is
inference, not observation: ET's fault is caused by `FS_VerifyOfficialPaks`
dereferencing names that the shared guard deliberately left `NULL`, and RTCW MP
is unaffected because that function was added for ET and has no counterpart in
either the original or the iortcw MP lineage. The defect is therefore
ET-specific rather than a general consequence of publishing an empty
`sv_pakNames`, and the existing `Com_GameIsWolfET()` scoping is correct as
written rather than merely untested. Publishing names for RTCW MP would spend
gamestate budget for a string no client, engine or module in that profile reads.

## Verification

Clean Windows x86 `RelWithDebInfo` dedicated builds with
`-Drtcw-development=true`, before and after the comment change; no errors. The
Python suite runs 1,559 passed, 67 skipped and 32,886 subtests passed, with
three failures this slice did not cause and did not fix:
`rtcw_md3_normal_tests.py` textually extracts `Q_rsqrt` from `q_math.c`, whose
`_MSC_SSE2` branch needs the `#include <intrin.h>` sitting just outside the
extracted function, so clang rejects `_mm_load_ss`. The same three fail
identically on a clean checkout of `182544b` with none of these changes applied.
The existing `tests/wolfet_pure_tests.py` already pins the scoping line and
still passes, so no new regression was added — the outcome of this slice is that
the code is already correct.

`scripts/probe_rtcw_retail_client.py` is new. It runs the dedicated server and a
retail `WolfMP.exe` on unswitched private desktops with isolated homes, windowed,
sound and input disabled, and console commands only — no mouse or keyboard input
is injected. It records the `sv_paks`/`sv_pakNames`/`sv_pure` the server actually
published for the run, so the condition under test is in the evidence rather than
assumed, and it fails the run if the client never took the pure path. The pure run
is `.tmp/rtcw-paknames/pure/result.json`; a non-pure control with the same binary
is `.tmp/rtcw-paknames/nonpure/result.json`.

| Artifact | SHA-256 |
| --- | --- |
| `fntech3.ded.exe` | `3b576a25d8a2a938324e4630a1cef6fa8d74241dcfea46cd04f8623bd664ec11` |
| retail `WolfMP.exe` | `7cf99a938cf18ed2f5aecd0be542cbae8868959e0cce24b4b6eea08991e321f5` |
| retail `qagame_mp_x86.dll` | `d8975b5c4e14488283c5eca7d549005304b04f1690705c13c174a171cc24c387` |

The probed server was built from the tree since committed as
`182544b72c1b8a24b0aa870bb0cd14ad3379deff` — then `369984c` plus an unrelated
in-progress ET download slice held in the worktree. The only engine source this
slice changes is a comment at the publication site, so it cannot affect what was
probed; rebuilding with it succeeds. Platform Windows 11 x64, profile `rtcw-mp`,
map `mp_beach`, one retail installation, spectator only.

## Remaining gates

- One map, one client, one installation, spectator only. This run says nothing
  about RTCW MP pure behaviour across other stock maps, mods with their own
  paks, the gamestate-overflow fallback or the degraded no-`sv_paks` path.
- The binary evidence is a string-table scan, not disassembly. It shows the two
  literals are absent from `WolfMP.exe`; it does not by itself prove no other
  reader of the names exists in that binary. The source and runtime evidence are
  what carry the conclusion.
- Downloads under pure RTCW MP, and the reverse direction with fnTech3 as the
  RTCW MP client, remain open.
- Everything else in the [RTCW matrix](COMPATIBILITY.md#rtcw-gate-matrix)
  remains as recorded.

## Unrelated observations

RTCW MP's `FS_ComparePaks` passes `fs_serverReferencedPakNames[i]` to `FS_idPak`
at `files.c:2830` before its own `fs_serverReferencedPakNames[i] && *…` check a
few lines below, so the original engine would dereference `NULL` there if a
server ever published `sv_referencedPaks` without matching
`sv_referencedPakNames`. fnTech3 publishes both keys for every profile, so its
servers do not create that condition, and fnTech3's own `FS_ComparePaks` is not
affected. This is an observation about the reference engine's robustness against
a hostile or unusual peer, not a defect in fnTech3, and no fnTech3 change is
proposed for it.

The retail RTCW MP client writes `main/rtcwconsole.log`, not the `qconsole.log`
the Quake 3 lineage uses. The first version of the probe looked for the wrong
name and under-reported the client-side pure lines; it was corrected and rerun
before any result here was recorded.
