# Player Highlights Guide

Player highlighting makes other players easier to spot by giving them a soft
coloured glow round their edges (a *rim light*), a solid coloured outline, or
both. It is off by default, and only changes the picture: it changes nothing
in the game itself, and other players cannot see it.

For the cartoon-style cel shading and outlines on every model, see
[Cel Shading](DISPLAY.md#cel-shading) in the Graphics guide.

## Which games it works in

Highlighting works in **Quake III Arena** and **Quake Live**, where fnTech3
can tell players apart from every other model. Elite Force draws its players
the same way, but whether highlighting finds them has not been checked yet.
Return to Castle Wolfenstein, Enemy Territory, Alice and F.A.K.K.2 draw their
models differently, so highlighting has no effect there.

fnTech3 recognises a player by the model it wears -- a `lower`, `upper` or
`head` model or skin under `models/players/` -- and the teams of a team game
by the game type (`g_gametype`) and the red or blue skin each player wears.
Custom or forced player models kept in other folders are not highlighted.

## Turning it on

- `cl_playerHighlight 0`: off. This is the default.
- `cl_playerHighlight 1`: the glow only.
- `cl_playerHighlight 2`: the outline only.
- `cl_playerHighlight 3`: both.

For example:

```cfg
seta cl_playerHighlight "3"
```

What gets highlighted:

- Every other player, in team games both teams. Your own player is never
  highlighted, nor are bodies after a death or the heads drawn on the HUD.
- In team games each team gets its team's colour; in free-for-all games
  every player gets the free-for-all colour.
- The outline needs a *stencil buffer*, a drawing feature fnTech3 turns on by
  default (`r_stencilbits 8`).

## Colours

Each colour is three numbers, red, green and blue, from `0` to `255`, with an
optional fourth for opacity.

- `cl_playerHighlightRedColor`: the red team's colour. Default `"255 32 0"`.
- `cl_playerHighlightBlueColor`: the blue team's colour. Default `"0 32 255"`.
- `cl_playerHighlightFreeColor`: the colour in free-for-all games. Default
  `"255 32 0"`.

The glow and the outline use the same colour, at different strengths so they
look like two separate effects.

### Enemies and teammates instead of team colours

If you would rather colour players by whose side they are on than by team,
set either or both of these. They are empty by default, which uses the team
colours above.

- `cl_playerHighlightEnemyColor`: replaces the colour of players on the other
  team, and the free-for-all colour.
- `cl_playerHighlightTeammateColor`: replaces the colour of players on your
  team. It does nothing in free-for-all games.

## Strength and thickness

- `cl_playerHighlightRimIntensity`: how strong the glow is, from `0` to `2`.
  Default `2.0`.
- `cl_playerHighlightOutlineIntensity`: how strong the outline is, from `0`
  to `2`. Default `2.0`.
- `cl_playerHighlightOutlineScale`: how thick the outline is. Default `1.01`;
  small changes go a long way, so stay close to `1.0`.

`0` hides one effect without changing `cl_playerHighlight`; values above `1`
strengthen it until it is fully solid.

## Examples

Both effects in the team colours:

```cfg
seta cl_playerHighlight "3"
```

One colour for every enemy in team games:

```cfg
seta cl_playerHighlight "3"
seta cl_playerHighlightEnemyColor "255 128 96"
seta cl_playerHighlightTeammateColor ""
```

Outlines in enemy and teammate colours:

```cfg
seta cl_playerHighlight "2"
seta cl_playerHighlightEnemyColor "224 96 96"
seta cl_playerHighlightTeammateColor "96 208 160"
```

A glow in a colour of your own for free-for-all:

```cfg
seta cl_playerHighlight "1"
seta cl_playerHighlightFreeColor "224 112 112"
seta cl_playerHighlightEnemyColor ""
```

## In the menu

In Quake Live, fnTech3's own settings pages carry these options in the
**Team** section: the teammate and opponent colours at the bottom of the
teammate and enemy columns, and a **Player Highlighting** block below them
with the mode, both strengths, the outline thickness and the red, blue and
free-for-all colours. The other games have no menu for them; set them in the
console or a settings file.

## Older setting names

The old `cl_enemyHighlight*` settings are now called `cl_playerHighlight*`.
Values saved under the old names are carried over the first time, as long as
the new settings are still at their defaults.

## Related guides

- [Graphics guide](DISPLAY.md) for cel shading, HDR, bloom and render scaling.
- [Widescreen guide](ASPECT_CORRECTION.md) for menus and movies.
- [Console guide](CONSOLE.md) for the console's size, look and controls.
