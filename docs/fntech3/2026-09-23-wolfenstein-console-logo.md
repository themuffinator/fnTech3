# RTCW SP's and Enemy Territory's console logo layer

## Scope

Retail RTCW and Enemy Territory lay a logo over the console background. The
[RTCW MP console record](2026-09-23-rtcw-mp-console.md) added that layer, a
`cls.consoleLogoShader` drawn by the textured style, for `rtcw-mp` alone. It
left RTCW SP's logo open and had not examined retail `et.exe`. This record
covers what retail `WolfSP.exe` and `et.exe` register and draw, which
definitions and images the `rtcw-sp` and `wolfet` profiles resolve, and the
change that registers both logos.

## Observed

### Retail executables

Both executables were read with capstone. Addresses are virtual addresses in
each image. The `WolfMP.exe` column is from the MP record, and its two
addresses were read again for this record.

| | `WolfSP.exe` | `et.exe` | `WolfMP.exe` |
| --- | --- | --- | --- |
| Version string | `Wolf 1.32` | `ET 2.60b` | `Wolf 1.4-MP` |
| SHA-256 | `07d206f61da5c80562ff60d68c645bca349b31adb5d7e0e7f186d61ad46683c7` | `4d1326c5936b61c5f878fc017e4aa0abddf13e008139e31fce2f13d57b26243e` | `7cf99a938cf18ed2f5aecd0be542cbae8868959e0cce24b4b6eea08991e321f5` |
| `CL_InitRenderer` | `0x40cdc0` | `0x40e050` | `0x40d910` |
| Charset | `gfx/2d/bigchars` | `gfx/2d/consolechars` | `gfx/2d/hudchars` |
| Background | `console` | `console-16bit` | `console-16bit` |
| Logo layer | `console2` | `console2-16bit` | `console2-16bit` |
| Logo handle | `0xf00f88` | `0x156c1f0` | `0xfec718` |
| `Con_DrawSolidConsole` | `0x407300` | `0x407220` | `0x4075d0` |
| Half-open test | `fcomp` at `0x407387` | `comiss` at `0x4072c7` | `fcomp` at `0x407657` |
| Logo draw | `0x4073bc` | `0x4072f7` | `0x40768c` |

Each `CL_InitRenderer` calls `BeginRegistration`, then `RegisterShader` four
times: the charset, `white`, the background and the logo, in that order, into
four consecutive `cls` fields. The logo is the last. `et.exe` holds no
`console2`, `console`, `gfx/2d/bigchars` or `gfx/2d/hudchars` string, and
`WolfSP.exe` no `-16bit` name.

`WolfSP.exe` and `et.exe` draw the logo exactly as `WolfMP.exe` does:

- **Background.** `SCR_DrawPic( 0, 0, 640, frac * 480 - 2 )` with the
  background shader, when that height is at least 1. Below 1, neither the
  background nor the logo is drawn.
- **Logo.** When `frac` is at least 0.5, the colour is set to
  `{ 2 * frac, 2 * frac, 2 * frac, 1 }`, the logo is drawn with
  `SCR_DrawPic( 192, 70, 256, 128 )`, and the colour is cleared. An unordered
  `frac` skips it too: `WolfSP.exe` tests the `fcomp` result's C0 bit, and
  `et.exe` branches on `jb` after `comiss`.
- **Separator.** `WolfSP.exe` then fills `0, frac * 480 - 2, 640, 2` in black
  at 0.6 alpha, as `WolfMP.exe` does. `et.exe` fills
  `0, frac * 480 - 2, 640, 1.25` in `{ 0.75, 0.75, 0.75, 1 }`, and only while
  `frac` is below 1.

The GPL Enemy Territory source (`cl_main.c` `CL_InitRenderer`, `cl_console.c`
`Con_DrawSolidConsole`) says the same. It was read as reference observation
only; no implementation text is incorporated from it.

### Retail assets

In the Steam installations, read without extracting:

- **RTCW SP.** The `rtcw-sp` profile mounts `pak0.pk3` and the `sp_` paks, and
  refuses every `mp_` pak (`Com_GameManagerAllowsPak`). No `sp_` pak holds a
  shader script. The one `console2` it can see is in `pak0.pk3`
  `scripts/gfx.shader`
  (`8ae0367c8d4bd2dd8e014d912b9092311bce389d3fd670acf03f99338088fb9f`): one
  stage mapping `ui/assets/wolficonback4.tga`, `blendfunc blend`, no `rgbGen`.
  `sp_pak1.pk3` holds that image, a 256x128 32-bit TGA
  (`c837f6cfe0dbb744cceea03eefa6bdcaaefc21116f33a03f962cd77a40292dd5`). It is
  byte for byte `mp_pak1.pk3`'s `ui_mp/assets/wolficonback4.tga`, RTCW MP's
  logo. The `console2` that `mp_pak0.pk3` and `mp_pak4.pk3` define names that
  `ui_mp` copy, and `rtcw-sp` does not mount them.
