# Alice profile, installation identity and startup contracts

## Scope

American McGee's Alice is a seventh title and an eighth profile, `alice`
(aliases `americanmcgeesalice`, `mcgee-alice`, `alice2000`). It is the first
supported title that is not an id Tech 3 derivative in the Quake III line:
Alice runs Ritual Entertainment's Heavy Metal: F.A.K.K.2 engine, which forked
from Quake III before the shipping id Tech 3 contracts were fixed. It has one
mode, so the title key names the profile directly and no mode choice is asked.

This first slice registers the title, its installation identity and its startup
contracts. The profile is experimental. Default builds reject it;
`-Dalice-development=true` lets the shared client and dedicated server activate
it for isolated probes. No Alice module, wire, savegame, asset or renderer
boundary exists yet, so no gameplay or interoperability claim follows from this
slice. Later slices are linked from the
[compatibility ledger](COMPATIBILITY.md#alice-gate-matrix).

## Evidence and provenance

Installed observations use a legitimate retail installation at `E:\Games\Alice`
(the November 2000 SafeDisc release: `drvmgt.dll` and `secdrv.sys` sit beside
`alice.exe`). No retail module, asset or payload is incorporated.

| File | Bytes | Linked | SHA-256 |
| --- | --- | --- | --- |
| `alice.exe` (engine) | 2,209,835 | 2000-11-29 | `8aa22917b7707f0e1a7c4dbe9a3c38332b1873ae2059174ac5329de26921fc18` |
| `base/fgamex86.dll` (game) | 2,084,930 | 2000-11-14 | `ae5a572728fbb2cbf8935d633ef9e23f023edad986d7dd9ee506d589eb261aea` |
| `base/cgamex86.dll` (client game) | 409,666 | 2000-11-14 | `fa70feb1be9883511cf46e1f5556e5c9a2bc52b6b6eecd31a28fa228fa3b443c` |
| `base/pak0.pk3` (assets and `default.cfg`) | 289,276,327 | 2000-11-14 | `6a63b59cd627902660bd4261461f904bf984e05891eb749d47bcc7fc8cedda62` |

Every numeric constant below was read from `alice.exe` itself rather than
inferred. The literal string pool is not encrypted: SafeDisc wraps the entry
point but leaves `.text` and `.rdata` readable, so the cvar registrations, the
format strings and the bounds checks are all directly observable. The method
used was to locate a literal's virtual address, find the `push imm32` sites
that reference it, and decode the instructions immediately around them. Each
constant below is cited with the address it was read at; a cvar's address is
its `Cvar_Get` call site, a bound's is the guard that precedes the error it
raises, and a literal's is its address in the data section.

A second reference is [OpenAlice](https://github.com/themuffinator/OpenAlice) at
`E:\Repositories\OpenAlice`, an independent reconstruction of this engine with a
checked-in Ghidra corpus for `alice.exe`, `fgamex86.dll` and `cgamex86.dll`.
Only its corpus was consulted, as reference observation; no implementation text
is incorporated, and OpenAlice is GPLv3 while fnTech3 is GPLv2, so nothing from
it may be copied into this tree in any case. Where OpenAlice and the retail
binary disagree, the retail binary is authoritative: OpenAlice still carries
Quake III's protocol 68/71, which the measurement below contradicts.

## Engine lineage

Five independent observations place Alice in the F.A.K.K.2 line rather than the
Quake III line, which is why this title needs its own contracts rather than a
variation on an existing profile's:

- **The master server is Ritual's.** `sv_master1` is registered with the default
  `fakkmaster.ritual.com` at call site `0x004557bc`.
- **The maps are `FAKK`, not `IBSP`.** All thirty-six carry ident `FAKK` and
  version 42, with a header checksum Quake III's does not have. See
  [the map format](#map-format) below.
- **The models are TIKI.** `TIKI_LoadAnim`, `TIKI_LoadSkel` and
  `TIKI_LoadAnim: %s has incompatible version %i (supported %d - %d)` are engine
  strings; `pak0.pk3` carries 801 `.tik` definitions, 1,402 `.ska` and 694
  `.tan` animations and 79 `.skb` skeletal bases, and no `.md3` at all.
- **The menus are the engine's own.** Alice loads `.urc` resource files and
  `.ritualfont` fonts and has no UI module; `UIButtonBase`, `UIListBase` and
  `UIAlice3D` are engine classes.
- **The sound system is Miles, not Quake III's.** `s_milesdriver` defaults to
  `auto` at call site `0x0042cb19`, with `s_reverb` and `s_speaker_type` beside it.

Both retail modules export Ritual's `str` class alongside their entry point, so
engine and modules share a C++ string implementation across the library
boundary. That is a property of the shipped ABI, and any module boundary this
title eventually gets has to reproduce it.

## Startup contracts

| Contract | Value | Where it was read |
| --- | --- | --- |
| Base game directory | `base` | the filesystem string pool at `0x00512af8`, beside `fs_game` and `.pk3`; confirmed by the installation |
| Base pak | `pak0.pk3` | the installation |
| Protocol | 1 | `Cvar_Get( "protocol", va( "%i", 1 ), CVAR_SERVERINFO \| CVAR_ROM )`, whose name push is at `0x00455624`, and the same `va( "%i", 1 )` written into the connect userinfo at push site `0x0042b8ce` |
| Demo extension | `.dm3`, fixed | `demos/%s.dm3` |
| Client configuration | `config.cfg` | the engine's own literal; `default.cfg`, `autoexec.cfg`, `localized.cfg` and `menu.cfg` are the other four it executes |
| Game module | `base/fgamex86.dll`, exporting `GetGameAPI` | the module's export table; `Sys_GetGameAPI without Sys_UnloadingGame` is the engine's own error |
| Client game module | `base/cgamex86.dll`, exporting `GetCGameAPI` | the module's export table; `GetProcAddress failed on GetCGameAPI` is the engine's own error |
| Menu module | none | no third library ships, and the menus are `.urc` resources |
| `sv_maxclients` | default 8, clamped to at most 128 | registered at call site `0x00454e0c`; the `cmp 0x80` clamp follows at `0x00454e34` |
| `MAX_CONFIGSTRINGS` | 1,328 | the `cmp 0x530` guarding the `CL_ConfigString: bad index: %i` raise at `0x0042b652`, and two more guarding the `configstring > MAX_CONFIGSTRINGS` raises at `0x00423fce` and `0x0042dfec` |
| `MAX_SUBMODELS` | 512 | the `cmp 0x200` guarding the `MAX_SUBMODELS exceeded` raise at `0x0042fd62` |

*Correction, 2026-09-24:* `sv_maxclients` defaults to 1, not 8. Retail's
`SV_Init` registers it with `"1"` and flags `0x24`, serverinfo and latched
(pushes at `0x0045566b..0x00455672`, `Cvar_Get` at `0x0045567c`); the `"8"`
row above is the call at `0x00454e0c`, which the profile's source names
`SV_BoundMaxClients`. Two things here are inferences rather than readings:
that `SV_Init`'s registration comes first, which is what the profile's
source states, and that the Quake III `Cvar_Get` this lineage keeps leaves an
existing cvar's value alone, so that the `"8"` only applies to a cvar nothing
registered before it. The 128 clamp stands. The profile carries 1
(`code/qcommon/game_profile.cpp`).

Three differences from Quake III's startup are worth naming because they will
shape later slices rather than this one:

- **The filesystem cvars are not Quake III's.** Alice registers `fs_basepath`,
  `fs_cdpath`, `fs_filedir` and `mapdir`. There is no `fs_homepath` and no
  `fs_basegame`; `fs_game` exists as a string but is not registered anywhere the
  scan found. fnTech3's own writable-home layout therefore has no retail
  counterpart to preserve here, and `config_server.cfg` is fnTech3's name for
  the dedicated-server configuration, as it is for every other profile.
- **There is no bot library.** No `bot_enable`, no `bot_minplayers`, no `.aas`
  navigation and no `botfiles` in any retail pak. The profile's bot policy is
  therefore off with a single storage slot, not Quake's.
- **The player is the game's own character.** `name` is registered as
  `UnnamedPlayer` at call site `0x0042caad`, which is fnTech3's own engine
  default, and the installation's `default.cfg` then `seta`s it to `Player`.
  No model, head or colour userinfo key is published at all, so the profile
  claims none.

`sv_pure` exists as a string at `0x0051ac44` but no registration for it was
found by the scan, and the profile claims no pure proof until that is settled.
The configstring index the players start at is likewise unpinned; it is a module
contract rather than an engine one, and it is left at zero rather than guessed.

## Installation identity

`base/pak0.pk3` names the installation. It carries `default.cfg` at its root,
1,322 bytes with CRC `3781148b`, and `maps/garden1.bsp` at 6,323,368 bytes with
CRC `cb85fdae`. `garden1` is the first level the campaign reaches and appears in
no other supported title's paks, which is what the shared recognizer requires of
a companion file.

The installation mounts more than one pak, and two of the groups are
alternatives rather than layers:

| Pak | Entries | Role |
| --- | --- | --- |
| `pak0.pk3` | 7,723 | maps, models, sounds, scripts, `default.cfg` |
| `pak1_small.pk3` / `pak1_large.pk3` | 3,427 / 3,368 | the two texture resolutions; one or the other |
| `Pak2.pk3`, `Pak3.pk3` | 71, 9 | the two patch paks, upper-cased as they ship |
| `pak4_english.pk3` and four siblings | 44 each | the localized voice and text sets, one per language |
| `pak5_dutch.pk3` | 44 | the Dutch set |

Their naming only half follows Quake's `pak0`-`pak8`. The profile claims no
stem of its own, which leaves the shared refusal on Quake's `pak` stem, and
that comparison is case-insensitive: `pak0`, `Pak2` and `Pak3` are refused for
download, and `pak1_small`, `pak1_large`, the four `pak4_*` sets and
`pak5_dutch` are not, because their suffixes are not what the numbered names
match. Naming a stem would not fix that; the suffixes need their own rule.
Which of `pak1_small` and `pak1_large` a given installation mounts, and how the
upper-cased `Pak2`/`Pak3` sort against the lower-cased names on a case-sensitive
filesystem, are open filesystem questions too. All of it is for the filesystem
slice, not settled here.

## Map format

Every retail map is ident `FAKK`, version 42. The header is not Quake III's:
after the ident and version comes a 32-bit checksum, and only then the lump
directory, so the first lump's offset is 172 rather than 144. There are twenty
lumps, in F.A.K.K.2's own order rather than Quake III's, and three of them —
side equations, light definitions and terrain — have no IBSP 46 counterpart at
all. `maps/centipede1.bsp` reads as:

| Lump | Offset | Bytes |
| --- | --- | --- |
| 0 | 172 | 5,396 |
| 1 | 5,568 | 190,752 |
| 2 | 196,320 | 835,584 |
| 3 | 1,031,904 | 381,564 |
| 4 | 1,413,468 | 797,368 |
| 5 | 2,210,836 | 129,216 |
| 6 | 2,340,052 | 70,200 |
| 7 | 2,410,252 | 41,660 |
| 8 | 2,451,912 | 269,280 |
| 9 | 2,721,192 | 201,492 |
| 10 | 2,922,684 | 97,544 |
| 11 | 3,020,228 | 13,788 |
| 12 | 3,034,016 | 72 |
| 13 | 3,034,088 | 480 |
| 14 | 3,034,568 | 38,830 |
| 15 | 3,073,400 | 934,656 |
| 16 | 4,008,056 | 145,600 |
| 17 | 4,153,656 | 6,240 |
| 18 | 4,159,896 | 219,384 |
| 19 | 4,379,280 | 183,716 |

The directory is contiguous and ends exactly at the file's 4,562,996 bytes,
which is what establishes the lump count. Which lump is which is **not** claimed
here: the offsets and sizes are measured, the names are not, and assigning them
is the first job of the map slice. The format identity
`COM_GAME_BSP_FAKK` is registered so that the collision loader refuses a
`FAKK` 42 map by name rather than misreading it as an `IBSP` header, and so that
no other profile accepts version 42.

The ident alone is not sufficient to identify the format: F.A.K.K.2's own maps
are `FAKK` 12 and Medal of Honor: Allied Assault's are the same ident again at
other versions. The profile therefore admits version 42 and nothing else.

## Asset inventory

Across `pak0.pk3`, the two `pak1` variants and the patch paks, the installed
formats this title will need are:

| Format | Count | What it is |
| --- | --- | --- |
| `.tik` | 801 | TIKI model definitions |
| `.ska` | 1,402 | skeletal animations |
| `.tan` | 694 | animations |
| `.skb` | 79 | skeletal bases |
| `.cik` | 797 | cinematic definitions |
| `.cam` | 396 | camera paths |
| `.pth` | 35 | motion paths |
| `.lip` | 366 | lip-sync tracks |
| `.scr` | 73 | Ritual script sources |
| `.snd` | 36 | sound alias sets |
| `.urc` | 12 | UI resources |
| `.ritualfont` | 14 | fonts |
| `.shader` | 52 | shader scripts |
| `.bsp` | 36 | maps |

Not one of these readers exists in fnTech3. `.wav` (1,896) and `.tga` (284) are
the only two formats in the list the engine already reads.

## Savegames

Alice has a single-player save system with engine-side archiving:
`SV_ArchiveLevelFile()`, `SV_ArchivePersistantFile()`, `SV_ArchiveServerFile(%s)`
and `Com_WipeSaveGame(%s)` are its own trace strings, and `savegame`,
`autosavegame` and "Selects a particular savegame slot" are its own commands.
It refuses to save in deathmatch and while dead. None of this is implemented;
it is recorded so the savegame gate has a shape.

## What this slice does not claim

No module is loaded, no packet is encoded, no map is read past its header, no
asset format is parsed, and nothing is drawn or heard. The profile is registered
and gated, and the numbers above are measured. That is the whole of it.
