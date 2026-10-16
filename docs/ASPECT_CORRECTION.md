# Widescreen Guide

Most of fnTech3's games were made for square-ish 4:3 screens. On a widescreen
or ultrawide monitor, fnTech3 widens your view of the world instead of
stretching it, keeps the HUD in proportion at the edges of the screen, and
can keep menus and movies in their original shape. Where a game already
handles widescreen itself, as Quake Live does, fnTech3 leaves it alone rather
than correcting twice.

## The four settings

| Setting | Default | What it does |
| --- | --- | --- |
| `r_fovCorrection` | `1` | Widens the view to fit your screen, keeping what you see top to bottom. `0` keeps the game's view exactly as the original drew it, for mods that widen it themselves. |
| `cl_hudAspect` | `1` | Keeps the HUD in its original shape, moved out to the screen's edges and corners. `0` stretches it across the screen, as the original did. |
| `cl_menuAspect` | `0`, or `1` in Quake Live | `1` keeps menus in a centred 4:3 area; `0` stretches them across the screen. |
| `cl_cinematicAspect` | `1` | `1` keeps movies at 4:3 with bars at the sides; `0` stretches them to fill the screen. |

Each one works on its own: moving the HUD never changes your view, and the
other way round. `r_fovCorrection` and `cl_hudAspect` only exist in the games
that use them -- `cl_hudAspect` is not in Quake Live or Medal of Honor, for
example. `cl_menuAspect` and `cl_cinematicAspect` exist in every game, but
only change the games the table below lists.

## What each game does

| Game | View | HUD | Menus | Movies |
| --- | --- | --- | --- | --- |
| Quake III Arena | widened | kept in shape | `cl_menuAspect` | `cl_cinematicAspect` |
| Quake Live | the game widens it for four screen shapes, and fnTech3 finishes the job on others | the game's own | `cl_menuAspect`, on by default, as in the original | `cl_cinematicAspect` |
| Return to Castle Wolfenstein, both modes | widened | kept in shape | drawn as the original drew them | always fill the screen; single player adds its own black bars |
| Enemy Territory | widened | kept in shape, with its full-screen panels as below | drawn as the original drew them | `cl_cinematicAspect` |
| Elite Force, both modes | widened | kept in shape | `cl_menuAspect` | `cl_cinematicAspect` |
| Jedi Academy, multiplayer | widened | kept in shape, with its scoreboard as below | `cl_menuAspect` | not checked yet |
| Jedi Outcast, multiplayer | widened | kept in shape, with its scoreboard as below | `cl_menuAspect` | not checked yet |
| Alice | widened | kept in shape | the game's own menus | `cl_cinematicAspect` |
| F.A.K.K.2 | widened | kept in shape | the game's own menus | `cl_cinematicAspect` |
| Medal of Honor | widened | the game's own, which fits any screen | the game's own menus | `cl_cinematicAspect` |

The IHV Test has no client, and the Q3Test versions have none of these
corrections. Both Jedi games draw their HUD and menus on a 640x480 screen
that the original stretched over the window; fnTech3 scales them to your
window the same way, then applies the settings below. Neither game's single
player gets as far as its menus yet.

## Field Of View

The *field of view* (FOV) is how wide your view of the world is. Games made
for 4:3 screens set it for a 4:3 screen. With `r_fovCorrection 1`, fnTech3
keeps what you can see from top to bottom the same and widens the view to
fill your screen. A 90-degree view on a 4:3 screen becomes about 106 degrees
on 16:9, for example. On a 4:3 screen nothing changes, and a view the game
crops in one direction, such as a letterboxed scene, keeps its shape. All
three renderers -- GLx, Vulkan and RTX -- widen the view the same way,
including when you render at a different resolution.

Set `r_fovCorrection 0` for a mod that already widens the view itself, or to
see exactly what the original showed.

