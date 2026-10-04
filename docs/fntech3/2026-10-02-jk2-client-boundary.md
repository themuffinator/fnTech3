# Jedi Outcast multiplayer: a separate client boundary

The developer client now routes Jedi Outcast's retail `vm/ui.qvm` and
`vm/cgame.qvm` through their own import numbers and frozen transfer records.
It forces the QVM loader for these modules. Unsupported imports stop with the
profile, ordinal and remaining boundary named. Single player refuses its
unimplemented client and menu boundaries by name.

This is Source, Unit and Asset evidence. No client game has drawn a map, no
font has been visually verified, and this record establishes no complete
client, renderer, audio, input or live-interoperability gate.

## Retail observations

The legitimate 1.02a installation at `E:/Games/Jedi Outcast` was read only.
`jk2mp.exe` SHA-256 is
`2f08dcf9a089cb66062766f4922fda63a9e553de893c174b497dfefeb7a1577a`.
Its cgame dispatcher at `0x4017a0` bounds ordinals at 300, indexes the byte
table at `0x4031a0`, and jumps through `0x402f14`. The UI dispatcher at
`0x40fb70` bounds them at 202, indexes `0x410894`, and jumps through
`0x4106fc`. Handler targets were read separately from the module call scan.

The shipped cgame QVM calls 133 distinct negative system-call addresses; the
UI calls 69. This counts their static call surface, not runtime execution.
Three differences prevent the Quake III dispatcher being used by number:

| Operation | Cgame | UI |
| --- | ---: | ---: |
| Remove command | 16 | absent from the measured UI table |
| Send client command | 17 | absent |
| Update screen | 18 | 28 |
| Collision map load | 19 | absent |
| Renderer configuration | 219 | 43 |
| Gamestate | 220 | absent |
| Snapshot | 222 | absent |
| User command | 225 | absent |
| Cvar register/update | 3/4 | 50/51 |
| Font register | 47 | 56 |
| Math floor/ceil | 110/111 | 107/110 |

The common console, filesystem, cvar, argument, scalar collision and math
services explicitly translate their ordinals. Cvar outputs are checked for
272 bytes, as established for the server module, and file-handle outputs for
four. Unknown ordinals cannot reach another profile's dispatcher. Required
Ghoul2, effects, ambient and unadapted renderer/audio imports are refused;
none returns fabricated success.

`CL_ParseServerMessage` at `0x40ed96` dispatches `(opcode - 1)` through the
eight entries at `0x40ee2c`. Opcode 8 selects `0x40edd7`, which checks the
cgame VM and invokes its export 24 with no argument and no payload read.
The preceding loop compares the opcode to EOF 9 at `0x40ed57`. Export 24 is
named `CG_MAP_CHANGE` by the later OpenJK contract; the notification and its
argument count were observed directly in this retail engine.

## Frozen records

| Record | Observed copy or writes | Frozen bytes |
| --- | --- | ---: |
| Gamestate | `0x401000`: `0x1519` words; 1,400 config offsets and 16,000 string bytes | 21,604 |
| Renderer configuration | `0x401020` and `0x4114d0`: `0xb11` words | 11,332 |
| UI client state | `0x410960`: scalars at 0/4/8; three 1,024-byte strings at 12/1036/2060 | 3,084 |
| Snapshot | `0x4010c0`: player at 44; entity count at 1412; 256 entities at 1416; command sequence at 77196 | 77,200 |
| User command | `0x401040`: seven words copied, 28-byte ring stride | 28 |

The snapshot player copy is `0x156` words, or 1,368 bytes. Entity ring
entries use the independently frozen 296-byte record. The unused snapshot
word at 77192 is initialized to zero. Outbound player transfers use the
lossless backing added with the JK2 wire adapter and preserve unnamed bytes.
The glconfig's whole size is observed; its equal Quake III field meanings and
spacing remain an inference, and its values are copied through the existing
field-by-field glconfig copier, never by exposing an internal record.

The gamestate adapter checks all offsets, termination, data capacity and
out-of-profile configstrings before publication. Snapshot adapters wrap the
parse-entity ring, clamp to retail's 256 entities, refuse invalid trajectories
and publish only complete transfers.

## Authored fonts through the engine's UI pipeline

The installation carries three `fonts/*.fontdat` files: `anewhope`, `ergoec`
and `ocr_a`, each 7,180 bytes, beside their texture atlases. Their point size
and height pairs decode as 17/20, 20/22 and 18/21. All three read through the
new bounded reader. The format has 256 28-byte glyphs followed by five short
metrics and alignment padding. Dimensions, advances, signed offsets,
baselines and atlas UVs are validated, including finite coordinates and
ordered UV bounds.

[OpenJK's pinned format declaration](https://github.com/JACoders/OpenJK/blob/1a6a643427aa347553e9073dac5570b33337c4d9/codemp/qcommon/qfiles.h)
names those on-disk fields. Its GPLv2 licence is compatible with fnTech3;
this use is format observation only. No external implementation text or
retail engine code was incorporated.

Cgame imports 47–51 and UI imports 56–60 now register, measure, count and draw
these authored fonts. The adapters bind the atlas with fnTech3's
`RegisterShaderNoMip` and submit glyphs through its HUD or menu layout and
`DrawStretchPic` paths. Handles stay module-local and registration refreshes
the shader after a renderer restart. Colour escapes, authored baselines,
character advances, clipping, blink and shadow requests are interpreted in
engine code. These are adapters of game data into FnQ3/FnQL presentation,
not a replacement retail renderer.

Only the single-byte atlas lane is implemented. Asian language services,
localized font overrides and the separate String Package service still
require their own evidence. No retail-exact presentation option was added.
Per-pixel world lighting, shadows, static lighting, engine sidecars, spatial
audio, EFX, zones, occlusion and HRTF have not been exercised by this slice;
world and audio adapters must adopt those engine features when added.

## Validation and remaining work

At source commit `a1251ede2c382190b18c154b45357f6979d8171c`, with the task's
uncommitted changes, Windows x86 standalone tests passed:

- `tests/jk2_client_adapter_tests.cpp`: record offsets/sizes, last configstring,
  malformed offset/termination/capacity rollback, lossless player bytes,
  snapshot ring wrap, entity clamp and invalid-trajectory rollback.
- `tests/jk2_font_tests.cpp`: signed metrics, UV decoding, text metrics,
  truncated input, nonfinite/out-of-range coordinates, invalid dimensions and
  transactional failure; the same executable accepted all three retail fonts.
- MSVC `/Zs` checks of `cl_cgame.cpp` and `cl_ui.cpp` passed with the configured
  win32-dev compile definitions. No runtime binary was linked or replaced.

Probe logs are under `.tmp/jk2-client-audit/`. Extracted font files were
removed immediately after the read-only asset probe.

The remaining client work includes menu/key and language services, Raven
render entities and views, Ghoul2 mesh/skeleton submission, effects,
ambient-set definitions and audio-channel semantics. The current RBSP
translator feeds collision only: all renderers still need Raven world data
adapted into their own lighting paths, with the four authored light styles
and light-array indexing preserved. A client map's runtime screenshot must
come from the engine screenshot command after those paths work. Single
player's `GetGameAPI` client/campaign/save boundary remains separate.
