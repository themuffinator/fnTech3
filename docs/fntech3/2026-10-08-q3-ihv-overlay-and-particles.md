# The IHV Test's damage overlay and its particles beside the original's, 2026-10-08

The [commands and 2D record](2026-10-08-q3-ihv-commands-and-2d.md) left two
points open: the overlay square had never been seen in use, and the client
game's particles had never been set beside the original's. Both now have
evidence. The IHV client game's own debug cvars drive them without input, in
fnTech3 and in the original alike. The
[JSON companion](2026-10-08-q3-ihv-overlay-and-particles.json) holds
identities, captures and the disassembly offsets.

## The client game's debug cvars

`cgamex86.dll` registers `cg_testblend` and `cg_testparticles` (flags 0,
default 0, `0x1000aaf0` and `0x1000ab07`). Each frame, while either is set:

- `cg_testparticles` (`0x10010e08`) calls `0x1000fbe0`. It fills 4,096 white
  particles in a fixed grid in front of the view, receding in depth.
- `cg_testblend` (`0x10010e65`) calls `0x1000fdb0`. It sets the overlay to
  the centre of the view (0, 0), half-size 0.5, white at alpha 0.5, with the
  shader `models/weaphits/blood201`.

`cg_blend`, default 1, zeroes the overlay when cleared (`0x10010ed2`).

## Particles

Both clients joined fnTech3's server on `tim_dm1` with `cg_testparticles 1`.
fnTech3's capture was scaled from 800x600 to the original's 640x480. Masks of
near-white pixels then compare as follows, leaving out the console line, the
HUD and the weapon:

| Measure | Pixels |
| --- | --- |
| original particle pixels | 14,753 |
| of those, also white in fnTech3's capture | 14,642 (99.2%) |
| fnTech3's white pixels not white in the original's | 3,758, all but 4 within 3 pixels of an original particle |

The grids match particle for particle, near and far, as large octagonal discs
near the eye and dots in the distance. fnTech3's discs carry a pixel or two of
softer edge, from its texture filtering. The large white discs of `sli.dm3`'s
opening are therefore the original's own look, and that comparison closes.

## The overlay

**Its use.** Besides `cg_testblend`, the client game writes the overlay in
`0x10010880`: for 500 ms after the player takes damage, it places the blood
splat at the damage direction, scaled by the damage, and fades its alpha (at
most 0.5). This is the IHV Test's damage blood splat.

**What the original does.** `R_BlendBlobs` (`0x442700`) runs after every
scene while `gl_polyblend` is set (default 1, `0x42a021`) and the overlay's
alpha is not zero. It binds the shader's first image, sets the overlay colour
(`glColor4fv` at `0x4427b4`), blends by source alpha with no depth test
(state `0x10065`), and draws the square. It reads none of the stage's own
colour rules.

Under cdb, with `cg_testblend 1`, both checks passed and the draw ran with
alpha 0.5 and geometry (0, 0, 0.5). Yet none of the original's screenshots
shows the splat. Frames with the cvar on and off differ by less than 0.4 of 255
in the centre of the view, and not at all with `r_drawentities 0`. So the
original never shows it, whatever was drawn before it.

Why has not been settled. The state it sets is sound: `0x10065` decodes in its
`GL_State` (`0x42aac0`) as source-alpha blending with the depth test off. An
inference **[I]** is that the square, placed in OpenGL's eye axes under an
identity model-view, lands outside the view: the original's projection may
expect Quake's own axes, which would put the square around and below the eye.

**What fnTech3 drew.** fnTech3 draws the square as a 2D picture of the
overlay shader. `blood201`'s stage asks for `rgbGen entity`, and a 2D picture
here has no entity colour, so the splat came out as an opaque black blot.

**The change.** Each renderer now offers `RegisterIHVOverlayShader`
(`code/renderercommon/tr_q3_ihv_overlay_shader.h`). It returns, once per
source shader, a 2D shader holding that shader's first image, in the vertex
colour and alpha, alpha-blended with no depth test, as `R_BlendBlobs` draws
it. A source with no active first stage gives none, where the original
reports an inactive shader and draws nothing. The client draws the overlay
with it, so the splat is the blood image in the overlay's colour and alpha.
fnTech3 shows the splat the client game asks for, where the original shows
none. `gl_polyblend 0` turns it off, as it does there.

## Results

All runs were on loopback, windowed, on private desktops, with homes or
writable copies under `.tmp`, and driven by console commands without input.
The installation was only read.

| Run | Result |
| --- | --- |
| fnTech3 and the original, `cg_testparticles 1`, `tim_dm1` | the grids match; see above |
| fnTech3 before the change, `cg_testblend 1` | an opaque black blot in the centre of the view |
| fnTech3 after it, `cg_testblend 1`: GLx, Vulkan, RTX | a translucent red-brown blood splat in the centre of the view, alike in all three |
| fnTech3 after it, no test blend, `gl_polyblend` 1 and 0 | no splat either way |
| the original, `cg_testblend` toggled, entities on and off | no splat in any frame; cdb shows the draw issued |

A screenshot command runs at the end of the frame it is given in, so a
capture taken in the frame the cvar was set already shows the overlay.

## Tests

`tests/q3_ihv_overlay_shader_tests.py` compiles the overlay shader against
each renderer's layout. It checks the first image, the vertex colour and
alpha, the blend and depth state, the cache, the refusals and other profiles,
and that every renderer exports the shader and the client draws with it.

## Open

Why the original's splat leaves no mark, the build's menus and input-driven
play remain open. The profile stays a Windows x86 developer path, and release
builds still refuse it.
