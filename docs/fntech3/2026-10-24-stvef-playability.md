# Elite Force against the replacement rule, 2026-10-24

AGENTS.md invariant 12, added today, asks every game to play fully and
perform well in fnTech3, with its servers and clients playing with the
retail ones in both directions and its own rendering drawn correctly, and
not to reproduce what a modern replacement leaves behind: disc and key
checks, dead online services, obsolete hardware paths, or a retail engine's
incidental output. This record reads Elite Force's open items against that
rule. It adds evidence the gate matrix lacked: RTX's path tracing runs both
modes on Windows x86, and fnTech3 plays Elite Force faster than
`stvoyHM.exe` and `stvoy.exe` 1.20 on every raster renderer. On the way it
fixes two defects: Holomatch's holodeck intro under the path tracer, and
Holomatch's internet browser, which could not read an Elite Force master's
list. Raven's master has gone, so Holomatch now also asks the community
master by default. It finds one fault in the retail single-player module,
which a probe reached by lowering `timescale`. It records which differences
are now accepted, and judges the Elite Force gates by the ledger's new
[gate sign-off convention](COMPATIBILITY.md#gate-status-and-sign-off), after
damaged-input probes for the traffic, demos, maps and saves the gates read:
seven of the thirteen pass. The
[JSON companion](2026-10-24-stvef-playability.json) holds the commands,
binaries, hashes and every run. This is Live, Interop, Unit and Source
evidence.

## Identity

Until the fix below, every fnTech3 run used `meson/build/win32-dev`, built
from `1b11e9e8` while the working tree differed from it only in `AGENTS.md`
and `scripts/probe_rtx_games.py`. Its binaries were client `3639aa0b…`, GLx
`2e196492…`, RTX `583b3d12…` and Vulkan `064cdbce…` (SHA-256 prefixes; the
companion has them whole), unchanged from before the first timed run to after
the last. The timings below used the renderer before the fix; the fix touches
only entities drawn with Elite Force's forced alpha. The traced runs after it
used a runtime built from this round's tree alone (`44e61fb2…`, built in an
export of it and staged at `.tmp/runtime/stvef-compat-after`): client
`d77c99d2…`, RTX `6e1fcfb6…`. An in-place rebuild of the developer build's RTX
renderer had also compiled another session's uncommitted renderer edits, so
its runs are not counted. Retail ran from the staged Holomatch runtime
(`.tmp/runtime/stvef-retail`), `stvoyHM.exe` `5b5b6e82…` and `stvoy.exe`
`8f7ccadb…`, reading copies of the four retail paks. fnTech3 read the same
paks and the game libraries from `.tmp/stvef-stock`, without the
installation's third-party texture pack. Each process ran windowed on a
private desktop that is never switched to, with no sound, no mouse and no
injected input, and an isolated home. The machine is a Windows 11 laptop on
mains power, with an NVIDIA GeForce RTX 4060 Laptop GPU and driver 591.74.

## Path tracing runs both modes

`scripts/probe_rtx_games.py` gained both Elite Force profiles. Single player
takes the October 9 `voy2` view (frozen NPCs and effects, no HUD) and
Holomatch joins `hm_borg1`; each then freezes the clock with `fixedtime 1`,
takes a traced capture under `rtx_rt_mode 2` with `rtx_rt_require 1`,
restarts into raster and takes a second. With the fixed renderer:

| Mode | Gate | Traced GPU time, 1280x720 | Exit | Traced / raster brightness |
| --- | --- | --- | --- | --- |
| Single player, `voy2` | active, compute | 21.3 to 31.3 ms | 0, no error | 2.636 |
| Holomatch, `hm_borg1` | active, compute | 24.6 to 26.3 ms | 0, no error | 1.054 |

