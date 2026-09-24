# Explosives the world owns: RTCW MP and ET dynamite

## Scope

A developer build running `rtcw-mp` dropped its listen server as soon as a
thrown charge of dynamite came to rest:

```text
ERROR: SV_ClipMoveToEntities: invalid pass entity 1022
----- Server Shutdown (Server crashed: SV_ClipMoveToEntities: invalid pass entity 1022) -----
```

Entity 1022 is `ENTITYNUM_WORLD`. The world's record lies past the entity
count RTCW MP's game publishes, and fnTech3 only let the server read records
inside that count. ET's game does the same with dynamite, landmines, satchels,
tripmines and smoke, so the `wolfet` profile has the same defect. This record
covers both profiles.

## Observed

### Game modules

The GPL sources say what happens. `G_BounceMissile` (`g_missile.c`) makes the
world the owner of a charge that stops moving, "so the player can shoot it".
`G_RunMissile` then passes `ent->r.ownerNum` to `trap_Trace` as the pass entity
on every frame. `G_InitGame` clears a `g_entities[MAX_GENTITIES]` arena and
calls `trap_LocateGameData` with `level.num_entities`, which starts at
`MAX_CLIENTS` and grows only as `G_Spawn` opens slots. `SP_worldspawn` numbers
the world's record `ENTITYNUM_WORLD` and fills its spawn flags, `r.worldflags`
and class name. It never sets the record's owner, so the owner stays at the
zero `G_InitGame` cleared it to.

The retail modules were read with capstone. Addresses are virtual addresses in
each image.

| | RTCW MP `qagame_mp_x86.dll` | ET 2.60b `qagame_mp_x86.dll` |
| --- | --- | --- |
| SHA-256 | `d8975b5c4e14488283c5eca7d549005304b04f1690705c13c174a171cc24c387` | `160a6218397865ad61071faa6316053713c03b9be80e5d05c7a5be2cab7f630b` |
| Arena cleared in `G_InitGame` | `rep stosd` at `0x20041d3f`: 1024 records of 1352 bytes from `0x21ed7900` | `rep stosd` at `0x20051ff7`: 1024 records of 1404 bytes from `0x206be1a0` |
| First `LocateGameData` | `0x20041da5`: base `0x21ed7900`, count 64, stride `0x548` | `0x20052063`: base `0x206be1a0`, count 64, stride `0x57c` |
| World made owner (`r.ownerNum = 0x3fe`) | `0x20047d7c`, after testing weapons 32 and 33 (dynamite) | `0x20057a4e` (bounce) and `0x2005a385` (map landmines) |
| Owner passed to the trace | `0x20048ae6..0x20048b0d`: `[esi+0x2a0]` pushed as `passEntityNum` | the same source line; not traced in the image |
| World record written by `SP_worldspawn` | `0x2005a4e4..0x2005a4f8`: spawn flags (+`0x2b8`), `r.worldflags` (+`0x2a8`), number `0x3fe` (+0), class name (+`0x2b4`); nothing at the owner (+`0x2a0`) | not traced |

The retail engines read that record without a bound:

| | `WolfMP.exe` | ET 2.60b `et.exe` |
| --- | --- | --- |
| SHA-256 | `7cf99a938cf18ed2f5aecd0be542cbae8868959e0cce24b4b6eea08991e321f5` | `4d1326c5936b61c5f878fc017e4aa0abddf13e008139e31fce2f13d57b26243e` |
| `SV_GentityNum` | `0x436530`: `sv.gentities + num * sv.gentitySize`, no range test | `0x445320`: the same |
| `SV_ClipMoveToEntities` pass owner | `0x43d28b..0x43d2b6`: if the pass entity is not `0x3ff`, read its record's `+0x2a0` and map `0x3ff` to -1 | `0x449db9..0x449de8`: the same with `+0x180` |

The server therefore reads the world's owner, 0, and a settled charge's traces
skip anything owned by the world (other settled charges) and anything owned by
client 0, as well as the pass entity itself. That filtering is inferred from
the code above; no retail session was compared against it.

### fnTech3

`ServerGameDataView` admits entity numbers below its bound capacity.
`SV_LocateGameData` set a capacity past the published count for RTCW SP only,
whose `G_LoadGame` links restored entities before its last `LocateGameData`.
Every other profile bound exactly the published count, so
`SV_ReadGameEntityByNum( 1022 )` failed and `SV_ClipMoveToEntities` raised the
error above.

## Change

