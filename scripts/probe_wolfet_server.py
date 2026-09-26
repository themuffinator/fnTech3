#!/usr/bin/env python3
"""Bounded Windows x86 ET startup using user-supplied assets and native modules."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import socket
import subprocess
import time


def probe(server, installation, output, mapname, automatic=False):
    output.mkdir(parents=True,exist_ok=True)
    home = output / 'home'
    home.mkdir(exist_ok=True)
    with socket.socket(socket.AF_INET,socket.SOCK_DGRAM) as reservation:
        reservation.bind(('127.0.0.1',0)); port = reservation.getsockname()[1]
    command = [str(server), '+set','com_gameProfile','auto' if automatic else 'wolfet',
        '+set','fs_basepath',str(installation), '+set','fs_homepath',str(home),
        '+set','dedicated','1', '+set','net_ip','127.0.0.1', '+set','net_port',str(port),
        '+set','net_enabled','1', '+set','sv_master1','', '+set','sv_master2','',
        '+set','sv_master3','', '+set','sv_master4','', '+set','sv_master5','',
        '+set','r_fullscreen','0', '+set','com_viewlog','0', '+set','logfile','2',
        '+set','sv_pure','0', '+set','sv_punkbuster','0', '+set','bot_enable','0',
        '+map',mapname, '+wait','100', '+quit']
    result = {'profile':'wolfet','map':mapname,'command':command,
        'executableSha256':hashlib.sha256(server.read_bytes()).hexdigest(),
        'moduleSha256':hashlib.sha256((installation/'etmain/qagame_mp_x86.dll').read_bytes()).hexdigest(),
        'passed':False}
    with (output/'stdout.log').open('wb') as log:
        options = {'creationflags':subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
        process = subprocess.Popen(command,cwd=server.parent,stdin=subprocess.DEVNULL,stdout=log,stderr=subprocess.STDOUT,**options)
        begin = time.monotonic()
        try:
            with socket.socket(socket.AF_INET,socket.SOCK_DGRAM) as query:
                query.settimeout(0.3)
                while process.poll() is None and time.monotonic()-begin < 25:
                    query.sendto(b'\xff\xff\xff\xffgetinfo fntech3-wolfet\n',('127.0.0.1',port))
                    try: data,peer = query.recvfrom(65535)
                    except (socket.timeout,ConnectionResetError): continue
                    if peer != ('127.0.0.1',port) or not data.startswith(b'\xff\xff\xff\xffinfoResponse\n'): continue
                    fields = data.decode('latin1').split('\n',1)[1].strip('\0\n ').split('\\')[1:]
                    info = dict(zip(fields[::2],fields[1::2]))
                    if info.get('mapname') == mapname and info.get('protocol') == '84':
                        result['serverInfo'] = info
                    if process.poll() is not None: break
        finally:
            if process.poll() is None:
                process.terminate()
                try: process.wait(timeout=5)
                except subprocess.TimeoutExpired: process.kill(); process.wait(timeout=5)
                result['timeout'] = True
            result['exitCode'] = process.returncode
            result['elapsedSeconds'] = round(time.monotonic()-begin,3)
    console = home/'etmain/qconsole.log'
    text = console.read_text(encoding='utf-8',errors='replace') if console.exists() else ''
    result['passed'] = bool(result.get('serverInfo') and result['exitCode'] == 0 and 'timeout' not in result)
    result['errors'] = [line for line in text.splitlines() if 'Error' in line or 'Unsupported' in line]
    (output/'result.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(result,indent=2))
    return result['passed']


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--server',type=Path,required=True)
    parser.add_argument('--installation',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--map',default='goldrush')
    parser.add_argument('--auto',action='store_true')
    args = parser.parse_args()
    if os.name != 'nt':
        parser.error('This installed native-module probe requires Windows x86')
    raise SystemExit(0 if probe(args.server.resolve(),args.installation.resolve(),args.output.resolve(),args.map,args.auto) else 1)
