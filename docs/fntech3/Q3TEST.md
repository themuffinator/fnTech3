# Quake III Q3Test 1.02 to 1.08

**Q3Test support is not complete.** The `q3test` profile is gated by default;
`-Dq3test-development=true` enables it in Windows x86 developer builds for
isolated probes, and release builds refuse it. This ledger records what each
public Q3Test release's own engine and modules establish, what fnTech3
implements of it, the runtime evidence and the gates still open.

The inputs are the six Win32 packages from Jason Brownlee's
[Quake3OfficialArchive](https://github.com/Jason2Brownlee/Quake3OfficialArchive/tree/960182ae296be8cbcd6b59db78b31aaa9d28ed9f),
whose archive hashes the [point-release evidence](2026-09-20-q3-point-releases.json)
already records. No executable, module, pak, map or recording from them is
redistributed.

## Releases

Q3Test shipped for Windows first as dedicated servers: 1.02 and 1.03 were
server packages holding `quake3.exe`, `qagamex86.dll` and `demoq3/pak0.pk3`,
with no client game. The Windows client arrived with 1.05. Each installer puts
the executable and modules in the installation root and the paks in `demoq3/`.

| Release | Engine version | Protocol | Map format | Game library | Client game | Menus |
| --- | --- | --- | --- | --- | --- | --- |
| 1.02 | `Q3T 1.02` | 35 | IBSP 44 | `GetGameAPI` | none shipped | none shipped |
| 1.03 | `Q3T 1.03` | 35 | IBSP 44 | `GetGameAPI` | none shipped | none shipped |
| 1.05 | `Q3T 1.05` | 36 | IBSP 44 | `GetGameAPI` | `GetCGameAPI` | engine menus, no module |
| 1.06 | `Q3T 1.06` | 38 | IBSP 45 | `GetBotAIAPI`, `GetGameAPI` | `GetCGameAPI` | `GetUIAPI` |
| 1.07 | `Q3T 1.07` | 39 | IBSP 45 | `GetBotAIAPI`, `GetGameAPI` | `GetCGameAPI` | `GetUIAPI` |
| 1.08 | `Q3T 1.08` | 40 | IBSP 45 | `GetBotAIAPI`, `GetGameAPI` | `dllEntry`, `vmMain` | `GetUIAPI` |

1.07 adds `demoq3/pak1.pk3`, which replaces `maps/q3tourney.bsp`; 1.08's
`pak0.pk3` carries that newer map itself.

## Profile and release identity

`q3test` is one profile under the Quake III title (aliases `q3t` and
`quake3-test`), because the six releases share one installation shape and one
title. Their contracts are not shared: each release has its own protocol, map
version and module tables. The profile therefore pins exactly one release at
startup, from installation evidence, before the filesystem, the protocol or any
module starts, and keeps it for the process's lifetime
(`code/qcommon/q3test_release.h`, `code/qcommon/q3test_install.hpp`).

Recognition reads, without loading any module:

- the root's `qagamex86.dll`: an x86 PE image exporting `GetGameAPI`, whose COFF
  `TimeDateStamp` names the release, and which exports `GetBotAIAPI` exactly
  when that release's does;
- `cgamex86.dll` and `uix86.dll` when present: each must carry the same
  release's stamp and entry points (`GetCGameAPI`, or `dllEntry` and `vmMain`
  for 1.08; `GetUIAPI`). A dedicated-server installation may omit them;
- `demoq3/pak0.pk3`'s central directory: `default.cfg` (1,634 bytes, CRC
  `022ce58a`, identical in every release) and `maps/q3test1.bsp`, whose size
  and CRC name the pak the release shipped. The installers wrote the map name
  in capitals up to 1.05, so it is compared case-insensitively.

| Release | `qagamex86.dll` stamp | `cgamex86.dll` stamp | `uix86.dll` stamp | `maps/q3test1.bsp` |
| --- | --- | --- | --- | --- |
| 1.02 | `0x37228c03` | -- | -- | 5,818,936 bytes, CRC `00610dfe` |
| 1.03 | `0x3727df48` | -- | -- | 5,818,936 bytes, CRC `00610dfe` |
| 1.05 | `0x37369456` | `0x37369443` | -- | 5,304,664 bytes, CRC `5024a87a` |
| 1.06 | `0x378fba49` | `0x378fba41` | `0x378fba4d` | 5,480,056 bytes, CRC `d02db1a3` |
| 1.07 | `0x37940ae0` | `0x37940ad8` | `0x37940ae5` | 5,480,056 bytes, CRC `d02db1a3` |
| 1.08 | `0x37a67322` | `0x37a671e0` | `0x37a67da4` | 5,480,056 bytes, CRC `d02db1a3` |

