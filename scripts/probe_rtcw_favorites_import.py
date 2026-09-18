#!/usr/bin/env python3
"""Check fnTech3's import of WolfMP favorites against retail WolfMP.exe.

1. Layout (with --reference): compiles the original RTCW MP netadr_t and
   serverInfo_t declarations, unchanged, for x86 in a disposable fixture and
   compares their size and offsets with code/compat/rtcw_server_cache.hpp.
2. Retail round trip: writes a WolfMP cache holding two IPv4 favorites and one
   IPX favorite to an isolated home root, starts retail WolfMP.exe with that home
   to its main menu and quits. The original UI loads the cache when it starts and
   saves it when it shuts down, zeroing every count if the stored array size
   does not match its own; a byte-identical rewrite shows the layout is WolfMP's.
   A control file whose stored size is one record short must come back with
   every count zeroed.
3. Import: copies the retail-written file to a fresh home and starts the fnTech3
   rtcw-mp client twice. The first start has no fnTech3 cache, so it imports the
   two IPv4 favorites and saves them in its own cache; the second start must not
   import again. The WolfMP file must be unchanged.

Windows only: windowed on unswitched private desktops with sound and
mouse/joystick input disabled; commands come from startup configs only.
"""
from pathlib import Path
import argparse
import hashlib
import json
import os
import re
import shutil
import struct
import subprocess
import sys
import tempfile
import time

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tests'))
from windows_private_process import WindowsPrivateProcess

RECORD, GLOBAL, OTHER = 204, 2048, 128
ARRAYS = (GLOBAL + 2 * OTHER) * RECORD
FAVORITES = 16 + (GLOBAL + OTHER) * RECORD
HOSTS = [('fnTech3 import A', (127, 0, 0, 1), 27960, 4), ('fnTech3 import B', (10, 1, 2, 3), 27961, 4),
         ('fnTech3 retired IPX', (10, 9, 9, 9), 27962, 5)]


def sha(data):
    return hashlib.sha256(data).hexdigest()


def wolfmp_cache():
    data = bytearray(16 + ARRAYS)
    struct.pack_into('<iiii', data, 0, 0, 0, len(HOSTS), ARRAYS)
    for index, (host, ip, port, kind) in enumerate(HOSTS):
        record = FAVORITES + index * RECORD
        struct.pack_into('<i4B', data, record, kind, *ip)
        struct.pack_into('>H', data, record + 18, port)
        data[record + 20:record + 20 + len(host)] = host.encode()
    return bytes(data)


def layout(reference):
    """Size and offsets of the original declarations, compiled for x86."""
    qcommon = (reference / 'qcommon/qcommon.h').read_text(errors='replace')
    client = (reference / 'client/client.h').read_text(errors='replace')
    enum = re.search(r'typedef enum \{\s*NA_BOT,.*?\} netadrtype_t;', qcommon, re.S).group(0)
    address = re.search(r'typedef struct \{\s*netadrtype_t type;.*?\} netadr_t;', qcommon, re.S).group(0)
    info = re.search(r'typedef struct \{\s*netadr_t adr;\s*char hostName.*?\} serverInfo_t;', client, re.S).group(0)
    source = ('#include <stddef.h>\n#include <stdio.h>\ntypedef unsigned char byte;\ntypedef int qboolean;\n'
              '#define MAX_NAME_LENGTH 32\n' + enum + '\n' + address + '\n' + info + '\n'
              'int main(void) { printf("%u %u %u %u %u %d\\n", (unsigned)sizeof(serverInfo_t), '
              '(unsigned)offsetof(serverInfo_t, adr), (unsigned)offsetof(netadr_t, ip), '
              '(unsigned)offsetof(netadr_t, port), (unsigned)offsetof(serverInfo_t, hostName), (int)NA_IP); '
              'return 0; }\n')
    compiler = shutil.which('clang')
    if not compiler:
        raise SystemExit('clang is required for the layout comparison')
    with tempfile.TemporaryDirectory(prefix='wolfmp-layout-', dir=ROOT / '.tmp') as temporary:
        fixture, program = Path(temporary) / 'layout.c', Path(temporary) / 'layout.exe'
        fixture.write_text(source)
        build = subprocess.run([compiler, '--target=i686-pc-windows-msvc', str(fixture), '-o', str(program)],
                               capture_output=True, text=True, timeout=120)
        if build.returncode:
            raise SystemExit(build.stdout + build.stderr)
        values = subprocess.run([str(program)], capture_output=True, text=True, timeout=30).stdout.split()
    measured = dict(zip(('recordBytes', 'addressOffset', 'ipOffset', 'portOffset', 'hostNameOffset', 'NA_IP'),
                        map(int, values)))
    header = (ROOT / 'code/compat/rtcw_server_cache.hpp').read_text()
    constant = lambda name: int(re.search(rf'{name} = (\d+);', header).group(1))
    expected = dict(recordBytes=constant('RecordBytes'), addressOffset=constant('AddressTypeOffset'),
                    ipOffset=constant('AddressIPOffset'), portOffset=constant('AddressPortOffset'),
                    hostNameOffset=constant('HostNameOffset'), NA_IP=constant('AddressIP'))
    return dict(measured=measured, expected=expected, matches=measured == expected,
                references={p: sha((reference / p).read_bytes()) for p in ('qcommon/qcommon.h', 'client/client.h')})


