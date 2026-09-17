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
