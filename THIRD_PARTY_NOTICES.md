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
