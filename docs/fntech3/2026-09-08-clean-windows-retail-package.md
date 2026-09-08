# Clean Windows x86 build and retail package validation

A fresh git archive of `b57c0d6` builds successfully with MSVC 19.44.35222 for
Windows x86. The complete suite passes, and an extracted package built from
those outputs passes retail interoperability and one full map change in both
directions for both Q3 and QL. This replaces reliance on incremental binaries
for this Windows package slice, not the complete compatibility or release gate.

The [machine-readable record](2026-09-08-clean-windows-retail-package.json) pins
source/archive identities, compiler recipe, tests, all package files and runtime
outcomes. All 1,461 exported tracked files remain unchanged after build, suite
and runtime testing. Dependency source archives may be reused from Meson's cache;
no compiled outputs were copied into the new build directory.

## Build, tests and package

The fresh release configuration uses static runtime/dependencies, strict warnings,
and separately loaded GLx, Vulkan and RTX renderers. All 1,152 build steps finish.
The full Windows suite reports 110 passed, zero failed and one skipped; its Python
aggregate runs 1,296 tests with five skips in 198.022 seconds. Runtime opt-ins
point to the fresh client and dedicated binaries for legitimate module probes.
These are local results, not new hosted-CI or all-platform results.

Meson installs the build into an isolated destination. Production release staging,
root/web archive generation, layout checks and dependency validation then produce
an engine-only Windows package. All six PE files pass dependency checks. Two ZIPs
are byte-identical after changing every staged file timestamp: 8,461,908 bytes
each. This establishes deterministic archiving of these inputs; compiler-output
reproducibility across independent builds is not claimed.

The archive is extracted to a new runtime directory and checked again. It contains
248 files, including the shared client/dedicated binaries, three renderer modules,
the audio-zone tool and engine-owned archives/docs. The same extracted client and
server files are used for both profiles. Documentation in this tested artifact
is the b57c0d6 version; this later report is not retroactively inserted into it.

## Retail runtime checks

The [synchronized map-change probes](2026-09-08-retail-map-changes.md) run against
the extracted binaries. Q3 uses installed retail quake3.exe; QL uses the installed
retail client and the separately distributed Linux x86 dedicated server under
WSL, with existing external provider/Steam API files for authentication.

All four final pairs pass admission, destination-map rendering and normal exits:
q3dm6 to q3dm7 in both Q3 directions, and Campgrounds to Overkill in both QL
directions. Engine screenshots show destination world, first-person weapon and
HUD. No participant requires forced termination. This exercises packaged startup,
module and asset loading, gamestate renewal and the already established retail
contracts together. It does not extend the short-session evidence to prolonged
matches, impairment, every renderer or every platform.

After all runs, all 248 extracted files still match their hashes, with no added
runtime-directory files. Assets and proprietary runtimes remain external and
untracked. Clients use isolated writable homes, windowed private unswitched
desktops and disabled input/audio. The Linux retail server is unprivileged.
Commands come from startup/local polling configs, and screenshots from engine
render targets. No keyboard/mouse injection, RCON or OS capture was used.

## Limits and diagnostics

The complete public multi-platform release entry point was not run or published.
Linux baseline evidence remains separately scoped to its earlier clean build;
other architectures, native ray tracing, long sessions and optional-service
failure coverage remain open. The local Python lacks tarfile's extraction-filter
argument; the export helper instead validates member paths and rejects links
before extracting the trusted git archive. That setup issue did not require an
engine change.

Existing optional asset, muted-audio, retail website and statistics diagnostics
persist. Their presence does not establish success for those services. This
report and ledger update pass documentation staging/link validation separately
from the unchanged tested package.
