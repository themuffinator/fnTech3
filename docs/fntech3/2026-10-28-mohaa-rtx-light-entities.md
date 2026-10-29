# Medal of Honor's light entities in RTX, 2026-10-28

The ledger had one open fault in Medal of Honor's RTX path tracer: it read a
map's lights "as the Quake III compilers write them", and a night level's
lamps lit little of what the raster renderers show lit. Two things were
wrong, and neither was the light's falloff. The path tracer's entity parse
stopped at the first empty value in a map's entities, and 1.00's spline
paths carry `"model" ""`: on twelve of the 54 maps it read few or none of
the lights (`m1l2a` 27 of its 343, `m4l3` and `obj/obj_team2` none). And it
read none of 2015's own light keys, so the 312 lamps a map points with
`angles` were traced as lights shining every way. Both are fixed. 2015's
rules were read from the maps themselves: each map's sphere-light lump is
the compiler's own record of every light it kept, with its spot flag,
direction and cone, and its lightmaps show the falloff. Three Elite Force
maps had lost lights to the same parse and gain them too; no other
installed map changes. The [JSON companion](2026-10-28-mohaa-rtx-light-entities.json)
holds the identities, commands and numbers. This is Source, Unit and Live
evidence; no compatibility gate closes.

## The parse stopped at an empty value

`RT_ParseLightEntities` (`code/rendererrtx/rt/rt_lights.c`) took an empty
token for the end of the entity text. `COM_ParseExt` hands back an empty
token both for an empty quoted string and at the end of the text; only at
the end does it clear the parse pointer. 1.00's `info_splinepath` entities
carry `"model" ""`, and the first one on a map ended the parse there.
Read the way the path tracer read them, every installed map's entities give
(Observed, `.tmp/mohaa-gaps/entity_parse_census.py`, the JSON's
`entity_parse_census`):

| Game | Maps | Maps losing lights | Lights lost |
| --- | --- | --- | --- |
| Medal of Honor | 54 | 12 | 1,401 |
| Elite Force | 103 | 3 (`dn3`, `dn5`, `stasis1`) | 568 |
| Quake III, Quake Live, RTCW, ET, Jedi Academy, Jedi Outcast, Alice, F.A.K.K.2, Q3Test | 570 | 0 | 0 |

The twelve Medal of Honor maps are `m1l2a` (27 of 343 read), `m1l3b` (0 of
54), `m2l3` (0 of 171), `m3l2` (2 of 49), `m4l0` (2 of 83), `m4l2` (0 of
101), `m4l3` (0 of 313), `m5l1a` (1 of 31), `m5l3` (0 of 21), `m6l1c` (110
of 164), `obj/obj_team2` (0 of 181) and `training` (5 of 37). Elite Force's
three carry editor leftovers, `"" ""` pairs and an empty `"chell"`; `dn5`
had kept 21 of its 466 lights. The parse now ends only where the parse
pointer is cleared, in every profile, and so does the worldspawn read of
Medal of Honor's sun (no 1.00 worldspawn has an empty value).

## 2015's light rules, from its maps

`MOHAA.exe` is encrypted, so its compiler's rules were read from what it
wrote. Lump 19 holds the sphere lights, the lights the compiler kept for
lighting models: origin, colour, intensity, a spot flag, the spot's
direction and its radius a unit along the axis (Ritual's 56-byte
`mapspherel_t`). Each of the 5,415 light entities in the 54 maps was matched
to the sphere light at its origin (Observed, `.tmp/mohaa-gaps/spotmatch.py`,
the JSON's `sphere_light_rules`; the first five rows count the lights the
last does not):

