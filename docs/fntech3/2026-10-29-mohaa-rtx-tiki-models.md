# Medal of Honor's TIKI models in RTX, 2026-10-29

RTX's path tracer had left Medal of Honor's TIKI models -- its characters,
vehicles, weapons and props -- to raster drawing: they cast no traced
shadow and appeared in no traced reflection, and `docs/RTX.md` said so. The
trace now carries them. Each surface is skinned by the engine exactly as the
raster back end draws it, with the same pose, scale and level of detail, and
the shader the front end chooses for it. On `m1l1`'s truck ride the frozen
frame traces 26 TIKI entities, 30,479 triangles, and the convoy's truck and
soldiers now block the trace's light; on `DM/mohdm6` the player in third
person casts a shadow on the floor. Raster draws a model the eye stands
inside, as the player rides inside the truck, and the trace keeps it for
shadows and reflections. The [JSON companion](2026-10-29-mohaa-rtx-tiki-models.json)
holds the identities, commands and numbers. This is Source, Unit and Live
subset evidence; no compatibility gate closes.

## What the trace takes

`code/renderercommon/tr_mohaa_ray_geometry.h` is the RTX transport's TIKI
appender, beside the MDS, MDC and Enemy Territory ones, and
`vk_rt_extract_dynamic_model_entity_geometry` (`code/rendererrtx/vk.c`)
calls it for `MOD_MOHAA_TIKI`. For each entity it:

