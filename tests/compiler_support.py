"""Keep standalone Clang probes aligned with an active MSVC library environment."""
import os
from pathlib import Path


def compiler_target_args(compiler: str) -> list[str]:
    if (os.name == "nt" and "clang" in Path(compiler).name.lower()
            and os.environ.get("VSCMD_ARG_TGT_ARCH", "").lower() == "x86"):
        return ["--target=i686-pc-windows-msvc"]
    return []
