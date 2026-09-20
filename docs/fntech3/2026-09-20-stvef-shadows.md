# Elite Force shadow geometry

Elite Force's legacy-light stencil shadows now extrude vertices along the full
light vector in all three renderers. The inherited height cap shortened a
vertical shadow from 512 units to 51.2, which could stop it before reaching
the receiving surface. The change is limited to the two Elite Force profiles;
other profiles and modern model-lighting mode retain their existing policy.

## Native observations and regression checks

The installed SP 1.20 executable's stencil routine at `0x4629c0` and Holomatch's
at `0x479380` both use the unscaled vector. Their projection deformation
routines at `0x462e20` and `0x4797e0` use the same ground-plane calculation
already present in fnTech3.

An independent local probe invokes each routine with synthetic vertices,
six light directions and two orientations/ground planes. The mapped stencil
invocation returns after extrusion, before any graphics API calls; the
projection routine runs unchanged. Both executables produce identical
results for all 24 cases. No installation or running game is modified.

The numeric fixture records these observations and the previous fnTech3
policy for other profiles. `stvef_shadow_tests.py` executes each renderer's
production setup/extrusion code and complete projection function, with model
lighting compiled both in and out. It compares all seven profiles, preserves
modern lighting, and checks empty batches and unavailable stencil buffers.
The observed maximum error falls from 460.8 units to below 0.000008; the
fixture allows 0.00003 for floating-point rounding.

All 11 focused Windows tests pass. The new fixture also passes on Ubuntu with
Clang x86 and address/undefined-behavior sanitizers. All three Windows x86
renderers build successfully. Final suite and live image results are recorded
in the [evidence manifest](evidence/2026-09-20-stvef-shadows.json).
The complete Windows x86 suite passes 120 targets with two skips; the Python
target passes 1,843 tests with 72 conditional skips.

## Runtime scope

The local windowed probe uses the retail `tutorial` save and a static model,
with isolated homes under `E:\Games\fnTech3-tests`. Images come only from
engine `screenshot` commands. It compares the installed retail engine with
the before/after GLx, Vulkan and RTX builds. No mouse or keyboard input is
controlled and no operating-system capture is used.

The static-model probe changes `cg_shadows` between off and stencil mode.
Before the correction, 1,061 pixels disagree with retail about whether they
are shadowed. After it, only three pixels disagree. Retail shadows 2,723
pixels and fnTech3 shadows 2,720; the corrected images are identical across
GLx, Vulkan and RTX. The mean difference in the isolated shadow contribution
is below 0.0007 per 8-bit colour channel. Background rendering differences
remain separate from this shadow mask comparison.

A second probe spawns Tuvok, lets his transporter effect settle, freezes his
AI and holds his animation frame through the module's debug cvars. Retail and
all three corrected renderers complete this probe; fnTech3's images are again
identical across renderers. Together, the two probes produce 33 engine images
across 11 windowed runs. Projection mode does not change these particular
visible scenes, so its evidence here remains the executable geometry fixture.

The mathematical fixture establishes the projection and extrusion contracts.
Scene-level image parity and movie audio remain separate evidence requirements.
The profile's development gate remains in place.
