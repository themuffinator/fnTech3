# Windows package staging and extracted-runtime checkpoint

The [clean Windows build at 4383039](2026-09-08-clean-windows-4383039.md) was
installed with Meson using `--no-rebuild --skip-subprojects` and an isolated
DESTDIR. The installed six PE files pass the runtime dependency validator;
the root archive passes validation for 149 audio-zone sidecars, 149 fog sidecars
and one sound shader.

The public release entry point requires the complete Windows/Linux artifact
set. This checkpoint does not bypass that requirement or produce a public
release. It invokes the same lower-level staging and archive functions from
`scripts/release.py` in the clean source checkout for a local Windows-only probe:
copy_release_artifact_contents, copy_docs, build_fnql_webpak, build_root_archive,
validate_stage_tree, write_release_archive and validate_release_archive_contents.
Three renderer import libraries are filtered out. The resulting archive has
18 files, including the client/server, three renderers, audio-zone tool, root/web
archives, license, credits, third-party notices and the selected player docs.

Writing the same stage twice produces byte-identical ZIP files with SHA-256
`74af32d282583651fdaa5e29bda08027cbd457d0d6e3a8fae1adb8ba402c8ef3`.
This establishes deterministic archiving of these inputs, not reproducible
compiler output or the complete release pipeline. The archive was extracted into
a fresh runtime directory outside the build tree, checked again for unshipped
compiler-runtime dependencies, and used directly for both profile probes.

Both profiles complete a generated fixture-pak HTTP download, automatic reconnect
and pure admission to active state. Q3 renders world/HUD; QL renders native join
UI over the world. Client and dedicated server exit zero without forced
termination in both runs. The same packaged binaries serve both profiles.
All 18 extracted files still match their archive hashes afterward, with no
additional files in the runtime directory. Writable homes are isolated; clients
run windowed with input disabled on private unswitched desktops. Screenshots are
engine-generated and remain untracked, as do legitimate game inputs.

The [evidence record](2026-09-08-windows-package-runtime.json) contains the archive
inventory, checksums, executable identities, configuration, commands and runtime
observations. No source implementation changed. The complete suite result for
these binaries remains the preceding clean-build checkpoint; copying and
archiving them did not require rerunning unrelated source tests.

A documentation gap remains: the packaged compatibility ledger contains relative
links to evidence documents that the minimal player-doc selection does not copy.
For example, its `2026-09-08-http-errors-and-curl-cleanup.md` target is absent.
The existing archive validator checks layout but does not detect those broken
links. This finding does not affect the observed executable runtime results,
but must be addressed before calling the packaged documentation complete.
Existing QL temporary-hunk and image/font warnings remain unresolved. Broader
platform packaging, public-release proof policy, all-renderer runtime coverage
and independent retail interoperability remain open.
