# RTCW MP's console charset and background

## Scope

The `rtcw-mp` profile drew its console with Quake III's art:
`gfx/2d/bigchars` for text and `console` for the background. Retail
`WolfMP.exe` uses neither. This record covers what retail registers and
draws, the change that registers it, and the catch that the water-ripple
record (`2026-09-23-rtcw-water-ripples.md`) left open. RTCW MP's cgame
registers the same charset itself, and HUD aspect correction named that shared
handle after the client, so the shipped `round_timer` rule would have stopped
matching.

## Observed

### Retail executables

Both retail executables were read with capstone. Addresses are virtual
addresses in each image.

| | `WolfMP.exe` | `WolfSP.exe` |
| --- | --- | --- |
| SHA-256 | `7cf99a938cf18ed2f5aecd0be542cbae8868959e0cce24b4b6eea08991e321f5` | `07d206f61da5c80562ff60d68c645bca349b31adb5d7e0e7f186d61ad46683c7` |
| `CL_InitRenderer` | `0x40d910` | `0x40cdc0` |
| Charset | `gfx/2d/hudchars` | `gfx/2d/bigchars` |
| Background | `console-16bit` | `console` |
| Logo layer | `console2-16bit` | `console2` |
| `Con_DrawSolidConsole` | `0x4075d0` | `0x407300` |
| Logo draw | `0x40768c` | `0x4073bc` |

Each `CL_InitRenderer` calls `BeginRegistration`, then `RegisterShader` four
times: the charset, `white`, the background and the logo. The handles go to
four consecutive `cls` fields (`0xfec70c` to `0xfec718` in MP). Neither
executable holds the other's names: `WolfMP.exe` has no `gfx/2d/bigchars`,
`console` or `console2` string, and `WolfSP.exe` has no `gfx/2d/hudchars`
or `-16bit` name.

Both `Con_DrawSolidConsole`s do the same thing:

- **Background.** `SCR_DrawPic( 0, 0, 640, frac * 480 - 2 )` with the
  background shader, when that height is at least 1.
- **Logo.** When `frac` is at least 0.5 (`fcomp` at `0x407657` in MP, and
  skipped when the comparison is unordered), the colour is set to
  `{ 2 * frac, 2 * frac, 2 * frac, 1 }`. The logo is drawn with
  `SCR_DrawPic( 192, 70, 256, 128 )` and the colour is cleared. In the GPL
  source an open console rests at 0.5 and is drawn at 1.0 only when
  disconnected with no menu up, so that colour is 1.0 or 2.0.
- **Separator.** `SCR_FillRect( 0, frac * 480 - 2, 640, 2 )` in black at 0.6
  alpha.

The GPL RTCW MP and SP sources say the same (`cl_main.c` `CL_InitRenderer`,
`cl_console.c` `Con_DrawSolidConsole`). They were read as reference
observation only; no implementation text is incorporated from them.

Retail's RTCW MP cgame, `cgame_mp_x86.dll`
(`a21378f24c6476335e81b0d72e3c684074e8a0a20a399062b9a3d64f4b542221`, the same
file in `Main` and in `mp_bin.pk3`), contains `gfx/2d/hudchars` and no
`gfx/2d/bigchars`. The GPL cgame registers `hudchars` for both its charset and
its menu charset. RTCW SP's GPL cgame registers `hudchars` too, but SP's
client charset is `bigchars`, so the two never share a handle there.

### Retail assets

In the Steam installation:

- `mp_pak0.pk3` `scripts/nerve-jpw.shader` defines `console-16bit` as two
  scrolling stages of `ui_mp/assets/SMOKE-16bit.tga`, at constant greys 0.3
  and 0.2. It defines `console2-16bit` as `ui_mp/assets/wolficonback4.tga`,
  alpha-blended, with no `rgbGen`. `mp_pak1.pk3` holds both images, the smoke
  as `.jpg`.
- The `console` shader MP can see, in `mp_pak0.pk3` and `mp_pak4.pk3`
  `scripts/gfx.shader`, names `gfx/colors/black.tga` and
  `ui_mp/assets/SMOKE.tga`. No pak holds either. `pak0.pk3`'s `console` names
  `ui/assets/SMOKE-16bit.tga`, which only `sp_pak1.pk3` holds, and the
  `rtcw-mp` profile does not mount `sp_` paks.
- `gfx/2d/hudchars.tga` is in `pak0.pk3` and `mp_pak0.pk3`, the same 256x256
  file in both.

### This client before the change

- `CL_InitRenderer` registered `gfx/2d/bigchars` and `console` for
  `rtcw-mp`, and no logo layer in any profile. The log said
  `R_FindImageFile could not find 'gfx/colors/black.tga' in shader 'console'`.
