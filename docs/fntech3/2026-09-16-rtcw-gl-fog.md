# RTCW GLx fog and startup voice query

The second approved startup retest passed the initial autosave and the previous
scene-fog guard, then stopped on cgame import 37, `CG_S_GETVOICEAMPLITUDE`.
Its inspected engine PNG shows that explicit error. This does not establish
playable SP or original-renderer image parity. The test consumed one ready
click, exited normally, used an isolated windowed private desktop without
switching it, and left the installation unchanged. Its writable home was removed.
Evidence is under `.tmp/rtcw-manager/startup-retest-2/`.

The GLx renderer now consumes each view's owned fog snapshot for material
passes. Fixed-function OpenGL and generic GLSL receive the same clamped RGB,
linear/exponential mode, distances and density. Alpha remains unchanged before
the authored blend operation. Original zero-density defaults and SP snooper
distance adjustments are retained. Sky passes select sky fog; `nofog` materials
can opt individual stages back in with `fog on`. Fog controls sky suppression,
the far clip and a requested background clear. Ordinary projected lights retain
their original temporary fog disable/restore.

The original [SP fog implementation](https://github.com/id-Software/RTCW-SP/blob/master/src/renderer/tr_main.c),
[SP material parser](https://github.com/id-Software/RTCW-SP/blob/master/src/renderer/tr_shader.c)
and [MP parser](https://github.com/id-Software/RTCW-MP/blob/master/src/renderer/tr_shader.c)
were read as behavior evidence; their implementation text was not incorporated.
SP publishes map/water shader fog settings to its cgame through the original
cvars. MP installs map/water slots directly and starts the map transition.
Malformed scalar input is bounded before integer conversion. Water inheritance
value zero is a no-op, avoiding the original SP uninitialized-string path.

When the driver advertises
[NV fog distance](https://registry.khronos.org/OpenGL/extensions/NV/NV_fog_distance.txt),
both GLx paths use radial distance; otherwise they use absolute eye-plane
distance. The generic GLSL path interpolates vertex distances, as allowed by
that extension. A zero-span linear range has a defined step to avoid NaN.

Static ARB VBO programs do not yet carry scene fog, so affected views use the
existing CPU tessellation path. Optional ARB depth fading and refraction also
fall back while scene fog is active. Q3/QL and unfogged views retain their
existing paths. Vulkan and RTX keep the explicit active-fog guard until their
GPU paths implement the contract. Portal backgrounds, transitions between maps,
advanced effect combinations and original-renderer parity remain to be tested.

Twelve focused checks exercise pass selection, immutable snapshots, GL/GLSL
parameter transfer, original profile shader directives through the real engine
tokenizer, and existing projected-light/material behavior. They pass on Windows
x86 (14.493 seconds), Windows x64 (14.347 seconds), and Linux x86 with address and
undefined-behavior sanitizers plus leak detection (85.740 seconds). The final
combined startup-render checkpoint passes 111 Meson tests with two skips;
its Python suite passes 1,465 tests with eleven skips in 529.459 seconds.

The next startup correction admits the exact zero response for original MP
(whose TALKANIM code is disabled) and for SP when no sound backend is initialized.
The actual sound interface determines that state; changing `s_initsound` after
initialization cannot fabricate a silent result. SP non-client indices return
zero as in the original; negative client indices and overflowing machine-word
arguments are rejected. Active SP audio still fails explicitly until real
lookahead amplitude tracking is implemented. This is not a mouth-animation or
live-dialogue compatibility claim.

Later approved retests passed this silent-backend response and the new decal
and corona entrypoints, then stopped at the missing in-game cinematic camera
loader. The [latest startup evidence](2026-09-16-rtcw-coronas.md) records those
outcomes and their limits. None of these error-dialog captures establishes
playable SP or original-renderer fog image parity.
