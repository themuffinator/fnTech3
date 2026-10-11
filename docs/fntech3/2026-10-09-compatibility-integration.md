# Compatibility work integration, 2026-10-09

This pass resumes the interrupted Alice and Elite Force work and validates
the combined uncommitted compatibility slices. It preserves their original
evidence dates and limits. No full compatibility gate closes, and the
recovered changes remain uncommitted. The
[JSON companion](2026-10-09-compatibility-integration.json) records build
identities, commands, outcomes and local evidence paths.

## Recovered work

- The [Alice compatibility round](2026-10-08-alice-compatibility-round.md)
  now includes its completed isolated Python suite: 2,639 passed, 129 skipped
  and 65,346 subtests passed. Its
  [module-signature follow-up](2026-10-08-alice-module-signatures.md) also
  completed 615 relevant tests. These are historical isolated-tree results,
  separate from the combined-tree checks below.
- The interrupted [Elite Force static-view record](2026-10-09-stvef-sp-retail-views.md)
  is complete: 80 main-sweep runs on eight scenes plus presentation controls.
  All 72 saved image comparisons were recomputed from engine screenshots.
  GLx, Vulkan and RTX raster SDR are covered; default RTX HDR and native ray
  tracing are not. Remaining image differences are recorded, not erased by
  changing fnTech3's presentation defaults.
- The combined tree includes the two Jedi audits, Jedi UI and localized
  voice work, shared interface-audio policy, MP3 last-frame correction,
  Alice compatibility and signature tables, and MOHAA GameSpy LAN search.
  Their records remain the authority for their narrower runtime claims.

## Integration corrections

The RTCW speech test's synthetic sound object was missing the new Academy
lip-sync members used by the shared production audio code. Its fixture now
includes the real lip-sync helper and initializes the Academy-only state,
while selecting the RTCW branch. The production speech behavior is unchanged.

The MOHAA probe's command manifest previously attempted password redaction
without matching the actual command-line shape. It now redacts every
case-insensitive `+set rconPassword` value in the recorded command while
preserving the launch arguments. A mocked regression test covers repeated
settings and verifies that only the manifest is redacted.

The Elite Force view probe now fails for abnormal exits, missing or extra
captures, missing or mismatched view positions, mismatched image dimensions,
or an altered installation. Unit tests cover these checks without launching
a game. The saved historical manifests satisfy the new checks.

Outcast's installed voice inventories were recounted excluding ZIP directory
entries: English 2,895, German 2,358 and French 2,356. The
[MP3 record](2026-10-08-jk2-mp3-sounds.md) now preserves its original
7,970/7,971 result as history and links the later 7,971/7,971 correction and
completed foreign-voice slice.

## Combined-tree validation

The Windows x86 `meson/build/win32-dev` preset builds the client, dedicated
server and GLx/Vulkan/RTX renderers with strict warnings and every development
profile enabled. The initial build was based on `60d3a85a` plus the recovered
working tree. A separate task committed the
[profile-frame rules](2026-10-09-profile-frame-rules.md) as `6a8ea608` while
this validation was running. The final incremental build includes that
commit and the recovered changes; its exact source and output hashes are in
the companion. Earlier runtime probes retain their earlier binary identities.

The native/project Meson suite, excluding its recursive Python wrapper,
passes **263 tests with two skips**. The skips are unavailable OpenAL loopback
support and an optional Quake Live retail-factory fixture. An earlier highly
parallel run exceeded the distance-cull test's 30-second timeout; its isolated
retry and the complete four-worker rerun pass.

The full Python source suite finishes in 47 minutes: **2,670 passed,
125 skipped, 65,820 subtests passed and six failed**. It imported test
definitions before the fixture and concurrent frame-rule changes. The
failures are the old RTCW speech fixture (one), old frame-rule tests (three),
old server-cadence fixture (one) and old entity-arena fixture (one). Their
traces show the missing newly required declarations or obsolete source
expectations. The original log retains these failures; it is not presented
as a clean monolithic pass.

The final focused run passes 14 tests and seven subtests covering RTCW speech,
frame conversion, server cadence, entity bounds and both probe helpers.
This reruns every affected current test file against the final source.
The skipped audio-zone tool cases also pass with the built executable supplied
through `FNQL_AUDIOZONESC`: 19 tests and 127 subtests. That resolves the
114 audio-tool skips; the other 11 skips require opt-in runtime probes, a
discovered Quake Live factory installation, or Windows symlink privileges.
The four new helper tests also pass directly under Linux/WSL's Python; this
is helper portability evidence, not a Linux engine build. The two script-only
checks outside Meson's registered source checks pass directly.
Documentation generation, 20 documentation/manual tests with 832 subtests,
909 local documentation link targets and `git diff --check` pass as well.

The fresh MOHAA loopback probe passes **52/52 checks** across dedicated,
listen-server and LAN-search cases, with the retail installation unchanged.
All five recorded rcon settings are redacted. It uses windowed processes,
isolated home directories and engine screenshots, with no injected input.
Its command lines, fixture and executable identities are retained in the
[LAN-search companion](2026-10-08-mohaa-gamespy-lan-search.json).

## Limits and follow-up

One specifically authorized Alice synthetic-input run exited normally and
left its installation unchanged, but the recipe began during the opening
cinematic and used Quake's unsupported `+attack` binding. It does not establish
gameplay input. A corrected recipe skips the cinematic, equips the mallet and
uses Alice's `+attackright`; its separate per-run permission request remains
pending. No engine defect is inferred from the invalid recipe.

No retail Alice or MOHAA interoperability is established here. Listening,
physical input, wider renderer comparisons and the other open profile gates
remain open. The MP3 work also retains its known unrelated limitation:
34 F.A.K.K.2 music files have differing sizing and decoded-sample counts,
as detailed in the [last-frame record](2026-10-08-mp3-last-frame.md).

Useful logs, commands, manifests and engine captures remain under
`.tmp/claude-continuation/`. Automatic approval review rejected both verified
recursive cache cleanup and narrower literal-file deletion with only
"blocked by policy" as its reason. The disposable probe caches therefore
remain there as well; no cleanup bypass was attempted.
