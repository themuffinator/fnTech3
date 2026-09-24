#!/usr/bin/env python3
"""Opt-in Windows Q3 1.29/1.30/1.31 peer and demo interchange probes.

Copies user-supplied assets into a fresh output directory. Every process uses
an isolated writable home and an unswitched private desktop, with windowed
rendering and mouse/joystick input disabled. No input injection or OS capture.
The report proves short active sessions, pure admission, recording and playback;
it does not establish sustained gameplay or visual parity.
"""
from pathlib import Path
import argparse
import json
import os
import re
import shutil
import socket
import sys
import time

from probe_demo_roundtrip import envelope, fntech3_command, retail_command, run, sha

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tests"))
from windows_private_process import WindowsPrivateProcess

ERRORS = re.compile(r"^.*(?:ERROR:|Illegible|read past end|was truncated|Unpure client).*$", re.M)


def command(executable, values, extra):
    result = [str(executable)]
    for key, value in values.items():
        result += ["+set", key, str(value)]
    return result + extra


def copy_installation(source, destination):
    game = destination / "baseq3"
    game.mkdir(parents=True)
    inputs = []
    for pak in sorted((source / "baseq3").glob("*.pk3")):
        shutil.copyfile(pak, game / pak.name)
        inputs.append(dict(name=pak.name, bytes=pak.stat().st_size, sha256=sha(pak)))
    if not (game / "pak0.pk3").is_file():
        raise ValueError("Installation must contain baseq3/pak0.pk3")
    return inputs


