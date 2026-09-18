# Elite Force profiles, installation identity and MP3 audio

## Scope

Star Trek Voyager: Elite Force is a fifth title with two mode profiles, as RTCW
is: `stvef-sp` (aliases `efsp`, `stvoy`, `eliteforce-sp`) for the single-player
campaign and `stvef-mp` (aliases `stvef-hm`, `holomatch`, `efhm`, `stvoyhm`) for
Holomatch multiplayer. Both run from the same `fntech3.exe`; the bare title key
`stvef` asks for a mode. This first slice registers the title, its installation
identity and startup contracts, and adds the MP3 decoding that both modes need
for their sounds and music.

Both profiles are experimental. Default builds reject them; `-Dstvef-development=true`
(the Windows helper's `-STVEFDevelopment`) lets the shared client and dedicated
server activate them for isolated probes. No Elite Force module, wire, renderer,
savegame or cinematic boundary exists yet, so no gameplay or interoperability
claim follows from this slice. Later slices are linked from the
[compatibility ledger](COMPATIBILITY.md#elite-force-gate-matrix).

## Evidence and provenance

Installed observations use a legitimate retail installation at
`E:\Games\Star Trek Voyager Elite Force` (Elite Force 1.20 with the Expansion
Pack content in `pak3.pk3`). No retail module, asset or payload is incorporated.

| File | Linker date | SHA-256 |
| --- | --- | --- |
| `stvoy.exe` (SP engine, `ST:V EF v1.20`) | 2001-04-16 | `8f7ccadb52b4230689c5b3275177f7ec7060a30bfaa3ade4e7b3fb0cda0dff74` |
| `stvoyHM.exe` (Holomatch engine) | 2001-04-18 | `5b5b6e82ce7ac0d720a45ab1fa9df32930c846d6c51847fa0c158ebc86487dff` |
| `efgamex86.dll` (SP game + cgame) | 2001-04-22 | `c159cba671544ddcc8694887c21bbeba13fead35bbf4488c08914f66695105a9` |
| `efuix86.dll` (SP menus) | 2001-04-17 | `35dfe9ca1a3ee20cc1d314013d1d5c281cd9ed2b68fc57b22af77bb2a0bedfb2` |

`efgamex86.dll` exports `GetGameAPI`, `dllEntry` and `vmMain` (ordinals 1-3);
`efuix86.dll` exports `GetUIAPI`. Neither SP binary imports a socket library:
the SP engine is loopback-only. The Holomatch QVMs are stored in the paks.

Reference material, kept distinct from implementation:

- A local pseudo-C decompilation of the retail `stvoy.exe` supplies engine-side
  facts; addresses are cited where used. It is observation, not incorporated text.
- Raven's official [Elite Force SP mod SDK](https://github.com/UberGames/SP-Mod-Source-Code/tree/04e0e618d1ee57a2919f1a852a688c03b1aa155d)
  at `04e0e618d1ee57a2919f1a852a688c03b1aa155d` is licensed under the non-free
  "STEF Game Source License". It is used only for interoperability facts such
  as names, ordinals and record layouts; no text from it is incorporated.
- [Lilium Voyager](https://github.com/zturtleman/lilium-voyager/tree/0f7dcd89d142c8b635ce2756a80cb62411731848)
  at `0f7dcd89d142c8b635ce2756a80cb62411731848` (GPL-2.0-or-later) is the
  reference for the Holomatch engine contract, and
  [VoyagerSP-Android](https://github.com/imjustadudegamer/VoyagerSP-Android/tree/0d8942e86a8469859da086f590875bcb66b4f4df)
  at `0d8942e86a8469859da086f590875bcb66b4f4df` (GPLv2 engine side) is consulted
  as a second, independently written single-player host. Its behavior differs
  from retail in several places recorded in later slices.
- MP3 decoding uses [minimp3](https://github.com/lieff/minimp3/tree/ea99364f61c14656440e8d77e9c233ccf3124633)
  (CC0 1.0) through `subprojects/minimp3.wrap`, pinned to that revision.

## Contracts established

| Contract | `stvef-sp` | `stvef-mp` |
| --- | --- | --- |
| Base directory / identity | `baseEF/pak0.pk3`: `default.cfg` 2,330 bytes, CRC `d7ab74a1`; `maps/borg1.bsp` 8,142,016 bytes, CRC `4e7d14d6` | Same pak and identity |
| Configuration | `efconfig.cfg`, fnTech3 dedicated `efconfig_server.cfg` | `hmconfig.cfg`, `hmconfig_server.cfg` (the name Lilium Voyager uses) |
| Protocol | 40, loopback only (`SVC_DirectConnect` compares the client's value with `0x28` at `0x424557`) | Retail 24; ioEF/Lilium 26 is registered as a separate family for a later codec |
| Module ABI | New Elite Force SP table ABI: `GetGameAPI`/`GetUIAPI` tables, the cgame's `dllEntry`/`vmMain` from the same library | QVM, plus legacy native for Holomatch mods |
| Native module names | `efgamex86.dll` for game and cgame, `efuix86.dll`; Win32 x86 only; read from the installation root, as retail does (`LoadLibrary` of `release/efgamex86.dll`, then `efgamex86.dll`, at `0x42e510`) | Quake III names (`qagamex86.dll` and so on) |
| Pure proof | Unavailable (no remote play) | QVM (`vm/cgame.qvm`, `vm/ui.qvm`) |
| BSP | IBSP 46 only, Quake III lump layout (all 67 retail maps checked) | Same |
| Steam | None: the title is not sold there, and discovery reports no path rather than guessing one | None |
| Window icon source | `stvoy.exe` | `stvoyHM.exe` |
| Userinfo defaults | `name Munro`, `legsModel`/`torsoModel hazard/default`, `headModel munro/default` (retail strings) | `name RedShirt`, `model munro/red`, `color`; no head-model key |
| Boot cinematic | `cinematic eflogo` (retail command string) | None |

One `baseEF/pak0.pk3` carries both modes. It therefore sets both modes'
installation marks, so automatic detection reports that a mode must be chosen,
exactly as an RTCW installation with both modes does. A pak mixed with another
title's identity remains ambiguous. Steam discovery now returns no path for a
profile without an AppID instead of formatting a null name.

## MP3 sound and music

Retail Elite Force stores 8,129 MP3 files in `pak0`-`pak3`: 44.1 kHz MPEG-1
Layer III, 8,021 mono and 108 stereo. 8,074 end in a Raven ID3v1 tag whose
comment carries `#UNCOMP <bytes>` and `#MAXVOL <n>`. Retail reads both fields
(`0x408c70`); `#UNCOMP` sizes the unpacked buffer and `#MAXVOL` scales
lip-sync, which the SP slice will consume.

Retail's unpack loops (`0x43bf70` sizing, `0x43c0a0` unpacking) decode from
the first frame sync while the remaining bytes are at least the first frame's
unpadded size. A final frame shorter than that is never decoded, and a leading
Xing frame is decoded as audio. The decoder initialization at `0x43bea0` passes
a mono-conversion flag for sound effects; streamed music keeps its channels.
minimp3's high-level API drops a Xing frame and would miss 2,048 retail sizes,
and a plain frame loop overshoots 6,009, so the codec uses its own frame walker
(`code/client/audio/codecs/snd_mp3_frames.h`) over minimp3's frame decoder.

`scripts/probe_stvef_mp3.py` decodes every retail MP3 through that walker and
compares the result with Raven's own tags. All 8,129 files decode, and all
8,074 `#UNCOMP` sizes are reproduced exactly: 8,071 as mono effects and the three
tagged music tracks as stereo streams. The report lives under
`.tmp/stvef/mp3-probe/result.json`; it lists sizes, CRCs and counts only, never
decoded audio.

The codec is registered only for Elite Force profiles. Retail Quake and
Wolfenstein engines decode no MP3, so their existing extension fallback must not
start finding `.mp3` files. The Meson option `-Dmp3=false` removes the codec.

## Installed observations

With `-Dstvef-development=true`, the shared dedicated server selects
`stvef-mp`, mounts all five `baseEF` paks in retail order (`pak0`-`pak3`, then
the installation's third-party `zSTVEFtex.pk3`), reads `hmconfig_server.cfg` and
exits normally. The command is:

```powershell
fntech3.ded.exe +set logfile 2 +set com_gameProfile stvef-mp +set fs_basepath "<Elite Force folder>" +set fs_homepath "<empty test home>" +set dedicated 1 +set sv_master1 "" +path +quit
```

No map is loaded: the Holomatch game module needs the QVM boundary that the next
slice adds.

## Tests

- `fntech3_game_profiles` covers both catalog entries: aliases, configuration
  names, protocol sets, module ABI admission, module names and capacity
  bounds, installation-root candidates, marks, mode-required detection, mixed
  identities, BSP formats and the Steam-less identity.
- `fntech3_stvef_mp3_frames` drives the frame walker with a scripted decoder:
  the first-frame size rule, padding, ID3v1/ID3v2 handling, mono mixdown,
  parameter changes, invalid requests and decoders that report impossible sizes.
- `bot_profile_defaults_tests.py` records that the SP profile starts with the
  bot library disabled; SP runs Raven's own navigation in its game module.

## Remaining gates

Everything beyond identity and audio decoding remains open for both modes: the
Holomatch QVM boundary, protocol 24 and 26 codecs, handshake, demos and pure
admission; the SP function-table host, cgame and UI tables, savegames, level
transitions, ambient sound sets, lip-sync and Bink cinematics; and Elite
Force's renderer and sound differences. The
[gate matrix](COMPATIBILITY.md#elite-force-gate-matrix) tracks them.
