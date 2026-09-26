# Build Guide

fnTech3 builds one engine for every supported game. A build produces:

- a single client executable, `fntech3` (`fntech3.exe` on Windows);
- a dedicated server, `fntech3.ded` (`fntech3.ded.exe`);
- exactly three renderer modules, `fnql_glx_<arch>`, `fnql_vk_<arch>` and
  `fnql_rtx_<arch>`, loaded beside the client and chosen at runtime with
  `\cl_renderer`, so no renderer needs its own executable;
- `fntech3-pkg.fnz`, the engine package of per-game sidecars; and
- `fnql-web.pak`, fnTech3's overlay for Quake Live's WebUI.

Meson is the preferred build system. The legacy Makefile and CMake files
remain for the platforms and CI jobs that still use them.

Every build runs **Quake III Arena** (`quake3`) and **Quake Live**
(`quakelive`). The other games are experimental and start only in a build made
with their development option; see
[Game development builds](#game-development-builds). A successful build proves
that the engine compiles and links. It does not establish any game's
compatibility: each profile must pass its runtime and interoperability gates
in the [compatibility ledger](docs/fntech3/COMPATIBILITY.md), and full
compatibility is not yet established for any profile.

## Supported targets

fnTech3's shared builds are **32-bit x86 only**. The retail game modules the
engine loads are 32-bit: Quake Live's `bin.pk3` carries `qagamex86.dll`,
`cgamex86.dll` and `uix86.dll`, and RTCW, Enemy Territory, Elite Force, Jedi
Academy, Alice and F.A.K.K.2 ship 32-bit Windows libraries. A 64-bit process
cannot load them. Build, test, package and run:

- **Windows x86 (Win32)** with MSVC or MinGW -- the shared client proof target
  and the only platform whose client runs Quake Live and the experimental games;
- **Linux i686** -- the dedicated-server proof target, with a maintained client
  for Quake III Arena and engine development.

macOS is disabled and unsupported, and fnTech3 does not build, test, package
or launch ARM, PowerPC, BSD or other non-x86 targets. The Windows Q3-only RTX
diagnostic build below is x86_64 and makes a narrower, separately documented
claim; it does not satisfy the shared-profile gates.

Enabling a game's development option never widens this contract.

## Quick start on Windows

Install Visual Studio 2017 or later with the C++ workload, Python 3, Meson and
Ninja. From an **x86 Native Tools** developer prompt at the repository root:

```bat
meson setup meson/build-msvc --buildtype=release
meson compile -C meson/build-msvc
meson install -C meson/build-msvc --destdir dist
```

Or use the build helper, which selects the static MSVC runtime and the
maintained options:

```powershell
.\.vscode\build-release.ps1 -Configuration RelWithDebInfo -BuildDir meson\build\win32-dev -Target both -Renderer all
```

Add a game's development switch, such as `-RTCWDevelopment`, to enable it; the
switches are listed under [Game development options](#game-development-options).
The helper builds an explicit list of runtime products, so it does not compile
Meson's test executables; add `-RunTests`, or use Meson directly, to test.

Copy or run the whole output directory: the executables need the renderer
modules, `fntech3-pkg.fnz` and `fnql-web.pak` beside them.

## Meson

Install Meson, Ninja, a C/C++ toolchain and the platform dependencies listed
below, then configure and build from the repository root:

```sh
meson setup meson/build
meson compile -C meson/build
meson test -C meson/build
```

Meson uses system libraries first, then the wrap files under `subprojects/`
for SDL3, OpenAL headers, libcurl, libjpeg-turbo, Ogg/Vorbis, FreeType, zlib,
minimp3 and FontStash when fallback downloads are allowed. Use `--wrap-mode=nofallback` for
a system-only build, or `--wrap-mode=forcefallback` to exercise the subproject
path. Do not add private in-tree copies of third-party libraries; the wraps own
the bundled fallbacks.

`meson test` runs the C++ unit tests and the Python source suite. The Python
suite alone runs with `python -m pytest tests`.

### General options

| Option | Default | Effect |
| --- | --- | --- |
| `-Dbuild-client=true\|false` | `true` | Build the client executable. |
| `-Dbuild-server=true\|false` | `true` | Build the dedicated server. |
| `-Drenderer-dlopen=true\|false` | `true` | Build the renderers as external modules beside one client executable. |
| `-Drenderers=glx,vk,rtx` | all three | Which renderer modules to build; valid entries are exactly `glx`, `vk` and `rtx`. |
| `-Drenderer-default=glx\|vk\|rtx` | `glx` | The default `\cl_renderer`, or the linked renderer when `renderer-dlopen=false`. |
| `-Dsdl=auto\|enabled\|disabled` | `auto` | Use SDL3 for video, input and audio output. |
| `-Dcurl=auto\|enabled\|disabled` | `auto` | cURL download support. |
| `-Dcurl-dlopen=auto\|true\|false` | `auto` | Resolve cURL at runtime; `auto` links it on Windows and loads it at runtime elsewhere. |
| `-Dsystem-jpeg=true\|false` | `false` | Require the system JPEG library instead of allowing the libjpeg-turbo fallback. |
| `-Dogg-vorbis=true\|false` | `true` | Ogg Vorbis sound and music, from system `vorbisfile` or the fallback. |
| `-Dmp3=true\|false` | `true` | The MP3 codec (minimp3 through `subprojects/minimp3.wrap`), registered only for the games whose data is MP3: Elite Force, Alice and F.A.K.K.2. |
| `-Dlocal-headers=true\|false` | `true` | Define `USE_LOCAL_HEADERS` for compatibility include conventions. |
| `-Ddefault-basedir=<path>` | empty | Compile in a `DEFAULT_BASEDIR`. |
| `-Daudio-tests=true\|false` | `true` | Build the audio tools and deterministic audio tests. |
| `-Dglx-tests=true\|false` | `true` | Build the deterministic GLx renderer logic tests. |
| `-Dstrict-warnings=true\|false` | `false` | Treat diagnostics in fnTech3's own targets as errors, without applying the policy to fallback dependencies. |
| `-Dq3-only=true\|false` | `false` | The Windows x86_64 Q3-only diagnostic target described below. |

Two renderer-linking modes remain for testing:
`-Drenderer-dlopen=false -Drenderer-default=vk` links one renderer into the
client, and `-Drenderer-dlopen=false -Drenderer-default=rtx` is the explicit
static RTX linkage and initialization gate.

The Windows build helpers select the static MSVC runtime and pin the nested
`zlib` fallback to a static library. Build directories configured by hand
need `-Dzlib:default_library=static` on reconfigure to adopt that policy; do
not copy a fallback `z-1.dll` into a distribution.

### Game development options

Each experimental game has one option. It lets the shared client and
dedicated server start that game's profiles for isolated development probes,
and prints an experimental-build banner. It changes no profile's contracts and
closes no gate, and release builds never enable it.

| Meson option | Helper switch | Profiles | What a developer build runs |
| --- | --- | --- | --- |
| `-Drtcw-development=true` | `-RTCWDevelopment` | `rtcw-sp`, `rtcw-mp` | Client and server, both modes. |
| `-Dwolfet-development=true` | `-WolfETDevelopment` | `wolfet` | Client and server. |
| `-Dstvef-development=true` | `-STVEFDevelopment` | `stvef-sp`, `stvef-mp` | Client and server, both modes; single player on Windows x86 only. |
| `-Djka-development=true` | `-JKADevelopment` | `jka-sp`, `jka-mp` | Dedicated server only. |
| `-Djk2-development=true` | `-JK2Development` | `jk2-sp`, `jk2-mp` | Dedicated server only. |
| `-Dq3-ihv-development=true` | `-Q3IHVDevelopment` | `q3-ihv` | Windows x86 dedicated server only. |
| `-Dq3test-development=true` | `-Q3TestDevelopment` | `q3test` | Windows x86 client and server, every release from 1.02 to 1.08. |
| `-Dalice-development=true` | `-AliceDevelopment` | `alice` | Windows x86 client and server. |
| `-Dfakk2-development=true` | `-FAKK2Development` | `fakk2` | Windows x86 client and server. |
| `-Dmohaa-development=true` | `-MOHAADevelopment` | `mohaa` | Windows x86 client and server. |

The options are independent of each other. None can be combined with
`-Dq3-only=true`. The [Games Guide](docs/GAMES.md) summarizes what each game
does today.

## Game development builds

Developer builds are for testing with your own legitimate installations. Keep
each run's writable data apart from the installation and from other games:

- select the profile explicitly with `+set com_gameProfile <profile>`;
- point `fs_basepath` at the installation and `fs_homepath` at a separate
  folder per game, for example under `.tmp/`;
- run windowed with `+set r_fullscreen 0`.

```powershell
.\fntech3.exe +set com_gameProfile rtcw-sp +set fs_basepath "<RTCW folder>" +set fs_homepath "<RTCW SP test home>" +set r_fullscreen 0
.\fntech3.ded.exe +set com_gameProfile jka-mp +set fs_basepath "<Jedi Academy folder>" +set fs_homepath "<JKA server home>" +set dedicated 1 +map mp/ffa1
```

The `scripts/probe_*.py` probes below start the engine the same way: isolated
homes, windowed clients on a private desktop, no injected mouse or keyboard
input, and captures from the engine's own `screenshot` command. Use a fresh
`--output` directory for each run. Each records evidence for a gate; none
establishes compatibility by itself.

### RTCW development profiles

Return to Castle Wolfenstein's single player (`rtcw-sp`) and multiplayer
(`rtcw-mp`) are two profiles of one game and use the same `fntech3.exe`.
Configure with `-Drtcw-development=true` (helper: `-RTCWDevelopment`). The
option does not permit an in-process mode switch: a mode change from the menu
relaunches the executable with the other profile. See the
[RTCW gate matrix](docs/fntech3/COMPATIBILITY.md#rtcw-gate-matrix) and the
[RTCW contracts](docs/fntech3/2026-09-14-rtcw-contracts.md).

The RTCW probes cover startup, maps, saves, navigation, audio, lights,
rendering and retail interoperability, for example
`probe_rtcw_campaign.py`, `probe_rtcw_save_cycles.py`, `probe_rtcw_pure.py`
and `probe_rtcw_retail_client.py`; each documents its arguments with `--help`.

### Enemy Territory development profile

Build the shared Windows x86 binaries with:

```powershell
.\.vscode\build-release.ps1 -Configuration RelWithDebInfo -BuildDir meson\build\win32-dev -Target both -Renderer all -WolfETDevelopment
```

The Meson equivalent is `-Dwolfet-development=true`. Default builds reject
`wolfet` in both the client and the dedicated server; with the option, both run
ET with its retail modules for isolated probes.

A bounded, loopback-only dedicated probe with an isolated writable home is:

```powershell
python scripts/probe_wolfet_server.py --server meson/build/win32-dev/fntech3.ded.exe --installation "E:\SteamLibrary\steamapps\common\Wolfenstein Enemy Territory" --output .tmp/wolfet/probe
```

Supply your own installation path. The probe disables pure mode and the
PunkBuster services fnTech3 does not provide, loads `goldrush`, queries
protocol 84, and exits. Use `--map` for another installed map and `--auto` for
installation detection. This is startup evidence; it does not establish
multiplayer interoperability.

On Windows, the retail-client probe checks inbound admission, pure or not, with
isolated homes and an unswitched private desktop. It keeps the client windowed
and disables mouse and joystick input. To check two objective-camera requests
and their replies:

```powershell
python scripts/probe_wolfet_retail_client.py --server meson/build/win32-dev/fntech3.ded.exe --installation "E:\SteamLibrary\steamapps\common\Wolfenstein Enemy Territory" --output .tmp/wolfet/camera --camera-check
```

Replace `--camera-check` with `--team-check` for Axis/Allies admission; the
modes are mutually exclusive. `--pure` runs the server with `sv_pure 1` and
requires the client to pass pure admission before entering the world.
`--move-check` spawns on Axis and runs the engine's own movement commands from
a config in the isolated home, comparing `viewpos` readouts;
`--rotation-check` changes the server map to `battery` while the client is
active and requires it to re-enter the world. `--map` selects another stock
map. None of these inject mouse or keyboard input, and none claim movement or
rendering parity; camera replies do not establish rendered-camera or gameplay
correctness.

To decode every skeletal model in an installation without running the engine:

```powershell
python scripts/probe_wolfet_models.py --installation "E:\SteamLibrary\steamapps\common\Wolfenstein Enemy Territory" --output .tmp/wolfet/models
```

It extracts each MDM and MDX into the output directory, parses it and reports a
verdict per file. This is asset-decode evidence and makes no rendering claim.

In a developer client the retail UI and cgame modules have called ET's import
boundaries. The UI drew ET's first-run profile dialog through its font, drawing
and precompiler imports. The cgame called its trace, per-map fog, decal, corona
and skin imports, among others, while it loaded oasis and then all six stock
maps on glx, vk and rtx; see the
[client run](docs/fntech3/2026-09-18-wolfet-client-run.md) and
[map load](docs/fntech3/2026-09-18-wolfet-map-load.md) records. Neither record
lists every import the runs reached, so the per-import counts, 146 of the
cgame's 147 and 114 of the UI's 121, remain source and unit evidence. The runs
claim no gameplay, sound playback or interoperability. `meson test` runs both
of the cgame boundary's regressions:

```powershell
meson test -C meson\build\win32-dev fntech3_wolfet_client_boundary
```

See the [ET gate matrix](docs/fntech3/COMPATIBILITY.md#wolfet-gate-matrix).

### Elite Force development profiles

Star Trek Voyager: Elite Force's single player (`stvef-sp`) and Holomatch
(`stvef-mp`) share one installation and `-Dstvef-development=true` (helper:
`-STVEFDevelopment`). Elite Force's sounds and music are MP3, so keep the
default `-Dmp3=true`. Single player runs the installation's `efgamex86.dll`
and `efuix86.dll`, so it needs Windows x86; other platforms refuse it before
filesystem startup. Holomatch runs the installed QVMs, and its dedicated
server has also been observed on Linux i686. Two probes check data without
running the game:

```powershell
python scripts/probe_stvef_mp3.py --installation "<Elite Force folder>" --output .tmp/stvef/mp3
python scripts/probe_stvef_demos.py --installation "<Elite Force folder>" --output .tmp/stvef/demos --probe meson/build/win32-dev/fntech3_stvef_demo_probe.exe
```

The first decodes every retail MP3 and compares its length with the size Raven
recorded in its tag; the second decodes the retail Holomatch demos through the
engine's message layer and re-encodes each delta. See the
[Elite Force gate matrix](docs/fntech3/COMPATIBILITY.md#elite-force-gate-matrix).

### Jedi Academy and Jedi Outcast development profiles

`-Djka-development=true` (helper: `-JKADevelopment`) enables Jedi Academy's
`jka-sp` and `jka-mp`, and `-Djk2-development=true` (helper:
`-JK2Development`) enables Jedi Outcast's `jk2-sp` and `jk2-mp`. Only the
dedicated server runs them. A multiplayer server runs the retail multiplayer
module with bots on a retail map, but no network client connects and no
snapshot is built. A single-player server loads the map's collision and then
refuses the single-player module by name, leaving the server idle until it is
stopped. No wire, savegame, client or renderer boundary exists yet for either
game. To read every Jedi Academy map, model and skeleton with the engine's own
readers:

```powershell
python scripts/probe_jka_assets.py --installation "E:/Games/Jedi Academy" --output .tmp/jka/assets
```

See the [Jedi Academy](docs/fntech3/COMPATIBILITY.md#jedi-academy-gate-matrix)
and [Jedi Outcast](docs/fntech3/COMPATIBILITY.md#jedi-outcast-gate-matrix)
gate matrices.

### Quake III IHV Test development profile

`-Dq3-ihv-development=true` (helper: `-Q3IHVDevelopment`) lets a Windows x86
dedicated server run the February 1999 IHV Test's original game DLL. Its
client and network paths stay gated, and its server drops every packet, so run
it with networking off. The lifecycle probe needs only the server and the
supplied installation:

```powershell
python scripts/probe_q3_ihv_game.py --server meson/build/win32-dev/fntech3.ded.exe --installation "<IHV installation>" --output .tmp/q3-ihv/game
```

See the [IHV ledger](docs/fntech3/Q3_IHV.md).

### Q3Test development profile

`-Dq3test-development=true` (helper: `-Q3TestDevelopment`) enables the public
Q3Test releases of 1999 (`q3test`), 1.02 to 1.08, on Windows x86, where their
Win32 modules run. The profile pins the release its installation holds. A
developer dedicated server runs every release's game module; a developer
client runs 1.05 to 1.08's client game and menus, and records and plays
`.dm3` demos. `scripts/probe_q3test.py` runs the servers, fnTech3's client,
the retail engines and a headless protocol client
(`fntech3_q3test_client_probe`, built with the tests) against each other on
127.0.0.1, windowed on a private desktop, capturing engine screenshots:

```powershell
python scripts/probe_q3test.py fntech3-client --installation "<Q3Test 1.08 installation>" --release 1.08 --server meson/build/win32-dev/fntech3.ded.exe --client meson/build/win32-dev/fntech3.exe --target retail --output .tmp/q3test/probes/client-108
python scripts/probe_q3test.py protocol-client --installation "<Q3Test 1.02 installation>" --release 1.02 --server meson/build/win32-dev/fntech3.ded.exe --probe meson/build/win32-dev/fntech3_q3test_client_probe.exe --target fntech3 --output .tmp/q3test/probes/protocol-102
python scripts/probe_q3test.py fntech3-client --installation "<Q3Test 1.08 installation>" --release 1.08 --client meson/build/win32-dev/fntech3.exe --target demo --demo q3demo1 --video sound --output .tmp/q3test/probes/sound-108
```

`--video` records a demo replay with the engine's `video` command through the
legacy mixer, which a client on a private desktop still runs silently, and
looks for each sound the replay started in the recorded mix (it needs numpy
and scipy). The retail 1.02, 1.03 and 1.05 engines fault while listing the host's
addresses; the probe starts them under `cdb` with a breakpoint that skips that
loop, and patches nothing. See the [Q3Test ledger](docs/fntech3/Q3TEST.md).

### Alice and F.A.K.K.2 development profiles

`-Dalice-development=true` (helper: `-AliceDevelopment`) enables American
McGee's Alice (`alice`), and `-Dfakk2-development=true` (helper:
`-FAKK2Development`) enables Heavy Metal: F.A.K.K.2 (`fakk2`). Both run
Ritual's engine rather than a Quake III one, on Windows x86. A developer
client runs each game's retail game and client-game libraries on all three
renderers, saves and loads, and records and plays back demos. For F.A.K.K.2,
`fs_basepath` must name an installed game: a copy of the disc keeps the game
libraries inside its InstallShield cabinets.

`probe_ritual_client.py` runs either game's client windowed on a private
desktop, executes console steps from a config in the isolated home and
captures engine screenshots:

```powershell
python scripts/probe_ritual_client.py --executable meson/build/win32-dev/fntech3.exe --installation "E:\Games\Alice" --profile alice --renderer glx --map garden1 --output .tmp/alice/garden1
```

Under these profiles `wait N` counts milliseconds, as the retail engine's does.
With `--actions <file>` the probe drives the menus and play with keys, clicks
and mouse motion queued inside the engine (`in_synthesize`, enabled only by
the `+set in_synthetic 1` the option adds); the script's syntax is in the
probe's help. `probe_ritual_audio.py` checks the engine's recorded mix against
each effect's own sound. See the [Alice](docs/fntech3/COMPATIBILITY.md#alice-gate-matrix) and
[F.A.K.K.2](docs/fntech3/COMPATIBILITY.md#fakk2-gate-matrix) gate matrices.

### Medal of Honor development profile

`-Dmohaa-development=true` (helper: `-MOHAADevelopment`) enables Medal of
Honor: Allied Assault (`mohaa`), 1.00, on Windows x86. It runs 2015's engine
through adapters of its own, not the Ritual profiles'. A developer client
runs the retail game and client-game libraries, loading the installation's own
`system86.dll` beside them; it draws the engine's retail menus and HUD, saves
and loads, and records and plays back demos. The retail `MOHAA.exe` is
SafeDisc-encrypted and plays no part. Probes run windowed with an isolated
home, for example:

```powershell
meson/build/win32-dev/fntech3.exe +set fs_basepath "<MoHAA installation>" +set fs_homepath .tmp/mohaa/home +set r_fullscreen 0 +set developer 1 +spmap m1l1
```

Under this profile `wait N` counts milliseconds, as 2015's engine does
(OpenMoHAA's reading), not frames.
See the [Medal of Honor gate matrix](docs/fntech3/COMPATIBILITY.md#medal-of-honor-gate-matrix).

### Visual Studio Code tasks and launches

VS Code's default `meson: build all games` task builds the client, dedicated
server, all three renderers, and runtime packages in `meson/build/win32-dev`
with debug symbols (`RelWithDebInfo`). It enables every development profile:
`-RTCWDevelopment`, `-WolfETDevelopment`, `-STVEFDevelopment`,
`-JKADevelopment`, `-JK2Development`, `-Q3IHVDevelopment`, `-AliceDevelopment`,
`-FAKK2Development`, `-MOHAADevelopment` and `-Q3TestDevelopment`, each passing
the matching Meson option. The matching `(Steam)` task additionally builds the
optional private provider. The release and debug tasks keep those profiles
gated.

All launch configurations use the already-built `win32-dev` binaries and have
no `preLaunchTask`. Build with Ctrl+Shift+B, then select a launch in Run and
Debug (Microsoft C/C++ extension, Windows). Launches are grouped by game and
mode, one group per profile. Normal startup entries are named `Q3 glx`,
`QL vk`, `RTCW SP rtx`, `EF MP glx` or `Alice vk`, with all three renderers
for Q3, QL, RTCW SP and MP, ET, Elite Force SP (`EF SP`) and Holomatch
(`EF MP`), and Alice. They use normal game startup
without a map command or forced intro skipping. Map launch names follow
`SP/MP bspname Map Title glx/vk/rtx`. The starter maps are Q3's `q3dm1`
(Arena Gate), QL's `campgrounds` (Campgrounds), RTCW SP's `escape1` (Escape),
RTCW MP's `mp_beach` (Beach Invasion), Elite Force SP's `borg1` (The Rescue),
Holomatch's `hm_borg1` (Assimilation) and Alice's `garden1` (Dry Landing),
each with GLx, Vulkan and RTX entries.

The **Wolfenstein: Enemy Territory** group has dedicated launches for all six
stock maps, with an installation-path prompt, loopback binding and a separate
writable home per map. PunkBuster is disabled and the launches keep
`sv_pure 0`; ET itself defaults to retail's `sv_pure 1`. `ET glx`, `ET vk`
and `ET rtx` open ET's menu, and one client launch per stock map loads that
map with `devmap` in a listen server, keeping `sv_pure 0` as well. Each uses
the same installation prompt, opens a 1024x768 window with the GL renderer,
binds IPv4 and IPv6 to loopback, turns off Steam integration and the WebUI, and
keeps its own writable home under `.tmp/vscode/wolfet/client/`. `ET pick map`
and `ET pick map ded` do the same for a map and, for the client, a renderer
chosen at launch.

Each of those profiles also has a dedicated-server launch of a starter map, named
`SP/MP bspname Map Title ded`: those above, plus the IHV Test's `ihv_test1`,
Jedi Academy's `t1_sour` and `mp/ffa1`, and Jedi Outcast's `kejim_post` and
`ffa_bespin`. Those five profiles have dedicated launches only: the IHV
client is blocked, Jedi Academy's client refuses its modules by name, and
nothing is wired for Jedi Outcast's. F.A.K.K.2, Medal of Honor and Q3Test have
no launches yet; start them from the command line or with their probe scripts.

`<name> pick map` launches, such as `Q3 pick map` or `JKA MP pick map ded`,
ask for any of the profile's stock maps, labelled with the game's own titles
where it has them, and a client launch then asks for the renderer. QL's also
ask for a factory, defaulting to `ffa`. RTCW SP uses `spmap` and ET's clients
`devmap`; every other map launch uses `map`. Installation paths are prompted
with local defaults. Launches force windowed mode and use separate writable
homes under `.tmp/vscode/<profile>/<renderer>`, or
`.tmp/vscode/<profile>/dedicated` for a dedicated server; ET keeps one home
per launch. Every dedicated launch but QL's binds IPv4 loopback, and IHV's
runs with networking off. The Elite Force and Alice clients open a 1024x768
window and bind IPv4 and IPv6 to loopback, as ET's do.

These entries do not establish runtime compatibility; every profile but Q3 and
QL remains experimental, and RTX requires suitable driver and device
capabilities. All VS Code build and launch entry points are intentionally
Win32 only, because the retail modules and Quake Live's Awesomium runtime are
32-bit.

## Windows

### MSVC

The [quick start](#quick-start-on-windows) builds with MSVC through Meson.
Alternatively, open `code/win32/msvc2017/fnql.sln` and select a Win32 Debug
or Release configuration. Its project delegates to the same Meson graph, with
strict warnings enabled and all renderer modules selected; build directories
are isolated below `meson/build/vs/`. Only Win32 configurations are exposed,
and direct project invocations also reject x64 and ARM64 before compilation.

### Optional Steam provider

Steam services are optional and never required to build fnTech3. Release
archives exclude the proprietary `fnql_steam.dll` provider and Valve's Steam
runtimes, release validation rejects those files, and release CI does not fetch
the provider. Developers with the separate `../FnQL-Steam` repository can pass
`-WithSteam` to `.vscode/build-release.ps1` to build its strict dynamic-library
target and stage only the resulting provider beside fnTech3. See the
[Steam provider notes](docs/fnql/STEAM_PROVIDER.md).

### Windows Q3-only RTX diagnostics (x86_64)

Use this explicit Meson target when the installed Vulkan driver exposes the
required KHR ray-tracing features to a 64-bit process but not to Win32. It is
an experimental Q3-only diagnostic build; the shared release target remains
Win32. From an **x64 Native Tools** developer prompt at the repository root,
configure a separate build directory:

```bat
meson setup .tmp/meson-q3-rtx-x64 --buildtype=release -Dq3-only=true ^
  -Dbuild-server=false -Drenderers=rtx -Drenderer-default=rtx ^
  -Db_vscrt=static_from_buildtype -Ddefault_library=static -Dstrict-warnings=true
meson compile -C .tmp/meson-q3-rtx-x64 fntech3-q3 fnql_rtx_x86_64
meson test -C .tmp/meson-q3-rtx-x64 --print-errorlogs
```

The outputs are `fntech3-q3.exe` and `fnql_rtx_x86_64.dll`; enabling
`-Dbuild-server=true` also produces `fntech3.ded-q3.exe`. Every other profile,
whether selected or detected, fails before filesystem startup with an
instruction to use the shared Win32 binary. Keep this build separate from
release staging and use legitimate Q3 assets. Native ray tracing still requires
the device capability checks; building this target does not establish
rendering compatibility. See the [RTX feature ledger](docs/fntech3/RTX_Q2RTX_PARITY.md)
and the [2026-09-06 runtime report](docs/fntech3/2026-09-06-rtx-round.md).

### MSYS2 and MinGW

The Makefile builds the MinGW32 client and server. With MSYS2, install the
dependencies from the `MSYS2 MSYS` shell:

```sh
pacman -Syu
pacman -S make mingw-w64-i686-gcc
```

then build from the `MSYS2 MINGW32` shell with `make ARCH=x86`. A standalone
MinGW toolchain uses the same command; the Windows dependencies are bundled.
Copy the binaries from the `build` directory, or stage them with
`make install DESTDIR=<output folder>`. Install into a folder of your own, not
into a game installation. Before the first Make build from a clean checkout,
run `meson subprojects download fontstash minimp3` (see
[Make and CMake](#make-and-cmake)).

## Linux

Linux i686 builds serve two purposes:

- **Dedicated servers.** A Quake Live server loads the retail
  `qagamei386.so` from `baseq3/bin.pk3`; fnTech3 does not build or validate
  the `qagamex64.so` alternative in the same archive. The archive's Win32
  `qagamex86.dll`, `cgamex86.dll` and `uix86.dll` are PE files: `dlopen`
  cannot load PE32 DLLs, so the Linux server does not rename or emulate them.
  Enemy Territory's Linux modules are `qagame.mp.i386.so`, `cgame.mp.i386.so`
  and `ui.mp.i386.so`. Whether another game's server has run on Linux is
  recorded in its gate matrix.
- **Clients.** The native Linux client is maintained for Quake III Arena and
  engine and platform development. Retail Quake Live ships no native Linux
  client or UI modules, and they and the Awesomium WebUI runtime are
  Win32-only, so the Linux client is not a Quake Live play path; use the Win32
  build.

Official Linux downloads are deterministic `.tar.gz` archives. Extract them
with `tar`, rather than copying individual files out of a browser, so the
executable modes on `fntech3`, `fntech3.ded` and the renderer modules survive.
Extract into a folder of your own and point the server at the game with
`fs_basepath`:

```sh
mkdir -p "$HOME/fntech3"
tar -xzf fntech3-*-linux-x86.tar.gz -C "$HOME/fntech3"
cd "$HOME/fntech3"
ldd ./fntech3.ded | grep 'not found' || true
./fntech3.ded +set com_gameProfile quakelive +set fs_basepath "$HOME/.local/share/Steam/steamapps/common/Quake Live" +set fs_homepath "$HOME/fntech3-homes/quakelive" +set dedicated 2 +map campgrounds
```

Steam may instead use `~/.steam/steam/steamapps/common/Quake Live` or a custom
library; pass that exact directory. For Quake Live, `fs_steampath` names an
additional Steam copy to search. Do not copy retail assets into the repository
or the fnTech3 package. The default Linux home, `~/.q3a`, is shared
by every game, so give each game its own `fs_homepath`.

### Meson i686 development build

On Debian or Ubuntu, this baseline builds the legacy X11 backend with system
development packages while Meson fetches only missing wrap fallbacks,
including the required pinned FontStash source:

```sh
sudo dpkg --add-architecture i386
sudo apt update
sudo apt install gcc-multilib g++-multilib libc6-dev-i386 \
  libgl-dev:i386 libx11-dev:i386 libxext-dev:i386 \
  libxrandr-dev:i386 libxxf86dga-dev:i386 libxxf86vm-dev:i386 \
  linux-libc-dev:i386 mesa-common-dev:i386 \
  git glslang-tools meson ninja-build pkg-config python3
meson setup meson/build-linux-x86 \
  --cross-file misc/meson/linux-x86.ini \
  --buildtype=debugoptimized -Dsdl=disabled -Dcurl=disabled \
  -Dogg-vorbis=false
meson compile -C meson/build-linux-x86
meson test -C meson/build-linux-x86 --print-errorlogs
```

For a complete i686 client build, install the corresponding `:i386` SDL3,
OpenAL, cURL, JPEG, Ogg/Vorbis and X11 development packages. The release
workflow is the canonical dependency baseline when distribution package names
differ.

The bundled SDL3 fallback additionally needs its Linux platform dependencies.
On Ubuntu 24.04, install them and exercise the same i686 path Linux CI uses:

```sh
sudo apt install libdbus-1-dev:i386 libegl-dev:i386 \
  libpipewire-0.3-dev:i386 libudev-dev:i386 libwayland-dev:i386 \
  libxcursor-dev:i386 libxi-dev:i386 libxkbcommon-dev:i386 wayland-protocols
meson setup meson/build-linux-sdl-x86 \
  --cross-file misc/meson/linux-x86.ini \
  --force-fallback-for=sdl3,fontstash \
  -Dbuild-server=false -Dcurl=disabled -Dogg-vorbis=false \
  -Drenderer-dlopen=false -Drenderer-default=glx -Dsdl=enabled
meson compile -C meson/build-linux-sdl-x86 fntech3
```

Stage a build and verify its layout before copying the whole flat runtime
directory:

```sh
mkdir -p .tmp
install_root="$(mktemp -d "$PWD/.tmp/linux-install.XXXXXX")"
meson install -C meson/build-linux-x86 --destdir "$install_root"
python3 scripts/verify_release_layout.py \
  "$install_root/usr/local/fntech3-pkg.fnz"
```

The staging prefix follows Meson's configured `--prefix`. Keep
`fntech3-pkg.fnz`, `fnql-web.pak` and all selected renderer modules beside the
client and server executables.

### Official-compatible i686 build

A 64-bit Linux host needs multiarch development packages. Package names vary
by Debian/Ubuntu release; this is the release-builder baseline:

```sh
sudo dpkg --add-architecture i386
sudo apt update
sudo apt install gcc-multilib g++-multilib make python3 pkg-config \
  libasound2-dev:i386 libcurl4-openssl-dev:i386 libjpeg-dev:i386 \
  libogg-dev:i386 libopenal-dev:i386 libvorbis-dev:i386 libx11-dev:i386 \
  libxrandr-dev:i386 libxxf86dga-dev:i386 libxxf86vm-dev:i386 \
  mesa-common-dev:i386
meson subprojects download fontstash minimp3
make -j"$(nproc)" release ARCH=x86 COMPILE_ARCH=x86 USE_SDL=0 \
  USE_RENDERER_DLOPEN=1 USE_GLX=1 USE_VK=1 USE_RTX=1
make install ARCH=x86 COMPILE_ARCH=x86 USE_SDL=0 DESTDIR="$PWD/.tmp/linux-x86/"
python3 scripts/verify_release_layout.py .tmp/linux-x86/fntech3-pkg.fnz
file .tmp/linux-x86/fntech3 .tmp/linux-x86/fntech3.ded
```

The release packager accepts a `linux-x86` artifact directory, restores
canonical executable modes after CI artifact transfer, writes a deterministic
`.tar.gz`, and reopens it to verify the client, dedicated server, all three
renderer modules, package sidecars, ELF i386 identity and safe archive paths.

## Make and CMake

The legacy Makefile and CMake inputs remain available while CI and packaging
finish migrating; new local work should prefer Meson. The Visual Studio
solution is a frontend for the Meson graph, not a separate source list.

Make and CMake use the same pinned FontStash and minimp3 sources owned by
`subprojects/fontstash.wrap` and `subprojects/minimp3.wrap`. Before the first
Make or CMake build from a clean checkout, fetch them with
`meson subprojects download fontstash minimp3`; CMake's default
`USE_SYSTEM_JPEG=OFF` also needs `meson subprojects download libjpeg-turbo`.
Client builds fail at configuration time if a dependency is unavailable; they
never substitute the incomplete host-font stubs.

Makefile options for Linux i686 and MinGW32 builds:

| Option | Effect |
| --- | --- |
| `BUILD_CLIENT=1` | Build the client, enabled by default. |
| `BUILD_SERVER=1` | Build the dedicated server, enabled by default. |
| `USE_SDL=0` | Use the legacy non-SDL Unix backend instead of SDL3 for video, audio and input. |
| `USE_GLX=1` | Build the GLx renderer, the canonical OpenGL-lineage renderer with capability tiers, debug callbacks, GPU timing, static-world acceleration, dynamic streaming, material execution, post-processing and output; enabled by default. |
| `USE_VK=1` | Build the Vulkan raster renderer (`vk`), enabled by default. |
| `USE_RTX=1` | Build the Vulkan ray-tracing renderer (`rtx`), enabled by default. |
| `USE_RENDERER_DLOPEN=1` | Build every enabled renderer as a module, switchable with `\cl_renderer`, instead of linking one into the client; enabled by default. |
| `RENDERER_DEFAULT=glx` | The default `\cl_renderer`, or the renderer a static build links when `USE_RENDERER_DLOPEN=0`; exactly `glx`, `vk` or `rtx`. Choosing `glx` also enables `USE_GLX=1`. |
| `USE_SYSTEM_JPEG=1` | Use the system JPEG library, enabled by default. |
| `USE_SYSTEM_OGG=1`, `USE_SYSTEM_VORBIS=1` | Use the system Ogg/Vorbis libraries, enabled by default. |
| `USE_CURL=1` | Use the system cURL library, enabled by default. Meson builds use the `subprojects/curl.wrap` fallback when no system libcurl is available. |

Examples:

- `make BUILD_SERVER=0 USE_RENDERER_DLOPEN=0 RENDERER_DEFAULT=vk` builds the
  client with one static Vulkan renderer and no dedicated server.
- `make BUILD_SERVER=0 USE_RENDERER_DLOPEN=0 RENDERER_DEFAULT=rtx` builds an
  explicit static RTX validation client.
- `make BUILD_SERVER=0 USE_GLX=1` includes the GLx module so it can be selected
  with `\cl_renderer glx` after a `\vid_restart`.

## Package sidecars

Meson and Make build `fntech3-pkg.fnz` and `fnql-web.pak` automatically. Legacy
CMake needs a separate
`python scripts/build_root_archive.py --output <executable-directory>/fntech3-pkg.fnz`
step.

- `fntech3-pkg.fnz` holds fnTech3's own per-game data -- audio zones, static
  light files, fog presets, HUD layouts and sound tuning -- under
  `games/<profile>/<gamedir>/`. Upgrade it together with the engine; the old
  `FnQL-pkg.fnz` layout is not read. See the
  [package layout](docs/fntech3/PACKAGES.md), which also lists the scripts
  that generate each game's sidecars from a legitimate installation.
- `fnql-web.pak` contains only fnTech3's settings pages for Quake Live's WebUI
  and still needs the retail `web.pak` for everything else. To build it alone,
  run `python scripts/build_webpak.py --source-root code/client/webui --output
  .tmp/fnql-web.pak`.

Release packaging, versioning and draft prereleases are described in
[RELEASING.md](docs/fntech3/RELEASING.md).
