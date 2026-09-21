"""Keep standalone probes aligned with the shared engine's x86 target."""
import os
import platform
from pathlib import Path
import sys


def compiler_target_args(compiler: str) -> list[str]:
    if (os.name == "nt" and "clang" in Path(compiler).name.lower()
            and os.environ.get("VSCMD_ARG_TGT_ARCH", "").lower() == "x86"):
        return ["--target=i686-pc-windows-msvc"]
    if (sys.platform.startswith("linux")
            and platform.machine().lower() in {"x86_64", "amd64", "i386", "i486", "i586", "i686"}):
        # GCC and Clang default to the host architecture, independently of a
        # Meson cross file. Shared engine headers require 32-bit pointers.
        return ["-m32"]
    return []
