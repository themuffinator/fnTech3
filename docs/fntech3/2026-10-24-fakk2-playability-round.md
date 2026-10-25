# Heavy Metal: F.A.K.K.2 playability round, 2026-10-24

## Scope

This round follows the [2026-10-23 gap round](2026-10-23-fakk2-gap-round.md)
and takes only the gaps a player of F.A.K.K.2 meets in fnTech3: what the game
looks like and does in single player. Retail's server, the game CD and the
machine key stay out of it, so the gaps that need them stay as that round
left them. It settles the dung pile on `creeperpens`, which fnTech3 drew brown
where retail draws it green, and the green glow its light casts round it. It
finds the trainer on `training` already settled. It checks saving, loading,
leaving and starting again on `creeperpens`, and sets views inside the
default fog volumes beside retail's. It traces the light that reaches a dark
fog passage on `cemetery`, and decides on retail's fade of blended stages in
a fog volume. Its machine-readable companion is
[2026-10-24-fakk2-playability-round.json](2026-10-24-fakk2-playability-round.json).

Every gate stays partial and the release gate stays closed.

## The runs

- **Build:** a `git archive` of `1b11e9e8` with this round's files laid over
  it, in `.tmp/fakk2-round/src`, built in `.tmp/fakk2-round/b` (Windows x86,
  MSVC 2022, `debugoptimized`, strict warnings, the three renderers). The
  export's client also prints the client game's lights and scene calls while
  the unregistered cvar `cl_diagLights` is set, a diagnostic that is not in
  the tree. The final build is staged in `.tmp/runtime/fakk2-fix`, which
  replaced the 2026-10-23 round's fold-only libraries there; their hashes
  stay in that round's companion. `.tmp/runtime/fakk2-diag` holds the build
  before the vertex-mode projection, with two more diagnostics, the cvars
  `r_diagNoEnclosed` and `r_diagNoAdditive`, which turn this round's model
  and additive rules off for the A/B runs below. Intermediate builds are named where a number comes from one. Hashes
  are in the companion.
- **Retail:** `fakk2.exe` 1.02 (SHA-256 `681bc51c...`) from
  `.tmp/runtime/fakk2-retail`, as a client only, once under cdb
  (`.tmp/fakk2-round/tools/retail_cdb.py`).
- **Installations:** the F.A.K.K.2 layout laid down from the user's disc under
  `.tmp/fakk2-install/game`, and American McGee's Alice at `E:\Games\Alice`,
  both read only; every manifest records them unchanged.
- **Conditions:** windowed at 800x600 on a private desktop, isolated homes
  under `.tmp/fakk2-round/`, loopback only, no mouse or keyboard input, and
  frames from the engines' own screenshot commands.

## The dung pile on creeperpens

**Observed.** Left of Julie's start on `creeperpens`, the dung pile beside the
green glowing lumps (`haypoo_model`, `obj_poopile.tik`, scale 0.8) is dark
green in retail and was brown in fnTech3. Its body is `pooshine`: a brown
texture under `rgbGen lightingDiffuse`, then an environment map added over
it. The level's `haypoo` script object, a brush model at (-224, 1552, 32),
carries the client game's constant light: radius 96, colour (0, 0.247, 0) and
type 4, additive. fnTech3's client game adapter added that light every frame,
in the scene the pile is drawn in.

**Retail's rule.** `fakk2.exe` 1.02 lights a model from every dynamic light in
reach by one rule (0x0048f23d..0x0048f2e0): a light no farther from where the
model is lit than its radius adds its colour times radius x 7500 / d^2 to the
directed light (the constant at 0x004d6050), and leaves the light's direction
as it was. If that takes any channel of the directed light past 255, the
directed and ambient light are both scaled until that channel is 255
(0x0048f2e6..0x0048f395). `alice.exe` has the same rule with 30,000
(0x004accfc..0x004acf07, the constant at 0x005004a0). For a flagged refdef it
also keeps up to eight lights' directions and colours, which nothing here
uses.

