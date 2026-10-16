from __future__ import annotations

import shutil
import io
import hashlib
import json
import plistlib
import struct
import sys
import tempfile
import unittest
import zipfile
from pathlib import Path
from types import SimpleNamespace
from unittest import mock

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))

from scripts import release
from scripts import fetch_steam_provider
from scripts import root_archive
from scripts import stock_ql_maps
from scripts import verify_release_layout
from scripts import windows_pe


def make_pe(*, dll: bool, imports: tuple[str, ...] = ()) -> bytes:
    binary = bytearray(0x600)
    pe_offset = 0x80
    optional_offset = pe_offset + 24
    optional_size = 224
    section_offset = optional_offset + optional_size
    raw_offset = 0x200
    import_rva = 0x1000 if imports else 0
    import_size = (len(imports) + 1) * 20 if imports else 0

    binary[0:2] = b"MZ"
    struct.pack_into("<I", binary, 60, pe_offset)
    binary[pe_offset : pe_offset + 4] = b"PE\0\0"
    struct.pack_into(
        "<HHIIIHH",
        binary,
        pe_offset + 4,
        windows_pe.PE_I386,
        1,
        0,
        0,
        0,
        optional_size,
        0x2102 if dll else 0x0102,
    )
    struct.pack_into("<H", binary, optional_offset, 0x10B)
    struct.pack_into("<I", binary, optional_offset + 92, 16)
    struct.pack_into("<II", binary, optional_offset + 104, import_rva, import_size)
    binary[section_offset : section_offset + 8] = b".rdata\0\0"
    struct.pack_into("<IIII", binary, section_offset + 8, 0x400, 0x1000, 0x400, raw_offset)

    string_offset = raw_offset + 0x200
    for index, dependency in enumerate(imports):
        encoded = dependency.encode("ascii") + b"\0"
        name_rva = 0x1000 + (string_offset - raw_offset)
        struct.pack_into("<IIIII", binary, raw_offset + index * 20, 0, 0, 0, name_rva, 0)
        binary[string_offset : string_offset + len(encoded)] = encoded
        string_offset += len(encoded)
    return bytes(binary)


def make_elf_i386() -> bytes:
    binary = bytearray(64)
    binary[0:4] = b"\x7fELF"
    binary[4] = 1
    binary[5] = 1
    binary[6] = 1
    struct.pack_into("<H", binary, 16, 3)
    struct.pack_into("<H", binary, 18, 3)
    return bytes(binary)


def make_macho64(cpu_type: int) -> bytes:
    return b"\xcf\xfa\xed\xfe" + cpu_type.to_bytes(4, "little") + bytes(56)


def make_universal2_macho() -> bytes:
    header = bytearray(b"\xca\xfe\xba\xbe" + (2).to_bytes(4, "big"))
    for cpu_type in (release.MACOS_X86_64_CPU, release.MACOS_ARM64_CPU):
        header.extend(cpu_type.to_bytes(4, "big"))
        header.extend(bytes(16))
    return bytes(header)


def make_macos_release_members(
    *,
    minimum_version: str = release.MACOS_MINIMUM_VERSION,
    short_version: str = "0.1.0",
    bundle_version: str = "1",
    renderer_parent: str = f"{release.MACOS_APP_ROOT}/MacOS",
    include_signature: bool = True,
    extra_names: tuple[str, ...] = (),
) -> list[release.ReleaseArchiveMember]:
    macho = make_macho64(release.MACOS_X86_64_CPU)
    info = plistlib.dumps(
        {
            "CFBundleExecutable": "FnQL",
            "CFBundleIdentifier": "org.fnql.fnql",
            "CFBundlePackageType": "APPL",
            "CFBundleShortVersionString": short_version,
            "CFBundleVersion": bundle_version,
            "LSMinimumSystemVersion": minimum_version,
            "NSHighResolutionCapable": True,
            "NSMicrophoneUsageDescription": "Optional in-game voice capture.",
        }
    )
    member = release.ReleaseArchiveMember
    members = [
        member("FnQL.app/Contents/Info.plist", info, 0o644),
        member("FnQL.app/Contents/MacOS/FnQL", macho, 0o755),
        member("FnQL.app/Contents/MacOS/fntech3-pkg.fnz", b"package", 0o644),
        member("FnQL.app/Contents/MacOS/fnql-web.pak", b"web", 0o644),
        member("fntech3.ded", macho, 0o755),
        member("fnql-audiozonesc", macho, 0o755),
        *(
            member(f"{renderer_parent}/fnql_{renderer}_x86_64.dylib", macho, 0o755)
            for renderer in release.PUBLIC_RENDERERS
        ),
        *(member(name, macho, 0o755) for name in extra_names),
    ]
    if include_signature:
        members.append(
            member("FnQL.app/Contents/_CodeSignature/CodeResources", b"signature", 0o644)
        )
    return members


def populate_linux_release_stage(stage_root: Path, *, include_server: bool = True) -> None:
    elf = make_elf_i386()
    (stage_root / "fntech3").write_bytes(elf)
    if include_server:
        (stage_root / "fntech3.ded").write_bytes(elf)
    for renderer in release.PUBLIC_RENDERERS:
        (stage_root / f"fnql_{renderer}_x86.so").write_bytes(elf)
    release.copy_docs(stage_root)
    release.build_fnql_webpak(stage_root)
    release.build_root_archive(stage_root)


def populate_macos_release_stage(stage_root: Path) -> None:
    macos = stage_root / "FnQL.app" / "Contents" / "MacOS"
    macos.mkdir(parents=True)
    macho = make_macho64(release.MACOS_X86_64_CPU)
    (macos / "FnQL").write_bytes(macho)
    (stage_root / "fntech3.ded").write_bytes(macho)
    (stage_root / "fnql-audiozonesc").write_bytes(macho)
    for renderer in release.PUBLIC_RENDERERS:
        (macos / f"fnql_{renderer}_x86_64.dylib").write_bytes(macho)
    info = {
        "CFBundleExecutable": "FnQL",
        "CFBundleIdentifier": "org.fnql.fnql",
        "CFBundlePackageType": "APPL",
        "CFBundleShortVersionString": "0.1.0",
        "CFBundleVersion": "1",
        "LSMinimumSystemVersion": release.MACOS_MINIMUM_VERSION,
        "NSHighResolutionCapable": True,
        "NSMicrophoneUsageDescription": "Optional in-game voice capture.",
    }
    (stage_root / "FnQL.app" / "Contents" / "Info.plist").write_bytes(
        plistlib.dumps(info)
    )
    signature = stage_root / "FnQL.app" / "Contents" / "_CodeSignature"
    signature.mkdir()
    (signature / "CodeResources").write_bytes(b"test signature envelope")
    release.copy_docs(stage_root)
    webpak = release.build_fnql_webpak(stage_root)
    package = release.build_root_archive(stage_root)
    (macos / release.FNQL_WEBPAK_NAME).write_bytes(webpak.read_bytes())
    (macos / release.ROOT_ARCHIVE_NAME).write_bytes(package.read_bytes())