- `con_backgroundStyle` defaults to `1` in every profile: a flat `24 0 0` fill
  at 0.8 opacity. The registered background is drawn only with
  `con_backgroundStyle 0`. Under `rtcw-mp` that drew a placeholder, not smoke.
- At `9e1841d`, every Windows profile but `ql` wrote console text through the
  host font lane in `segoeui.ttf`, so those consoles showed no bitmap charset.
  The water-ripple record's host-font change removes that lane outside Quake
  Live.

### HUD aspect correction

`CL_HudLookupShaderName` reported a draw whose handle was the client's
charset, `white` or console background as `charset`, `white` or `console`,
before it looked at the cgame's names. The renderer keeps one handle per
shader name, so a client and a cgame that register the same name share it.

- **Q3.** The cgame registers `gfx/2d/bigchars`, the client's charset.
- **RTCW MP.** The cgame registers `gfx/2d/hudchars`. While the client used
  `bigchars` the two never met, and the shipped `round_timer` rule
  (`pkg/games/rtcw-mp/main/fntech3-hud.json`) matches `gfx/2d/hudchars` by
  name.

Registering `hudchars` for the client alone would have renamed every RTCW MP
HUD text draw `charset`, and `round_timer` would have stopped matching.
Preferring the cgame's name instead would have renamed Q3's text draws
`gfx/2d/bigchars` in dumps, and broken rules written against `charset`.

## Change

- **Registration.** For `rtcw-mp`, `CL_InitRenderer` registers
  `gfx/2d/hudchars`, `console-16bit` and, in a new `cls.consoleLogoShader`,
  `console2-16bit`. Every other profile registers the same names as before,
  and no logo.
- **Logo.** The textured style draws the logo after the background and before
  the separator and text, placed by `fntech3::client::ConsoleLogoPlacement`
  ([`console_logo.hpp`](../../code/client/console_logo.hpp)).
  - It is drawn only while the console is at least half open.
  - It is retail's 256x128 rectangle at 192,70, scaled uniformly with the
    640x480 screen and centred in the console's width. On a 4:3 screen that
    is retail's rectangle. On any other the logo keeps its shape instead of
    stretching.
  - It is drawn in white. Retail's `2 * frac` is at least 1.0 whenever the
    logo is drawn, and retail's shader has no `rgbGen`, so it ignores vertex
    colour. This renderer converts vertex colour to bytes without clamping,
    so 2.0 would overflow.
  - The flat style draws no logo.
- **HUD names.** Aspect correction still reports a shared handle by the
  client's name, so dumps and rules written against `charset`, `white` or
  `console` are unchanged. A rule's `shader` pattern now also matches the name
  the cgame registered for that handle.
  - Q3's text draws answer to `gfx/2d/bigchars` as well as `charset`. RTCW
    MP's answer to `gfx/2d/hudchars` as well as `charset`.
  - An RTCW MP `hudchars` draw now always counts as text, as the client's
    charset always did. No shipped RTCW MP rule tests `textLike`.
  - Of the shipped patterns, only `gfx/2d/hudchars` can match a charset
    name. It appears in RTCW MP's `round_timer` and in RTCW SP's
    `pickup_text`, where the handle is not the client's charset. Every
    shipped rule therefore keeps the matches it had.

## Evidence

### Tests

- [`tests/hud_aspect_tests.py`](../../tests/hud_aspect_tests.py) runs the real
  `cl_hud.cpp`. The new cases put the cgame's `gfx/2d/bigchars` (Q3) and
  `gfx/2d/hudchars` (RTCW MP) on the client charset's handle. Under the
  shipped presets, Q3's lands on `upper_right_text` and MP's on `round_timer`.
  A rule naming `charset` still matches, a `GFX/2D/*` rule matches the cgame's
  name, and a dump reports `charset`. Against the previous `cl_hud.cpp`, MP's
  round timer is drawn at the centred 571.25 instead of 11.25, and Q3's
  `GFX/2D/*` rule does not match (1235 instead of 1795). RTCW SP passes either
  way.
- [`tests/rtcw_mp_console_tests.py`](../../tests/rtcw_mp_console_tests.py)
  runs `ConsoleLogoPlacement` at 640x480 and 1280x960, at 1920x1080 across the
  window and across its 4:3 middle, and at 1280x1024. It also runs it below
  half open, with an unordered fraction and with no screen size. It checks
  that only `rtcw-mp` registers the MP art, that the other profiles' names are
  unchanged, and that only the textured style draws the logo.