**Retail's values.** Under cdb, breakpoints armed by the screenshot command
printed the pile's light in `fakk2.exe`'s own entity lighting, before and
after its dynamic lights (0x0048f1f6, 0x0048f39b). The pile is lit at
(-218.53, 1552.73, 43.01), 12 units from the light. Its game module flags it
0x40000, so it starts from the flat light, 150 ambient and directed along the
default sun (0x0048ed5a, 0x0048f143). The green light then leaves it 28.90
ambient, grey, and a directed light of 28.90, 255, 28.90.

**Why fnTech3 missed it.** fnTech3's dynamic lights are its own. Per pixel, by
default, they light the faces turned toward them, and every outward face of
a pile turns away from a light inside it. In vertex mode the light was
weighed by Quake III's rule, which turns the light's direction toward it and
never rescales. None of `r_dlightScale 1`, `r_dlightIntensity 4`,
`r_staticLights 0` or `r_dlightMode 0` turned the pile green.

**Change.** `R_RitualAddEnclosedDlights`
(`code/renderercommon/tr_ritual_model_light_render.h`, called from each
renderer's `R_SetupAliceEntityLighting`) lights a model by retail's rule from
the lights it holds: those a Ritual client game gave, lying within the lit
frame's radius of where the model is lit (twice the reach the TIKI path
already passes). It weighs each light at the radius and in the colour its
game gave, which the scene keeps beside the light
(`dlight_t.ritualRadius`, `ritualColor`) for the client game's lights alone.
fnTech3's own controls do not reach it: `r_dlightScale` halves a light's
radius and `r_dlightSaturation`, 0.8 by default, greys its colour. A first
build weighed the greyed colour. Under a diagnostic print, fnTech3 lit the
pile at retail's point and from its flat light, but left a directed light of
104.24, 255, 104.24, and the pile stayed nine levels too red. Map sidecar
lights and every other profile's lights are untouched. Every other light
stays the renderer's own, per pixel or in vertex mode, and vertex mode no
longer also weighs a held light. Retail divides by the distance squared even
at zero; this rule stops at one unit. Neither Ritual engine registers an
overbright bit, so with `r_overBrightBits` raised the light and its ceiling
come down as the minimum light does. The rule applies in all three renderers
and every dynamic light mode, under both Ritual profiles.

**Result.** On the pile, below the HUD (x 85..150, y 375..435), and in its
middle (x 100..140, y 380..420):