**Quake Live** widens its own view, but only for four screen shapes: 4:3,
5:4, 16:10 and 16:9. On any other screen the original uses the widest of
those your screen is at least as wide as, stretched across it: at 21:9,
`cg_fov 100` gives the 16:9 view of about 116 degrees, spread across the
wider screen. With `r_fovCorrection 1`, fnTech3 widens it
properly, to about 129 degrees at 21:9, and changes nothing on the four
shapes the game already handles. Quake Live's markers and teammate icons stay
on their targets.

## The HUD

With `cl_hudAspect 1`, fnTech3 keeps each part of the HUD in its original
shape and places it by the game's own layout -- health in a lower corner, for
example -- instead of stretching it. Anything it does not recognise stays in
the centre. Full-screen fades and backgrounds still cover the whole screen,
and small 3D models in the HUD keep their own view.

fnTech3 brings HUD layouts for Quake III Arena, both modes of Return to Castle
Wolfenstein, Enemy Territory, both modes of Elite Force, Jedi Academy
multiplayer, Alice and F.A.K.K.2. `cl_hudAspect 0` gives the original
stretched HUD.

**Game notes:**

- **Quake III Arena's** scoreboards and loading screen are shown as centred
  4:3 screens, and the rest of its HUD stays at its edges and corners on wide
  and tall screens alike. Item names, votes, team chat and the attacker's
  name stay in one piece beside what they belong to
  ([record](fntech3/2026-10-06-q3-hud-alignment.md)).
- **Enemy Territory** draws some full screens of its own, laid out for 4:3.
  While you hold the scoreboard (`+scores`) or the large command map
  (`+mapexpand`), and at the end-of-round debriefing, the whole screen is
  shown as one centred 4:3 area. The limbo screen is stretched, as the
  original drew it.
- **Elite Force single player** draws brackets round targets in the world.
  With `cl_hudAspect 1` it is given a 4:3 screen to draw on, so the brackets
  stay on their targets. The setting takes effect at the next map or
  `vid_restart`.
- **Jedi Academy's** multiplayer client game is given a 4:3 screen to draw
  on, as Elite Force single player is, so its crosshair, which follows what
  you aim at, stays on target. The scoreboard is shown as one centred 4:3
  screen, and the weapon, force power and item bars stay whole in the middle.
  Switching `cl_hudAspect` takes full effect at the next map or `vid_restart`.
- **Jedi Outcast's** multiplayer client game is given the same 4:3 screen.
  Its scoreboard is shown as one centred 4:3 screen, and its weapon bar stays
  whole in the middle. Its zoom, rocket lock and loading screen have not
  been checked in a wide window yet.
- **Alice's** gauges and cards are small 3D models; each keeps its 4:3 size
  at its edge of the screen.
- **F.A.K.K.2's** HUD is four of the game's own menus -- health, weapons, two
  weapons, and the boss's health -- and only those follow the layout, with a
  weapon model following its ring. Every other menu, the inventory and the
  rest of the game's own drawing stay stretched across the screen.

## Menu And Cinematic Controls

- `cl_menuAspect 0`: stretch menus across the screen. This is the default in
  every game except Quake Live.
- `cl_menuAspect 1`: keep menus, including the 3D models in them, in a centred
  4:3 area. This is Quake Live's default, and how the original Quake Live
  draws them.
- `cl_cinematicAspect 1`: keep movies at 4:3, with bars at the sides. This is
  the default.
- `cl_cinematicAspect 0`: stretch movies to fill the screen.

`cl_menuAspect` applies to Quake III Arena, Quake Live, both modes of Elite
Force and the multiplayer menus of both Jedi games, whose originals stretched
them; the 3D models in a Jedi menu move with it. Return to Castle Wolfenstein
and Enemy Territory draw their menus as the originals did, and Alice,
F.A.K.K.2 and Medal of Honor draw their own.
Return to Castle Wolfenstein ignores `cl_cinematicAspect`: its movies always
fill the screen, and single player adds its original black bars.

`cl_menuBlur` (on by default) softly blurs the game behind a menu, in every
game except Alice and F.A.K.K.2; see [Menu Soft Focus](fnql/MENU_SOFT_FOCUS.md).

