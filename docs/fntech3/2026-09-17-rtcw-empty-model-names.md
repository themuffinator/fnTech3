# RTCW empty model registrations

The `RE_RegisterModel: NULL name` warning in `escape1` also applies to an
empty string. The RTCW UI/cgame bridge already returned handle 0 for null
model names, but forwarded empty strings to the shared renderer, which
printed the warning. Original SP and MP silently return 0 for both forms.

The bridge now handles both forms before calling the renderer. Non-null
pointers still pass the existing VM pointer/bounds checks, and nonempty names
retain normal registration and failure behavior. The common bridge covers
both RTCW modules and all three renderers. Q3/QL dispatch and shared renderer
diagnostics are unchanged.

## Evidence

Windows x86, on an already modified worktree based on
`0b6af9ba7a17d73bf27c5c016c00772a1a93899a`:

- The extended renderer-import regression fails before the correction when
  an empty name reaches the backend, then passes with the correction. It
  covers SP/MP UI/cgame, repeated null and one-byte empty strings, invalid
  pointers, rejected bounds checks, named registrations/backend failures,
  and Q3/QL isolation.
- The focused Windows x86 import/skin/model-shader selection passes four
  tests and nine subtests. The ordinary Win32 Release client builds. The
  development client first links successfully to an isolated path because
  its normal executable is locked by the user's running game. After permission
  to close that session, the normal development client, dedicated server,
  renderer and package targets also build successfully.
- Three installed SP `escape1` probes on GL, Vulkan, and RTX raster complete
  37 seconds after the intro Ready click with zero empty-model warnings and
  no engine errors. Engine `screenshotPNG` captures at approximately 5 and
  35 seconds show the scene and character models. All six PNGs were inspected.
- A client linked from unmodified production translation units also passes
  SP `escape1` and MP `mp_beach` startup on GL, with zero empty-model warnings.
  Both checks also pass against the final rebuilt development executable.
  All four engine captures were inspected. These probes stop at the briefing/menu.

The legitimate installation is
`E:/SteamLibrary/steamapps/common/Return to Castle Wolfenstein`; its file
inventory remains unchanged. Tests use isolated writable homes, 640x480
windowed mode, disabled input devices, and unswitched private desktops.
Only the three cinematic probes perform input: one intro Ready click each,
under the standing AGENTS.md permission. Their private client-frame hook is
compiled separately and does not change production sources or build objects.

Evidence is retained under `.tmp/rtcw-model-name/`: `evidence.json`,
`before-test.log`, `after-test.log`, `focused-x86.log`, the build logs, and
`runtime/manifest.json`, `production-runtime/manifest.json` and
`final-runtime/manifest.json`. Manifests record
commands, executable/renderer hashes, screenshots and results. The historical
GL cinematic log from the transparent-fog checkpoint contains 91 instances of
the warning; its identity is recorded separately from these new probes.
Writable runtime homes are removed. Automatic approval review rejected cleanup
of the remaining task-local outputs with `blocked by policy`; the private
`probe-bin/`, `production-bin/`, generated translation unit/object and linker
response files therefore remain alongside the evidence and reproduction helpers.

## Provenance and limits

The original [SP renderer](https://github.com/id-Software/RTCW-SP/blob/master/src/renderer/tr_model.c)
and [MP renderer](https://github.com/id-Software/RTCW-MP/blob/master/src/renderer/tr_model.c)
were inspected for behavior only. Their GPLv3 implementation text is not
incorporated; the bridge change and regression checks are independently
authored GPLv2-or-later code. No retail modules or assets are included.

This establishes the absent-model return/diagnostic contract and sampled
runtime behavior. It does not close broader model, campaign, interoperability,
or native ray-tracing gates. The separately documented RTX fog-brightness
difference remains visible in the captures; this change does not address it.