A module from another release, a missing or foreign pak, a changed
`default.cfg` or a duplicated map entry makes the installation unrecognized,
and startup then stops with a message naming the six releases. The stamps name
builds; they do not authenticate them. The pinned release's protocol is the
profile's only protocol: 40 is also Elite Force's, Jedi Academy's and Jedi
Outcast's single-player number, and the other five Q3Test numbers belong to no
other profile. Only the pinned release's map version is read.

`tests/q3test_identity_tests.cpp` builds synthetic installations for every
release and checks each rejection above. Given installation paths it also
recognizes real ones read-only: all six Win32 layouts are recognized.

## Retail engines on this machine

The retail 1.06, 1.07 and 1.08 engines start as dedicated servers and as
windowed clients on a private desktop, and play their shipped recordings
(`timedemo 1`, 1.08's `q3demo1`: 1,107 frames). The 1.02, 1.03 and 1.05
engines fault during network startup: their `NET_GetLocalAddress` walks the
host's address list two entries at a time (it increments its index both in
the loop condition and in the body; 1.05 `0x004291c1..0x00429271`), so with an
odd number of IPv4 addresses it reads past the list's terminator. 1.06 fixed
the loop. For probes, a debugger breakpoint that ends the loop before its
first entry (`test esi,esi` at 1.02 `0x00428265`, 1.03 `0x00428a35`, 1.05
`0x004291e5`) lets these engines start with no local addresses recorded; the
1.05 client then plays `q3test1` (9,179 frames). No retail file is modified.

Every retail engine runs in its restricted demo mode (`FS_SetRestrictions`):
without its product file it mounts only `demoq3`, requires each pak's pinned
checksum and reads no loose file but configs (1.02 to 1.05 only
`q3config.cfg`). A retail client therefore cannot load a demo, map or pak made
elsewhere; the probes do not lift that restriction.

## What fnTech3 runs

### Maps and collision

`code/qcommon/bsp_q3test.h` translates both map versions to IBSP 46 in
memory for the collision loader and all three renderers: IBSP 44 gains a
shader lump built from its surfaces' names, planar faces become fans, patch
bounds become LOD bounds, submodel ranges come from each model's head node,
and fogs gain a visible side; IBSP 45 changes its plane, model and fog record
sizes. Collision keeps Q3Test's own curve rules (`code/qcommon/cm_patch.c`):
facets of at most four borders, no bevels, facets wider than 4096 units
dropped, every trace against curves using 1.32's point algorithm with the box
offset, 1.02-1.05's patch contents forced from the one `powerupshit` shader,
and 1.06-1.07's surface flags read from the shader's contents. A trace that
starts and ends inside a brush keeps the contents an earlier brush wrote, as
every release's brush sweep does. Every map of every release loads
(`tests/q3test_bsp_tests.cpp`), and every release's retail server answers
the same world traces as fnTech3's
([trace record](2026-10-08-q3test-traces.md)).

### Server

A developer dedicated server runs each release's retail game module through
its own table (`code/server/server_q3test_game.hpp`) and speaks its client
session (`code/server/server_q3test_net.hpp`): the release's connect text and
challenge, one of Q3Test's two netchans (1.02-1.05's IHV-shaped channel with
one reliable block; 1.06-1.08's fragmenting channel with a per-byte
scrambler; `code/compat/q3test_netchan.hpp`), message bodies through the
verified codec (`code/qcommon/q3test_wire.hpp`), the retail snapshot entity
walk and usercmd rules. When a message cannot hold every unacknowledged
configstring or command with its snapshot, it carries as many as leave room
for the snapshot and the rest follow: 1.02-1.05 send the oldest changes first
and stamp the snapshot with the newest one sent, which is all the client then
acknowledges. A client that names a delta frame not yet sent gets a full
snapshot. Its serverinfo and systeminfo are each release's own:
the engine's serverinfo cvars that release had (1.02's `hostname` and
`privateclients`, 1.06 and 1.07's `sv_zone`, 1.02-1.05's `cheats`) and the
game's, not this engine's download, rate and flood keys; the version string is
fnTech3's. 1.08's game reads the same string through `GetServerinfo`. A server
takes up to 128 clients, as every release clamps `sv_maxclients` (1.02
`SV_Init` 0x41c126, 1.06 `SV_BoundMaxClients` 0x41cb30). Private slots are
each release's: 1.02 keeps `privateclients` slots for a client whose password
is the rcon password, when one is set; 1.03 and later keep
`sv_privateClients` slots for one whose password equals
`sv_privatePassword`, set or not, so with no private password every client
may take them.

