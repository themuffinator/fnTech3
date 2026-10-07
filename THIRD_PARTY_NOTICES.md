# Third-party notices

This file records externally sourced implementation data that warrants a notice
beyond the preserved per-file license headers. It supplements, and does not
replace, `LICENSE` or the licenses shipped by dependencies.

## QuakeLive beta files decoder (`quakelivedec` / qldec)

`code/qcommon/quakelive_pk3_xor.h` contains the Quake Live beta `.pk3` XOR table
published in Luigi Auriemma's `quakelivedec.c` version 0.1.

- Copyright 2008 Luigi Auriemma
- Source: [QuakeLive beta files decoder on the author's
  site](https://aluigi.altervista.org/papers.htm#qldec)
- Archived source package: [`quakelivedec.zip` on the author's official
  mirror](https://mirror.aluigi.org/papers/quakelivedec.zip)
- License: GNU General Public License, version 2 or (at your option) any later
  version (`GPL-2.0-or-later`)

The source package's license header permits redistribution and modification
under GPL-2.0-or-later. That license is compatible with this repository's GPL
license. fnTech3 incorporates only the published fixed table and its provenance,
not the decoder executable.

## General engine lineage

`code/client/cl_hud.cpp` adapts the GPL-2.0-or-later
[FnQuake3 HUD implementation](https://github.com/themuffinator/FnQ3/blob/7d8c9629cdc7979f09697266470359101ffd9eda/code/client/cl_hud.cpp).
The source matches the pinned merger revision; this is an implementation
import, distinct from reference-only observations. Its project GPLv2 license
and GPL-2.0-or-later engine lineage are compatible with fnTech3. The source
file had no individual copyright header; the adaptation records its project
provenance and license, retaining the original HUD rule and dump behavior.

The RTCW HUD JSON presets are original fnTech3 data authored from stock layout
and shader-name observations, including legitimate retail draw metadata and
the published [RTCW SP](https://github.com/id-Software/RTCW-SP) and
[RTCW MP](https://github.com/id-Software/RTCW-MP) cgame contracts. Those GPLv3
source references were inspected only for behavior; no implementation text
or proprietary menu assets are incorporated.

The Heavy Metal: F.A.K.K.2 HUD JSON preset is original fnTech3 data authored
from the placement of the game's four retail HUD menus, observed in a
legitimate installation, and from fnTech3's own HUD draw dump. No retail menu
file, text or asset is incorporated.

The Enemy Territory, Elite Force and Alice HUD JSON presets are original
fnTech3 data authored from fnTech3's own HUD draw dumps and from layout facts
observed in the Enemy Territory GPL source, Raven's Elite Force single-player
SDK and the Elite-Reinforce port. Those sources, and Lilium Voyager's engine,
were inspected only for behavior; no implementation text is incorporated.

The Q3 assets under `pkg/games/quake3/` are incorporated from the
[FnQuake3 package tree](https://github.com/themuffinator/FnQ3/tree/7d8c9629cdc7979f09697266470359101ffd9eda/pkg),
whose GPL-2.0-or-later engine lineage and GPLv2 `LICENSE` are compatible with
this repository. Original authored data and provenance comments are preserved.
The [package import record](docs/fntech3/2026-09-17-fnq3-package-assets.json)
pins all 92 files and the two filename mappings; these are engine-owned
sidecars/presets, not retail game assets.

The pak and PNG readers use [zlib](https://zlib.net/) by Jean-loup Gailly and Mark
Adler through the existing `subprojects/zlib.wrap` fallback or a system package.
Its permissive zlib license (verified in the pinned 1.3.2 `zlib.h`) is compatible
with this repository's GPL license. The original license remains in the
dependency. The 2026-09-14 loading update removes the embedded zlib 1.1.3
decoder from `unzip.c`; its adapted Gilles Vollant minizip frontend and
attribution remain. No replacement third-party implementation is vendored.

The 2026-09-06 WebUI request and avatar retry update incorporates reviewed
implementation, tests, and an optional retail probe from
[FnQL](https://github.com/themuffinator/FnQL), copyright 2026 FnQL contributors,
under GPL-2.0-or-later. This is compatible with fnTech3's GPL license. Original
file headers are preserved. These changes were uncommitted in the reference
worktree; the [source snapshot record](docs/fntech3/2026-09-06-webui-request-port.md)
identifies them separately from the committed merger baseline. No retail
runtime, asset, or Steam provider implementation is incorporated by this port.

The id Software, ioquake3, Quake3e, FnQuake3, and FnQL engine lineage is
credited in [`CREDITS.md`](CREDITS.md). GPL-derived source files retain their
original copyright and license headers. Meson subprojects and system
dependencies retain their own license files and notices in their respective
packages.

## Enemy Territory reference observations

The original [Enemy Territory source release](https://github.com/id-Software/Enemy-Territory/tree/40342a9e3690cb5b627a433d4d5cbf30e3c57698)
is GPL-3.0-or-later with accompanying additional terms. It is a reference only:
fnTech3 does not incorporate its implementation text or relicense this project.
Frozen declarations, ordinal tables, field encodings, profile adapters and TAG
asset validation are independently authored interoperability work. Source probes
compile the user-selected external reference in disposable project-local folders;
only independently generated synthetic packet vectors are checked in. Retail
modules, maps, configuration assets and packets containing private data are not
redistributed. The [ET evidence record](docs/fntech3/2026-09-17-wolfet-contracts.md)
separates source/unit evidence from installed observations and open gates.

## OpenMoHAA

Medal of Honor: Allied Assault support incorporates code from
[OpenMoHAA](https://github.com/openmoh/openmohaa/tree/ab43a0def6c4feccedae36fdb58c2e007d7e4c35)
at `ab43a0def6c4feccedae36fdb58c2e007d7e4c35`, copyright the OpenMoHAA team,
id Software and, for the terrain collision, Leszek Godlewski, under
GPL-2.0-or-later, which is compatible with fnTech3's GPL license: the model
runtime under `code/mohaa/` (listed file by file in its
[README](code/mohaa/README.md)), `code/qcommon/mohaa_msg.cpp`,
`code/qcommon/cm_mohaa_terrain.c`, `code/qcommon/mohaa_terrain_squares.h`,
`code/client/cl_mohaa.cpp`, `code/server/server_mohaa_net.hpp` and
`code/server/server_mohaa_world.hpp`.
Each keeps its original copyright and licence header, and the changes are
noted in it. The rest of the profile consults OpenMoHAA as a reference only.
No retail Medal of Honor module, runtime (`MOHAA.exe`, `system86.dll`) or
asset is redistributed; `system86.dll` is loaded from the user's installation.

## Elite Force references and minimp3

The [movie audio probe](docs/fntech3/2026-09-20-stvef-movie-audio.md) independently
declares the observed Bink sound callback boundary and uses the Windows
DirectSound interface. It contains no proprietary runtime or decoded audio.

The [shadow fixtures](docs/fntech3/2026-09-20-stvef-shadows.md) record
independently chosen synthetic geometry observed through the user's installed
executables. They contain no proprietary implementation or game assets.

fnTech3's Elite Force support is interoperability work against the user's
legitimate installation. Raven's [Elite Force SP mod SDK](https://github.com/UberGames/SP-Mod-Source-Code/tree/04e0e618d1ee57a2919f1a852a688c03b1aa155d)
is distributed under the non-free "STEF Game Source License"; it is consulted
only for names, ordinals and record layouts, and none of its text is
incorporated. [Lilium Voyager](https://github.com/zturtleman/lilium-voyager/tree/0f7dcd89d142c8b635ce2756a80cb62411731848)
and the engine side of [VoyagerSP-Android](https://github.com/imjustadudegamer/VoyagerSP-Android/tree/0d8942e86a8469859da086f590875bcb66b4f4df)
are GPL-2.0-or-later and GPLv2 references; their text is not incorporated
either. Observations of the retail `stvoy.exe` are cited by address in the
[Elite Force records](docs/fntech3/2026-09-18-stvef-contracts.md).

The protocol-26 tests contain synthetic encoded bytes produced by the pinned
Lilium message encoder. The optional generator compiles against a separately
supplied GPL checkout; neither its implementation nor game content is copied
into fnTech3. The independently written codec and fixture provenance are
documented in the [ioEF record](docs/fntech3/2026-09-20-stvef-ioef.md).

The RDM5 and lighting tests contain independently authored fixtures and numeric
observations from retail mathematical routines. No retail binary or model data
is included; their identities and the limits of the comparison are recorded in
the [model and lighting record](docs/fntech3/2026-09-20-stvef-mdr.md).
The [effect geometry fixtures](docs/fntech3/2026-09-20-stvef-effects.md) likewise
contain only independently chosen inputs and numerical observations; no
retail code, assets or captured game state is included.

The MP3 codec builds [minimp3](https://github.com/lieff/minimp3/tree/ea99364f61c14656440e8d77e9c233ccf3124633)
(copyright Lion "lieff") from `subprojects/minimp3.wrap`. minimp3 is dedicated
to the public domain under Creative Commons CC0 1.0 Universal, which is
compatible with this repository's GPL license; its `LICENSE` file stays with
the fetched subproject. Retail MP3 files, Bink movies and the proprietary Bink
runtime are never redistributed.

The Windows x86 SP client can load the installation's `binkw32.dll` by an
absolute path. This optional interoperability boundary contains independently
declared exports and no proprietary SDK headers or implementation text. Its
framing checks, platform limits and installed observations are documented in
the [Bink movie record](docs/fntech3/2026-09-20-stvef-bink.md).
The [presentation follow-up](docs/fntech3/2026-09-20-stvef-cinematic-presentation.md)
uses synthetic pixels and measured fade values from the installed retail
executable; no proprietary rendering or decoder implementation is incorporated.

The optional Windows x86 SP force-feedback bridge likewise loads the user's
installed `FFC10.dll` by an absolute path. Its independently declared CFeel 1.0
ABI and effect parameters are interoperability observations; no proprietary
SDK headers, runtime implementation or DLL is incorporated. The
[feedback fixture and record](docs/fntech3/2026-09-21-stvef-force-feedback.md)
contain numeric observations and owned test callbacks, not game assets or
proprietary code.

## Dear ImGui

The entity editor builds [Dear ImGui](https://github.com/ocornut/imgui/tree/f1cc2ae15e53a861a874c3034aae6798fde194ab)
1.92.9b (copyright Omar Cornut and the Dear ImGui contributors) from
`subprojects/imgui.wrap`: its core (`imgui.cpp`, `imgui_draw.cpp`,
`imgui_tables.cpp`, `imgui_widgets.cpp`) and `misc/cpp/imgui_stdlib.cpp`,
with the `stb_truetype`, `stb_rect_pack` and `stb_textedit` headers it carries
(Sean Barrett's, under the MIT License or in the public domain) and its
embedded ProggyClean and ProggyForever fonts (Tristan Grimmer's and Disco
Hello's, under the MIT License). The MIT License is compatible with this
repository's GPL license. ImGui's `LICENSE.txt` stays with the fetched
subproject, and no ImGui source is copied into the tree; the wrap's
`meson.build` in `subprojects/packagefiles/imgui` is this project's own.

Built copies of fnTech3 contain Dear ImGui, so its notice travels with them
here, as its license asks:

```text
The MIT License (MIT)

Copyright (c) 2014-2026 Omar Cornut

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

The two fonts it embeds are under the same MIT License, with these
copyrights: ProggyClean, Copyright (c) 2004, 2005 Tristan Grimmer; and
ProggyForever, Copyright (c) 2026 Disco Hello, Copyright (c) 2019, 2023
Tristan Grimmer. The `stb` headers are used under their public-domain
alternative.

## SDL3 and the launcher

`fntech3-launcher` links [SDL3](https://github.com/libsdl-org/SDL/tree/release-3.4.0)
3.4.0 (copyright Sam Lantinga and the SDL contributors) from
`subprojects/sdl3.wrap`, or a system SDL3, for its window, input, folder
dialogs and drawing; client builds that enable `-Dsdl` use the same library
for their window, input and audio. SDL3 is under the zlib License, which is
compatible with this repository's GPL license. Its `LICENSE.txt` stays with
the fetched subproject, and its notice is reproduced below.

The launcher also builds Dear ImGui's core from `subprojects/imgui.wrap`,
under ImGui's MIT notice in this file. Its SDL3 and SDL_Renderer glue for
ImGui (`code/launcher/launcher_imgui_sdl.cpp`) is written for fnTech3 against
the two libraries' public interfaces; ImGui's own SDL3 backends were read for
reference only, and none of their text is incorporated. The launcher reads
the system's own interface font at run time and ships no font beyond ImGui's
embedded fallback.

```text
Copyright (C) 1997-2025 Sam Lantinga <slouken@libsdl.org>

This software is provided 'as-is', without any express or implied
warranty.  In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not
   claim that you wrote the original software. If you use this software
   in a product, an acknowledgment in the product documentation would be
   appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must not be
   misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.
```
