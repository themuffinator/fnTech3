# FOV, HUD, Menu And Cinematic Aspect Handling

Most of fnTech3's games were written for 4:3 screens. How much the engine may
correct on a widescreen display depends on who owns each transform: where a
game's modules already handle widescreen, as Quake Live's do, the engine never
applies a second correction.

| Game | World FOV | HUD | Menus | Cinematics |
| --- | --- | --- | --- | --- |
| Quake III Arena | `r_fovCorrection` | `cl_hudAspect`, HUD rules | `cl_menuAspect` | `cl_cinematicAspect` |
| Return to Castle Wolfenstein, both modes | `r_fovCorrection` | `cl_hudAspect`, HUD rules | drawn by the game as retail draws them | always stretched; single player adds its letterbox bars |
| Quake Live | the retail modules' Hor+, completed by `r_fovCorrection` beyond its four shapes | owned by the retail modules | `cl_menuAspect`, default `1`, as retail draws them | `cl_cinematicAspect` |
| Enemy Territory | `r_fovCorrection` | `cl_hudAspect`, HUD rules; its own full-screen panels as described below | drawn by the game as retail draws them | `cl_cinematicAspect` |
| Elite Force, both modes | `r_fovCorrection` | `cl_hudAspect`, HUD rules; single player's client game is given the 4:3 frame | `cl_menuAspect` | `cl_cinematicAspect` |
| Alice | `r_fovCorrection` | `cl_hudAspect`, HUD rules for its 3D HUD | the engine's own `.urc` menus | `cl_cinematicAspect` |
| F.A.K.K.2 | `r_fovCorrection` | `cl_hudAspect`, HUD rules for its HUD menus | the engine's own `.urc` menus | `cl_cinematicAspect` |
| Medal of Honor | `r_fovCorrection` | owned by the retail modules, which place it for any screen | the engine's own `.urc` menus | `cl_cinematicAspect` |

The IHV Test has no client, and Jedi Academy and Jedi Outcast have none yet;
the Q3Test releases have neither correction. The correction cvars exist only
in the games that use them: `r_fovCorrection` is registered in every game in
the table, and `cl_hudAspect`, `cl_hudDump` and `hud_reload`
in every one except Quake Live and Medal of Honor. Setting them in a config
for another game changes nothing.

## World FOV and HUD

fnTech3 restores FnQuake3's optional world FOV and HUD correction for Quake
III Arena and both RTCW profiles, and applies it to every other game whose
modules assume a 4:3 screen.

- `r_fovCorrection 1` (default) preserves the vertical framing of a horizontal
  FOV authored for 4:3, widening or narrowing it for the viewport. For
  example, 90 degrees at 4:3 becomes about 106.26 degrees at 16:9. A view the
  game cuts from its screen in one direction only, such as a letterbox, keeps
  the framing that cut has on a 4:3 screen, so a 4:3 display is never
  changed. All three renderers use the same policy, including stretched
  render targets and render-scale modes that preserve aspect. Use `0` for
  mods that already calculate an aspect-correct FOV.
- Quake Live's modules fit their own view to the screen, but only four
  shapes: they keep a 4:3 vertical FOV and widen the horizontal one for the
  `r_aspectRatio` preset the engine publishes, which is 4:3, 5:4, 16:10 or
  16:9, whichever the screen is at least as wide as. On any other screen
  retail spreads that preset's field across it: at 21:9, `cg_fov 100` gives
  115.6 degrees, the 16:9 value, across a 21:9 view, stretching the world by
  a third. With `r_fovCorrection 1`, the renderer widens that horizontal FOV
  by the screen's shape over the preset's, to 128.8 degrees at 21:9. It keeps
  QL's vertical FOV and underwater wobble and changes nothing on a preset's
  own shape. QL's markers and teammate icons are projected through the
  renderer, so they stay on their targets.
- `cl_hudAspect 1` (default) remaps legacy stretched HUD geometry into uniform
  4:3 space, centered unless a rule changes its placement. Use `0` to retain
  the module's unmodified HUD coordinates.
- Fullscreen fades and backgrounds remain full-frame by default. HUD 3D model
  viewports follow the same placement rules as images and retain their authored
  FOV. Menu scenes also keep their authored FOV. RTCW gradient pictures and MP
  rotated pictures follow HUD layout while retaining their colors and UVs.
- Enemy Territory draws several screens of its own through its HUD's calls,
  each laid out for a 4:3 screen. While the scoreboard (`+scores`) or the
  expanded command map (`+mapexpand`) is held, and a moment after, and at the
  end-of-round debriefing, the HUD rules pause and the whole frame is one
  centered 4:3 screen. The limbo panel, which holds a world view of its own,
  is drawn stretched as retail drew it.
- Elite Force single player projects points of the world onto its 2D, such as
  the brackets around a target, with the view it computed itself. With
  `cl_hudAspect 1` its client game is told its screen is the 4:3 frame, so
  that view is a 4:3 one; the engine then widens the world and places the 2D,
  and both agree. The setting is read when the client game starts, so
  changing it takes effect at the next map or `vid_restart`.
