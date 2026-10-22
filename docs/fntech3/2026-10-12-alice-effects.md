# Alice's targeting glow and smoke particles, 2026-10-12

Alice's targeting glow was missing because the scene adapter rejected
particles carrying their emitter's hidden flag. The adapter now accepts
those submissions. The investigation also corrected Alice's transparent
sprite ordering and the handling of zero-sized sprites in all three
renderers.

This is Source, Unit and Live subset evidence. The
[JSON companion](2026-10-12-alice-effects.json) identifies the source base,
binaries, installation, commands and local evidence. No compatibility gate
closes and no retail-engine visual comparison is claimed.

## Measured contracts and corrections

- **Hidden carriers, visible effects.** The retail targeting emitter
  `models/fx_emitter_target.tik` is hidden and emits `meta.spr` or
  `metared.spr`. Its particles inherit `RF_DONTDRAW` (`0x80`). Alice's
  `RE_AddRefSpriteToScene` (`0x00491700..0x004917d2`) copies the flag without
  rejecting the sprite. The client game filters hidden network sprites
  itself (`cgamex86.dll`, `0x3002208a`), but submits emitter particles
  through the same import without that filter (`0x30018677`). Removing
  fnTech3's second rejection restores the glow in the armed `gvillage`
  scene. The targeting decal is a separate effect; its mark-fragment
  requests were already returning geometry.
- **Model effects use the same contract.** Retail's entity submission
  (`0x00491650`) also copies its input without rejecting `RF_DONTDRAW`.
  The client game filters network models before submission
  (`0x300263da..0x300263e8`), whereas its temporary model effects are
  submitted at `0x30018684`. The entity adapter now accepts those effects,
  retaining the original flags in the Ritual record rather than confusing
  them with Quake III's flag positions.
- **Overlapping smoke.** Alice's sprite front end
  (`0x0047910e..0x00479183`) orders a shader's sprites by descending squared
  distance from the eye. The shared renderer had ordered them by entity
  number. A shared helper now sorts consecutive Alice sprite surfaces of
  the same shader from farthest to nearest after the ordinary radix sort.
  It preserves dispatch keys, surface pointers and pass flags. Different
  shaders and intervening non-sprite surfaces retain their existing order.
  Every other profile retains its previous sorting. Invalid coordinates
  receive a deterministic tie order, including in fast-math builds.
- **Zero scale.** Retail's sprite draw multiplies the supplied scale
  directly (`0x004af4d1`). The adapter and renderer had each replaced zero
  or negative scale with one, making a zero-sized particle full-sized.
  Both sprite submission paths now preserve scale, and the shared sprite
  geometry consumes it directly in GLx, Vulkan and RTX.

These are independently implemented corrections informed by the legitimate
retail binaries and assets. No external implementation text or proprietary
assets are incorporated. The shared Ritual adapters also serve F.A.K.K.2;
its existing hidden-emitter sprite behavior remains supported, its sprites
also preserve scale, and its sorting is unchanged. MOHAA continues to use
its own submission adapters and scale field.

## The old gnome's smoke flow

The user identified the old gnome's pipe and described its flow as wrong.
An original test script places `c_gnomeold.tik` in `gvillage`, disables its
AI and plays `pipetalk01`; the retail actor attaches its own pipe. A second
fixture places the pipe alone to separate particle motion from arm motion.

The installed `gnomepipe.tik` asks for 15 `firesmoke.spr` particles per
second, scale `0.05`, life `0.7`, forward speed `6`, upward acceleration
`10`, growth and fade. The shader uses the smoke image with vertex color
and alpha blending. Diagnostic observations show the pipe's tag axis
reaching the client game unchanged, particles rising from the stationary
pipe, and previously emitted particles continuing their motion as the
gnome moves his arm. The renderer's geometry fixture verifies that the
submitted particle positions reach the quad unchanged.

The confirmed smoke correction is transparency ordering. The probes did
not reproduce a separate reversed-direction or timing fault, and do not
establish that ordering explains every aspect of the reported flow.
Particle trajectories, emission rates and lifetimes remain owned by the
retail client game. Temporary diagnostic logging was removed.

## Presentation and validation

The effects use fnTech3's existing sprite, shader and transparent rendering
passes. Its model lighting, dynamic lights, shadows and map sidecars remain
available; no feature is disabled and no retail-exact mode is introduced.
The targeting glow is an authored particle effect and needs no bloom
setting. Audio was disabled in these probes. RTX was tested in raster mode
(`rtx_rt_mode 0`), not with ray tracing.

- Rebuilt the Windows x86 client and all three renderer modules in the
  stable `meson/build/win32-dev/` preset.
- The production adapter, sorter and sprite-geometry fixtures run on
  Windows x86 and Linux x86. They cover inherited hidden flags, retained
  position/color/time/pose, null and invalid submissions, capacity,
  zero/negative scales, ordering from different viewpoints, equal-distance
  ties, invalid coordinates in strict and fast-math builds, non-sprite
  surfaces and isolation of every other profile.
- All 178 Alice, F.A.K.K.2, Ritual and MOHAA effect Python regressions
  passed (plus 360 subtests), as did 25 native Alice/F.A.K.K.2 tests.
  The four effect tests also passed on Linux x86; 16 player-manual tests
  passed with 834 subtests. The entire multi-profile suite was not rerun.
- `final-glx`, `final-vk` and `final-rtx` each capture the gnome, its pipe
  and the restored targeting glow. `final-fakk2` checks the first-level
  scene through the shared adapters. All runs exit normally and leave
  their installations unchanged.

Evidence is retained under `.tmp/alice-effects/`: the `diagnostic-armed`
before-glow capture, `after-filter` first correction, final renderer runs,
`flow-static` and `flow-gnome` traces/captures, original scene overlays and
build/test logs. Runs use isolated writable homes on a private desktop,
windowed mode, disabled mouse/joystick input and console scripts. Every
image comes from the engine's `screenshotPNG` command; no input is injected.

Unrelated existing diagnostics: `gvillage` reports reused images with
mixed flags for `textures/testliq/top3.tga` and
`textures/effects/swirlie_02.tga`. Neither warning names the pipe's smoke
image, and neither prevented these probes.
