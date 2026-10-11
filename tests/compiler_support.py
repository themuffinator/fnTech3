"""Keep standalone probes aligned with the shared engine's x86 target."""
import os
import platform
from functools import lru_cache
from pathlib import Path
import subprocess
import sys


@lru_cache(maxsize=16)
def _is_mingw_x86(compiler: str) -> bool:
    name = Path(compiler).name.lower()
    if not any(token in name for token in ('gcc', 'g++')):
        return False
    result = subprocess.run([compiler, '-dumpmachine'], capture_output=True,
                            text=True, timeout=10, check=False)
    target = result.stdout.strip().lower()
    return result.returncode == 0 and 'mingw' in target and target.startswith(
        ('i386-', 'i486-', 'i586-', 'i686-'))


def compiler_target_args(compiler: str) -> list[str]:
    if (os.name == "nt" and "clang" in Path(compiler).name.lower()
            and os.environ.get("VSCMD_ARG_TGT_ARCH", "").lower() == "x86"):
        return ["--target=i686-pc-windows-msvc"]
    if os.name == 'nt' and _is_mingw_x86(compiler):
        # A nonempty x86 target prevents callers from selecting the Windows
        # x64-only diagnostic fallback for an already-32-bit MinGW compiler.
        return ['-m32']
    if (sys.platform.startswith("linux")
            and platform.machine().lower() in {"x86_64", "amd64", "i386", "i486", "i586", "i686"}):
        # GCC and Clang default to the host architecture, independently of a
        # Meson cross file. Shared engine headers require 32-bit pointers.
        return ["-m32"]
    return []
