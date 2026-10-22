# Widescreen field of view in every game, 2026-10-22

`r_fovCorrection 1` widens a game's 4:3 view to the screen, keeping its
vertical framing (FnQuake3's Hor+ rule, `code/renderercommon/tr_q3_fov.h`).
Four of the sixteen profiles never registered it: `q3-ihv`, `q3test`,
`jk2-sp` and `jk2-mp`. Setting it there did nothing, and Jedi Outcast
multiplayer's world was drawn stretched sideways on any wide screen, although
the [widescreen guide](../ASPECT_CORRECTION.md), the client's own comment and the
[Outcast client record](2026-10-16-jk2-cgame-ghoul2.md) all said it was
widened. This record registers the correction for every profile, keeps the
game's own underwater warp through it, and gives Jedi surface sprites the
game's field rather than the widened one. The
[JSON companion](2026-10-22-fov-correction-all-profiles.json) has the
identities, commands and captures. No compatibility gate closes: this is
presentation.

## Observed

- Jedi Outcast multiplayer's client game is told a 4:3 screen
  (`client_jk2_cgame.hpp`, glconfig import 219, `CL_HudModuleScreen`), so it
  builds a 4:3 pair of angles, and the client stretches that view over the
  window (`cl_hud.cpp` `CL_HudAdjustRefdef`). With no correction the world
  is 1.33 times too wide at 16:9 and 1.78 times at 21:9.
- The Q3Test client games compute a Quake III pair for the real screen
  (vertical from horizontal and the view's shape), so without the correction
  a wide screen shows less above and below than a 4:3 one did.
- The IHV Test's client game (`cl_q3ihv_cgame.cpp`) hands its own angle pair
  through unchanged, as the others did before they were corrected.
- The 4:3 rule rebuilt both angles from the horizontal one and dropped the
  game's vertical angle. Quake III's underwater warp (CG_CalcFov's
  `WAVE_AMPLITUDE`, kept by RTCW, Enemy Territory, Elite Force and the Jedi
  games) adds about a degree to the horizontal angle and takes as much from
  the vertical one, so the corrected view lost that squeeze and pulsed as a
  plain zoom instead.
- Raven's surface sprites (grass) lengthen their fade distances as the view
  narrows, measured against the first field between 50 and 135 degrees. They
  were handed the widened field: past about 32:9 at `cg_fov 97` it is over
  135, no baseline is ever taken and zoom stops lengthening them, and a
  window resized after the baseline skewed the ratio.

## Served

- `R_ApplyProfileFovCorrection` registers `r_fovCorrection` (default 1,
  `CVAR_ARCHIVE_ND`) under every profile. Quake Live keeps its own rule
  (its client game fits four preset shapes itself); every other game takes the
  4:3 rule.
- The 4:3 rule now recovers the game's warp before widening: it takes the
  shape the game derived its vertical angle for -- the view's own (a game told
  the real screen) or the 4:3 frame's (a game told a 4:3 one), whichever the
  pair's tangents are nearer -- solves for the unwarped horizontal angle whose
  pair, warped, has the same sum, and widens that, putting the warp back in
  degrees as the game applied it. A pair no warp between 0.01 and 5 degrees
  explains is taken as plain, which is every ordinary view; that leaves the
  results for ordinary views exactly as before.
- Jedi surface sprites measure their range against `fovXAuthored`, the game's
  own angle, as Wolfenstein: Enemy Territory's foliage and Medal of Honor's
  detail rule already did.

FnQ3/FnQL features: this is FnQuake3's Hor+ field of view, now in all sixteen
profiles; `r_fovCorrection 0` keeps each game's retail view as the explicit
retail-exact option.

## Verification

- `tests/q3_fov_profile_tests.py` compiles the header in its three backend
  modes and runs all sixteen profiles and an unknown one: each registers the
  cvar once, Quake Live keeps its preset rule, and the others widen 90 degrees
  at 16:9 to 106.26 by 73.74. Each case now hands the vertical angle a Quake III
  game derives for its view instead of an arbitrary 60, with every former
  expectation unchanged. New cases warp a 16:9 game's pair (+0.8 degrees) and a
  4:3-framed game's (-0.6) and get the same warp around the widened pair, and a
  pair no small warp explains (90 by 30) is taken as plain.
- `tests/jedi_surface_sprite_tests.py`: with the game's field 90 and the
  widened one 121.28 (21:9), the same sprites draw and fade as at 90.
- Live, windowed 1280x720 on a private desktop with no input, from the shared
  developer build (`fntech3.exe` `ad64a9c6...`, OpenGL `fnql_glx_x86.dll`
  `36f0a57b...`):
  - Jedi Outcast `ffa_bespin`, one run, one view: with the correction the
    bridge and sky line stay at the same height while the view widens; with
    `r_fovCorrection 0` the same view is stretched sideways (a pillar 85
    pixels wide becomes about 115, 1.35 times).
  - Q3Test 1.08 `q3demo1`, frozen between the two captures (`cl_freezeDemo`):
    the corrected view shows more of the arena on every side; without it the
    narrower vertical slice a 16:9 screen cut from Quake III's field.
  - IHV Test `sli`: the correction applies, but the demo kept moving between
    the two captures (freezing it there shows only the title, and `timedemo`
    reaches its scoreboard first), so that pair is not frame-matched.

## Not covered, and open

- Zoom and scope masks stay stretched over a world that is no longer stretched
  (Academy's and Outcast's binoculars and disruptor, RTCW's sniper and
  snooper, Enemy Territory's binoculars, Elite Force's zoom): their round
  windows come out oval on a wide screen. Placing them as centred 4:3 pictures
  with black sides when `r_fovCorrection` is on needs a HUD rule of its own.
- Quake III's and Quake Live's view flags reach the renderer unfiltered, so a
  mod setting bit 2 or 8 opts out of the correction or hides the weapon.
  Retail Quake III uses only bits 1 and 4; Quake Live's own bits are not
  measured, so neither is filtered yet.
- The warp is recovered for the two shapes this engine's games use. A game
  deriving its vertical angle some third way would have its own skew kept,
  within 5 degrees, rather than dropped.
- Neither single-player Jedi client draws a world yet, so `jk2-sp` and
  `jka-sp` have no live evidence.
