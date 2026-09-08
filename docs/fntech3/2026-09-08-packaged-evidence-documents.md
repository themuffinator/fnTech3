# Include compatibility evidence in player packages

The [Windows package runtime checkpoint](2026-09-08-windows-package-runtime.md)
found that the selected player docs included COMPATIBILITY.md but omitted its
linked fnTech3 evidence documents. Packaging now includes Markdown and JSON
files from docs/fntech3, preserving their relative paths. It excludes other file
types and does not traverse into the repository's ignored local probe inputs.
Symlinked evidence files or resolved paths outside that evidence tree are
rejected. Existing license/credits and other selected player docs remain.

Both stage and final archive validation now check inline file links within the
packaged fnTech3 evidence tree. Fragment suffixes and URL-encoded paths resolve
to their underlying file; missing Markdown/JSON targets fail validation. This
check deliberately covers the repository's current inline link convention, not
a full Markdown grammar or heading-anchor validator. External URLs, source-code
links and references to ignored local investigation files are not bundled
package dependencies. Those local-input references can still require the
original investigation environment; this change does not claim universal
offline link closure.

The regression stages real repository docs, validates their evidence links,
removes a linked document and confirms rejection. A controlled nested fixture
checks fragment handling and the distinction between evidence files, website
links and ignored local probes. The previous 18-file Windows archive is rejected
for its first missing ledger target. The new local Windows package includes 159
evidence files at the test snapshot and passes both stage and ZIP validation.
Two archive writes from fixed inputs have identical SHA-256:
`188f5c0ef4fd48a8fc917750e3189ce17fecefebabd1b706e6e1ddd716e7ac42`.
The count predates this checkpoint's own documentation.

The Windows packaging suite passes 46 tests with one platform-specific skip;
WSL Ubuntu passes all 46 tests. Final stage/archive validation also passes after
the stage reader was limited to Markdown payloads, avoiding redundant binary
reads for link checking. Validation log identities and the fixed-input archive
record are in the [evidence record](2026-09-08-packaged-evidence-documents.json).

This change affects packaging scripts and tests, not engine binaries. It uses
the previously verified clean Windows binaries from 4383039. No gameplay rerun
or clean compiler rebuild is claimed for a documentation-only payload change.
The local Windows archive is a diagnostic artifact; public release still
requires the complete supported artifact set and its proof gates. No external
implementation was incorporated. Existing runtime warnings remain outside this
change. No additional unrelated engine issue was found.
