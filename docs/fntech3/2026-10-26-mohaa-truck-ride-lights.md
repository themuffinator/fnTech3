# Medal of Honor's truck-ride lights, 2026-10-26

The [frame-time record](2026-10-25-mohaa-frame-time.md) left a question: on
the truck ride that opens `m1l1`, the following truck's two headlights were
taken to be dynamic lights that cast a shadow cube each, yet a frozen frame
was byte-identical with `r_dynamiclight 0`. They are not dynamic lights. The
only light the client game adds there is the lantern hanging in the player's
own truck, behind the player, and the frozen view sees nothing it reaches,
as 2015's renderer would see nothing either. The engine was at fault in one
way: a model tested a light only against its own bounds, never against the
view, so the lantern, wholly outside the view, still listed the player's
truck as a receiver and was given a shadow cube. Such a light now lights
none of the view's surfaces. The frozen frame takes 6.0 to 6.4 ms in GL
where it took 10.5 to 10.9, and 3.1 to 3.2 ms in Vulkan where it took 6.6
to 7.4, and every capture is the same, byte for byte. The
[JSON companion](2026-10-26-mohaa-truck-ride-lights.json) holds the
identities, commands and numbers. This is Source, Unit, Build and Live
evidence; no compatibility gate closes.

## How it was probed

Every run was fnTech3's client on a private desktop, windowed at 1280x720,
with the retail 1.00 installation read-only, an isolated home and an empty
`configs/unnamedsoldier.cfg`, driven by rcon with no input. `devmap m1l1`,
`finishloadingscreen`, four seconds of the ride, then `timescale 0` holds one
frame, and each case is captured by the engine's `screenshot` and timed by
`com_speeds` (`cl`, the client's milliseconds). A temporary diagnostic build
printed each light the renderer received, its receivers and each entity's
model and origin, and the client game's every `R_AddLightToScene` call; a
temporary cvar switched the change on and off within one run. Neither is
committed.

## The lights on the ride

At the frozen moment the renderer holds nine dynamic lights (Observed):

| Lights | Source | Radius drawn | Shadow |
| --- | --- | --- | --- |
| 1 | the client game: the lantern, intensity 128 to 152, warm, type 0 | 64 to 76 | planned |
| 4 | `m1l1.lights.json`: spots `m1l1-light-0004` to `-0007`, the map's sphere lights | 291 | not eligible (spots) |
| 4 | `m1l1.lights.json`: points `-0076`, `-0077`, `-0079`, `-0080` | 108 to 121 | two eligible, no receivers |

The client game called `R_AddLightToScene` once a frame, for the lantern
alone. `maps/m1l1.scr`'s `lantern` thread makes it a `fx/dummy.tik` script
model with `lightOn` and `light`, follows the lantern hanging from the path
model's `tag_lantern`, and flickers its colour and radius through
`lanterncolors`; the client game turns that constant light into the call, as
OpenMoHAA's `CG_EntityEffects` reads it **[O]**. The other eight are
fnTech3's own static lights, promoted from the package's sidecar for the
map's sphere lights; they stand 5,100 to 9,000 units from the eye.

The following truck's headlights are no light at all (Observed, from 1.00's
data). `$introtruck2` is `vehicles/opeltruck.tik`, whose `opellights` surface
draws the beams, an additive `beam.tga` cone over scrolling dust
(`scripts/opel.shader`), and the script's
`global/spotlight.scr::deadcorona` attaches a `fx/searchlight.tik` sprite at
each of its `light left` and `light right` tags, a `corona_util` glow that
grows as the truck faces the player. Neither the truck's TIKI nor the sprite
carries a client-side `dlight`, so in retail too the road ahead of the truck
takes only the beams' glow. The beams and coronas draw in fnTech3's capture.

## Why the frozen frame shows none of it