| A light with | Lights | The compiler's light |
| --- | --- | --- |
| a `target` that names an entity | 360 | a spot toward it, `(radius + 16) / distance` wide a unit along (q3map's; largest error 5e-7) |
| a `target` that names nothing | 11 | a point light |
| `angles` and no target | 312 | a spot along those angles: `(radius + 16) / 64` (radius 64 when unset; 228 lights, exact), or `tan( spot_angle )` where `spot_angle` is set, the radius then ignored (84 lights, largest error 2e-6) |
| `angle` alone | 67 | a point light |
| neither | 3,812 | a point light |
| `spawnflags 2` or `no_entity_light 1` | 853 | none at all |

The 853 left out of the sphere lights still light the world: a `spawnflags
2` lamp on `DM/mohdm2`, `light 30`, stores 27, 21 and 17 at 104, 120 and 136
units, where the law below gives 21, 16 and 12 from it alone and its
neighbours, 340 units off, add the rest. They are kept off models only,
which is what the key `no_entity_light` says. The 28 of them with `angles`
and no target are taken to keep their spots, which their missing sphere
lights cannot show **[I]**. The intensity of every sphere light is the
entity's `light`. No 1.00 light uses `spawnflags` 1 (q3map's linear light);
`overbright_range`, on 2,440 lights, changes nothing the sphere lights
record, and what it does to the lightmaps is not established **[I]**.

The falloff is id's q3map's: a light of intensity `I` stores `I * 7500 *
cos / d^2` in the lightmaps, in the units the path tracer already uses for
the worldspawn sun. On `m2l1` a `light 300`, 4,750 units from any other
light, stores 98, 67 and 55 above the map's ambient level at 152, 184 and
200 units (the brightest tenth of the luxels nearly facing it), where the
law gives 97, 67 and 56; `m5l1a`'s `light 65` stores 65, 32 and 13 at 88,
120 and 200 units against 63, 34 and 12 (Observed,
`.tmp/mohaa-gaps/pointfit.py`, the JSON's `point_law_isolated_lights`).
Luxels stop at 127 with their hue kept, so the fit stays outside the
saturated core. The spot's cone is taken to be q3map's too, fading over its
outer 32 units, as the compiler wrote q3map's cone width for it **[I]**.

## What changed

Under the `mohaa` profile only, `RT_AddEntityLights` now gives a light with
`angles` and no target the spot the compiler made of it, with the cone
above, and gives a light with `spawnflags 2` or `no_entity_light` the path
tracer's existing "surfaces only" class (`RT_LIGHT_NO_MODELS`, which keeps a
light off the surfaces lit as models). The parse reads `angles`,
`spot_angle` and `no_entity_light` for every map, but no other profile acts
on them. The load line names the model `2015` (Source).

`tests/rtx_light_entity_tests.py` runs the production parse and light setup:
an empty value and an empty key read past in two profiles, an entity cut
short by the end of the text, each rule of the table above, a half-angle of
90 or more falling back to the radius, a malformed direction, and Quake III
and Alice reading the same keys as before. Run against the parser before
this change, the check stops at its first case, "0 light entities" (Unit).

## Live

fnTech3's client on a private desktop, windowed at 800x600, the retail 1.00
installation read-only, an isolated home with an empty settings file, no
network and no input; console steps from a startup script, frames from the
engine's `screenshotPNG`. The renderer is RTX with its default path tracer
(`rtx_rt_mode 2`, the compute backend; this 32-bit client has no hardware
ray tracing), its default `rtx_rt_lighting 0`, which lights a level by its
own lights with nothing baked. "Before" is the same build without the
change, copied to `.tmp/runtime/mohaa-rtx-lights-before`; raster is
`rtx_rt_mode 0`, which draws the lightmaps.

- `m1l2a` from its start: the path tracer reported 27 point lights before
  and all 343 after. The wall sconce beside the poster, which glowed on a
  dark wall, now lights it, and the floor under the hanging lamp takes its
  light.
- `DM/mohdm6`, whose 293 lights were all read before, at the sun record's
  view under two `light 600` lamps pointed down with `angles 90 0 0`: the
  pillar and the walls above the lamps lose the light the lamps had thrown
  up and sideways. The back wall's raster level is the map's `ambientlight
  20 19 18` doubled by overbright alone, so the compiler gave it no lamp
  light; the path tracer has no ambient floor and now leaves it dim.

