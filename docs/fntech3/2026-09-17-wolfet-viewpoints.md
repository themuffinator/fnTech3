# Enemy Territory viewpoints and server snapshot capacity

## Scope and authority

This work follows `f04cf62`, with a clean worktree at the start. It implements
ET origin-only brush visibility, self/exclusive portal viewpoints and leaning,
and separates server snapshot capacity from the frozen client-module limit.
The ET client and default-release gates remain unchanged.

Interoperability facts come from the pinned original
[ET server snapshot selection](https://github.com/id-Software/Enemy-Territory/blob/40342a9e3690cb5b627a433d4d5cbf30e3c57698/src/server/sv_snapshot.c),
[world linking](https://github.com/id-Software/Enemy-Territory/blob/40342a9e3690cb5b627a433d4d5cbf30e3c57698/src/server/sv_world.c),
[game flags](https://github.com/id-Software/Enemy-Territory/blob/40342a9e3690cb5b627a433d4d5cbf30e3c57698/src/game/g_public.h)
and [client snapshot transfer](https://github.com/id-Software/Enemy-Territory/blob/40342a9e3690cb5b627a433d4d5cbf30e3c57698/src/client/cl_cgame.c).
The reference is GPL-3.0-or-later with additional terms. No implementation text
is incorporated into fnTech3. The engine changes extend its existing flag
translation, checked game-data access, world linking and snapshot selection.
The source comparison compiles reference functions only in disposable local
translation units; they do not enter fnTech3 binaries or committed fixtures.

## Visibility and viewpoint contracts

The external ET bits `0x4000`, `0x8000` and `0x10000` map to distinct engine
flags. They are not Q3 flags and do not activate RTCW behavior. Native writes
preserve unknown module-owned bits, including ET's POW/BLANK bits that share
positions with obsolete RTCW meanings.

For ET brush models, linking caches the cluster containing `r.currentOrigin`.
The origin-only flag tests that cluster against the viewer's PVS before area
or bounding-box visibility checks, preserving original precedence. Broadcast
and client restrictions still apply, and accepted candidates still pass the
game's snapshot callback. Negative/out-of-range clusters never index the PVS;
relinking or clearing the world discards stale origin-cluster state.

The connected client's exclusive-camera flag selects its `s.origin2` as the
base viewpoint. Otherwise the playerstate origin remains the base. Viewheight
is then applied. Lean shifts the eye along the view's right vector, with half
the lean added to roll. All new coordinates are checked for nonfinite values
before collision/PVS lookup. The other four profiles retain their previous
viewpoint behavior.

The self-portal flag merges the followed/viewed player's `s.origin2` into the
snapshot and area mask. A per-snapshot visit marker expands it once. The
published original function recurses into the same self-portal branch without
a guard; the intended second-view behavior is implemented with bounded visits
instead. This deliberate difference is Source/Unit evidence, not a claim of
observed retail behavior for that flag. Ordinary portal cycles continue to use
the existing entity visit counters.

## Server and module capacities are different

Original ET's private server candidate array has 2,048 slots, while its entity
index space has only 1,024 slots. The server can therefore consider every unique
entity. The 512 limit belongs to the cgame `snapshot_t`; original
`CL_GetSnapshot` truncates when transferring a larger decoded snapshot into that
module record. It is not a 512-entity server-selection limit.

The preceding fnTech3 slice mistakenly used the cgame limit for its dedicated
server. This follow-up gives internal server storage the full entity-index
capacity and retains the immutable 512-entry native ET module contract.
Q3, QL and both RTCW server-selection limits remain 256. A failing-before
regression builds a dense broadcast snapshot through `SV_BuildClientSnapshot`;
the corrected ET result contains all 1,022 other encodable entity numbers in
order, with the client's own entity and the wire terminator excluded.
Other-profile caps and adjacent frame sentinels
are checked in the same executable fixture.

The larger internal frame arrays add 64 KiB per client on the Windows x86
proof target (at most 4 MiB for 64 clients). No external module record or wire
layout changes.

## Evidence

`probe_wolfet_contracts.py` passes 1,326 layout/constant/ordinal assertions against
the original headers on both Windows x86 and x64, including the three added
flag constants. The x64 result is layout evidence only; the shared runtime
proof remains Windows x86.

`probe_wolfet_viewpoints.py` compares 4,936 finite origin/angle/lean/camera cases
against the original `SV_BuildClientSnapshot` viewpoint calculation. Both
probes use their respective actual `AngleVectors` implementation and agree
exactly at nine significant decimal digits. This covers ordinary/exclusive
origins, followed clients, positive/negative lean, varied angles and heights.
It does not establish x87 retail arithmetic, PVS or visual parity.

Executable regressions cover all five profiles, exhaustive ET flag round trips,
native private-tail preservation, origin-cluster relinking/reset, invalid PVS
indices, callback/broadcast/client-filter precedence, self/ordinary portal
cycles, area-mask merging, alternate-camera integration into the actual client
snapshot builder, positive/negative lean and malformed coordinates. The dense
snapshot regression keeps module and server limits separate.

The Windows x86 RelWithDebInfo shared client, dedicated server and all three
renderers build with both development options enabled. The full Meson run
passes 113 checks with zero failures and two expected skips. Its recursive
Python suite runs 1,550 tests in 500.016 seconds with 62 expected skips.
The seven focused x86 regressions pass; the final dense-fixture and self-portal
area-mask refinements also pass in the standalone viewpoint suite. Only test
fixtures and documentation changed after the full build.

The final dedicated binary passes both bounded retail ET probes: Axis/Allies
admission, and two objective-camera requests with received `portalcampos`
replies. Each run lasts 55.688 seconds, reaches `CS_ACTIVE` and `ClientBegin`,
disconnects normally, exits both processes with code zero, and leaves the
installation unchanged. The same binary passes Q3 and QL pure startup probes
under both automatic and explicit profile selection. These runs do not close
the ET pure, rendered visibility or broader gameplay gates.

All final runtime evidence uses the Windows x86 dedicated executable built from
`f04cf62e82eb60341d3fa797ba354131c848af80` plus this working change. The retail
installation is Steam app 1873030, build 7997832, under
`E:\SteamLibrary\steamapps\common\Wolfenstein Enemy Territory`.

| Input | SHA-256 |
| --- | --- |
| Final `fntech3.ded.exe` | `bec9a816747d80347c4156e70373e2d87b345257ee1a5115b6f0c718e3ae4ddb` |
| Retail `et.exe` | `4d1326c5936b61c5f878fc017e4aa0abddf13e008139e31fce2f13d57b26243e` |
| Retail `qagame_mp_x86.dll` | `160a6218397865ad61071faa6316053713c03b9be80e5d05c7a5be2cab7f630b` |

An attempted comparison with retail `etded.exe` did not produce valid runtime
comparison evidence: the fnTech3 probe expects a different server log name and
bounded-wait timing. The original-server result is retained as a probe
limitation, not recorded as an engine compatibility failure. The published
source comparison above is the original-engine viewpoint evidence.

## Reproduction and local evidence

```powershell
python scripts/probe_wolfet_contracts.py `
  --source "<original ET source>" --target i686-pc-windows-msvc `
  --report .tmp/wolfet-visibility/contracts-x86.json
python scripts/probe_wolfet_viewpoints.py `
  --source "<original ET source>" `
  --report .tmp/wolfet-visibility/viewpoint-source.json
.\.vscode\build-release.ps1 -Configuration RelWithDebInfo `
  -BuildDir meson\build\win32-dev -Target both -Renderer all `
  -RTCWDevelopment -WolfETDevelopment -RunTests
python scripts/probe_wolfet_retail_client.py `
  --server meson/build/win32-dev/fntech3.ded.exe `
  --installation "<legitimate ET installation>" `
  --output .tmp/wolfet-visibility/camera-new --camera-check
```

The camera probe opens/toggles the retail limbo panel and sends two objective
requests through ordinary console commands. It checks the received
`portalcampos` replies, connection lifecycle and unchanged installation. It
uses a windowed client, unswitched private desktop, disabled mouse/joystick
input and isolated writable homes. It does not inject device input, take screen
captures or claim rendered-camera correctness. Team admission uses the separate
`--team-check` mode.

Source reports, build/test logs and installed probe reports are retained under
`.tmp/wolfet-visibility/`, including `verification-final.log`,
`testlog-final.txt` (inherited environment omitted), `targeted-complete.log`,
`viewpoint-complete.log`,
`interop-camera-final/result.json`, `interop-teams-final/result.json` and
`nonreg-final/manifest.json`. The reports include exact runtime commands.
They are not committed. No retail input, generated
client identifier or authentication data enters the repository history.
Console logs were copied outside disposable runtime homes. Automatic approval
review rejected removal of those homes and the test-temp directory with
"blocked by policy"; these ignored task files remain in the same evidence area.

## Remaining gates

Live origin-only brush/proxy/landmine selection, lean visibility at doors,
rendered objective-camera parity, maximum-density retail exchanges and other
platforms remain open. The camera and dense-scene synthetic checks cannot close
those live gates. Client UI/cgame and MDM/MDX rendering, pure mode, downloads,
demos, broader game imports, movement/combat/objectives, rotation/restarts and
bidirectional interoperability remain incomplete. See the
[ET gate matrix](COMPATIBILITY.md#wolfet-gate-matrix).

No new unrelated product issue was found. The previously recorded Gold Rush
script warnings remain outside this snapshot slice.