- takes the pose a view computed this frame (`ent->e.mohaa.bones`, from the
  front end's `R_MOHAAPose`). An entity no view drew -- culled from the main
  view, but still able to cast into it or show in a reflection -- is posed
  now into the frame's pose pool through a copy, as a dynamic light's shadow
  poses one, so the back end's entity is left as it was
  (`R_MOHAATracePose`, `code/rendererrtx/tr_mohaa.c`). An entity the engine
  cannot pose has nothing to trace, as raster has nothing to draw;
- picks each surface's shader with `R_MOHAASurfaceShader`, the choice
  `R_AddMOHAATikiSurfaces` makes (skin number plus the surface byte's skin
  bits, a skin past the last being the first, the crossfade bit, a custom
  shader). The RTX front end now calls the same function, so the two cannot
  drift. A surface the definition marks `nodraw`, one its byte hides, and an
  empty one are left out;
- skins each surface with the engine's `Skin` (`R_MOHAASkinTraceSurface`)
  at the entity's scale, at the detail the main view measured for it
  (`ent->e.mohaa.lod`, whole for an entity it did not measure) and with the
  view-weapon flag for a depth-hacked model, as `RB_SurfaceMOHAATiki` does.
  A surface the raster passes kept in the frame's skin cache
  (`tr_mohaa_skin_cache.h`) is copied from it; a surface skinned by a pose
  only the trace computed is not kept, which leaves the cache's room to the
  raster passes. A surface too small on screen to draw is left out, as raster
  leaves it out, and a triangle naming a vertex past the surface is not
  traced.

The geometry cache (`rt_geometry_cache.h`) keys a model by frame and
backlerp; a TIKI pose has no frame -- the engine blends up to sixteen
animations, bone controllers and the entity's own skeleton -- so TIKI
entities are not cached and are skinned afresh each frame, as MDS and Enemy
Territory models are.

## Who draws what

A traced entity's surfaces are drawn by the raster base pass for depth and
coloured by the trace, as every traced model format's are
(`vk_rt_surface_owner`, `code/rendererrtx/rt/rt_backend.c`). Two kinds of
entity stay with raster, every pass of them (`R_MOHAATraceable`):

- one that draws a second shader over a surface, a crossfade's second skin
  or a custom shader drawn over the skins (2015's render flag `1 << 16`).
  The trace carries one shader a surface and could layer neither. No retail
  file asks for a crossfade: none of the 1,378 TIKI files in 1.00's paks
  names one, and their 276 `surface` commands set and clear only `nodraw`
  (Observed, `.tmp/mohaa-rtx-tiki/scan_crossfade.py`). Whether 1.00's client
  game ever sets the custom-pass flag was not measured;
- a sky entity, which only the sky portal's view draws.

**The truck ride.** The player rides `m1l1`'s opening inside the truck's
model, `models/vehicles/gmctruck_opening.tik`, the eye within its bed and
canopy. Its hull's faces are single-sided: raster culls their backs and sees
out through them, but the trace counts an opaque face from both sides. The
first build traced the hull from inside, and the lower half of the view went
black: the region under the board's top averaged 4.8 where it had been 31.6,
the board itself 0.1 where it had been 27.2 (Live subset, the JSON's
`first_attempt_hull_from_inside`). So an entity whose model bounds hold the
trace's eye is now traced as the viewer's own body is -- for shadows and
reflections only -- and raster draws it in the view (`R_MOHAAViewInside`,
the definition's bounds at the entity's scale, along its own axes). The
board is 27.4 again and the region under its top 34.7, and the truck stays
in the trace's shadow and reflection rays. The player's
own body in first person keeps the body class it has in every profile, and
a view weapon the weapon class.

## Unit

`tests/mohaa_ray_geometry_tests.py` compiles the RTX renderer's own
`tr_mohaa.c` -- registration, front end, back end and the trace's helpers --
with the appender, against a stand-in engine whose pose and skinning answer
from every argument. It draws an entity through `R_AddMOHAATikiSurfaces` and
`RB_SurfaceMOHAATiki`, traces the same entity in the same frame, and checks
that every traced vertex, normal, texture coordinate, triangle and shader is
the raster passes' one, transformed by the entity: visible, at a measured
detail, as a view weapon, culled from the view (posed by the trace, the
entity untouched, nothing kept in the cache), unposable, and with a custom
shader. It checks the shader choice against the documented rule for every
surface byte, skin number and custom shader, `R_MOHAATraceable`,
`R_MOHAAViewInside`, bad indexes, failure propagation and the scratch
arrays. Nine deliberate mutations of the helpers and the appender (whole
detail, no view-weapon flag, keeping trace-only poses, no scale, no
crossfade, sky entities traced, the trace posing the back end's entity,
hidden surfaces traced, bad triangles traced) each fail it.
`tests/rtx_surface_owner_tests.py` adds the TIKI surface type to the posed
formats and checks that an entity layering a second shader or holding the
eye is raster's in the view.

## Live

fnTech3's client on a private desktop, windowed at 1280x720, the retail 1.00
installation read-only, an isolated home with an empty settings file and no
input. A listen server on 127.0.0.1 took console steps by rcon, froze the
level with `timescale 0` and took one engine `screenshot` per case after six
seconds of the frozen frame (`.tmp/mohaa-rtx-tiki/freeze_probe.py`, a copy
of `.tmp/mohaa-headlights/probe.py`). The renderer is RTX with its default
path tracer (`rtx_rt_mode 2`, the compute backend; this 32-bit client is
offered no hardware ray tracing) and default lighting, the level's own
lights with nothing baked. "Before" is `meson/build/win32-dev` at d792d7c8,
copied to `.tmp/runtime/mohaa-rtx-tiki-before`; "after" is the same
executable with the RTX renderer built with the change. Raster is
`rtx_rt_mode 0` after a `vid_restart` in the same run. After the runs the
installation re-hashed unchanged against its manifest, 2,663 files.

`m1l1`, frozen four seconds into the truck ride: a one-frame dump from a
diagnostic build listed 26 TIKI entities traced, 125 surfaces and 30,479
triangles. 11 of them no view drew and the trace posed: the four rangers
sitting behind the player, their weapons, the canteen and the swinging
lantern. Mean sRGB levels of fixed regions (Live subset):

| Region | Raster | Traced before | Traced after |
| --- | --- | --- | --- |
| The convoy's truck | 74.9 | 68.6 | 55.1 |
| Its three soldiers | 49.3 | 37.6 | 33.6 |
| The player's truck's board | 28.4 | 27.2 | 27.4 |

Before, the convoy's truck and soldiers were raster's, lit by the light grid;
now they are traced, lit by the moon behind them. The entity-shadow view
(`rtx_rt_debug_visualizer 13`, green for a sample a moving model blocked)
had no blocker anywhere before; on the ground in front of the convoy it now
reads 148.6 where it read 1.1. The raster frames before and after differ by
0.04/255 on average (3,277 pixels, the frozen frame's animated sprites).

`DM/mohdm6` at the sun record's view (`cmd tele 554 -455 -150`, `cmd face
25 90 0`, Allied rifleman, `popmenu 0` closing the weapon menu): the view
weapon is traced, in the weapon class, and lit by the level's lamps. With
`cg_3rd_person 1` the player's body is traced, and in the light-only view
(`rtx_rt_debug_visualizer 10`) the floor beside his legs falls from 222.6 to
131.8, his shadow, while the open floor stays at 248.0. The rifle in his
hands is hidden behind his back in these frames: one frozen frame captured
with the weapons traced and then handed to raster (a temporary switch in a
diagnostic build) shows it hidden in both.

The frozen `m1l1` frame, uncapped, at 1280x720 (two runs each, Live subset):

| | Before | After |
| --- | --- | --- |
| Frames over 8 s | 311, 326 | 240, 248 |
| Client time a frame (`com_speeds`) | 3.53, 1.88 ms | 10.6, 9.14 ms |
| Path tracer GPU, median | 28.44, 28.44 ms | 30.30, 30.50 ms |
| Path tracer CPU | 0.09, 0.07 ms | 9.39, 7.79 ms |

The appender itself takes 1.3 to 1.6 ms of that CPU time a frame, 0.1 ms
posing and 0.5 to 0.6 ms skinning (a diagnostic build's timers); the rest is
the compute backend's shared work for 30,000 more dynamic triangles each
frame: their light stamps, motion records, partitions, BVH refits and
rebuilds, and uploads. The frozen frame also accumulates fewer still frames
(164 and 232 where it had 854 and 856): the client game moves the
characters' poses in 38 to 63 of every 240 frames even under `timescale 0`,
and each change restarts the average.

## FnQ features

The path tracer is the FnQ feature here, applied to 2015's own data: its TIKI
surfaces, shaders, skins and levels of detail, posed and skinned by its own
skeletor (invariant 11). The trace still stands on the level's own lights
alone; no lightmap, light grid or vertex light feeds it. Raster frames appear
only as a comparison. A model the eye stands inside keeps raster's drawing
in the view, because raster's face culling is what lets the player see out
of the truck; its shadows and reflections are traced.

## Alice and F.A.K.K.2

Their TIKI models could share the appender's shape, not its code as it
stands. Ritual's models are tessellated by the renderer itself
(`RA_TikiTessellateLod`, `ent->e.alice.bones`, a detail per view slot in
`aliceLod`), not by an engine import, and their shader choice adds custom
skins and Ritual's stencil and projected shadow passes. One appender over a
per-format pose, shader and skinning callback would serve both lines, but it
needs its own ownership rules and a before-and-after on both games, so they
are unchanged here and still drawn by raster under RTX.

## Not done

- The compute backend's per-triangle cost of dynamic geometry (6 to 8 ms of
  CPU a frame for `m1l1`'s 30,000 TIKI triangles, outside the appender) is
  not profiled; it is shared by every profile's models.
- The hardware ray-tracing pipeline has not run the change, as this
  machine's driver offers only compute.
- The eye-inside rule reads the model's unposed bounds: a large model whose
  box holds the eye but whose geometry does not loses its traced shading in
  the view while the eye is in the box **[I]**.
- Whether 1.00's client game sets the custom-shader-pass flag on any entity
  is unmeasured; such an entity stays raster's.
- No retail frame compares: `MOHAA.exe` cannot run here, and 2015's engine
  drew no traced light.
