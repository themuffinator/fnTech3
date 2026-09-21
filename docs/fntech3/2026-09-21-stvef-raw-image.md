# Elite Force SP raw-image brightness and ordering

Elite Force SP now draws the images its UI hands the engine at the brightness
the UI asked for, and every image submitted in one client frame keeps its own
pixels until the frame is drawn. The implementation is commit `b66b036`,
following baseline `52668db`.

## Failure and correction

Retail's `DrawStretchRaw` import carries a brightness value alongside the
pixels. The client adapter accepted that parameter, discarded it, and passed
the image to the cinematic scratch path, which has no brightness of its own.
Every SP preview was therefore drawn at full intensity regardless of what the
menu asked for.

Native `RE_StretchRaw` (`stvoy.exe` 0x44fb50) hands the value straight to
`glColor3f` (0x44fc69..0x44fc70) and then uploads and draws the image
immediately: flush pending commands, synchronize, bind, upload, set the four
sampler parameters when the size changed, set the color, and emit one quad.
Because the draw happens inside the call, one scratch image was always
enough and a menu was never refused.

This engine queues 2D geometry instead. A shared scratch image is uploaded
again before the earlier quads referencing it are drawn, so every queued
preview in a frame would show the last upload's pixels. The correction is a
dedicated renderer export rather than a reuse of the cinematic path.

`DrawSTVEFRaw` takes the brightness and carries it as exact vertex colors on
the engine's canonical quad. The native boundary clamps at `glColor3f`, so
out-of-range values clamp the same way here and round to the nearest byte the
quad can represent; the recorded cases include -1 and 1.5. Each submission in
a client frame takes its own pooled image and shader, and the pool is reused
the next frame rather than regrown. Past the pool the last slot is reused: an
earlier quad sharing it can show the wrong preview, which is a visible fault
rather than dropping the client out of the menu it is drawing.

The export is added to all three renderers and moves `REF_API_VERSION` from
27 to 28. The shader it registers ignores source alpha and the UI's current
2D color, and takes its color from the vertices; `rgbGen` is a stage field in
the OpenGL-lineage renderer and a bundle field in the Vulkan and RTX ones, so
each renderer sets the field its own struct owns. The cgame's save-screenshot
trap uses the same export at full brightness. Images that cannot be uploaded,
non-finite brightness and non-single-player profiles are refused before any
upload. This is independently implemented engine code; no external code is
incorporated.

## Executable validation

`tests/stvef_sp_raw_image_tests.py` compiles the actual shared header against
owned renderer stubs, in both its OpenGL and its Vulkan upload configuration,
and replays the 21 recorded native cases. Each case's four vertex positions
and texture coordinates must match the recording exactly, including the seven
cases with negative extents and the half-texel inset at both recorded sizes.
The brightness must reach the quad as the expected byte on all four corners
with opaque alpha, and the upload decisions must follow retail's: a full
upload on each of the three recorded size changes with its four sampler
parameters, a sub-image upload otherwise.

The same probe then covers what the recording cannot: three previews drawn in
one frame must land on three different images that still hold their own
pixels, the pool must be reused rather than regrown on the next frame, a frame
that draws past the pool must keep running, and the input guards must refuse
rather than draw. The Linux runs build it with AddressSanitizer and
UndefinedBehaviorSanitizer.

The ordering check was confirmed to fail on the defect it guards. Restoring a
single shared image for every submission in a frame — `slot = tr.stvefRawUsed++`
becoming `slot = 0` — fails three tests in both probe configurations, first at
`image->pixels[0] == fills[draw]`. The header was restored byte-identical
afterwards.

A source check pins the export, the pool fields and the shader setup across
all three renderers, and derives the expected `rgbGen` expression from each
renderer's own struct rather than asserting one spelling everywhere.

Three raw-image tests and 56 focused tests pass on Windows x86. The full
Windows suite passes 120 targets with 2 skips; its Python aggregate runs
1,868 tests with 72 skips in 853.078 seconds. The first aggregate run failed
one assertion: `underwater_view_source_tests` still pinned the previous
renderer API version. That pin belongs to this slice and is corrected in the
same commit.

## Production runtime observations

All runs use canonical binaries and user-supplied legitimate modules and
assets. The two recorded sessions run an engine built from the committed
sources plus one bounded diagnostic print inside the new export, capped at 64
draws; the diagnostic was reverted and the tree rebuilt before this record, and
the committed sources carry no such print. The manual-stage preflights below
run the refreshed installation's own artifacts, which carry no diagnostic.

The single-player session loads an existing save, writes another, and loads
that result. It records 64 image draws across 62 client frames, every one a
256 by 256 image at full brightness taking the first pool slot — the cgame's
save-screenshot trap, one preview per frame, with the per-frame reset
returning to the first slot each time. No draw was refused and the session
exits normally. The Holomatch session records none, which is the profile gate
holding: the export belongs to single player.

| Runtime group | Sessions | Engine captures |
| --- | ---: | ---: |
| SP save load, save and reload | 1 | 1 |
| Holomatch map, bot and restart | 1 | 1 |
| Refreshed manual-stage preflight | 2 | 2 |

The manual installation at `E:/Games/fnTech3-tests/stvef-manual` carries six
refreshed engine artifacts from `b66b036`. Its seven settings, save and
launcher files retain their hashes; only the README's engine-provenance
paragraph and its menu save/load check change. Both refreshed profiles pass
preflight, each through a `renderer_switch` that re-initializes the renderer
at the new API version, with no image draw refused.

All four games run windowed on a private inactive desktop, with mouse, joystick,
audio and feedback disabled. Images come from the engine's `screenshot`
command. No keyboard or mouse input is controlled, injected or captured.
Scripts, logs and the trace binaries remain in the worktree's
`.tmp/stvef-gaps`; the runtime homes remain under `E:/Games/fnTech3-tests`.

The [evidence manifest](evidence/2026-09-21-stvef-raw-image.json) identifies
the baseline source objects, the recorded native provenance, the mutation
check and the runtime sessions.

## Remaining gates

These checks establish the recorded native draw behavior and the engine-side
geometry, brightness and image identity. They do not establish how a preview
looks inside a menu. The save and load menus are opened by key input, which
this harness does not drive, so their on-screen previews remain a manual gate.
Runtime coverage is the OpenGL renderer's save-screenshot path; the Vulkan and
RTX upload paths are covered by the compiled probe only. The refreshed manual
installation now asks for that menu check by name.

Campaign traversal, physical movement, look and fire, menu rebinding, save and
load, subjective listening, feedback hardware and universal renderer parity
remain open, as before. Both Elite Force profiles remain development-gated,
and retail SP remains a Windows x86 runtime boundary.

Existing unrelated issues remain: the installed `video/st_21.bik` has a bad
ZIP CRC, missing asset paths and excess `messhall` subwaves produce warnings,
the Windows x86 driver lacks native RT support, existing compiler warnings
remain, and the wider Linux suite has previously recorded failures outside
this slice. None is represented as fixed by this change.
