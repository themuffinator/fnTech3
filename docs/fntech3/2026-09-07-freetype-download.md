# FreeType download endpoint — 2026-09-07

Clean Linux regression and RTX CI lanes repeatedly stopped while downloading
FreeType from Savannah, with HTTP 502 responses and read timeouts. The logs
show configuration failed before compilation or tests; the subsequent missing
Meson build-directory error was secondary. These runs cannot establish a
compiler or compatibility regression.

FreeType's [official download page](https://freetype.org/download) lists
SourceForge alongside Savannah. The wrap now uses that project's SourceForge
endpoint as its primary URL. Version 2.14.3, the archive filename, WrapDB
fallback and SHA-256 pin are unchanged. Independent Windows and WSL downloads
both returned 2,670,220 bytes with SHA-256
`36bc4f1cc413335368ee656c42afca65c5a3987e8768cc28cf11ba775e785a5f`.
No dependency implementation, licensing, build options or checksum policy was
changed. Existing dependency provenance and notices still apply.

A fresh WSL Meson fallback project also downloaded and unpacked FreeType from
the new endpoint; its log is `.tmp/freetype-mirror-probe.log`. Direct downloads
and this local probe do not establish hosted-runner availability. Clean CI
must still validate the changed wrap. Vulkan verification now triggers on
wrap-file changes, matching the dependency coverage already present in Linux
verification. Font/platform tests passed: 38 tests and two subtests.

Follow-up: the initial force-fallback project was stopped after expanding into
optional HarfBuzz/GLib dependencies. A separate focused configuration disabled
FreeType's optional harfbuzz/png/zlib/brotli/bzip2 features and completed all
45 build steps, linking `libfreetype.so.6.20.6`; its log is
`.tmp/freetype-mirror-focused.log`. These options apply only to that disposable
download/build probe, not the engine's dependency configuration.

All runtime, interoperability and release gates remain as recorded in the
compatibility ledger. Dependency-host availability remains an external risk;
this change provides an alternative primary endpoint, not an uptime guarantee.
