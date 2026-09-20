# Elite Force single-player scissor

The SP cgame's `CG_R_SCISSOR` import now clips subsequent 2D rendering in
GLx, Vulkan and RTX raster. The retail menu table's unused `R_ScissorPic`
slot stays empty, as retail initializes it. The development gate remains.

## Contract and implementation

Retail `stvoy.exe` 1.20 forwards four floats at `0x4030b7`. Its backend at
`0x45175e` restores the whole framebuffer when x is negative; otherwise it
truncates x, framebuffer height minus y minus h, w and h to integers before
calling `glScissor`. Coordinates are already framebuffer pixels. These are
observed interface facts; no external implementation text is incorporated.

The shared helper in `tr_stvef_scissor.h` performs that conversion and
intersects with the framebuffer before narrowing. Non-finite values and
negative dimensions are rejected without queuing a partial command. Empty
and entirely offscreen rectangles draw no pixels. The renderer command
flushes preceding pictures before changing the clipping state; Vulkan also
invalidates the cached depth range so the next draw updates its scissor.
Entering a new 2D projection resets the rectangle. Internal Vulkan clears
explicitly discard 2D clipping before clearing a whole framebuffer.

The renderer API gains one appended export and advances from 26 to 27.
Client and renderer modules must be rebuilt together. Other profiles issue
no new scissor commands and retain their existing drawing paths.

## Verification

`tests/stvef_scissor_tests.py` executes the shared command producer and
consumer for GL and Vulkan, checking fractional rounding, clipping at every
edge, reset, empty rectangles, extreme finite values, non-finite values,
negative dimensions, unregistered renderers, full command queues, and the
ordering of submitted vertices versus clipping changes. The focused suite
passed all 72 tests.

After removing the temporary drawing block, the full Windows x86 Meson
suite passed 120 targets with two platform-specific skips. Its recursive
Python target ran 1,826 tests successfully, including 72 conditional skips.

A fresh Windows x86 RelWithDebInfo build on base commit `9e1841d` compiled
the client, dedicated server, GLx, Vulkan and RTX renderers. A temporary
draw-only block at the end of `SCR_DrawScreenField` exercised the renderer
export in a windowed SP menu session with mouse input disabled and pointer
grabbing disabled. It queued, in order:

1. Reset, then a blue 640 by 480 picture.
2. Scissor `(100.75, 80.25, 200.75, 120.5)`, then a green full-screen picture.
3. Reset, then a red 20 by 20 picture at `(10, 10)`.
4. Empty scissor `(300, 300, 0, 0)`, then a yellow full-screen picture.
5. Reset.

The engine's `screenshot` command captured all three renderers. Each image
contains exactly 282,800 blue pixels, 24,000 green pixels, 400 red pixels
and no yellow pixels. The green rectangle starts at `(100, 81)` and ends
at `(299, 200)`, proving rounding after the lower-left conversion. Every
process exited normally. No operating-system capture or input injection
was used. The temporary drawing block was removed after the probe.

[The evidence manifest](evidence/2026-09-20-stvef-scissor.json) records
commands, executable and renderer hashes, pixel counts and image hashes.
Build logs and the synthetic engine captures remain under
`.tmp/stvef-gaps/` in the isolated `stvef-support` checkout.

This closes the ignored scissor import with unit and live renderer evidence.
It does not establish retail image parity for RDM5 characters or lighting,
or Bink movie playback. During the movie audit, Python's ZIP reader also
reported a bad CRC for the installation's `video/st_21.bik`; the installation
was not modified.