Both runs pass the probe: the gate is active with the compute backend, primary
dispatch validates as tracing, the log has no error, and the process exits
normally. The driver still offers the 32-bit client no ray-tracing pipeline
(`VK_KHR_acceleration_structure`, `VK_KHR_deferred_host_operations` and
`VK_KHR_ray_tracing_pipeline` missing), and the compute backend traces without
one. This supersedes the September 21 observation of raster fallback or a
strict refusal only ([runtime
coverage](2026-09-21-stvef-runtime-coverage.md)). Single player's world is
42,962 traced triangles in a 26,719-node BVH, lit by 73 point lights and 3,021
emissive triangles; Holomatch's is 14,525 triangles. Single player's `voy2`
view faces a dimly lit wall, which the traced frame shows 2.6 times as bright
as raster. Five more October 9 views, traced the same way from this round's
runtime (`.tmp/stvef-playability/sp_scenes_rtx.py`), put that in proportion:

| Scene | Traced | Raster | Traced / raster |
| --- | ---: | ---: | ---: |
| `voy2` | 29.0 | 11.0 | 2.64 |
| `stasis1` | 16.1 | 26.4 | 0.61 |
| `scav2` | 48.2 | 35.0 | 1.38 |
| `borg2` | 13.3 | 11.2 | 1.19 |
| `forge2` | 15.2 | 13.0 | 1.17 |
| `voy14` | 29.5 | 24.8 | 1.19 |

Mean brightness of the top 83 % of each frame, out of 255. Most scenes trace
about a fifth brighter than raster, within the 0.83 to 1.49 the other
profiles' probe views gave ([path tracer
record](2026-09-24-rtx-path-tracer.md)); `voy2` is the dim exception, and
`stasis1` the dark one. Its corridor ends in a stasis door drawn with
`textures/stasis/m_stasisdoor3` and `m_stasisdoor3_mem`, single opaque stages
with `surfaceparm nolightmap`, which raster draws fullbright, whatever the
light around it. The tracer lights such a surface by the scene and lets only
its bright texels glow (`RT_ShaderFullbrightLit`,
`code/rendererrtx/rt/rt_material.c`: from a channel of 0.55, fully at 0.85),
as the [RTX guide](../RTX.md) tells players ("exactly as bright or as dark as
its own lights make it"). The door's textures are dark (mean 0.08, 99th
percentile 0.46 and 0.60), so traced it is nearly black. That is the tracer's
rule for every profile, not an Elite Force fault. Asked whether surfaces a map
leaves unlit should glow at their full colour under it, the maintainer kept
the rule for every profile (2026-10-24), so the traced door is an accepted
difference.

## The holodeck intro under the path tracer, fixed

A player joining a Holomatch map stands inside a holodeck room while the
level appears around it. The retail client game draws that room as a model,
`models/mapobjects/podium/hm_room.md3`, in one procedure of `vm/cgame.qvm`
(`pak2.pk3`, from instruction 72472): opaque for its first three seconds while
its frames assemble it, then with its alpha falling from 255 to 1 over two
seconds, with render flags `0x818`. The `0x800` among them is Elite Force's
forced alpha, which blends each single-texture stage with the entity's
alpha; its shader, `models/mapobjects/podium/holodeck`, has no blend of its
own. The raster renderers honour that flag
(`code/renderercommon/tr_stvef_alpha.h`).

The path tracer did not. `RT_EntityClass` (`code/rendererrtx/rt/rt_backend.c`)
already sends Jedi Academy's colour and alpha overrides to the raster
overlay, because a traced material is per shader and cannot carry them, but
it traced Elite Force's fading room as an opaque mesh. The first Holomatch
traced capture therefore showed a different scene from the raster one taken
a moment later at the same viewpoint: walls, pillars and the holographic
weapon pickup missing, other parts of the map showing through. Three runs
separate this from timing. `vid_restart` leaves the player where it was
(`(160 -360 -206) : 45` before and after, GLx, normal time). On GLx alone,
the probe's two captures of the intro differ by 5.9 of 255 with the same
structure. After letting the intro end (`fixedtime 0; wait 900` before the
freeze), traced and raster agree.

