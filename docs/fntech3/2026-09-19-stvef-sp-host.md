# Elite Force single-player game host

## Scope

This record covers hosting the retail single-player game module,
`efgamex86.dll`, on fnTech3's server: its function-table boundary, its
records and the server's view of its entities. The cgame half of the same
library, snapshots on the loopback wire, the menus in `efuix86.dll`,
savegames, level transitions, movies and lip-sync belong to later slices.
The profile stays behind `-Dstvef-development=true`.

The frozen single-player records in `code/compat/stvef_game_abi.h` cite this
record for their retail evidence; the offsets below are the ones retail code
reads.

## The boundary

| Contract | Retail `stvoy.exe` 1.20 | fnTech3 |
| --- | --- | --- |
| Loading | Loads `efgamex86.dll` from beside the executable (`0x42e510`), passes `GetGameAPI` 42 imports filled in a fixed order (`SV_InitGameProgs` `0x425490`), then attaches the cgame half of the same library: `dllEntry` with the client's syscall handler, `vmMain` kept (`0x42e670`). It refuses any API version but 6 | `VM_CreateTable` finds the library as the profile's native module and resolves `GetGameAPI`; the host passes the imports in retail's order, calls `dllEntry` before `Init` and requires version 6. The game half calls cgame functions as it runs (it starts world sounds through them), so without the attachment the game crashes at its first sound; with no client, as on a dedicated server, those calls do nothing |
| Exports | `Init` at +4, `Shutdown` +8, `WriteLevel` +0xc, `ReadLevel` +0x10, `ClientConnect` +0x18 to `RunFrame` +0x30, `ConsoleCommand` +0x34; `gentities`, `gentitySize` and `num_entities` at +0x38 to +0x40 | The server drives the table through `gvm`: a dispatcher turns the engine's game calls into table calls |
| `Init` | Map name, `spawntarget`, `sv_mapChecksum`, the entity string, the level time, `com_frameTime` as the seed, the current milliseconds, how the level began (saved game, autosave) and whether it is a hub transition | The same; the last two stay "neither" until the savegame and transition slices |
| Spawn | Level time 1000, `Init`, three 100 ms frames, baselines, `ClientConnect(0, 0, 0)` for a connected client, one more frame (`SV_SpawnServer` `0x4259c0`) | The shared spawn sequence, which has this shape |
| Error levels | 0 fatal, 1 drop (after writing the `current` save), 2 disconnect, 3 missing CD (`Com_Error` `0x411170`) | Mapped by meaning; fnTech3 numbers its levels differently |
| Cvars | The game keeps the engine's `cvar_t` pointers and reads `string`, `value` and `integer` (+0x4, +0x1c, +0x20) | fnTech3's `cvar_t` keeps those offsets on the only host, so its pointers are passed as they are |
| Pass entity | Only `ENTITYNUM_NONE` (1023) means none; any other number is looked up without a bounds check, so the game's -1 names a record before `g_entities` that nothing matches (`SV_ClipMoveToEntities` `0x428ff0`) | A pass entity outside the array is `ENTITYNUM_NONE` |
| `WriteCam` | Appends to `maps/<map>_cam.map`, opened on first use, and prints its path (`0x411110`) | The same |
| `S_Override` | A 1,024-entry per-entity voice volume that the sound system sets for lip-sync and the game reads (`0x4214cc`) | An engine array, zero until the lip-sync slice |
| Savegame imports | Write and read the game's chunks of a save | Not yet: a save is neither written nor read |
| Memory | `Malloc` is zero-filled zone memory; `Free` releases it | The same |

## Records

The game's `gentity_t` is 1,056 bytes (`GetGameAPI` in `efgamex86.dll`,
`0x200691c0`, publishes version 6 and a `gentitySize` of 0x420), of which the
engine reads a 332-byte prefix:

| Field | Offset | Retail reader |
| --- | --- | --- |
| `s` (entity state, 232 bytes) | 0 | Baselines copy 0xe8 bytes per entity (`0x425910`) |
| `client` (player state pointer) | 0xe8 | `status` follows it to the score (`0x424280`) |
| `inuse`, `linked` | 0xec, 0xf0 | Baselines are made only for entities with both set |
| `svFlags` | 0xf4 | Snapshots test no-client, broadcast and portal there (`0x428653`) |
| `owner` (gentity pointer) | 0x148 | `SV_ClipMoveToEntities` |

The player state is 544 bytes; `playersave` (`0x423ea0`) reads its weapon
(+0x94), weapon state (+0x98), view angles (+0x9c), stats (+0xbc), ammo
(+0x17c) and Borg adaptation counts (+0x18c), and `status` its persistent
score (+0xfc).

## The server's view

The server reaches every module's entities through one view, which reads and
writes them field by field. For single player it binds the game's whole fixed
`g_entities` array of 1,024 records and refreshes the count after every call
into the game, which spawns entities as it runs. The owner pointer reads as an
entity number (a null pointer, or one naming no gentity, as none), and a
write-back of an unchanged number leaves the pointer alone. The record has no
link count or single-client field: the engine keeps its own link counts and
sends every entity to the one client. Each player state is reached through its
gentity's client pointer. Of the server flags, the engine reads only no-client,
bot, broadcast and portal; the NPC, usable, trimodel and other bits are the
game's.

## Results

A developer dedicated server with the `stvef-sp` profile ran each of the 63
single-player maps in the retail paks for ten seconds, with no client, and then
asked the game for its `game_memory` report. All 63 initialise and run without
error: the game reports between 1,127,424 and 1,372,032 of its 2,097,152 bytes
in use, and parses its NPC, bolt-on and ICARUS data as retail does. On `borg1`,
200 frames in, the game's `entitylist` shows 22 spawned NPCs among its movers
and scripts; on `_brig`, its NPC spawners, a spawned NPC, doors, a script
runner and navigation goals. The installation was left unchanged.

Two retail rules came out of these runs. Without the cgame attached before
`Init`, `borg1` crashed on its first world sound, which the game starts through
the cgame's own syscall. And maps whose scripts trace with a pass entity of -1
(three holodeck maps, `dn1` and `dn6` among them) stopped until -1 was taken as
no entity.

## Tests

- `stvef_sp_host_tests` round-trips every single-player entity, player and
  command field through the engine's state and refuses values the narrow
  fields cannot hold; drives the view over a 1,056-byte gentity arena (owner
  pointers, foreign pointers left intact, engine link counts, the player state
  behind the client pointer, one client); and pins the 42 imports, the
  attachment before `Init` and the version check.
- `fntech3_stvef_abi_layouts` pins the import and export table offsets
  retail fills and calls.

## Remaining

The client: the cgame half with its imports, single-player snapshots on the
loopback wire and the menus. Then savegames, level transitions and
autosaves, Bink movies, lip-sync and ambient sound sets.
