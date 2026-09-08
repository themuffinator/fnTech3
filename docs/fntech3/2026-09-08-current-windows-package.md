# Windows package validation after the filesystem corrections

A local Windows x86 stage at clean checkout 51368e7 uses Meson installation
with --no-rebuild and --skip-subprojects into an isolated destination. It packages
the existing corrected MSVC x86 binaries through the production release staging,
root/web archive builders, documentation copy and validation functions. This is
an incremental-build checkpoint, not a fresh compiler build or the complete
multi-platform public release entry point.

The stage filters three renderer import libraries and contains 224 files,
including the client, dedicated server, three renderers, audio-zone tool,
engine-owned root/web archives, license/notices and the fnTech3 evidence documents.
All six PE files pass the runtime-dependency validator. Stage and archive checks
validate layout and internal evidence links. The [record](2026-09-08-current-windows-package.json)
contains the complete file/hash inventory and archive identity.

Two archives are byte-identical after changing every staged file timestamp:
8,342,144 bytes each. This proves deterministic archiving of these fixed inputs,
not reproducible compiler artifacts. The archive is extracted to a new directory
outside the build tree and checked again. Both profiles launch the identical
extracted client and dedicated binaries directly from that directory.

Q3 and QL each complete the exact synthetic HTTP pak download and pure admission.
Engine screenshots show Q3 world/weapon/HUD and QL world with its native join panel.
All four processes exit zero without forced termination; probe durations are
36.110 and 35.500 seconds. These are short self-pairs, not independent retail
interoperability or active QL match evidence. After both runs all 224 extracted
files still match their package hashes, with no extra runtime-directory files.

Clients use windowed private unswitched desktops, disabled input/audio/optional
services and isolated writable homes. Legitimate installations remain read-only.
No keyboard/mouse control or OS screenshot is used. Package binaries, synthetic
runtime outputs and engine screenshots remain ignored. The archive contains the
documentation as of 51368e7; this later result report is not retroactively inserted
into that verified artifact.

No engine source changed in this checkpoint. The preceding
[allocation correction](2026-09-08-file-allocation-bounds.md) supplies the complete
Windows suite result for these binaries; unrelated tests were not rerun for
copying and archiving them. Broader platform/package matrices, compiler
reproducibility, all-renderer runtime, optional services and full compatibility
remain open. Existing image/font and QL temporary-hunk diagnostics remain outside
this checkpoint. No external implementation was incorporated.