The view looks out of the back of the truck, from (2541.8, 86.7, 1006.9)
along (0.91, 0.43, 0). The lantern hangs at (2496.6, 4.8, 1012.2), 94 units
from the eye and 76 behind its plane, and the field's far side plane puts it
93 units outside the view, more than its radius of 64 to 76: no pixel the
view draws is within its reach (Observed). Nor do the static lights reach
anything in view: no world surface took them, and the spots' only receivers
were the two path models below. Raising the lantern to its game's full radius (`r_dlightScale 1`)
lets its sphere into the view, and then 2,982 pixels of the canopy's edge
change by at most 6 levels across their three channels. So the radius is not why the frame shows
nothing, nor is the truck's body around the lamp (the lit pass never drops
the light; nothing visible is in range).

2015's renderer would show nothing there either **[O]**. OpenMoHAA's
reading adds a dynamic light to the lightmaps texel by texel within its
radius, `375 x (1 - d / R)^2` of its colour (`R_DlightSample`), and to a
model's whole light, `R x 7500 / d^2` of its colour, when the model's
lighting origin is within the radius (`RB_GetEntityGridLighting`), the radius
being the intensity the client game gave. The models within 152 units of
the lantern are behind the eye but for the player's own truck, whose
lighting origin is 138 units from it, and the turret that holds the player,
a `fx/dummy` skeleton at the eye. Which point 2015 lights a model from is not settled: that
reading never sets the trace origin it uses.

## Lights the view cannot see

The world walk has always passed a light by when its reach lies wholly
outside one of the view's four side planes (`R_CullDlight`). A model's lights
were tested against the model's bounds only (`R_LightCullBounds`, after the
model itself passed the view), so the lantern still added lit surfaces for
three visible models whose bounds it touched: the player's truck
(`gmctruck_opening.tik`, its `opel`, `gmcinside` and `heat_crate` surfaces)
and the two path models that carry the trucks along the ride
(`gmctruckpath_opening.tik`, whose bounds span the ride). Those receivers
made it a shadow candidate, and the planner gave it a cube every frame: six
faces, 258 to 264 draws of 361 to 367 caster surfaces, for no pixel.

