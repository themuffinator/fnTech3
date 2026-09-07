# Regression checkpoint — 2026-09-08

At `b60580f`, the complete local Windows MSVC x86 Meson suite passed 110
tests with zero failures and one unavailable OpenAL-loopback skip. The
aggregate Python suite ran 1,250 tests with seven skips. This includes the
new native/pure selection and shader RGB parser regressions.

The [local record](evidence/2026-09-08-regression-checkpoint.json) pins
executable/renderer and log hashes. The run used `vcvars32` and the existing
Meson build with `--no-rebuild --print-errorlogs` after rebuilding changed
targets. It establishes complete local regression coverage, not a clean build
or closure of runtime compatibility gates.

Separately verified clean CI results:

- [Linux verification at `57f201a`](https://github.com/themuffinator/fnTech3/actions/runs/34167526496)
  passed Make/Meson/CMake i686 dedicated builds, renderer builds, the complete
  i686 suite and the bundled SDL3 client build.
- [Release validation at `62f97d1`](https://github.com/themuffinator/fnTech3/actions/runs/34167668744)
  passed Windows MSVC/MSYS2 x86, Ubuntu x86, source validation and Linux i686
  regressions. Publication was skipped by that push workflow; this is build
  and packaging validation, not a published-release claim.

Those CI revisions include the config-migration correction and earlier
engine-only packaging work. They precede the native pure-selection and shader
RGB corrections. CI for the latter changes was still running/queued when
checked; it is not recorded as a pass. No new unrelated issue was found.