`RT_EntityClass` now also leaves an entity to raster while its forced alpha
is in force, an alpha from 1 to 254, the same test `R_STVEFForcedAlpha`
makes. At 0 and 255 raster draws it with its own stages and the trace keeps
it. With the fix the traced intro matches raster (traced/raster brightness
1.05, against 1.25 before). `tests/rtx_surface_owner_tests.py` runs the real
`RT_EntityClass` and `vk_rt_surface_owner` and now checks both cases; the
new checks fail against `1b11e9e8`'s tracer. Only entities carrying Elite
Force's forced alpha reach the new test, so no other profile changes;
the three other test files that compile `RT_EntityClass` pass.

Feature use (invariant 11): the traced world, models and lights are the
path tracer's own; an entity the trace cannot express falls back to the
raster overlay, as Jedi Academy's do, and keeps raster's forced-alpha
blending. No retail-exact option is involved.

## A retail module fault below timescale 1

The first single-player traced run stopped with `ACCESS_VIOLATION`
`efgamex86.dll@131d8` once the probe set `timescale 0`, on GLx as on RTX.
The view stands inside `voy2` brush 2233, `textures/common/ladder`
(contents `0x20000080`). The retail module explains it:

- Its client game's prediction runs its own `Pmove` only while the
  `timescale` cvar is below 1.0 (`0x20032659..0x2003266a`), and sets the
  move's entity to null first (`0x200326a3`).
- The water check tests the contents at the feet and waist against `0xb8`,
  which counts ladders (`0x80`) as water (`0x2001315c`, `0x200131c8`).
- In that case it compares the entity's NPC type with `"biohulk"`
  (`0x200131d2..0x200131e4`), reading through the null entity.