- Existing pins pass unchanged, including
  [`tests/wolfet_map_load_tests.py`](../../tests/wolfet_map_load_tests.py)
  (ET's `gfx/2d/consolechars` and `console-16bit`).
- In a clean worktree at `9e1841d` with only this change (Windows x86 MSVC
  `debugoptimized`, strict warnings, `-Drtcw-development=true`), the build
  has no warnings. Of the project's 118 C++ meson tests, 116 pass and 2 skip
  for their environment: the system OpenAL has no `ALC_SOFT_loopback`, and no
  retail Quake Live path is set. pytest passes 1,823 tests, with 76 skipped.
  Rebased onto `c937bdb` it is the same: no warnings, the same C++ result,
  and 1,828 pytest passes with 76 skipped. A run on `8849e8d` in between
  failed 19 release-packaging tests, because this record then linked the
  uncommitted water-ripple record. It now names that record without a link.

### Runtime

Two Windows x86 client builds came from that worktree. With the host font lane
on, console text is `segoeui.ttf` in both, and no charset can be seen. Both
therefore also carried the water-ripple record's four-line host-font guard in
`code/renderercommon/tr_font_stash.c`, which this change does not include.
The "before" build had nothing else; the "after" build added this change.

| Build | `fntech3.exe` SHA-256 | `fnql_glx_x86.dll` SHA-256 |
| --- | --- | --- |
| Before | `002f81aa66c1e8593b83bdf85ab7a40b07f4605a8785290550810977f8d0bc7a` | `9625805c1d8de6f0c062c8c379a269d1e645516596a6fef694b997cba86f0838` |
| After | `815b35134c56bdff2c63a1315415af54a3390a9d05f83cfd3247ef09d017e8e8` | `6fe7957fb5643b886a9e8bfc66dc539efa55566c08f3ff52286cb5c4941bca3e` |

The renderer's source is the same in both. Its hash differs only because it
was linked again.

Every run used an unswitched private desktop, a window, mouse, joystick and
sound off, and an isolated home. The legitimate Steam RTCW installation was
read-only, and its inventory was unchanged after every run. The console was
opened with `toggleconsole` from a config and captured with `screenshotPNG`.
On `mp_beach`, the cgame's `CloseLimboMenu` command closed the limbo menu
first. No input was sent.
[`scripts/probe_rtcw_console.py`](../../scripts/probe_rtcw_console.py) runs
each case; the [evidence file](evidence/2026-09-23-rtcw-mp-console.json)
holds the commands, hashes and outcomes.

| Run | Before | After |
| --- | --- | --- |
| Main menu, 640x480, `con_backgroundStyle 0` | `bigchars` over a placeholder; the missing-image warning above | `hudchars` over the `console-16bit` smoke; the logo sits where retail's does, in its 192,70 256x128 rectangle; no warning |
| Main menu, 1280x720, `con_backgroundStyle 0` | `bigchars` over a placeholder | The logo centred and unstretched, placed at 448,105, 384x192 |
| Main menu, 640x480, default flat style | `bigchars` on the flat fill | `hudchars` on the same fill; no logo |
| `mp_beach`, 1280x720, limbo closed | Round timer drawn from x 11 to 53 | Identical, pixel for pixel |
| `cl_hudDump` on `mp_beach` | The text group at 5,48, 32x16 is `gfx/2d/hudchars` | The same group is `charset` |

The round timer at the window's left edge is `round_timer` matching. Without
it the timer would sit in the centred 4:3 area, about 160 pixels to the
right.

Retail `WolfMP.exe` ran the same way (`r_mode 3`, isolated `fs_homepath`,
PunkBuster off), with `toggleconsole` and its own `screenshot` on its command
line. Its first attempts exited at once with status 5 and wrote no log; after
the user restarted Steam it exited 0. At 640x480, its capture (SHA-256
`5c946afabf84381d6b37771e2052cd3d5d33b2d7659d9d14f5fc82825cb5869f`) and the
after build's show the same charset, the same smoke background, and the logo
in the same place at the same size. The remaining differences are outside this
change:

- **Text cell.** Retail uses the 8x16 small character; in these builds this
  console used Quake Live's 6x12 cell.
- **Separator.** Retail draws a black bar at 0.6 alpha; this console draws
  `con_lineColor`, red by default.
- **Version label.** It differs, and this console adds a scrollbar.
- **Smoke brightness.** This client's smoke is lighter. Retail ran with
  `r_ignorehwgamma 1`, and the two renderers' gamma handling differs.

## Open

- `con_backgroundStyle` defaults to the flat style in every profile, so RTCW
  MP shows its retail art only with `con_backgroundStyle 0`. Whether RTCW
  should default to the textured style is a presentation choice this record
  does not make.
- RTCW SP's logo layer. Retail `WolfSP.exe` draws `console2` exactly as MP
  draws its logo, and so does retail `et.exe` with its `console2-16bit`. Both
  are registered and drawn since the
  [console logo record](2026-09-23-wolfenstein-console-logo.md).
- Retail's separator bar, version label and 8x16 text cell.
- On Windows the charset shows only with the water-ripple record's host-font
  change.
