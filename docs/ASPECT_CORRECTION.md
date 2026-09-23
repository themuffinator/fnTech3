# FOV, HUD, Menu And Cinematic Aspect Handling

fnTech3 restores FnQuake3's optional world FOV and HUD correction for Quake III
Arena and both experimental RTCW profiles. The selected game profile controls
availability. Retail Quake Live modules already own these transforms: the
engine never applies a second FOV/HUD correction, loads HUD rules, or collects
HUD dumps in the QL profile, even if a config explicitly sets the legacy cvars.

## World FOV and HUD

- `r_fovCorrection 1` (default for Q3/RTCW) preserves the vertical framing of
  a horizontal FOV authored for 4:3, widening or narrowing it for the viewport.
  For example, 90 degrees at 4:3 becomes about 106.26 degrees at 16:9.
  All three renderers use the same policy, including stretched render targets
  and render-scale modes that preserve aspect. Use `0` for mods that already
  calculate an aspect-correct FOV.
- `cl_hudAspect 1` (default for Q3/RTCW) remaps legacy stretched HUD geometry
  into uniform 4:3 space, centered unless a rule changes its placement.
  Use `0` to retain the module's unmodified HUD coordinates.
- Fullscreen fades and backgrounds remain full-frame by default. HUD 3D model
  viewports follow the same placement rules as images and retain their authored
  FOV. Menu scenes also keep their authored FOV. RTCW gradient pictures and MP
  rotated pictures follow HUD layout while retaining their colors and UVs.

These controls are independent: HUD placement does not change world FOV.
They operate on engine-owned copies after module ABI conversion.

## HUD Rules and Diagnostics

`fntech3-hud.json` is the preferred rule file; `fnq3-hud.json` remains a
compatible fallback if the preferred filename is absent. For each filename,
the engine first checks `fntech3-pkg.fnz` in the selected profile's namespace,
then the active game filesystem. Q3's bundled `games/quake3/baseq3/fnq3-hud.json`
preset is enabled. RTCW ships separate stock HUD presets:

| Profile | Packaged rules | Placement |
| --- | --- | --- |
| RTCW SP | [`games/rtcw-sp/main/fntech3-hud.json`](../pkg/games/rtcw-sp/main/fntech3-hud.json) | Health, armor, stamina and pickup text at the left; ammo, weapon heat, holdables and notebook notification at the right; centered cursor hints. |
| RTCW MP | [`games/rtcw-mp/main/fntech3-hud.json`](../pkg/games/rtcw-mp/main/fntech3-hud.json) | Health, lives and stamina at the left; ammo and class charge at the right; round timer/objectives at the upper left; team overlay at the upper right; centered compass and markers. |

The presets preserve crosshair proportions and keep unrecognized draws centered.
They describe the stock module layouts; a mod with a different HUD can supply
rules in its own package namespace, or disable correction with `cl_hudAspect 0`.
SP and MP never read one another's rules. Q3 presets never leak into QL or RTCW.

Use `hud_reload` to reparse after editing. Missing, malformed, excessively
nested, or oversized documents fall back to centered placement. Files are
limited to 256 KiB and 256 rules; invalid rule fields are skipped. Rules are
also reloaded when cgame restarts, so changing mods cannot retain old rules
or shader names.

The top-level value is either an array or an object containing a `rules` array.
The first matching rule wins. Supported fields are:

| Field | Meaning |
| --- | --- |
| `name` | Optional rule label. |
| `match.shader` | Case-insensitive shader name; a trailing `*` matches a prefix. The client's own console font, `white` and console background answer to `charset`, `white` and `console`. When the cgame registers one of them under its own name, as Q3's does with `gfx/2d/bigchars` and RTCW MP's with `gfx/2d/hudchars`, a rule may name either. |
| `match.textLike` | Boolean or 0/1; approximate glyph classification in 640x480 units. |
| `match.refdef` | Boolean or 0/1; selects a 3D HUD viewport. |
| `match.region` | `{ "x": ..., "y": ..., "w": ..., "h": ... }`; tests the draw's center in uncorrected 640x480 coordinates. |
| `mode` | `uniform` (default) or `stretch`. |
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

Set `cl_hudDump 1` to write grouped cgame draw metadata to
`fntech3-hud-dump.json` in the active writable game directory. It includes
deduplicated regions, shader names, 3D viewport markers, sample counts, and
suggested alignments. A shared shader is dumped as `charset`, `white` or
`console`. This records draw metadata, not mouse or keyboard input.
Set it back to `0` after collecting samples, review the suggested rules, and
save the edited file as `fntech3-hud.json`. Dump files are never loaded
automatically.

## Menu and cinematic controls

- `cl_menuAspect 0`: Stretch ordinary menu widgets to the framebuffer.
- `cl_menuAspect 1`: Keep menu widgets in centered 4:3 space, including native
  UI model-preview viewports. This is the default and matches retail QL.
- `cl_cinematicAspect 0`: Stretch UI and fullscreen cinematics.
- `cl_cinematicAspect 1`: Keep cinematics in centered 4:3 space.

## Menus And 3D Widgets

In Quake Live, with `cl_menuAspect 1`, traditional menu art and UI-rendered model scenes use
the same centered 4:3 transform. The UI module remains responsible for the
scene projection; the engine changes only the destination viewport.

Quake Live configs created by early FnQL builds may contain the former archived default,
`cl_menuAspect 0`. FnQL migrates that legacy value to `1` once so upgraded
profiles receive retail drawing. Setting it back to `0` after that migration
remains an explicit full-frame stretch opt-out.

## Connecting Screen

Retail `ui/connect.menu` declares `ui/assets/backscreen_smoke` as a 1920x1080
background. FnQL preserves that authored 16:9 aspect with a centered cover
crop: narrower displays crop the sides, while displays wider than 16:9 crop
the top and bottom. The image is never stretched, and ultrawide modes do not
smear its outermost texture columns across the extra width. The complete
connection screen keeps the retail UI's centered coordinates regardless of
the optional `cl_menuAspect` override used by ordinary menus.

The retail `backgroundSize` path submits this backdrop in framebuffer pixels,
unlike traditional 640x480 menu quads. FnQL recognizes that exact draw before
applying `cl_menuAspect 0`; treating it as a traditional quad would widen the
already-native geometry a second time on widescreen displays.

The retail cgame takes over after the initial connection dialog and uses the
same authored 16:9 crop for its smoke and map-levelshot loading backgrounds.
FnQL recognizes only that full-frame loading draw signature during
`CA_LOADING` and `CA_PRIMED`, and applies the same centered cover crop. HUD and
other cgame draws are not modified.

## Cinematics

`cl_cinematicAspect` is independent of menu presentation. Use `1` for
pillarboxed 4:3 video or `0` for fullscreen stretching.

Console layout and scaling are documented in the [Console Guide](CONSOLE.md).