Retail does the same. `.tmp/stvef-playability/retail_timescale.py` runs
`stvoy.exe` 1.20 and fnTech3 (GLx, this round's runtime) through the same
steps: `fixedtime 50`, `devmap voy2`, its opening waited out, NPCs and
effects frozen, the October 9 view, then `timescale 0.5`. Retail stopped at
once with exit code `0xC0000005`; its Windows crash dump faults at
`efgamex86+0x131d8`, `mov ecx,[eax+35Ch]` with `eax` zero, the instruction
fnTech3 reported. fnTech3 caught the same fault and returned to its menu. At
`timescale 1` retail runs the same steps to the end and exits normally. The
dump was moved out of Windows' crash folder and the older dump it had
evicted put back. The game reaches the fault with cheats on, or when
a level script slows time (its `timescale` hook is at `0x200a71d0`) while
the player is on a ladder or in water; whether a shipped script does so
there was not checked. The fault is the retail module's own, which fnTech3
survives where retail does not, so the engine does not change; the probe no
longer lowers `timescale` for either Elite Force profile, as `fixedtime 1`
already freezes the view.

## Performance

`.tmp/stvef-playability/perf.py` timed each of the three retail Holomatch
demos in `timedemo`, one process per demo and engine, every engine back to
back per demo, the whole set three times. Single player has no demos, so it
loads `voy2`, freezes its NPCs and effects at the October 9 view and times
3,000 uncapped frames between two console markers read from the log. All
runs are 1024x768 windowed (retail `r_mode 6`), `r_swapInterval 0`,
`com_maxfps 0`, no sound. fnTech3 runs its own presentation defaults, as a
player gets them. Other sessions were compiling and testing on the same
machine throughout, at 21 to 97 % of the CPU during the runs, so single runs
vary; the table gives each engine's best of three, and the companion every
run with the machine's load during it.

| Scene | retail 1.20 | GLx | Vulkan | RTX raster | RTX traced |
| --- | ---: | ---: | ---: | ---: | ---: |
| `demo0000` | 226 | 586 | 428 | 772 | 53 |
| `demo0001` | 344 | 766 | 770 | 1181 | 32 |
| `extreme` | 284 | 533 | 490 | 729 | 36 |
| `voy2` view (SP) | 889 | 995 | 995 | 1005 | 50 |

Frames per second, best of three. Every timedemo run reports the frame count
retail reports (11,803, 6,248 and 2,268).

On every raster renderer fnTech3 plays each Holomatch demo faster than retail:
its best run is 1.7 to 3.4 times retail's best. Path-traced Holomatch runs at
32 to 53 fps through the compute backend on this laptop GPU, well below
raster; tracing is an optional mode. In single player the three raster
renderers reach about 1,000 fps, the engine's floor of one millisecond a
frame, so that view does not limit them; retail reaches 889, and the path
tracer 50. A first single-player pass reported 62.5 fps for every fnTech3
renderer: a private-desktop window never has focus, and `com_maxfpsUnfocused`
keeps its default 60 there; the rerun sets it to 0, and the companion keeps
both. Retail varied most with the load (`demo0000` 82 to 226 fps).

All 60 counted runs exited normally with no error.

## Server lists

Holomatch's server and browser use retail's own master,
`master.stef1.ravensoft.com:27953` (`stvoyHM.exe` 0x434091), as
`sv_master1`. That master is gone. Its name still resolves (12.206.251.141),
but on 2026-10-24 at 08:14 UTC it answered none of three `getservers 24 full
empty` queries, three seconds apart, the request fnTech3's browser sends it.
The same query to Quake III's default master, `master.quake3arena.com:27950`
with protocol 68, answered at once with ten packets listing about 988
servers, so the query and this machine's network were sound. No other master
was contacted then. Under invariant 12 the retail default is now a dead
service: Holomatch's internet list was empty, and a public server's
heartbeats went unanswered, as they do for any player of the retail game.

Lilium Voyager 1.40 lists the community master `efmaster.tjps.eu` as its
`sv_master2`. With the maintainer's approval it is now Holomatch's default
`sv_master2` too (`Com_GameCommunityMasterServer`,
`code/qcommon/game_profile.cpp`, read by `SV_Init`), with Raven's kept as
`sv_master1`. The retail menu offers each master as a server source,
`INTERNET1` to `INTERNET5`: its source N asks `sv_masterN`, skipping an
empty one, and sends `globalservers N 24` (`vm/ui.qvm` in `pak2.pk3`, from
instruction 61875, and the command built at 61748), so the community list is
`INTERNET2`.

That master answered fnTech3's own request (`globalservers 2 24 full empty`)
on Raven's port, 27953, but fnTech3 listed none of its servers. It answers
in Holomatch's own text form, `getserversResponse \C00002016D38\...` (here
192.0.2.1:27960, a documentation address): after each backslash eight hex
digits of address and four of port. fnTech3 read
Quake III's binary entries there, so the first digits failed its syntax
check. Retail `stvoyHM.exe` reads exactly the text form
(`CL_ServersResponsePacket` 0x40e6e0): it skips to a backslash, reads twelve
hex digits in any case, keeps the entry only when another backslash follows
(0x40eb4e), stops at `\EOT`, at the end of the packet or after 256 entries a
packet (0x40eb95..0x40ebb8), and keeps up to 2,048 (0x40ebe7). A request
naming the game, `getservers STEF1 24`, as ioquake3-derived ports may send,
gets an empty list from this master, so the bare retail request fnTech3
sends is the one it lists Holomatch servers for.

fnTech3 now reads Holomatch's lists as retail does
(`code/client/client_stvef_masters.hpp`, called from
`CL_ServersResponsePacket` for the Holomatch profile's ordinary replies). It
stops at a character that is not a hex digit, and inside the packet, where
retail would read a stray character as 0 and read on. Every other profile's
lists, and Holomatch's extended IPv6 replies, are read as before. With this
round's build (client `40ff7f30…`, from an export of the round's tree, staged
at `.tmp/runtime/stvef-compat-after`) and none of its own settings,
`.tmp/stvef-playability/master_query.py` had the browser ask both defaults:
Raven's master stayed silent and `efmaster.tjps.eu` listed 19 servers
(`getserversResponse: 19 servers parsed`). `tests/stvef_master_list_tests.py`
runs the parser on synthetic lists with documentation addresses, well formed
and damaged, including every truncation of an entry;
`tests/legacy_server_discovery_tests.py` now checks every profile's second
default, only Holomatch's being set.

