# Enemy Territory profile, wire contracts and dedicated startup

## Scope

Wolfenstein: Enemy Territory is a separate `wolfet` profile (aliases `et`,
`enemy-territory`, `enemyterritory`), not an RTCW mode. This first slice adds
profile selection, Steam installation discovery, native module identities,
frozen records, protocol-84 message bodies, server memory access, game imports,
parser metadata, attachment tags, world-only traces and bounded dedicated probes.
A separate retail-client probe adds non-pure inbound spectator and team-admission
evidence.

ET remains experimental. Default builds reject it. `-Dwolfet-development=true`
permits the shared dedicated binary to activate ET; the client still rejects
it even with that option. The incomplete UI/cgame and MDM/MDX renderer boundaries
are release blockers. No playable client or complete multiplayer compatibility
claim follows from this work.

The changes follow checkpoint `15d9dda`, which committed the existing worktree
before ET implementation. Runtime evidence was collected from that checkpoint
plus the then-uncommitted ET changes recorded with this document; executable
hashes below identify the tested artifacts. No retail modules or assets are
incorporated.

## Evidence and provenance

The original [id Software ET release](https://github.com/id-Software/Enemy-Territory/tree/40342a9e3690cb5b627a433d4d5cbf30e3c57698)
at `40342a9e3690cb5b627a433d4d5cbf30e3c57698` supplies interoperability facts.
Its GPL-3.0-or-later implementation and additional terms were inspected before
use. No implementation text is incorporated into this GPL-2.0-or-later project.
The adapters reuse fnTech3's own checked transfer/scalar primitives and are
independently designed; the tables contain names, ordinals, offsets and widths.
Source probes compile the external reference only in disposable local builds.
See [credits](../../CREDITS.md) and [notices](../../THIRD_PARTY_NOTICES.md).

Installed observations use Steam application `1873030`, build `7997832`, at
`E:\SteamLibrary\steamapps\common\Wolfenstein Enemy Territory`.
Its `etmain/qagame_mp_x86.dll` SHA-256 is
`160a6218397865ad61071faa6316053713c03b9be80e5d05c7a5be2cab7f630b`.
The game reports `etmain`, built March 10, 2005. Installation detection uses
the `pak0.pk3` pair `default.cfg` (1,496 bytes, ZIP CRC `061b5285`) and
`maps/goldrush.bsp` (14,757,996 bytes, ZIP CRC `7ebac2f4`). Mixed-title evidence
is rejected. Steam supplies discovery only, not authentication or Workshop.

## Contracts established by source and unit checks

| Contract | ET value |
| --- | --- |
| Base directory / client config | `etmain` / `etconfig.cfg` |
| fnTech3 dedicated config | `etconfig_server.cfg`, isolated from other profiles |
| Live/demo protocol | 84 only; other profiles cannot select it |
| Module APIs | Legacy native game 8, cgame 3, UI 4; no QVM substitution |
| Windows module names | `qagame_mp_x86.dll`, `cgame_mp_x86.dll`, `ui_mp_x86.dll` |
| Linux module names | `qagame.mp.i386.so`, `cgame.mp.i386.so`, `ui.mp.i386.so` |
| Clients / configstrings / player base | 64 / 1,024 / 689 |
| WOLFINFO / gamestate string bytes | Configstring 21 / 16,000 |
| User command / entity / player POD | 28 / 288 / 1,452 bytes |
| Parser token | 1,048 bytes, including line and crossed-line metadata |
| Entity shared data | Begins at byte 288; does not contain RTCW's duplicated entity state |
| Client-module snapshot / reliable commands / message bytes | 512 entities / 256 commands / 32,768 bytes |
| BSP / collision model handles | Version 47, ordinary 17-lump header / 512 |
| Entity/player delta fields | 71 / 77, ordered independently from RTCW |

`probe_wolfet_contracts.py` passes 1,323 size/alignment/field/constant/ordinal
assertions against the original headers for both Windows x86 and x64. This
is layout evidence, not x64 runtime support. The shared proof build is x86.
All 522 UI/cgame/game imports and exports have distinct, checked lookup tables.
Only the dedicated import subset is implemented; unknown calls fail explicitly.

`probe_wolfet_messages.py` passes 1,169 synthetic source comparisons, including
exact encoded bytes and bit counts, local decode, and cross-decoding with the
original encoder/decoder in both directions. The ET keyed-command decoder
retains the original extra key bit for integer angles while truncating byte
fields and masking `doubleTap` to three bits. Checked-in vectors contain only
generated values, not captured retail sessions.

Executable unit checks cover transactional state transfers, all trajectory
encodings, high weapon slots, extra events, malformed/truncated messages,
profile immutability, build gates, module names and traversal rejection,
server array strides and sentinels, and TAG bounds/nonfinite values.
At this checkpoint server snapshot storage accommodated 512 entities; construction kept
the existing 256-entity cap for Q3, QL and both RTCW profiles. An executable
regression fills and overflows the window for each profile and checks the
destination frame capacity. ET snapshot callbacks and visibility flags remain
separate open gates at this checkpoint. The later
[snapshot follow-up](2026-09-17-wolfet-snapshots.md) implements callbacks and
single/multiple visibility proxies with Source/Unit evidence; live behavior and
the other ET visibility contracts remained open. The later
[viewpoint/capacity follow-up](2026-09-17-wolfet-viewpoints.md) implements the
origin-only and camera flags and corrects the server capacity: 512 is the frozen
cgame record limit, while server selection must cover the full entity-index space.

## Installed dedicated observations

The Windows x86 shared client, dedicated server and GLx/Vulkan/RTX raster
renderers compile with both development options enabled. Dedicated probes load
the legitimate game DLL and all six stock maps: `battery`, `fueldump`,
`goldrush`, `oasis`, `radar`, `railgun`. Explicit and automatic profile selection
each pass for all six maps, answering a loopback protocol-84 `getinfo` and
exiting normally after 100 server frames. Each probe has a separate writable
home, disabled masters/bots/PunkBuster, and `sv_pure 0`.

The reproducible command is documented in [BUILD.md](../../BUILD.md#enemy-territory-development-profile).
Detailed command lines, executable/module hashes, responses and exit status
are retained under `.tmp/wolfet/`. No keyboard/mouse automation or screen
capture is used. The map probe does not connect a client, so map initialization
must not be promoted to gameplay or interoperability evidence. A separate
inbound client probe below supplies narrower interoperability evidence.

The installed `etded.exe` also passes the Gold Rush baseline with the same
module and isolated settings. Its SHA-256 is
`7eb606913a56d6901e358a598fb0079120b2730e113a10fc963f90b777580cdf`.
Both engines report three `setstate called and no entities found` warnings
and `0 teams with 0 entities` during this map's startup. These are observed
in the retail baseline as well; their gameplay implications remain untested.

The stock installation contains nine TAG assets. All use `TAG3`, version 1,
despite the published header naming `TAG1`. Both observed identifiers are
accepted; lengths, counts, names and finite transform components are checked
before publishing a tag handle. All nine installed assets (19 attachment
transforms) also pass the standalone C++ decoder. Failed reads publish no
partial state.

## Inbound retail-client interoperability

`scripts/probe_wolfet_retail_client.py` connects the legitimate Windows ET
client to fnTech3 on loopback, non-pure Gold Rush. It uses unswitched private
desktops, `r_fullscreen 0`, disabled mouse/joystick input and isolated writable
homes. No input is injected and no screen capture is used.

The first attempt reached `CS_ACTIVE`, then exposed ET's `passEntityNum == -2`
world-only trace contract. The server now preserves that behavior only for ET;
an executable regression covers 90 combinations across all five profiles,
including ordinary pass entities, world hits/misses and box/capsule traces.

The corrected run and the reusable probe both reached `CS_ACTIVE`, called
`ClientBegin`, disconnected normally, and exited both processes with status 0.
The installation's file sizes and modification timestamps remained unchanged.
The reusable run took 55.437 seconds. This is an inbound spectator
session; no gameplay input, audio, visual parity, pure admission, or reverse
interop claim follows from it. The fnTech3 client remains gated.

| Tested executable | SHA-256 | Evidence |
| --- | --- | --- |
| Shared x86 dedicated, before world-only trace correction | `fb9bdc5a8aac982973951b8a67b578888b6cf0cc7576deaf9ccf50252792ea44` | All six ET maps under explicit/auto selection; Q3/QL pure startup under explicit/auto selection |
| Shared x86 dedicated, with world-only trace correction | `dfd77e629a56074b49199ffd6c4bb48085e093d715451f5addecd74a8cabd5d1` | Retail ET inbound non-pure spectator session; normal client/server exits |
| Shared x86 dedicated, with 512-entity ET snapshots and shutdown tag lifetime | `43ca92158950551090c87f9ed5a5d07af435256810a4d63d78c7e6dde81989c2` | Inbound retail spectator and Axis/Allies admission probes; normal exits; unchanged installation; Q3/QL pure startup |
| Legitimate retail ET client | `4d1326c5936b61c5f878fc017e4aa0abddf13e008139e31fce2f13d57b26243e` | Peer for the inbound session |

The exact commands are in `.tmp/wolfet/interop-inbound/result.json`; they can be
reproduced with `python scripts/probe_wolfet_retail_client.py --server
meson/build/win32-dev/fntech3.ded.exe --installation "<your ET installation>"
--output .tmp/wolfet/interop-new`. The script is Windows-specific and must use
a fresh output directory. Local homes/logs may contain generated client IDs;
only hashes and non-private observations are retained in this document.
The final snapshot/tag-lifetime binary repeats the probe successfully in
55.703 seconds; its commands and result are in
`.tmp/wolfet/interop-final/result.json`.
That same binary also passes Q3 and QL pure-mode dedicated startup with both
automatic and explicit selection; see `.tmp/wolfet/nonreg-final/manifest.json`.

The optional `--team-check` probe issues ordinary client console commands to
join Axis, then Allies, as a soldier with the team's stock submachine gun.
Both team changes and respawns appear in the server log. Both processes exit
normally, the installation remains unchanged, and the run completes in
55.407 seconds (`.tmp/wolfet/interop-teams/result.json`). This exercises team
admission and additional client lifecycle calls with mouse/joystick input
disabled. Movement, combat, objective completion, sound and visual correctness
remain unverified; no screenshot is taken.

## Additional correction

The existing botlib token reader copied uninitialized local data when the
underlying read failed at EOF. It now returns before touching the caller's
record. The parser regression verifies unchanged destination sentinels on EOF
and invalid handles, exact line/crossed-line metadata, unread validation and
bounded/private latched-cvar output.

## Final validation

The 2026-09-17 Windows x86 MSVC `RelWithDebInfo` build completes for the shared
client, dedicated server and all three renderer modules, with both
`-RTCWDevelopment` and `-WolfETDevelopment`. The full Meson run reports
113 passed, zero failed and two skipped. Its recursive Python suite reports
1,546 tests, 62 skips and no failures in 701.375 seconds.

The existing ten-minute Python timeout was insufficient, so the gate now allows
a bounded fifteen minutes. This changes the test budget, not test selection.
The OpenAL loopback check skips because `ALC_SOFT_loopback` is unavailable.
The QL read-only factory probe initially skips because its installation variable
is unset; rerunning with `FNQL_RETAIL_QL_PATH` pointing at the legitimate QL
installation passes all five checks. Other opt-in/platform-dependent Python
checks retain their existing skip conditions.

Build/test evidence is retained in `.tmp/wolfet/verification-complete.log`,
`.tmp/wolfet/testlog-final.txt` (inherited environment omitted), and
`.tmp/wolfet/ql-factory.log`. Source comparison reports, the six-map matrix,
same-binary Q3/QL probes and retail ET connection reports are adjacent.
Generated test data, retail inputs and these local logs are not committed.

## Remaining gates

- ET UI/cgame imports, exports, client-state and renderer records; MDM/MDX
  skeletal models and renderer/audio/input behavior.
- Full game syscall coverage, live callback/proxy visibility, remaining ET
  viewpoint/visibility flags, binary messages, anti-lag collision options and
  restart/gameplay behavior; see the snapshot follow-up above.
- Pure native-module proof and pure pak semantics. Experimental ET servers
  default to non-pure; requesting pure mode fails explicitly.
- Full connect/gamestate/snapshot coverage and bidirectional retail peers;
  brief inbound non-pure spectator/team-admission sessions do not close this gate.
- Demo discovery/playback, downloads, malformed asset recovery, long-running
  map rotations, mod coverage and all-profile runtime non-regression.
- PunkBuster is unavailable and reported as such; its stat-report import is
  inert as in the published original engine. No service success is fabricated.

The [compatibility ledger](COMPATIBILITY.md#wolfet-gate-matrix) remains the
release gate. ET's open gates also keep the five-profile unified goal open.
