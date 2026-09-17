# RTCW shader animation limits

The `entityOnFire1` and `entityOnFire2` warnings came from applying Q3's
eight-frame ordinary-shader limit to RTCW materials. Both original RTCW
profiles allow sixteen frames. The existing shared texture bundle already
holds twenty-four, so this correction changes the parser's profile selection
without changing renderer storage or an external ABI.

GL, Vulkan and RTX now retain up to sixteen frames for ordinary RTCW shaders,
including selected `animmapcomp`/`animmapnocomp` alternatives. Q3 and QL retain
their eight-frame ordinary-shader limit. The existing `.shaderx` extension
retains twenty-four frames in every profile. Excess images still produce a
warning and are consumed without loading or indexing beyond the selected limit.

## Evidence

Windows, on the modified worktree based on
`0b6af9ba7a17d73bf27c5c016c00772a1a93899a`:

- The new compiled regression executes each renderer's actual animation
  parser branch with its real storage fields and the engine tokenizer. It
  fails on retained frame counts before the fix in all three renderers and
  passes afterward. It checks all four profiles, ordinary/extended shaders,
  frame order through the final retained image, 0/1/8/9/15/16/17/24/25/64
  frames, warning limits, following directives, missing frequency/images,
  and image flags. The initial before/after fixture runs target Windows x64;
  the subsequent focused run targets the shared Windows x86 contract.
- Fourteen focused shader tests pass on Windows x86. The client and all three
  renderer targets build with the existing strict MSVC development preset
  under `meson/build/win32-dev`.
- Read-only inspection of the legitimate installation finds sixteen frames
  in each MP fire animation and both SP compression alternatives. The prior
  GL SP/MP startup logs contain the two reported warnings; their identities
  are retained as historical evidence, separately from the new runs.
- Six windowed installed-map probes pass: SP `escape1` and MP `mp_beach`
  through GL, Vulkan and RTX raster. All exit normally with zero excess
  animation warnings and no engine errors. The six engine `screenshotPNG`
  captures were inspected: SP reaches its briefing and MP its deployment
  menu. No input or desktop switching is used. Writable homes are isolated
  and removed; the installation inventory stays unchanged.

Evidence is retained in `.tmp/rtcw-animmap/`: `before-test.log`,
`after-test.log`, `focused-x86.log`, `build.log`, `installed-shaders.json`,
`evidence.json`, and `runtime-{glx,vk,rtx}/`. Runtime manifests record exact
commands, executable/renderer identities, installation, profile, map, exit
status and capture hashes. The installation is
`E:/SteamLibrary/steamapps/common/Return to Castle Wolfenstein`.

## Provenance and limits

The original [SP header](https://github.com/id-Software/RTCW-SP/blob/master/src/renderer/tr_local.h)
and [MP header](https://github.com/id-Software/RTCW-MP/blob/master/src/renderer/tr_local.h)
provide the sixteen-frame contract; their local SHA-256 identities are in
`evidence.json`. Their GPLv3 source was inspected for this limit only. The
production change and synthetic tests are independently authored; no reference
implementation text or retail shaders/assets are incorporated.

The runtime probes establish fire-material registration and removal of the
reported truncation warnings. They do not exercise burning entities, establish
fire-effect GPU parity, validate native ray tracing or close broader RTCW gates.
The full automated suite was not rerun for this bounded parser correction.

Other observed diagnostics remain outside this correction: IPv6 loopback bind
failure, map bevel/lightmap warnings, missing light-source/MP console images,
MP TGA orientation warnings, and image reuse with different flags. The extra
retained SP frames also encounter the existing image-reuse diagnostic. RTX
prints successful MDC model-fallback warnings, and its SP briefing body text
has lower contrast than GL/Vulkan in these captures. These observations are
recorded separately from the resolved animation limit.
