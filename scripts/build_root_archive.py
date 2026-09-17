from __future__ import annotations

import argparse
import sys
from pathlib import Path
from typing import Sequence

sys.path.insert(0, str(Path(__file__).resolve().parent))

from root_archive import (
    DEFAULT_ROOT_ARCHIVE_REQUIRED_ASSETS, PKG_ROOT, ROOT_ARCHIVE_NAME,
    write_root_archive,
)


def parse_args(argv: Sequence[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=f"Build the {ROOT_ARCHIVE_NAME} runtime package archive.",
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=Path(ROOT_ARCHIVE_NAME),
        help=f"Archive path to write. Defaults to ./{ROOT_ARCHIVE_NAME}.",
    )
    parser.add_argument(
        "--package-root",
        type=Path,
        default=PKG_ROOT,
        help="Source tree with games/<profile>/<gamedir>/ assets. Defaults to repo pkg/.",
    )
    return parser.parse_args(argv)


def main(argv: Sequence[str] | None = None) -> int:
    args = parse_args(sys.argv[1:] if argv is None else argv)
    archive_path = args.output.expanduser()
    package_root = args.package_root.expanduser()

    try:
        write_root_archive(archive_path, package_root=package_root,
                           required_assets=DEFAULT_ROOT_ARCHIVE_REQUIRED_ASSETS)
    except (OSError, ValueError) as exc:
        print(f"build_root_archive.py: {exc}", file=sys.stderr)
        return 1

    print(f"{ROOT_ARCHIVE_NAME}: {archive_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