| Run | Pile | Middle |
| --- | --- | --- |
| Retail | 17.8, 43.8, 7.0 | 13.5, 31.6, 3.9 |
| fnTech3 before, per-pixel lights (default) | 56.3, 52.9, 15.9 | |
| fnTech3 before, `r_dlightMode 0` | 40.1, 42.3, 12.1 | |
| The first build (greyed colour) | 26.7, 43.9, 9.3 | 23.9, 31.8, 6.7 |
| The rule alone (the game's colour) | 18.7, 43.8, 7.5 | 13.3, 31.7, 3.9 |
| Final build, GL renderer | 19.4, 45.9, 8.2 | |
| Final build, Vulkan renderer, `r_bloom 0` | 19.5, 46.1, 8.3 | |
| Final build, RTX renderer at its defaults, `r_bloom 0` | 18.6, 44.3, 7.4 | |

The pile is retail's colour. The final build's pile is two levels greener
again, from the additive light below.

**Other profiles and views.** In one run each, the rule on, off
(`r_diagNoEnclosed 1`) and on again, after each level's opening
(`.tmp/fakk2-round/tools/alice_enclosed_ab.py`). The counts are pixels where
on and off differ by more than 20 while the two "on" frames agree within 8.
On F.A.K.K.2's `fakkhouse`, `homes1`, `cemetery`, `towncenter_good` and
`swamp1`, which add no client game light at all, they were 415 to 3,799, the
measure's own floor: animation that happens to repeat. `creeperpens` gave
2,189. The Alice views do add lights, small blue item glows of radius 75 to
150 and a gold one on `skool1`; their counts were 539 to 2,164, within the
floor. In the most changed crops their means move by at most 0.3 of 255
(`skool1`, a dark model by the gold light), and on `hatter1` not at all.
These counts are from the first build. On the final build the Alice counts
were 508 to 2,188, and the F.A.K.K.2 floor 2,770 to 3,868 (`creeperpens`
3,081), but for `jlair2`, 12,980: there the Jabberwock's fight is under way,
and Alice's spell effect differs between all three frames, the two "on"
frames averaging 57.5 and 67.8 red. The measure cannot separate the rule
from that animation.

**Tests.** `tests/ritual_model_lights_tests.py`'s renderer fixtures, one for
each renderer's `R_SetupAliceEntityLighting`, now check:

- the pile's case in vertex and per-pixel modes: 7200 x the game's colour,
  not the renderer's greyed one, rescaled to 255 with the ambient, the
  direction unchanged;
- a dim light added without the rescale, and Alice's four times F.A.K.K.2's;
- an overbright bit halving the light and its ceiling;
- a light at the lit point weighed at one unit;
- a held light past its radius adding nothing in either mode;
- a light outside the frame, a model with no frame, and a light no client
  game gave, all lit by the renderer's own rule as before.

## Additive lights on the world

**Observed.** Retail's pile light also paints the wooden wall behind the pile
green. fnTech3's left it brown.

**Retail's rule.** Retail draws a dynamic light over the world and brush
models as a projected light. Bit 4 of the light's type chooses its blend
(`fakk2.exe` 1.02 0x0047d32b..0x0047d33d; `alice.exe`
0x004935ab..0x004935bd, the same code). An additive light is added
`GL_ONE GL_ONE`, whatever the surface; any other light is modulated by what it
lights, `GL_DST_COLOR GL_ONE`. It projects the light over its whole radius.

**Change.** fnTech3 keeps its per-pixel lights, with their own falloff and
facing. For a Ritual client game's additive light, each renderer's light pass
lights the world and brush models from white in place of their texture, so
the light adds its own colour (`RB_RitualAdditiveWorldLight`,
`code/renderercommon/tr_ritual_additive_light.h`). The GL renderer's three
light passes and the Vulkan and RTX renderers' each choose the texture that
way. Models keep their texture: retail lights them by its model rule. Such a
light also keeps the radius its game asked for, with its shadow's range to
match. `r_dlightScale` halves every other light's radius, a balance for lights
that modulate a surface; at half its radius the pile's light did not reach
the wall. Every other light, and every other profile's, is as before.

**Result.** On the wall behind the pile (x 0..70, y 330..440):

| Run | Mean RGB | Green over red |
| --- | --- | --- |
| Retail | 33.7, 51.8, 15.5 | 18.1 |
| fnTech3 before | 54.3, 52.1, 25.2 | -2.2 |
| Final build, GL renderer | 64.1, 81.7, 35.0 | 17.6 |
| Final build, Vulkan renderer, `r_bloom 0` | 64.2, 81.8, 35.0 | 17.6 |
| RTX renderer at its defaults, `r_bloom 0`, before | 34.2, 34.8, 15.4 | 0.6 |
| Final build, RTX renderer at its defaults, `r_bloom 0` | 38.1, 43.2, 17.7 | 5.1 |

The wall takes retail's green. It stays brighter than retail's, from
fnTech3's static lights, as the gate matrix already records for other walls.
**Vertex mode.** `r_dlightMode 0`, an option, draws dynamic lights as
projected lights, and its pass already adds an additive light
`GL_ONE GL_ONE`. But it mapped every vertex into the light's image by world
x and y, as Quake III does, and faded it by height. So the wall, a face
(`textures/creeperpens/edenbarn2`) about 66 units from the light along x,
fell outside the image, and stayed 41.2, 41.4, 18.3. Retail maps a vertex
across the plane of its normal's dominant axis: y and z for a normal mostly
along x, else x and z for one mostly along y, else x and y. The image spans
the radius over 0.75 about its middle, and the light fades with the
vertex's distance along its normal, (radius - |dist . normal|) / radius
(`fakk2.exe` 1.02 0x0047d0a1..0x0047d268; `alice.exe`
0x004932b3..0x0049348b, the same constants at 0x0050036c, 0x004ff9e8 and
0x00500368). Its light image is the same shape as fnTech3's, 32 texels
across at 16000 / d^2 where fnTech3's is 16 at 4000 / d^2, both cut below 75
(0x0046fcf2..0x0046fd6d). The Ritual profiles' projected pass now maps and
fades by retail's rule in all three renderers
(`RB_RitualProjectDlightVertex`,
`code/renderercommon/tr_ritual_projected_light.h`). It weighs a client
game's light in the colour its game gave, and the Vulkan and RTX passes
clamp each byte, where they had let it wrap. The GL renderer fills a
vertex's normal only for a shader that asks for one
(`USE_TESS_NEEDS_NORMAL`); a first build read stale normals there and lit
the boards a tenth less green than Vulkan. GL now fills them for every
surface under the Ritual profiles while dynamic lights are projected. GLX's
developer-only GPU projection (`r_glxDlightProjectedProgram`, set off by
every GLX profile) evaluates Quake III's rule, so it now leaves the Ritual
profiles to the CPU pass; with it set, the frame is the default's.

In vertex mode with fnTech3's static lights off, since retail has none:

| Box | Retail | GL | Vulkan | RTX |
| --- | --- | --- | --- | --- |
| Boards (x 10..40, y 320..420) | 33.2, 42.1, 15.2 | 32.7, 43.0, 15.3 | 32.7, 42.9, 15.3 | 35.5, 33.4, 16.2 |
| Below them (x 40..80, y 380..440) | 31.8, 80.6, 18.5 | 31.9, 80.4, 18.7 | 31.9, 80.5, 18.8 | 33.8, 69.5, 20.1 |
| The wall box | 33.7, 51.8, 15.5 | 33.4, 52.5, 15.7 | 33.4, 52.5, 15.7 | 35.8, 41.1, 16.7 |
| The pile | 17.8, 43.8, 7.0 | 17.5, 43.5, 7.0 | 17.7, 43.6, 7.1 | 17.8, 43.6, 7.2 |

GL and Vulkan match retail to a level. The RTX column is the path tracer
before it took the light from white ([below](#the-path-tracer)).
With the static lights on, GL and Vulkan agree again (102.9 and 102.7 green
below the boards): the static lights, drawn after the green one with retail's
modulating blend, `GL_DST_COLOR GL_ONE`, brighten the green they find as
well as adding their own warm light. Before this round both renderers drew
the view alike under Quake III's rule: with the static lights off, 64.3
green below the boards in each.
The floor to Julie's right and the far wall are unchanged to 0.1 of 255. RTX's
path tracer took only part of the change until it was given its own rule
([below](#the-path-tracer)).

**Other profiles and views.** In-run A/B with `r_diagNoAdditive`:
`creeperpens` changed 7,930 pixels, `fakkhouse`, with no client game light,
3,777, the floor. The Alice levels `garden1`, `fortress1`, `skool1`,
`hatter1` and `wforest` changed 511 to 1,754. In their most changed blocks
the "on" frames repeat exactly, and the light adds about three levels
(`fortress1` 121.6 against 118.5 red, `skool1` 14.6 against 11.6, `hatter1`
133.4 against 129.8), with no change on `garden1`.

**Tests.** `tests/ritual_additive_light_tests.py` compiles the predicate
against stand-in renderer types: the world and brush models take an additive
Ritual light from white, and models, sprites, modulated lights and every
other light do not. It checks that each renderer's light passes choose their
texture through it. It also checks that the three renderers keep the game's
radius, colour and shadow range alike, and that every other light starts with
no Ritual radius.

### The path tracer

**Observed.** RTX's path tracer, at its defaults, took only part of the
glow: green over red 5.1 on the wall in this round's build, where GL and
Vulkan gave 17.6. It traces a dynamic light as irradiance, and the frame
multiplies irradiance by the surface's reflectance, the wall's brown.

**Change.** The tracer marks such a light `RT_LIGHT_ADDITIVE` when it uploads
it: additive, and with a Ritual radius, the light's half of
`RB_RitualAdditiveWorldLight` (`code/rendererrtx/rt/rt_backend.c`
`RT_UploadLights`). On a world or brush-model surface its sample is radiance
the reflectance does not scale: at the first hit it joins the specular
signal, which the denoiser filters and the composite adds as it is, and at a
bounce it joins the path's radiance (`code/rendererrtx/shaders/rt_path.glsl`
`shade_direct`). A model keeps its reflectance, as in raster.

That alone took the wall to 9.7. The tracer resamples one light at each
point. On the wall the pile's light competes with dozens of the map's, so
only the points that chose it took it, and the denoiser's neighbourhood
clamps lost much of that sparse light. With `rtx_rt_debug_light` set to the
pile's light alone, the wall took about as much as GL's light adds there.
So a frame with such a light (`RT_FRAME_ADDITIVE_LIGHTS`) gives each world or
brush-model point a second light sample, among the additive lights alone, and
leaves them out of the first. The second sample's mask names the bit a light
must carry (`RT_LIGHT_REQUIRE_SHIFT`, `rt_lighting.glsl` `rt_light_eval`).
Both samples pass through the sampler's one call site, in a loop: a second
inlined copy of the sampler made the compute trace too large for the 32-bit
driver to compile (`rt_path.glsl`). A bounce's point, a surface the raster
light pass skips, `rtx_rt_debug_light`, and a frame with no additive light
sample as before. This keeps the tracer's own features: the light keeps its
falloff and facing, its traced shadow from the light's body, and the
denoiser. Retail's projected light has no shadow; fnTech3's keeps one.

**Runs.** The change was built on `bd43efdc`, which holds this round and
the static spot cones, configured like `win32-dev`. The client and its
renderers were built from `bd43efdc` alone and with the change, and staged
in `.tmp/runtime/fakk2-rtx-additive-before` and
`.tmp/runtime/fakk2-rtx-additive`: the same executable and GL and Vulkan
libraries, and two RTX libraries. The harness is
`.tmp/fakk2-round/tools/poo_view.py` with `FN_RENDERER`, and the evidence is
in `.tmp/fakk2-rtx-additive` (`views/1025` for these runs). This machine's
32-bit driver offers no ray-tracing pipeline, so the tracer ran its compute
backend, and the change's pipeline was created there.

**Result.** On the record's boxes, at the defaults with `r_bloom 0`:

| Box | Retail | GL | Vulkan | RTX before | RTX after |
| --- | --- | --- | --- | --- | --- |
| The wall box | 33.7, 51.8, 15.5 | 43.2, 64.6, 25.5 | 43.2, 64.6, 25.5 | 38.3, 43.5, 17.6 | 43.8, 64.0, 24.8 |
| Its green over red | 18.1 | 21.4 | 21.4 | 5.2 (5.4 again) | 20.1 (20.3 again) |
| Boards (x 10..40, y 320..420) | 33.2, 42.1, 15.2 | 41.5, 54.2, 24.0 | 41.5, 54.2, 24.1 | 38.0, 36.1, 17.1 | 42.9, 53.7, 23.3 |
| Below them (x 40..80, y 380..440) | 31.8, 80.6, 18.5 | 40.8, 90.8, 27.7 | 40.8, 90.8, 27.7 | 35.3, 71.1, 20.5 | 41.4, 91.6, 27.6 |
| The pile (a model) | 17.8, 43.8, 7.0 | 18.2, 44.9, 7.7 | 18.4, 45.0, 7.8 | 18.1, 43.9, 7.3 | 18.7, 45.7, 8.0 |

The wall takes retail's green in the path tracer too, and its wall, boards
and the floor below them come within about two levels of GL's and
Vulkan's. GL's wall there is darker than this round's own build measured
above (64.1, 81.7, 35.0): the static spot cones confined the map's spot
lights. In vertex mode with the static lights off, the tracer's wall is
43.8, 63.8, 24.8 and its boards 42.5, 53.2, 23.3, where they were 38.3,
43.2, 17.5 and 37.2, 35.1, 16.7. In 50-pixel blocks, green changed by more
than two levels in 13 of 192, all round the pile (x 0..200, y 300..500), by
2.5 to 25.1; two runs of one build differ there by at most 2.3. In the
specular view (`rtx_rt_debug_visualizer 5`) the wall's green reads 37.4 with
the denoiser off and 36.4 with it on, where it read 12.0 and 9.9, the light's
highlights alone, before. A first build on this round's tree, which only
routed the existing sample, read 27.9 and 22.8 there, and 9.7 green over
red on the wall.

**Other profiles and cost.** Quake III's frozen demo frame
(`scripts/probe_rtx_games.py`, `.tmp/q3-hud-align/before-219/record/hudtest.dm_68`)
traced at 1.387 and 1.386 of its raster frame before and after. Its traced
frames differ by 0.46 of 255 a pixel, its raster frames by 0.35: run to run.
Alice's `fortress1` and `skool1` traced without error, and changed in 376
and 69 pixels by more than 20 levels; two runs of one build differed on
`fortress1` in 1,540 on this round's tree. The cost was timed on this
round's tree with the machine otherwise idle: `creeperpens`' trace took 12.7
to 13.7 ms before and 13.1 to 13.9 after, over three runs each at 800x600,
each before-and-after pair 0.3 to 0.4 ms apart, and Quake III's demo frame
20.1 to 21.1 ms before and 20.6 to 21.4 after. On `bd43efdc` other sessions'
game clients were running, and single runs swung between 13.5 and 24.7 ms,
so no timing is taken from them.

**Tests.** `tests/rtx_ritual_additive_light_tests.py` runs the shader's
receiver masks as C against its own lines. Split, every light the one sample
would take is taken once, an additive one in the second sample, any other in
the first. Without required bits, the test is the old one for every light
word. It checks that the host and shader constants agree and that the light
bits stay below the required ones. It checks that only an additive Ritual
light is marked, and that world and brush-model surfaces take it without
their reflectance at both hits while models keep theirs. It checks that the
sampler and its shadow traversal each keep one call site, and that the
embedded SPIR-V carries the change. `tests/rtx_emitter_lighting_tests.py` and
`tests/rtx_renderer_contract_source_tests.py` follow the new sampler call.

## The trainer on training

The contracts record said the trainer on `training` drew blue-grey beside the
blue wall, where retail shows skin tones and blonde hair. That note predates
the compiled-light model lighting of 2026-09-25: she is a sentient, lit from
the map's compiled lights. On this round's first build, set beside retail's
frame (`frames_cmp.py training`), her figure (x 420..480, y 300..460)
averages 60.8, 63.1, 64.0 against retail's 55.8, 58.4, 59.5. She has the same
tones and blonde hair in both engines. The whole 3D view is 3.29 apart. The
note is corrected.

## Leaving and loading on creeperpens

Retail's module faults on `creeperpens` when the level runs frames with
nobody connected. It posts `stopwatchingactor` to a player it no longer has.
In fnTech3's single player the listen server shuts down with its last client,
so that frame never runs. `.tmp/fakk2-round/tools/creeper_quit.py` ran these
steps on `creeperpens`, each with its marker in the log:

1. `devmap`, then skip the opening;
2. `savegame`;
3. `loadgame`, which restarted the level from the save;
4. `disconnect` to the menu;
5. `devmap` and skip again;
6. `disconnect` once more.

Every marker was reached, with no error, and the save wrote its `.sav`,
`.ssv` and `.tga` files. The dedicated server with no player stays as the
earlier record left it.

## Inside the fog volumes

`.tmp/fakk2-round/tools/fog_inside.py` sets retail's client and fnTech3's
side by side, each on fnTech3's dedicated server. The client asks the server
to place Julie by rcon, through a server alias for
`script $player origin "x y z"`, since a Ritual alias keeps its quotes.
Waits in the server's own cfg never end: only a client frame counts a
millisecond wait down, as in `alice.exe` (0x0043a183), whose rule fnTech3
keeps for both Ritual profiles, so on a dedicated server only `wait 1`
passes.

- **`cemetery`, fog 0:** a black fog of depth 256 filling a passage
  (3008..3520, -128..384, 256..640), with Julie at (3264, 128, 300). Retail
  averages 2.9, 2.1, 1.4 and fnTech3 7.6, 4.4, 3.0, 2.93 apart. With
  `r_staticLights 0`, fnTech3 averages 4.1, 2.4, 1.7, 0.79 apart, near pixel
  for pixel: the pillar is 8, 5, 3 beside retail's 9, 6, 3. The fog volume
  matches retail's. The far pixels are the same with the static lights and
  without them, so the light pass is fogged. The rest is fnTech3's static
  lights; see below.
- **`oracleway`:** skipping its opening hands the level over to `oracle`, so
  the point taken from `oracleway`'s volume put Julie in `oracle`'s void.
  From outside the world fnTech3 drew a grey grid where retail drew the
  floating island fogged black. No player can reach that view. `oracle`'s
  own first view matches retail's, 6.83 apart as before, and `oracleway`'s
  volume shows only during its opening.
- **`cliff1`:** Julie placed on a bridge fell into the river at the bottom of
  the chasm, inside its mist (depth 2,560). There retail averages 35.4, 53.5,
  51.2 and fnTech3 36.8, 55.0, 52.4, 7.08 apart, the water's own animation.
  The first views of `cliff1` and `cliff2`, 2.96 and 5.22 apart, look across
  the chasm.

**The light in the dark passage.** The static-light counters
(`r_staticLightDebug`, `r_dlightShadowDebug`) show eight of the map's 128
lights promoted in that view and two eligible for shadows. No shadow is
planned: neither eligible light lights a surface in view. With
`r_staticLightShadowMaxLights 8`, seven are eligible and none lights a
surface. The eighth, not eligible, is a spot: `cemetery-light-0117`, from the
map's compiled lights at (2016, 224, 1984), pointing down, with a 15-degree
cone, intensity 12,000 and radius 4,743. A 15-degree cone from there lights
nothing within 1,000 units of the passage. fnTech3's static lights draw a
spot as a linear light, a capsule along the spot's axis lit all round
(`R_AddStaticMapLightsToScene`, `R_StaticMapLightSpotEnd`); only spot
shadows, off by default (`r_spotShadows 0`), use the cone. Its capsule
passes 1,073 units from the passage, inside its reach of 2,372, and no other
spot's capsule reaches it. Nothing blocks it: `r_staticLightShadows 0`
leaves every pixel as it was. (A copy of the sidecar without that light,
laid into the client's home, was not read: the engine's package supplies
the map's sidecar ahead of the home.) That is fnTech3's static-light model for every profile with
spot-light sidecars, not a F.A.K.K.2 contract, and it is recorded here, not
changed by this round. The
[static spot cone record](2026-10-24-static-spot-cones.md) has since lit
every sidecar spot only inside its cone, and the passage now matches its view
with the static lights off.

## Blended stages in fog: decided

Retail fades a blended stage's colours in a fog volume on the CPU, for the
stage's RGB (0x004944f0) and its two other fog adjustments (0x004945c0,
0x00494650; the calls at 0x0047d9b5..0x0047d9df). Each vertex is multiplied
by max(0, 1 - (t - 0.25) x s x 256), from the fog's own texture coordinates
(0x00494d30), with no upper bound, and the product goes back as a byte. Its
s grows by 1/(8 x depth) a unit, as for opaque surfaces. So inside a volume,
where t is 31/32, a blended stage fades out within depth / 23 units, 11 in
`cemetery`'s black fog, where opaque surfaces take about eight depths. A
vertex with t below 1/4 is multiplied by more than 1, and past 255 its byte
wraps. That happens at a point outside the volume, or near its surface with
the eye outside.

fnTech3 keeps fading blended stages by the fog image, retail's own since
2026-10-23. How an engine fades a stage in fog is its presentation, which
fnTech3 takes from its own fog (invariant 11); the data, the volume and its
depth, is retail's. The blended stages inside the four maps' default volumes
are scenery: `cliff1`'s rivers and see-through rock under the chasm mist, and
`cemetery`'s ivy in a black passage. In the one view of them a player
reaches, in `cliff1`'s river, the two engines agree within the water's
animation. The river's top layer there blends `GL_SRC_ALPHA
GL_ONE_MINUS_SRC_ALPHA` at alpha 0.4. The formula's other half is a byte
overflow, not data. No module, file or player depends on it, so under
invariant 12 it is an accepted difference, recorded once here, and not an
open gap.

## Alice

Retail Alice cannot run on this machine: `alice.exe` is SafeDisc-wrapped. Both
rules here rest on `alice.exe`'s own code, the same as `fakk2.exe`'s but for
the model rule's constant, and the A/B runs above are the evidence for Alice.

## Not established

- **Retail as the server**, and saves from later levels in retail, still need
  the disc, which this round left out
  ([gap round](2026-10-23-fakk2-gap-round.md#not-established)).
- **`creeperpens` with no player** on retail's own dedicated server is still
  inferred **[I]**.
- **Alice** beside retail Alice: no retail Alice can run here.
- **Static spot lights** lit all round their axis, so `cemetery`'s
  black-fog passage took light a 15-degree spot would not give it, and spot
  shadows did not limit it (`r_spotShadows 1`). Drawing a spot's cone needed
  every renderer's per-pixel light programs, so it was a change of its own:
  settled by the [static spot cone record](2026-10-24-static-spot-cones.md).
- **RTX's path tracer** took only part of an additive light's glow: its
  traced lights took the surface's reflectance. Settled by its own change
  after this round ([path tracer](#the-path-tracer)). Its ray-tracing
  pipeline carries that change (`rt_main.rgen`) but has not run it: this
  machine's 32-bit driver offers only the compute backend.
- **Listening** and **other platforms**, as before.

Accepted, under invariant 12: retail's CPU fade of blended stages in a fog
volume, above.

## Validation

The final build compiled every target with strict warnings.
`tests/ritual_model_lights_tests.py` (4 tests, 69 subtests),
`tests/ritual_additive_light_tests.py` (3 tests, 5 subtests),
`tests/ritual_projected_light_tests.py` (4 tests, 3 subtests) and
`tests/alice_renderer_parity_tests.py` passed. The projected-light test
compiles the helper against stand-in types: the wall's case, a floor, a
normal mostly along y, retail's tie-breaks, the fade past the radius on
either side, back faces culled only when asked, and each edge's clip bit.
It also checks that the three renderers' passes take a Ritual vertex
through it, in the game's colour, before Quake III's mapping, and clamp the
Vulkan and RTX bytes; that GL fills the normals it reads; and that GLX's GPU
projection leaves the Ritual profiles to it. So did every test file that
reads a changed source or names a changed function, 94 in all, in the shared
tree: 89 under pytest, one file at a time, and five that run as scripts. The
changelog and generated-docs tests passed too.

The path tracer's change was validated on `bd43efdc`. The client and its
three renderers compiled with strict warnings, and
`code/rendererrtx/shaders/build_shaders.py` regenerated the 97 shader
variants, which `--check` reports current. `tests/rtx_ritual_additive_light_tests.py`
(6 tests) passed, and so did every test file that reads a changed tracer
source or names a changed function: 35 under pytest, one file at a time,
and `tests/shadow_projection_source_tests.py` as a script, with the
changelog, generated-docs, release-workflow and release-packaging tests.