[`tr_dlight_view_cull.h`](../../code/renderercommon/tr_dlight_view_cull.h)
marks each of a view's lights once, before any surface is added, when its
sphere (or a linear light's capsule) lies wholly outside one of the four
side planes, the test `R_CullDlight` makes, and `R_LightCullBounds` refuses a
marked light, so no model, brush model or world surface of that view takes
it, in all three renderers. A planned light's casters are still gathered
whatever the view sees. `r_nocull 1` keeps every light. Every fragment a
view draws lies inside those planes and the per-pixel pass ends at a light's
radius, so no picture can change; only the work goes. A portal's or mirror's
view copies the lights and marks its copies against its own frustum.

## Results

The frozen frame, the change switched in one run, each case 4 s of
`com_speeds` (mean `cl` ms):

| Renderer | Kept | Culled | Kept again | Culled again | No dynamic lights | Captures |
| --- | --- | --- | --- | --- | --- | --- |
| GL | 10.87, 10.69 | 5.96, 6.43 | 10.54 | 6.30 | 6.49 | 7 identical |
| Vulkan | 6.60, 7.30 | 3.09, 3.24 | 7.43 | 3.19 | 3.40 | 7 identical |
| RTX raster (`rtx_rt_mode 0`) | 7.54, 7.04 | 3.13, 3.08 | 7.44 | 3.11 | 3.31 | 7 identical |
| RTX traced | 8.64, 8.20 | 3.48, 3.38 | 5.51 | 2.74 | 2.16 | every capture differs, kept against kept too |

With the cull the planner reports `plan:0/9 ... render lights:0 faces:0`
where it had reported `plan:1/9 ... faces:6 batches:264`. The build as
committed, without the switch, plans no cube for the lantern in GL and
Vulkan.

On the moving ride (no freeze; the switch alternated every 3 s; Vulkan mean
`cl` ms): 8.57 kept, 4.78 culled, 5.74 kept, 5.86 culled, 8.08 kept, 5.27
culled. In the middle pair both cases planned two cubes a frame. A census of the
moving ride in GL, with the cull on, found such cubes held by the map's
static points, in view about 5,000 units off, lighting a few distant ground
and wall surfaces (below).

The change was compared where shadowed lights are common:

- **Quake III.** `demos/four.dm_68`, Quake III 1.32's own demo, under
  `timedemo 1`, 20 captures from frame 60 to 1200, with the change off, on
  and off again: 20 of 20 identical in Vulkan and GL, 20 of 20 in RTX's
  raster path. Every run planned 956 shadowed lights and 5,668 faces over
  591 of its 778 frames: none of them had been out of view.
- **Return to Castle Wolfenstein MP.** A recorded `mp_ice` demo replayed in
  Vulkan with the change off, on and off: its replays do not repeat their
  pictures even unchanged (3 of 12 captures identical between the two runs
  without it), but all three planned the same 113 shadowed lights, of the
  904 static lights promoted, 226 of them eligible.
- `tests/dlight_view_cull_tests.py` compiles the mark beside each renderer's
  own `R_CullDlight` and checks that they agree on 40,000 random lights in
  200 random views, point and linear, and that the lantern's case is out at
  its radius and in at twice it; and it checks the three renderers' wiring.

## The lantern's radius

fnTech3 draws a client-game light at half its intensity (`r_dlightScale`
0.5, every profile but RTCW single player), where 2015 lights to the full
intensity **[O]**. The curves differ more than their ends: fnTech3 adds
`smoothstep(1 - d^2 / r^2)` of the colour, 2015's lightmaps
`1.47 (1 - d / R)^2`. At a quarter of the intensity both give about 0.83;
at 0.4 of it fnTech3 gives 0.30 and 2015 0.53; past half of it fnTech3 gives
nothing and 2015 0.37 falling to 0.09 at three quarters. No single scale
follows 2015's curve: 1.0 would give 0.84 at half and 0.40 at three
quarters. The default was left as it is. Medal of Honor's static-light
sidecars were balanced for 0.5 ([sidecar record](2026-09-25-mohaa-map-sidecars.md)),
every muzzle flash and explosion would reach twice as far, and no frame of
2015's can be set beside fnTech3's here. `r_dlightScale 1` lets a player
choose the longer reach.

## FnQ features

Nothing a player sees changes. The per-pixel lights, their falloff,
specular and colour controls, and the shadow cubes with their filtering,
biases and atlas stay as they were (invariant 11); a light the view cannot
see no longer takes their work. The headlights' beams and coronas are
1.00's own shaders and sprites, drawn as before.

## Not done

- A shadow-eligible light in view but far away gets a full cube however
  few pixels it lights: on the moving ride the map's static points, about
  5,000 units off, held two cubes, and the middle window above took 5.7 to
  5.9 ms in Vulkan either way. The planner's priority
  falls with distance but has no floor while the budget is not full. A floor
  would change shadows in every profile and was not tried here.
- The two path models (`gmctruckpath_opening.tik`, `truck2path_opening.tik`)
  are drawn with `<default>` surfaces, their TIKI naming no shader, and their
  bounds span the ride, so every light near the route lists them. Whether
  1.00 draws those surfaces, and where they are, was not checked.
- The lantern's own model (`swinging_lantern.tik` and its flame) holds the
  light inside it. The F.A.K.K.2 round lights such a model by retail's model
  rule (`R_RitualAddEnclosedDlights`); Medal of Honor has no such rule, and
  the lantern was never in view to show whether it needs one.
- The probes cannot turn the player's view on the ride without input: the
  player is held by `m1l1truckride.tik`'s turret, and `cmd face` and
  `cg_3rd_person 1` left the view as it was. No capture shows the lantern
  lighting the truck's inside.
- `r_nocull 1` changes the frozen frame by itself, so it is no comparison
  for this change; the in-run switch was.
