# fnTech3 manual releases

The first fnTech3 version is **0.1.0**, tagged **v0.1.0**. The
[`release` workflow](../../.github/workflows/release.yml) validates pushes to
`main`; only **Run workflow** creates a GitHub release, always as a **draft
prerelease**. Publishing that reviewed draft is a separate manual action.
The workflow never commits version changes, moves tags, clears changelog
history or sends announcements. No AI service, webhook or extra token is needed.

## Version policy

[`version/fntech3_version.h`](../../version/fntech3_version.h) is authoritative.
Use `MAJOR.MINOR.PATCH`, without leading zeroes; tags add `v`. All numeric and
Windows resource forms must agree. Windows limits each component to 65535.
`TWEAK` stays zero for versioned releases; commit IDs and workflow run numbers
belong in `release-manifest.json`, not a fourth version component.

- Patch: fixes compatible with the current minor series.
- Minor: new functionality; during 0.x, also use a minor increase for breaking
  changes and explain the migration in the changelog.
- Major: incompatible changes after 1.0.0. Version 1.0.0 requires the unified
  compatibility gates and release sign-off in [COMPATIBILITY.md](COMPATIBILITY.md).

Once published, never reuse a version or replace its tag or assets. A repair
gets a new patch version. Old FnQL/FnQuake3 tags do not determine fnTech3's next
version. This workflow uses the exact requested tag, not the newest tag or a
commit count. The legacy `manual_release.py` and `manual` packaging channel
remain available for historical local snapshot tooling; they are not the
fnTech3 publication path.

## Maintain the changelog

Add concise, reviewed changes to the root [CHANGELOG.md](../../CHANGELOG.md)
under `## [Unreleased]` as work lands. Group them under useful `###` headings:
Highlights, Compatibility, Rendering and Display, Audio, Builds and Packaging,
Fixes, Documentation and Tooling, or Known limitations. Preserve exact profile
scope and separate observed behavior from inference. Planned work is not a
release entry. Historic FnQL notes under `docs/fnql/` remain archival.

Preparation moves the entire Unreleased body, including prose and limitations,
into a dated version section and creates a fresh Unreleased section. It does
not summarize or discard reviewed text. Entries and dates are newest first.
Do not run the legacy `clear-unreleased` command as part of publication.

## Prepare and tag

Start with reviewed changes on `main`, including the intended changelog bullets.
The preparation command edits only the version header, root changelog and the
two generated landing documents; it does not commit, tag or contact GitHub.

```sh
python scripts/release_workflow.py prepare --version 0.1.0
python scripts/release_workflow.py check
python -m pytest
git diff -- version/fntech3_version.h CHANGELOG.md README.md .install/README.html
```

The date defaults to today in UTC; `--date YYYY-MM-DD` may set a reviewed past
date. Duplicate versions, decreasing versions, missing notes, invalid dates,
stale generated docs and inconsistent resource versions fail validation.

Commit the prepared files through the normal review process, merge to `main`,
and tag the reviewed commit. Ensure your checkout contains that exact commit
and `git status --short` is clean before tagging:

```sh
git tag -a v0.1.0 -m "fnTech3 0.1.0"
git push origin v0.1.0
```

Signed annotated tags are encouraged when a maintainer has a signing key.
Tags must already exist remotely and point to a commit reachable from `main`.
A tag push alone does not publish or start this release workflow.

## Build and review the draft

In GitHub **Actions → release → Run workflow**, use the `main` workflow and
enter `v0.1.0` as the tag. Equivalently:

```sh
gh workflow run release.yml --ref main -f tag=v0.1.0
```

The workflow resolves and validates the tag, header and dated changelog, then
pins every job to the full source commit. It runs the complete source suite,
Windows MSVC and MinGW builds and Meson suites, Linux i686 Meson regressions,
and the Ubuntu 20.04 x86 distribution build. Runtime dependency, glibc baseline
and package layout checks must pass. A failed or skipped gate prevents a draft.

The draft contains exactly:

- `fntech3-0.1.0-windows-msvc-x86.zip`
- `fntech3-0.1.0-windows-mingw-x86.zip`
- `fntech3-0.1.0-linux-x86.tar.gz`
- `release-manifest.json` (full source SHA, workflow run/attempt, compatibility
  scope and archive checksums)
- `SHA256SUMS.txt` (all three archives and the manifest)

Downloads are also retained as the `release-v0.1.0` workflow artifact for 30
days. The packager includes the changelog, compatibility ledger, legal
notices, `README.html` and the player guides as web pages in `manual/`,
built by `scripts/player_manual.py`; a broken link between guides stops it.
All packages are engine-only, and none enables a game development option:
they start Quake III Arena and Quake Live and refuse the experimental games.

Verify the five uploaded assets and release notes, extract and test the target
builds with legitimate installations and isolated writable home paths, and
record relevant runtime evidence. Runtime testing must be windowed and obey
the project's input and engine-screenshot rules. CI success does not establish
retail runtime behavior or complete compatibility. Keep the **prerelease**
flag set while the applicable gates remain open, then click **Publish release**
when the scoped development release is ready.

After downloading all five assets into one directory, Linux users can run
`sha256sum --check SHA256SUMS.txt`; Windows users can compare
`Get-FileHash -Algorithm SHA256 <file>` with the published checksum file.

## Retries and failures

Runs for the same tag are serialized and do not cancel uploads. A retry can
repair an unpublished draft only when its tag, full target SHA and prerelease
state agree. Unexpected assets or an already published release cause failure.
All local checksums are verified before uploading; GitHub's returned asset
sizes and SHA-256 digests are checked afterwards. A failed upload leaves a
draft for inspection; retry the workflow for the same tag. Do not publish while
a workflow is uploading or retrying. Never delete or move a published tag to
make a retry succeed.

If an unpublished tag was created from incorrect source, prepare a new version
and tag; inspect and remove any abandoned draft manually. If a build gate fails,
fix and review the source before preparing a new version. After publication,
leave the frozen changelog section in place and accumulate subsequent changes
under Unreleased until the next explicit version preparation.

The `candidate` packaging channel records runtime proof as **not-established**
when none was supplied. It is exclusively for these scoped prereleases. The
existing `release` channel still requires reviewed GLx runtime evidence and
applicable rollback metadata; switching a label does not satisfy that gate or
the separate unified compatibility ledger. A future stable publication path
must bind the required evidence to the exact binaries being published.

Repository administrators should restrict `v*` tag creation/update/deletion
to maintainers and enable immutable published releases where available. These
repository settings are not configured by source files. The workflow needs
only the built-in `GITHUB_TOKEN`, with `contents: write` limited to its draft
job. Draft creation uses GitHub CLI's documented
[`--draft`, `--prerelease` and `--verify-tag` options](https://cli.github.com/manual/gh_release_create).
Draft discovery uses the authenticated, paginated
[release listing API](https://docs.github.com/en/rest/releases/releases#list-releases),
which includes drafts; the by-tag REST endpoint is for published releases.
