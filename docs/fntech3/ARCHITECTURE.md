# fnTech3 architecture overview

This is the maintainer's map of fnTech3: how one engine carries sixteen game
profiles, where each subsystem lives, and how changes are verified. The rules
every change must keep are in [AGENTS.md](../../AGENTS.md); the evidence and
open gates for each profile are in the [compatibility ledger](COMPATIBILITY.md);
building is described in the [Build Guide](../../BUILD.md). Player-facing
behaviour is described in the [documentation index](../README.md).

## Lineage

fnTech3 merges two GPL id Tech 3 engines into one profile-driven codebase:

- [FnQuake3](https://github.com/themuffinator/FnQ3), the Quake III
  modernization line, pinned for the merger at
  `7d8c9629cdc7979f09697266470359101ffd9eda`;
- [FnQL](https://github.com/themuffinator/FnQL), the Quake Live compatibility
  baseline, pinned at `fce44fe714564621e6ceb79e3da7c53089646af2`.

Both descend from Quake3e, ioquake3 and id Software's Quake III Arena source.
fnTech3's own work is everything since the merger: the profile system, the
adapters for eight more games and the IHV and Q3Test builds, Ritual-engine and 2015-engine
support, and
the extension of the shared renderers and audio to every game. FnQL's design
notes remain in [`docs/fnql/`](../fnql/README.md) where they still describe
the Quake Live profile or a shared subsystem. The full chain and
acknowledgements are in [CREDITS.md](../../CREDITS.md).

fnTech3 is engine-only. It contains no retail module, asset or proprietary
runtime, and no reconstruction of a game module; every profile is exercised
with a user-supplied legitimate installation.

## Profiles

### The catalog

[`code/qcommon/game_profile.cpp`](../../code/qcommon/game_profile.cpp) holds
one catalog entry per profile, and
[`code/qcommon/game_manager.h`](../../code/qcommon/game_manager.h) is the
read-only interface to it. An entry owns everything compatibility-sensitive
about its game or mode: canonical key and aliases, title, base directory and
base pak, preferred and accepted protocols, accepted module interfaces, Steam
identity, installation marker, config file names, pak prefix rules, client and
configstring limits, collision-handle count and entity hull rules, server and
bot policy, pure-proof method, userinfo defaults and startup cinematics.

| Profile | Base directory | Protocol (also accepted) | Module interface | Pure proof |
| --- | --- | --- | --- | --- |
| `quake3` | `baseq3` | 68 (1.29-1.31's 66 and 67, ioquake3 71; demos 43, 45, 46, 48) | QVM, legacy native | QVM checksums |
| `q3-ihv` | `baseq3` | 33 | IHV native, API 4 | unavailable |
| `q3test` | `demoq3` | the pinned release's: 35 (1.02, 1.03), 36 (1.05), 38, 39, 40 (1.06 to 1.08) | Q3Test function tables (`GetGameAPI`, `GetCGameAPI`, `GetUIAPI`), 1.08's cgame `dllEntry`/`vmMain` | unavailable |
| `quakelive` | `baseq3` | 91 | structured native from `bin.pk3` | QL native modules |
| `rtcw-sp` | `main` | 49 (iortcw 50) | legacy native | unavailable |
| `rtcw-mp` | `main` | 60 (iortcw 61) | legacy native, `_mp_` modules | native module pair |
| `wolfet` | `etmain` | 84 | legacy native, `_mp_` modules | native module pair |
| `stvef-sp` | `baseEF` | 40 | Elite Force SP function tables | unavailable |
| `stvef-mp` | `baseEF` | 24 (ioEF 26) | QVM, legacy native | QVM checksums |
| `jka-sp` | `base` | 40 | Jedi Academy SP function table | unavailable |
| `jka-mp` | `base` | 26 | legacy native | native module pair |
| `jk2-sp` | `base` | 40 | Jedi Outcast SP function table | unavailable |
| `jk2-mp` | `base` | 15 | QVM | QVM checksums |
| `alice` | `base` | 1 | Ritual function tables, `fgamex86`/`cgamex86` | unavailable |
| `fakk2` | `fakk` | 1 | Ritual function tables, `gamex86`/`cgamex86` | unavailable |
| `mohaa` | `main` | 6 | 2015's function tables, `gamex86`/`cgamex86` with the installation's `system86.dll` | unavailable |

The same protocol number can belong to several games -- 40 is spoken by three
single-player loopbacks, 26 by Holomatch's successors and Jedi Academy, 1 by
both Ritual games -- so a number maps to protocol *families* and each profile
admits only its own. A connection also keeps the wire contract chosen for its
number only when that contract serves the profile
(`code/qcommon/protocol_contract_profile.hpp`), so a profile with no wire of
its own, as the Jedi titles' are so far, refuses network clients.

### Selection

`Com_InitGameProfile` in [`common.c`](../../code/qcommon/common.c) runs before
the filesystem, network, modules, renderer or any service starts:

1. `com_gameProfile` is read from the command line only. An explicit key or
   alias wins; a title key with more than one mode (`rtcw`, `stvef`, `jka`,
   `jk2`) is refused as *mode required*.
2. With `auto`, [`Com_ProbeGameProfileMarkers`](../../code/qcommon/game_profile.cpp)
   looks in `fs_basepath`, then the executable's directory, then each
   profile's Steam library. A marker is a base pak whose `default.cfg` (or,
   for F.A.K.K.2, `menu.cfg`) and one companion file match a known size and
   CRC; see the [pak identity record](2026-09-08-pak-identity.md). The IHV
   Test, which has no paks, is recognized from its loose files. Two titles in
   one place are ambiguous; two modes of one title require a mode.
3. `Com_ActivateGameProfile` makes the choice permanent for the process. The
   cvar becomes read-only, the window and console titles name the game, and
   any later attempt to activate a different profile is a fatal error.

A profile whose adapters are incomplete carries a `runtimeBlocker` string.
Detection refuses it unless the build defines that game's development macro --
`FNTECH3_RTCW_DEVELOPMENT`, `FNTECH3_WOLFET_DEVELOPMENT` and so on, set by
the Meson options described in the [Build Guide](../../BUILD.md#game-development-builds).
Those options let developers run the game; they never change its contracts or
close a gate.

Changing mode within a title is a relaunch, not a mutation: `gameMode` (and
RTCW's `startSingleplayer`/`startMultiplayer`) saves the current config, shuts
every subsystem down and starts the same executable with the new profile
through [`process_relaunch.cpp`](../../code/platform/process_relaunch.cpp).
The [RTCW contracts record](2026-09-14-rtcw-contracts.md) describes the
handoff and its failure handling.

### Launcher

`fntech3-launcher` ([`code/launcher`](../../code/launcher/launcher_main.cpp))
is a separate program, built beside the client (Meson option `launcher`), that
lists game installations and starts the client for one of them. It never
activates a profile itself and never mixes contracts: every launch names
exactly one profile and one installation on the command line -- `+set
com_gameProfile <key> +set fs_basepath <root>`, and `fs_game` for a mod -- so
the engine's explicit selection (step 1 above) decides before anything else
starts, as it does for a shortcut.

Discovery reuses the engine's evidence rather than a list of its own. The
launcher links `game_profile.cpp`; Steam's library manifests, GOG's and disc
installers' registrations, Heroic, Lutris, Wine prefixes and the usual game
folders only nominate folders, and a folder is listed only where
`Com_ProbeGameProfileMarkers` recognizes it (or its `GameData` or `game`
subfolder). A folder the player adds without that evidence is listed as
unverified under the profile they chose. Store names are labels and never
decide a profile. Whether a profile can start follows the same compile-time
gates (`Com_GameManagerCanActivate`) as the client built beside it, plus the
host check `Com_GameManagerNativeModuleFilename` makes for games whose retail
modules exist only as Windows x86 libraries.

The command line is quoted exactly where `unix_main.cpp`'s
`Sys_BuildCommandLine` quotes POSIX arguments, so Windows and POSIX hand the
engine's tokenizer the same text, and it stays within the engine's 32 startup
commands and its 1,023-character Windows command line. The launcher draws with
SDL3 and Dear ImGui through its own small SDL_Renderer glue, reads the
system's interface font at run time, and ships no artwork beyond the engine's
own icons. Its settings are its own JSON file, which the engine never reads.
`tests/launcher_core_tests.cpp` covers discovery over synthetic installations,
Steam's files, settings and the command line; the [Launcher
Guide](../LAUNCHER.md) is the player's view.

## Boundaries and adapters

Retail modules, network peers, demos and saves see exact, frozen layouts --
record sizes, field offsets, import and export ordinals, delta-field order.
Engine code works on its own wider internal types. The two meet only in
adapters that convert at the boundary and fail by name when they meet
something they do not know.

- `code/compat/` holds the frozen per-profile module ABIs (`*_game_abi.h`,
  `*_module_abi.h`), the import/export tables as `.def` lists, record and
  state adapters (`*_state_adapter.hpp`), and format readers for each title's
  own assets: Jedi's models, ICARUS scripts and motion paths, and Ritual's
  TIKI, SKB/SKA, `.urc` menus and `.ritualfont` fonts.
- The client and server keep per-profile hosts beside the shared code:
  `client_rtcw_*`, `client_wolfet_*`, `client_stvef_*` and `cl_alice_*` in
  `code/client/`, and `server_*_game.hpp` in `code/server/`.
- `code/game`, `code/cgame` and `code/ui` hold only the public module
  headers; no game logic is compiled into the engine.

An import a profile does not serve is refused with its profile, ordinal and
name before its arguments are read. The gate matrices count, per module, how
many imports are served.

## Subsystems

### Filesystem and packages

[`files.c`](../../code/qcommon/files.c) builds the search path from
`fs_steampath` (Quake Live only), `fs_basepath`, `fs_homepath` and `fs_game`
under the profile's base directory. Profile rules decide which archives mount
(RTCW's `sp_`/`mp_` prefixes, each profile's official paks), which config
names are protected, which paks may be downloaded, and how pure admission is
proved. Writes are contained to the writable home. Native modules loaded from
paks are copied into a hash-named cache under the home.

The engine package, `fntech3-pkg.fnz`, is searched beside the executable
before game data, but only for engine-owned sidecars and only inside the active
profile's `games/<profile>/<gamedir>/` namespace. It is never mounted as a
general pak. See the [engine package](PACKAGES.md) document.

### Modules

The QVM interpreter and x86 compiler (`vm*.c`) serve Quake III, Holomatch and
Jedi Outcast multiplayer. Native modules are loaded from the installation or
from paks with guarded imports (`native_seh.c`), through the interface the
profile names: Quake III's `dllEntry`/`vmMain`, Quake Live's structured
tables, the function tables of Elite Force, Jedi and IHV single player, and
Ritual's `GetGameAPI`/`GetCGameAPI`. Windows modules are 32-bit, which is why
the shared client proof target is Windows x86.

### Network and demos

[`msg.cpp`](../../code/qcommon/msg.cpp) and
[`net_chan.cpp`](../../code/qcommon/net_chan.cpp) select the profile's message
codec, delta tables and channel framing:
[`netchan_profile.h`](../../code/qcommon/netchan_profile.h) and
[`protocol_contract.hpp`](../../code/qcommon/protocol_contract.hpp) keep them
apart. Examples of profile-owned framing are Holomatch's packet scramble, the
Ritual wire's long fragment offset, Medal of Honor's out-of-band direction
byte and extra opcodes, and each title's handshake. Demos use the
profile's format: `.dm_<protocol>` for the Quake III family and Medal of
Honor (`.dm_6`, whose gamestate carries 2015's own baselines), `.efdemo` for
Holomatch and Ritual's fixed `.dm3` for Alice and F.A.K.K.2. A profile never
reads another's demos.

### Collision and worlds

The collision model (`cm_*.c`) reads each supported BSP through its own
loader: IBSP 46 (Quake III, Elite Force), IBSP 47 with Quake Live's
advertisement lump or RTCW's and ET's layouts, BSP 43 for the IHV Test,
Raven's RBSP for both Jedi games, Ritual's FAKK (version 12 for
F.A.K.K.2, 42 for Alice), and 2015's version 19 with its terrain and static
models for Medal of Honor. Quake III Arena and Quake Live keep the set their
baselines shared (43, 46 and 47); every other profile admits only its own
format (`Com_GameManagerBspFormat`). Hull and box-contents rules for entity
traces are profile policies.

### Server

The server hosts each profile's game module with its own policies: client
and configstring limits, reliable-command windows, bot support (Quake III's
AAS bot library where the game has one, RTCW's cast AI, none for Ritual's
games or Medal of Honor), restarts and broadcasts. Quake Live adds its factories, statistics
and ZMQ endpoints; these exist only in that profile. One selector chooses a
level's entity text for every profile -- Quake Live's `sv_altEntDir` file,
then a checked `maps/<map>.ent` override, then the BSP's lump -- and the game
module, the bot library and a listen server's client game all read its
choice (`SV_EntityText`); `code/qcommon/entity_text.hpp` reads and writes
that text as the engine hands it to modules.

### Client

The client runs each profile's client game and menus: Quake III-family UI
modules, Quake Live's WebUI (Awesomium from the retail installation, Windows
x86 only), and the engine-drawn `.urc` menus of Ritual's games and of Medal
of Honor, which draws its HUD and saved-game list from them too. Console fonts and notify
lines follow each profile, and each module sees its own game's key numbers;
the key names used by binds and configs are shared. The HUD aspect system
(`cl_hud.cpp`) applies FnQuake3's widescreen rules to Quake III Arena and
RTCW, whose modules assume 4:3. The entity editor (`cl_entity_editor.cpp`)
is engine-owned in every profile: it holds an input catcher bit modules
cannot clear, takes each adapter's world view before the renderer draws it,
holds the game's HUD and menus back while it is open, and draws its
Dear ImGui interface (`entity_editor_ui.hpp` turns ImGui's draw lists into
renderer commands) and its overlay through the renderers' shared 2D path.

### Renderers

Three renderer modules share the ABI in
[`code/renderercommon/tr_public.h`](../../code/renderercommon/tr_public.h):
GLx (`code/rendererglx`, with the compatibility substrate in `code/renderer`),
Vulkan (`code/renderervk`) and RTX (`code/rendererrtx`). They learn the
active profile through their import table. Profile formats -- RTCW's fog,
coronas and MDC/MDS models, ET's skeletal models, Raven's effects, Ritual's
TIKI models and shaders -- are read in `code/renderercommon` and the
renderers, and then drawn with the shared features: per-pixel and shadowed
dynamic lights, static map lights from `.lights.json` sidecars, HDR and
post-processing, and path tracing on RTX.

### Audio

`code/client/audio/` exposes one `S_*` facade over two backends: OpenAL
(default) and the legacy software mixer. Profile adapters turn each game's
sound data -- channels, distance laws, loops, ambient sets, speech, Ritual's
aliases -- into the OpenAL pipeline's distance model, occlusion, environment
reverb, audio zones and HRTF. The legacy mixer keeps each game's retail gain,
and its pan where that was read, as the documented retail-exact option. Codecs
cover WAV, Ogg Vorbis and MP3, which is registered only for Elite Force, Alice,
F.A.K.K.2 and Medal of Honor.

### Platform and services

SDL3 provides windows, input and audio output wherever the build finds it,
Windows included; the native Win32 and X11 layers remain for builds without
it. The platform code also owns the clipboard, process relaunch and each
game's own application icon. Optional services -- the Quake Live Steam
provider, Workshop, WebUI, cURL downloads -- are loaded dynamically and report
unavailability honestly; none is ever faked.

## Presentation policy

Retail evidence decides what a profile must honour: its ABIs, wire and demo
formats, saves, file formats, gameplay timing and the data it hands the engine.
It does not decide how the engine presents that data. Audio, rendering, input,
console and diagnostics use fnTech3's shared systems for every profile, fed
with the profile's own data; a retail-exact presentation may exist only as an
explicit, documented option. Each profile's evidence records which shared
features its path uses, which it leaves out and why (AGENTS.md invariant 11).

## Verification

### Tests

- `meson test` runs the C++ unit tests and the Python source suite.
- `python -m pytest tests` runs the Python suite alone. Many tests compile
  production functions sliced from the source against small stubs, so a new
  dependency in such a function can break fixtures that slice it.
- `tests/audio`, `tests/glx` and `tests/vulkan` hold deterministic renderer
  and audio tests and runtime sweep analyzers.

### Runtime probes

`scripts/probe_*.py` start the engine against a legitimate installation with
an isolated writable home under `.tmp/`, windowed, and usually on a private
desktop without user input. Rendering is judged from the engine's own
`screenshot` command, never an operating-system capture. Reuse the existing
build output paths so firewall permissions stay valid; see AGENTS.md for the
full runtime-test rules.

### Evidence records

Each compatibility change is recorded in a dated document under
`docs/fntech3/`, often with a machine-readable companion: what was observed,
what was inferred, the executable identity, commit, platform, profile,
command, installation identity and outcome. A companion sits beside its
record and shares its name, `2026-09-24-x.json` beside `2026-09-24-x.md`;
any further data file takes the record's date and a name of its own, in the
same folder. The ledger defines the evidence levels (Source, Unit, Build,
Fixture, Live, Interop) and links each record from the gate it moves.
Records never include retail files or credentials. Release packages ship the
whole `docs/fntech3/` tree, and the packager checks that every link between
its documents resolves.

## Documentation, versions and releases

- [`version/fntech3_version.h`](../../version/fntech3_version.h) is the single
  source of the version and of the supported-game line.
- `README.md` and `.install/README.html` are generated from
  `docs/templates/` by `python scripts/generate_docs.py`; edit the templates.
- The player guides in `docs/` are written for players. The packager renders
  them into web pages in each package's `manual/` folder with
  [`scripts/player_manual.py`](../../scripts/player_manual.py) and the theme
  in [`docs/manual/`](../manual/README.md).
- User-visible changes go under Unreleased in the root
  [CHANGELOG.md](../../CHANGELOG.md). [RELEASING.md](RELEASING.md) describes
  preparing a version and the draft prerelease workflow.
- Release packages contain the engine, the renderer modules,
  `fntech3-pkg.fnz`, `fnql-web.pak` and documentation, and exclude every retail
  or proprietary file, including the optional Steam provider.

## Adding a game or mode

A new title touches every layer above. In order:

1. Record the retail contracts first: installation identity, protocol, module
   tables and record layouts, each pinned to an observation.
2. Add the catalog entry, installation marker, protocol family and module
   interface, with a `runtimeBlocker` and a development option so default
   builds refuse it.
3. Build the adapters -- modules, wire, demos, saves, assets -- in
   `code/compat/` and the client and server hosts, one boundary at a time.
4. Give the profile its engine-package namespace in `scripts/root_archive.py`,
   and generate its sidecars from a legitimate installation.
5. Add a gate matrix to the ledger, then update AGENTS.md, the supported-game
   line in the version header, the README templates, the
   [Games Guide](../GAMES.md) and the [Build Guide](../../BUILD.md).