The lights cost no time: frozen at the start of `m1l2a` at 1280x720 with no
frame cap, the path tracer ran 35.3 frames a second with all 343 lights and
35.9 with 27 (Live subset); its light grid lists 614,551 local entries
where it listed 260,687, and the light sampler still weighs each cell's
few. Mean sRGB levels of fixed regions of the frames (Live subset):

| View, region | Raster (lightmaps) | Traced before | Traced after |
| --- | --- | --- | --- |
| `m1l2a`, poster wall | 124 | 21 | 140 |
| `m1l2a`, floor | 197 | 165 | 192 |
| `DM/mohdm6`, pillar | 94 | 135 | 81 |
| `DM/mohdm6`, back wall | 39 | 35 | 11 |
| `DM/mohdm6`, floor | 200 | 192 | 183 |

Elite Force's `stasis1`, at the view of `scripts/probe_stvef_sp_views.py`
(`fixedtime 50`, NPCs and effects frozen, HUD and gun hidden), the
retail paks from `.tmp/stvef-stock`, at 640x480: the path tracer read 219
point lights before and 275 after. The frame's mean level was 14.1 before
and 30.9 after, where raster draws 24.2; the
[Elite Force playability record](2026-10-24-stvef-playability.md) found its
traced views about a fifth brighter than raster, and this one, which had
traced at little more than half, now does the same (Live subset). Elite
Force's light model is unchanged: only the parse differs.

## FnQ features

The path tracer's default still stands on the level's own lights alone; no
lightmap, light grid or vertex light feeds it (invariant 11). The lights are
now the ones the level's designer placed, with the cones the compiler gave
them. Raster and lightmap frames appear here only as a comparison.

## Also checked

A sweep loaded all 54 maps in the Vulkan renderer and again in GL (each map
for 15 seconds on a private desktop with `cmd god`, `com_speeds` for ten).
Every map ran with no drop. The console's notices are 1.00's own: models its
scripts name and its paks lack or spell otherwise (`models/player/
german_winter1.tik` where the pak has `german_Winter_1.tik`; TIKI `cache`
lines naming `vehicles/...` without `models/`, which OpenMoHAA's client
fails alike **[O]**), sound aliases a level plays before it defines them,
`flickeralpha`, an event the welding torch's TIKI names and 1.00's client
game lacks, and "light grid mismatch" on `credits` and `void`, which ship no
light grid, where OpenMoHAA warns that none is present **[O]**. The startup
`Unknown command "fov"` is `autoexec.cfg`'s `fov 80`, run before any game
has registered the command, as OpenMoHAA's engine prints it too **[O]**; it
is an accepted difference, not an open one.

`m1l1` now builds its world BVH at the default `r_subdivisions 1`, 533,088
triangles in one tree, and traces its whole view; the
[RTX world record](2026-10-11-mohaa-rtx-world.md) had found one partition
too large to allocate there. Its 319,993 triangles that cast no shadow are
the ground and cliff patches of the `*_noshd` shaders, which 1.00's designers
marked `trans`, as q3map lets light through them.

The sweep's frame times, `com_speeds` over the ten seconds at 1280x720 with
no frame cap, the machine's other work included (Live subset):

| Renderer | 26 maps with far-plane fog, mean (range) | 28 without, mean (range) |
| --- | --- | --- |
| Vulkan | 2.1 ms (0.2 to 6.9) | 1.3 ms (0.2 to 4.0) |
| GL | 5.0 ms (2.6 to 8.1) | 4.0 ms (1.9 to 7.3) |

`m2l1`, the snowfield, is the slowest in both (6.9 and 8.1 ms). Frozen
there in Vulkan (`timescale 0`), its frame takes 4.6 ms, 3.7 of them in the
back end, spread over the dynamic lights' shadow atlas, the main draw and
the present with no one hot spot in 23 main-thread stack samples. GL takes
2.8 times Vulkan's time on the fogged maps and 4.1 times on the others: its
scene fog keeps the world off its buffer, as for RTCW's, Enemy Territory's
and Ritual's fog, since the buffer's ARB programs carry no fog, but the
clear maps' larger gap has another cause, not traced here. Single frames of
100 to 270 ms appeared on four maps in Vulkan; on three a repeat did not
show them, and on `m1l3a`'s scripted fight they recur, all client time,
while the level registers models and sounds it first uses, as 2015's engine
loads them on demand.

