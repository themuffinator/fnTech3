# Each profile's large client text

## Scope

`SCR_DrawStringExt` in `code/client/cl_scrn.cpp` draws the client's own
large strings: the demo-recording notice, the spatial-audio debug overlay, and
the text and cursor of a big edit field (`Field_BigDraw`). FnQL, the Quake
Live baseline, draws them through Quake Live's host font 2.

On 2026-09-23 the renderer's host-text lane became the `ql` profile's alone
(see `docs/fntech3/2026-09-23-rtcw-water-ripples.md`). Every other profile
then reached the client's own fallback in `RE_DrawScaledText`. That fallback
was written for Quake Live, for when its fonts are missing: each glyph is 0.6
of its height wide, drawn above a baseline at `y`, with no shadow. Before that
change, on Windows, those profiles drew this text in `segoeui.ttf`. Linux
builds already used the fallback. Neither matched any original engine. The
recording notice also sat where Quake Live puts it, 420 lines down.

## Observed

Each retail executable was read with capstone. Addresses are virtual
addresses in each image. `n` is the length of the formatted notice,
`RECORDING %s: %ik`, which every engine below that has one uses unchanged.

| Executable | SHA-256 | Notice call | Notice |
| --- | --- | --- | --- |
| `quake3.exe` Q3 1.32 | `80b0add0e014b863cf47cab6c997e0a54939236219a56910b7a3eb77dc2d44d4` | `0x412537` to `0x411c40` | `(80 - n) * 4`, `20`, size 8, forced white |
| `WolfSP.exe` Wolf 1.32 | `07d206f61da5c80562ff60d68c645bca349b31adb5d7e0e7f186d61ad46683c7` | `0x4103e8` to `0x40ffb0` | same as `quake3.exe` |
| `WolfMP.exe` Wolf 1.4-MP | `7cf99a938cf18ed2f5aecd0be542cbae8868959e0cce24b4b6eea08991e321f5` | `0x412c13` to `0x4127f0` | `5`, `470`, size 8, forced white |
| `stvoyHM.exe` ST:V HM v1.20 | `5b5b6e82ce7ac0d720a45ab1fa9df32930c846d6c51847fa0c158ebc86487dff` | `0x4122e8` to `0x411a90` | same as `quake3.exe` |
| `stvoy.exe` ST:V EF v1.20 | `8f7ccadb52b4230689c5b3275177f7ec7060a30bfaa3ade4e7b3fb0cda0dff74` | none | no `record` command and no demo path |
| `et.exe` ET 2.60b | `4d1326c5936b61c5f878fc017e4aa0abddf13e008139e31fce2f13d57b26243e` | none | no notice string; names `cl_demorecording`, `cl_demofilename` and `cl_demooffset` |
| `quakelive_steam.exe` | `c926fe9f6c851e00b3b9332e88903ad01f28fdd60454873891c0158f5ded1299` | `0x4be022` to `0x4bddf0` | `(80 - n) * 4`, `420`; or `REC` at `9`, `477` beside its icon |
| `quake3.exe`, February 1999 IHV test | `e9fcaa0a5682e7f07d540aacee08cdb77f3ea40190719f5cecf31109a5135d37` | `0x407701` to `0x4075b0` | `(40 - n) * 8`, `40`, its 16-pixel big string, alpha 1 |
| `jamp.exe` | `e8062d3948ff05917b48e47d8eec0ee43bcc6fb03924410712412404ddfee28b` | `0x422c38` to `0x422940` | same as `quake3.exe` |
| `jk2mp.exe` JK2MP: v1.02a | `2f08dcf9a089cb66062766f4922fda63a9e553de893c174b497dfefeb7a1577a` | `0x40f381` to `0x40f010` | same as `quake3.exe` |
| `jasp.exe` | `9351a8de230fcd577e2b474889d08f38982450306767a6823dd659482566d6ce` | none | no `record` command and no demo path |
| `jk2sp.exe` JK2: v1.02a | `0d3c635ee5b20ac32fc957e00cf21e65ebc1584112b11f6ae026706dcc0d0e0c` | none | no `record` command and no demo path |
| `alice.exe` | `8aa22917b7707f0e1a7c4dbe9a3c38332b1873ae2059174ac5329de26921fc18` | none | records `demos/%s.dm3` and has no notice string |

The large-string routines of `quake3.exe` (`0x411c40`), `WolfSP.exe`
(`0x40ffb0`, cell `0x410110`), `WolfMP.exe` (`0x4127f0`, cell `0x412950`) and
`stvoyHM.exe` (`0x411a90`) do the same thing:

- The shadow comes first. Every character is drawn at `x + 2`, `y + 2` in
  black, carrying the caller's alpha, skipping `^` escapes.
- Then the text, from the top-left of `x`, `y`. A `^` escape sets
  `g_color_table[(c - '0') & 7]` with the caller's alpha unless the color is
  forced.
- Each character is a `size` by `size` cell of `cls.charSetShader`, scaled
  from 640x480 to the screen. The pen then moves to `(int)(xx + size)`. A
  space moves it without a cell, and a cell with `y < -size` is culled.
- The texture cell is 1/16 of the sheet in both directions, except that
  `stvoyHM.exe` adds 0.03125 (`0x4b7478`) for its width. Elite Force's glyphs
  fill the left half of each `charsgrid_med` cell, and the screen cell stays
  square.

The IHV build draws its notice with `0x4075b0`, which calls `0x407480`. That
routine also draws a shadow at `x + 2`, `y + 2` and 16-pixel cells scaled from
640x480 (`0x407300`). Its escapes are ESC (`0x1b`) and a digit from `0` to
`7`, not `^`.

