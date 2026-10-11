from __future__ import annotations

import json
import re
import shutil
import tempfile
import unittest
from pathlib import Path
from types import SimpleNamespace
from unittest import mock

from scripts import fntech3_meta, release, release_workflow as workflow

ROOT = Path(__file__).resolve().parents[1]


class ReleaseWorkflowTests(unittest.TestCase):
    def setUp(self):
        scratch = ROOT / ".tmp" / "release-workflow-tests"
        scratch.mkdir(parents=True, exist_ok=True)
        self.temporary = tempfile.TemporaryDirectory(dir=scratch)
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        for name in (workflow.VERSION_PATH, "docs/templates/README.md.in",
                     "docs/templates/install-readme.html.in", "README.md", ".install/README.html"):
            target = self.root / name
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(ROOT / name, target)
        self.changelog = self.root / "CHANGELOG.md"
        self.changelog.write_text("# fnTech3 Changelog\n\n## [Unreleased]\n\n"
                                  "### Known limitations\n\n- A reviewed change.\n\n"
                                  "Keep this exact explanatory paragraph.\n", encoding="utf-8")
        self.header = self.root / workflow.VERSION_PATH
        # Keep fixtures independent of the repository's next release version.
        header = self.header.read_text(encoding="utf-8")
        for key, value in {"MAJOR": "0", "MINOR": "1", "PATCH": "0", "TWEAK": "0", "STRING": '"0.1.0"'}.items():
            header = re.sub(rf"(?m)^#define FNTECH3_VERSION_{key} .+$", f"#define FNTECH3_VERSION_{key} {value}", header)
        header = re.sub(r"(?m)^(#define FNTECH3_WINDOWS_(?:FILE|PRODUCT)_VERSION) .+$", r"\1 0,1,0,0", header)
        self.header.write_text(header, encoding="utf-8")
        self.info = {"version": "0.1.0", "release_tag": "v0.1.0", "head_commit": "a" * 40,
                     "build_date": "2026-09-01"}

    def prepare(self, version="0.1.0", date="2026-09-01"):
        workflow.prepare(version, date, root=self.root)

    def test_prepare_keeps_reviewed_prose_and_synchronizes_every_representation(self):
        self.prepare()
        workflow.check(self.root)
        text = self.changelog.read_text(encoding="utf-8")
        self.assertIn("## [Unreleased]\n\n_None yet._", text)
        self.assertIn("## [0.1.0] - 2026-09-01", text)
        self.assertIn("### Known limitations", text)
        self.assertIn("Keep this exact explanatory paragraph.", text)
        self.assertEqual(fntech3_meta.base_metadata(self.header)["version"], "0.1.0")
        self.assertEqual(fntech3_meta.read_version_defines(self.header)["FNTECH3_WINDOWS_FILE_VERSION"], "0,1,0,0")

    def test_next_version_preserves_history_and_refreshes_generated_docs(self):
        self.prepare()
        text = self.changelog.read_text(encoding="utf-8").replace("_None yet._", "- New fix.", 1)
        self.changelog.write_text(text, encoding="utf-8")
        self.prepare("0.1.1", "2026-09-02")
        workflow.check(self.root)
        self.assertEqual([entry[0] for entry in workflow.sections(self.changelog.read_text())],
                         ["Unreleased", "0.1.1", "0.1.0"])
        self.assertEqual(fntech3_meta.base_metadata(self.header)["version"], "0.1.1")

    def test_invalid_preparations_leave_all_files_unchanged(self):
        before = {p: p.read_bytes() for p in self.root.rglob("*") if p.is_file()}
        for version, date in (("0.0.9", "2026-09-01"), ("01.1.0", "2026-09-01"),
                              ("0.1.0", "2099-01-01"), ("0.1.0", "2026-02-30"),
                              ("0.1.0", "20260901"), ("65536.0.0", "2026-09-01")):
            with self.subTest(version=version, date=date), self.assertRaises(ValueError):
                self.prepare(version, date)
            self.assertEqual(before, {p: p.read_bytes() for p in before})

    def test_repeated_prepare_rejects_duplicate_version_without_erasing_notes(self):
        self.prepare()
        before = self.changelog.read_bytes()
        with self.assertRaisesRegex(ValueError, "already prepared"):
            self.prepare()
        self.assertEqual(before, self.changelog.read_bytes())

    def test_empty_unreleased_is_rejected(self):
        self.changelog.write_text("# Changelog\n\n## [Unreleased]\n\n_None yet._\n")
        with self.assertRaisesRegex(ValueError, "reviewed changelog"):
            self.prepare()

    def test_windows_resource_drift_and_duplicate_defines_fail(self):
        header = self.header.read_text()
        with self.assertRaisesRegex(ValueError, "WINDOWS_FILE_VERSION"):
            fntech3_meta.base_metadata(text=header.replace("FILE_VERSION 0,1,0,0", "FILE_VERSION 0,2,0,0"))
        with self.assertRaisesRegex(ValueError, "Duplicate"):
            fntech3_meta.base_metadata(text=header + "\n#define FNTECH3_VERSION_PATCH 1\n")

    def test_tag_requires_exact_header_and_frozen_nonempty_changelog(self):
        with self.assertRaisesRegex(ValueError, "dated"):
            workflow.check_content(self.header.read_text(), self.changelog.read_text(), "0.1.0")
        self.prepare()
        with self.assertRaisesRegex(ValueError, "canonical version"):
            workflow.check_content(self.header.read_text(), self.changelog.read_text(), "0.1.1")
        pending = self.changelog.read_text().replace("_None yet._", "- Pending change.", 1)
        with self.assertRaisesRegex(ValueError, "pending"):
            workflow.check_content(self.header.read_text(), pending, "0.1.0")

    def test_duplicate_or_malformed_changelog_headings_fail(self):
        for suffix in ("## [Unreleased]\n", "## [0.1.0]\n- Fix\n", "## Broken\n"):
            with self.subTest(suffix=suffix), self.assertRaises(ValueError):
                workflow.sections(self.changelog.read_text() + "\n" + suffix)

    def test_metadata_reads_annotated_tag_not_changed_worktree_or_legacy_tags(self):
        self.prepare()
        def git(*args):
            return workflow.git(*args, root=self.root)
        git("init", "--initial-branch=main")
        git("add", ".")
        git("-c", "user.name=Test", "-c", "user.email=test@example.invalid", "commit", "-m", "Prepared")
        git("-c", "user.name=Test", "-c", "user.email=test@example.invalid", "tag", "-a", "v0.1.0", "-m", "Reviewed")
        git("tag", "0.1.0.999-20260901-deadbeef")
        original = workflow.metadata("v0.1.0", root=self.root)
        self.header.write_text("invalid working copy\n")
        self.assertEqual(original, workflow.metadata("v0.1.0", root=self.root))
        self.assertEqual(original["head_commit"], git("rev-parse", "HEAD"))
        rendered = workflow.notes(original, root=self.root)
        self.assertIn("Keep this exact explanatory paragraph.", rendered)
        self.assertIn(original["head_commit"], rendered)
        self.assertIn("does not establish full unified compatibility", rendered)
        for invalid in ("--help", "v0.1.0;echo bad", "v0.1.0\nfoo", "v0.1.0.1", "0.1.0"):
            with self.subTest(tag=invalid), self.assertRaises(ValueError):
                workflow.metadata(invalid, root=self.root)

    def test_large_release_notes_keep_highlights_limits_and_link_full_history(self):
        changelog = ("# Changelog\n\n## [Unreleased]\n\n_None yet._\n\n"
                     "## [0.1.0] - 2026-09-01\n\n### Highlights\n\n"
                     "- Play [Q3](docs/GAMES.md#quake-iii).\n\n### Compatibility\n\n"
                     + "- Detailed evidence.\n" * 4000
                     + "\n### Known limitations\n\n- Experimental profiles stay disabled.\n")
        with mock.patch.object(workflow, "git", return_value=changelog):
            rendered = workflow.notes(self.info, root=self.root)
        self.assertLess(len(rendered.encode("utf-8")), 60_000)
        source = f"https://github.com/themuffinator/fnTech3/blob/{self.info['head_commit']}"
        self.assertIn(f"[Q3]({source}/docs/GAMES.md#quake-iii)", rendered)
        self.assertIn("Experimental profiles stay disabled.", rendered)
        self.assertIn(f"[Complete versioned changelog]({source}/CHANGELOG.md)", rendered)
        self.assertNotIn("Detailed evidence.", rendered)

    def make_manifest(self):
        packages = self.root / "packages"
        packages.mkdir()
        archives = []
        for suffix in workflow.ARTIFACTS:
            path = packages / f"fntech3-0.1.0-{suffix}"
            path.write_bytes(f"fixture: {suffix}".encode())
            archives.append({"archive": path.name, "sha256": workflow.sha256(path)})
        manifest = {"project": "fnTech3", "channel": "candidate", "prerelease": True,
                    "release_tag": "v0.1.0", "version": "0.1.0", "source_commit": "a" * 40,
                    "build_date": "2026-09-01", "archives": archives}
        path = self.root / "release-manifest.json"
        path.write_text(json.dumps(manifest))
        sums = [f"{entry['sha256']}  {entry['archive']}" for entry in archives]
        sums.append(f"{workflow.sha256(path)}  {path.name}")
        (self.root / "SHA256SUMS.txt").write_text("\n".join(sums) + "\n")
        return path

    def test_manifest_checks_exact_archives_source_identity_and_checksums(self):
        path = self.make_manifest()
        files = workflow.release_files(path, self.info)
        self.assertEqual(len(files), 5)
        with self.assertRaisesRegex(ValueError, "source_commit"):
            workflow.release_files(path, {**self.info, "head_commit": "b" * 40})
        files[-1].write_bytes(b"tampered")
        with self.assertRaisesRegex(ValueError, "checksum"):
            workflow.release_files(path, self.info)

    def test_bad_checksum_list_or_traversal_archive_is_rejected(self):
        path = self.make_manifest()
        sums = self.root / "SHA256SUMS.txt"
        sums.write_text(sums.read_text() + "bad  extra.zip\n")
        with self.assertRaisesRegex(ValueError, "SHA256SUMS"):
            workflow.release_files(path, self.info)
        manifest = json.loads(path.read_text())
        manifest["archives"][0]["archive"] = "../escape.tar.gz"
        path.write_text(json.dumps(manifest))
        with self.assertRaisesRegex(ValueError, "exactly"):
            workflow.release_files(path, self.info)

    def test_draft_validation_blocks_published_wrong_commit_and_unknown_assets(self):
        files = workflow.release_files(self.make_manifest(), self.info)
        valid = {"draft": True, "prerelease": True, "tag_name": "v0.1.0",
                 "target_commitish": "a" * 40, "assets": []}
        workflow.validate_draft(valid, self.info, files)
        for change in ({"draft": False}, {"target_commitish": "b" * 40},
                       {"prerelease": False}, {"assets": [{"name": "unexpected.zip"}]}):
            with self.subTest(change=change), self.assertRaises(ValueError):
                workflow.validate_draft({**valid, **change}, self.info, files)

    def test_candidate_is_versioned_but_does_not_claim_runtime_proof(self):
        fixture = fntech3_meta.base_metadata(self.header)
        with mock.patch.object(fntech3_meta, "base_metadata", return_value=fixture):
            meta = fntech3_meta.channel_metadata("candidate", ref_name="v0.1.0")
        self.assertEqual(meta["archive_prefix"], "fntech3-0.1.0")
        self.assertEqual(meta["release_tag"], "v0.1.0")
        proof = release.resolve_glx_runtime_proof(SimpleNamespace(channel="candidate", glx_proof_root=None))
        self.assertEqual(proof["status"], "not-established")
        with self.assertRaises(ValueError):
            fntech3_meta.channel_metadata("candidate", build_number=42)
        with self.assertRaises(ValueError):
            release.resolve_glx_runtime_proof(SimpleNamespace(channel="release", glx_proof_root=None))

    def exercise_draft(self, *, existing=None, moved_tag=False, corrupt_upload=False):
        path = self.make_manifest()
        files = workflow.release_files(path, self.info)
        note_path = self.root / "notes.md"
        note_path.write_text("Reviewed notes\n")
        uploaded = {"draft": True, "prerelease": True, "tag_name": "v0.1.0",
                    "target_commitish": "a" * 40, "html_url": "https://example.invalid/draft",
                    "assets": [{"name": p.name, "size": p.stat().st_size,
                                "digest": "sha256:" + workflow.sha256(p)} for p in files]}
        if corrupt_upload:
            uploaded["assets"][0]["digest"] = "sha256:" + "0" * 64
        calls = []
        api_calls = 0
        def fake_gh(*args, **kwargs):
            nonlocal api_calls
            calls.append(args)
            if args[0] == "api":
                if "/git/ref/" in args[-1]:
                    return json.dumps({"object": {"type": "commit", "sha": remote_sha}})
                api_calls += 1
                if api_calls == 1:
                    return json.dumps([[] if existing is None else [existing]])
                return json.dumps([[uploaded]])
            return ""
        remote_sha = "b" * 40 if moved_tag else "a" * 40
        with mock.patch.object(workflow, "metadata", return_value=self.info), \
             mock.patch.object(workflow, "notes", return_value="Reviewed notes\n"), \
             mock.patch.object(workflow, "git", return_value="a" * 40), \
             mock.patch.object(workflow, "gh", side_effect=fake_gh):
            try:
                result = workflow.create_draft("v0.1.0", path, note_path)
            finally:
                self.draft_calls = calls
        return result

    def test_create_draft_verifies_local_and_uploaded_assets(self):
        self.assertEqual(self.exercise_draft(), "https://example.invalid/draft")
        create = next(call for call in self.draft_calls if call[:2] == ("release", "create"))
        self.assertIn("--verify-tag", create)
        self.assertIn("--draft", create)
        self.assertIn("--prerelease", create)
        self.assertIn("a" * 40, create)
        self.assertEqual(sum(call[:2] == ("release", "upload") for call in self.draft_calls), 1)

    def test_retry_repairs_only_same_commit_draft(self):
        existing = {"draft": True, "prerelease": True, "tag_name": "v0.1.0",
                    "target_commitish": "a" * 40, "assets": []}
        self.exercise_draft(existing=existing)
        self.assertFalse(any(call[:2] == ("release", "create") for call in self.draft_calls))
        self.assertTrue(any(call[:2] == ("release", "upload") for call in self.draft_calls))

    def test_published_release_cannot_be_modified(self):
        with self.assertRaisesRegex(ValueError, "already published"):
            self.exercise_draft(existing={"draft": False, "tag_name": "v0.1.0"})
        self.assertTrue(all(call[0] == "api" for call in self.draft_calls))

    def test_moved_remote_tag_prevents_github_writes(self):
        with self.assertRaisesRegex(ValueError, "tag moved"):
            self.exercise_draft(moved_tag=True)
        self.assertTrue(all(call[0] == "api" for call in self.draft_calls))

    def test_upload_digest_mismatch_leaves_release_unpublished(self):
        with self.assertRaisesRegex(ValueError, "Uploaded asset verification failed"):
            self.exercise_draft(corrupt_upload=True)
        self.assertFalse(any("--draft=false" in call for call in self.draft_calls))

    def test_github_api_errors_are_not_treated_as_missing_releases(self):
        for status in ("HTTP 403", "HTTP 404", "HTTP 500"):
            with mock.patch.object(workflow.subprocess, "run", return_value=SimpleNamespace(returncode=1, stderr=status, stdout="")):
                with self.assertRaisesRegex(ValueError, status):
                    workflow.find_release("v0.1.0")

    def test_draft_lookup_paginates_and_refuses_duplicate_tag_entries(self):
        draft = {"tag_name": "v0.1.0", "draft": True}
        with mock.patch.object(workflow, "gh", return_value=json.dumps([[{"tag_name": "v0.2.0"}], [draft]])) as cli:
            self.assertEqual(workflow.find_release("v0.1.0"), draft)
            self.assertIn("--paginate", cli.call_args.args)
        with mock.patch.object(workflow, "gh", return_value=json.dumps([[draft], [draft]])):
            with self.assertRaisesRegex(ValueError, "Multiple releases"):
                workflow.find_release("v0.1.0")

    def test_annotated_remote_tag_is_peeled_to_its_source_commit(self):
        annotation = {"object": {"type": "tag", "sha": "b" * 40}}
        target = {"object": {"type": "commit", "sha": "a" * 40}}
        with mock.patch.object(workflow, "gh", side_effect=[json.dumps(annotation), json.dumps(target)]):
            self.assertEqual(workflow.remote_tag_commit("v0.1.0"), "a" * 40)

    def test_workflow_pins_jobs_and_keeps_draft_creation_manual(self):
        text = (ROOT / ".github/workflows/release.yml").read_text()
        self.assertEqual(text.count("ref: ${{ needs.prepare.outputs.head_commit }}"), 6)
        self.assertIn("github.event_name == 'workflow_dispatch'", text)
        self.assertIn("cancel-in-progress: false", text)
        self.assertIn("git merge-base --is-ancestor", text)
        self.assertIn("--channel candidate", text)
        self.assertNotIn("stamp-version", text)
        self.assertNotIn("clear-unreleased", text)
        self.assertNotIn("git push", text)
        self.assertNotIn("models: read", text)


if __name__ == "__main__":
    unittest.main()
