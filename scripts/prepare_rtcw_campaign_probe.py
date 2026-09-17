#!/usr/bin/env python3
"""Build an isolated intro-only RTCW probe, restore source, rebuild the client."""
from pathlib import Path
import argparse
import hashlib
import json
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    build = args.build.resolve(strict=True); build.relative_to(ROOT)
    output = args.output.resolve(); output.relative_to(ROOT / '.tmp')
    output.mkdir(parents=True, exist_ok=False)
    source = ROOT / 'code/client/cl_main.cpp'
    original = source.read_bytes()
    anchor = b'void CL_Frame( int msec, int realMsec ) {'
    assert original.count(anchor) == 1
    modified = original.replace(anchor, b'#include "../../tests/rtcw_campaign_probe.hpp"\n' + anchor + b'\n\tCL_RTCWCampaignFrame();')
    anchor = b'\tCmd_AddCommand( "modelist", CL_ModeList_f );'
    assert modified.count(anchor) == 1
    modified = modified.replace(anchor, anchor + b'\n\tCmd_AddCommand("fntech3_campaign_start", CL_RTCWCampaignStart);')
    sha = lambda b: hashlib.sha256(b).hexdigest()
    (output / 'instrumentation.json').write_text(json.dumps(dict(
        source=str(source), originalSha256=sha(original), instrumentedSha256=sha(modified),
        hookSha256=sha((ROOT / 'tests/rtcw_campaign_probe.hpp').read_bytes()),
        authority='AGENTS.md standing intro-only Ready-button permission',
        inputLimit='One briefing Ready click per process; no movement or combat'), indent=2) + '\n')
    command = ['meson', 'compile', '-C', str(build), 'fntech3']
    try:
        source.write_bytes(modified)
        with (output / 'build.log').open('w') as log:
            subprocess.run(command, cwd=ROOT, stdout=log, stderr=subprocess.STDOUT, check=True)
        binary = output / 'bin'; binary.mkdir()
        identities = []
        for name in ('fntech3.exe', 'fnql_glx_x86.dll', 'fnql-web.pak'):
            dest = binary / name; shutil.copy2(build / name, dest)
            identities.append(dict(path=str(dest), sha256=sha(dest.read_bytes())))
        (output / 'binary.json').write_text(json.dumps(identities, indent=2) + '\n')
    finally:
        if source.read_bytes() != modified:
            raise RuntimeError('Client changed during probe build; refusing to overwrite concurrent edits')
        source.write_bytes(original)
        with (output / 'restore-build.log').open('w') as log:
            subprocess.run(command, cwd=ROOT, stdout=log, stderr=subprocess.STDOUT, check=True)
    print('Probe built; original client source restored and rebuilt.', flush=True)

if __name__ == '__main__':
    main()