The Enemy Territory GPL source agrees with `et.exe`. Its
`SCR_DrawDemoRecording` only sets `cl_demooffset`. Its cgame draws
` demo %s: %ik ` from `cl_demorecording`, `cl_demofilename` and
`cl_demooffset`.

## Change

- `SCR_DrawStringExt` draws Quake III's large string in every profile but
  `ql`, through `SCR_DrawCharsetStringExt` and `SCR_DrawChar`, from
  `cls.charSetShader` with `CL_CharsetCellWidth`. `noColorEscape` draws the
  escape characters, as in the FnQuake3 reference. The `ql` path is unchanged.
  The empty-string and missing-color checks still run first for every
  profile.
- A big edit field's cursor is the charset's own cursor cell, 10 or 11 in
  overstrike, outside `ql`, as Quake III and FnQuake3 draw it. `ql` keeps its
  `|` and `_`. Its host face has no glyph for those control bytes.
- `SCR_DrawDemoRecording` outside `ql` places the notice through
  `SCR_DemoRecordingNotice`:
  - centred at `20` for `q3`, `rtcw-sp` and `stvef-mp`, and for any profile
    without a row;
  - `5`, `470` for `rtcw-mp`;
  - no notice for `wolfet` or `stvef-sp`.
  Quake Live's two notices are unchanged.
- `RE_DrawScaledText`'s charset fallback is unchanged. Only `ql` reaches it
  now, from its module text syscalls and from `SCR_DrawStringExt`.

## Evidence

- `tests/large_text_profile_tests.py` compiles the client's own
  `SCR_DrawStringExt`, `SCR_DrawBigString`, `SCR_DrawDemoRecording` and
  `Field_BigDraw` with the renderer and host text stubbed. It runs them for
  every profile id, and with no profile selected, at 1600x900. Outside `ql` it
  checks each cell's position, size, texture cell and color. It covers the
  shadow, skipped, literal and forced escapes, big strings, culling, each
  notice placement, the field cursor, and missing input. Under `ql` it checks
  that the host-text calls are exactly the old ones. Against the previous
  source every non-`ql` case fails and every `ql` check passes.
- The commit's tree, `9e1841d` with this change alone, was exported and built
  for Windows x86 with MSVC, `debugoptimized`, strict warnings (`/WX`) and the
  RTCW, Elite Force and Enemy Territory development options. It built without
  a warning. `fntech3.exe` had SHA-256
  `9c712db488647f0e95fa6b1aa37d0dc8de2c19bf561a48de1d3f8542dd9e50f3` and
  `fnql_glx_x86.dll`
  `269000c6dfcf275282bf3e2b9e403e96d2f06994c69a8e8bda345aebf80bea22`.
- Runtime, same build. Each run used a private desktop and a windowed
  1280x960 window, where a virtual pixel is two. Mouse input, sound, Steam
  integration and the web UI were off, and the home directory was isolated.
  After `+devmap` (`+spdevmap` for RTCW SP), the active action ran
  `record largetext`, waited, took `screenshotPNG`, then `stoprecord` and
  `quit`. `cl_drawRecording` was 1. Every run exited 0, and every
  installation's file inventory was unchanged.
  - `q3`, Steam Quake III on `q3dm17` and `q3dm1`: the notice's glyph pixels
    span x 411-868, y 43-54, which is 29 `bigchars` cells from (408, 40). On
    `q3dm1` the black shadow shows down and to the right.
  - `rtcw-mp` on `mp_beach`, after `CloseLimboMenu`: x 13-486, y 943-954,
    cells from (10, 940).
  - `stvef-mp` on `hm_voy1`: x 410-867, y 41-54. Each `charsgrid_med` glyph
    is drawn from half a texture cell into a square screen cell.
  - `stvef-sp` on `borg1` recorded a demo and drew no notice.
  - `ql` on `campgrounds` (`ffa`) drew its notice in `droidsansmono`, on the
    baseline at 840 from x 409, as before.
  - `rtcw-sp` on `escape1` recorded a demo, but the capture shows the mission
    briefing. The UI draws it after the notice, as retail does. Dismissing it
    needs a click, which these probes do not send.
  - This tree predates the host-lane gate. Every non-`ql` run logged
    `QL fonts: using Windows fallback`, so it drew charset text with the host
    lane ready.

## Open

- These rows wait until the profiles exist in a commit: `q3-ihv` at
  `(40 - n) * 8`, `40` in 16-pixel cells, with ESC escapes; no notice for
  `jka-sp`, `jk2-sp` and `alice`. `jka-mp` and `jk2-mp` already get their
  engines' placement, the default.
- Closed since: Enemy Territory's cgame draws its own notice from
  `cl_demorecording`, `cl_demofilename` and `cl_demooffset`. This client set
  none of them, so ET showed no notice at all. It now keeps them as `et.exe`
  does; see the [recording cvars record](2026-09-23-wolfet-demo-recording-cvars.md).
- The notice names `demos/<name>`, truncated (`clc.recordNameShort`, from
  Quake3e). The original engines name the demo alone.
- A notice is hidden during a single-player demo (`ui_recordSPDemo`) in every
  profile. `quake3.exe`, `jamp.exe` and `jk2mp.exe` test for that;
  `WolfSP.exe`, `WolfMP.exe` and `stvoyHM.exe` do not.
- Escapes use fnTech3's extended color table (`ColorIndexFromChar`), as the
  small string does. `^0` to `^7` match the retail engines, but other
  characters do not. Quake III, RTCW and Elite Force wrap with `& 7`. Enemy
  Territory has 32 colors, and its `^*` restores the caller's color; here it
  reads as white.
- RTCW MP's own charset, `gfx/2d/hudchars`, is registered since the
  [console record](2026-09-23-rtcw-mp-console.md), so its large text uses it.
