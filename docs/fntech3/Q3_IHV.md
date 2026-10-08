# Quake III IHV compatibility

**Full IHV support is not implemented.** The separate `q3-ihv` profile is gated
by default. `-Dq3-ihv-development=true` enables Windows x86 dedicated probes of
the original game DLL with all four supplied maps, and a Windows x86 developer
client that runs the original client game, plays the supplied `sli.dm3` and
joins the original dedicated server (see [below](#developer-client-and-demo-playback)). BSP 43 map loading and native
game lifecycle dispatch now have runtime evidence. Server link/trace decisions
and protocol-33 state, message and demo codecs have isolated test evidence, but complete engine integration,
native gameplay, rendered demo playback and live interoperability remain open.
Loading these maps with retail Q3 QVMs does not run the IHV game.

The supplied reference is `E:/Repositories/Quake3-IHV/ref/install`, from
[Quake3-IHV](https://github.com/themuffinator/Quake3-IHV/tree/d5ce877155904c57f8b28fe0fd0c8aa2e9473337).
The repository describes a reverse-engineering project but currently supplies
reference binaries/assets, not reconstructed engine or module source. Binary
observations, hashes, commands and outcomes are in the
[2026-09-20 evidence record](2026-09-20-q3-ihv.json). No reference executable,
module, map, recording or other proprietary asset is incorporated into fnTech3.
The DLLs/executable are ignored by the reference repository, so the recorded
hashes, rather than its commit alone, identify those supplied inputs.

## Contract ledger

| Surface | Observed reference | fnTech3 evidence and remaining work |
| --- | --- | --- |
| Identity/filesystem | `4.01 x86 Feb 12 1999 RELEASE`, supplied loose `baseq3` assets, module DLLs beside the executable; original also contains `pak%i.pk3` lookup | **PARTIAL — Unit/Fixture/Live:** separate `q3-ihv` profile, bounded Win32 export/BSP43 metadata detection, native module names and root containment. Mixed retail/IHV markers fail as ambiguous. Dedicated startup validates loose installation evidence instead of retail pak checksums. Native reads own their buffers; writable paths are bounded to the isolated home/game. **OPEN:** full original filesystem/pak contract; automatic recognition covers the supplied loose installation. Other profiles retain their pak checks. |
| Game module | `qagamex86.dll` exports `GetGameAPI`; returns API 4; copies 35 import words; advertises an entity stride of 556 bytes | **PARTIAL — Unit/Build/Fixture/Live:** API 4 dispatch now starts the original DLL, spawns all four supplied maps, runs frames, preserves game allocations between maps and shuts down cleanly. Separate cvar/entity records, allocation validation and partial-startup cleanup have executable tests. All thirty-five imports are implemented: the bot lifecycle, bot scheduler, snapshot getters, the snapshot builder that fills their history and the protocol-33 message that carries a frame all use native client records compared against the original functions. **OPEN:** transport, shared player projections, client/gameplay callbacks and parity. This is not retail Q3 GAME API 8 or its `dllEntry`/`vmMain` interface. |
| Client module | `cgamex86.dll` exports `GetCGameAPI`, returning eight callbacks; its Init copies 62 import words | **PARTIAL — Unit/Source/Fixture/Live:** exact client records and ordered delivery; an isolated host completes original Init, all 56 `sli.dm3` frame updates and draw callbacks without patching cgame. The developer client now hosts the module with all 62 imports served by engine services -- collision, renderers, audio, 2D -- and plays `sli.dm3` to its end on GLx, Vulkan and RTX ([client record](2026-10-08-q3-ihv-client.md)). Live, it predicts against the original dedicated server ([live client record](2026-10-08-q3-ihv-live-client.md)). Its 2D art takes the colour the module sets, and its overlay square is drawn ([commands and 2D record](2026-10-08-q3-ihv-commands-and-2d.md)). Its particles match the original's and its damage overlay draws the splat's image in the overlay colour ([overlay and particles record](2026-10-08-q3-ihv-overlay-and-particles.md)). **OPEN:** the build's menus, input, why the original's splat leaves no mark. |
| Server protocol | Original dedicated server starts `tim_dm1`; `getinfo ihvprobe` returns an `info` text reply advertising 33 | **PARTIAL — Unit/Fixture:** separate entity/player/usercmd codecs and transactional gamestate/configstring/snapshot/command message readers and writers, with ordered events. The dedicated server now builds each client's snapshot message, byte-identical to the original writer across 1,536 generated scenarios, and its netchan matches the original's packets byte for byte across 5,202 of them. The query reply is not the retail `infoResponse` key/value format. Sessions run in both directions: the fnTech3 client joins the original server ([live client record](2026-10-08-q3-ihv-live-client.md)), and the original client joins fnTech3's ([server sessions record](2026-10-08-q3-ihv-server-sessions.md)). Both clients share a game for three minutes while fnTech3's processes drop 10 to 25% of the packets they receive, through level changes ([loss and level change record](2026-10-08-q3-ihv-loss-and-level-change.md)). Its serverinfo is the original's key set and order ([serverinfo record](2026-10-08-q3-ihv-serverinfo.md)). **OPEN:** fought play, remote peers. |
| Demo container | `.dm3` records contain a signed length followed by message bytes, without the later per-record server sequence word | **PARTIAL — Unit/Fixture/Live:** bounded length-only reader/writer and protocol-33 message parsing; `demo` plays a record at a time as the client game asks ([client record](2026-10-08-q3-ihv-client.md)). `record` writes the original's form, and recordings play in both engines ([recording record](2026-10-08-q3-ihv-demo-recording.md)). This is not the later raw-43 `.dm3` contract. |
| `sli.dm3` | 57 message records plus a `-1` length terminator; initial configstrings advertise 33 and `km_portal` | **Fixture and reference runtime:** isolated codecs parse/regenerate all 57 messages; 55 are byte-identical. All 56 native snapshots and 680 entity states match the original parser. A later isolated cgame host completes Init, unmodified snapshot callbacks and 56 frame/draw updates, capturing 124 scenes with synthetic collision/resource services. The original engine completes timedemo playback. The fnTech3 developer client plays it to its end through the original client game on all three renderers ([client record](2026-10-08-q3-ihv-client.md)). |
| `berj.dm3` | One 879-byte configstring block; no gamestate opener, snapshots or terminator | **Malformed/incomplete fixture:** the new message decoder rejects it cleanly in x86 and x64 tests. The original engine exits with access violation `0xC0000005`. Successful playback cannot be inferred from the filename. |
| `candy.dm2` | Quake II serverdata opcode 12, protocol 34 | Outside the Q3 IHV demo codec. Its presence in this directory does not make it a Q3 recording. |
| BSP geometry | All four maps are IBSP 43 with a 14-lump directory | **PARTIAL — Unit/Build/Fixture/Live:** all translate, load collision/world data, produce engine screenshots and exit normally in GLx, Vulkan and RTX using retail Q3 QVMs. Material parity and original collision behavior remain open. |
| Server link geometry | `SV_LinkEntity` at `0x41b970`, 16-bit solid packing, eight-unit bounds, brush sentinel 31 and state/current transform selection | **PARTIAL — Unit/Fixture:** 2,740 synthetic comparisons match original solid/bounds results with x87 precision 53. The actual engine link branch now passes those vectors through the native record adapter without changing padding/private bytes. IHV's Quake II-style cluster record is stored only for this profile and drives its snapshot visibility. The reference BSP query is stubbed empty; original PVS/link membership and complete BSP collision parity remain open. |
| Server sweeps/point contents | `SV_Trace` at `0x41c110`, entity clipping at `0x41bf80`, point contents at `0x41c280`; world entity zero and native owner/transform rules | **PARTIAL — Unit/Fixture:** actual server functions match 4,737 original sweeps and 641 point queries through native records and shared engine records. Includes result replacement, owner filtering, sweep bounds and state/current transforms. Original CM replies and contact enumeration are stubbed; full BSP collision, native import dispatch and gameplay remain open. |
| Models | All 111 supplied `.md3` files have `IDP3` / version 15 headers; cgame reads a 112-byte collision-model descriptor and its 56-byte frame records directly | **PARTIAL — Unit/Fixture:** bounded frame/tag metadata, stable native x86 model mirrors and 112-byte tag interpolation. 79,625 original-function comparisons include all supplied models; 243,600 pose components match bit for bit in the recorded x86 run. Native MD3 vertex/normal interpolation and origin-expanded culling now have original-function comparisons across all renderers and RTX rays, including 5,408,046 components from all 111 models. Native animation controls and model-light sampling now have original-function evidence below. Native scene integration and rendered parity remain open. |
| Particles | 16-byte points become view-facing triangles; original procedural texture and premultiplied blend state | **PARTIAL — Unit/Build/Fixture:** frame-owned submission and per-view expansion in GLx, Vulkan and RTX, including RTX ray geometry. 1,130,688 original vertex components and all 1,024 texture bytes match. Native scene integration and rendered demo comparisons remain open. |
| Entity effects | Separate 100-byte native records, native view/depth/origin flags, special `*beam` model | **PARTIAL — Unit/Build/Fixture:** owned renderer batches, visibility, camera axes, origin interpolation and native beam geometry/material across GLx/Vulkan/RTX, including rays. All 166,656 captured beam components match. Flag `0x80` is explicitly rejected pending sorted transform/lighting retention. Native scene dispatch and retained transform/lighting remain open; portal views follow the original's pass, with no range and its camera roll ([stageless and portals record](2026-10-08-q3-ihv-stageless-and-portals.md)); model-light sampling and animation controls have fixture evidence below. The animation follow-up below implements native MD3 interpolation and bounds. |
| Materials/fog/sky | Original dialect includes flat five-argument `fogparms`, `foggen`, `fogonly`, `cloudparms`, `clampTexCoords`, `surfacelight` and `rgbGen fromClient` | **PARTIAL:** legacy sky/cloud/color/depth commands and the supplied malformed `common.shader` are handled. Flat fog parameters and animated per-vertex density now have original-function comparisons and runtime evidence in all three renderers. Cloud colors and lightning now match original-function numeric observations and reach all three renderers. A script with no stage that is neither sky nor fog draws the image of its name, as the original's `R_FindShader` (`0x439fd0`) does ([stageless and portals record](2026-10-08-q3-ihv-stageless-and-portals.md)). Clamp semantics and original lighting parity remain open. |
| Navigation/gameplay/audio/input | Original `tim_dm1` server loads its `.nav` data and game DLL; original client runs `sli` with sound/input disabled | **PARTIAL — Unit/Fixture/Live:** fnTech3's dedicated adapter lets the original DLL load `tim_dm1.nav` (86 surfaces, 319 neighbors). The native bot lifecycle, scheduler, snapshot getters and snapshot selection match the original functions. **OPEN:** IHV game rules, bot behavior, original BSP visibility parity, navigation behavior, renderer effects, audio and input contracts. No interactive input was exercised. |

Public Q3Test builds use other protocols and BSP 44/45. They remain tracked in
the [point-release ledger](Q3_POINT_RELEASES.md); neither their contracts nor
retail Q3's contracts establish IHV compatibility.

## Profile identity and server link geometry

The `q3-ihv` catalog entry shares Quake III's title identity while keeping its
installation marker, native API, protocol 33, BSP43 policy and config selection
separate. Existing profile IDs retain their values. The profile permits only
the supplied Win32 x86 `qagamex86.dll` / `cgamex86.dll` module names, with no
QVM, `vmMain`, QL structured API or UI DLL fallback. It has no Steam identity,
Workshop or retail pure-proof contract. Default builds remain blocked. The
developer option is restricted to Windows x86. Its server takes protocol-33
clients, and its client plays demos and connects to the original server.

## Developer client and demo playback

The [client record](2026-10-08-q3-ihv-client.md) runs the original
`cgamex86.dll` inside the developer client. Its 62-word import table is read
from `quake3.exe`'s `CL_InitCGame` (`0x405e30`), and its eight exports from
`GetCGameAPI` (`0x10009590`). The engine pushes it each snapshot, configstring
and server command as the original does, and asks it every frame whether it
can draw, reading another demo record whenever it cannot. Reading the original
corrected the 112-byte model record, whose headnode is at byte 24, and showed
that import 16 is the brush query, not a leaf query. `sli.dm3` plays to its
end on GLx, Vulkan and RTX, and with OpenAL sound. The
[commands and 2D record](2026-10-08-q3-ihv-commands-and-2d.md) colours its 2D
art as `Draw_StretchPic` (`0x43fd60`) does, draws the overlay square of
`R_BlendBlobs` (`0x442700`) and lets console commands reach the server's
game. The
[stageless and portals record](2026-10-08-q3-ihv-stageless-and-portals.md)
draws a stageless script's image, as the original's `R_FindShader`
(`0x439fd0`) does, and portal views as its portal pass (`0x42c160`) does: at
any distance, with the camera's roll. `km_portal`'s walls draw black and its
portal shows the turning view, as in the original. The
[overlay and particles record](2026-10-08-q3-ihv-overlay-and-particles.md)
compares the client game's particles with the original's, which match, and
draws its damage splat's image in the overlay colour, as `R_BlendBlobs`
(`0x442700`) does. The build's menus remain open.

The [live client record](2026-10-08-q3-ihv-live-client.md) connects it. The
handshake is Quake II's -- `getchallenge`, `challenge`, `connect 33 <qport>
<challenge> "<userinfo>"`, `client_connect` -- and the move is Q3Test 1.02's,
with the client game's provisional command and the frame's ping read from
`quake3.exe`'s own `CL_SendCmd` and `CL_ParseFrame`. On loopback the client
enters the original server's `tim_dm1`, receives its frames and leaves
cleanly.

The [server sessions record](2026-10-08-q3-ihv-server-sessions.md) serves the
original client from fnTech3's dedicated server. Its connectionless verbs,
connect, gamestate, moves, server commands, configstring repeats, rate
schedule, ping, level changes and drops are read from `quake3.exe`'s own
server (`0x41a560`, `0x4181d0`, `0x418750`, `0x418cc0`, `0x419e90`,
`0x41af60`, `0x41b520`, `0x41a910`, `0x419966`, `0x4186e0`). The original
client joins `tim_dm1` on loopback, plays and leaves cleanly. Beside
fnTech3's own client it survives a level change, and a kick reaches it as a
clean disconnect. The
[loss and level change record](2026-10-08-q3-ihv-loss-and-level-change.md)
keeps both in one game for three minutes under simulated loss. It fixes
fnTech3's client, which after a level change sent no move until its command
count passed the previous level's: the gamestate now clears its ring of sent
packets, as the original's `CL_ClearState` (`0x404ac0`) does.

The [recording record](2026-10-08-q3-ihv-demo-recording.md) adds `record`
(`CL_Record_f` `0x404560`). The first record is a gamestate of the client's
configstrings and the baselines that carry a model. Each message after the
channel's header follows once a full frame ends the wait (`0x405812`,
`0x406af4`), and `stoprecord` writes one -1 (`0x4044f0`). Recordings play in
both engines, and the original client's gamestate record re-encodes
byte-identically. The original client's parser has no download message
(`0x407188` sends opcode 6 to "Illegible server message"), so no IHV peer can
carry a download.

## Dedicated native game integration

The [game adapter record](2026-09-21-q3-ihv-game-bridge.json) covers the actual
engine's API 4 lifecycle. It retains the initialized native game and its owned
allocation across hunk resets, while resetting level strings, collision links
and level allocations. `SpawnEntities` receives time 1000; the settling callbacks
receive 1000, 1100 and 1200. The modern `map_restart` command uses this same level
spawn path. A complete server shutdown calls native `Shutdown` once; failed
initialization reclaims host ownership without calling a partially initialized
module. Getter and shutdown failures also have synthetic C-error-unwind tests.

`scripts/probe_q3_ihv_game.py` runs all four supplied BSPs, repeats `tim_dm1`,
then shuts down and loads `ihv_test1` again in one process. The first five spawns
retain the same native entity base and 556-byte stride. Both complete shutdowns
leave zero allocations before host cleanup. Separate processes reject an
oversized native allocation and `sv_maxclients` exceeding the game's actual
client allocation, then exit through normal engine error cleanup. Supplied
input identities are checked before and after. The writes stay in fresh probe
homes; the original installation remains unchanged.

The import table marshals native cvars, tag/file ownership, arguments, commands,
configstrings, brush models, traces, point contents, PVS/area queries, links and
contact lists. Debug graphs are absent on the dedicated server. The bot
lifecycle and snapshot getters are described below. Native client
connect is refused and protocol-33 packets never enter the retail dispatcher.
These are empty-server lifecycle probes, not connected gameplay evidence.
Collision facet warnings remain visible; matching original BSP/PVS behavior
requires separate comparisons.

The native systeminfo string uses `serverid` and `mapChecksum`, observed at
original addresses `0x41986e..0x4198bd`, and is available before map spawning.
Retail systeminfo strings remain unchanged for the other seven profiles.
The module's cvars use separate stable 32-byte records and native flag mapping;
the module never receives an engine `cvar_t`. Read-file buffers are released
immediately after making native-owned copies. `FileForWriting` validates the
home/game path and both intermediate path lengths before creating directories;
it does not open or truncate the target file.

The original server defaults `sv_maxclients` to eight, resets values less than
or equal to one to eight, and caps values above 256 at 256 (`0x419acb`,
`0x419c71..0x419cc1`, float constant `0x45b4a0`). Fifteen direct calls to the
original clamp suffix verify those boundaries, including the next representable
float above one and 256, infinities and NaN (which resets to eight). Only the
cvar-set and integer-formatting calls are replaced. The game DLL's separate
`maxclients` cvar is limited
to `maxentities / 8` by its initializer at RVA `0xf9ba..0xfa03`. The earlier
128-client/64-entity probe therefore reduced the game count to eight; that
observation does not establish an eight-client engine limit. The catalog now
records the engine limit of 256 and default of eight. Synchronization of the
two cvars and server storage above the current 128-client internal limit remain
native integration gates.

Four more isolated original-DLL Init/Shutdown probes confirm 16 and 128 clients
with 1,024 entities, reduction of a requested 256 clients to 128 at that capacity,
and 256 clients with 2,048 entities. All release their allocations. These are
allocation observations; they do not establish a working 256-client engine
session or compatibility of every entity index with the 10-bit wire format.

A separate callback host calls the original `ClientConnect` after Init. A wrong
password returns `Invalid password` without publishing a client pointer,
configstring or broadcast. Accepted slots zero and seven point from entities
one and eight into the original client allocation at offsets zero and 10,248;
both publish connecting state one at client offset 276 and configstrings 544
and 551. The host captures the original userinfo strings and broadcasts, then
Shutdown releases all allocations. This exercises no map spawn, ClientBegin,
ClientThink, handshake or network session; those integration gates remain open.

IHV has zero retail AAS worlds: its navigation and bots belong to the native
game. Retail botlib setup, AAS setup/map loading, action allocation and the
fixed goal/movement pools reject this profile without allocating. Regression
tests cover those rejections explicitly; successful AAS fixtures exercise the
seven profiles that own that subsystem. This keeps IHV's larger server limit
out of the retail bot arrays without asserting native bot compatibility.

Automatic recognition requires a nonempty `baseq3/default.cfg`, one of the four
known maps with a bounded IBSP43 lump directory, and the two native modules
with `GetGameAPI` / `GetCGameAPI` exports. The bounded PE metadata reader checks
x86 PE32 DLL headers, sections, export arrays, ordinals and exact names; it
rejects overlapping mappings, forwarded/duplicate matching exports and paths
that resolve outside the installation. It never loads a DLL. This is identity
evidence, not executable authentication or complete PE/BSP validation; the
module loader and asset parsers retain their own responsibilities. Mixed
retail and IHV evidence is ambiguous even though both belong to the same title.

IHV link geometry packs `x | down<<5 | up<<10` into 16 bits, with each component
quantized by eight, limits 31/31/63 and a +32 upper-Z bias. Brush models use
31. The original contents test excludes only bits `0x04000000` and `0x40000000`;
retail Q3's solid/body test produces different results. The native flag `0x80`
chooses current origin/angles; otherwise IHV links from the state origin/angles.
Rotated brush bounds use a radius from their maximum absolute extents. Binary32
bounds are stored before adding the one-unit pad. Integer conversion preserves
the original signed-int64 truncation and low-dword wrap before clamping.

The independently written helper and actual `SV_LinkEntity` branch pass 2,740
numeric vectors with FNV-1a64 `7c40185466a69dbd`, including threshold boundaries,
every contents bit, rotations, both transform choices and large finite extents.
The optional reference host runs the hash-pinned original routine, its original
radius and integer-conversion routines, with x87 precision set to 53. It replaces
only the BSP leaf query with an empty result. These comparisons establish link
geometry under that control mode, not original BSP queries, native module
binding or gameplay. NaN, infinity, inverted and overflowing bounds are rejected
before replacing prior output or unlinking an existing entity. Synthetic engine
tests also check unchanged retail Q3, QL, both RTCW, ET and both Elite Force link
policies. The standalone bounds suite passes x64 AddressSanitizer with fast math.

The [identity and collision evidence](2026-09-21-q3-ihv-identity-collision.json)
records source identities, commands and validation. Reproduce original numeric
observations with an x86 host built at image base `0x400000`, as for the wire
probe, using `tests/q3_ihv_reference_collision_probe.cpp` and the supplied
`quake3.exe`. Run `fntech3_q3_ihv_identity_tests.exe` with the installation path
to verify detection read-only; the ordinary CI tests use synthetic inputs only.
Useful logs remain under `.tmp/q3-ihv/identity-*`, `collision-*`,
`reference-collision-*`, `reference-server-limit.*`, `reference-client-*`,
`clients-*` and `server-world-tests.log`. The complete Windows x86 regression
run passes 126 Meson targets with two skips and no failures; its Python target
passes 1,837 cases with 72 skips in 666 seconds. The record also retains the
initial failed catalog/bot-boundary run and the corrections it required.

## Native bot clients, scheduling and snapshot history

The last five imports now run against native server client records that the
adapter owns. They are separate storage: the retail `client_t`, `playerState_t`
and `entityState_t` records have different layouts, and no IHV player passes
through the retail player projection. The records outlive a level spawn, as the
original's client array does, and a complete shutdown releases them. Evidence
and commands are in the
[bot client record](2026-09-21-q3-ihv-bot-clients.json).

`BotAllocateClient` (`0x417640`) takes the lowest free slot, publishes entity
`slot + 1` and that entity's signed-short number, marks the client connected,
sets the last-packet time to the server time and clears the outgoing message.
A full client array returns null rather than failing. The original compares the
slot against the `maxclients` cvar's float value, so a fractional setting would
admit one more slot; fnTech3 sizes the array from the integer the rest of its
client accounting already uses. `BotFreeClient`
(`0x4176e0`) only marks the slot a zombie and clears the visible name; it calls
no game export and keeps the entity association, so a zombie is not a free slot
and the scheduler still visits it. Entity cleanup remains the game's
responsibility. `BotSetUserinfo` (`0x417720`) copies at most 1023 bytes
into the client's own record, hands the module that copy through
`ClientUserinfoChanged`, then rereads it exactly as `SV_UserinfoChanged`
(`0x418ab0`) does: the name is copied with a 31-character limit, an absent or
empty `rate` becomes 5000 and anything else clamps to 100..15000, and an absent
or empty `snaps` becomes 50ms while anything else divides 1000 by a value
limited to 1..30. The original copies the name and userinfo with `strncpy` and
therefore leaves a full-length value unterminated; fnTech3 stores the same
characters and always terminates them.

The bot scheduler (`0x4177e0`) runs inside the simulation step, before the
game's `RunFrame`, with the step just added to the server time (`0x41ac3d`).
fnTech3 dispatches one `GAME_RUN_FRAME` per step, so the adapter derives that
step from the distance to the previous dispatch and uses the spawn time for the
first one. The scheduler visits every slot that holds an entity association
whose entity carries the native bot flag `0x8`. Each visit clears the outgoing
message and refreshes the last-packet time; a client that is not active yet
becomes active and receives `BotBegin`, and every later visit receives
`BotThink` with that step. A missing `BotBegin`, `BotThink` or
`ClientUserinfoChanged` export fails explicitly instead of dispatching a null
call. Retail botlib remains disabled for this profile, so `SV_BotFrame` never
reaches the native game.

`SnapshotEntityCount` (`0x41b370`) reads the frame that the outgoing sequence's
low five bits select, and `SnapshotEntityNumber` (`0x41b3b0`) adds the index to
that frame's first entity, takes the x86 signed remainder by the shared ring
capacity and returns the stored entity number as a signed short. The original
derives the client slot from the entity's own number word and prints a warning
when that word disagrees with the entity's position; fnTech3 validates the
pointer against the bounded entity array first and rejects a module whose
published number contradicts it. An index outside the frame's count, and a
remainder that would address below the ring, are refused rather than read.

The builder that fills that history is described below. Connected native
clients, `ClientBegin`, `ClientThink`, the handshake and bidirectional sessions
remain open.

`tests/q3_ihv_reference_bot_client_probe.cpp` runs the hash-pinned original
imports, scheduler, userinfo update and snapshot getters beside fnTech3's
records on the same inputs. Eleven allocations across one-, two- and eight-slot
hosts, thirty-three scheduler steps with their callback order and arguments,
twelve userinfo cases including oversized values and unparsable numbers, and
6,528 snapshot results match, with FNV-1a64 `35b3b922bb1be73c`. It maps only
the observed code pages plus the original's read-only and initialized data, and
starts no original entry point, filesystem, display, input, audio or network
service.

## Native snapshot building

`SV_BuildClientSnapshot` (`0x41b210`) runs once per server frame for every
active client whose interval has elapsed, from the original's client message
pass (`0x41b6a0`, `0x41ac87`). It takes the client's own view point, selects
what that point can see, sorts those entity numbers and copies each 164-byte
native state into the shared ring, leaving the count and ring position in the
client's frame. `SV_SendClientSnapshot` then skips transmission for a bot
(`0x41b544`), so the original builds bot snapshots and only withholds the
packet. That is why the two getters matter for bot awareness, and why fnTech3
builds these snapshots while its protocol-33 writer is still gated.

The player state is the first 276 bytes of the game's own client record, copied
whole; no retail player projection is involved. The view point is that record's
origin plus its integer view height (`0x41b290..0x41b2a9`). The frame also keeps
the viewer's 32 area bits and the byte count the writer would send, so the
message writer has the record it needs.

IHV's visibility record is Quake II's, not Quake III's. `SV_LinkEntity` stores
a deduplicated cluster list, and once an entity's box covers 128 leafs or more
than 16 distinct clusters it stores `-1` and the first node the box straddled
(`0x41bce6..0x41bd72`). The walk then tests that node with `CM_HeadnodeVisible`
rather than scanning a cluster range. fnTech3 records this only for the
`q3-ihv` profile: the other seven keep Quake III's list and its overflow
cluster, `CM_BoxLeafnumsTopnode` supplies the straddled node, and
`CM_HeadnodeVisible` refuses out-of-range nodes, leafs and clusters instead of
reading outside the model.

The walk itself (`0x41b040`) visits entities 1 upward, never the world. It skips
an entity that is not in use, repairs and reports a published number that
disagrees with the entity's own slot, then skips one that is not linked or
carries `0x1`. An entity already added through another view is skipped, `0x20`
is added regardless of visibility, and otherwise the entity must share an area
with the viewer through either of its two area numbers and then be visible
through its clusters or its stored node. An added entity carrying `0x40`
expands the view again from its second origin (entity state offset 100), which
terminates because the add mark precedes the recursion. Selection stops at
1,024 entities.

fnTech3 keeps that walk in `q3_ihv_snapshot_select.hpp` with its collision and
link queries supplied by the host, so the dedicated server and the
original-function probe run the same code.
`tests/q3_ihv_reference_snapshot_probe.cpp` drives the hash-pinned original
builder and fnTech3's selection over the same synthetic collision replies:
2,048 generated scenarios agree on every selected entity number, the frame's
area bytes and bits and the repaired-number reports, covering 5,850 selections,
1,928 portal expansions and 412 repairs, with FNV-1a64 `c91eb87c13abd929`. The
probe also confirms that the frame's player state is the client record copied
whole. Synthetic collision replies establish the selection rules, not original
BSP visibility; matching the original's own leaf, cluster and area answers
remains part of the open BSP parity work.

The writer that turns these frames into protocol-33 messages is described
below.

## Protocol-33 snapshot messages

`SV_WriteSnapshotToClient` (`0x41add0`) emits one `svc_snapshot`: the server
time, the message number, the delta base, the configstring and command
sequences, the snapshot flags, the area bits, the player state delta and the
entity deltas. The message codec in `q3_ihv_message.hpp` already encoded that
layout; this follow-up adds the server side that feeds it.

A client names the message it wants the new one delta'd against. The original
refuses a request that is not positive, one more than 28 messages behind the
message being written (`0x41addd`), and one whose entities the shared ring has
already overwritten (`0x41ae40`), printing `Delta request from out of date
packet` or `... entities` and sending a full snapshot instead. That last rule
is why the frame keeps the running entity counter unreduced, and why it is
conservative at the boundary: a frame starting exactly one ring capacity behind
the counter is already refused. fnTech3 counts in 64 bits, so it cannot reach
the wrap the original's `int` would.

IHV's first configstring is the serverinfo, and a protocol-33 reader refuses a
gamestate or snapshot whose serverinfo does not name protocol 33. fnTech3's
dedicated server now publishes that key for the `q3-ihv` profile alone; the
other seven keep their own serverinfo, which never carried it. The second
configstring stays the native `serverid`/`mapChecksum` pair.

A new entity is sent from its baseline. `SV_CreateBaseline` (`0x4196a0`) runs
after the three settling frames and takes Quake II's rule: an entity gets a
baseline only while it is in use and carries a model index, a looping sound or
effect flags. A slot without one keeps its zero record, so its entity is sent
delta'd against zero. fnTech3 stores these as native 164-byte states for the
`q3-ihv` profile; the other seven keep Quake III's `entityState_t` baselines.

Every configstring change bumps one server-wide sequence that each header
carries (`0x41964a`), so a client can tell which updates it has already seen;
fnTech3 counts them where all seven profiles set a configstring, not only where
the module does. The developer packet padding (`0x41ae62`) prefixes the
message with that many nops, using the same cvar the other profiles use.

`SV_EmitPacketEntities` (`0x41ace1`) stops adding entities once the message has
passed 1,350 bytes and writes the shorter count back into the frame, so a later
delta against that frame matches what the client actually received. The shared
codec takes that limit as a parameter and reports how many entities it wrote.

`tests/q3_ihv_reference_snapshot_message_probe.cpp` gives both the original
writer and fnTech3's the same client frames, shared ring and baselines and
compares the messages byte for byte. 1,536 generated scenarios agree on every
byte and on every delta rejection: 980 delta messages, 366 refused requests
across both rules, 415 truncated entity lists and 750,368 message bytes, with
FNV-1a64 `3e67d3f6510e5e16`. The probe replaces only the diagnostics; the
original's own `MSG_` writers produce the reference bytes.

The envelope that would carry these messages is described below.

## Protocol-33 transport

IHV's netchan is neither Quake III's nor Quake II's alone. Every packet opens
with Quake II's two sequence words, each carrying a reliable bit
(`0x41285d..0x41286e`): the first is the outgoing sequence with a "this packet
repeats the reliable block" flag, the second is the last sequence received with
the receiver's reliable bit. A client then writes its qport and a server reads
one (`0x41288d`, `0x412b82`). Reliable delivery is Quake II's single slot, not
Quake III's command ring: one block is retransmitted verbatim until the peer's
reliable bit matches, and only then does a waiting message take its place.
`Netchan_NeedReliable` (`0x412730`) returns at `0x41274f` as soon as it decides
to retransmit, so unlike Quake II's version it never promotes a pending message
in the same call.

The reliable block is always framed as `{short start, short length, bytes}`.
When it reaches 1300 bytes the channel fragments it Quake III's way
(`0x41283d..0x41297b`), sending successive slices and, when the last one is
exactly 1300 bytes, one more empty fragment. A fragmented transmission carries
no unreliable payload; an unfragmented one appends the caller's data after the
reliable block, or prints `Netchan_Transmit: dumped unreliable` and drops it
when it no longer fits the 1400-byte packet.

Everything after the first ten bytes is scrambled (`0x412690`). The first
sequence word seeds a key stream advanced by `x = 69069x + 1`, one word per
four payload bytes, low byte first. The same pass encodes and decodes, so
`Netchan_Process` (`0x412b30`) runs it before reading anything. A receiver
rejects a sequence at or behind the last one accepted, counts the gap as
dropped packets, clears its reliable slot when the peer's reliable bit matches,
and reassembles fragments by their start offsets.

fnTech3 keeps all of this in `q3_ihv_netchan.hpp`, registered as its own wire
profile so protocol 33 can never fall back to a Quake III contract: the
netchan feature table gives it the sequence pair, the reliable slot and the
payload key stream, and none of the reliable XOR, sequence checksum or command
ring the other seven profiles use.

`tests/q3_ihv_reference_netchan_probe.cpp` drives the hash-pinned original and
fnTech3's channel with the same sequences, reliable blocks and payloads. 512
scrambler trials, 5,202 transmitted packets over 1,024 transmit scenarios and
512 received messages agree byte for byte, covering 598 fragmented
transmissions and 3,467,972 packet bytes, with FNV-1a64 `c25b7812f8c406f6`.
Only the diagnostics, the address formatter and the sending routine are
replaced, so the original's own writers and readers produce the reference
bytes and no socket is opened.

**fnTech3 boundary:** the original drops an out-of-order fragment while keeping
the offset it expected (`0x412d15`), so one lost fragment stops every later
reassembly on that channel. fnTech3 forgets the partial message instead, which
lets the peer's retransmission start over.

Client sessions now carry these packets in both directions; see the
[live client record](2026-10-08-q3-ihv-live-client.md) and the
[server sessions record](2026-10-08-q3-ihv-server-sessions.md).

## Native server game-data adapter

`ServerGameDataView` now binds IHV's 268-byte entity prefix using its published
stride, count and checked allocation capacity. Entity numbers are signed
16-bit words, so setting one cannot overwrite the adjacent entity type. Owner
pointers resolve only to exact records in the bound arena; malformed pointers
fail without dereferencing them. Client slot `n` belongs to entity `n + 1`.
The trace follow-up assigns native null owners the internal value `-1`, keeping
IHV's valid entity 1023 distinct from null. No external record layout changes.
The IHV native binding is explicitly restricted to 32-bit hosts, matching the
supplied module. Portable state/ownership tests also run in a diagnostic x64
build; that build rejects native binding.

IHV's client pointer is initially null and becomes available after a native
callback publishes it. The host must supply an allocation validator, called
on every nonnull client access. Revocation, freeing, interior/wrong-slot
pointers and failed rebinding are tested. Ping is the final word of the
276-byte player prefix: original server instructions at `0x41a9a5..0x41a9ab`
write `entity->client + 0x110`. The adapter modifies only those four bytes.

Entity transfers keep all 164 native bytes, including padding and float
payloads, across a shared-engine round trip; narrowing failures preserve the
previous native state. Native snapshot overloads return exact entity/player
records for the protocol-33 codec. The generic `playerState_t` overload still
rejects IHV; its remaining shared projections must be pinned before server
snapshot assembly uses them. No retail wire layout is substituted.

The optional `tests/q3_ihv_reference_adapter_probe.cpp` loads the hash-pinned
supplied DLL and uses the real engine adapter through Init, password rejection,
accepted ClientConnect callbacks for slots zero/seven, raw state transfers,
ping writes and Shutdown. Every allocation is released. It starts no level,
renderer or network session. The actual `SV_LinkEntity` path separately passes
all 2,740 original geometry vectors through synthetic native records; complete
arena comparisons protect world records, padding, private tails and guards.

The [native game-data evidence](2026-09-21-q3-ihv-game-data.json) records the
commands, inputs, limitations and regression results. Build the optional host
with `tests/q3_ihv_reference_adapter_probe.cpp`,
`code/server/server_game_data.cpp` and `code/compat/game_abi_adapter.c`, using the
project's x86 environment, C++17 and `advapi32.lib`; retain an `asInvoker`
manifest. Pass the supplied `qagamex86.dll` and `baseq3` paths as its two arguments.
The original lifecycle probe remains separately runnable.
This adapter checkpoint passes 127 Meson targets with two skips; its Python
target completes 1,838 cases with 72 skips in 783 seconds.

## Server sweeps and point contents

The IHV branch of the actual `SV_Trace` now returns entity zero for both a world
hit and a clear world trace. It replaces the entire current result when an
entity reply is `allsolid`, `startsolid`, or has a smaller fraction. A previous
`startsolid` flag is therefore not carried into a later replacement. The
original instructions at `0x41c0aa..0x41c0dc` establish this behavior.

Positive pass-entity values exclude that entity, its owner, and entities owned
by it. Other projectiles sharing its owner remain candidates. Zero and negative
pass values do not dereference an owner. The native state's signed entity
number and an owner's pointer/index have distinct roles in these checks.
Null owners project to `-1`; entity 1023 can be assigned as an owner and cleared
without colliding with the retail sentinel. The native pointer itself remains
unchanged by a read/write round trip.

Sweeps select state origin/angles unless native flag `0x80` requests the current
transform; boxes use zero angles. Original point-content queries always pass
state origin/angles, including for boxes and entities with that flag. Sweep
query bounds preserve the original x87 precision-53 sum and one-unit padding
before the float store. Invalid/null/nonfinite input, inverted or overflowing
bounds, invalid owner pointers and unavailable records fail before publishing
a trace result. Existing behavior remains covered for all seven later profiles,
including Enemy Territory's `-2` world-only trace.

The [server trace evidence](2026-09-21-q3-ihv-server-trace.json) records 4,737
sweep and 641 point-content observations. The complete query transcript and
returned trace bytes match through both synthetic native module records and
shared engine records. Native tests also protect module padding/private tails,
neighboring records, guards and aliased start/output storage. These are server
selection/aggregation fixtures: the contact list, model lookup and CM replies
are synthetic, so they do not establish original BSP collision or gameplay.

Reproduce the reference with `tests/q3_ihv_reference_trace_probe.cpp`, compiled
for x86 at image base `0x400000`, with dynamic base disabled, the reserved `.ihv`
section executable/readable/writable, `advapi32.lib` and an `asInvoker` manifest.
Pass the hash-pinned original `quake3.exe` path. The probe calls only the selected
original numerical functions and starts no engine session. Ordinary regression
tests require no proprietary input.
The full Windows x86 regression run passes 127 Meson targets with two skips;
its Python target completes 1,840 cases with 72 skips in 693 seconds.

## Material corrections

The three renderers share the Q3-only IHV material parser. `sky` loads the six
outer images without overwriting cloud settings; `cloudparms` selects the height
and full/half coverage without overwriting the outer images. The existing
half-cloud tessellation path is now reachable for that explicit legacy command.
Later material dialects retain their previous coverage and QL's bottom-face
option. Missing arguments, nonfinite numbers, unsafe heights and overlong sky
paths fail before image lookup or cloud initialization. The bounded height range
is 0–65536; all observed supplied heights fit it. This is input validation, not
an observed original limit.

Original parser observations are at `0x43a50f` (`cloudparms`: height and required
`full`/other coverage token), `0x43a886` (`sky`), `0x43a802` (skip compiler-only
`surfacelight`), and `0x43b22c`/`0x43b374` (RGB/alpha `fromClient`, ordinal 3).
`fromClient` selects the existing entity color generators. Original `depthmask`
at `0x43b45f` sets the depth-write bit `0x100`; the Q3-only alias now selects the
existing explicit depth-write path. The original cloud geometry at `0x4443af`
uses a lower side subdivision of -4 for full coverage and -1 for half coverage,
and omits the bottom in both cases, matching the new path with Q3's default
bottom-face setting. Implementation is
independent of external source text. These material commands do not select IHV
native modules, filesystem rules or protocol 33.

The supplied 8672-byte `common.shader` lacks the outer closing brace after
`textures/common/light5`, before `light6`; it is not merely an unfinished final
definition. Previously that discarded every definition in the file. The repair
matches the filename, length, whole-file FNV-1a fingerprint `87e8f157` and local
byte guard, then rewrites five bytes of whitespace/braces in the read buffer.
It does not grow the allocation, write the installed file, or relax validation
of arbitrary malformed files. Tests parse all resulting definitions, retain
both light5/light6, and reject every single-byte mutation of the supplied file
as a repair candidate. Other profiles receive unchanged bytes.

[Material evidence](2026-09-20-q3-ihv-materials.json) records the new module hashes,
commands, warnings and captures. All 12 map/renderer probes pass load, capture
and shutdown checks with retail Q3 QVMs. Per-map warnings fell from 9/13/10/10
to 2/6/4/5 in GLx, with the same new counts in Vulkan and RTX. The remaining
warnings included `fogonly`, `lightning`, portal `depthMask`, and a missing retail
light grid. BSP43 has no retail light-grid lump; synthesizing one is not evidence
for the original entity-lighting behavior. Inspected captures still show
checkerboard skies and missing fog, so visual parity remains **OPEN**. A final
three-renderer `km_portal` follow-up after adding `depthMask` passes again and
reduces that map's warnings from six to four. The portal stage no longer falls
back because of that keyword; correct portal scenes still need the IHV cgame.

The new tests compile the shared parser and each renderer's actual color
selection and cloud tessellation with the engine tokenizer, including under
`-ffast-math`. To additionally exercise the supplied file without committing it:

```powershell
$env:FNTECH3_IHV_SHADER_DIRECTORY = 'E:/Repositories/Quake3-IHV/ref/install/baseq3/scripts'
python -m unittest discover -s tests -p q3_ihv_shader_tests.py -v
```

The later [sky correction](#sky-colors-and-lightning) handles `lightning`; the
earlier material captures above retain their original evidence scope.

Original-engine `screenshot` probes also succeeded on the unswitched private
desktop. Their early `sli` frames showed a scoreboard/black frame, and an early
local-map capture showed connection progress. Those captures establish the
reference's screenshot path, **not** a usable visual comparison with fnTech3.
A subsequent `tim_dm1` probe waited for the original native client/server to
connect, then invoked `screenshot` and `quit` through authenticated localhost
rcon. It captured the world with dense green fog and exited normally. The
engine-written TGA and a PNG preview are retained with the material evidence;
the camera and modules differ from the fnTech3 asset probes, so this is not a
matched visual-parity pass. Reference startup also reports absent xian player
parts and an unnamed missing model/image. Those input/runtime limitations must
remain distinct from fnTech3 defects.

The material full-suite run completed 123 Meson targets: 120 passed, two skipped
and the Python target had one failure among 1829 cases (72 skipped). The failure
was a transient docs snapshot: the new ledger link was written during the run
before its JSON target existed. Existing package rules include JSON evidence.
After the document set was complete, all 49 release-packaging checks passed
with one platform skip; the final IHV parser/fixture tests passed as well. No
packaging implementation change was needed. Full-run and rerun logs are retained
under `.tmp/q3-ihv`. The later fog run below provides a fresh passing full
suite; neither run completes the remaining IHV runtime and interoperability gates.

## Legacy fog

The [fog evidence record](2026-09-20-q3-ihv-fog.json) adds an independent
implementation of the original density calculation. Flat `fogparms` stores
RGB, depth and signed integer gradient; `foggen` accepts the observed sine,
square and triangle depth waves; `fogonly` permits the untextured fog material.
These commands are Q3-only. Missing, malformed or nonfinite arguments fail
without partially publishing parameters. The later parenthesized `fogParms`
syntax retains its existing behavior and clears legacy parameters when it
replaces them.

Original function `0x4448f0` clips the ray from the local eye to each vertex
against a Z-density plane and integrates density using Euclidean distance.
Positive gradients use the brush's maximum Z; negative gradients use minimum
Z. Animated depth uses integer refdef time, the observed 1024-entry wave tables,
and a minimum depth of one. The IHV sine table uses 1023 in its angular step.
The original zero-gradient plane is zero and produces no density; an earlier
inference that it meant retail uniform fog was incorrect. Finite extreme
inputs use wider intermediates and saturate animated depth at `FLT_MAX` to
avoid overflow. Those safety bounds are implementation policy.

The optional Windows x86
[reference probe](../../tests/q3_ihv_reference_fog_probe.cpp) verifies the
supplied executable's full SHA-256 before loading the observed function and
constants into its own reserved address space. It does not execute the engine
entry point, initialize graphics, load game modules or touch input. The probe
supplies wave tables from the separately observed `R_Init` formulas. All 306
outputs across 51 cases match fnTech3 within absolute error `0.00001`, including
outside/inside clipping, both directions, zero gradient, time wrapping and
rotated entities. The committed
[numeric vectors](../../tests/fixtures/q3_ihv_fog_vectors.json) contain observed
facts; no original implementation or proprietary inputs are embedded.

All three renderers blend authored fog RGB with the calculated vertex alpha
and a white texture. Transparent stages use the same density factors for
RGB/alpha attenuation. The loader preserves the original unscaled fog color.
Static VBO fog and Vulkan's collapsed/fog-only shaders are bypassed for these
volumes. Maps containing IHV fog use the existing classic dynamic-light pass
before the fog overlay because the programmable light shaders implement retail
fog. The fallback is scoped to these Q3 maps and announced in the console;
other profiles and maps retain their selected light mode. This preserves
compositing order but does not establish original IHV lighting parity.

Four automated controllers compile each renderer's parser, loader, draw and
light-selection code, including under `-ffast-math`, and check the numeric
vectors, malformed input, finite extremes, transparent colors, pipeline/image
binding, world unload and all seven profile boundaries. The normal Windows x86
renderer builds pass. All 12 final map probes load, capture and exit normally.
Inspected `tim_dm1` captures show green fog and `test_fog2` shows colored fog;
remaining per-map warning counts are 2/3/2/3 in every renderer. `ihv_test1` does
not select the fog/light fallback; the other three maps do. These observations
still use retail QVMs, different cameras and fallback skies, so they do not
constitute matched original rendering or full IHV gameplay evidence.

The fresh complete Meson suite passes 121 targets with two skips and no
failures. Its Python target passes 1,833 cases with 72 skips in 626 seconds.
Logs are retained as `.tmp/q3-ihv/fog-suite-testlog.txt` and
`.tmp/q3-ihv/fog-suite-testlog.json`. The reference probe also rejects a file
with an unexpected hash before calling any original function.

Run the portable tests with:

```powershell
python -m unittest discover -s tests -p q3_ihv_fog_tests.py -v
```

To reproduce the numeric reference comparison from a Windows x86 compiler
prompt, build `tests/q3_ihv_reference_fog_probe.cpp` with `/MD`,
`/BASE:0x400000`, `/DYNAMICBASE:NO` and `advapi32.lib`, putting the executable and
object under `.tmp/q3-ihv`. Pass the supplied original `quake3.exe` as its sole
argument. The exact build/run commands and executable identity are recorded in
the fog evidence JSON.

## Sky colors and lightning

The [sky evidence record](2026-09-20-q3-ihv-sky.json) adds the original cloud
iterator's color behavior. Stage zero is white. Later stages can switch between
black and white during lightning, using one renderer-wide random stream and a
last-flash timestamp belonging to each shader. The original updates this state
before testing a stage's active flag, including the inactive terminator. Generic
RGB/alpha generators do not replace the cloud iterator's color; alpha stays
opaque. `lightning` sets shader offset `0x5c`; `polygonOffset` uses `0x28a0`.

The optional hash-pinned Windows x86 probe calls original iterator `0x443510`
with drawing stubbed. Ten combinations of zero, one, two, three and eight active
stages, with lightning off/on, produce 730 frames and 2,044 stage RGB results.
The independently authored implementation matches every color, random seed and
flash timestamp. Only numeric facts enter the fixture; no reference code or
assets enter fnTech3. Build/run commands are recorded in the evidence JSON.

All three renderers preserve IHV cloud stages when vertex lighting, white-stage
simplification or multitexturing would otherwise merge them. Vulkan/RTX retain
a vertex-color pipeline and cannot reuse a previous stage's color calculation.
The override is limited to the Q3 profile's IHV sky syntax and clears after the
sky draw, including empty geometry. Tests compile the actual sky iterator,
cloud builder, color dispatch and pipeline/reuse branches, cover all seven
profiles, and pass with `-ffast-math` in Windows x86 and diagnostic x64 builds:

```powershell
python -m unittest discover -s tests -p q3_ihv_sky_tests.py -v
```

Twelve more map/renderer probes pass, plus three `tim_dm1` sequences containing
14 engine screenshots each. All four maps now report only the host IPv6 bind
warning and the absent retail light grid; no unknown IHV keyword warnings remain
in these runs. The `tim_dm1` checkerboard sky is replaced with clouds. GLx
sequence frames 340 and 360 show a lightning change. Other sequence cameras
show little sky or dense fog, so those captures do not independently establish
lightning parity. Native portals, collision, model/entity lighting and complete
original rendering parity remain open.

The complete Windows x86 regression run passes 121 Meson targets, with two
skips and no failures. Its Python discovery suite passes 1,835 cases with
72 skips. Logs are `.tmp/q3-ihv/sky-suite-testlog.txt` and
`.tmp/q3-ihv/sky-suite-testlog.json`.

The runtime probe accepts repeated `--capture-frame N` arguments for animation
sequences. It continues to use an isolated writable home, disabled input and
sound, windowed rendering on the private desktop, and the engine screenshot
command. Reports and 54 captures are under `.tmp/q3-ihv/sky-final-*` and
`.tmp/q3-ihv/sky-sequence-*`.

## Further native and sampling observations

A guarded x86 getter probe confirms the supplied game DLL's 35-word import
copy, API 4 and 556-byte entity stride. It maps both native DLLs without running
their entry points and calls only the observed export getters. The subsequent
native game boundary probe below runs the game DLL's lifecycle separately.
The original engine fills all 35 game imports in
`0x419430`. Client initialization `0x405e30` builds 62 words, copied to cgame
RVA `0xa0f80`. `CL_GetParseEntityState` at `0x405dd0` copies 164 bytes. These
observed sizes do not establish the full field layouts or callback signatures.

The [cgame copy follow-up](2026-09-21-q3-ihv-cgame-copy.json) records all eight
export addresses and their engine call sites. Slot one receives a snapshot,
copying 348 bytes into a 32-entry ring. A guarded original-DLL probe passes
36 cases covering ring rollover, counter wrap and neighboring-record
preservation. It replaces the post-copy packet-graph helper with a return and
does not initialize cgame, rendering or gameplay. The optional reusable host is
`tests/q3_ihv_reference_cgame_snapshot_probe.cpp`. Complete native client
integration remains open.

The [native configstring record](2026-09-21-q3-ihv-configstrings.json) closes a
missing message case: opcode three updates a configstring after the initial
gamestate. It carries a short index and string. The decoder now accepts these
updates and retains event order across config changes, commands, nops and
snapshots; serverinfo cannot switch the selected protocol. Parsing, table
overflow or a malformed later opcode leaves previous state and output intact.

The native string record contains 1,024 little-endian offsets, 16,000 string
bytes and a trailing data count, totaling 20,100 bytes. Initial gamestate
strings append in wire order, including duplicate indices and empty strings.
A changed update rebuilds nonempty strings in index order; an unchanged update
preserves every byte and emits no change notification. The new storage follows
these rules without aliasing retail `gameState_t`.

The optional original parser host passes 512 complete-record comparisons and
384 change notifications (FNV-1a `06594df1f7ec01c4`). It calls `0x406df0` with
synthetic message reads and a notification callback. No engine entry point,
cgame initialization, display, input, filesystem or network session runs.
Native records remain separate from engine startup. The exact byte sizes for
gamestate and snapshot transfers are declared in `q3_ihv_client_abi.h`;
the subsequent client data follow-up below supplies complete snapshot
projection. Native engine cgame dispatch remains open.

A second optional host passes 36 calls to the original cgame's configstring
callback using these records. It verifies the native gamestate copy, model
and sound registration paths, custom-sound exclusion and music notification
with test callbacks; no actual assets or audio are registered. A separate
original getter probe covers 33,152 command/entity history cases and their
signed 32-bit wrap behavior. These observations are prerequisites for the
native client adapter, whose integration remains incomplete.

The Windows x86 native-boundary, wire, message and configstring targets pass.
Clang x64 AddressSanitizer checks pass with `-O2 -ffast-math`, including aliased
string views, maximum capacity, malformed updates, recovery and both supplied
IHV demo inputs. `sli.dm3` retains its previous 57 messages, 56 snapshots and
680 states; the incomplete `berj.dm3` now reports the precise missing-gamestate
reason. Full-suite results and artifact identities are in the record above.
The complete follow-up run passes 128 Meson targets with two skips; the Python
target completes 1,840 cases with 72 skips. The two optional native hosts were
then rebuilt and checked from their retained test sources; production code
was unchanged after that suite.

## Native client data and ordered packet delivery

The [client data record](2026-09-21-q3-ihv-client-data.json) adds the engine side
of native data imports in `code/compat/q3_ihv_client_data.hpp` and a packet
session in `code/client/client_ihv_session.hpp`. They preserve IHV records
separately from retail client state. This component does not start the engine,
initialize cgame, predict movement, render frames or execute server commands.
Connecting the remaining module and transport contracts is still required.

The original command getter keeps 64 records of 14 bytes, with a counter naming
the latest command. Entity history keeps 1,024 records of 164 bytes and its
counter names the next slot. Both use signed dword comparisons even at wrap;
the independent adapter performs unsigned arithmetic and explicit signed
interpretation. A future entity request is an error, while unavailable history
leaves output untouched. Original getters and the actual adapter match on
33,152 boundary/wrap queries, including output guards (FNV `b009f6ef775c6672`).
Gamestate replacement clears both histories, as original `0x404ac0` does.

The native snapshot is exactly 348 bytes, with little-endian dwords and these
offsets, established by original parser `0x406a30` and the cgame copy callback:

| Byte offset | Native field |
| --- | --- |
| 0, 4, 8, 12 | Valid flag, snapshot flags, server time, command sequence |
| 16, 20, 24 | Message number, delta number, ping |
| 28 | 32 area-mask bytes, unused bytes zeroed |
| 60 | 276-byte player record |
| 336, 340, 344 | Config sequence, entity count, first entity sequence |

Original parsing and entity getters match complete adapter records for 40
synthetic frames and 4,583 entity reads (FNV `5179d02fca70b215`). Cases include
empty and 1,023-entity snapshots, ring rollover, signed message-number wrap,
delta frames, variable area bytes and ping. The optional host uses original
delta routines with synthetic baselines/timing and a capture callback; it never
runs the executable entry point. It also reads hash-pinned `sli.dm3` unchanged:
all 56 snapshots and 680 entity states match (FNV `4582fb032c471ba6`). Initial
baselines are supplied by the decoder and ping is fixed at zero; no original
gamestate initialization or map loading occurs in this comparison.

`ClientSession` decodes and checks every publication on private state before
delivering callbacks. Delivery then follows wire order, so a snapshot sees
earlier configstring updates and cannot see later ones. Import callbacks use
one stable client-data address. Malformed input publishes nothing; unchanged
configstrings do not notify. Recursive packet delivery is refused, a callback
can clear the session to stop remaining delivery, and exception unwinding
clears all session state. The future engine error adapter must unwind this C++
call before invoking `Com_Error`/`longjmp`; clearing records alone cannot make
a jump across C++ object lifetimes safe. That integration remains open.

A second optional host feeds these packets through the real session component
to the original cgame's configstring and snapshot callbacks. It verifies the
entire 32-entry snapshot ring, counters, first-snapshot flag, native string
copy, 4,583 entity imports and command history. The synthetic run records 40
server commands and uses model/sound/music registration stubs. A late malformed
opcode leaves cgame untouched; an injected native registration failure clears
the session. `sli.dm3` then delivers 57 messages, 56 snapshots and 680 entity
imports through the same route, while `berj.dm3` causes no callback. Cgame Init
is bypassed, the packet-graph helper is stubbed, and commands are only recorded. This is
data-delivery evidence, not rendered playback or a native game session.

The new `fntech3_q3_ihv_client_data` target checks publication bounds, aliasing,
packet rollback, callback ordering, reentrancy, failure cleanup and recovery.
It also passes Clang x64 AddressSanitizer with fast math and both supplied demo
inputs. An initial catch/rethrow implementation exposed a Windows ASAN
exception-unwinding failure also reproduced in an independent tiny program,
consistent with [LLVM issue 215376](https://github.com/llvm/llvm-project/issues/215376).
Cleanup now uses a scope guard and the sanitizer run passes without disabling
sanitizer checks. The evidence record carries the full-suite result and hashes.

Reproduction uses the Windows x86 toolchain and the optional hosts
`tests/q3_ihv_reference_client_history_probe.cpp`,
`tests/q3_ihv_reference_client_snapshot_probe.cpp` and
`tests/q3_ihv_reference_client_session_probe.cpp`. The first two reserve original
code addresses at host bases `0x3f0000` and `0x300000`, respectively; use their
recorded compiler commands. Supply the original executable to the first two,
the cgame DLL to the third, then optional `sli.dm3` (and `berj.dm3` for the third).
Sources, fixtures and evidence contain no original implementation or assets.
Logs remain in `.tmp/q3-ihv/client-data-*`, `adapter-history.*` and
`reference-client-{snapshot,session}.*`.

The separately retained `tests/q3_ihv_reference_cgame_init_probe.cpp` exercises
the original Init prefix through `ClientSession` and `CvarMirror`. It observes
59 cvar name/default/flag requests (FNV `4995ca0b04c29cc7`), copies the native
gamestate and checks serverinfo fields, the map path and the mutable client
state word. One case stops at the collision-map import; another supplies a
synthetic collision reply, checks sound-registration order and stops at the
renderer map import. Each of the 59 cvar requests also has an injected failure
case, verifying session cleanup. These 61 runs do not complete Init or load
collision, renderer or sound assets. Logs use `reference-cgame-init-*`.
Read-only observations also establish scalar viewport width/height at import
words 37/38 and a 152-byte scene copy; complete scene/entity layouts and drawing
callbacks still require their own adapter and comparison evidence.

## Native model metadata and cgame frame callbacks

The [model and cgame follow-up](2026-09-21-q3-ihv-cgame-runtime.json) adds
`ModelTags` and `NativeModelMetadata`. They independently parse little-endian
MD3 frame/tag tables, validate ranges, multiplication, overlap, names and finite
values, and clear failed registrations. The native descriptor is 112 bytes:
name at 28, MD3 flag at 92, frame/tag counts at 96/100 and their 32-bit array
pointers at 104/108. Frames are 56 bytes; tags are 112 bytes with origin at 64
and axes at 76. The native mirror is available only on x86; portable metadata
parsing and interpolation also run in the x64 diagnostic.

The descriptor cannot be replaced by an opaque C++ object. Original cgame
RVA `0x42cb..0x42f5` follows its frame pointer to position HUD models. The new
mirror owns stable frame/tag storage for that registration. This corrected
heap-address-derived HUD positions in the exploratory host; repeat runs now
produce identical captured scene data.

Original `CM_GetTag` at `0x40a4c0` clamps high frame indices and performs exact,
case-sensitive name lookup. `CM_LerpTag` at `0x40a570` blends the two poses,
normalizes each axis independently and leaves the result's 64 name bytes
untouched. The adapter preserves those rules, while rejecting negative frames,
nonfinite input and overflowing extrapolation without changing the output.
It checks fraction storage before arithmetic so the validation survives
finite-math optimization. Numeric probes cover 79,625 queries over generated
fixtures and all 111 supplied MD3 files (54 with tags). All 243,600 returned pose
components match the original bit for bit with x87 precision 53; the original
result digest is `c8cd2bc9eb0e1ad7`. This result does not establish arbitrary
compiler/float-mode equivalence.

`tests/q3_ihv_reference_cgame_runtime_probe.cpp` loads the hash-pinned cgame DLL
normally, initializing its CRT without starting an engine or display session.
Using `DONT_RESOLVE_DLL_REFERENCES` is insufficient for these callbacks because
frame processing reaches the DLL's imported CRT support. The host runs original
Init and unmodified snapshot, update-frame and draw-view exports through
`ClientSession`. All 56 demo frame updates return ready; cgame makes 472 tag
queries using 11 owned MD3 registrations and submits 124 scenes containing
818 entities, 42,696 particles and one polygon. Both fresh-process runs produce
scene digest `a1f3252c1dfd6fe0` after excluding native array addresses.

Those are captured submissions, **not rendered fnTech3 playback**. Collision
queries report synthetic empty geometry; model/skin/shader/sound registration
uses synthetic tokens; audio and drawing calls are recorded without execution.
No commands, input or network traffic are generated. The host verifies scene
array extents and registration ownership, releases file buffers, and checks
session invalidation when tag or scene imports throw. Exact scene/entity
translation, actual BSP collision/PVS, sound delivery, native lifecycle and
renderer integration remain open.

This also corrects an earlier observation: helper RVA `0x4b90`, stubbed in the
snapshot-copy and client-data probes, maintains packet-loss/ping graph history;
it is not a prediction helper. The older JSON records retain their original
wording and hashes; this record supersedes that label. The new host does not
patch the helper. Its graph counter reaches 154 for demo message numbers
99 through 154.

The `fntech3_q3_ihv_model_tags` target covers malformed/truncated metadata,
unaligned input, output preservation, native pointer ownership, failed reloads,
frame clamping and interpolation. The standalone tests pass MSVC x86 and Clang
x64 AddressSanitizer with `-O2 -ffast-math`. Reproduction commands and the full
regression result are in the linked evidence record. Optional hosts are
`tests/q3_ihv_reference_model_tags_probe.cpp` (base `0x3f0000`, supplied executable
and optional asset root) and `tests/q3_ihv_reference_cgame_runtime_probe.cpp`
(supplied DLL, asset root, optional `sli.dm3` and mode `frames`, `fail-tag` or
`fail-scene`). Logs remain under `.tmp/q3-ihv/model-tags-*` and
`.tmp/q3-ihv/cgame-runtime-*`; no original asset or implementation is incorporated.

## Owned native scene boundary

The [scene-boundary follow-up](2026-09-21-q3-ihv-scenes.json) freezes the native
152-byte scene, 100-byte entity, 28-byte light, 16-byte particle, 16-byte polygon
and 24-byte polygon-vertex records. `q3_ihv_scene.hpp` reads little-endian fields
through a checked memory service and publishes a complete owned submission only
after every array and resource reference validates. Its output contains no native
addresses. Resource resolution checks model, skin and shader kinds against the
current registration lifetime. Polygon grouping links are scratch written by the
original renderer (`0x42d1e6`/`0x42d208`); the reader never follows them.

The boundary checks address arithmetic, signed counts, per-array and aggregate
vertex budgets, finite coordinates, viewport/FOV bounds and required resources.
The default submission budgets are 2,048 entities, 32 lights, 4,096 particles,
256 polygons, 1,024 vertices per polygon and 65,536 total vertices; these are
bounded host policies, not asserted original-engine maxima. Empty arrays ignore
their address words. Failed reads, resource resolution and callback exceptions
leave the previous output intact; callers must handle the failure rather than
submit stale output. Tests cover every truncated fixture record/array, each
floating field, unaligned input, address wrap, array ownership and failure at
every read and resolution position. MSVC x86 and x64 Clang AddressSanitizer with
`-O2 -ffast-math` pass.

IHV area masks have the opposite meaning to retail `refdef_t.areamask`. Original
`R_MarkLeaves` at `0x441a90` fills a missing mask with ones (`0x441ad7`) and admits
areas whose bit is set (`0x441bf3`). The boundary preserves those visible bits and
provides their complement for a renderer that expects blocked bits. A numeric
host checks 1,024 generated masks/PVS inputs against the original function over
256 synthetic leaves: all 262,144 decisions match. The point-leaf and cluster-PVS
services are replaced; this is not evidence of real BSP traversal or PVS loading.
Original color functions `0x444e40`/`0x444e70` also confirm entity RGBA at byte 96,
including all four inverted channels, in 147,456 component checks. Their combined
digest is `ac3de553280a283f`.

The original cgame host now passes every one of its 124 `sli.dm3` scene submissions
through this boundary: 818 entities, 42,696 particles and one polygon. Two fresh
runs retain the earlier raw-scene digest and produce owned-field digest
`43981fe999dd3fd8`. The supplied sequence uses entity flags `0`, `2`, `0x15` and
`0x800`, world/no-world views and no overlay. Null model pointers on its 56 portal
records are valid. The resource resolver still uses synthetic registrations, and
collision/audio/display remain outside this host.

Native effect flags, reserved scene data and overlay geometry are preserved as
IHV fields, **not mapped to retail render flags or drawn yet**. Original skin
selection uses a shader list by surface ordinal; a retail named-surface skin
handle cannot establish that behavior. Camera-facing/world-coordinate entities,
portal behavior, model lighting, particles, overlay drawing, actual resources
and all three backend submissions still need their rendering adapters and visual
comparisons. Native startup remains gated. The new source and observations
supersede the earlier scene-layout investigation; older records retain their
historical source hashes.

`fntech3_q3_ihv_scene` is the automated target. Optional reference probes are
`tests/q3_ihv_reference_scene_probe.cpp` (hash-pinned original executable, base
`0x3f0000`) and the updated cgame runtime probe. Exact commands and validation
results are in the linked record; logs are under `.tmp/q3-ihv/scene-*`.

## IHV skin registration and renderer selection

The [skin follow-up](2026-09-21-q3-ihv-skins.json) connects IHV registration and
surface selection to GLx, Vulkan and RTX, including the RTX ray-material path.
The separate profile selects these rules; retail Q3, QL, RTCW, ET and Elite
Force retain named-surface skin selection. This is renderer implementation and
CPU comparison evidence; native IHV startup and rendered cgame demos remain open.

Original `R_RegisterSkin` at `0x439930` returns a 196-byte record containing a
64-byte name, a shader count at 64 and 32 shader pointers starting at 68. Cache
names and the `.skin` extension comparison are case-sensitive. A name without
that exact extension registers one shader. Missing or empty skin files produce
one default shader and a successful cached registration; adding the file later
does not replace that cache entry. The original permits 1,024 registrations.
The new internal renderer array reserves an additional slot for the modern
engine's fallback skin while keeping all other profiles' existing capacity.

Skin text supplies shader names after commas, in their original order; surface
names do not select materials. Whitespace, quotes and comment-looking text are
literal. A `tag_` prefix at the parser's current cursor skips to the next LF;
an empty shader followed by CR/LF is skipped. A tag line after a nonempty shader
is not necessarily at that cursor, so it can register a shader if it has one.
These observed cursor rules are preserved rather than substituting the retail
`CommaParse` grammar. Embedded NUL ends the text. The bounded parser checks the
supplied byte extent, permits 32 shaders and 1,023-byte names, and refuses
overflow instead of reproducing the original unchecked writes.

The original model loop (`0x441f56..0x441f67`) selects the custom skin shader
using `surfaceOrdinal % shaderCount`; entity `skinNum` is used only when there
is no custom skin. The three raster paths and RTX's MD3 ray-material resolver
now follow that rule for IHV. An explicit custom shader still takes precedence.
Shared tests exercise ordinal wrapping, unmatched surface names, fallback,
cache identity, both capacity boundaries and malformed input; other-profile
tests retain named-surface and RTCW blink behavior.

`tests/q3_ihv_reference_skin_probe.cpp` calls the hash-pinned original registration
function with substituted file/shader/CRT services and no engine or graphics
startup. It compares 54 registrations, including all three supplied Sarge skins,
and 553 parsed shader names, then verifies the original 1,024-entry capacity and
cached fallback behavior. The digest is `aec31f68665977a9`; 52 file requests
include one missing file, and all 51 returned buffers are released. No original
skin text or implementation is incorporated.

The sanitizer run also exposed two adjacent existing issues. The shared RTCW
scale parser now validates volatile stored float bits, because Clang finite-math
optimization removed both its former `isfinite` check and an ordinary local bit
check. RTX's embedded MD3 shader selection now clamps negative skin numbers to
the first shader, matching the raster helper. The new cases cover NaN, both
infinities, conversion overflow and negative skin indices. Exact compiler,
reference and regression results are recorded in the linked evidence; logs stay
under `.tmp/q3-ihv/skin-*`.

## IHV particle submission and native asset profile

The [particle follow-up](2026-09-21-q3-ihv-particles.json) connects an owned point
submission to all three renderer backends. The appended internal renderer export
`AddIHVParticlesToScene` accepts only `q3-ihv`, copies the complete batch into the
existing frame-owned polygon storage and returns failure without advancing the
scene when input is invalid or capacity is exhausted. It stores a distinct
`SF_IHV_PARTICLES` surface; the points are never interpreted as a polygon fan.
Renderer API 27 introduced this entry point; API 28 also requires the entity
entry point described below. Older renderer DLLs are rejected.

Each backend expands the stored points using its current view, so a queued
scene, mirror or cubemap view retains its own camera-dependent geometry. Original
`0x4428a0` uses one triangle per point, with 1.5-scaled view axes, a right-axis
sign change in mirrors and depth accumulated in z, x, y order. Depth below 20
uses scale 1; otherwise the scale is `1 + depth * .004`. The discontinuity at
20 is retained. Texture coordinates are `(0,0)`, `(1.125,0)`, `(0,1.125)` and
each vertex keeps the point's RGBA bytes. Stored arithmetic intermediates preserve
the observed precision under fast-math builds. Invalid or overflowing triangles
are rejected without publishing partial vertices.

The original built-in image routine (`0x439650`) generates a 16-by-16 binary
RGBA disc centered at `(5,5)`, with squared radius 20 and no mipmaps. Shader
creation (`0x43c570`) selects exact vertex color/alpha and state `0x62`.
Calling original `GL_State` at `0x42aac0` confirms `ONE / ONE_MINUS_SRC_ALPHA`
blending and disabled depth writes. The new shader uses these observed settings.
The numeric host substitutes image, formatting, shader-finalization and GL
callbacks; it starts no graphics or game session and incorporates no original
implementation or asset bytes.

`tests/q3_ihv_reference_particles_probe.cpp` compares 144 generated geometry
cases across ordinary/mirrored views and counts around the original 341-point
batch boundary. All 1,130,688 float components match bit for bit, with digest
`c4df4de645bbaffb`; all 1,024 procedural image bytes match too. Renderer tests
exercise retained source lifetime, multiple queued scenes, frame reset, capacity,
nonfinite input and actual surface dispatch. RTX's ray path produces independent
triangles matching the raster vertices, UVs and colors. Its geometry signature
includes the particle view and mirror state, so moving the camera invalidates
the corresponding geometry. Ordinary polygons retain their existing signature.

This follow-up also fixes an integration omission: the earlier IHV shader,
fog, sky, image-text repair and classic-light fallback checks accepted only the
retail Q3 profile used by those asset probes. They now accept the separate
`q3-ihv` profile as well. Tests cover both profiles and preserve the other six
profiles' existing paths. This does not change module or network selection.

The renderer export is ready for the native scene adapter; the gated native
client does not yet call it. The evidence is **Unit/Build/Fixture**, not native
rendered demo playback or complete visual parity. Native lifecycle, other
entity effects, portals, model lighting, BSP collision/PVS, audio and live
protocol-33 sessions remain open. Commands, hashes and exact validation results
are in the linked record; logs remain under `.tmp/q3-ihv/particles-*`.

## Native entity submission and beams

The [entity follow-up](2026-09-21-q3-ihv-entities.json) adds
`AddIHVEntitiesToScene` to renderer API 28. The input contains engine-owned
integer resource handles, rather than native pointer words. Each renderer checks
the entire batch's handles, finite pose values and remaining frame capacity
before copying anything or advancing the entity cursor. Multiple queued scenes
retain independent copies until the existing frame reset. The generic entity
entry point clears the internal IHV marker, preserving the external contracts
of the other profiles. The earlier native reader now permits a null model,
matching the original missing-model path, while unknown nonzero tokens fail.

Original frontend `0x42bac0` establishes native visibility: `0x2` hides a body
in the primary view, `0x4` hides a first-person entity in portal views, and
`0x800` suppresses ordinary surface submission while retaining the portal
marker in the scene. Flag `0x400` uses the primary camera's axes even during a
secondary view. The original host exercises 122,880 cases, with 30,720 visible
and digest `6e5cc870e1d76bee`. Only the verified first/third-person and depth-hack
bits are mapped into retail `renderfx`; the complete native word is retained
separately. The original backend's `0x10` selects depth range `(0,0.3)` and
restores `(0,1)`; the renderer uses its existing weapon-depth path.

The origin interpolation helper (`0x4423a0`) runs only for native flag `0x40`.
It projects `oldorigin-origin` onto the entity axes in z,y,x order, stores
binary32, multiplies by the unclamped fraction and stores binary32 again.
229,376 comparisons match bit for bit, digest `172d67dd2e8164e5`, including
negative fractions and fractions above one. Raster MD3 and RTX ray MD3 paths
now combine that offset with native frame interpolation before the final
binary32 store, as detailed in the animation follow-up below. The earlier
entity record predates that correction.

Original model registration recognizes exactly `*beam`, before attempting an
inline-model lookup. The original beam queue uses a zero fog index. All three
renderers now register this special model and route it to an entity-transformed,
unfogged red beam with a white image, additive `ONE/ONE` blending and no depth
writes. The native beam routine (`0x442450`) emits a six-sided radius-four ring,
extrudes it by `oldorigin-origin` and repeats its first pair to close the strip.
The independent geometry matches all 166,656 captured float components from
4,096 generated cases, including 128 zero-length beams that draw nothing,
with digest `c8e8eadc64d65e92`. Twelve stored vertices and twelve triangles
preserve those positions and strip winding in the indexed renderer paths.
RTX ray geometry uses the same vertices, colors and indices; exact native
flags, poses and relevant camera axes participate in its geometry signature.

Native flag `0x80` skips both transform and lighting setup, retaining the prior
draw's state. Batches containing it fail explicitly until that sorted-draw
contract is implemented across raster and ray paths. The original lighting
probe also establishes that any nonzero native flags word adds 32 to sampled
RGB before maximum-channel normalization. The model-light follow-up below now
implements those observations, map sampling, dynamic lights and animation
controls. Sorted retention, portal semantics, clamp behavior and rendered
comparisons remain open.

`tests/q3_ihv_reference_entity_probe.cpp` runs selected, hash-pinned original
functions in an x86 CPU host. Transform, lighting, surface and GL callbacks are
recorded locally; no engine, graphics, input, audio or network session starts.
Portable tests exercise the actual registration, scene storage, frontend
selection, beam dispatcher, entity transform, MD3 offset and RTX beam functions,
including malformed/overflowing inputs, source lifetime and other profiles.
Six generated fixtures also pass AddressSanitizer with fast math, including
the MD3 ray integration and malformed-frame checks. Reproduce
the native host with the project's x86 MSVC environment, C++17, `advapi32.lib`,
`/BASE:0x3f0000 /FIXED /DYNAMICBASE:NO`, and the supplied `quake3.exe` path.
Logs and generated sanitizer sources remain under `.tmp/q3-ihv/entities-*`;
commands, hashes and complete validation results are in the linked record.

This is **Unit/Build/Fixture** evidence. Native client lifecycle and scene-adapter
dispatch, rendered demos, actual collision/PVS, audio, gameplay and bidirectional
protocol-33 sessions remain required for full IHV support.

## Native MD3 animation and culling

The [animation follow-up](2026-09-21-q3-ihv-animation.json) implements the native
MD3 evaluator in GLx, Vulkan and RTX, including ray geometry. Its private
1,024-entry sine table uses the original `/1023` sampling, with no retail phase
offset. All entries match original initialization (`0x42a685`), digest
`6685f509b8dc9008`; the retail tables remain unchanged.

Original interpolation (`0x445020`) combines weighted packed coordinates and
the origin move before storing the resulting float. Normal decoding keeps a
double X product and a binary32 Y product; interpolated normals then use the
observed double-length normalization. A zero fraction copies the current frame,
ignores the move and does not normalize. Fractions remain unclamped, and equal
frame indices select zero. The implementation matches 451,584 generated
components in 14,336 batches (`fd2298c8d50fd698`). A second read-only probe covers
all frames of all 111 supplied models: 439 surfaces, 6,804 batches and 5,408,046
components match bit for bit (`d00a4c31b3506226`).

Original culling (`0x4420b0`) first asks frame spheres when axes are normalized.
If they do not decide, it merges frame bounds and expands them by binary32
`origin-oldorigin` for flag `0x40`, without using the interpolation fraction or
axes. All three actual renderer cull functions match 165,888 generated original
cases, including 129,024 box calls (`69adefd497b482bd`). Expanded bounds also
remain available to existing light/caster consumers after sphere early returns.
Malformed or overflowing bounds fail closed.

Raster and ray paths stage a surface's native positions/normals before
publishing its vertices. Frame-span checks precede indexing; tests include
unaligned records, nonfinite values, extrapolation overflow, truncated spans,
frame/count boundaries, preserved packed fourth components and other-profile
dispatch. Eight x64 AddressSanitizer fixtures pass with `-O2 -ffast-math`.
The Windows x86 build and complete suite results are in the linked record.

The optional `tests/q3_ihv_reference_model_animation_probe.cpp` requires the
project's x86 MSVC environment, C++17, `advapi32.lib`, and
`/BASE:0x3f0000 /FIXED /DYNAMICBASE:NO`. Pass the supplied `quake3.exe`, then
optionally its `baseq3` directory to include all MD3 files. It verifies the
executable hash and invokes selected numeric functions with recording callbacks;
it never starts the original engine, graphics, input, audio or network systems.
No original implementation or asset bytes are incorporated. Logs, generated
sanitizer fixtures and asset identities remain under `.tmp/q3-ihv/animation-*`.

This is **Unit/Build/Fixture** evidence. The follow-up below implements animation
controls and model-light sampling. Native lifecycle/scene dispatch, sorted
flag-`0x80` transform/lighting retention, portals, collision/PVS, audio and
bidirectional protocol-33 sessions remain open. The startup gate stays in place
until those contracts and rendered native demos pass.

## Native model lighting and animation control

The [model-light follow-up](2026-09-21-q3-ihv-model-light.json) implements the
original downward model-light sample in GLx, Vulkan and RTX raster rendering.
It retains an owned raw RGB lightmap copy and separate face/tree metadata in
the world hunk. Atlas packing, overbright conversion and stage collapse cannot
change those samples. Other game profiles do not allocate this state.

The original samples planar faces beneath the entity along a 2,048-unit ray.
It walks BSP leaves front to back, projects onto reciprocal texture vectors,
floors the stored float coordinates, and reads RGB with a 128-pixel row pitch.
Equal-distance surface hits replace earlier samples; traversal prunes when a
split reaches the current nearest distance. A miss or NOWORLDMODEL starts white.
Texture vectors are normalized twice with an intermediate float store; an
algebraically simplified squared-length formula can round differently.

The optional original-function host verifies raw lightmap retention and metadata
for 4,280 planar faces (4,108 lightmapped) in all four supplied BSPs. It compares
the actual BSP43 translator and renderer light loader against 32,768 original
map traces, with 8,528 hits (`1a9be97f02802995`). Another 32,768 traces through 32
generated trees cover rotated planes, misses, backfaces, ordering and split
mutation (`e913a0fd97554969`). The 960 single-face boundary cases match the
original digest `e3c9b7700ac668f1`.

Dynamic lights contribute only within their radius, using squared-radius power
and a minimum distance of 16. Any nonzero native entity flags word adds 32;
colors above 255 normalize together by the maximum channel. Ambient and directed
RGB remain identical. The light direction is a fixed native world vector projected
onto entity axes without normalization. Diffuse vertex colors preserve the
original double arithmetic, truncation, negative-byte wrapping and upper clamp.
All 24,576 dynamic cases (`0349e9b926854cfd`), 2,048 direction transforms and 34,816
diffuse vertices (`fdd6653145e344f2`) match selected original functions. This is
the raster lighting contract; RTX's optional ray lighting still uses its existing
transport/material path and does not establish original rendered parity.

`r_lerpmodels` is registered only for the IHV profile, defaulting to `1` with no
archive flags. It tests the float value: signed zeros and NaNs disable animation;
other nonzero values enable it. Equal frame indices always select zero fraction.
The control matches 86,016 original cases (`cdae50c20e1398f5`), reaches raster and
ray geometry, and changes the model ray-cache signature only for native entities.

Portable tests exercise actual entity setup and diffuse dispatch in all three
renderers, owned lightmap lifetime, malformed extents/numbers, bounded tree walks,
cache invalidation and other-profile isolation. The numeric header scopes precise
arithmetic inside optimized builds and inspects malformed integer-conversion
inputs by address to avoid finite-only scalar argument assumptions. Build,
complete-suite and fast-math sanitizer outcomes are retained in the linked record.

`tests/q3_ihv_reference_model_lighting_probe.cpp` uses the same x86 MSVC setup as
the animation probe. Pass the hash-pinned `quake3.exe`, its `baseq3` directory,
then `baseq3/maps`. Original image/shader/allocation services use CPU recording
stubs; engine startup, display, input, audio and network code never run. Map
shader stubs enable lightmap metadata, while separate renderer tests verify its
material-stage gating. No proprietary inputs are incorporated. Evidence remains
under `.tmp/q3-ihv/model-light-*`.

## Further texture observations

The original `clampTexCoords` parser at `0x43b0e2` sets bit `0x01000000`.
`GL_State` at `0x42aac0` selects `GL_CLAMP` (10496) for both S/T and restores
`GL_REPEAT` (10497) when that bit clears. A second probe also calls original
texture binding at `0x437c90`, with GL calls stubbed. It demonstrates that the
original caches wrapping transitions globally while wrapping belongs to each
image: rebinding alone does not reset that cache. This can retain a prior
texture's clamp state even when the next requested state is repeat.

Legacy clamp blends edge and border samples, as the
[Khronos extension specification](https://registry.khronos.org/OpenGL/extensions/ARB/ARB_texture_border_clamp.txt)
explains. It cannot be replaced by a simple `clampmap` alias. Exact sampling,
filtering and historical state behavior remain open. Probe sources and raw
numeric results are retained under `.tmp/q3-ihv`; the consolidated record is
`.tmp/q3-ihv/next-contract-observations.json`. No native module or sampled game
asset is incorporated.

## Native game boundary

The [native game observations](2026-09-21-q3-ihv-native-game.json) establish
another prerequisite for IHV support. The original game DLL completes `Init`
and `Shutdown` in an independent x86 host using the new external records and
allocation ownership code. These components are **not yet wired into engine
startup**; the probe does not run a map, client, bot, network session or demo.

`code/compat/q3_ihv_game_abi.h` declares the 35 imports and 20 export words in
the original order. The game getter at RVA `0xfad0` returns its table at
RVA `0x7e2e0`, advertises API 4 and initially publishes null/zero entity
storage. Engine `0x4195b0` calls the no-argument initializer, `0x419419` calls
shutdown, `0x419905` passes map/entity text/time to spawning, and `0x41991b`
passes time to a game frame. Declarations for later callbacks follow their
observed call sites; their behavior remains untested. The reserved export at
offset `0x34` remains untyped. User commands and the 164-byte entity state stay
in separate external byte records. The protocol observations below establish
their wire fields; conversion to engine gameplay records is still required.

The shared entity prefix is 268 bytes inside each 556-byte game allocation.
Engine linking at `0x41b970` reads the state number as a signed 16-bit value;
the shared client pointer is at `0xa4`, `inuse` at `0xa8`, `linked` at `0xac`,
link count at `0xb0`, server flags at `0xb4`, brush-model flag at `0xb8`,
bounds at `0xbc/0xc8`, contents at `0xd4`, absolute bounds at `0xd8/0xe4`,
current origin/angles at `0xf0/0xfc`, and owner pointer at `0x108`. The module
owns the undeclared remainder. Engine `0x4190d0` and `0x417640` subtract one
when converting an entity's array index to a client slot: native index zero
is the world and client entities begin at one. The bounded array helper
validates allocation extent, count, stride, alignment and pointer arithmetic;
it does not alias the engine's retail entity array.

Original `Cvar_Get` at `0x40de80` allocates 32 bytes, placing string pointers at
0/4/8/12, flags at 16, modified at 20, float value at 24 and next pointer at 28.
There is no retail modification count or integer field. `CvarMirror` owns
stable string storage and publishes these exact 32-bit records. It rejects
oversized strings and unsupported native pointer widths without partially
changing the previous record. The engine host still needs to resolve cvar
permissions, latching and flag conversion: for example IHV user/server info
bits are `4/8`, latch is `32` and read-only is `128`.

The original initializer registers 28 cvars and requests zeroed tagged blocks:
`maxentities * 556`, `maxclients * 1464` and `maxclients * 700`, all tag 765.
With defaults it publishes 9 entities, capacity 1,024 and 8 clients, and reads
`scripts/bots.cfg`. Shutdown frees that file and tags 766 then 765, leaving no
owned allocations. A second case supplies 128 clients and capacity 64; the
module reduces the client count to 8 through its cvar-set import before
allocating. Missing bot configuration is handled normally. Injected failure
at each allocation aborts initialization and permits host cleanup without
calling shutdown on a partially initialized module. Unimplemented imports
fail explicitly rather than returning synthetic success.

All six cases pass both the release probe and a `/RTC1` debug build, with
identical numeric output and no stack-check failures. The probe verifies the
DLL hash before loading it, restricts file reads to the supplied asset root,
and exposes no filesystem writes, windows, input or network imports. A wrong
hash is rejected before module loading. It uses the original DLL entry point
and static CRT; unlike the earlier getter probe, this is actual isolated
module lifecycle execution.

```powershell
# Run in the project's Windows x86 build environment.
cl /nologo /EHsc /W4 /WX /MD /std:c++17 `
  tests/q3_ihv_reference_game_probe.cpp `
  /Fe:.tmp/q3-ihv/reference-game-probe.exe `
  /Fo:.tmp/q3-ihv/reference-game-probe.obj /link advapi32.lib
.tmp/q3-ihv/reference-game-probe.exe `
  E:/Repositories/Quake3-IHV/ref/install/qagamex86.dll `
  E:/Repositories/Quake3-IHV/ref/install/baseq3 normal
```

Repeat with `missing-files`, `clamp-clients`, or `fail-allocation 0`, `1`, `2`
in place of `normal`. Standalone CI coverage is
`fntech3_q3_ihv_native_boundary`: exact layouts/ordinals, stable cvar storage,
malformed entity pointers/extents, signed entity numbers, tagged ownership,
invalid frees, allocation limits and repeated cleanup. It passes in the
Windows x86 preset and a Clang x64 AddressSanitizer diagnostic build; x64
explicitly rejects native cvar-pointer publication. Existing BSP and Elite
Force ABI tests also pass. Logs and numeric observations are retained under
`.tmp/q3-ihv/native-game-*` and `.tmp/q3-ihv/native-boundary-*`.

## Protocol 33 state, message and demo codecs

The [wire evidence](2026-09-21-q3-ihv-wire.json) records independent codecs in
`code/qcommon/q3_ihv_wire.hpp` and `q3_ihv_message.hpp`. They are **not yet
selected by engine startup, live sessions, demo playback or recording commands**.
No retail/QL state structures, opcode tables or protocol negotiation paths are
changed by these components.

The optional x86 oracle calls the supplied executable's original entity
writer/reader (`0x411090`/`0x411530`), command writer/reader
(`0x411910`/`0x411a50`) and player writer/reader (`0x411b20`/`0x412180`). It
compares encoded bytes, bit counts, decoded records and consumed bits for
1,690 entity cases, 241 command cases and 2,593 player cases at all eight bit
alignments: **36,192 comparisons**. Inputs are synthetic storage-bit changes,
deterministic random states, special floats, entity-number boundaries, removal
and forced/omitted records. Checked-in fixtures contain only numeric hashes
of those results. A wrong executable hash is rejected before any original
code runs; neither its entry point nor platform/import initialization runs.

The external records are 164 bytes per entity, 276 bytes per player and 14
bytes per command. Entity numbers use ten bits and valid values 1–1023; zero
ends the snapshot entity list. Entities use 50 ordered change-bit fields with
mixed byte, short, integer and float storage. Player groups and four masked
arrays use a separate flag format. Commands carry an eight-bit change mask,
short angles, byte movement/buttons/weapon and an unconditional duration byte.
No retail Huffman or keyed usercmd encoding is applied. Untransmitted bytes
are retained from the baseline by the state codecs.

Original behavior includes three details that ordinary retail codecs lose:

- The entity writer compares the new 32-bit light field at offset 144 to the
  old sound byte at 148; its reader uses the old light field. This can suppress
  a real light change and is retained for wire compatibility.
- The player color group compares only its first byte before sending all four.
  x87 float comparisons treat NaNs as unchanged and equate positive/negative
  zero; changed float reads quiet signaling NaNs.
- Player angles use the original single-precision scale and truncate through
  a **64-bit** integer before retaining the low 16 bits. The original `_ftol`
  at `0x448050` establishes that width; an int32 conversion gives wrong results
  for large finite angles.

Message parsing follows the original gamestate/snapshot readers and server
writer observations. A gamestate carries configstring and baseline commands
terminated by opcode zero. Snapshot headers contain five dwords (server time,
message number, delta number, config sequence and command sequence), flags,
area bytes, player delta and sorted packet entities. Server commands carry a
string with no retail sequence prefix. The outer stream ends at the byte
extent, with zero padding in the final partial byte and no retail EOF opcode.

The decoder keeps 32 frames and the original 896-entity delta-age limit.
Limits include 16,384 message bytes, 1,024 configstrings, 16,000 appended
configstring bytes, 1,023 string characters, 32 area bytes and 1,023 entities
per snapshot. Malformed/truncated messages, unsupported downloads and missing
gamestates/delta frames leave previous state and caller output intact.
Server commands are returned as data, never executed. Writers stage each
command before appending, validate sorted entity numbers and delta-frame
identity, and preserve previous output on capacity or validation failure.
Session history selection and live transport remain the driver's responsibility.

The `.dm3` container reader/writer uses signed little-endian lengths and a
`-1` terminator, without a per-record sequence word. Zero, oversized and other
negative lengths fail; missing terminators, truncation and trailing data are
distinguished from successful completion. The supplied `sli.dm3` parses as
57 messages, 56 snapshots and 680 retained entity states from time 1,398,225
through 1,400,975. Regeneration preserves every configstring, baseline, player,
entity and snapshot metadata field. Of 57 messages, 55 are byte-identical.
The first two full snapshots each contain six unchanged entity records whose
baselines are absent (47, 65, 66, 73, 74, 75). The original unchanged-state
reader copies that empty baseline, and `0x40677b` discards number zero.
Canonical regeneration omits those no-op records, reducing each message from
407 to 398 bytes and the recording from 8,628 to 8,610 bytes. The incomplete
`berj.dm3` is rejected before publishing a gamestate.

Both new CI targets pass in the Windows x86 preset and under Clang x64
AddressSanitizer with `-ffast-math`. Coverage includes every truncated byte
prefix of representative messages/records, bit mutations, 4,096 deterministic
random packets, unknown opcodes/flags, duplicate entities/snapshots, stale
delta frames, bad lengths, message capacity limits, overlapping
demo output and transactional recovery. The native-boundary, existing message,
netchan, protocol-contract and BSP43 targets also pass (seven targets total).
This is fixture and memory-safety evidence, not rendered playback or live
interoperability evidence.

```powershell
# Use the project's Windows x86 build environment, with TEMP/TMP in .tmp/q3-ihv.
cl /nologo /EHsc /W4 /WX /MD /O2 /std:c++17 `
  tests/q3_ihv_reference_wire_probe.cpp `
  /Fe:.tmp/q3-ihv/reference-wire-probe.exe `
  /Fo:.tmp/q3-ihv/reference-wire-probe.obj `
  /link /BASE:0x400000 /DYNAMICBASE:NO advapi32.lib
.tmp/q3-ihv/reference-wire-probe.exe `
  E:/Repositories/Quake3-IHV/ref/install/quake3.exe
meson/build/win32/fntech3_q3_ihv_message_tests.exe `
  E:/Repositories/Quake3-IHV/ref/install/baseq3/demos/sli.dm3 `
  E:/Repositories/Quake3-IHV/ref/install/baseq3/demos/berj.dm3
```

No original binary, recording or asset is checked in. Reproduction uses the
user-supplied reference read-only. Logs are retained under
`.tmp/q3-ihv/wire-*`, `.tmp/q3-ihv/message-*` and
`.tmp/q3-ihv/reference-wire-*`.

## BSP corrections

The inherited converter rejected `ihv_test1` and `tim_dm1` because the dword at
surface offset 68, previously labelled `brushSideNum`, sometimes exceeds the
collision-side table. The supplied maps contain 49 and 47 such surfaces,
respectively. Treating an in-range value as a side also borrowed unrelated
flags and could hide patches. Original executable observations at
`0x4083f0–0x4084c0` show patch loading from dimensions/vertices and zero surface
flags; that field is not used there. The translator now ignores this unproven
association, preserves actual brush/side flags independently, and uses neutral
surface flags. Retail collision receives solid patches; this is an explicit
translation policy, **not proof of IHV trace-mask or trace-result parity**.

Inline models now use their declared face ranges and exact unique brush sets
from their headnodes. Unordered lists, duplicate leaf references and internal
headnodes no longer silently include unrelated brushes or produce empty models.
Only noncontiguous sets require duplicate output brush records; world leaf
indices keep their original meanings.

The translator validates byte ranges without signed overflow, normalizes
unaligned inputs, checks finite numeric records and terminated shader names,
validates patch dimensions, bounds visibility rows and triangle-fan growth,
and rejects node cycles, shared internal subtrees and excessive tree depth.
Failure does not publish a partial output, and scratch allocations are released.

Collision checksums now come from the original installed bytes before
translation. `km_portal.bsp` computes to `425045198`, matching the IHV recordings'
`mapChecksum`; checksumming the generated BSP 46 bytes changed that identity.
Other profiles and nontranslated maps keep their original file checksums.

## Validation and reproduction

Build the project's normal Windows x86 preset. The shared client/server and
all three renderer modules build successfully. `fntech3_bsp43_contract_tests`
executes synthetic geometry/membership checks, every truncated fixture prefix,
malformed trees/ranges/patches/visibility/numbers, unaligned input, allocator
failure and recovery. Its optional arguments load supplied files read-only:

```powershell
meson/build/win32/fntech3_bsp43_contract_tests.exe `
  E:/Repositories/Quake3-IHV/ref/install/baseq3/maps/ihv_test1.bsp `
  E:/Repositories/Quake3-IHV/ref/install/baseq3/maps/km_portal.bsp `
  E:/Repositories/Quake3-IHV/ref/install/baseq3/maps/test_fog2.bsp `
  E:/Repositories/Quake3-IHV/ref/install/baseq3/maps/tim_dm1.bsp
```

The same tests/files pass a Clang AddressSanitizer build using the existing
Windows x64 Q3-only diagnostic define. This is a memory-checking test harness,
not evidence for a complete x64 IHV engine. The four loader controllers also
pass their existing Q3/QL/RTCW profile and resource-lifetime tests; the checksum
stub now verifies that it receives the original file bytes.
The full Meson suite passed 121 targets with two skips and no failures; its
Python suite passed 1,828 cases with 72 skips. Final focused BSP43, QL BSP,
message-codec and netchan tests also passed after the tree-depth correction.
The final checksum-publication ordering was rebuilt and the three loader
controller tests passed again, including a failed-conversion checksum check.

Use a new output directory for each runtime probe:

```powershell
python scripts/probe_q3_ihv.py `
  --installation E:/Repositories/Quake3-IHV/ref/install `
  --retail 'C:/Program Files (x86)/Steam/steamapps/common/Quake 3 Arena' `
  --client meson/build/win32/fntech3.exe `
  --output .tmp/q3-ihv/reproduce-glx --renderer glx
```

Repeat with `--renderer vk` or `rtx` and a fresh output path. This copies assets
into independent writable homes, reads the supplied retail paks, disables
mouse/joystick/sound, forces windowed rendering on a private unswitched desktop,
and uses `screenshotPNG`. Reports preserve shader warnings. It deliberately
loads retail QVMs and does not count these runs as IHV module or protocol tests.

Logs, reports and engine captures are retained under `.tmp/q3-ihv`. The reference
installation stays unchanged. Probes also emitted a host IPv6-loopback bind
warning while their IPv4/local sessions succeeded.

Automatic approval review rejected cleanup of disposable test copies with
`blocked by policy`. They remain in `.tmp/q3-ihv` along with subsequent material
probes. No alternate deletion mechanism was attempted.
