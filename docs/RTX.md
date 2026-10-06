# RTX Renderer Guide

RTX is fnTech3's ray-tracing renderer. Instead of the lighting each map had
painted onto its walls in advance, it works light out the way light really
travels -- from each map's own lamps, glowing surfaces, sky and sun -- with
true shadows, light that bounces off walls, and reflections. It only changes
the picture: gameplay, online play and demos are exactly the same. Where your
graphics card cannot ray trace, RTX draws the game the normal way instead.

## What you need

- **A graphics card with Vulkan** and up-to-date drivers. RTX traces in one of
  two ways, which give the same picture:
  - with the card's **ray-tracing hardware**, on cards and drivers that offer
    it to fnTech3;
  - with **compute**, which works on any Vulkan card, but more slowly.

  fnTech3 picks the hardware way when it can and compute otherwise. fnTech3 is
  a 32-bit program, and some drivers only offer ray-tracing hardware to 64-bit
  programs -- the NVIDIA driver fnTech3 is developed on is one -- so even a
  ray-tracing card may trace with compute.
- **The RTX renderer file**, `fnql_rtx_x86`, beside `fntech3.exe`. It comes in
  the download.

Every game with a client can use RTX. It traces the model formats of Quake
III Arena and its relatives; models it cannot trace -- those of Alice,
F.A.K.K.2 and Medal of Honor, for example -- are drawn the normal way, without
traced shadows or reflections. Return to Castle Wolfenstein and Enemy
Territory have been drawn with it in testing, but, like those games
themselves, that is still experimental.

## Turning RTX On

Open the console and type:

```cfg
seta cl_renderer "rtx"
vid_restart
```

To go back, use `cl_renderer glx` (the default) or `cl_renderer vk`, then
`vid_restart`. `rtx_caps_report 1` shows what your graphics card can do, and
the console says which way RTX is tracing when it starts.

### Ray tracing or normal drawing

- `rtx_rt_mode 2` ray traces the view. This is the default.
- `rtx_rt_mode 0` keeps the RTX renderer but draws normally, without ray
  tracing.
- `rtx_rt_mode 1` is a developer test mode, not for playing.

A settings file saved before ray tracing became the default may still say
`seta rtx_rt_mode "0"`; set `rtx_rt_mode 2` and `vid_restart` to trace.

If ray tracing cannot start, or a frame fails, RTX tells you why in the
console and carries on drawing normally. Keep `rtx_rt_require 0` for play:
`1` makes such a failure stop the renderer instead, which is only for
testing.

`rtx_rt_backend` chooses the way to trace: `0` (the default) the hardware
where available and compute otherwise, `1` hardware only, `2` compute only.
Use `vid_restart` after changing it.

RTX uses fnTech3's HDR picture (`r_hdr 1`) by default; `r_hdr 0` gives the
games' original colour handling, for comparing pictures with the original.

## What You Will See

- **Each map lit by its own lights.** Nothing painted in advance is used at
  all. Rooms are lit by the lamps their designer placed and by surfaces that
  glow -- lamps, windows, flames, screens -- and outdoors by the sky and sun.
  Quake Live's and Enemy Territory's maps keep no placed lamps, so their rooms
  are lit only by their glowing fixtures, the sky and the sun. A map is
  exactly as bright or as dark as its own lights make it, so some places look
  different from what you remember.
- **Real shadows** from every light, soft where a light is large, and through
  grates, fences and cages. Rockets, muzzle flashes and other moving lights
  cast shadows too.
- **Bounced light.** Light reflects off walls and floors, so a sunlit floor
  lights the room around it. One bounce is the default.
- **Reflections** on shiny surfaces, which show the lights around them.
- **A clean picture.** Ray tracing starts out grainy, and fnTech3 smooths it
  as you play. When everything stands still -- a paused demo, say -- it keeps
  refining towards a perfectly clean image.

Some things are always drawn the normal way, on top of the traced picture:
see-through and glowing effects, smoke and sparks, sprites, water, the sky,
the HUD and menus. Mirrors, portals, small views and the distant sky that
Return to Castle Wolfenstein and Enemy Territory draw are drawn normally too.
Fog is drawn over the traced picture as usual. Your own body is not shown,
but it casts a shadow and appears in reflections.

**Seeing it through a camera.** `rtx_rt_camera 1` shows the light as a camera
would see it, with brightness that adapts as your eyes do when you move
between dark rooms and sunlit places, much as the Quake II RTX remaster does.
`rtx_rt_lighting 3` lights a map only from its sky and glowing surfaces, as
Quake II RTX lights Quake II, and always uses the camera.