def run(command, cwd, stdout, seconds=90):
    process = WindowsPrivateProcess(command, cwd, stdout)
    begin = time.monotonic()
    try:
        while process.poll() is None and time.monotonic() - begin < seconds:
            time.sleep(0.2)
        forced = process.poll() is None
        code = process.poll()
    finally:
        try:
            process.close()
        except subprocess.TimeoutExpired:
            process.wait(60)
            process.close()
    return dict(exitCode=code, forcedTermination=forced, elapsedSeconds=round(time.monotonic() - begin, 1))


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--client', type=Path, required=True)
    parser.add_argument('--installation', type=Path, required=True, help='RTCW installation with WolfMP.exe')
    parser.add_argument('--reference', type=Path, help='original RTCW MP src directory for the layout comparison')
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    if os.name != 'nt':
        parser.error('This probe requires the Windows private-desktop harness')
    client, installation = args.client.resolve(strict=True), args.installation.resolve(strict=True)
    retail = installation / 'WolfMP.exe'
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    report = dict(clientSha256=sha(client.read_bytes()), retailSha256=sha(retail.read_bytes()),
                  sourceCommit=subprocess.run(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True,
                                              capture_output=True).stdout.strip(), failures=[])
    if args.reference:
        report['layout'] = layout(args.reference.resolve(strict=True))
        if not report['layout']['matches']:
            report['failures'].append('layout')
    written = wolfmp_cache()
    retail_home = output / 'retail-home'
    (retail_home / 'main').mkdir(parents=True)
    (retail_home / 'servercache.dat').write_bytes(written)
    # Back-date the file so a rewrite is visible even when its bytes are identical.
    os.utime(retail_home / 'servercache.dat', ns=(1_000_000_000_000_000_000, 1_000_000_000_000_000_000))
    (retail_home / 'main' / 'probe.cfg').write_text('wait 300\nquit\n', encoding='utf-8')
    command = [str(retail)]
    for key, value in dict(fs_basepath=str(installation), fs_homepath=str(retail_home), r_fullscreen='0', r_mode='3',
                           in_mouse='0', in_joystick='0', in_nograb='1', s_initsound='0', logfile='2',
                           cl_punkbuster='0', com_introplayed='1', r_ignorehwgamma='1').items():
        command += ['+set', key, value]
    command += ['+exec', 'probe.cfg']
    report['retail'] = run(command, installation, output / 'retail-stdout.log')
    rewritten = (retail_home / 'servercache.dat').read_bytes()
    report['retail'].update(writtenSha256=sha(written), rewrittenSha256=sha(rewritten),
                            rewrittenByRetail=(retail_home / 'servercache.dat').stat().st_mtime_ns
                            != 1_000_000_000_000_000_000,
                            identical=rewritten == written,
                            counts=list(struct.unpack_from('<iiii', rewritten)) if len(rewritten) >= 16 else None)
    if not (report['retail']['identical'] and report['retail']['rewrittenByRetail']) \
            or report['retail']['exitCode'] != 0:
        report['failures'].append('retail round trip')
    # Control: a stored array size one record short makes retail discard every count.
    control_home = output / 'retail-control-home'
    (control_home / 'main').mkdir(parents=True)
    control = bytearray(written)
    struct.pack_into('<i', control, 12, ARRAYS - RECORD)
    (control_home / 'servercache.dat').write_bytes(control)
    (control_home / 'main' / 'probe.cfg').write_text('wait 300\nquit\n', encoding='utf-8')
    command = [argument.replace(str(retail_home), str(control_home)) for argument in command]
    report['retailControl'] = run(command, installation, output / 'retail-control-stdout.log')
    discarded = (control_home / 'servercache.dat').read_bytes()
    report['retailControl']['counts'] = list(struct.unpack_from('<iiii', discarded)) if len(discarded) >= 16 else None
    if report['retailControl']['counts'] != [0, 0, 0, ARRAYS] or report['retailControl']['exitCode'] != 0:
        report['failures'].append('retail control')
    home = output / 'fntech3-home'
    (home / 'main').mkdir(parents=True)
    (home / 'servercache.dat').write_bytes(rewritten)
    (home / 'main' / 'probe.cfg').write_text('wait 300\nquit\n', encoding='utf-8')
    own = home / 'main' / 'fntech3-rtcw-mp-servercache.dat'
    report['fntech3'] = []
    for start in (1, 2):
        command = [str(client)]
        for key, value in dict(com_gameProfile='rtcw-mp', fs_basepath=str(installation), fs_homepath=str(home),
                               fs_game='', r_fullscreen='0', r_mode='3', in_mouse='0', in_joystick='0', in_nograb='1',
                               s_initsound='0', com_steamIntegration='0', fs_skipWorkshop='1', cl_webuiEnable='0',
                               cl_renderer='glx', logfile='2', net_port='0', com_introplayed='1').items():
            command += ['+set', key, value]
        command += ['+exec', 'probe.cfg']
        row = run(command, client.parent, output / f'fntech3-{start}-stdout.log')
        log = (home / 'main' / 'qconsole.log').read_text(encoding='utf-8', errors='replace')
        (output / f'fntech3-{start}-console.log').write_text(log, encoding='utf-8')
        cache = own.read_bytes() if own.exists() else b''
        row.update(command=' '.join(command[1:]), importLines=re.findall(r'^Imported [^\r\n]+', log, re.M),
                   ownCacheSha256=sha(cache) if cache else None,
                   favoriteCount=struct.unpack_from('<i', cache, 4)[0] if len(cache) >= 12 else None,
                   hostsInOwnCache=[h for h, _, _, _ in HOSTS if h.encode() in cache])
        report['fntech3'].append(row)
    first, second = report['fntech3']
    expected_hosts = [h for h, _, _, kind in HOSTS if kind == 4]
    if first['exitCode'] != 0 or first['importLines'] != ["Imported 2 favorite servers from WolfMP's servercache.dat"] \
            or first['favoriteCount'] != 2 or first['hostsInOwnCache'] != expected_hosts:
        report['failures'].append('first fnTech3 start')
    if second['exitCode'] != 0 or second['importLines'] or second['favoriteCount'] != 2 \
            or second['hostsInOwnCache'] != expected_hosts:
        report['failures'].append('second fnTech3 start')
    report['wolfmpFileUnchanged'] = (home / 'servercache.dat').read_bytes() == rewritten
    if not report['wolfmpFileUnchanged']:
        report['failures'].append('WolfMP file changed')
    report['passed'] = not report['failures']
    (output / 'result.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(json.dumps({k: v for k, v in report.items() if k not in ('fntech3',)}, indent=1))
    for row in report['fntech3']:
        print({k: v for k, v in row.items() if k != 'command'})
    return 0 if report['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
