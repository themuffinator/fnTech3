"""Opt-in Q3 legacy qagame runtime checks; dedicated server, isolated home."""
import hashlib
import datetime
import json
import os
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path
from native_cgame_abi_probe_runtime_tests import find_vsdevcmd
from windows_private_process import WindowsPrivateProcess

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'tests/fixtures/q3_native_qagame_probe.c'


class Q3NativeQagameRuntimeTests(unittest.TestCase):
    def test_legacy_server_contract_and_rejected_binding(self):
        value = os.environ.get('FNTECH3_Q3_QAGAME_RUNTIME_EXE', '')
        if os.name != 'nt' or not value:
            self.skipTest('set FNTECH3_Q3_QAGAME_RUNTIME_EXE for Windows x86 dedicated probe')
        exe = Path(value).resolve(strict=True)
        assets = Path(os.environ.get('FNTECH3_Q3_RETAIL_PATH', 'C:/Program Files (x86)/Steam/steamapps/common/Quake 3 Arena'))
        if not (assets / 'baseq3/pak0.pk3').is_file():
            self.skipTest('legitimate Q3 assets required')
        vs = find_vsdevcmd()
        if not vs:
            self.skipTest('MSVC x86 fixture compiler required')
        output = os.environ.get('FNTECH3_Q3_QAGAME_PROBE_OUTPUT', '')
        out = Path(output) if output else None
        if out:
            out.mkdir(parents=True, exist_ok=False)
        asset_identity = []
        if out:
            for pak in sorted((assets / 'baseq3').glob('*.pk3')):
                with pak.open('rb') as stream:
                    digest = hashlib.file_digest(stream, 'sha256').hexdigest()
                asset_identity.append({'name':pak.name, 'bytes':pak.stat().st_size, 'sha256':digest})
        with tempfile.TemporaryDirectory(prefix='fntech3-q3-qagame-') as temporary:
            root = Path(temporary)
            dll = root / 'qagamex86.dll'
            build = f'call "{vs}" -arch=x86 -host_arch=x64 >nul && cl /nologo /TC /LD /O2 /Fo:"{root / "probe.obj"}" /Fe:"{dll}" "{SOURCE}"'
            compiled = subprocess.run(build, shell=True, cwd=root, capture_output=True, text=True, creationflags=subprocess.CREATE_NO_WINDOW, timeout=30)
            self.assertEqual(compiled.returncode, 0, compiled.stdout + compiled.stderr)
            for bad in (False, True):
                with self.subTest(invalid_stride=bad):
                    home = root / ('bad-stride' if bad else 'valid')
                    game = home / 'baseq3'
                    game.mkdir(parents=True)
                    shutil.copyfile(dll, game / dll.name)
                    cfg = '\n'.join(['set sv_master%d ""' % i for i in range(1, 6)]) + '\nmap q3dm6\nwait 6\nmap_restart 0\nwait 6\nfntech3_q3_qagame_probe\nquit\n'
                    (game / 'probe.cfg').write_text(cfg)
                    values = {'com_gameProfile':'q3', 'fs_basepath':str(assets), 'fs_homepath':str(home), 'fs_game':'', 'fs_cdpath':'', 'vm_game':'0', 'sv_pure':'0', 'dedicated':'1', 'net_ip':'127.0.0.1', 'net_port':'0', 'logfile':'2', 'developer':'1', 'com_steamIntegration':'0', 'fs_skipWorkshop':'1', 'fntech3_probe_bad_stride':str(int(bad)), 'r_fullscreen':'0', 'in_mouse':'0', 'in_joystick':'0', 'in_nograb':'1'}
                    command = [str(exe)]
                    values['fntech3_probe_world_restart'] = '0' if bad else '1'
                    for key, value in values.items():
                        command += ['+set', key, value]
                    command += ['+exec', 'probe.cfg']
                    process = WindowsPrivateProcess(command, exe.parent, home / 'stdout.log')
                    code = None
                    error = None
                    try:
                        code = process.wait(30)
                    except subprocess.TimeoutExpired as exc:
                        error = str(exc)
                    finally:
                        forced = process.poll() is None
                        process.close()
                    logpath = game / 'qconsole.log'
                    log = logpath.read_text(errors='replace') if logpath.exists() else ''
                    stdout = (home / 'stdout.log').read_text(errors='replace')
                    if out:
                        record = out / home.name
                        record.mkdir()
                        (record / 'console.log').write_text(log)
                        (record / 'stdout.log').write_text(stdout)
                        data = {'command':command, 'config':cfg, 'exitCode':code, 'error':error, 'forcedStop':forced, 'privateDesktop':process.name, 'desktopSwitched':False, 'engineSha256':hashlib.sha256(exe.read_bytes()).hexdigest(), 'fixtureSha256':hashlib.sha256(dll.read_bytes()).hexdigest(), 'fixtureSourceSha256':hashlib.sha256(SOURCE.read_bytes()).hexdigest(), 'commit':subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(), 'worktreeDirty':bool(subprocess.check_output(['git','status','--porcelain'],cwd=ROOT)), 'markers':[line for line in log.splitlines() if 'Q3 qagame native probe:' in line]}
                        data.update({'utc':datetime.datetime.now(datetime.timezone.utc).isoformat(), 'architecture':'Windows x86', 'fixtureBuildCommand':build, 'assets':asset_identity, 'consoleLogSha256':hashlib.sha256(logpath.read_bytes()).hexdigest() if logpath.exists() else None, 'stdoutLogSha256':hashlib.sha256((home/'stdout.log').read_bytes()).hexdigest(), 'diagnostics':[line for line in log.splitlines() if 'game-data binding' in line or 'forcefully unloading' in line or 'VM_LoadDll(qagame) found' in line]})
                        (record / 'manifest.json').write_text(json.dumps(data, indent=2)+'\n')
                    self.assertIsNone(error, (log+stdout)[-4000:])
                    self.assertFalse(forced)
                    self.assertIn('VM_LoadDll(qagame) found legacy vmMain', log)
                    self.assertEqual(code, 0, (log+stdout)[-4000:])
                    if bad:
                        self.assertIn('invalid Quake III Arena game-data binding', log+stdout)
                        self.assertLess(log.index('entity stride too small'), log.index('Q3 qagame native probe: init ok'))
                    for marker in ('binding','entity token','configstring','usercmd','bot usercmd','init','link','area','trace','contents','unlink','frame','shutdown'):
                        self.assertGreaterEqual(log.count('Q3 qagame native probe: '+marker+' ok'), 2, log[-6000:])
                    self.assertIn('Q3 qagame native probe: command ok', log)
                    if not bad:
                        self.assertIn('Q3 qagame native probe: restart old link ok',log)
                        self.assertIn('Q3 qagame native probe: restart world cleared ok',log)
                    self.assertFalse(any('FAILED' in line for line in log.splitlines() if 'Q3 qagame native probe:' in line), log[-6000:])


if __name__ == '__main__':
    unittest.main()