- Alice's HUD is the engine's: its gauges and cards are 3D models, each drawn
  in a view of the whole screen. Each view becomes a 4:3 screen's, placed by
  the rules, and each model keeps its 4:3 size.
- F.A.K.K.2's HUD is four of the engine's own `.urc` menus -- `healthhud`,
  `weaphud`, `dualweaphud` and `bosshud` -- drawn in the menus' 640x480
  units. Only those four follow the HUD rules: each widget is first cut to
  its menu and the screen as every menu's is, then placed, and a hand's
  weapon model follows its ring. Every other menu, the inventory and the
  client game's own 2D layer stay stretched to the screen.

These controls are independent: HUD placement does not change world FOV.
They operate on engine-owned copies after module ABI conversion. The
[widescreen record](fntech3/2026-09-25-widescreen-hud-and-fov.md) holds each
title's evidence and what is not covered.

## HUD Rules and Diagnostics

`fntech3-hud.json` is the preferred rule file; `fnq3-hud.json` remains a
compatible fallback if the preferred filename is absent. For each filename,
the engine first checks `fntech3-pkg.fnz` in the selected profile's namespace,
then the active game filesystem. Q3's bundled `games/quake3/baseq3/fnq3-hud.json`
preset is enabled. Every other game with HUD correction ships its own stock
preset:

| Profile | Packaged rules | Placement |
| --- | --- | --- |
| RTCW SP | [`games/rtcw-sp/main/fntech3-hud.json`](../pkg/games/rtcw-sp/main/fntech3-hud.json) | Health, armor, stamina and pickup text at the left; ammo, weapon heat, holdables and notebook notification at the right; centered cursor hints. |
| RTCW MP | [`games/rtcw-mp/main/fntech3-hud.json`](../pkg/games/rtcw-mp/main/fntech3-hud.json) | Health, lives and stamina at the left; ammo and class charge at the right; round timer/objectives at the upper left; team overlay at the upper right; centered compass and markers. |
| Enemy Territory | [`games/wolfet/etmain/fntech3-hud.json`](../pkg/games/wolfet/etmain/fntech3-hud.json) | Stamina, health, head, skills, experience and lives at the lower left; weapon, ammunition, heat and charge at the lower right; compass, minimap and the timer, FPS and reinforcement boxes at the upper right; lagometer and objective icon at the right; fireteam overlay at the upper left; a scope's side masks reach the screen's edges. Popups, prompts, chat and center text stay centered, so no line is split. |
| Elite Force SP | [`games/stvef-sp/baseEF/fntech3-hud.json`](../pkg/games/stvef-sp/baseEF/fntech3-hud.json) | The health and armor gauges at the lower left and the ammunition gauge at the lower right, matched by their own art; a cutscene's letterbox bars reach the screen's edges. The talking head stays with its talk box. |
| Elite Force Holomatch | [`games/stvef-mp/baseEF/fntech3-hud.json`](../pkg/games/stvef-mp/baseEF/fntech3-hud.json) | The same gauges, and the score box above the ammunition at the lower right. |
| Alice | [`games/alice/base/fntech3-hud.json`](../pkg/games/alice/base/fntech3-hud.json) | The health gauge at the left and the mana gauge at the right, centered vertically; the weapon card at the lower left and the powerup card at the lower right. |
| F.A.K.K.2 | [`games/fakk2/fakk/fntech3-hud.json`](../pkg/games/fakk2/fakk/fntech3-hud.json) | Health and water gauge at the upper left; both weapon HUDs, their rings, ammunition and weapon models at the upper right; the boss's health bar at the left, centered vertically as it slides in. |

The presets preserve crosshair proportions and keep unrecognized draws centered.
They describe the stock module layouts; a mod with a different HUD can supply
rules in its own package namespace, or disable correction with `cl_hudAspect 0`.
Each profile reads only its own rules: SP and MP never read one another's, and
Q3 presets never leak into QL or RTCW. A mod that replaces one of F.A.K.K.2's
HUD menus with a different layout gets centered placement for draws outside
the stock menus' places until it ships matching rules.

Use `hud_reload` to reparse after editing. Missing, malformed, excessively
nested, or oversized documents fall back to centered placement. Files are
limited to 256 KiB and 256 rules; invalid rule fields are skipped. Rules are
also reloaded when cgame restarts, so changing mods cannot retain old rules
or shader names. F.A.K.K.2's HUD menus are known to the rules by their art's
names (`textures/hud/health_gradient`), `white` for a fill and
`gfx/fonts/<font>` for text. Alice's HUD views, like other 3D HUD viewports,
are matched with `refdef`, at the center of the whole-screen view each widget
is drawn in: its offset from the screen's center plus (320, 240).

The top-level value is either an array or an object containing a `rules` array.
The first matching rule wins. Supported fields are:

