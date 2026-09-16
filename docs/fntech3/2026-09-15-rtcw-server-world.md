# RTCW server world and campaign startup

The experimental shared Windows x86 executable now passes the MP map's initial
entity spawning on `mp_beach`, including brush models and world linking. Both
profiles then stop at unimplemented botlib variable import 202. SP uses the
proper `spdevmap escape1` campaign path in this checkpoint. Neither result is
playable-game or AI compatibility evidence.

## Profile-owned server policy

The game manager catalog now owns these engine defaults. Explicit cvar values
survive ordinary registration; map aliases and SP campaign allocation have the
source engine's separate override rules.

| Profile | Default game type | Default clients | SP map alias clients | Campaign allocation | Frag limit |
| --- | --- | --- | --- | --- | --- |
| Q3 | 0 | 8 | 8 | No extra rule | 20, serverinfo |
| QL | 0 | 8 | 8 | No extra rule | 50, serverinfo |
| RTCW SP | 0 | 8 | 32 | 64, retaining an explicit 128 | 20, serverinfo |
| RTCW MP | 5 | 20 | 32 | No extra rule | 0, not serverinfo |

All four use game type 2 for the inherited single-player aliases. RTCW map
commands register skill defaults 1 (SP) and 3 (MP); SP also registers episode
zero. Before tearing down a server, SP applies pending client allocation,
enforces its campaign slot policy and enables AI. Its later skill-default
registration of 2 preserves the existing value, matching the source order.
QL factory preflight, activation and deactivation behavior is preserved.

These are observations of the original SP/MP `game/bg_public.h`,
`game/q_shared.h`, `server/sv_ccmds.c` and `server/sv_init.c` under the supplied
local reference roots. In particular, `MAX_SP_CLIENTS` is 64 and the absolute
SP client limit is 128; the map alias's intermediate 32 is not the final AI
allocation. The policies and tests are independently authored. No GPLv3 game
logic or reference implementation text is incorporated.

## World boundary

New RTCW import adapters cover link/unlink, brush model selection, area-entity
queries, box/capsule contact, PVS queries, area portals and connectivity. They
use the existing frozen SP/MP records through `ServerGameDataView`; an opaque
module entity pointer is never dereferenced as an engine `sharedEntity_t`.

The boundary checks registered entity membership, exact QVM ranges before
offset masking, integer widths, finite/ordered vectors, brush names and indices,
and area-query output capacity. Vectors and query results use aligned local
storage, supporting unaligned QVM outputs and input/output aliases. A rejected
query does not write the output. Native pointer checks cannot establish the
readability of arbitrary module-owned allocations; native entity membership
is checked against the explicit game-data registration.

Shared world linking now checks malformed/overflowing geometry before removing
the old linkage, clamps solid dimensions before float-to-integer conversion,
defines signed link-counter wrap, and explicitly leaves outside-world entities
unlinked. PVS queries reject negative/out-of-range destination clusters before
indexing a visibility mask. Valid Q3/QL bounds and visibility behavior remain
unchanged in the executed fixtures.

`tests/rtcw_server_world_tests.py` executes the actual shared world functions,
canonical read/write functions and new imports with the real game-data adapter.
It compares complete raw allocations for Q3, QL, SP and MP, including private
tails, the second entity state, neighboring records and guards. Coverage includes
relink/unlink, sector traversal, bounded queries, outside-world entities,
128-leaf overflow, signed counter wrap, huge finite dimensions, NaN/inverted/
overflowed geometry, brush/contact calls and visibility boundaries. SP/MP run
through native and QVM address paths. The collision model in these tests is a
controlled fixture; this is not original-engine collision parity.

`tests/rtcw_server_policy_tests.py` executes actual map commands, default cvar
registration and SP preparation with all four catalog entries. It covers explicit
settings, missing-map preflight, QL factory ownership, cheats, direct campaign
loads, pending slot counts and AI enabling. Cvars/filesystem are controlled hosts.