`SV_GameEntityArena` in `sv_game.cpp` now names the arena for each profile
whose module owns one past its published count: 1024 records for both RTCW
modes (`FNTECH3_RTCW_GENTITYNUM_BITS`) and for ET
(`FNTECH3_WOLFET_GENTITYNUM_BITS`). `SV_LocateGameData` binds that capacity,
and `ServerGameDataView::BindNative` accepts it for RTCW MP and ET as it already
did for RTCW SP. Snapshots, the level-start walk and the ET visibility-proxy
checks still stop at the published count; only reads by number and by pointer
reach the rest of the arena. A native binding claims the full span, and a QVM must hold it
inside its data segment or the binding is refused. The trace itself is
unchanged: it now reads the world's real record, as retail does, and takes its
owner from there rather than assuming one.

Quake III, Quake Live, Holomatch, Jedi Outcast and Jedi Academy still bind
exactly their published counts. None of their reference modules was found
making the world an owner or passing entities past the count to a trace.

## Evidence

- `tests/server_game_data_tests.cpp` binds RTCW MP and ET with the retail
  strides, count 64 and capacity 1024. It reads the world's number and zero
  owner at 1022, by index and by checked pointer, and still rejects record
  1024, a capacity of 1025 and a span one byte short. Quake III, Quake Live,
  Holomatch, Jedi Outcast and Jedi Academy still refuse a capacity larger than
  their count. Against the previous validator the ten new RTCW MP and ET checks
  fail.
- `tests/world_owner_trace_tests.py` compiles the production
  `SV_GameEntityArena`, the view, the entity reads and `SV_Trace` with
  `SV_ClipMoveToEntities`. It traces with the world as the pass entity over
  retail-sized RTCW MP and ET arenas holding two clients, two world-owned
  charges and one grenade from each client. Only the two clients and client 1's
  grenade are traced, the module memory and its guards are unchanged, and a
  pass entity past the arena is still refused. Quake III and Quake Live, with no
  arena, still refuse the world as a pass entity. Run with the previous arena
  rule, the RTCW MP and ET traces are refused as the live build was.
- Runtime, Windows x86 `meson/build/win32-dev` (`-Drtcw-development=true`,
  `-Dwolfet-development=true`), tree at `7173858` plus this change and other
  sessions' uncommitted work. Each run used a private desktop, a windowed
  640x480 client, mouse input off, sound off and isolated homes under
  `.tmp/world-owned-explosives/`, with the legitimate Steam RTCW and ET
  installations read-only and their inventories unchanged.
  `scripts/probe_world_owned_explosives.py` joins the Allies as an engineer,
  closes the limbo menu, selects dynamite, looks down and throws one charge
  with a single console `+attack`/`-attack`, then captures with
  `screenshotPNG` half a second and five seconds later. The user allowed this
  firing probe for this task.

  | Run | Server | Result |
  | --- | --- | --- |
  | RTCW MP listen, previous build | `fntech3.exe` `8914694b57772a96877569e4c634d8e2ada7d8a88d989772950a180dd64f4c3a` | Charge thrown ("Dynamite is set, but NOT armed!"), then `invalid pass entity 1022` and the server shut down |
  | ET listen, previous build | the same | the same error on oasis |
  | RTCW MP dedicated, fixed | `fntech3.ded.exe` `1733e622ac66b989b14452b954216e23ac7bccdb71524aebb723d16ef80002ab`, previous client connected over 127.0.0.1 | Charge at rest on mp_beach's sand in the five-second capture; no errors on either side; the client quit cleanly and the server was still running |
  | ET dedicated, fixed | the same server | the same on oasis |
  | RTCW MP listen, fixed | `fntech3.exe` `a06362707a8e5bb2f83e574e5dc0eb8bebb6c23f056b6202d576bcd3e3681abd` | Charge at rest; no errors |
  | ET listen, fixed | the same | Charge at rest; no errors |

  RTCW MP's `ClientSpawn` takes the class from the usercmd's `mpSetup` bits,
  which its cgame builds from `mp_playerType`, so the probe sets that cvar as
  well as passing the class to `team`. The RTCW and ET entity-delta readers do
  not print `cl_shownet` records, so the captures and the paired refusals are
  what show that a charge was thrown and settled.

## Not established

- A retail session comparing what a settled charge's traces then collide with.
- Arming, defusing and exploding charges, landmines, satchels, tripmines and
  smoke under fnTech3; only an unarmed charge was thrown.

## Later work

Original RTCW MP's `SV_MapRestart_f` reads the world record's `r.worldflags` to
keep `g_gametype` to a type the map allows. With the record reachable,
fnTech3's `map_restart` now makes that check for `rtcw-mp`; see
`2026-09-23-rtcw-mp-restart-game-type.md`.
