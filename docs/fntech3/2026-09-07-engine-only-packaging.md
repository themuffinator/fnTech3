# Engine-only release packaging — 2026-09-07

The inherited release workflow fetched a proprietary `fnql_steam.dll` into
each Windows artifact, and the packager required that DLL. This contradicted
fnTech3's engine-only distribution contract. The preserved provider notice
grants bundling rights to official FnQL releases; this change does not extend
or reinterpret those rights.

Both Windows CI lanes now omit provider staging. Stage and archive validation
accept provider-free Windows packages and reject known provider/Valve Steam
library filenames on every platform, including nested directories and mixed
case. The optional runtime loader and its public ABI are unchanged. Users can
supply a compatible provider separately; this does not establish successful
authentication or online-service interoperability.

README templates and generated installation documentation describe the current
distribution policy. The inherited provider document explicitly distinguishes
fnTech3 policy from historical FnQL terms. The notice, pinned metadata and
standalone historical staging tool remain in the source tree; release CI does
not invoke that tool. No proprietary implementation was added or distributed
by this change.

Validation on Windows, based on `8a02323` plus this patch:

- Release packaging and provider-contract tests: 69 passed, 1 skipped,
  14 subtests passed. The skip is Windows symlink creation privilege.
- A package containing the existing shared x86 client, dedicated server, all
  three renderer DLLs, generated root/WebUI packages and release documentation
  passed both stage and archive validation without a provider.
- Local archive: `.tmp/engine-only-package-20260907.zip`, SHA-256
  `8f2d14c5848dd3c3df3036aa54fedbe7c29ecaabd6d9201e16b4e9a4bb8b5c3a`.
  This is a packaging probe, not a reproducible-release or runtime claim.
- Engine binary identity is unchanged from the
  [reference interoperability checkpoint](2026-09-07-q3-reference-interop.md).

The filename checks enforce the known packaging boundary; they are not a
general scanner for renamed proprietary material. Package-content provenance,
clean CI validation of this workflow revision and packaged Q3/QL runtime gates
remain required before release sign-off. Existing retail interoperability,
shader/model warnings and the RTX CI dependency-download failure are unchanged.