Windows x64 and x86 focused logs each record **15 passing methods**;
`world-focused-sanitizers.log` records **four passing methods** on Linux x86
with ASan/UBSan/leak detection. The experimental x86 client, dedicated server
and all three renderers build (`world-dev-build.log`). Evidence lives under
`.tmp/rtcw-manager/`.

## Installed map probe

`wrld-map/manifest.json` records the commands, module and renderer diagnostics
and client SHA256
`69bbbae9f53107ace0f77c655c1a99ce6e1e19f0c84dbf789592abd0c0e381b2`.
The working base is `c75573c7375e44c908256020ada2d7e12970cacf`, with uncommitted
work. The default RTCW activation gate is unchanged.

SP `spdevmap escape1` reaches `BOTLIB_LIBVAR_SET` before campaign game
initialization completes. MP `devmap mp_beach` completes its initial map entity
spawning and item registration, then reaches the same unsupported import.
Neither run produces a frame: the bounded probe stops each after the engine
error. These runs use the legitimate installation at
`E:/SteamLibrary/steamapps/common/Return to Castle Wolfenstein`, windowed GLX,
sound disabled and unswitched private desktops, with no input control or OS
capture. Both isolated homes are removed; installation inventory is unchanged.

Botlib/AI, full collision parity, save/load, gameplay, rendering and live
interoperability remain open. The preceding
[music/menu checkpoint](2026-09-15-rtcw-music-menus.md) retains its full-suite and
six Q3/QL runtime results; those binaries predate this server change.

## Same-executable installed-menu transitions

`mode-hop3/manifest.json` adds installed SP-to-MP and MP-to-SP transitions using
the same experimental binary/hash above. The probe sends the engine console
commands `startMultiplayer` and `startSingleplayer`, the commands used by RTCW's
menus. It does not click the menus or inject input. Each initial process exits
with code zero and launches exactly one child from the identical executable
path; each child also exits normally. The log switches from `uix86.dll` /
`cgamex86.dll` to their `_mp_x86` equivalents, and back in the reverse run.

All four engine screenshots were inspected: both source and destination native
menus are textured and readable. A Windows job owns each initial process and
its descendants, and the private desktop stays open until they exit. Both
processes' windows were observed on that same unswitched desktop. The probe
uses windowed 640x480 GLX, disables input/audio/services, removes isolated
writable homes and verifies the installation is unchanged. This is real
same-image menu handoff evidence on Windows x86; it does not establish running
game/session transfer, every renderer's handoff, or macOS support.

Two preceding probe attempts remain recorded. `mode-hop` incorrectly treated
an empty desktop window enumeration as an error and stopped its owned process.
`mode-hop2` completed both real mode relaunches, but scheduled the target capture
too early: a later startup `EXEC_NOW` reset its pending `wait`. `mode-hop3`
schedules the target capture through the last startup autoexec instead. These
were probe defects; no engine workaround was added.

`world-live/manifest.json` records six additional Q3/QL checks on the updated
default Windows x86 client, SHA256
`d5fc108cf45316c608997eb1d0ff7774603b9b5d51f9fa897271fb8b086fd260`.
All three renderers retain Q3 `q3dm6` world/weapon/HUD rendering and QL
`campgrounds` world/native join UI rendering. All captures were inspected and
audited, with all three legitimate modules loaded in each run. These checks
exercise map startup/world linking, not bidirectional network interoperability.

The completed default Windows x86 suite after this change passes **111 Meson
targets, zero failures and two configured skips** (`world-full-suite.log` /
`world-full-testlog.txt`). Its Python aggregate passes **1,359 methods in
290.385 seconds, with 11 skips**. The first attempt exposed one isolated cvar
fixture missing the new world-host stub; the corrected fixture passes Windows
x86/x64 and Linux x86 sanitizers, followed by the successful full rerun. Initial
failure evidence remains in `world-full-failed*`.