The technical detail of what RTX traces and how it lights each map is in the
[RTX Renderer Contract](fnql/RTX_RENDERER.md#path-tracer-scope-and-light-model).

## Texture Packs With Material Maps

No original game includes them, but a texture pack can add *material maps*
beside a texture to give RTX bumps, shine and metal. RTX uses the same names
as the OpenJK engine's rend2 renderer:

| Image | What it holds |
| --- | --- |
| `<texture>_nh`, or else `<texture>_n` | A normal map, for bumps: red leans the surface along the texture's width, green along its height. The `_nh` alpha is unused. |
| `<texture>_specGloss` | Shine colour, with glossiness in the alpha. |
| `<texture>_rmo`, or else `<texture>_orm` | Roughness, metalness and occlusion, in that order or as occlusion, roughness, metalness. |

`<texture>` is the surface's main texture, without its extension; its
scrolling and other animation apply to the maps too. A metal surface
reflects in its texture's colour. The occlusion channel is unused, because
RTX finds that itself. Only RTX uses these maps; `rtx_rt_material_maps 0` and
`vid_restart` ignore them.

## Settings

| Setting | Default | What it does |
| --- | --- | --- |
| `rtx_rt_mode` | `2` | `2` ray traces the view; `0` draws normally. Needs `vid_restart`. |
| `rtx_rt_require` | `0` | `1` stops the renderer instead of drawing normally when tracing fails; for testing. Needs `vid_restart`. |
| `rtx_rt_backend` | `0` | `0` hardware where available, else compute; `1` hardware only; `2` compute only. Needs `vid_restart`. |
| `rtx_rt_material_maps` | `1` | Use a texture pack's material maps. Needs `vid_restart`. |
| `rtx_rt_lighting` | `0` | `0` lights each map with its own lamps, glowing surfaces, sky and sun, with nothing painted in advance; `3` with the sky and glowing surfaces only, as Quake II RTX does. `1` and `2` are for comparing with the original lighting: `1` uses the painted lighting and traces only moving lights over it, `2` traces the lights as the map's tools painted them. |
| `rtx_rt_camera` | `0` | `1` sees the light through a camera, with automatic brightness; `0` shows it exactly as traced. `rtx_rt_lighting 3` always uses the camera. |
| `rtx_rt_emissive_light` | `2.0` | How much light glowing surfaces give off, as a multiple of how bright they look; `0` none. |
| `rtx_rt_exposure` | `0` | With the camera on: its brightness. `0` adjusts automatically; any other value is fixed, with a film-like curve. |
| `rtx_rt_exposure_bias` | `0` | With the camera on: makes the automatic brightness brighter (positive) or darker (negative), in stops -- each stop doubles or halves it. |
| `rtx_rt_exposure_speed` | `1` | With the camera on: how quickly the automatic brightness adapts; `0` instantly. |
| `rtx_rt_tonemap_range` | `7` | With the camera on: how wide a range of brightness it fits onto the screen, in stops. |
| `rtx_rt_tonemap_curve` | `0.5` | With the camera on: how much of its adaptive curve to use; the rest is a simpler curve. |
| `rtx_rt_checkerboard` | `0` | Traces bounced light for half the screen each frame, in a checkerboard, for speed: `0` only in compute mode, `1` always, `2` never. |
| `rtx_rt_bounces` | `1` | How many times light bounces, `0` to `4`. More is brighter in enclosed rooms and slower. |
| `rtx_rt_indirect_strength` | `1.0` | How strong bounced light is; `0` keeps only direct light. |
| `rtx_rt_specular` | `1` | Shine and reflections. |
| `rtx_rt_mis` | `1` | Lets mirrors and shiny surfaces show the shape of lamps; `0` turns that off. |
| `rtx_rt_sun_intensity` | `1.0` | How strong the sun is. |
| `rtx_rt_patch_shadows` | `1` | Curved surfaces cast shadows, as id's maps were built for; `0` for maps built without that. |
| `rtx_rt_sky_intensity` | `1.0` | How much light the sky gives, and how bright it looks in reflections. |
| `rtx_rt_emissive_scale` | `1.0` | How bright glowing surfaces look. |
| `rtx_rt_dlight_scale` | `1.0` | How strong moving lights are; `1` matches normal drawing. |
| `rtx_rt_denoise` | `1` | Smooths the grain out of the picture. |
| `rtx_rt_denoise_iterations` | `4` | How strongly, `0` to `6`. |
| `rtx_rt_accumulate` | `1` | Keeps refining the picture while nothing moves. |
| `rtx_rt_iqm_skinning` | `1` | How IQM models are posed: `0` not at all, `1` smoothly animated, `2` without smoothing. |

## Troubleshooting

**RTX will not start.**

1. Type `vkinfo` and `rtx_caps_report 2` in the console and note the first
   thing reported missing.
2. Update your graphics driver, and check `fnql_rtx_x86` is beside
   `fntech3.exe`.
3. Go back to another renderer with `cl_renderer vk` or `cl_renderer glx`,
   then `vid_restart`.
4. Keep `rtx_rt_require 0`, so a problem falls back to normal drawing.

**Part of the picture is drawn normally.** The console says why, once per
session, in a line starting `RTX RT:`. Mirrors, portals and small views are
always drawn normally.

**It is too slow.** Compute mode is slower than ray-tracing hardware. Render
at a lower resolution with `r_renderScale` (see the
[Graphics guide](DISPLAY.md#sharper-edges-and-render-resolution)), keep
`rtx_rt_bounces 1` or try `0`, or use `rtx_rt_checkerboard 1`.

**A large map fails to load.** fnTech3 is a 32-bit program with 2 GB to work
in, and ray tracing adds about 200 MB on a large map. fnTech3 keeps room for
the game's own code while the renderer restarts, so even Enemy Territory's
largest maps load back; if a map still will not load with RTX, play it with
GLx or Vulkan.

Developer tools -- views of the tracer's inner workings, timings, and
pointing at a surface or a light to see how it is traced -- are described in
the [RTX Renderer Contract](fnql/RTX_RENDERER.md#developer-diagnostics).
Keep them off for normal play.

## Related Documentation

- [Graphics guide](DISPLAY.md) for the settings every renderer shares.
- [RTX Renderer Contract](fnql/RTX_RENDERER.md) for how RTX works, its known
  limits, building it and testing it.
- [Build Guide](../BUILD.md) for building the renderers.
