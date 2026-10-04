#!/usr/bin/env python3
"""Probe both game profiles with one dedicated binary and user-owned installs.

This fixture gate verifies module/map startup and connectionless protocol
identity. It is not a gameplay, renderer, authentication, or interop sign-off.
All writable game data goes to disposable homes under the project's .tmp.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
from pathlib import Path
import socket
import stat
import subprocess
import sys
import tempfile
import time
import uuid


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tests'))
from server_query_evidence import observed_port, server_info

REPARSE_POINT = getattr(stat, 'FILE_ATTRIBUTE_REPARSE_POINT', 0x400)


def sha256(path: Path) -> str:
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def installation_observed(log: str, marker: Path) -> bool:
    expected = os.path.normcase(os.path.normpath(str(marker.resolve())))
    return any(os.path.normcase(os.path.normpath(match.group(1))) == expected
               for match in re.finditer(r"^(.+) \(\d+ files\)\r?$", log, re.MULTILINE))


def temporary_home_root() -> Path:
    """Keep writable probe homes inside the project, without following links."""
    project = ROOT.resolve(strict=True)
    scratch = project / '.tmp'
    scratch.mkdir(exist_ok=True)
    metadata = scratch.lstat()
    if (stat.S_ISLNK(metadata.st_mode)
            or getattr(metadata, 'st_file_attributes', 0) & REPARSE_POINT
            or scratch.resolve(strict=True).parent != project):
        raise ValueError('Profile probe .tmp must be a real directory inside the project')
    return scratch


def probe(executable: Path, install: Path, profile: str, selection: str,
          output: Path, timeout: float, steam_discovery: bool = False) -> dict:
    protocol, mapname = (68, "q3dm6") if profile == "q3" else (91, "campgrounds")
    # Reserve a candidate loopback port. A concurrent claimant is detected by
    # the per-run challenge/map checks and the server log; no public bind occurs.
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as reservation:
        reservation.bind(("127.0.0.1", 0))
        port = reservation.getsockname()[1]
    result = {"profile": profile, "selection": selection, "map": mapname,
              "protocol": protocol, "installation": str(install), "passed": False,
              "steamDiscovery": steam_discovery, "requestedPort": port}
    challenge = 'fntech3-' + uuid.uuid4().hex
    marker = install / "baseq3" / ("pak0.pk3" if profile == "q3" else "pak00.pk3")
    result["basePakSha256"] = sha256(marker)
    with tempfile.TemporaryDirectory(prefix="fntech3-profile-", dir=temporary_home_root()) as temporary:
        home = Path(temporary)
        command = [str(executable)]
        for name, value in {
            "com_gameProfile": selection, "fs_basepath": str(install),
            "fs_homepath": str(home), "dedicated": "1", "net_enabled": "1",
            "net_ip": "127.0.0.1", "net_ip6": "::1",
            "net_port": str(port), "sv_pure": "1", "logfile": "2",
            "com_steamIntegration": "0", "fs_skipWorkshop": "1",
            "sv_autoRecordDemos": "0", "r_fullscreen": "0",
        }.items():
            if steam_discovery and name == "fs_basepath":
                continue
            command.extend(["+set", name, value])
        # Early FS startup precedes logfile creation; query the mounted paks
        # after initialization instead of treating a missing early line as failure.
        command.extend(["+path", "+map", mapname])
        if profile == "ql":
            command.append("ffa")
        command.extend(["+net_port", "+wait", "300", "+quit"])
        result["command"] = command
        options = {"creationflags": subprocess.CREATE_NO_WINDOW} if os.name == "nt" else {}
        start = time.monotonic()
        with (output / f"{profile}-{selection}-stdout.log").open("wb") as stdout:
            process = subprocess.Popen(command, cwd=executable.parent,
                                       stdin=subprocess.DEVNULL, stdout=stdout,
                                       stderr=subprocess.STDOUT, **options)
            replies = []
            try:
                with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
                    sock.settimeout(0.3)
                    while process.poll() is None and time.monotonic() - start < timeout:
                        sock.sendto(b"\xff\xff\xff\xffgetinfo " + challenge.encode('ascii') + b"\n",
                                    ("127.0.0.1", port))
                        try:
                            payload, address = sock.recvfrom(65535)
                        except (socket.timeout, ConnectionResetError):
                            continue
                        if address != ("127.0.0.1", port):
                            continue
                        reply = payload.decode("latin1").rstrip("\x00")
                        if reply not in replies:
                            replies.append(reply)
                        info = server_info(payload, 'infoResponse', mapname, challenge, protocol)
                        if info is not None:
                            result["serverInfo"] = info
                            break
                process.wait(timeout=max(0.1, timeout - (time.monotonic() - start)))
            except subprocess.TimeoutExpired:
                result["error"] = "server did not quit before timeout"
            finally:
                if process.poll() is None:
                    process.terminate()
                    try:
                        process.wait(timeout=5)
                    except subprocess.TimeoutExpired:
                        process.kill()
                        process.wait(timeout=5)
                result["exitCode"] = process.returncode
                result["elapsedSeconds"] = round(time.monotonic() - start, 3)
                log_path = home / "baseq3" / "qconsole.log"
                log = log_path.read_text(encoding="utf-8", errors="replace") if log_path.exists() else ""
                (output / f"{profile}-{selection}-console.log").write_text(log, encoding="utf-8")
                (output / f"{profile}-{selection}-replies.json").write_text(json.dumps(replies, indent=2), encoding="utf-8")
                module = "VM_LoadDll(qagame) found native exports" if profile == "ql" else "qagame loaded in"
                result["moduleObserved"] = module in log
                result["installationObserved"] = installation_observed(log, marker)
                result["observedPort"] = observed_port(log)
                result["passed"] = bool(result.get("serverInfo") and result["moduleObserved"]
                                        and result["installationObserved"]
                                        and result["observedPort"] == port
                                        and process.returncode == 0 and "error" not in result)
    return result


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--server", type=Path, required=True)
    parser.add_argument("--q3", type=Path)
    parser.add_argument("--ql", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--timeout", type=float, default=35)
    parser.add_argument("--steam-discovery", action="store_true",
                        help="omit fs_basepath and test explicit profiles against discovered Steam installs")
    args = parser.parse_args()
    if not (args.q3 or args.ql):
        parser.error("provide --q3 and/or --ql installations")
    if args.timeout <= 0:
        parser.error("--timeout must be positive")
    executable = args.server.resolve(strict=True)
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    manifest = {"executable": str(executable), "executableSha256": sha256(executable),
                "commit": subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip(),
                "worktreeDirty": bool(subprocess.check_output(["git", "status", "--porcelain"], cwd=ROOT)),
                "platform": os.name, "runs": []}
    for profile, install in (("q3", args.q3), ("ql", args.ql)):
        if install:
            for selection in ((profile,) if args.steam_discovery else ("auto", profile)):
                result = probe(executable, install.resolve(strict=True), profile, selection, output,
                               args.timeout, args.steam_discovery)
                manifest["runs"].append(result)
                (output / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
                print(f"{profile}/{selection}: {'PASS' if result['passed'] else 'FAIL'}; exit={result['exitCode']}", flush=True)
    return 0 if all(run["passed"] for run in manifest["runs"]) else 1


if __name__ == "__main__":
    raise SystemExit(main())