### Client

The client session (`code/client/cl_q3test.cpp`) is each release's: the
connect text, the netchan, the retail send rules (35/36's provisional last
command, 38-40's repeated commands) and every systeminfo key applied to
client cvars as retail's `CL_SystemInfoChanged` does. The client games run
through `code/client/cl_q3test_cgame.cpp`: 1.05's 64-word table that pushes
configstrings and server commands, 1.06 and 1.07's 57-word table, and 1.08's
syscalls, each with its own snapshot, gamestate, glconfig, refdef, entity,
trace, cvar and usercmd records. The menus (`code/client/cl_q3test_ui.cpp`)
are 1.06-1.08's `uix86.dll` with its 51- or 55-word import table and twelve
exports, with the engine pushing the connection screen's strings; commands
the module runs immediately that would reload it (`vid_restart`,
`snd_restart`, `map`, `demo`) wait for the next client frame, when no call
into the module is on the stack. Retail freed and reloaded `uix86.dll` under
its own frames and survived only because Windows mapped it at the same base.
1.05 shipped no menu module: its engine menus are this engine's own
(`code/client/cl_q3test_menus.cpp`), driving the same cvars and commands.
As in every release, only a systeminfo that turns cheats off resets the
cheat cvars; loading a client game does not.
The server browser keeps Q3Test's LAN list, master list and ping queue
(`localservers`, `globalservers <protocol> [zone]`, `ping`). `.dm3` demos
record and play as each release's `CL_Record_f` and `CL_ReadDemoMessage`
wrote and read them.

### Presentation

Menus, HUD and world are drawn with fnTech3's renderers and features. The
releases' own data is read as they read it (AGENTS.md invariant 11):

- **Shader dialect** (`code/renderercommon/tr_q3test_shader_dialect.h`):
  `fogParms r g b depth type` with five bare values, whose type's sign picks
  the fog volume's visible face; `fogonly`; `foggen`, which only an unused
  test shader carries; and `clampTexCoords`, which clamps its stage's map
  wherever it stands in the stage. 1.02-1.05's stageless sky shader is drawn
  black, as 1.06 gave the same shader a black stage.
- **gfx.shader repair** (`code/renderercommon/tr_q3test_shader_text.h`):
  1.06-1.08's `gfx/2d/crosshairc` closes its stage with `{`. Retail found
  shaders by the first token of any line, so only that shader failed and
  became the default shader; this engine's brace-checked scan would have
  dropped the whole file. Five bytes of those exact files are rewritten in
  memory to the same outcome.
- **2D pictures** (`code/renderercommon/tr_q3test_2d.h`): retail's
  `RB_StretchPic` drew a picture's first image alpha-blended whatever its
  shader said, and the menu art relies on it (`gfx/2d/menuinfo2` is white
  under an alpha mask). Q3Test pictures are drawn with the engine's 2D shader
  of that image.
- **Entity light:** 1.06-1.08 maps carry a light grid. IBSP 44 has none; its
  entities take the no-grid light, as 1.06-1.08 do without one, instead of
  1.02-1.05's lightmap sample beneath the entity.
- **HUD:** the client games draw their HUD on a 640x480 screen that is
  stretched to the window, as retail did. fnTech3's widescreen HUD placement
  (`cl_hudAspect`) does not reach them yet: their 2D calls go straight to the
  renderer, not through `code/client/cl_hud.cpp`.
- **Sound:** the client games start and loop their sounds through the
  engine's sound calls, so fnTech3's OpenAL pipeline (distance model,
  occlusion, EFX, and HRTF where OpenAL Soft is present) plays them. No
  Q3Test map has an audio-zone sidecar.

## Probes

`scripts/probe_q3test.py` runs everything windowed on an unswitched private
desktop, with engine screenshots only, isolated homes and read-only
installations:

| Scenario | What it runs |
| --- | --- |
| `retail-demo` | a retail client plays a shipped recording, optionally as a timedemo |
| `retail-server` | a retail dedicated server answers `getinfo` and `getstatus` |
| `fntech3-server` | `fntech3.ded.exe` starts maps |
| `retail-client` | `fntech3.ded.exe` serves a map to the release's retail client |
| `fntech3-client` | `fntech3.exe` joins `fntech3.ded.exe` (`--target fntech3`), the retail server (`retail`) or its own listen server (`local`); shows its menus (`menu`); plays or records demos (`demo`, `--record`); with `--video`, records a replay's sound mix and looks for each effect in it |
| `protocol-client` | the headless protocol client joins the retail server or `fntech3.ded.exe` |

Nobody hears a probe: a client on a private desktop mixes into a silent
buffer. `--video` records a demo replay with the engine's `video` command
through the legacy mixer, which writes that mix beside the frames, and prints
each effect the replay starts (`s_show 1`, at the painted sample time). The
probe keeps the track as a WAV and looks for every started effect in it by
normalised cross-correlation with the effect's own WAV from the release's
paks: a start counts as found when the correlation within 60 ms of it beats
the 99th percentile of the same effect at 400 places away from its starts,
after one offset that the strongest matches agree on aligns the log with the
track (`scripts/probe_ritual_audio.py`).

`fntech3_q3test_client_probe` (`tests/fixtures/q3test_client_probe.cpp`) is
that headless client: it speaks any release's client protocol through the
same codecs, joins, runs forward while turning, acknowledges what it receives,
leaves, and prints a summary, with the slot its player state names. For 1.02 and 1.03, which have no retail client,
it is the peer for the server's live gates: run against the retail server it
shows the probe follows retail, and run against fnTech3's it shows the server
does.

## Evidence

The [client and interoperability record](2026-09-25-q3test-client-interop.md)
lists every run with its executable hashes, commands and results: 88 probes
of one build, all passed, across every pairing below.

## Gate ledger

| Gate | Status | Evidence and remaining work |
| --- | --- | --- |
| Profile / identity | PARTIAL -- Unit, Live | Release pinning and recognition as above; every release activates from its installation on the client and the server. |
| Filesystem | PARTIAL -- Live | `demoq3` mounts and each release's paks load. The retail restricted mode's pak pins and loose-file rule are not enforced; fnTech3 reads its own home as for every profile. |
| Map format / collision | PARTIAL -- Unit, Live | Every map of every release loads and is played on; Q3Test's curve rules are unit-tested; 64,800 world traces and 18,000 point contents on every release's retail server agree with fnTech3's in every hit, flag and contents, and within 0.0026 units, since a trace left inside a brush keeps its contents as every release's does ([trace record](2026-10-08-q3test-traces.md)); entities and brush models are not compared. |
| Module ABI | PARTIAL -- Live | Every release's game module hosts every map it ships; 1.05-1.08's client games and 1.06-1.08's menus run in fnTech3's client. No bots are claimed: no release shipped a bot library. |
| Wire (both directions) | PARTIAL -- Interop subset | Short one-client sessions on loopback: fnTech3's server with each retail client (1.05-1.08) and with the protocol client (every release, also in slot 120 of 128); fnTech3's client with each retail server; the protocol client with every retail server, which gives it the same slot and, but for 1.05, the same info keys. A retail 1.08 client and fnTech3's client share fnTech3's server, and both share a retail 1.08 server, for five minutes at 10% loss through a map change; the retail client hears every chat line of fnTech3's, and cannot be made to answer, as retail reads its config files only at startup ([sustained sessions record](2026-10-08-legacy-sustained-sessions.md)). No session with a remote peer. |
| Demos | PARTIAL -- Live | The six shipped recordings time-demo in fnTech3, 1.06-1.08's to retail's frame counts; fnTech3's recordings replay in fnTech3 and an independent decoder reads every record. 1.05's machine-dependent time-demo count is not reproduced, and a retail client cannot load fnTech3's recordings (restricted mode). |
| Renderer / assets | PARTIAL -- Live | Every map of each client release draws with menus and HUD in GLx; Vulkan and the RTX renderer, raster and path-traced, draw both map formats. Entity light on IBSP 44 is not retail's lightmap sample. |
| Audio / input | PARTIAL -- Live (audio) | Each client release's replay mixes its recording's effects at their logged times in the legacy mixer's capture; the OpenAL mix is not captured. No input reached a client: simulated buttons need the user's permission per run. |
| Gameplay | PARTIAL -- Interop subset | The protocol client moves on every retail and fnTech3 server; combat, pickups and deaths are seen and heard only in the shipped recordings. No live play or movement comparison with retail. |
| Server browser | PARTIAL -- Source | LAN, master and ping paths implemented; no live LAN or master run recorded (the 1999 master is gone). |