### Quake Live menus

Quake Live keeps its menus, and the 3D scenes in them, in the centred 4:3
area by default, as the original does. Settings files from early versions of
fnTech3's predecessor, FnQL, may hold the old default, `cl_menuAspect 0`;
fnTech3 changes it to `1` once, after which `0` is your choice to stretch
them.

### Quake Live's connecting screen

The smoky background of Quake Live's connecting and loading screens is a 16:9
picture. fnTech3 keeps its shape on every screen and crops it to fill the
screen -- from the sides on narrower screens, from the top and bottom on
wider ones -- instead of stretching it. The rest of the connecting screen
stays where the original puts it, whatever `cl_menuAspect` says.

## For mod makers: HUD layouts

A mod with a different HUD can bring its own layout rules, or players can
turn the correction off with `cl_hudAspect 0`.

**Where the rules live.** fnTech3 reads `fntech3-hud.json`, or else the older
name `fnq3-hud.json`, first from its own package, under the running game's
folder, then from the game's files. Each game, and each mode, reads only its
own rules. These are the layouts fnTech3 brings:

| Game | Rules | Placement |
| --- | --- | --- |
| Quake III Arena | [`games/quake3/baseq3/fnq3-hud.json`](../pkg/games/quake3/baseq3/fnq3-hud.json) | Status bar and weapon bar centred at the bottom; item pickups, team chat and the lower team overlay at the lower left; score boxes, flags, powerups, the lower team overlay and the lag meter at the lower right; FPS, timer, attacker and the upper team overlay top right; votes at the left edge; the holdable item at the right. Lines of text are never split, and the scoreboards and loading screen are centred 4:3 screens. |
| RTCW single player | [`games/rtcw-sp/main/fntech3-hud.json`](../pkg/games/rtcw-sp/main/fntech3-hud.json) | Health, armour, stamina and pickup text at the left; ammunition, weapon heat, items and the notebook notice at the right; hints in the centre. |
| RTCW multiplayer | [`games/rtcw-mp/main/fntech3-hud.json`](../pkg/games/rtcw-mp/main/fntech3-hud.json) | Health, lives and stamina at the left; ammunition and class charge at the right; timer and objectives top left; team overlay top right; compass and markers in the centre. |
| Enemy Territory | [`games/wolfet/etmain/fntech3-hud.json`](../pkg/games/wolfet/etmain/fntech3-hud.json) | Stamina, health, head, skills, experience and lives lower left; weapon, ammunition, heat and charge lower right; compass, minimap and the timer, FPS and reinforcement boxes top right; lag meter and objective icon at the right; fireteam overlay top left; a scope's side masks reach the screen's edges. Pop-ups, prompts, chat and centre text stay centred. |
| Elite Force single player | [`games/stvef-sp/baseEF/fntech3-hud.json`](../pkg/games/stvef-sp/baseEF/fntech3-hud.json) | Health and armour gauges lower left, ammunition gauge lower right; a cutscene's black bars reach the screen's edges; the talking head stays with its talk box. |
| Elite Force Holomatch | [`games/stvef-mp/baseEF/fntech3-hud.json`](../pkg/games/stvef-mp/baseEF/fntech3-hud.json) | The same gauges, and the score box above the ammunition lower right. |
| Jedi Academy multiplayer | [`games/jka-mp/base/fntech3-hud.json`](../pkg/games/jka-mp/base/fntech3-hud.json) | Health and armour lower left, with chat above them; force, ammunition, saber style and score lower right; the leader's portrait, team scores and timers top right; votes and holocrons at the left. The crosshair, centre messages and the weapon, force and item bars stay centred, and the scoreboard and loading screen are centred 4:3 screens. Lines of text are never split. |
| Jedi Outcast multiplayer | [`games/jk2-mp/base/fntech3-hud.json`](../pkg/games/jk2-mp/base/fntech3-hud.json) | Health and armour lower left; force, ammunition and the score lower right; the leader's portrait and name top right. The crosshair and the weapon, force and item bars stay centred, and the scoreboard is a centred 4:3 screen. Lines of text are never split. |
| Alice | [`games/alice/base/fntech3-hud.json`](../pkg/games/alice/base/fntech3-hud.json) | Health gauge at the left and mana gauge at the right, centred top to bottom; weapon card lower left, powerup card lower right. |
| F.A.K.K.2 | [`games/fakk2/fakk/fntech3-hud.json`](../pkg/games/fakk2/fakk/fntech3-hud.json) | Health and water gauge top left; both weapon HUDs, their rings, ammunition and weapon models top right; the boss's health bar at the left, centred as it slides in. |

