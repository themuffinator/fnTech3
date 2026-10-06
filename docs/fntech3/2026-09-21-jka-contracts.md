# Jedi Academy profiles, installation identity and startup contracts

## Scope

Star Wars Jedi Knight: Jedi Academy is a sixth title with two mode profiles, as
Return to Castle Wolfenstein and Elite Force are: `jka-sp` (aliases `jasp`,
`jediacademy-sp`, `jedi-academy-sp`) for the single-player campaign and `jka-mp`
(aliases `jamp`, `jediacademy-mp`, `jedi-academy-mp`) for multiplayer. Both run
from one `base/` installation and one `fntech3.exe`; the bare title key `jka`
asks for a mode. This first slice registers the title, its installation
identity and its startup contracts.

Both profiles are experimental. Default builds reject them; `-Djka-development=true`
lets the shared client and dedicated server activate them for isolated probes.
No Jedi Academy module, wire, savegame, asset or renderer boundary exists yet,
so no gameplay or interoperability claim follows from this slice. Later slices
are linked from the [compatibility ledger](COMPATIBILITY.md#jedi-academy-gate-matrix).

## Evidence and provenance

Installed observations use a legitimate retail installation at
`E:\Games\Jedi Academy` (Jedi Academy 1.01: `version.inf` reports
`SPExtVersion=1.01`, `MPExtVersion=1.01`, `IntVersion=6.0`). No retail module,
asset or payload is incorporated.

| File | Bytes | Linked | SHA-256 |
| --- | --- | --- | --- |
| `jasp.exe` (single-player engine) | 1,236,992 | 2003-10-24 | `9351a8de230fcd577e2b474889d08f38982450306767a6823dd659482566d6ce` |
| `jamp.exe` (multiplayer engine) | 1,527,808 | 2003-11-06 | `e8062d3948ff05917b48e47d8eec0ee43bcc6fb03924410712412404ddfee28b` |
| `jagamex86.dll` (single-player game and cgame) | 1,802,240 | 2003-10-24 | `6b87b804137f56f3bb8901235788c735e6c36cf1bea42cab48dad7f2d15b6ee3` |

Three observations from that installation correct what the sources alone would
have suggested:

- **The single-player module sits in the installation root**, beside
  `jasp.exe`, not in `base/`. It exports `GetGameAPI`, `dllEntry` and `vmMain`,
  which is exactly the shape Elite Force's single player uses: a function table
  for the game and the client game's entry points from the same library.
- **The multiplayer modules ship inside the paks.** `base/` carries no loose
  module at all: `assets2.pk3` holds `jampgamex86.dll`, `cgamex86.dll` and
  `uix86.dll`, and `assets3.pk3`, the 1.01 patch, holds newer copies of all
  three. Each exports `dllEntry` and `vmMain` and nothing else, which is Quake
  III's native shape. No module was loaded to read this; only the export tables
  were parsed.
- **Both configurations live in `base/assets2.pk3`**, not beside the maps, so
  that pak names the installation: `default.cfg` at 2,178 bytes with CRC
  `fb11a25b` and `mpdefault.cfg` at 2,845 bytes with CRC `4999454a`. The maps
  are in `assets0.pk3` and `assets3.pk3`.

The installation also writes `base/jaconfig.cfg`, confirming the single-player
configuration name from the game itself.

The source reference is [OpenJK](https://github.com/JACoders/OpenJK/tree/1a6a643427aa347553e9073dac5570b33337c4d9)
at `1a6a643427aa347553e9073dac5570b33337c4d9` (2026-07-11, GPL-2.0-or-later),
which carries Raven Software's released Jedi Academy sources: `code/` is the
single-player engine and game, `codemp/` the multiplayer engine and modules.
This is reference observation only; no implementation text is incorporated, and
the citations below are file and line references into that tree.

OpenJK is a maintained port, not the retail engine, and it renames or retunes
some engine-owned values (its window title, home path and configuration names,
for instance). Facts taken from it, where the installation does not settle them
itself, are therefore split into two kinds:

- **Format and layout facts** -- protocol numbers, module entry points and
  names, configstring layouts, limits, BSP identity. These are contract-bound:
  a port that changed them could not load Raven's own modules or maps, so the
  reference is strong evidence for the retail contract.
- **Engine default values** -- `sv_pure`, `sv_maxclients`, `fraglimit` and the
  userinfo defaults. A port can change these without breaking anything, so each
  one is recorded with its source line and stays open until a retail
  observation confirms it.

Where OpenJK marks a retail-compatibility branch, fnTech3 follows the retail
side. `BASE_SAVE_COMPAT` (`code/qcommon/q_shared.h:91`) is such a branch: it is
defined by default and reserves the 96 configstrings a retail single-player
build carries, so fnTech3 uses the reserved layout.

## Contracts established

| Contract | `jka-sp` | `jka-mp` |
| --- | --- | --- |
| Base directory | `base`, beside the executables (installed) | Same directory and assets |
| Installation identity | `base/assets2.pk3`: `default.cfg` 2,178 bytes, CRC `fb11a25b`; `mpdefault.cfg` 2,845 bytes, CRC `4999454a` (installed) | Same pak and identity |
| Configuration | `jaconfig.cfg`, written by the installed game, fnTech3 dedicated `jaconfig_server.cfg` | `jampconfig.cfg` (`codemp/qcommon/files.cpp:4093`), `jampconfig_server.cfg` |
| Protocol | 40, loopback only (`code/qcommon/qcommon.h:199`) | 26 (`codemp/qcommon/qcommon.h:214`) |
| Module ABI | A `GetGameAPI` function table, with the client game's `dllEntry`/`vmMain` taken from the same loaded library; all three exports observed in the installed module; game API version 10 (`code/game/g_public.h:28`) | Quake III's `dllEntry`/`vmMain` native shape, both observed in all six installed modules; game API 1, cgame 2, UI 3 (`codemp/game/g_public.h:32`, `codemp/cgame/cg_public.h:26`, `codemp/ui/ui_public.h:26`) |
| Native module names | `jagamex86.dll` for both the game and the client game; no menu module of its own (installed) | `jampgamex86.dll`, `cgamex86.dll`, `uix86.dll` (installed, and `codemp/server/sv_gameapi.cpp:2806`, `codemp/client/cl_cgameapi.cpp:1702`, `codemp/client/cl_uiapi.cpp:1277`) |
| Module location | The installation root, beside `jasp.exe` (installed) | Inside `base/assets2.pk3` and `assets3.pk3`, so the filesystem serves them like any other pak member (installed) |
| QVM | None. The installation's paks carry no QVM at all, and `codemp/qcommon/` has no VM back end to run one | Same |
| Pure proof | Unavailable (no remote play) | The native `cgamex86.dll` and `uix86.dll`, which is what its own reference flags name (`codemp/qcommon/files.cpp:1427`) |
| BSP | `RBSP` version 1 with 18 lumps, including the light-array lump; all 57 installed maps carry that identity (`codemp/qcommon/qfiles.h:307`-`373`) | Same |
| Clients | 1 (`code/qcommon/q_shared.h:618`) | 32 (`codemp/qcommon/q_shared.h:890`) |
| Configstrings | 1,300 with players at 743 (`code/qcommon/q_shared.h:663`, `:683`-`:697`) | 1,700 with players at 1,131 (`codemp/qcommon/q_shared.h:937`, `codemp/game/bg_public.h:125`-`:137`) |
| Collision handles | 512 submodels, the box and capsule handles at the end (`code/qcommon/q_shared.h:642`) | 512 (`codemp/qcommon/cm_local.h:30`) |
| Entity hulls | Caller-supplied trace shape, as Quake III uses | Same (`codemp/server/sv_world.cpp:818`) |
| Steam | None recorded. Jedi Academy is sold on Steam, but the installation observed here is not a Steam one, so discovery reports no path rather than guessing an AppID or folder |  Same |
| Window icon source | `jasp.exe` | `jamp.exe` |
| Server defaults | No game type, one client slot, no frag limit | `g_gametype 0`, `sv_maxclients 8`, `fraglimit 20` in the server info, `timelimit 0` (`codemp/server/sv_init.cpp:932`-`:953`) |
| Pure default | Not applicable | `sv_pure 0`, system info, not latched (`codemp/server/sv_init.cpp:970`) |
| Bots | None: single player runs the game module's own AI and no bot library | Enabled, one navigation world, 32 client and 1,024 entity storage slots, bot rate 16,384 with no snapshot interval (`codemp/server/sv_bot.cpp:652`, `:189`) |
| Userinfo defaults | `name Jaden` (`code/client/cl_main.cpp:1290`); no model key -- the player is chosen from the menus and carried in the savegame | `name Padawan`, `model kyle/default`, saber colours `color1`/`color2` at 4 (`codemp/client/cl_main.cpp:2839`-`:2847`, `codemp/game/bg_public.h:45`) |
| Boot cinematic | None registered by the engine | None |

## One pak names the installation

Every title in the catalog is recognized by a pak's `default.cfg` and one
companion file, each pinned to a size and checksum observed in a legitimate
installation. Jedi Academy keeps its configuration and its maps in different
paks, so its companion is the multiplayer configuration beside it:
`base/assets2.pk3` must hold exactly one `default.cfg` of 2,178 bytes with CRC
`fb11a25b` and exactly one `mpdefault.cfg` of 2,845 bytes with CRC `4999454a`,
and no other title's companion name.

One installation carries both modes, so that pak sets both marks and automatic
detection reports that a mode must be chosen, exactly as an RTCW or Elite Force
installation with both modes does. The modules beside the pak are not
consulted for identity: `cgamex86.dll` and `uix86.dll` are names Quake III and
Quake Live installations carry too, and mistaking one of those for this title
would select the wrong profile before the filesystem starts.

The retail installation is recognized by this rule, and reports that a mode
must be chosen, in `tests/jka_identity_tests.cpp` when it is given the
installation directory.

## Protocol numbers shared with another title

Jedi Academy's two protocol numbers are both already spoken by Elite Force:
single player uses 40 as Elite Force's does, and multiplayer's 26 is the number
ioEF and Lilium Voyager use. A protocol version therefore no longer names one
family: `Com_ProtocolFamilyForVersion` returns every family that uses the
number, and each profile admits only the family it was registered with. A
`jka-mp` profile accepts 26 and refuses 40; `stvef-sp` accepts 40 and neither
accepts the other title's module or wire contract.

Admitting a number is not yet a connection. Until 2026-09-26 the server and
client took the contract `fnql::protocol::Find` chose for the number without
asking whose it was, so a `jka-mp` server would have served a protocol-26
client under ioEF's contract, and a `jk2-mp` one a protocol-15 client under
Quake III's legacy fallback. Each contract now names the profile families it
serves (`code/qcommon/protocol_contract_profile.hpp`), `SV_DirectConnect` and
the client's `connectResponse` refuse one that is not the active profile's,
and `tests/game_profile_tests.cpp` checks that no contract serves either Jedi
title. Neither title's network client connects until its own codec exists.

## Deliberately not claimed

This slice claims none of the following. Later slices on the same day extended
some of them; each says exactly how far it goes.

- No Steam AppID, install directory or discovery behavior.
- No retail pak identity, file count or asset inventory.
- No wire codec. Protocol 26 and 40 are registered numbers here, not encoders:
  no message body, delta table or demo format has been written or tested.
- No module ABI. This slice registers only the module file names and
  entry-point shapes above; multiplayer's call numbers arrived with the
  [module record](2026-09-21-jka-module-abi.md), which dispatches none of them,
  and single player's function table is still unwritten.
- No asset support. Maps, models and motion paths arrived with the
  [map](2026-09-21-jka-maps.md), [model](2026-09-21-jka-models.md) and
  [motion](2026-09-21-jka-roff.md) records, which read them and nothing more;
  ICARUS scripts, the `.sav` savegame format and Raven's shader extensions are
  all unimplemented.
- No renderer, audio or input contract.

## Known gaps this slice leaves open

- Jedi Academy numbers its game types differently from Quake III: free-for-all
  is 0, but single player is 5 and team deathmatch 6 (`codemp/game/bg_public.h:234`).
  fnTech3's inherited `spmap` command sets Quake III's single-player number,
  which under a Jedi Academy profile would select Jedi Master. Retail
  multiplayer has no `spmap` command at all. The server slice owns this; the
  profile stays runtime-blocked until then. (Corrected by the
  [Jedi listen-server record](2026-10-06-jedi-listen-server-game-types.md):
  retail `jamp.exe` 1.01 registers `spmap`, which selects 5, and fnTech3's
  now does the same.)
- `sv_pure`, `sv_maxclients`, `fraglimit` and the userinfo defaults are
  OpenJK-sourced engine defaults, listed above with their lines, and remain
  open until a retail observation confirms each one.
- Multiplayer's delta encoding is not a fixed table: a mod may retune any
  field's bit width at runtime by shipping `netf_overrides.txt` or
  `psf_overrides.txt`, which the engine reads by field name
  (`codemp/qcommon/msg.cpp:1889`). The installation ships both, at
  `ext_data/MP/` inside `assets1.pk3`, so a wire codec for this title has to
  carry that mechanism, not just Raven's default widths.
- **Those shipped files are not the delta tables**, and a codec built from
  them would be wrong. They are name-keyed overrides, so their order means
  nothing to the engine, and their content has drifted from it: the installed
  `netf_overrides.txt` lists 130 entity fields where the engine's table has
  132, `psf_overrides.txt` lists 143 where the table has 152, both list them
  in a different order, and several widths disagree -- the entity `weapon`
  field is 8 bits in the table and 32 in the file. The table order is
  wire-visible, because a delta sends one bit per field in table order, so the
  wire slice has to take its table from the engine contract and treat the
  shipped files only as the override mechanism's input.
- Single player's `sex`, `snd` and `handicap` userinfo cvars
  (`code/client/cl_main.cpp:1296`-`:1301`) are savegame-carried and belong to
  the single-player client slice, not to the appearance policy.