def ready(port, process, timeout):
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
        sock.settimeout(.2)
        until = time.monotonic() + timeout
        while process.poll() is None and time.monotonic() < until:
            sock.sendto(b"\xff" * 4 + b"getinfo point-probe", ("127.0.0.1", port))
            try:
                data, peer = sock.recvfrom(65535)
                if peer == ("127.0.0.1", port) and data.startswith(b"\xff" * 4 + b"infoResponse\n"):
                    return data.decode("latin1")
            except (socket.timeout, ConnectionResetError):
                pass
    raise RuntimeError("Dedicated peer did not become ready")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("client", "server", "reference", "installation", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--modern-installation", type=Path,
                        help="Full 1.32 assets for testing automatic 66/67 negotiation from protocol 68")
    parser.add_argument("--protocol", type=int, choices=(66, 67), required=True)
    parser.add_argument("--port", type=int, default=29300)
    parser.add_argument("--timeout", type=float, default=30)
    args = parser.parse_args()
    if os.name != "nt":
        parser.error("This private-desktop runtime probe requires Windows")
    for name in ("client", "server", "reference", "installation", "output", "modern_installation"):
        value = getattr(args, name)
        if value is not None:
            setattr(args, name, value.resolve())
    for source in (args.installation, args.modern_installation, args.reference.parent):
        if source and (args.output.is_relative_to(source) or source.is_relative_to(args.output)):
            parser.error("Output and source installations must be disjoint")
    if not 1024 <= args.port <= 65530 or args.timeout <= 0:
        parser.error("Choose a port from 1024 to 65530 and a positive timeout")
    args.output.mkdir(parents=True, exist_ok=False)  # Never reuse stale logs.
    installation = args.output / "inputs" / "point-release"
    inputs = copy_installation(args.installation, installation)
    reference = installation / args.reference.name
    shutil.copyfile(args.reference, reference)
    for dll in args.reference.parent.glob("*.dll"):
        shutil.copyfile(dll, installation / dll.name)
    modern = None
    modern_inputs = []
    if args.modern_installation:
        modern = args.output / "inputs" / "modern"
        modern_inputs = copy_installation(args.modern_installation, modern)
    report = dict(protocol=args.protocol, platform="Windows x86", inputs=inputs,
                  modern_inputs=modern_inputs,
                  executables={name: dict(path=str(getattr(args, name)), sha256=sha(getattr(args, name)))
                               for name in ("client", "server", "reference")}, sessions=[], replays=[])

    def save():
        (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")

    save()
    for direction in ("reference-to-fntech3", "fntech3-to-reference", *(["automatic"] if modern else [])):
        for pure in (0, 1):
            output = args.output / f"{direction}-pure{pure}"
            shome, chome = output / "server-home", output / "client-home"
            shome.mkdir(parents=True)
            chome.mkdir()
            incoming = direction == "reference-to-fntech3"
            server = args.server if incoming else reference
            client = reference if incoming else args.client
            port = args.port + len(report["sessions"])
            common = dict(fs_basepath=installation, r_fullscreen=0, in_mouse=0,
                          in_joystick=0, in_nograb=1, logfile=2, net_ip="127.0.0.1",
                          bot_enable=0, sv_pure=pure, g_synchronousClients=1)
            server_values = dict(common, fs_homepath=shome, dedicated=1, developer=1,
                                 net_port=port, sv_maxclients=2, sv_punkbuster=0)
            client_values = dict(common, fs_homepath=chome, dedicated=0, developer=0,
                                 r_mode=3, s_initsound=0, r_ignorehwgamma=1, com_maxfps=60,
                                 com_maxfpsUnfocused=60, com_maxfpsMinimized=60, net_port=0,
                                 cl_motd=0, com_introplayed=1, cl_allowDownload=0,
                                 activeAction="wait 60; echo POINT_ACTIVE; record point; wait 120; stoprecord; quit;")
            if incoming:
                server_values.update(com_gameProfile="q3", protocol=args.protocol)
            else:
                client_values.update(com_gameProfile="q3", protocol=args.protocol,
                                     cl_renderer="glx", cl_webuiEnable=0, com_steamIntegration=0)
                if direction == "automatic":
                    client_values.update(fs_basepath=modern, protocol=68)
            scmd = command(server, server_values, ["+map", "q3dm1"])
            ccmd = command(client, client_values, ["+connect", f"127.0.0.1:{port}"])
            sproc = WindowsPrivateProcess(scmd, server.parent, output / "server-stdout.log")
            try:
                info = ready(port, sproc, args.timeout)
                code, forced, elapsed, log = run(ccmd, client.parent, output / "client-stdout.log",
                                                chome / "baseq3/qconsole.log", args.timeout)
            finally:
                sproc.close()
            demo = chome / "baseq3/demos" / f"point.dm_{args.protocol}"
            recorded = envelope(demo) if demo.is_file() else None
            active = bool(re.search(r"^POINT_ACTIVE\s*$", log, re.M))
            errors = ERRORS.findall(log)
            row = dict(direction=direction, pure=pure, server_command=scmd, client_command=ccmd,
                       server_info=info, exit_code=code, forced=forced, elapsed=elapsed,
                       active=active, errors=errors, demo=recorded,
                       passed=code == 0 and not forced and active and not errors and
                              bool(recorded and recorded["terminated"] and recorded["messages"] > 1))
            report["sessions"].append(row)
            save()
            print(direction, pure, "PASS" if row["passed"] else "FAIL", flush=True)
            if not row["passed"] or not pure or direction == "automatic":
                continue
            for player in ("fntech3", "reference"):
                replay = output / ("replay-" + player)
                game = replay / "home/baseq3"
                (game / "demos").mkdir(parents=True)
                shutil.copyfile(demo, game / "demos" / demo.name)
                (game / "probe.cfg").write_text(
                    'set com_introplayed 1\nset cl_motd 0\nset timedemo 1\n'
                    'set nextdemo "echo POINT_REPLAY_COMPLETE; quit"\ndemo ' + demo.name + "\n",
                    encoding="utf-8")
                executable = args.client if player == "fntech3" else reference
                extra = ["+exec", "probe.cfg"]
                cmd = (fntech3_command(executable, installation, replay / "home", "q3",
                                       ["+set", "protocol", str(args.protocol)] + extra)
                       if player == "fntech3" else
                       retail_command(executable, installation, replay / "home", {"dedicated": "0"}, extra))
                code, forced, elapsed, log = run(cmd, executable.parent, replay / "stdout.log",
                                                game / "qconsole.log", args.timeout)
                complete = bool(re.search(r"^POINT_REPLAY_COMPLETE\s*$", log, re.M))
                errors = ERRORS.findall(log)
                row = dict(recorder=direction, player=player, command=cmd, exit_code=code,
                           forced=forced, elapsed=elapsed, completed=complete, errors=errors,
                           timing=re.findall(r"\d+ frames, [^\r\n]+", log),
                           passed=code == 0 and not forced and complete and not errors)
                report["replays"].append(row)
                save()
                print("replay", direction, player, "PASS" if row["passed"] else "FAIL", flush=True)
    report["passed"] = (all(row["passed"] for row in report["sessions"] + report["replays"])
                        and len(report["replays"]) == 4)
    save()
    return 0 if report["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