Not verified: whether `efmaster.tjps.eu` lists a public fnTech3 server from
its heartbeat, which this machine cannot host where the internet reaches it,
and joining or even pinging the servers it lists, which are other people's.

## The rule applied

**Not reproduced.** The disc check (`Game CD not in drive`, `NEED CD` in
both executables), CD-key authorization through Raven's authorize server
(`Awaiting CD key authorization`, `authorize server timed out` in
`stvoyHM.exe`), MIDI input and IPX. No authorize or master success is
synthesized.

**Accepted differences**, recorded here once rather than as open gaps:

- Flare appearance. fnTech3 tests flares with GL's depth margins in every
  renderer, so they show at the lights retail shows them
  ([renderer parity record](2026-10-23-stvef-renderer-parity.md)); their
  size and fade follow fnTech3's flare settings rather than retail's
  intensity rule. Presentation, invariant 11.
- Vulkan's and RTX's default bloom and uncompressed textures, fnTech3
  features (same record).
- The traced look of surfaces a map leaves unlit, such as `stasis1`'s
  door: the tracer lights them by the scene in every profile, by the
  maintainer's choice.
- Pixel identity with retail beyond the measured bounds: GLx 0.03 to 1.4
  of 255 in eight synchronized scenes, the three renderers within about
  0.25 of one another like for like, and 0.39 to 2.20 over 54 Holomatch
  effect frames ([effect captures](2026-09-21-stvef-effect-captures.md)).
- The defaults, flags, unread settings and serverinfo key order the
  [registrations record](2026-10-23-stvef-retail-registrations.md) left as
  they are.

**Not run.** Hands-on keyboard, mouse and joystick checks and listening
checks (the user chose to skip them on 2026-10-08), and force-feedback
output (no device). Joystick input on Windows is absent in every fnTech3
profile, not only Elite Force: the Windows client compiles its joystick
code out (`meson.build` defines `USE_JOYSTICK` only for Linux without SDL).

**Already in evidence**, for play: both modes move, look, fire, rebind and
save and load through their menus with the engine's own input
([input record](2026-10-08-stvef-input-menus.md)); single player plays
through its campaign to the closing movie and credits
([campaign record](2026-10-09-stvef-sp-campaign.md)); Holomatch plays every
game type with bots and plays with retail and Lilium as client and server,
pure and not, downloads included (gate matrix, Interoperability).

## Gate sign-off

The ledger had status values but no rule for when a gate passes. Asked
whether to add one and mark the Elite Force gates that meet it, the
maintainer said yes (2026-10-24). The
[convention](COMPATIBILITY.md#gate-status-and-sign-off) passes a gate for a
profile only with evidence at the level it needs over its whole scope on the
shared Windows x86 build, damaged input included wherever it reads outside
data; nothing open, unverified or not run in its row except differences
accepted under invariant 12 and stated platform limits; and committed
identities. A pass closes that gate alone.

Elite Force had no damaged-input evidence for its traffic, demos, maps or
saves: the corrupted-traffic, damaged-demo and damaged-map probes had run
under Quake III, Quake Live and RTCW only. Each probe gained the Elite Force
profiles, and `scripts/probe_stvef_save_faults.py` was written for single
player's saves (a save fnTech3 writes of `voy2`, then fault variants each
loaded in a fresh process). All ran this round's committed code, client
`40ff7f30…` with dedicated server `a1a2dc82…` from commit `7f7b1f56`,
windowed on private desktops:

