# RTCW material compression directives — 2026-09-15

The first MP deployment-menu captures exposed white borders and clipped chat
labels. The renderer rejected `nocompress` as an unknown shader directive and
fell back before reading the material's alpha-blending stages. Supporting that
directive restores the authored menu materials; it is not a UI-layout change.

All three renderers now recognize RTCW's `nocompress` and `allowcompress`
directives. A shared policy supplies image flags for ordinary maps, clamp maps,
animated maps, sky images and implicit material images. `nocompress` opts out;
compression mode 2 requires explicit opt-in. RTCW's case-sensitive lightmap,
sky and weapon-name exemptions are retained. Existing image-loading and GPU
upload paths consume `IMGFLAG_NO_COMPRESSION`. Other game profiles retain their
existing parsing and image flags.

The setting belongs to each material and resets with `InitShader`. This
deliberately avoids the original engine's global `allowCompress` value leaking
between materials in compression mode 1. Opt-in permits compression; it does
not require a renderer/device to compress. The inherited image cache retains
its first-loaded-image semantics. Compression formats, cache-order effects and
original-engine pixel parity remain separate open requirements.

`nofog`, stage fog and active scene/sky GPU fog remain unfinished. They have
not been silently accepted by this change. The default RTCW startup gate stays
closed; SP's installed map still has the independent AAS map-loading blocker.

## Evidence

The directive/image-flag tests and related shader loader/variant regressions
pass four tests on Windows x64 (7.678 s), Windows x86 (9.286 s) and Linux x86
ASan/UBSan (43.655 s). They exercise each renderer's actual image wrapper,
directive handling, mixed case, unknown tokens, compression modes, image
exemptions, flag retention, per-material reset and Q3/QL isolation. Reports
are `.tmp/rtcw-manager/material-compression-{x64,x86,sanitizers}.log`.

The development client remains SHA-256
`8fb6a56f42529bece0411fa1952765c43a24a1921b3b8f0bb36b45416818b78b`:
this slice changes renderer modules. Its Windows x86 renderer identities are:

| Renderer | SHA-256 |
| --- | --- |
| GLX | `3a687030c9d09ce29acd107d87bfa0e91b0cd8ca8b49a988554d8c0f936bea45` |
| Vulkan | `13d0eae165cc8fef576bbe611103b0d9dfca5ed0940fd111157a43e8db6e1873` |
| RTX raster | `bafff9ddf3145ff3c76774d0decaad34b9441f3dc512327ce3cd4fe55d996bf5` |

Installed `mp_beach` passes local native module/pure admission, deployment-menu
movie playback, engine capture and normal shutdown in all three. The inspected
captures show textured controls, readable chat and correct alpha borders, with
the decoded briefing movie. The earlier border/label defects are absent.
The world remains obscured by the menu: this does not establish player input,
gameplay, peer interoperability, acoustic output or image parity with retail.
Reports are `material-compression-mp-{glx,vk,rtx}/manifest.json` in the task
directory. All runs use renderer API 23, isolated writable homes, unchanged
read-only installations, unswitched private desktops, windowed mode and no
injected input.

Three additional SP main-menu runs pass boot, normal exit and inspected
textured-menu captures with these renderer modules; reports are
`material-compression-sp-{glx,vk,rtx}/manifest.json`. Six refreshed Q3/QL
captures also pass inspection and module/renderer/windowed checks, recorded in
`material-compression-live/manifest.json`. The default client remains SHA-256
`780ad7be9cf62586e3478c119645df37faca9b9152a747345fc505d5b8d0616c`;
each report records the newly built renderer DLL hashes. Q3 shows its world,
weapon and HUD; QL shows its native join UI over the world without joining.

The default Windows x86 client/server and renderer rebuild passes. The complete
Meson suite passes 111 checks with two skips; the Python aggregate passes
1,377 tests (11 skips, 327.647 s). Logs are
`material-compression-default-build.log`, `material-compression-full-tests.log`
and `material-compression-full-testlog.txt` in `.tmp/rtcw-manager/`.

MP map/briefing runs also pass with legacy audio at verified zero master/music
volume and with focus-muted OpenAL. Both produce inspected engine captures and
exit normally (`material-compression-mp-legacy-fixed/manifest.json` and
`material-compression-mp-openal/manifest.json`). The map has no background
music stream in these observations; that does not prevent cinematic audio.
This is backend/lifecycle evidence, not audible output or phase parity.

The initial `material-compression-mp-legacy` run exposed a probe assumption:
retail startup reset the requested master volume to 0.80. Focus mute was still
observed, but zero-volume evidence failed. The probe now reapplies the volume
settings in its explicit config and verifies both reported master and music
values. It also distinguishes main-menu background-music queue checks from
map probes, which can legitimately stop background music. The old report
retains its failed volume result; the corrected run observes both volumes zero.

The production code is independently authored from original SP/MP shader and
image-creation behaviour. No retail shaders, images or reference implementation
text are incorporated.
