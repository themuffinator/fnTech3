# The IHV Test's stageless scripts and portal views, 2026-10-08

Two rendering gaps on `km_portal`, the map of the build's own `sli.dm3`, found
by capturing the original client and fnTech3's at the same spawn. Both are
now closed:

- the map's walls drew fnTech3's default grid where the original draws black;
- its portal drew white where the original shows the view through it.

The [JSON companion](2026-10-08-q3-ihv-stageless-and-portals.json) holds
identities, captures and the disassembly offsets.

## Stageless scripts

**Observed in the original.** `textures/sfx/blackness` (`scripts/sfx.shader`)
has surface parameters and no stage. The original's `ParseShader`
(`0x43a3c0`) returns no shader for a script with no stage that is neither sky
nor fog (`0x43ad34`..`0x43ad54`). Its `R_FindShader` (`0x439fd0`, through
`0x43a350`) then treats the name as unscripted:

- it loads `<name>.tga`, `.bmp`, `.wal` or `.pcx` (`0x43a1a9`..`0x43a297`);
- it builds the implicit shader on what the script had set
  (`0x43a2d8`..`0x43a337`);
- only a missing image gives the default shader (`0x43a299`).

The build ships `textures/sfx/blackness.tga`, a black 32x32 image, so the
original draws those walls black.

**What fnTech3 drew.** fnTech3 follows Q3 1.32: a stageless script that is
neither sky nor fog fails to parse, and the map's surface gets the default
shader. The shader list marked `textures/sfx/blackness` as defaulted, and the
room's seven large walls drew fnTech3's grid.

**The fix.** For this profile, such a script now goes on to find the image of
its name, as the original does (`code/renderercommon/tr_q3_ihv_stageless.h`,
in all three renderers' shader parsers). The implicit shader is fnTech3's own,
the image under the surface's lightmap. Every other profile keeps Q3 1.32's
default shader.

In this build only `textures/sfx/blackness` has such an image. Its other 32
stageless scripts, such as `textures/common/caulk` and the light scripts that
only name a `surfacelight`, have none, so they keep the default shader in both
engines. `km_portal` and `ihv_test1` use `blackness`.

## Portal views

**Observed in the original.** The `portal` keyword only marks the shader
(`0x43a78a`); the dialect has no `alphaGen portal` and no range. The mirror and
portal pass (`0x42c160`) has no off-screen or distance test. It stops only:

- inside another portal ("recursive mirror/portal found");
- while `r_noportals` is set;
- when no portal entity (flag `0x800`) lies within 64 units of the surface's
  plane (`0x42bde0`).

The camera of a portal that is not a mirror always rolls about its forward
axis (`0x42c0b6`..`0x42c12c`). A nonzero frame (record byte 52) turns it by
`time * 0.001 * frame` degrees; otherwise it bobs by `sin(time * 0.003) * 4`
plus `skinNum` (byte 76). `km_portal`'s portal entity has frame 25, so its view
turns 25 degrees a second. Captures of the original a few seconds apart show
the view through the portal turned.

**What fnTech3 drew.** fnTech3's portal planner is Q3 1.32's. It skips a portal
farther away than its shader's `alphaGen portal` range. The IHV script sets
none, so the range was 0 and the view was never drawn: the portal's own stages
drew alone, white and red. Its camera rolled only when the entity's `oldframe`
was set, which the IHV game never does.

**The fix.** For this profile, portals have no range, and the camera rolls as
the original's does (`code/renderercommon/tr_q3_ihv_portal.h`). Both rules are
used by the portal planner that GLx and Vulkan share, and by RTX's own portal
code. fnTech3 still skips a portal that is off-screen or seen from behind, as
Q3 1.32 does. Its view cannot be seen then, so nothing visible changes. The
view itself is fnTech3's: its scissor, dynamic lights and renderer features
apply as in any other profile's portal.

## Results

All runs were windowed, on private desktops, with homes under `.tmp`, driven
by console commands alone, and captured with the engine's own `screenshot`
commands. The installation was only read.

| Run | Before | After |
| --- | --- | --- |
| fnTech3 client, `km_portal`, fnTech3 server, GLx | grid walls, white portal | black walls; the portal shows the turning view, red clouds and the far structure |
| the same, Vulkan and RTX (path traced) | not captured | as GLx |
| the same with `r_noportals 1` | not captured | the portal's own stages, white and red, as before the fix |
| original client, `km_portal`, fnTech3 server | black walls, turning view through the portal | unchanged (reference) |
| fnTech3 and original clients, `ihv_test1` | -- | the doorway draws black in both |
| `sli.dm3` in fnTech3 | grid walls, white portal | black walls and the portal view |

Some differences were not changed. fnTech3's lighting is brighter than these
captures of the original. That is expected: the original's probe ran with
hardware gamma off, and fnTech3 keeps its own lighting presentation. During
`sli.dm3`'s opening, the client game's particle burst still draws as large
white discs, with no capture of the original at that moment to compare.

## Tests

- `tests/q3_ihv_stageless_portal_tests.py` checks:
  - the two rules for this profile and for eight others;
  - the camera's roll, bit for bit, against the original's arithmetic for nine
    cases;
  - where each renderer's shader parser and portal code call the rules.
- `tests/model_shader_tests.py`, which compiles the shader lookup, gains the
  new flag in its fixture.

## Open

The particle comparison, the build's menus and input-driven play remain open.
The profile stays a Windows x86 developer path, and release builds still
refuse it.