The layouts keep the crosshair's proportions and leave anything they do not
recognise in the centre. A mod that replaces one of F.A.K.K.2's HUD menus
gets centred placement until it brings matching rules.

**Writing rules.** The file holds an array of rules, or an object with a
`rules` array; the first rule that matches a piece of the HUD decides where
it goes. Positions are in the game's 640x480 screen.

| Field | Meaning |
| --- | --- |
| `name` | An optional label. |
| `match.shader` | The picture's name, in any case; a trailing `*` matches the start of a name. The console's font, plain white and console background answer to `charset`, `white` and `console`; a game's own name for them, such as Quake III's `gfx/2d/bigchars` or RTCW multiplayer's `gfx/2d/hudchars`, works too. |
| `match.textLike` | `true` or `1` matches drawing that looks like text. |
| `match.refdef` | `true` or `1` matches a 3D view in the HUD. |
| `match.region` | `{ "x": ..., "y": ..., "w": ..., "h": ... }`: matches drawing whose centre is in that area. |
| `match.edge` | `left` or `right`: test the drawing's left or right end -- or, with `textRuns`, its line's -- against the region instead of its centre, for a line that starts at an edge however long it is. |
| `match.minHeight`, `match.maxHeight` | Match only drawing at least, or at most, this tall. |
| `panel` | `centered`: from this drawing on, the rest of the frame is one centred 4:3 screen, for a game's scoreboard or loading screen. |
| `mode` | `uniform` (the default) keeps the shape; `stretch` stretches it; `extend` keeps the shape but carries any edge that touches the screen's border out to the real edge, for a mask beside a centred picture such as a scope's. |
| `align.x` | `left`, `center` (or `middle`) or `right`. |
| `align.y` | `top`, `center` (or `middle`) or `bottom`. |

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

The object form also takes two options. `"textRuns": true` places text by
the line: each piece of a line is matched by the whole line, so a line is
never split between two places. `"widthScaledShaders"` lists pictures a game
scaled by the screen's width even from top to bottom, as Quake III's
proportional font is, so they keep their shape. Quake III's rules use both.

F.A.K.K.2's HUD pictures are matched by their art's names (such as
`textures/hud/health_gradient`), `white` for a plain fill and
`gfx/fonts/<font>` for text. Alice's HUD views are matched with `refdef`, at
their centre's offset from the screen's centre plus (320, 240). Jedi
Academy's text, drawn with its own fonts, has no picture name in a dump:
match it by its area, and with `textRuns` each string is one line.

**Finding what to match.** Set `cl_hudDump 1` while the HUD is on screen to
write what it draws -- areas, picture names, 3D views and suggested
alignments -- to `fntech3-hud-dump.json` in the game's folder in your home
folder. It records drawing only, never your mouse or keys. Set it back to
`0`, review the suggestions, and save your edited rules as
`fntech3-hud.json`; dump files are never loaded by themselves.

**Trying changes.** `hud_reload` reloads the rules after you edit them, and
they reload whenever a map starts, so switching mods never keeps old rules. A
missing or broken file, or one over 256 KiB, falls back to centred placement;
rules after the 256th are ignored, and a rule with a bad field is skipped.

The details behind each game's corrections, and what is not covered yet, are
in the [widescreen record](fntech3/2026-09-25-widescreen-hud-and-fov.md).
Console size and layout are in the [Console guide](CONSOLE.md).
