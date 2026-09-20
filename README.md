<div align="center">

<h1>
  <img src="assets/logos/fntech3-readme-banner.png" alt="fnTech3 — Unified source port for id Tech 3" width="720">
</h1>

### One engine for Quake III Arena, Quake Live, RTCW, Enemy Territory, and Elite Force

<p>
  <a href="docs/fntech3/COMPATIBILITY.md"><img alt="Compatibility Status" src="https://img.shields.io/badge/Compatibility-In%20Progress-2563eb?style=for-the-badge"></a>
  <a href="#3-getting-started"><img alt="Quick Start" src="https://img.shields.io/badge/Quick-Start-16a34a?style=for-the-badge"></a>
  <a href="#4-documentation"><img alt="Player Documentation" src="https://img.shields.io/badge/Player-Docs-f59e0b?style=for-the-badge"></a>
  <a href="BUILD.md"><img alt="Build Guide" src="https://img.shields.io/badge/Build-Guide-7c3aed?style=for-the-badge"></a>
  <a href="LICENSE"><img alt="GPL-2.0 License" src="https://img.shields.io/badge/License-GPL--2.0-c2410c?style=for-the-badge"></a>
</p>

*Compatibility targets: Quake III Arena, Quake Live, Return to Castle Wolfenstein (SP/MP), Wolfenstein: Enemy Territory, and Star Trek Voyager: Elite Force (SP/Holomatch) · Base version: `0.1.0` · Current build: `0.1.0`*

</div>

## 1. Introduction

**fnTech3** is a unified source port for id Tech 3 games. Its goal
is one engine distribution and shared executable set for **Quake III Arena**,
**Quake Live**, **Return to Castle Wolfenstein**, including RTCW's single
player and multiplayer modes, **Wolfenstein: Enemy Territory**, and
**Star Trek Voyager: Elite Force**, including its single player and Holomatch
modes.
It builds on FnQuake3 modernization and the FnQL
compatibility baseline.

The shared client and dedicated server select one immutable game profile before
filesystem, protocol, module, renderer, or optional-service startup. Shared
rendering, audio, input, platform, and tooling improvements must preserve each
game's own module, asset, network, demo, and gameplay contracts.

### Games and profiles

| Game / mode | Startup profile | Current availability |
| --- | --- | --- |
| Quake III Arena | `q3` (canonical `quake3`) | Enabled; compatibility validation remains in progress. |
| Quake Live | `ql` (canonical `quakelive`) | Enabled; retail client modules require Windows x86; compatibility validation remains in progress. |
| Return to Castle Wolfenstein — single player | `rtcw-sp` | Experimental; requires a build with `-Drtcw-development=true`; gated in default builds. |
| Return to Castle Wolfenstein — multiplayer | `rtcw-mp` | Experimental; requires the same development option; gated in default builds. |
| Wolfenstein: Enemy Territory | `wolfet` (alias `et`) | Experimental dedicated probes require `-Dwolfet-development=true`; client startup remains blocked. |
| Star Trek Voyager: Elite Force — single player | `stvef-sp` | Experimental; Windows x86 with the installed retail modules; requires `-Dstvef-development=true`; gated in default builds. |
| Star Trek Voyager: Elite Force — Holomatch | `stvef-mp` | Experimental; requires the same development option; gated in default builds. |

These five games and seven profiles define the current project scope.
**Full compatibility is not yet established for any profile.** Track observed
build, unit, fixture, live, and interoperability evidence in the
[compatibility ledger](docs/fntech3/COMPATIBILITY.md). A successful build or an
inherited FnQL feature does not close a fnTech3 runtime gate.

