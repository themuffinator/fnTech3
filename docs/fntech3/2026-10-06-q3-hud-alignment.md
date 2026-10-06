# Quake III Arena HUD alignment

## Scope

This record covers `cl_hudAspect 1` for the Quake III Arena profile (`q3`,
baseq3's retail 1.32 cgame), on wide, tall and 4:3 screens. Each part of the
HUD now keeps its place and every line of text stays whole.

It is a presentation feature (AGENTS.md invariant 11): the HUD rules place
the 2D draws the retail cgame hands the engine. No module ABI, wire, demo,
save, pak or pure contract changes. `cl_hudAspect 0` still draws retail's
stretched HUD. Team Arena (`missionpack`) draws its HUD from menu scripts and
is not covered.

## What was wrong

*Observed* at 1680x720 (21:9) with the engine's `screenshotPNG`, using the
build before this change (`.tmp/runtime/q3hud-before`, the Steam 1.32
installation):

- **Pickup names split.** The first glyph stayed at the left and the rest
  moved toward the centre ("M" ... "ega Health"). A weapon's pickup icon
  moved to the centre with the weapon bar.
- **Centred lines shifted right or split.** This hit the warmup lines
  ("Waiting for players", "Free For All", and the duel's "UnnamedPlayer vs
  Sarge", which split after "Unna"), "LOW AMMO WARNING" and "OUT OF AMMO".
  It also hit the scoreboard's "Fragged by", "1st place with" and "Teams are
  tied" lines, and the tourney scoreboard's title and clock.
- **The attacker's name left its head** whenever `cg_drawFPS` or
  `cg_drawTimer` pushed it down, and it sat about 360 pixels to the left.
- **The no-ammo cross over a weapon in the bar's right half** moved to the
  right, away from its icon.
- **Votes split** mid-word ("VOTE(28):fragl" ... "imit 30"). The team vote
  sat at the 4:3 frame's edge, not the screen's.
- **Team chat, and the team overlay at the lower left and right, scattered.**
  The overlay's weapon and powerup icons landed in its name column.
- **The lagometer and the "connection interrupted" icon** sat at the 4:3
  frame's corner, not the screen's.
- **The loading screen:**
  - its proportional text was stretched vertically and pushed down;
  - "FRAGLIMIT" and several item icons were thrown to the right;
  - one icon was pinned to the left edge.

*Cause:*

- The rules from FnQuake3 matched each glyph by its own centre, so a line
  crossing a region's edge split. A wide `upper_right_text` region (x 112 to
  638, y 2 to 78) also caught centred lines and votes.
- Nothing recognised the scoreboard, tourney scoreboard or loading screen as
  a screen of the game's own.
- The cgame's `UI_DrawProportionalString` (cg_drawtools.c, GPL source) scales
  y by `screenXScale`. Undoing it with the vertical scale distorts the font
  on any screen that is not 4:3.

## What the engine does

`code/client/cl_hud.cpp` gains rule-file features. Each is opt-in, and the
other profiles' rule files use none of them:

- **`"textRuns": true`** (top level). The client game's glyphs are held
  until their line is complete, with the colour changes between them. Each
  glyph is then matched by the whole line's rectangle, so a line takes one
  placement.
  - A line is the same font and height on one baseline, moving right with at
    most four glyph heights of space.
  - Another picture, a scene or the end of the client's frame submits the
    held line first.
  - Held glyphs are placed through `CL_HudAdjustPic`, so whatever hides the
    HUD at that moment still hides them.
  - The generic client-game dispatch routes `CG_R_SETCOLOR`,
    `CG_R_DRAWSTRETCHPIC` and `CG_R_RENDERSCENE` through
    `CL_HudQueueColor`, `CL_HudQueueStretchPic` and `CL_HudFlushQueue`.
    Without the option, nothing is held.
- **`match.edge`**: `left` or `right` tests the line's (or picture's) end in
  place of its centre. A vote or a team chat line stays at the left edge,
  however long.
- **`match.minHeight` / `match.maxHeight`**: tell 8-unit team-overlay icons
  from the weapon bar's and the powerups'.
