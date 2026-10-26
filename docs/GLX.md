# GLx Renderer Guide

GLx is fnTech3's main OpenGL renderer, and the one every game uses unless you
choose another. Most of fnTech3's graphics features arrive in GLx first, and
it draws each game's own models, effects, fog and lights -- including those of
Return to Castle Wolfenstein, Enemy Territory, Elite Force, Alice and
F.A.K.K.2. The settings it shares with the other renderers are in the
[Graphics guide](DISPLAY.md).

## Selecting GLx

GLx is the default. If you switched to another renderer, come back with:

```cfg
seta cl_renderer "glx"
vid_restart
```

The other choices are `cl_renderer vk` (Vulkan) and `cl_renderer rtx`
(ray tracing). `renderer_switch` changes renderer in one step, which is handy
for comparing them:

```cfg
renderer_switch glx fast
renderer_switch vk fast
```

## Troubleshooting

**GLx does not start.** Check that `fnql_glx_x86` is beside `fntech3.exe`; it
comes in the download. GLx never quietly swaps itself for another renderer,
so a missing file is a problem with the files, not with your settings:
unpack the download again.

**The screen is black, or the picture looks wrong.** Switch to Vulkan with
`cl_renderer vk` and `vid_restart`. If Vulkan looks right, the problem is in
GLx: please report it, with the details below.

**It runs slower than expected.** Compare with Vulkan in the same place. Turn
`r_speeds 7` on to see how hard the renderer is working, and include it in a
report.

**Walls or floors look wrong in GLx but not in Vulkan.** GLx keeps the parts
of a map that never change on the graphics card (`r_vbo 1`, the default). Try
`r_vbo 0` and `vid_restart`, which builds the whole map again every frame
instead. If that fixes it, please report it, with the map and a screenshot
taken each way.

**GLx crashes or misbehaves with your graphics driver.** Use Vulkan
(`cl_renderer vk`, then `vid_restart`) until a fix arrives, and update your
driver.

**A problem only happens in one game.** Say which game and map, and attach a
screenshot taken with the `screenshot` command in both `glx` and `vk`.

## What to include in a report

Type these in the console where the problem shows, then `condump report.txt`
to save their messages into `report.txt`, in the game's folder in your home
folder, ready to attach:

```cfg
glxinfo
glxmaterial
glxpostprocess
glxstaticworld 8
seta r_speeds "7"
```

- `glxinfo` names what your graphics card supports and which GLx features are
  in use.
- `glxmaterial` reports how surfaces are being drawn.
- `glxpostprocess` reports the brightness, glow, HDR and screenshot settings
  in use.
- `glxstaticworld 8` reports how the map itself is being drawn.
- `r_speeds 7` shows, on screen, how much drawing each frame takes. Its
  numbers are not in the console, so take a `screenshot` of them.

[Getting Started](GETTING_STARTED.md#getting-help) says where to send it.

## For developers

GLx is built with the other renderers by default; the
[Build Guide](../BUILD.md) has the options. Its design, its diagnostics in
depth and the automated runtime sweeps that check it against Vulkan are in the
[GLx renderer notes](fnql/GLX_RENDERER.md), and the scenes those sweeps use
are listed in the [GLx proof corpus](fnql/GLX_PROOF_CORPUS.md).