## GL's frame time, 2026-10-29

`DM/mohdm1` shows the widest gap of the sweep: GL 2.8 ms a frame against
Vulkan's 0.3. Frozen there in GL (`timescale 0`), skipping the back end
(`r_skipBackEnd 1`) leaves 0.26 ms, and of 56 main-thread stack samples 20
wait in `SDL_GL_SwapWindow`, the driver presenting the frame. GLx's own
timer queries put the back end's GPU work at 1.9 to 2.3 ms a frame, the
same with the map's static lights off (`r_staticLights 0`, 2.24 ms) or its
dynamic lights off (2.60 ms), where Vulkan does the whole frame in 0.3 ms:
GL is GPU-bound on Medal of Honor's world and models, by the structure of
its draws, which every profile on GL shares (Live subset). That cost needs
a GPU-level profile, not taken here.

Three of the 56 samples found something else: every 15 frames GLx's post
process asks the platform for the display output (HDR, white level, colour
profile), and `GLimp_QueryDisplayOutput` read the window's ICC profile from
disk with `SDL_GetWindowICCProfile` each time, opening the file anew, about
twenty times a second at this frame rate. It now keeps the profile for its
window and display and reads it again only for another window or display,
or a second after the last read (`code/sdl/sdl_glimp.cpp`; Vulkan and RTX
ask once, at start, and are unchanged). With it, no sample of the frozen
frame is in the read (0 of 36; Live subset, `tests/sdl_icc_profile_cache_tests.py`).
The machine's other work held the CPU at 100% throughout, so frame rates
did not separate between runs, and none is claimed.

The spot cone's edge was looked for in the lightmaps too. Only five
downward spots (`angles 90 0 0`, no target, no `spot_angle`) stand far
enough from other lights, and their floors are lit three to five times over
by their neighbours, inside and outside the cone alike, so the edge cannot
be read from them; q3map's fade stays inferred **[I]** (`.tmp/mohaa-gaps/spotfade.py`,
the JSON's `gl_follow_up_2026_10_29`).

## Accepted differences

Under invariant 12 a difference that no module, peer, file or player
depends on is recorded once, with its reason, and not carried as an open
gap. These three are such differences.

- **The spot cone's last 32 units.** The path tracer fades a spot over q3map's
  outer 32 units, as the compiler wrote q3map's cone width for it; the
  lightmaps cannot show 2015's own edge (above) **[I]**. It decides only how
  a traced cone's rim compares with a baked one, which nothing reads.
- **`overbright_range`.** The tracer does not read it. It changes nothing the
  compiler's own light records hold (every sphere light's intensity is the
  entity's `light`), so whatever it does is to the baked lightmaps, which the
  tracer's default does not use.
  Isolated lamps do not show it either: the five with 0.6 or more are
  `DM/mohdm5`'s identical `light 55` lamps (0.74), whose luxels hold 2 to 3
  times the falloff law, more the farther out, like light from elsewhere,
  while lamps without it range from 0.6 to 3.9 times
  (`.tmp/mohaa-gaps/obrfit.py`) **[I]**.
- **GL's frame time.** GL does several times Vulkan's GPU work on Medal of
  Honor's world and models, by how GL draws in every profile, and scene fog
  keeps its world off its buffer on the fogged maps. It still plays every
  map well: 8.1 ms a frame on the slowest, `m2l1`, and 2 to 5 ms on most, at
  1280x720 on this machine, under other sessions' load. Vulkan is the faster
  path, and the players' guide says so. A single frame's GPU time swings by
  two fifths from run to run on this laptop GPU, so toggles cannot place a
  hot spot in it.

## Not done

- Medal of Honor's TIKI models are still drawn by raster under RTX, so its
  characters and vehicles cast no traced shadow; that work is under way in
  its own task.