RTCW SP and MP use the same `fntech3.exe`. Switching modes relaunches that
executable with the selected profile. Development builds have recorded menu,
map, and save/load evidence; campaign, multiplayer interoperability, and broader
renderer/audio/input gates remain open. See the
[RTCW gate matrix](docs/fntech3/COMPATIBILITY.md#rtcw-gate-matrix) for the current
limits and the [game-manager contracts](docs/fntech3/2026-09-14-rtcw-contracts.md)
for profile and mode-switch behavior.

The [ET implementation record](docs/fntech3/2026-09-17-wolfet-contracts.md)
describes protocol 84, native game-module boundaries, dedicated probes, and
the remaining client, renderer, pure-server and interoperability blockers.

fnTech3 is engine-only. Supply a legitimate Quake III Arena, Quake Live,
Return to Castle Wolfenstein, or Enemy Territory installation for the selected profile. Retail
modules, assets, and proprietary runtimes stay
in that installation. Release packages exclude the optional proprietary Steam
provider and Valve Steam runtimes. Steam services require a separately supplied
compatible provider; missing services report their unavailability honestly.

## 2. Features

These capabilities come from the engine baselines and ongoing compatibility
work. Their behavior must be validated separately for each applicable profile
against the compatibility ledger. QL-specific WebUI, Steam, and Workshop
services remain part of the QL profile.

- **Unified game compatibility** — profile-owned installation, module ABI,
  protocol, demo, filesystem, asset, and optional-service contracts for Q3, QL,
  and RTCW SP/MP, including RTCW campaign and savegame boundaries.
- **Fast, modern hardware rendering** — choose between optimized **GLx**,
  **Vulkan raster** and **Vulkan RTX** renderers, with GPU-cached world
  geometry, optimized lightmaps, reduced rendering overhead, improved depth
  precision and better performance on large or demanding maps.
- **Modern visual upgrades** — optional HDR rendering and output, MSAA,
  supersampling, render scaling, anisotropic filtering, bloom, tone mapping,
  colour grading, high-quality dynamic lighting and shadows, motion blur, lens
  flares, soft particles, enhanced liquids and per-map fog.
- **Improved player visibility** — optional player rim lighting, stencil
  outlines or both, with configurable enemy, teammate, team and free-for-all
  colours, intensity and outline thickness. Cel shading, model outlines and
  world-edge outlines are also available.
- **Better modern hardware and OS support** — SDL3-based video and input,
  OpenAL audio, modern graphics APIs, high-resolution and high-refresh
  displays, borderless/windowed/fullscreen modes, fast Alt+Enter switching,
  controller hotplugging and improved compatibility with current desktop
  systems.
- **Low-latency, configurable controls** — raw mouse input, Quake
  Live-accurate CPI scaling, signed acceleration, sensitivity caps and
  angle-history filtering, while retaining classic Quake3e/ioquake3 mouse
  styles. Modern gamepads and Unicode text input are also supported.
- **Modern spatial audio** — OpenAL and HRTF headphone positioning, surround
  output, reverb, geometry-based occlusion, output-device selection, live
  device recovery and automatic fallback to the classic software mixer.
- **Major quality-of-life improvements** — scalable TTF console text, smooth
  scrolling, mouse selection, clipboard support, drag-and-drop editing, live
  command/CVar/map completion, minimize hotkeys, focus muting, lower background
  CPU usage and more flexible player configuration.
- **Better screenshots, video and streaming tools** — PNG, TGA, JPEG and BMP
  capture, clipboard screenshots, custom filenames, camera metadata,
  configurable levelshots, watermarks, cubemap capture, arbitrary-resolution
  output and FFmpeg video piping.
- **Faster content handling** — quicker startup and map restarts with large PK3
  libraries, raised filesystem and map limits, fast redirected downloads and
  automatic handling of server-required Workshop content.
- **Greater stability and security** — extensive Quake3e and FnQ3 bug fixes,
  improved memory management, lower memory usage, stronger network protection
  and graceful fallbacks when optional rendering, audio, WebUI or Steam
  functionality is unavailable.

## 3. Getting Started

Build from source using the [Build Guide](BUILD.md). The initial shared client
proof target is Windows x86 because retail QL client modules require that ABI.
The Linux i686 dedicated path needs its own profile-specific runtime evidence.
RTCW development probes use the same Windows x86 client. Other platform and
architecture claims require separate evidence; the Q3-only x64 RTX diagnostic
build does not establish the shared game contract.

Keep `fntech3-pkg.fnz`, `fnql-web.pak`, the `fnql_<renderer>_x86` modules, and
required redistributable libraries beside `fntech3.exe` and
`fntech3.ded.exe`. The engine package stores assets under
`games/<profile>/<gamedir>/` so games and mods have separate namespaces.
Upgrade the executable and `fntech3-pkg.fnz` together; the old `FnQL-pkg.fnz`
layout is no longer read. See the [package layout](docs/fntech3/PACKAGES.md).

### Windows client profile selection

Use an isolated writable home path for each profile and select the legitimate
installation explicitly. Both examples force windowed mode:

```powershell
.\fntech3.exe +set com_gameProfile q3 +set fs_basepath "<Quake III Arena folder>" +set fs_homepath "<Q3 test home>" +set r_fullscreen 0
.\fntech3.exe +set com_gameProfile ql +set fs_basepath "<Quake Live folder>" +set fs_homepath "<QL test home>" +set r_fullscreen 0
```

For experimental RTCW startup, first enable the
[RTCW development build option](BUILD.md#rtcw-development-profiles), then use
the same executable with an explicit mode:

```powershell
.\fntech3.exe +set com_gameProfile rtcw-sp +set fs_basepath "<Return to Castle Wolfenstein folder>" +set fs_homepath "<RTCW SP test home>" +set r_fullscreen 0
.\fntech3.exe +set com_gameProfile rtcw-mp +set fs_basepath "<Return to Castle Wolfenstein folder>" +set fs_homepath "<RTCW MP test home>" +set r_fullscreen 0
```

Automatic selection requires unambiguous installation evidence. An RTCW
installation containing both modes requires an explicit `rtcw-sp` or `rtcw-mp`
choice. The profile cannot change during the process lifetime. These commands
describe startup; complete gameplay, demo, and interoperability gates remain
open for every profile.

### Native Linux dedicated server

The shared dedicated executable is `fntech3.ded`. Use an explicit
profile and separate home path, for example:

```sh
./fntech3.ded +set com_gameProfile ql +set fs_basepath "<Quake Live folder>" +set fs_homepath "<QL server test home>" +set dedicated 2 +map campgrounds
```

QL's native server boundary uses retail `qagamei386.so`; the native Linux client
cannot load Win32-only retail client/UI modules. Steam and WebUI remain optional
QL services and must report unavailable services honestly.

## 4. Documentation

- [Changelog](CHANGELOG.md) for fnTech3's reviewed release history, starting at 0.1.0.
- [Manual releases and versioning](docs/fntech3/RELEASING.md) for preparing a version and reviewing a draft release.

### Player guides

- [Display Guide](docs/DISPLAY.md) for renderer selection, display modes, HDR,
  render scaling, anti-aliasing, bloom, lighting, shadows, fog, liquids, lens
  flares, motion blur, and other presentation controls.
- [RTX Renderer Guide](docs/RTX.md) for ray-tracing requirements, renderer
  selection, fallback behavior, safe starting settings, and troubleshooting.
- [Visuals Guide](docs/VISUALS.md) for player highlighting, team and enemy
  colours, cel shading, and outlines.
- [Aspect Handling Guide](docs/ASPECT_CORRECTION.md) for retail WebUI, menu,
  and cinematic layout behavior.
- [Audio Guide](docs/AUDIO.md) for backends, devices, HRTF, spatial effects,
  fallback behavior, and troubleshooting.
- [Console Guide](docs/CONSOLE.md) for console layout, scaling, completion,
  clipboard use, and mouse interaction.
- [Screenshot Guide](docs/SCREENSHOTS.md) for capture commands, formats,
  filenames, metadata, levelshots, watermarks, and cubemaps.
- [FnQL baseline release history](https://github.com/themuffinator/FnQL/releases)
  for historical downloads and changelogs; these are not fnTech3 releases.

### Technical and build docs

- [Compatibility ledger](docs/fntech3/COMPATIBILITY.md) for the unified goal,
  all five profiles, observed evidence, open gates, and release sign-off.
- [Project mission and contributor rules](AGENTS.md) for shared-engine
  invariants and non-regression requirements.
- [Build Guide](BUILD.md) for compiling fnTech3 locally.
- [GLx Renderer Guide](docs/GLX.md) for GLx availability, diagnostics, and
  troubleshooting.
- [Input Compatibility Notes](docs/fnql/INPUT_COMPATIBILITY.md) for retail mouse
  math, text input, controllers, and preserved legacy input profiles.
- [WebUI Backend Notes](docs/fnql/WEBUI_BACKEND.md) for retail resources,
  platform boundaries, overlay order, and fallback rules.
- [Steam Provider Notes](docs/fnql/STEAM_PROVIDER.md) for the open loader ABI,
  separately distributed provider, and runtime policy.
- [RTX Renderer Contract](docs/fnql/RTX_RENDERER.md) for the renderer's Quake
  Live ABI boundary, conservative defaults, build gates, and runtime validation.
- [FnQL baseline technical notes](docs/fnql/TECHNICAL.md) for inherited
  subsystem history and QL reference contracts. Current fnTech3 scope and
  release requirements are defined by the compatibility ledger.

## 5. Credits

fnTech3 combines [FnQuake3](https://github.com/themuffinator/FnQ3) and
[FnQL](https://github.com/themuffinator/FnQL), with the id Software, ioquake3,
and Quake3e engine lineage. QLSRP supplies behavioral and ABI evidence for
independently written implementation work.

See [CREDITS.md](CREDITS.md) for pinned source lineage, acknowledgements, and
trademark notices, and [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) for
incorporated external implementation data and its license.
