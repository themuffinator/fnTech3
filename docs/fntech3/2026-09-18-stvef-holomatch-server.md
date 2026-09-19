# Elite Force Holomatch dedicated server: retail game module and bots

## Scope

This slice lets the `stvef-mp` dedicated server host the retail Holomatch game
module, `vm/qagame.qvm` from `pak2.pk3` (1.20), with the retail bots. It adds
the game import boundary, the module's record layouts, Elite Force's server
flags and shield boxes, the float conversion its QVMs expect, the Quake III
1.17 bot contract and the botlib differences retail relies on, the Expansion
Pack modes the retail engine owns, and the server side of protocol-24 message
framing (gamestate, snapshot acknowledgement, client messages, downloads, pure
checksums, info and status replies, master heartbeats).

The client, the cgame and UI modules, the renderer, `.efdemo` playback and a
session with a retail client are later slices. Both profiles stay behind
`-Dstvef-development=true`.

## Evidence and provenance

Retail facts come from `stvoyHM.exe` 1.20 (identified in the
[foundation record](2026-09-18-stvef-contracts.md)), read through a local
decompilation and checked against its instructions, and from the retail QVMs in
the installation. [Lilium Voyager](https://github.com/zturtleman/lilium-voyager/tree/0f7dcd89d142c8b635ce2756a80cb62411731848)
was used as a cross-check; where it differs from retail, retail is followed.

| Contract | Retail source | fnTech3 |
| --- | --- | --- |
| Game imports | Game dispatcher at `0x431030`: 0-40, 100-111, 200-211, 303-318, 400-427, 500-572 | 0-40, the math traps, 200-211, 303-318 and 500-572 keep Quake III 1.32's contract; 300-302 are refused as retail refuses them; 400-427 (Quake III 1.17 order, with 427 alternate attack) and the calls below are decoded for Elite Force |
| Records | Copies of 204-byte entity state and 468-byte player state; `r` addressed at 204 | Frozen `sharedEntity_t` (304 bytes) and player state with its adapter; `trap_GetUsercmd` and bot commands use the 24-byte Elite Force `usercmd_t` |
| Float to int | JIT emits `fld`/`fistp` in the default rounding mode (`0x43e1fd`) | Elite Force QVMs round `OP_CVFI` to nearest, ties to even, in the interpreter and the x86 compilers; other compilers fall back to the interpreter |
| Server flags | `SVF_SHIELD_BBOX` 0x2, `SVF_ELIMINATED` 0x10 | 0x2 maps to an engine-internal shield flag (Quake III's 0x2 is its client mask); 0x10 stays with the module |
| Shield boxes | `SV_LinkEntity` `0x436550` | Tested before contents: a 4-unit X extent means a Y-facing shield; `s.solid` carries the wide axis, height capped at 254; `EF_SHIELD_BOX_X`/`_Y` set in `eFlags` |
| Bot actions | qagame `BotInputToUserCommand` tests 1.17 bits; engine slot `ea+0x6c` for 427 | The botlib keeps its flags; `trap_EA_GetInput` translates to the 1.17 order, with bit 13 alternate attack. Moving up/down are jump/crouch and the previous-jump marker is MOVELEFT, as for RTCW |
| Bot chat | 2-argument `BotSetChatName`; no reply chat | Chat states speak as the client the game names, with no private messages; no `rchat.c` is loaded |
| Bot entities | botlib entity types with `ET_ALT_MISSILE` before movers | Movers are type 5 |
| Bot weapons | `0x48feb0`: third argument weighs slots 11 and 23 at 300 (`0x4bdaa4` = `0x43960000`); `0x48f800` rejects bad slots silently | Implemented as retail does |
| Skill | `BotLoadCharacter` takes an integer skill | Converted to the botlib's float |
| Perpendicular vector | Game trap 109 at `0x4263e0` scans z, y, x | Same scan order |
| Trace defaults | `CM_BoxTrace` `0x418960` presets only `fraction` = 1; a trace that hits nothing reports no surface flags | Same. Lilium presets `SURF_NOIMPACT`, which retail does not |
| Expansion Pack modes | `g_pMod*` registered latched serverinfo; conflicts resolved at `0x42e160`; `map_restart` reloads when one changed; `spmap` clears them; `vv 1` in `getinfo` (`0x4349ff`) | Same rules, in retail's order |
| Gamestate | `SV_SendGameState` `0x42fc30` | Raw bits, no leading acknowledgement, zero-byte terminator, checksum feed, no client number |
| Snapshots | `SV_SendClientSnapshot` `0x435550` | Reliable acknowledgement after `svc_snapshot`; no `svc_EOF` |
| Client messages | `SV_ExecuteClientMessage` `0x430930` | Raw bits; the message ends where its data ends |
| Pure checksums | `cp` with no server id | `cp cgame ui @ refs... mixed` accepted; QVM proof for both modules |
| Downloads | `0x430060`: 2 KB blocks, window 8; error is `-1` then an empty chunk | Same sizes and error form; blocks travel after an empty snapshot, as Lilium's server sends them, so retail clients see their acknowledgements |
| `connect`, `getinfo`, `getstatus` | Plain-text `connect`; `infoResponse "<info>"`; `protocol` in status | Same |
| Heartbeat | `\heartbeat\<port>\gamename\STEF1\` | Same; masters default to 27953 and none is configured by default |

## Dedicated-server observations

Each run used the retail installation read-only and an empty temporary home
directory, with no master servers:

```powershell
fntech3.ded.exe +set logfile 2 +set com_gameProfile stvef-mp +set fs_basepath "<Elite Force folder>" +set fs_homepath "<empty test home>" +set dedicated 1 +set sv_master1 "" +set sv_pure 0 +set bot_enable 1 +map hm_borg1 +addbot Chell 3 +addbot Chakotay 3 +addbot Foster 3 +addbot Gamma 3
```

- `hm_borg1`, free-for-all, four bots, 45 s: the QVM compiled and initialised
  (14 item types, `hm_borg1.aas`, 69 bots and 33 arenas parsed). Bots collected
  48 items, scored 5 kills including alternate fire (`MOD_PHASER_ALT`), and
  chatted. The log held no warning.
- `ctf_voy1`, capture the flag with Specialties, six bots, `timescale 3`: the
  match ran to its capture limit (`Exit: Capturelimit hit.`, blue 3, red 0)
  with 18 kills. Assimilation, requested for this CTF game, was switched off
  before the game started, as retail's rules require.

The first CTF runs stopped with "program tried to bypass data segment bounds":
the retail game passes `MAX_GENTITIES` as the capacity of a smaller list on its
stack to `trap_EntitiesInBox`, which retail never checks. For Elite Force the
capacity is now clamped to the VM data segment; the engine still writes only
the entities it finds.

## Differences and open items

- Bots choose items as retail's botlib does (long-term goals `0x48ec70`,
  nearby goals `0x48efb0`), not as Quake III 1.32's does. The 1.20 game passes
  a last argument to `BotChooseLTGItem` and `BotChooseNBGItem` (535/536):
  when it is set, items that never spawned are considered and every item but
  a roam point weighs `ceil( random() * 10 )` in place of its fuzzy weight.
  Retail also weighs with the undecided fuzzy weights, passes over any goal
  it is still avoiding, marks a chosen goal as an item only, and avoids it for
  half its respawn time (30 seconds under 10, 5 when dropped). Lilium keeps
  the Quake III rules and ignores the argument.
- `sv_maxclients` stays within the game module's 64 client slots; retail allows
  128.
- CD-key authorization and master publication are optional services with no
  synthesized success.
- The client, its modules, the renderer, demos and sessions with other
  engines are in the [client](2026-09-18-stvef-holomatch-client.md),
  [cgame and UI](2026-09-18-stvef-holomatch-cgame-ui.md) and
  [sessions](2026-09-18-stvef-holomatch-sessions.md) records.

## Tests

- `fntech3_stvef_vm_rounding` checks the interpreter's rounding against
  `cvtss2si` for two million floats, and the edge cases.
- `stvef_bot_item_choice_tests` compiles the retail item choice from the
  botlib and checks eligibility, the random weight, avoided goals, the goal
  flag and avoid times, and the nearby-goal limits.
- `fntech3_stvef_server_policy` pins the mode rules, the 1.17 action bits,
  which imports keep Quake III's contract, and the perpendicular vector.
- `fntech3_server_game_data` reads and writes Holomatch records through the
  view; `fntech3_stvef_abi_layouts` pins the bot input record.
- `server_game_flags_tests` round-trips every Holomatch server flag and keeps
  0x4 and 0x10 with the module; `pure_profile_contract_tests` verifies a `cp`
  with no server id.
- The RTCW bot and server probes, which walk every profile, now expect the
  1.17 action contract, the mover type, the mode stubs and the new flag table
  for `stvef-mp`.