| Field | Meaning |
| --- | --- |
| `name` | Optional rule label. |
| `match.shader` | Case-insensitive shader name; a trailing `*` matches a prefix. The client's own console font, `white` and console background answer to `charset`, `white` and `console`. When the cgame registers one of them under its own name, as Q3's does with `gfx/2d/bigchars` and RTCW MP's with `gfx/2d/hudchars`, a rule may name either. |
| `match.textLike` | Boolean or 0/1; approximate glyph classification in 640x480 units. |
| `match.refdef` | Boolean or 0/1; selects a 3D HUD viewport. |
| `match.region` | `{ "x": ..., "y": ..., "w": ..., "h": ... }`; tests the draw's center in uncorrected 640x480 coordinates. |
| `mode` | `uniform` (default), `stretch`, or `extend`: uniform, with each edge that touches the 640x480 frame carried out to the screen's edge, for a mask beside a centered picture such as a scope's. |
| `align.x` | `left`, `center`/`middle`, or `right`. |
| `align.y` | `top`, `center`/`middle`, or `bottom`. |

```json
{
  "version": 1,
  "rules": [
    {
      "name": "upper_right_model",
      "match": { "refdef": true, "region": { "x": 568, "y": 0, "w": 72, "h": 72 } },
      "mode": "uniform",
      "align": { "x": "right", "y": "top" }
    }
  ]
}
```

Set `cl_hudDump 1` to write grouped HUD draw metadata -- the cgame's, or
F.A.K.K.2's HUD menus' -- to `fntech3-hud-dump.json` in the active writable
game directory. It includes
deduplicated regions, shader names, 3D viewport markers, sample counts, and
suggested alignments. A shared shader is dumped as `charset`, `white` or
`console`. This records draw metadata, not mouse or keyboard input.
Set it back to `0` after collecting samples, review the suggested rules, and
save the edited file as `fntech3-hud.json`. Dump files are never loaded
automatically.

## Menu and cinematic controls

- `cl_menuAspect 0`: Stretch ordinary menu widgets to the framebuffer. This is
  the default in every game except Quake Live.
- `cl_menuAspect 1`: Keep menu widgets in centered 4:3 space, including native
  UI model-preview viewports. This is Quake Live's default and matches retail
  Quake Live.
- `cl_cinematicAspect 0`: Stretch UI and fullscreen cinematics.
- `cl_cinematicAspect 1`: Keep cinematics in centered 4:3 space. This is the
  default.

`cl_menuAspect` applies to menus drawn through the Quake III-style UI path --
Quake III Arena, Quake Live and Elite Force Holomatch -- and to Elite Force
single player's menus. RTCW's and Enemy Territory's menus are drawn as their
retail engines drew them, and Alice and F.A.K.K.2 draw their engine's own
`.urc` menus. RTCW ignores `cl_cinematicAspect`: its cinematics always fill the
screen, and single player adds its original letterbox bars.

`cl_menuBlur` (default `1`) softens the scene behind an in-game menu in every
game except Alice and F.A.K.K.2; see [Menu Soft Focus](fnql/MENU_SOFT_FOCUS.md).

## Quake Live Menus And 3D Widgets

In Quake Live, with `cl_menuAspect 1`, traditional menu art and UI-rendered
model scenes use the same centered 4:3 transform. The UI module remains
responsible for the scene projection; the engine changes only the destination
viewport.

Quake Live configs created by early FnQL builds may contain the former
archived default, `cl_menuAspect 0`. fnTech3 migrates that legacy value to `1`
once so upgraded profiles receive retail drawing. Setting it back to `0` after
that migration remains an explicit full-frame stretch opt-out.

## Quake Live Connecting Screen

Retail `ui/connect.menu` declares `ui/assets/backscreen_smoke` as a 1920x1080
background. fnTech3 preserves that authored 16:9 aspect with a centered cover
crop: narrower displays crop the sides, while displays wider than 16:9 crop
the top and bottom. The image is never stretched, and ultrawide modes do not
smear its outermost texture columns across the extra width. The complete
connection screen keeps the retail UI's centered coordinates regardless of
the optional `cl_menuAspect` override used by ordinary menus.

The retail `backgroundSize` path submits this backdrop in framebuffer pixels,
unlike traditional 640x480 menu quads. fnTech3 recognizes that exact draw before
applying `cl_menuAspect 0`; treating it as a traditional quad would widen the
already-native geometry a second time on widescreen displays.

The retail cgame takes over after the initial connection dialog and uses the
same authored 16:9 crop for its smoke and map-levelshot loading backgrounds.
fnTech3 recognizes only that full-frame loading draw signature during
`CA_LOADING` and `CA_PRIMED`, and applies the same centered cover crop. HUD and
other cgame draws are not modified.

## Cinematics

`cl_cinematicAspect` is independent of menu presentation. Use `1` for
pillarboxed 4:3 video or `0` for fullscreen stretching.

Console layout and scaling are documented in the [Console Guide](CONSOLE.md).