class ReleasePackagingTests(unittest.TestCase):
    def test_candidate_packaging_produces_a_complete_verifiable_release_bundle(self) -> None:
        from scripts import release_workflow
        from scripts.fntech3_meta import base_metadata

        scratch = ROOT / ".tmp" / "release-packaging-tests"
        scratch.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=scratch) as temporary:
            root = Path(temporary)
            artifacts = root / "artifacts"
            for variant in release.PUBLISHED_RELEASE_ARTIFACTS:
                directory = artifacts / variant
                directory.mkdir(parents=True)
                if variant.startswith("linux"):
                    for name in (*release.LINUX_RELEASE_EXECUTABLES, *release.LINUX_RELEASE_RENDERER_MODULES):
                        (directory / name).write_bytes(make_elf_i386())
                else:
                    for name in ("fntech3.exe", "fntech3.ded.exe", "fnql_glx_x86.dll", "fnql_vk_x86.dll", "fnql_rtx_x86.dll"):
                        (directory / name).write_bytes(make_pe(dll=name.endswith(".dll")))
            version = str(base_metadata()["version"])
            source = release.resolve_proof_source_commit(None)
            args = SimpleNamespace(channel="candidate", artifact_root=artifacts,
                                   output_dir=root / "output", temp_dir=root / "staging",
                                   build_number=None, build_date="2026-09-01", commit=source,
                                   ref_name=f"v{version}", glx_proof_root=None,
                                   glx_rollback_metadata=None, require_glx_proof=False)
            manifest = release.build_archives(args)
            self.assertEqual(manifest["glx_runtime_proof"]["status"], "not-established")
            self.assertEqual(manifest["source_commit"], source)
            self.assertFalse(manifest["prerelease"])
            info = {"version": version, "release_tag": f"v{version}",
                    "head_commit": source, "build_date": "2026-09-01"}
            files = release_workflow.release_files(args.output_dir / "release-manifest.json", info)
            self.assertEqual(len(files), 5)

    def test_copy_release_artifact_contents_filters_build_garbage(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            source = root / "artifact"
            target = root / "stage"
            source.mkdir()
            (source / "fntech3").write_text("binary", encoding="utf-8")
            (source / "README.txt").write_text("keep", encoding="utf-8")
            (source / "baseq3" / "maps").mkdir(parents=True)
            (source / "baseq3" / "maps" / "q3dm1.azb").write_bytes(b"zones")
            (source / "cpma" / "vm").mkdir(parents=True)
            (source / "cpma" / "vm" / "cgame.qvm").write_text(
                "mod data",
                encoding="utf-8",
            )
            (source / "renderer.pdb").write_text("debug", encoding="utf-8")
            (source / ".DS_Store").write_text("finder", encoding="utf-8")
            (source / "meson-info").mkdir()
            (source / "meson-info" / "intro-targets.json").write_text("{}", encoding="utf-8")
            (source / "__pycache__").mkdir()
            (source / "__pycache__" / "junk.pyc").write_bytes(b"\0")
            (source / "FnQL.dSYM" / "Contents").mkdir(parents=True)
            (source / "FnQL.dSYM" / "Contents" / "Info.plist").write_text("debug", encoding="utf-8")

            skipped = release.copy_release_artifact_contents(source, target)

            kept = sorted(path.relative_to(target).as_posix() for path in target.rglob("*") if path.is_file())

        self.assertEqual(
            kept,
            [
                "README.txt",
                "baseq3/maps/q3dm1.azb",
                "cpma/vm/cgame.qvm",
                "fntech3",
            ],
        )
        self.assertIn("renderer.pdb", skipped)
        self.assertIn(".DS_Store", skipped)
        self.assertIn("meson-info", skipped)
        self.assertIn("__pycache__", skipped)
        self.assertIn("FnQL.dSYM", skipped)

    def test_packaged_evidence_links_and_missing_target(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            stage = Path(temporary)
            release.copy_docs(stage)
            files = {p.relative_to(stage).as_posix(): p.read_bytes()
                     for p in stage.rglob("*") if p.is_file()}
            release.validate_evidence_document_links(files)
            target = "docs/fntech3/2026-09-08-http-errors-and-curl-cleanup.md"
            self.assertIn(target, files)
            del files[target]
            with self.assertRaisesRegex(ValueError, "Missing packaged evidence document"):
                release.validate_evidence_document_links(files)
            self.assertFalse(any(name.startswith(".tmp/") for name in files))

    def test_evidence_link_fragments_and_external_references(self) -> None:
        files = {"docs/fntech3/ledger.md":
                 b"[local](evidence/probe.json#result) [site](https://example.com/a.md) "
                 b"[private](../../.tmp/probe.json) [anchor](#section)",
                 "docs/fntech3/evidence/probe.json": b"{}"}
        release.validate_evidence_document_links(files)
        del files["docs/fntech3/evidence/probe.json"]
        with self.assertRaisesRegex(ValueError, "Missing packaged evidence document"):
            release.validate_evidence_document_links(files)

    def test_packaged_docs_are_minimal_player_archive_docs(self) -> None:
        destinations = {destination.as_posix() for _source, destination in release.DEFAULT_DOCS}

        self.assertIn("LICENSE", destinations)
        self.assertIn("CREDITS.md", destinations)
        self.assertIn("THIRD_PARTY_NOTICES.md", destinations)
        self.assertIn("docs/fntech3/COMPATIBILITY.md", destinations)
        self.assertIn("README.html", destinations)
        self.assertIn("docs/fnql/TECHNICAL.md", destinations)
        self.assertIn("docs/fnql/RTX_RENDERER.md", destinations)
        self.assertIn("docs/fnql/STEAM_PROVIDER_BINARY_NOTICE.txt", destinations)
        self.assertIn("docs/GLX.md", destinations)
        self.assertIn("docs/RTX.md", destinations)
        self.assertNotIn("docs/fnql/GLX_PROMOTION.md", destinations)
        self.assertNotIn("docs/fnql/GLX_ROLLBACK_PACKAGE.md", destinations)
        self.assertNotIn("docs/fnql/GLX_VISUAL_DOSSIER.md", destinations)

    def test_linux_release_tarball_is_deterministic_and_executable(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            stage_root = root / "stage"
            stage_root.mkdir()
            populate_linux_release_stage(stage_root)
            first = root / "fnql-linux-x86.tar.gz"
            second = root / "fnql-linux-x86-copy.tar.gz"

            release.write_release_archive(stage_root, first)
            for path in stage_root.rglob("*"):
                if path.is_file():
                    path.touch()
            release.write_release_archive(stage_root, second)

            self.assertEqual(first.read_bytes(), second.read_bytes())
            release.validate_release_archive_contents(first)
            verify_release_layout.verify_release_layout(first)
            members = {member.name: member for member in release.read_release_archive(first)}

        self.assertEqual(members["fntech3"].mode, 0o755)
        self.assertEqual(members["fntech3.ded"].mode, 0o755)
        self.assertEqual(members[release.FNQL_WEBPAK_NAME].mode, 0o644)
        for renderer in release.PUBLIC_RENDERERS:
            self.assertEqual(members[f"fnql_{renderer}_x86.so"].mode, 0o755)

    def test_linux_release_tarball_requires_client_server_and_renderers(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            stage_root = root / "stage"
            stage_root.mkdir()
            populate_linux_release_stage(stage_root, include_server=False)
            archive_path = root / "fnql-linux-x86.tar.gz"
            release.write_release_archive(stage_root, archive_path)

            with self.assertRaisesRegex(ValueError, "missing required Linux executables"):
                release.validate_release_archive_contents(archive_path)

    def test_linux_release_tarball_accepts_exact_root_renderer_names(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            stage_root = root / "stage"
            stage_root.mkdir()
            populate_linux_release_stage(stage_root)
            archive_path = root / "fnql-linux-x86.tar.gz"

            release.write_release_archive(stage_root, archive_path)
            release.validate_release_archive_contents(archive_path)

            archived_names = {
                member.name for member in release.read_release_archive(archive_path)
            }

        self.assertTrue(
            set(release.LINUX_RELEASE_RENDERER_MODULES).issubset(archived_names)
        )

    def test_release_proof_runtime_identities_come_from_exact_archive_members(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            stage_root = root / "stage"
            stage_root.mkdir()
            populate_linux_release_stage(stage_root)
            archive_path = root / "fnql-linux-x86.tar.gz"
            release.write_release_archive(stage_root, archive_path)

            identities = release.release_archive_runtime_identities(
                archive_path,
                "linux-x86",
            )

            self.assertEqual(identities["client"]["member"], "fntech3")
            self.assertEqual(identities["glxModule"]["member"], "fnql_glx_x86.so")
            self.assertEqual(
                identities["client"]["sha256"],
                hashlib.sha256((stage_root / "fntech3").read_bytes()).hexdigest(),
            )
            self.assertEqual(
                identities["glxModule"]["sha256"],
                hashlib.sha256((stage_root / "fnql_glx_x86.so").read_bytes()).hexdigest(),
            )

            windows_stage = root / "windows-stage"
            windows_stage.mkdir()
            windows_client = make_pe(dll=False)
            windows_glx = make_pe(dll=True)
            (windows_stage / "fntech3.exe").write_bytes(windows_client)
            (windows_stage / "fnql_glx_x86.dll").write_bytes(windows_glx)
            windows_archive = root / "fnql-windows-x86.zip"
            release.write_release_archive(windows_stage, windows_archive)

            for variant in ("windows-mingw-x86", "windows-msvc-x86"):
                windows_identities = release.release_archive_runtime_identities(
                    windows_archive,
                    variant,
                )
                self.assertEqual(windows_identities["client"]["member"], "fntech3.exe")
                self.assertEqual(
                    windows_identities["glxModule"]["member"],
                    "fnql_glx_x86.dll",
                )
                self.assertEqual(
                    windows_identities["client"]["sha256"],
                    hashlib.sha256(windows_client).hexdigest(),
                )
                self.assertEqual(
                    windows_identities["glxModule"]["sha256"],
                    hashlib.sha256(windows_glx).hexdigest(),
                )

    def test_linux_release_tarball_rejects_nested_renderer_modules(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            stage_root = root / "stage"
            stage_root.mkdir()
            populate_linux_release_stage(stage_root)
            nested = stage_root / "nested"
            nested.mkdir()
            for name in release.LINUX_RELEASE_RENDERER_MODULES:
                (stage_root / name).rename(nested / name)
            archive_path = root / "fnql-linux-x86.tar.gz"

            release.write_release_archive(stage_root, archive_path)

            with self.assertRaisesRegex(
                ValueError,
                "missing required Linux renderer modules",
            ):
                release.validate_release_archive_contents(archive_path)

    def test_linux_release_tarball_rejects_misnamed_renderer_modules(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            stage_root = root / "stage"
            stage_root.mkdir()
            populate_linux_release_stage(stage_root)
            for name in release.LINUX_RELEASE_RENDERER_MODULES:
                source = stage_root / name
                source.rename(stage_root / name.replace("_x86.so", "_i386.so"))
            archive_path = root / "fnql-linux-x86.tar.gz"

            release.write_release_archive(stage_root, archive_path)

            with self.assertRaisesRegex(
                ValueError,
                "missing required Linux renderer modules",
            ):
                release.validate_release_archive_contents(archive_path)

    def test_release_zip_writer_normalizes_metadata(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            stage_root = root / "stage"
            stage_root.mkdir()
            (stage_root / "fntech3.exe").write_bytes(b"not-a-real-binary")
            first = root / "fnql-windows-x86.zip"
            second = root / "fnql-windows-x86-copy.zip"

            release.write_release_archive(stage_root, first)
            (stage_root / "fntech3.exe").touch()
            release.write_release_archive(stage_root, second)

            self.assertEqual(first.read_bytes(), second.read_bytes())
            with zipfile.ZipFile(first) as archive:
                info = archive.getinfo("fntech3.exe")
                self.assertEqual(info.date_time, release.RELEASE_ZIP_DATETIME)
                self.assertEqual((info.external_attr >> 16) & 0o7777, 0o644)

    def test_stock_ql_sidecars_are_packaged_in_root_archive(self) -> None:
        audio_destinations = {
            destination.as_posix()
            for _source, destination in root_archive.DEFAULT_AUDIO_ZONE_ASSETS
            if destination.parts[1] == "quakelive"
        }
        fog_destinations = {
            destination.as_posix()
            for _source, destination in root_archive.DEFAULT_GLOBAL_FOG_ASSETS
            if destination.parts[1] == "quakelive"
        }
        required_destinations = {
            destination.as_posix()
            for _source, destination in root_archive.DEFAULT_ROOT_ARCHIVE_REQUIRED_ASSETS
        }
        sources = {
            source.relative_to(ROOT).as_posix()
            for source, _destination in root_archive.DEFAULT_AUDIO_ZONE_ASSETS
        }

        self.assertEqual(len(stock_ql_maps.STOCK_QL_MAPS), 149)
        self.assertEqual(len(audio_destinations), 149)
        self.assertEqual(len(fog_destinations), 149)
        # 441 before Enemy Territory added a zone file for each of its six maps,
        # 447 before Alice added a zone and a static light file for its 36,
        # 519 before Heavy Metal: F.A.K.K.2 added the same for its 30,
        # 579 before RTCW added a static light file for each of its 47,
        # 626 before F.A.K.K.2 added its HUD rules, 627 before Medal of
        # Honor: Allied Assault added a zone and a static light file for its 54,
        # 735 before Alice, Enemy Territory and both Elite Force modes added
        # their HUD rules, 739 before Elite Force added a zone file for
        # each of its 99 maps, 838 before Jedi Academy multiplayer added its
        # HUD rules, and 839 before Jedi Outcast multiplayer added its own.
        self.assertEqual(len(required_destinations), 840)
        self.assertIn("pkg/games/quakelive/baseq3/maps/campgrounds.azb", sources)
        self.assertIn("games/quakelive/baseq3/maps/campgrounds.azb", audio_destinations)
        self.assertIn("games/quakelive/baseq3/maps/campgrounds.fog", fog_destinations)
        self.assertIn("games/quakelive/baseq3/maps/bloodrun.azb", audio_destinations)
        self.assertIn("games/quakelive/baseq3/maps/siberia.fog", fog_destinations)
        self.assertNotIn("games/quakelive/baseq3/maps/q3dm1.azb", audio_destinations)
        self.assertIn("games/quakelive/baseq3/sound/fnql-weapon-sounds.sndshd", required_destinations)
        self.assertIn("games/quakelive/baseq3/scripts/fnql.shader", required_destinations)
        self.assertIn("games/quake3/baseq3/sound/fnql-weapon-sounds.sndshd", required_destinations)
        self.assertNotIn("games/quakelive/baseq3/maps/test_bigbox.azb", audio_destinations)

        with tempfile.TemporaryDirectory() as tmp:
            archive_path = Path(tmp) / release.ROOT_ARCHIVE_NAME

            release.write_root_archive(archive_path)
            release.validate_root_archive(archive_path)

            with zipfile.ZipFile(archive_path) as archive:
                packaged = set(archive.namelist())

        expected_packaged = {
            destination.as_posix()
            for _source, destination in root_archive.iter_package_assets()
        }
        self.assertTrue(required_destinations.issubset(packaged))
        self.assertEqual(packaged, expected_packaged)
        self.assertEqual(
            len({name for name in packaged if name.startswith("games/quake3/")}),
            92,
        )
        self.assertIn("games/quake3/baseq3/maps/q3dm1.azb", packaged)
        self.assertIn("games/quake3/missionpack/maps/mpteam1.fog", packaged)
        self.assertIn("games/quake3/missionpack/sound/fnql-weapon-sounds.sndshd", packaged)
        for profile, names in root_archive.STOCK_RTCW_MAPS.items():
            self.assertEqual(
                {name for name in packaged if name.startswith(f"games/{profile}/")},
                {f"games/{profile}/main/maps/{name}{suffix}" for name in names
                 for suffix in (".azb", ".lights.json")}
                | {f"games/{profile}/main/fntech3-hud.json"},
            )
        # Enemy Territory ships a zone file for each of its six official maps,
        # and its HUD rules.
        self.assertEqual(
            {name for name in packaged if name.startswith("games/wolfet/")},
            {f"games/wolfet/etmain/maps/{name}.azb" for name in root_archive.STOCK_WOLFET_MAPS}
            | {"games/wolfet/etmain/fntech3-hud.json"},
        )
        # Elite Force ships each mode's HUD rules and a zone file for each of
        # its maps, single player's ten tour maps in their own folder.
        for mode, names in root_archive.STOCK_STVEF_MAPS.items():
            self.assertEqual({name for name in packaged if name.startswith(f"games/{mode}/")},
                             {f"games/{mode}/baseEF/maps/{name}.azb" for name in names}
                             | {f"games/{mode}/baseEF/fntech3-hud.json"})
        self.assertEqual({mode: len(names) for mode, names in root_archive.STOCK_STVEF_MAPS.items()},
                         {"stvef-sp": 66, "stvef-mp": 33})
        self.assertEqual(len(root_archive.STOCK_WOLFET_MAPS), 6)
        # Alice ships a zone file and a static light file for each of its 36 maps.
        self.assertEqual(
            {name for name in packaged if name.startswith("games/alice/")},
            {f"games/alice/base/maps/{name}{suffix}" for name in root_archive.STOCK_ALICE_MAPS
             for suffix in (".azb", ".lights.json")}
            | {"games/alice/base/fntech3-hud.json"},
        )
        self.assertEqual(len(root_archive.STOCK_ALICE_MAPS), 36)
        # Heavy Metal: F.A.K.K.2 ships the same two files for each of its 30,
        # and the HUD rules that place its HUD menus.
        self.assertEqual(
            {name for name in packaged if name.startswith("games/fakk2/")},
            {f"games/fakk2/fakk/maps/{name}{suffix}" for name in root_archive.STOCK_FAKK2_MAPS
             for suffix in (".azb", ".lights.json")}
            | {"games/fakk2/fakk/fntech3-hud.json"},
        )
        self.assertEqual(len(root_archive.STOCK_FAKK2_MAPS), 30)
        # Medal of Honor ships the same two files for each of its 54, beside
        # each map in the folder the pak keeps it in.
        self.assertEqual(
            {name for name in packaged if name.startswith("games/mohaa/")},
            {f"games/mohaa/main/maps/{name}{suffix}" for name in root_archive.STOCK_MOHAA_MAPS
             for suffix in (".azb", ".lights.json")},
        )
        self.assertEqual(len(root_archive.STOCK_MOHAA_MAPS), 54)
        self.assertIn("games/mohaa/main/maps/DM/mohdm1.azb", packaged)
        self.assertIn("games/mohaa/main/maps/obj/obj_team2.lights.json", packaged)
        self.assertEqual(len(root_archive.DEFAULT_STATIC_LIGHT_ASSETS), 167)
        self.assertNotIn("games/quakelive/baseq3/fnql-hud.json", packaged)

    def test_meson_root_archive_inputs_include_non_map_pkg_assets(self) -> None:
        meson_build = (ROOT / "meson.build").read_text(encoding="utf-8")

        self.assertNotIn("'pkg/games/quakelive/baseq3/fnql-hud.json'", meson_build)
        self.assertIn("files('scripts/stock_ql_maps.py')", meson_build)
        self.assertIn("map_name + '.azb'", meson_build)
        self.assertIn("map_name + '.fog'", meson_build)
        self.assertIn("'pkg/games/quakelive/baseq3/scripts/fnql.shader'", meson_build)
        self.assertIn("'pkg/games/quakelive/baseq3/sound/fnql-weapon-sounds.sndshd'", meson_build)
        self.assertIn("files('scripts/stock_q3_assets.py')", meson_build)
        self.assertIn("files('pkg/games/quake3/' + asset)", meson_build)
        self.assertIn("files('scripts/stock_rtcw_maps.py')", meson_build)
        self.assertIn("files('scripts/stock_wolfet_maps.py')", meson_build)
        self.assertIn("'scripts/stock_wolfet_maps.py',", meson_build)
        self.assertIn("files('scripts/stock_alice_maps.py')", meson_build)
        self.assertIn("'scripts/stock_alice_maps.py',", meson_build)
        self.assertIn("files('scripts/stock_fakk2_maps.py')", meson_build)
        self.assertIn("'scripts/stock_fakk2_maps.py',", meson_build)
        self.assertIn("files('scripts/stock_mohaa_maps.py')", meson_build)
        self.assertIn("'scripts/stock_mohaa_maps.py',", meson_build)
        for profile in ("rtcw-sp", "rtcw-mp"):
            self.assertIn(f"'pkg/games/{profile}/main/fntech3-hud.json'", meson_build)
        for preset in ("fakk2/fakk", "alice/base", "wolfet/etmain", "stvef-sp/baseEF", "stvef-mp/baseEF",
                       "jka-mp/base", "jk2-mp/base"):
            self.assertIn(f"'pkg/games/{preset}/fntech3-hud.json'", meson_build)
        self.assertNotIn("pkg/missionpack/", meson_build)

    def test_root_archive_rejects_unsafe_custom_archive_names(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            package_root = root / "pkg"
            package_root.mkdir()
            source = package_root / "asset.txt"
            source.write_text("asset", encoding="utf-8")

            with self.assertRaisesRegex(ValueError, "unsafe path component"):
                root_archive.write_root_archive(
                    root / release.ROOT_ARCHIVE_NAME,
                    package_root=package_root,
                    assets=[(source, Path("..") / "asset.txt")],
                )

        unsafe_names = (
            "games/quakelive/baseq3/maps/bad:name.azb",
            "games/quakelive/baseq3/maps/bad\nname.azb",
        )
        for name in unsafe_names:
            with self.subTest(name=name):
                with self.assertRaisesRegex(ValueError, "unsafe|stream"):
                    root_archive.validate_archive_member_names([name], archive_name="pkg.fnz")

        with self.assertRaisesRegex(ValueError, "duplicate"):
            root_archive.validate_archive_member_names(
                ["games/quakelive/baseq3/maps/q3dm1.azb", "games/quakelive/baseq3/maps/Q3DM1.azb"],
                archive_name="pkg.fnz",
            )

    def test_root_archive_rejects_custom_sources_outside_package_root(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            package_root = root / "pkg"
            package_root.mkdir()
            outside = root / "outside.txt"
            outside.write_text("do not package", encoding="utf-8")

            with self.assertRaisesRegex(ValueError, "escapes package root"):
                root_archive.write_root_archive(
                    root / release.ROOT_ARCHIVE_NAME,
                    package_root=package_root,
                    assets=[(outside, Path("baseq3") / "outside.txt")],
                )

    def test_root_archive_packs_all_profiles_and_mods_deterministically(self) -> None:
        payloads = {
            "games/quake3/baseq3/maps/shared.azb": b"q3",
            "games/quakelive/baseq3/maps/shared.azb": b"ql",
            "games/rtcw-sp/main/maps/shared.azb": b"sp",
            "games/rtcw-mp/main/maps/shared.azb": b"mp",
            "games/quake3/cpma/sound/custom.sndshd": b"mod",
        }
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            package_root = root / "pkg"
            for name, payload in payloads.items():
                source = package_root / name
                source.parent.mkdir(parents=True, exist_ok=True)
                source.write_bytes(payload)
            first, second = root / "first.fnz", root / "second.fnz"
            root_archive.write_root_archive(first, package_root=package_root)
            root_archive.write_root_archive(second, package_root=package_root)
            self.assertEqual(first.read_bytes(), second.read_bytes())
            with zipfile.ZipFile(first) as archive:
                self.assertEqual({name: archive.read(name) for name in archive.namelist()}, payloads)
                for info in archive.infolist():
                    self.assertEqual(info.create_system, 3)
                    self.assertEqual(info.date_time, (1980, 1, 1, 0, 0, 0))
                    self.assertEqual(info.external_attr >> 16, 0o644)

    def test_root_archive_rejects_unscoped_alias_unknown_and_overlong_names(self) -> None:
        for name in (
            "baseq3/maps/shared.azb", "maps/shared.azb",
            "games/q3/baseq3/maps/shared.azb", "games/ql/baseq3/maps/shared.azb",
            "games/rtcw/main/maps/shared.azb", "games/unknown/main/maps/shared.azb",
            "games/quakelive/baseq3/../../quake3/baseq3/maps/shared.azb",
            "games/quakelive/baseq3/maps/" + "a" * 256 + ".azb",
        ):
            with self.subTest(name=name), self.assertRaises(ValueError):
                root_archive.validate_root_archive_names([name], assets=[])

    def test_invalid_package_does_not_replace_previous_archive(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            source = root / "pkg/games/quakelive/baseq3/maps/shared.azb"
            source.parent.mkdir(parents=True)
            source.write_bytes(b"zone")
            output = root / root_archive.ROOT_ARCHIVE_NAME
            output.write_bytes(b"previous package")
            with self.assertRaisesRegex(ValueError, "missing required"):
                root_archive.write_root_archive(
                    output, package_root=root / "pkg",
                    required_assets=root_archive.DEFAULT_ROOT_ARCHIVE_REQUIRED_ASSETS,
                )
            self.assertEqual(output.read_bytes(), b"previous package")
            self.assertFalse(output.with_name(output.name + ".tmp").exists())
            with self.assertRaisesRegex(ValueError, "canonical game profile"):
                root_archive.write_root_archive(
                    output, package_root=root / "pkg", assets=[(source, Path("baseq3/maps/shared.azb"))],
                )
            self.assertEqual(output.read_bytes(), b"previous package")

    def test_release_layout_verifier_requires_root_package_archive(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)

            release.copy_docs(root)
            release.build_fnql_webpak(root)
            archive_path = release.build_root_archive(root)

            verify_release_layout.verify_release_layout(root)

            archive_path.unlink()
            with self.assertRaises(FileNotFoundError):
                verify_release_layout.verify_release_layout(root)

    def test_release_layout_verifier_requires_package_docs_for_directories(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)

            release.build_fnql_webpak(root)
            release.build_root_archive(root)

            with self.assertRaisesRegex(FileNotFoundError, "missing required release files"):
                verify_release_layout.verify_release_layout(root)

    def test_release_archive_keeps_game_dirs_at_archive_root(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            stage_root = root / "stage"
            stage_root.mkdir()
            (stage_root / "fnql.x86.exe").write_text("binary", encoding="utf-8")
            (stage_root / "cpma" / "vm").mkdir(parents=True)
            (stage_root / "cpma" / "vm" / "cgame.qvm").write_text(
                "mod data",
                encoding="utf-8",
            )
            release.copy_docs(stage_root)
            release.build_fnql_webpak(stage_root)
            release.build_root_archive(stage_root)
            archive_path = Path(
                shutil.make_archive(str(root / "fnql-root"), "zip", root_dir=stage_root)
            )

            release.validate_release_archive_contents(archive_path)
            with zipfile.ZipFile(archive_path) as archive:
                names = set(archive.namelist())
                root_archive_bytes = archive.read(release.ROOT_ARCHIVE_NAME)
            with zipfile.ZipFile(io.BytesIO(root_archive_bytes)) as root_archive:
                root_archive_names = set(root_archive.namelist())

        self.assertIn(release.ROOT_ARCHIVE_NAME, names)
        self.assertIn(release.FNQL_WEBPAK_NAME, names)
        self.assertIn("cpma/vm/cgame.qvm", names)
        self.assertNotIn("games/quakelive/baseq3/maps/q3dm1.azb", names)
        self.assertIn("games/quakelive/baseq3/maps/campgrounds.azb", root_archive_names)
        self.assertIn("games/quakelive/baseq3/maps/campgrounds.fog", root_archive_names)
        self.assertIn("games/quakelive/baseq3/maps/bloodrun.azb", root_archive_names)
        self.assertIn("games/quakelive/baseq3/maps/bloodrun.fog", root_archive_names)
        self.assertNotIn("games/quakelive/baseq3/maps/q3dm1.azb", root_archive_names)
        self.assertIn("games/quakelive/baseq3/sound/fnql-weapon-sounds.sndshd", root_archive_names)
        self.assertNotIn("maps/q3dm1.azb", names)

    def test_release_archive_validation_requires_package_docs(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            stage_root = root / "stage"
            stage_root.mkdir()
            (stage_root / "fnql.x86.exe").write_text("binary", encoding="utf-8")
            release.build_fnql_webpak(stage_root)
            release.build_root_archive(stage_root)
            archive_path = Path(
                shutil.make_archive(str(root / "fnql-missing-docs"), "zip", root_dir=stage_root)
            )

            with self.assertRaisesRegex(ValueError, "missing required release files"):
                release.validate_release_archive_contents(archive_path)

    def test_release_layout_rejects_renderer_modules_outside_public_three(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / "fnql.x86.exe").write_text("binary", encoding="utf-8")
            (root / "fnql_opengl_x86.dll").write_text(
                "old renderer", encoding="utf-8"
            )

            with self.assertRaisesRegex(ValueError, "only glx, vk, and rtx"):
                release.validate_stage_tree(root)

    def test_release_layout_accepts_the_public_renderer_modules(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / "fnql.x86.exe").write_text("binary", encoding="utf-8")
            for renderer in ("glx", "vk", "rtx"):
                (root / f"fnql_{renderer}_x86.dll").write_text(
                    renderer, encoding="utf-8"
                )

            release.validate_stage_tree(root)

    def test_windows_release_accepts_engine_without_provider(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / "fntech3.exe").write_text("binary", encoding="utf-8")

            release.validate_stage_tree(root)

    def test_engine_only_policy_rejects_nested_proprietary_libraries(self) -> None:
        for filename in ("fnql_steam.dll", "FNQL_STEAM.DLL", "steam_api.dll",
                         "steam_api64.dll", "libsteam_api.so", "libsteam_api.dylib",
                         "fnql_steam.so", "libfnql_steam.dylib"):
            with self.subTest(filename=filename), tempfile.TemporaryDirectory() as tmp:
                root = Path(tmp)
                nested = root / "plugins"
                nested.mkdir()
                (nested / filename).write_bytes(b"fixture")
                with self.assertRaisesRegex(ValueError, "must not redistribute"):
                    release.validate_stage_tree(root)
                release.copy_docs(root)
                release.build_fnql_webpak(root)
                release.build_root_archive(root)
                archive = Path(shutil.make_archive(str(root.parent / (root.name + "-probe")), "zip", root_dir=root))
                try:
                    with self.assertRaisesRegex(ValueError, "must not redistribute"):
                        release.validate_release_archive_contents(archive)
                finally:
                    archive.unlink()

    def test_windows_release_forbids_valve_redistributable(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / "fntech3.exe").write_text("binary", encoding="utf-8")
            (root / "steam_api.dll").write_bytes(make_pe(dll=True))

            with self.assertRaisesRegex(ValueError, "must not redistribute"):
                release.validate_stage_tree(root)

    def test_windows_release_rejects_unshipped_runtime_imports(self) -> None:
        for runtime in ("libgcc_s_dw2-1.dll", "z-1.dll"):
            with self.subTest(runtime=runtime), tempfile.TemporaryDirectory() as tmp:
                root = Path(tmp)
                (root / "fntech3.exe").write_bytes(
                    make_pe(dll=False, imports=("KERNEL32.dll", runtime))
                )

                with self.assertRaisesRegex(ValueError, "unshipped Windows runtime DLLs"):
                    release.validate_stage_tree(root)

    def test_pinned_provider_stager_checks_digest_and_i386_dll_identity(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            source = root / "provider.dll"
            output = root / "out" / "fnql_steam.dll"
            manifest = root / "provider.json"
            provider = make_pe(dll=True, imports=("KERNEL32.dll",))
            source.write_bytes(provider)
            manifest.write_text(
                json.dumps(
                    {
                        "version": "test",
                        "tag": "test",
                        "asset": "fnql_steam.dll",
                        "url": "https://invalid.example/fnql_steam.dll",
                        "sha256": hashlib.sha256(provider).hexdigest(),
                        "pe_machine": "i386",
                    }
                ),
                encoding="utf-8",
            )

            digest = fetch_steam_provider.stage_provider(
                output, manifest_path=manifest, source=source
            )

            self.assertEqual(digest, hashlib.sha256(provider).hexdigest())
            self.assertEqual(output.read_bytes(), provider)

    def test_copy_release_artifact_contents_rejects_symlinks(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            source = root / "artifact"
            target = root / "stage"
            source.mkdir()
            outside = root / "outside.txt"
            outside.write_text("outside", encoding="utf-8")
            link = source / "outside-link.txt"
            try:
                link.symlink_to(outside)
            except (NotImplementedError, OSError) as exc:
                self.skipTest(f"symlink creation is unavailable: {exc}")

            with self.assertRaisesRegex(ValueError, "symbolic link"):
                release.copy_release_artifact_contents(source, target)

    def test_copy_release_artifact_contents_rejects_target_inside_source(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            source = root / "artifact"
            source.mkdir()
            (source / "fnql.x86.exe").write_text("binary", encoding="utf-8")

            with self.assertRaisesRegex(ValueError, "inside artifact source"):
                release.copy_release_artifact_contents(source, source / "stage")

    def test_release_artifact_dirs_rejects_empty_roots(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)

            with self.assertRaisesRegex(ValueError, "does not contain any artifact directories"):
                release.release_artifact_dirs(root)

    def test_release_artifact_dirs_require_supported_windows_linux_set(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            for name in release.PUBLISHED_RELEASE_ARTIFACTS:
                (root / name).mkdir()

            self.assertEqual(
                [path.name for path in release.release_artifact_dirs(root)],
                sorted(release.PUBLISHED_RELEASE_ARTIFACTS),
            )

        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            for name in release.PUBLISHED_RELEASE_ARTIFACTS:
                (root / name).mkdir()
            (root / "macos-x86_64").mkdir()

            with self.assertRaisesRegex(
                ValueError,
                "supported Windows/Linux set.*unsupported: macos-x86_64",
            ):
                release.release_artifact_dirs(root)

    def test_release_stage_rejects_a_64_bit_pe_even_without_an_arch_suffix(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            binary = bytearray(128)
            binary[0:2] = b"MZ"
            binary[60:64] = (64).to_bytes(4, "little")
            binary[64:68] = b"PE\0\0"
            binary[68:70] = (0x8664).to_bytes(2, "little")
            (root / "fntech3.exe").write_bytes(binary)

            with self.assertRaisesRegex(ValueError, "PE machine 0x8664"):
                release.validate_stage_tree(root)

    def test_release_binary_header_gate_rejects_64_bit_elf_and_macho(self) -> None:
        elf = bytearray(64)
        elf[0:4] = b"\x7fELF"
        elf[4] = 2
        elf[5] = 1
        elf[18:20] = (62).to_bytes(2, "little")

        for name, binary in (
            ("fntech3", bytes(elf)),
            ("fnql.dylib", b"\xcf\xfa\xed\xfe" + bytes(60)),
        ):
            with self.subTest(name=name):
                with self.assertRaisesRegex(ValueError, "32-bit x86 only"):
                    release.validate_release_binary_stream(name, io.BytesIO(binary))

    def test_macos_policy_accepts_only_the_declared_native_architecture(self) -> None:
        x86 = make_macho64(release.MACOS_X86_64_CPU)
        arm = make_macho64(release.MACOS_ARM64_CPU)

        release.validate_release_binary_stream(
            "FnQL",
            io.BytesIO(x86),
            platform="macos",
            expected_macos_arch="x86_64",
        )
        with self.assertRaisesRegex(ValueError, "architecture mismatch"):
            release.validate_release_binary_stream(
                "FnQL",
                io.BytesIO(arm),
                platform="macos",
                expected_macos_arch="x86_64",
            )

        release.validate_release_binary_stream(
            "FnQL",
            io.BytesIO(make_universal2_macho()),
            platform="macos",
            expected_macos_arch="universal2",
        )
        with self.assertRaisesRegex(ValueError, "Universal 2 artifact contains a thin binary"):
            release.validate_release_binary_stream(
                "FnQL",
                io.BytesIO(x86),
                platform="macos",
                expected_macos_arch="universal2",
            )

    def test_macos_zip_preserves_code_modes_and_validates_the_app(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            stage = root / "stage"
            macos = stage / "FnQL.app" / "Contents" / "MacOS"
            macos.mkdir(parents=True)
            macho = make_macho64(release.MACOS_X86_64_CPU)
            (macos / "FnQL").write_bytes(macho)
            (stage / "fntech3.ded").write_bytes(macho)
            (stage / "fnql-audiozonesc").write_bytes(macho)
            for renderer in release.PUBLIC_RENDERERS:
                (macos / f"fnql_{renderer}_x86_64.dylib").write_bytes(macho)
            info = {
                "CFBundleExecutable": "FnQL",
                "CFBundleIdentifier": "org.fnql.fnql",
                "CFBundlePackageType": "APPL",
                "CFBundleShortVersionString": "0.1.0",
                "CFBundleVersion": "1",
                "LSMinimumSystemVersion": "11.0",
                "NSHighResolutionCapable": True,
                "NSMicrophoneUsageDescription": "Optional in-game voice capture.",
            }
            (stage / "FnQL.app" / "Contents" / "Info.plist").write_bytes(
                plistlib.dumps(info)
            )
            signature = stage / "FnQL.app" / "Contents" / "_CodeSignature"
            signature.mkdir()
            (signature / "CodeResources").write_bytes(b"test signature envelope")
            release.copy_docs(stage)
            webpak = release.build_fnql_webpak(stage)
            package = release.build_root_archive(stage)
            (macos / release.FNQL_WEBPAK_NAME).write_bytes(webpak.read_bytes())
            (macos / release.ROOT_ARCHIVE_NAME).write_bytes(package.read_bytes())
            archive = root / "fnql-test-macos-x86_64.zip"

            release.write_release_archive(stage, archive)
            release.validate_release_archive_contents(archive)
            members = {member.name: member for member in release.read_release_archive(archive)}

        self.assertEqual(members["FnQL.app/Contents/MacOS/FnQL"].mode, 0o755)
        self.assertEqual(
            members[f"FnQL.app/Contents/MacOS/{release.ROOT_ARCHIVE_NAME}"].mode,
            0o644,
        )

    def test_prebuilt_macos_payload_is_published_byte_for_byte(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            stage = root / "stage"
            artifact = root / "macos-x86_64"
            artifact.mkdir()
            populate_macos_release_stage(stage)
            payload = artifact / release.MACOS_PREBUILT_PAYLOAD_NAME
            release.write_release_archive(stage, payload)
            expected = payload.read_bytes()
            destination = root / "fnql-test-macos-x86_64.zip"

            self.assertTrue(
                release.publish_prebuilt_macos_payload(artifact, destination)
            )
            self.assertEqual(destination.read_bytes(), expected)

            (artifact / "unexpected.txt").write_text("reject", encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "must contain only"):
                release.publish_prebuilt_macos_payload(
                    artifact,
                    root / "second.zip",
                )

    def test_unsigned_prebuilt_macos_payload_requires_explicit_policy(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            stage = root / "stage"
            artifact = root / "macos-x86_64"
            artifact.mkdir()
            populate_macos_release_stage(stage)
            shutil.rmtree(stage / "FnQL.app" / "Contents" / "_CodeSignature")
            payload = artifact / release.MACOS_PREBUILT_PAYLOAD_NAME
            release.write_release_archive(stage, payload)
            destination = root / "fnql-test-macos-x86_64.zip"

            with self.assertRaisesRegex(ValueError, "_CodeSignature/CodeResources"):
                release.publish_prebuilt_macos_payload(artifact, destination)

            self.assertTrue(
                release.publish_prebuilt_macos_payload(
                    artifact,
                    destination,
                    require_macos_signature=False,
                )
            )
            self.assertEqual(destination.read_bytes(), payload.read_bytes())

    def test_macos_policy_requires_supported_minimum_and_signed_app(self) -> None:
        with self.assertRaisesRegex(ValueError, "LSMinimumSystemVersion"):
            release.validate_macos_distribution_files(
                make_macos_release_members(minimum_version="12.0"),
                archive_name="macos-x86_64.zip",
            )

        with self.assertRaisesRegex(ValueError, "_CodeSignature/CodeResources"):
            release.validate_macos_distribution_files(
                make_macos_release_members(include_signature=False),
                archive_name="macos-x86_64.zip",
            )
        with self.assertRaisesRegex(ValueError, "must be an unsigned macOS app bundle"):
            release.validate_macos_distribution_files(
                make_macos_release_members(),
                archive_name="macos-x86_64.zip",
                require_signature=False,
            )
        release.validate_macos_distribution_files(
            make_macos_release_members(include_signature=False),
            archive_name="macos-x86_64.zip",
            require_signature=False,
        )

        with self.assertRaisesRegex(ValueError, "CFBundleShortVersionString"):
            release.validate_macos_distribution_files(
                make_macos_release_members(short_version="0.1.0.42"),
                archive_name="macos-x86_64.zip",
            )
        with self.assertRaisesRegex(ValueError, "CFBundleVersion"):
            release.validate_macos_distribution_files(
                make_macos_release_members(bundle_version="0.1.0.42"),
                archive_name="macos-x86_64.zip",
            )

    def test_macos_policy_requires_app_local_renderer_modules(self) -> None:
        with self.assertRaisesRegex(ValueError, "renderer modules outside"):
            release.validate_macos_distribution_files(
                make_macos_release_members(renderer_parent="renderers"),
                archive_name="macos-x86_64.zip",
            )

    def test_macos_policy_forbids_valve_steam_runtime(self) -> None:
        with self.assertRaisesRegex(ValueError, "must not redistribute.*libsteam_api.dylib"):
            release.validate_macos_distribution_files(
                make_macos_release_members(
                    extra_names=("FnQL.app/Contents/MacOS/libsteam_api.dylib",)
                ),
                archive_name="macos-x86_64.zip",
            )

    def test_cmake_sets_macos_deployment_target_before_project(self) -> None:
        cmake = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
        before_project = cmake[: cmake.index("\nPROJECT(")]
        self.assertIn(
            "IF(NOT DEFINED CMAKE_OSX_DEPLOYMENT_TARGET)",
            before_project,
        )
        self.assertIn(
            'SET(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING',
            before_project,
        )
        self.assertNotIn(
            "IF(APPLE AND NOT DEFINED CMAKE_OSX_DEPLOYMENT_TARGET)",
            before_project,
        )
        self.assertIn("FNQL_MACOS_SHORT_VERSION", cmake)
        self.assertIn("FNQL_MACOS_BUNDLE_VERSION", cmake)
        self.assertNotIn(
            'MACOSX_BUNDLE_SHORT_VERSION_STRING "${FNQL_VERSION_STRING}"',
            cmake,
        )

    def test_release_workflow_publishes_only_supported_windows_and_linux(self) -> None:
        workflow = (ROOT / ".github" / "workflows" / "release.yml").read_text(
            encoding="utf-8"
        )

        self.assertIn("name: windows-mingw-x86", workflow)
        self.assertIn("name: windows-msvc-x86", workflow)
        self.assertIn("name: linux-x86", workflow)
        self.assertIn("ARCH=x86 COMPILE_ARCH=x86", workflow)
        self.assertEqual(
            workflow.count("verify_release_layout.py bin/fntech3-pkg.fnz"),
            3,
        )
        self.assertEqual(workflow.count("--skip-subprojects"), 2)
        self.assertNotIn("fetch_steam_provider.py", workflow)
        self.assertEqual(workflow.count("check_windows_runtime_deps.py --require-pe bin"), 2)
        self.assertIn("-Dc_link_args=-static", workflow)
        self.assertIn("-Dcpp_link_args=-static", workflow)
        self.assertEqual(workflow.count("-Dzlib:default_library=static"), 2)
        self.assertIn("-Db_vscrt=static_from_buildtype", workflow)
        self.assertEqual(workflow.count("--wrap-mode=forcefallback"), 2)
        meson = (ROOT / "meson.build").read_text(encoding="utf-8")
        self.assertIn("'zlib:default_library=static'", meson)
        self.assertIn("'zstd=disabled'", meson)
        self.assertIn("'brotli=disabled'", meson)
        self.assertIn("'ssh=disabled'", meson)
        self.assertIn("if cc.get_argument_syntax() == 'gcc'", meson)
        self.assertIn("find_program('windres', 'rc', 'llvm-rc'", meson)
        self.assertNotIn("verify_release_layout.py bin\n", workflow)
        self.assertNotIn("FNQ3_", workflow)
        self.assertNotIn("docs/fnquake3/", workflow)
        self.assertIn('python scripts/release_workflow.py draft --tag "$RELEASE_TAG"', workflow)
        self.assertNotIn("ubuntu-arm64:", workflow)
        self.assertNotIn("sign_macos:", workflow)
        self.assertNotIn("  macos:", workflow)
        self.assertNotIn("  macos-release-sign:", workflow)
        self.assertIn(
            "needs: [prepare, windows-msys32, windows-msvc, source-validation, "
            "linux-x86-regression, ubuntu-x86]",
            workflow,
        )
        self.assertIn(
            "needs: [prepare, push-build-validation]",
            workflow,
        )
        self.assertIn("needs.push-build-validation.result == 'success'", workflow)
        publish_job = workflow.split("  publish:", 1)[1]
        self.assertNotIn("Download Intel macOS artifact", publish_job)
        self.assertNotIn("Download Apple Silicon macOS artifact", publish_job)
        self.assertNotIn("allow-unsigned-macos", publish_job)
        self.assertNotIn("arch: [x86, x86_64]", workflow)
        self.assertNotIn("arch: [arm64, x86, x64]", workflow)

    def test_release_workflow_runs_complete_regressions(self) -> None:
        workflow = (ROOT / ".github" / "workflows" / "release.yml").read_text(
            encoding="utf-8"
        )

        self.assertEqual(
            workflow.count("meson test -C meson/build-x86 --print-errorlogs")
            + workflow.count("meson test -C meson\\build-x86 --print-errorlogs"),
            2,
        )
        self.assertNotIn("meson test -C meson/build-x86 fnql_factory_rotation", workflow)
        self.assertNotIn("meson test -C meson\\build-x86 fnql_factory_rotation", workflow)
        self.assertIn("run: python -m pytest", workflow)
        self.assertIn("linux-x86-regression:", workflow)
        self.assertIn(
            "meson test -C .tmp/meson-linux-release-tests --print-errorlogs",
            workflow,
        )
        self.assertIn(
            "source-validation, linux-x86-regression, ubuntu-x86",
            workflow,
        )
        # The GLx release-proof matrix gated publishing on self-hosted runners
        # labelled glx that are not registered, plus a retail basepath and a
        # reviewed baseline directory that no repository variable supplies, so
        # every release queued forever against labels nothing could satisfy.
        # Publishing must not depend on runners this repository does not have.
        self.assertNotIn("glx-release-proof:", workflow)
        self.assertNotIn("self-hosted", workflow)
        self.assertNotIn("--require-glx-proof", workflow)
        self.assertIn("needs: [prepare, push-build-validation]", workflow)

    def test_public_manual_proof_flag_preserves_local_snapshot_exemption(self) -> None:
        local_args = SimpleNamespace(
            channel="manual",
            glx_proof_root=None,
            require_glx_proof=False,
        )
        local_proof = release.resolve_glx_runtime_proof(local_args)
        self.assertFalse(local_proof["required"])
        self.assertEqual(local_proof["status"], "not-required")

        public_args = SimpleNamespace(
            channel="manual",
            glx_proof_root=None,
            require_glx_proof=True,
        )
        with self.assertRaisesRegex(ValueError, "public and tagged releases"):
            release.resolve_glx_runtime_proof(public_args)

        tagged_args = SimpleNamespace(
            channel="release",
            glx_proof_root=None,
            require_glx_proof=False,
        )
        with self.assertRaisesRegex(ValueError, "public and tagged releases"):
            release.resolve_glx_runtime_proof(tagged_args)

        with tempfile.TemporaryDirectory() as tmp:
            tagged_args.glx_proof_root = Path(tmp)
            with self.assertRaisesRegex(ValueError, "numeric --glx-proof-build-run-id"):
                release.resolve_glx_runtime_proof(tagged_args, "a" * 40, {})

    def test_public_manual_proof_flag_revalidates_supplied_root(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            proof_root = Path(tmp)
            args = SimpleNamespace(
                channel="manual",
                glx_proof_root=proof_root,
                require_glx_proof=True,
                glx_proof_build_run_id="1234",
                glx_proof_build_run_attempt="1",
            )
            runtime_identities = {"linux-x86": {"client": {}, "glxModule": {}}}
            with mock.patch.object(
                release,
                "validate_release_proof_root",
                return_value={"status": "passed", "failures": []},
            ) as validate:
                proof = release.resolve_glx_runtime_proof(
                    args,
                    "a" * 40,
                    runtime_identities,
                )

        validate.assert_called_once_with(
            proof_root,
            expected_source_commit="a" * 40,
            expected_build_run_id="1234",
            expected_build_run_attempt="1",
            expected_runtime_identities=runtime_identities,
        )
        self.assertTrue(proof["required"])
        self.assertEqual(proof["status"], "passed")

    def test_release_cli_parser_rejects_negative_build_numbers(self) -> None:
        with self.assertRaisesRegex(Exception, "non-negative"):
            release.non_negative_int("-1")


if __name__ == "__main__":
    unittest.main()