| Probe | Profile and input | Cases | Outcomes |
| --- | --- | ---: | --- |
| `scripts/probe_net_corruption.py` | `stvef-mp`, `hm_borg1`, one sequenced datagram in 8 for 15 s, both ways | 20 | 19 dropped, 1 connected |
| `scripts/probe_demo_faults.py` | `stvef-mp`, `extreme.efdemo` from `pak2.pk3`, with 40 seeded body faults | 47 | 41 rejected, 6 completed |
| `scripts/probe_bsp_faults.py` | `stvef-mp`, `hm_borg1`, with 17 seeded lump faults | 90 | 81 rejected, 9 loaded |
| `scripts/probe_bsp_faults.py` | `stvef-sp`, `voy2`, with 17 seeded lump faults | 89 | 78 rejected, 11 loaded |
| `scripts/probe_stvef_save_faults.py` | `stvef-sp`, a `voy2` save of 727,763 bytes in 3,407 chunks, with 20 seeded faults | 35 | 34 rejected, 1 loaded |

None faulted, crashed or hung. Every run wrote only to its own home under
`.tmp/stvef-faults`, and the damaged-demo probe also found the game folder
it read, `.tmp/stvef-stock`, unchanged. A damaged
save is refused by the chunk checks, which give retail's own messages
(`Failed checksum check for chunk`, `Bad savegame magic for chunk COMM`);
one with bytes after its last chunk loads, as retail reads a save chunk by
chunk. A damaged demo ends in the demo reader's length checks or the
message decoder's bounds, and a damaged map in the loader's lump checks or a
level that still loads.

Retail `stvoyHM.exe` 1.20 also plays a demo fnTech3 records: a listen-server
session on `hm_borg1`, recorded with `g_synchronousClients 1`
(`.tmp/stvef-faults/demo_interop.py`), plays to its end in retail, 288
frames, as in fnTech3, with no error. fnTech3's recordings had been played in
fnTech3 and Lilium Voyager only.

| Gate | Status | Why |
| --- | --- | --- |
| Profile / identity / discovery | PARTIAL | other retail installations not probed |
| Audio codecs | PARTIAL | listening checks not run |
| Holomatch wire codec | PASS -- Interop | retail demos decode and re-encode; both protocols live both ways; corrupted traffic dropped cleanly |
| Holomatch game module / server | PASS -- Live, Interop | the retail game module in every mode with bots; retail clients on it; it survives corrupted traffic and damaged maps |
| Holomatch client framing / demos | PASS -- Interop | retail handshake; retail demos play; retail plays fnTech3's demo; damaged demos and traffic end cleanly |
| Holomatch cgame / UI | PARTIAL | the menu's internet sources not driven through the menu |
| Languages | PARTIAL | listening checks not run |
| SP module host / savegames | PASS -- Live, Interop | the campaign on the retail module; saves cross both ways with retail; damaged saves and maps end cleanly |
| SP menus / cgame | PASS -- Live | the retail menus and cgame on every level, saving and loading through the menus |
| Renderer / cinematics | PASS -- Live | all three renderers and path tracing, effects compared with retail, movies, damaged maps; accepted differences recorded |
| Interoperability | PASS -- Interop | retail and Lilium as client and server, both protocols, pure, downloads, loss |
| Input / physical devices | PARTIAL | physical devices not run |
| Optional services | PARTIAL | a public fnTech3 server's listing on the community master not verified |

Neither mode is fully compatible: its other gates stay open, and the full
sign-off on release artifacts is still to come.

## Not covered

- Live internet servers: none was joined or pinged. Of the masters, Raven's,
  `efmaster.tjps.eu` and, as a control, Quake III's were queried.
- Path tracing beyond six single-player views and one Holomatch view with
  its intro; other entities drawn with forced alpha were not captured
  traced; the denoised image's quality.
- Other GPUs, drivers and an idle machine's frame rates.
- Single-player frame rates in play rather than at a frozen view.