- **RTCW SP background.** `pak0.pk3`'s `console` names
  `ui/assets/SMOKE-16bit.tga`. `sp_pak1.pk3` holds it as `.jpg`, which the
  renderer loads in its place.
- **Enemy Territory.** `pak0.pk3` `scripts/_unsorted.shader`
  (`0b02dff2da3ebee9d4c5708bb7b3efb897078561908d12b422e71776c858f905`) defines
  `console2-16bit` as one stage mapping `ui/assets/et_logo.tga`,
  `blendFunc GL_SRC_ALPHA GL_ONE`, `alphaGen const 0.1`, no `rgbGen`.
  `pak0.pk3` holds that image, a 512x256 32-bit TGA
  (`10905055aeff57777bf7b6456d65e958d4a7e189cf278339453188f8cd480193`). No
  other ET pak defines `console2-16bit` or `console-16bit`.

In this renderer, as in the original, a stage with no `rgbGen` whose source
blend is `GL_SRC_ALPHA` takes `identityLighting`
(`code/renderer/tr_shader.c`, `ParseStage`). Neither logo stage therefore
reads vertex colour. SP's alpha is the image's own, which peaks at 50 of 255,
and ET's is the image's scaled by the constant 0.1.

### This client before the change

`CL_InitRenderer` registered `console2-16bit` for `rtcw-mp` and no logo for
any other profile. `rtcw-sp` and `wolfet` drew their backgrounds with nothing
over them.

## Change

- **Registration.** `CL_InitRenderer` now registers `console2` for `rtcw-sp`
  and `console2-16bit` for `wolfet`, in `cls.consoleLogoShader`, as it already
  did `console2-16bit` for `rtcw-mp`. Every other profile still registers no
  logo, and every profile's charset and background are unchanged.
- **Drawing.** Nothing new. The textured style, `con_backgroundStyle 0`, draws
  a registered logo through `ConsoleLogoPlacement`
  ([`console_logo.hpp`](../../code/client/console_logo.hpp)) as it drew RTCW
  MP's. The logo appears while the console is at least half open. It sits in
  retail's 192,70 256x128 rectangle, scaled uniformly with the 640x480 screen
  and centred in the console's width. It is drawn white: retail's `2 * frac`
  colour is at least 1.0 whenever the logo is drawn, and no logo shader reads
  vertex colour. The default flat style draws no logo.

## Evidence

### Tests

[`tests/rtcw_mp_console_tests.py`](../../tests/rtcw_mp_console_tests.py) now
pins the three logo registrations: `console2-16bit` for `wolfet` and `rtcw-mp`,
`console2` for `rtcw-sp`, and none for any other profile. The placement harness
cites the SP and ET half-open tests beside MP's.
[`tests/wolfet_map_load_tests.py`](../../tests/wolfet_map_load_tests.py), which
pins ET's `gfx/2d/consolechars` and `console-16bit`, passes unchanged.

An export of `f65471e` with this change and nothing else was built for Windows
x86 with MSVC (`debugoptimized`, strict warnings, `-Drtcw-development=true`,
`-Dwolfet-development=true`, the GL renderer), with no warnings. Of the
project's 118 C++ meson tests, 116 pass and 2 skip for their environment: the
system OpenAL has no `ALC_SOFT_loopback`, and no retail Quake Live path is set.
pytest passes 1,832 tests, with 76 skipped. The
[evidence file](2026-09-23-wolfenstein-console-logo.json) holds these
results and every run below.

### Runtime

Two Windows x86 client builds came from an export of `9919af9`. As in the MP
record, both carried the water-ripple record's uncommitted four-line host-font
guard in `code/renderercommon/tr_font_stash.c`, without which console text is
`segoeui.ttf` on Windows. This change does not include it. The baseline build
had nothing else; the candidate added this change. They ran from the fixed
`.tmp/runtime/baseline/` and `.tmp/runtime/candidate/` locations.