- **`"panel": "centered"`** on a rule: from the draw it matches, the rules
  pause for the rest of the frame, as `HUD_PANEL_CENTERED` does for Enemy
  Territory. The scoreboard (its `menu/tab/*` header), the tourney scoreboard
  (its full-screen fill) and the loading screen (its levelshot) each become
  one centred 4:3 composite. The HUD drawn before them keeps its rules.
- **`"widthScaledShaders"`** (top level): draws of these shaders are mapped
  back with the horizontal scale on both axes, undoing the proportional
  font's quirk.

## The Quake III rules

[`pkg/games/quake3/baseq3/fnq3-hud.json`](../../pkg/games/quake3/baseq3/fnq3-hud.json)
is rewritten from baseq3's own geometry in id's GPL cgame source
(`cg_draw.c`, `cg_scoreboard.c`, `cg_weapons.c`, `cg_info.c`), used for
reference only. The retail 1.32 QVMs ran in every capture.

| Part | Placement |
| --- | --- |
| Snapshot, FPS and timer lines, attacker head and name, team overlay (`cg_drawTeamOverlay 1`) | right, top; text by its line's centre right of x 330, down to y 200, so the stack holds under the overlay |
| Votes and team votes | left, top, by the line's left end |
| Holdable item | right, middle |
| Powerups and their timers | right, bottom, with the score boxes they climb from |
| Status bar: numbers, icons, 3D models | centre, bottom |
| Team background and team chat band | `extend` to the screen's sides, bottom |
| Weapon bar, its no-ammo crosses and marker, the weapon name | centre, bottom |
| Item pickup icon and name, team overlay at the lower left (`3`), team chat | left, bottom |
| Score boxes, flags, limits, team overlay at the lower right (`2`) | right, bottom |
| Lagometer and the interrupted-connection icon | right, bottom |
| Spectator lines | centre, bottom |
| Scoreboard, tourney scoreboard, loading screen | one centred 4:3 screen |

Everything else, such as centre prints, warmup, follow, rewards, the
crosshair and its names, stays centred.

## Evidence

- **Unit/Fixture.** `tests/hud_aspect_tests.py` compiles the real
  `cl_hud.cpp`. It now covers:
  - held lines, the colour order between their glyphs, and flushing on a
    picture, at the frame's end and outside a frame;
  - edges, heights and panels, and malformed options and rules;
  - with the shipped Quake III rules, baseq3's whole HUD in cg_draw.c's
    coordinates through the client game's path: 71 checks at 2560x1080,
    1280x1024 and 960x720, every glyph of every line checked.

  Fed HEAD's former rules, the same fixture fails at the attacker's name,
  drawn 560 pixels left of its place.
  The full Python suite passed in the shared tree (2594 passed, 125
  skipped).
- **Live.** Windows x86, `meson/build/win32-dev`, the Steam Quake III 1.32
  installation, windowed on a private desktop with no input. Every action is
  a console command, captured by the engine's `screenshotPNG`. The scenarios
  are:
  - FFA: ammo warnings, the weapon bar with a no-ammo cross, item and weapon
    pickups, the holdable, all six powerups, rewards, 2D icons, scoreboard;
  - a skill-5 bot fight: the attacker's head and name under the FPS and
    timer, "Fragged by", intermission;
  - FFA and duel warmup;
  - spectator and follow;
  - team deathmatch with team chat, all three overlay positions and the
    snapshot stack, then the team and tourney scoreboards;
  - CTF with the flag carried, 3D and 2D;
  - "Connection Interrupted";
  - demo playback, which draws the lagometer;
  - two clients on loopback for an open vote and team vote;
  - the loading screen, held by `sv_packetdelay`.

  The after build ran every scenario at 1680x720, 1280x720, 960x720 and
  900x720. The before build ran them at 1680x720, and the loading screen at
  960x720 too. At 4:3 the after build's loading frames are byte-identical to
  the before build's. The machine-readable companion lists builds, commands
  and captures.

## Not covered

- Mods with their own HUD (OSP, CPMA and others) get these rules only if
  they bring none of their own. Team Arena's scripted HUD is not covered.
- A centre print whose line is wider than the screen starts at x 0 and is
  placed as a left-edge line would be. baseq3's lines are far shorter.
- The levelshot behind the loading screen still stretches to the screen, as
  every full-screen background does.
