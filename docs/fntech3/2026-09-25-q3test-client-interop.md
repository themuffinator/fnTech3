# Q3Test 1.02 to 1.08: client, interoperability and sound runs

## Scope

This record is the runtime evidence for the `q3test` profile described in the
[Q3Test ledger](Q3TEST.md). One developer build ran 88 probes across the six
public Win32 releases:

- fnTech3's client with fnTech3's server, with each release's retail server
  and with its own listen server, on every map the release ships;
- each retail client (1.05 to 1.08) on fnTech3's server;
- the headless protocol client with every retail server and with fnTech3's,
  including 1.02 and 1.03, which have no retail client;
- a client in slot 120 of a 128-slot fnTech3 server, for every release;
- the menus, the shipped recordings (played, time-demoed and heard),
  recordings made by fnTech3 (replayed and decoded independently), and the
  Vulkan and RTX renderers.

Every run passed. Several gates stay open, for the reasons under
[Open](#open). This record does not make the `q3test` profile complete.

## Build and inputs

| Item | Value |
| --- | --- |
| Commit | `d1d98ba8fa23363355472eab5cb2b2860a857f40` and the shared working tree's uncommitted changes, the Q3Test work included |
| Build | `meson/build/win32-q3test`, MSVC 2022 x86, `debugoptimized`, `-Dq3test-development=true` |
| `fntech3.exe` | `2874d0aa487c27f000f939f618a7946db8695385ec07dd7130e592d3294d29ac` |
| `fntech3.ded.exe` | `297d5860a3de8b16308a20ef593832143b7c9472434be972e9c0671b6de0df33` |
| `fnql_glx_x86.dll` | `abc62daab453885b898e0d94824612dc0e34d0eb962992d4d064ac35c3862c99` |
| `fnql_vk_x86.dll` | `08dcb76e301fb6db081f0c083e7edf5a75231968beed105300e954fb3295f8f1` |
| `fnql_rtx_x86.dll` | `e01452232504857c3eddb414b3715d70e43754a6d39b965a4e78d636b6105c4a` |
| `fntech3_q3test_client_probe.exe` | `f3852b11a8e9c9461148bc354a982a1ac43e6ee3bc6458bfcadcb93bb8c1f85e` |
| Machine | Windows 11 Home 10.0.26200; NVIDIA GeForce RTX 4060 Laptop GPU |
| Installations | The six Win32 packages whose archive hashes [the point-release evidence](2026-09-20-q3-point-releases.json) records, laid down read-only under `.tmp/q3test/install/<release>/`. Each executable, module and pak hash is in the JSON. |

Every run of this build reported the same three executable hashes, and every
installation hashed the same before and after each run. Seven of the 88 runs
time-demo the retail engines alone and use no fnTech3 binary. The
[JSON evidence](2026-09-25-q3test-client-interop.json) lists every run: its
scenario, command line, outcome, captures, and the probe's and the sound
check's figures, with the scripts that reproduce it. An earlier build ran the
same matrix with the same outcome before the fixes under
[Changes after review](#changes-after-review).

## Method

`scripts/probe_q3test.py` ran every probe windowed on an unswitched private
desktop, with mouse and joystick input off, loopback addresses only, an
isolated home per run, and frames taken with each engine's own `screenshot`
command. Console commands from startup configs drove every client; no key or
mouse input was simulated. The retail 1.02, 1.03 and 1.05 engines ran under
cdb with the breakpoint the ledger describes; nothing on disk was patched.

## Observed

### Sessions

| Pairing | 1.02 | 1.03 | 1.05 | 1.06 | 1.07 | 1.08 |
| --- | --- | --- | --- | --- | --- | --- |
| fnTech3 client, fnTech3 server | -- | -- | pass | pass | pass | pass |
| fnTech3 client, retail server | -- | -- | pass | pass | pass | pass |
| fnTech3 client, own listen server | -- | -- | pass | pass | pass | pass |
| retail client, fnTech3 server | -- | -- | pass | pass | pass | pass |
| protocol client, retail server | pass | pass | pass | pass | pass | pass |
| protocol client, fnTech3 server | pass | pass | pass | pass | pass | pass |
| protocol client, slot 120 of 128 on fnTech3 | pass | pass | pass | pass | pass | pass |

"--" marks a release that shipped no client. Each fnTech3 client session
reached the game and took four engine screenshots on `q3test1`; further runs
joined fnTech3's server on `q3test2` and, for 1.06 to 1.08, `q3tourney`. Each
retail client entered the game 1.0 to 3.0 s after it started and took eight
to fifteen screenshots. fnTech3's dedicated server started every map of every
release.

The protocol client stayed 8 s in each session and took 128 to 140
snapshots, one or two of them without a delta and none invalid. It walked
forward while turning, 644 to 1,671 units from its spawn point. Retail and
fnTech3 servers put it in the same slot: the first, which 1.02 to 1.05 number
1 in the player state and 1.06 to 1.08 number 0. Both servers saw it enter
and leave. With `sv_maxclients 128` and 120 private slots, it landed in slot
120 on fnTech3's server in every release (player-state number 121 in 1.02 to
1.05, 120 in 1.06 to 1.08), entered the game there and left. A retail 1.06
client landed in the same slot, and fnTech3's `status` listed it as slot
120.

### Info strings

The serverinfo and systeminfo each retail server and fnTech3's gave the
protocol client carry the same keys in 1.02, 1.03 and 1.06 to 1.08. Only the
version string and `sv_serverid` differ in value. In 1.05, retail's strings
lack the game's `gamename`, `gamedate`, `nohealth` and `teamflags` and its
`g_syncronousClients`, which fnTech3's carry. The game registers them with
the flags 1.02's game uses (1.05 `qagamex86.dll` 0x20012dda..0x20012eb7), and
retail 1.02 does publish them.

### Menus

fnTech3 drew 1.05's engine menus and the retail menu modules of 1.06, 1.07
and 1.08, each release's version string included.

### Shipped recordings

fnTech3 played one shipped recording of each client release with screenshots
and time-demoed all six. The retail engines time-demoed the same recordings
on the same machine:

| Release | Recording | Snapshot interval | fnTech3 frames | Retail frames |
| --- | --- | --- | --- | --- |
| 1.05 | `q3test1` | 50 ms | 2,703 | 9,303 (9,179 in an earlier run) |
| 1.05 | `q3test2` | 50 ms | 3,838 | 14,053 |
| 1.06 | `q3testdemo1` | 50 ms | 1,653 | 1,653 |
| 1.06, 1.07 | `q3testdemo2` | 100 ms | 1,528 | 1,528 |
| 1.08 | `q3demo1` | 50 ms | 1,107 | 1,107 |
| 1.08 | `q3demo2` | 50 ms | 819 | 819 |

1.06 to 1.08 step a time demo 50 ms per frame, as fnTech3 does. 1.05's
`CL_SetCGameTime` (0x4016f0) sets its server-time delta to the frame count
times 50 and still adds real time, so its count depends on the machine and
the run.

### fnTech3's recordings

In each client release, fnTech3's client recorded `fnrec.dm3` on its own
server (9,068 to 10,833 bytes, 225 records) and replayed it as a time demo
(223 frames). The independent decoder written from the retail engines read
every record: one gamestate and 224 snapshots, all valid. Re-encoding each
record with its reference writers reproduced every record of 1.06, 1.07 and
1.08. In 1.05 it left one record different. That record's decoded state is
identical: it carries an entity sent unchanged from an empty baseline, which
decodes as removed, and the reference writer leaves it out. Retail 1.05's own
shipped recordings carry such entities too. The same writers leave 45 and 26
records of the shipped 1.05 recordings, 2 and 5 of 1.06's, and 1 and 0 of
1.08's unreproduced, so this check is a consistency check, not a byte oracle.

### Sound

With `--video`, fnTech3 replayed each client release's first shipped
recording through the legacy mixer and wrote 29.2 s of its mix (22,050 Hz,
stereo, 16-bit). Each effect the replay started (`s_show 1`) was looked for
in that track:

| Release | Median level (100 ms RMS) | Blocks above -60 dBFS | Starts found |
| --- | --- | --- | --- |
| 1.05 | -18.4 dBFS | 92.8% | 124 of 742 |
| 1.06 | -15.5 dBFS | 100% | 244 of 649 |
| 1.07 | -20.2 dBFS | 100% | 141 of 526 |
| 1.08 | -16.5 dBFS | 98.3% | 156 of 420 |

In each release 12 to 21 effects agreed on one offset between the log and
the track. Loud one-shots were found at their logged times, among them
1.08's jumps, hit feedback and weapon pickups, the rocket explosions of every
release and 1.05's shotgun; the JSON has each effect's count. Dense
machine-gun fire and ricochets, footsteps and distant sounds overlap too much
to stand out, as in the F.A.K.K.2 check this method comes from.

### Renderers

GLx drew every session above. On `q3test2`, Vulkan (`cl_renderer vk`) and
the RTX renderer drew 1.05's IBSP 44 and 1.08's IBSP 45: RTX once with its
raster renderer (its default, `rtx_rt_mode 0`) and once path-traced
(`rtx_rt_mode 2`). The path tracer built its world from 6,489 and 6,543
triangles, lit the base lights as surface lights, and reported no fallback.

## Changes after review

A review of the Q3Test client and server sources, after the first matrix,
found defects that none of these runs reaches. They were fixed, and the
matrix above ran again on the fixed build:

- **Menu reloads.** The 1.06 to 1.08 menus run `vid_restart`, `snd_restart`,
  `map` and `demo` with EXEC_NOW from their own key handlers (the video
  menu's apply: ui1.06 0x4000db34). fnTech3 held them until the outermost
  export returned, but that is still inside `VM_Call`, where `VM_Free`
  refuses to unload the module (`VM_Free(ui) on running vm`, a fatal error).
  They now run at the start of the next client frame. No run pressed a menu
  key, so none reached it; `tests/q3test_client_tests.py` runs the production
  functions and checks that reloads issued in an export, or a nested one, run
  only at the frame hook with no module call on the stack, in order.
- **Snapshot overflow.** A configstring or command that did not fit left the
  message writer failed, so the whole snapshot was refused and the client
  stalled until it timed out. A message now carries as many as leave room for
  its snapshot; 1.02 to 1.05 send the oldest changes first and stamp the
  snapshot with the newest one sent, since the client acknowledges that
  stamp. A message that fits is written as before. The helpers are unit-tested
  (`FittingPrefix` in `tests/q3test_wire_tests.cpp`).
- **MD3 offsets.** `CM_LoadModel` added a file's frame and tag offsets to the
  tables' sizes before checking them against the file, so an offset near
  2^31 wrapped and passed. Hostile headers are now refused
  (`tests/q3test_client_tests.py`).
- **Cheat cvars.** The shared client-game start reset every cheat cvar at each
  load. No Q3Test release does that: 1.06's and 1.08's `Cvar_SetCheatState`
  (0x4116b0, 0x412050) each have one caller, `CL_SystemInfoChanged`, when the
  systeminfo turns cheats off. Q3Test now keeps to that.
- **1.05 LAN list.** Four `Info_ValueForKey` lookups in one argument list
  shared its two rotating buffers, so a compiler that evaluates arguments left
  to right showed numbers for names.
- **Delta frames.** A client that named the frame being built as its delta
  base made its own snapshots fail every frame; a number not yet sent now
  names no frame.
- **Sessions.** The server's Q3Test channels are freed when it shuts down.

Left as is: the 1.02 to 1.05 game imports `SnapshotEntityCount` and
`SnapshotEntityNumber` read frames the Q3Test path does not fill. No shipped
path can create a bot in those releases (their games' `bot` and `nav`
handlers are unreferenced), so nothing calls them.

## Inferred

- Q3Test's client games reach the engine's sound system through the same
  calls as Quake III's, so its default OpenAL pipeline -- distance model,
  occlusion and EFX, with HRTF where OpenAL Soft is present -- serves them.
  No audio-zone sidecar exists for a Q3Test map. Only the legacy mixer writes
  a capture, so only its mix was checked; the OpenAL mix was not.
- Why retail 1.05 leaves its game's keys out of its info strings is not
  identified. Its `SV_SpawnServer` publishes both strings before the game
  starts, as 1.02 does, so the difference lies in when it publishes them
  again.

## Open

- **Input.** No key or mouse input reached any client. Driving a client with
  simulated buttons needs the user's permission for that run.
- **Gameplay.** Only the protocol client moved in a live session. Combat,
  item pickups and deaths were seen and heard only in the shipped
  recordings. No movement or trace was compared with retail's.
- **Wire.** Every session had one client, on loopback, for seconds. Sessions
  with several clients, packet loss or remote peers are untested.
- **Time demos.** 1.05's frame count is not reproduced, because retail's
  depends on the machine.
- **1.05 info strings.** fnTech3 publishes the game's keys that retail 1.05
  leaves out of them.
- **Server browser.** LAN discovery and the 1999 master were not exercised.
- **HUD placement.** The client games' HUD is stretched to the window, as
  retail's was. fnTech3's widescreen HUD placement (`cl_hudAspect`) does not
  reach it, because their 2D calls go straight to the renderer. The probes
  ran 4:3 windows, where the two agree.
- **Retail readers.** A retail client cannot load fnTech3's recordings: its
  restricted demo mode reads only its pinned paks.

## Unrelated observations

- The retail servers answered `status` in their own format. fnTech3's
  `status` under `q3test` uses the engine's format, with a `steamid` column.
- `cg_stereoSeparation` is registered by the engine with `0` and by each
  Q3Test client game with `0.4`, and the engine warns about the conflict at
  every start.