| Build | `fntech3.exe` SHA-256 | `fnql_glx_x86.dll` SHA-256 |
| --- | --- | --- |
| Baseline | `d3c55cea5705777871f6993e2d286d289dda60a986b19c2bd07c168be86fded6` | `8fc1ccdc428f48f46f4c11b6677217e3d94ed02d421902ddb537f417f265c5a8` |
| Candidate | `d9296b65b5c6d2b2c40bbbae1813f15a28f27a588b38eb6f79b209a250e64f24` | the same |

Every run used an unswitched private desktop, a window, mouse, joystick and
sound off, and an isolated home, at the main menu. The legitimate Steam
installations were read-only, and each was unchanged after every run. The
console was opened with `toggleconsole` and captured with the engine's own
screenshot command. No input was sent.
[`scripts/probe_rtcw_console.py`](../../scripts/probe_rtcw_console.py) now takes
`--profile rtcw-sp` and `--profile wolfet` as well as MP. After an fnTech3
capture it runs `path`, `shaderlist`, `imagelist` and `which`, and reports what
they print.

| Profile | What the engine reported |
| --- | --- |
| `rtcw-sp` | Search path `sp_pak4` to `sp_pak1`, then `pak0.pk3`, with no `mp_` pak. `scripts/gfx.shader` found in `pak0.pk3`, `ui/assets/wolficonback4.tga` in `sp_pak1.pk3`, and `ui_mp/assets/wolficonback4.tga` not found. The candidate adds `console2`, explicit with one stage, and loads `ui/assets/wolficonback4.tga` at 256x128. No `R_FindImageFile` warning in either build |
| `wolfet` | Search path `pak2`, `pak1`, `pak0`, `mp_bin`. `scripts/_unsorted.shader` and `ui/assets/et_logo.tga` found in `pak0.pk3`. The candidate adds `console2-16bit`, explicit with one stage, and loads `ui/assets/et_logo.tga` at 512x256. No warning in either build |
| `rtcw-mp` | Both builds load `console2-16bit` and `ui_mp/assets/wolficonback4.tga`, as before this change |

To find where the logo was drawn, each capture's luminance was correlated with
the logo image's alpha, read from its pak in memory, over offsets around the
expected rectangle. A pair of captures was compared by their difference, and a
single capture by normalised cross-correlation of both after a high-pass
filter. The logo is dark in SP, so it correlates negatively there, and bright
in ET.

| Capture | Where the logo is |
| --- | --- |
| `rtcw-sp`, 640x480, textured | The candidate's difference from the baseline peaks at 192,70 at 256x128; 5% smaller or larger scores lower. In the candidate alone the correlation is -0.81 at 192,70, and the baseline has no match |
| `rtcw-sp`, 1280x720, textured | 448,105 at 384x192: centred and unstretched |
| `wolfet`, 640x480, textured | The difference peaks at 192,70 at 256x128. In the candidate alone, 0.49 at 192,70; the baseline has no match |
| `wolfet`, 1280x720, textured | 448,105 at 384x192 |
| Both, 640x480, default flat style | No logo-shaped difference |
| `rtcw-mp`, 640x480, textured | The same match at 192,70 in both builds |
| Retail `et.exe`, 640x480 | 0.11 at 192,70, the best of the searched offsets and about six standard deviations above their median |
| Retail `WolfSP.exe`, 640x480 | Its brighter 8x16 text covers about 30% of the rectangle. With that text left out, the match is at 191,71, and stays at 191,70 or 191,71 across text thresholds 120 to 180 and filter widths 3 to 5 |

Side by side at 640x480, each retail capture and its candidate capture show
the same logo, in the same place, at the same size. Retail runs used
`r_mode 3`, an isolated `fs_homepath`, PunkBuster, MOTD and auto-update off,
and `r_ignorehwgamma 1`. An earlier `et.exe` run without `cl_motd 0` resolved
`etmaster.idsoftware.com` on port 27951, the message-of-the-day server and
port in the GPL ET source, so the probe now turns both requests off. The
recorded runs resolved nothing. The remaining differences are outside this
change:

- **Text cell.** Retail uses the 8x16 small character; these builds used
  Quake Live's 6x12 cell for every profile.
- **Separator.** Retail draws the bars described above; this console draws
  `con_lineColor`, red by default.
- **Background brightness.** Beside the text, this client's SP smoke averages
  120 of 255 in luminance to retail's 74, as the MP record found for MP's.
  Its ET clouds average 29 to retail's 54.

## Open

- Retail's separator bars and 8x16 text cell, for both profiles.
- The console backgrounds' brightness: ET's clouds are darker here than in
  retail and RTCW's smoke lighter. The cause is not established.
- Whether RTCW and ET should default to the textured style.
- On Windows the bitmap charset shows only with the water-ripple record's
  host-font change.
