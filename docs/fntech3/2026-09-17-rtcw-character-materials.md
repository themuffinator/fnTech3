# RTCW character materials and conditional texture selection

This development slice follows commit `1c2b716` with uncommitted renderer
changes. The earlier camera checkpoint showed floating attachments and missing
character skins. Stock SP head shaders were rejected at `picmip2`; conditional
texture directives were also rejected. Accepting those exposed the missing
`alphaGen normalzfade` contract and MP's `nomipmap` alias.

## Implementation

All three renderers now recognize SP character mip selection and the shared
RTCW `map16`, `map32`, `mapcomp`, `mapnocomp`, `animmapcomp` and
`animmapnocomp` alternatives. Unselected alternatives consume their arguments
without swallowing stage braces. Selection follows framebuffer/compression
state, including the reference's asymmetric `mapnocomp` condition. Q3/QL do
not gain these RTCW-only keywords, and `nomipmap` remains MP-only.

SP registers an independent, latched `r_picmip2`, defaulting to zero for full
character texture detail. A private image flag carries that choice through
the image cache and uploader. `picmip2` overrides automatic model-path
filtering while preserving an explicit `nopicmip` or `nomipmaps`. Mip levels
are bounded before shifts. The code does not change `r_picmip`; the prior
screenshot audit already observed zero throughout its 38 runs.

RTCW normal-based alpha uses an independently written shared evaluator and
bounded optional-argument parser. It covers the triangular fire-shell alpha,
entity opacity, model-axis transformation and zombie sentinel. It preserves
RGB and does not mutate the submitted entity's rise direction. Invalid or
nonfinite arguments fail atomically, braces remain available to the enclosing
parser, and degenerate intervals return zero. Renderer stage merging compares
the new parameters, normals are requested, and GL's stream path uploads the
computed vertex alpha instead of passing an unsupported mode to its shaders.

## Source and execution evidence

`scripts/probe_rtcw_normal_fade.py` compiles unchanged original/iortcw SP/MP
switch bodies only in disposable fixtures, with their notices intact. The
matrix includes model/world draws, axes, rise directions, ordinary and sentinel
bounds, opacity, peak alpha and sampled normals. Each reference agrees exactly
on **819,000 alpha bytes** on Windows x86, Windows x64 and Linux x86. The Linux
probe uses address/undefined-behavior sanitizers with leak detection. Reports
retain the source and implementation hashes. Initial one-byte discrepancies
were corrected by preserving the intermediate direction/alpha rounding.

Four focused tests cover each renderer's actual parser branches, image-policy
and mip helpers, real tokenizer, alpha dispatch and model-shader cache paths.
They pass on Windows x64 and Linux x86 with sanitizers; Windows x86 coverage
is included in the complete configured suite. These are synthetic CPU
contracts, not GPU blending or whole-engine parity with RTCW.

The complete Windows x86 MSVC development build and configured Meson suite
pass: **111 passed, zero failed, two skipped**, including **1,471 Python tests
with eleven skips** in 440.413 seconds. The detailed log is
`.tmp/rtcw-manager/build-rtcw-dev/meson-logs/material-validation.txt`.
The first test invocation with `--no-rebuild` could not run because optional
zlib test executables had not been built; normal Meson dependency preparation
resolved that setup issue. Final source/binary identities and the runtime
audit are retained as `source-identities.json` and `runtime-audit.json` in
the evidence directory below.

Evidence lives under `.tmp/rtcw-manager/material-textures/`:

- `normal-fade-x86.json`, `normal-fade-x64-final.json` and
  `normal-fade-linux-x86-final.json` pin the source comparisons.
- `focused-x64.log` and `focused-linux-x86.log` record the focused suites.
- `runtime-v2/` contains inspected SP mission-briefing and MP deployment PNGs.
  Both installed registration probes report zero unknown shader directives.
- `ready-test/` retains the first explicitly approved one-click cinematic
  test. Its images show assembled heads/bodies and first-person startup.
- `ready-final/` records the final-renderer cinematic retest under the user's
  standing intro-button authorization. It retains the permission, one-action
  ledger, private binary identities and temporary instrumentation separately.
- `render-live/` contains six inspected Q3/QL engine images using the final
  ordinary development executable, through GL/GLSL, Vulkan and RTX raster.
  Q3 shows the world, weapon and HUD; QL shows its join menu over the world.
  Spawn positions and backend presentation differ; this is a regression smoke,
  not an image-parity or peer-interoperability comparison.

## Runtime scope and remaining gates

The SP test advances the introductory briefing with one engine-only Ready
click, takes registered `screenshotPNG` captures near 2, 35 and 90 seconds,
and exits near 95 seconds. Captures use 1600x1200, CSM disabled, `r_picmip 0`
and `r_picmip2 0`. All launches force windowed mode and isolated writable
homes; no desktop switching, OS capture, movement or combat input is used.
The production client source is unchanged by the transient capture hooks.

The final run exits normally with an unchanged installation and its temporary
home removed. Its first two captures have a valid snapshot and camera mode
one; the 90-second first-person capture has a valid snapshot and camera mode
zero. No unknown shader directives, missing head-skin warnings or engine
errors are recorded. All three final PNGs were inspected. The gallery under
`.tmp/screenies/` now selects their byte-identical copies; the earlier images
and immutable per-run evidence remain available. Its refreshed audit checks
27 selected images and 39 archived runs with `r_picmip 0`, plus `r_picmip2 0`
at each refreshed SP capture.

These observations close the specific missing-character material failures in
the sampled opening cinematic. They do not establish complete character or
material parity. Active dialogue/voice amplitude, campaign/AI, movement/combat,
save/load and transitions remain open. MP objectives, sustained sessions,
demos and bidirectional interoperability remain open. Vulkan/RTX's active RTCW
scene-fog path remains guarded; complete fire/zombie GPU blending and broader
installed shader combinations still need live comparisons. Default RTCW
release support remains gated.

## Provenance

The original [SP renderer](https://github.com/id-Software/RTCW-SP/tree/master/src/renderer),
[MP renderer](https://github.com/id-Software/RTCW-MP/tree/master/src/renderer)
and [iortcw](https://github.com/iortcw/iortcw) provide observed material and
alpha contracts. Their GPLv3-or-later implementation text is confined to the
disposable comparison fixtures. The production code is independently written
GPLv2-or-later; no reference implementation, retail shader files, assets or
game modules are incorporated.

Previously recorded unrelated issues remain open: Q3's menu-logo triangular
gaps, the generic Q3/QL routing-cache bounds audit, the source-inferred OpenAL
big-endian PCM16 mismatch, and the local Steam provider's release-manifest
mismatch. No new unrelated issue was established by this slice.
