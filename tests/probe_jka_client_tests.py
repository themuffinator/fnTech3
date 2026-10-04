"""Validate Academy probe isolation and evidence without launching a game."""
from pathlib import Path
import argparse
import contextlib
import io
import json
import struct
import sys
import tempfile
import unittest
from unittest.mock import patch
import zipfile

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'scripts'))
import probe_jka_client as probe


def fixture(folder):
    runtime=folder/'runtime';runtime.mkdir();exe=runtime/'fntech3.exe'
    image=bytearray(128);image[:2]=b'MZ';struct.pack_into('<I',image,60,80);image[80:84]=b'PE\0\0';struct.pack_into('<H',image,84,0x14c)
    exe.write_bytes(image);(runtime/'fnql_glx_x86.dll').write_bytes(b'generated renderer fixture')
    install=folder/'installation';(install/'base').mkdir(parents=True);(install/'jamp.exe').write_bytes(b'generated identity fixture')
    with zipfile.ZipFile(install/'base/assets0.pk3','w') as pak:
        pak.writestr('uix86.dll',b'generated module identity fixture')
    return argparse.Namespace(executable=exe,installation=install,output=folder/'evidence',mode='startup-ui',
        port=None,renderer='glx',frames=15,timeout=5)


class Process:
    EXIT_GRACE=5
    name='mock-private-desktop'
    frontier=False
    running=False
    def __init__(self,command,cwd,stdout):
        self.command=command;self.closed=False;self.terminated=False
        home=Path(command[command.index('fs_homepath')+1]);game=home/'base'
        mode='connect' if '+connect' in command else 'startup-ui'
        lines=['VM_LoadDll: loaded native module cache "native/uix86.dll"','800 x 600 windowed',
            '--- Common Initialization Complete ---',probe.MARKER+mode.upper().replace('-','_')]
        if self.frontier:
            lines+=['ERROR: Unsupported jka-mp UI import 57 (UI_R_REGISTERFONT)']
        else:
            if mode=='connect':lines+=['CL_FirstSnapshot: 12']
            (game/'screenshots').mkdir();(game/'screenshots'/('fntech3-jka-'+mode+'.png')).write_bytes(b'generated capture fixture')
        (home/'native').mkdir();(home/'native/uix86.dll').write_bytes(b'generated cache fixture')
        (game/'qconsole.log').write_text('\n'.join(lines)+'\n');stdout.write_text('fixture stdout\n')
    def poll(self):return None if self.running and not self.terminated else 0
    def terminate(self):self.terminated=True;return True
    def wait(self,timeout):return 1 if self.terminated else 0
    def close(self):self.closed=True


class ProbeJKAClientTests(unittest.TestCase):
    def setUp(self):
        self.parent=ROOT/'.tmp/tests/jka-client-probe';self.parent.mkdir(parents=True,exist_ok=True)
        self.temporary=tempfile.TemporaryDirectory(dir=self.parent);self.folder=Path(self.temporary.name).resolve()
        self.args=fixture(self.folder)
    def tearDown(self):
        self.assertTrue(self.folder.is_relative_to(self.parent.resolve()))
        self.assertFalse(any(p.is_symlink() or getattr(p.lstat(),'st_file_attributes',0)&0x400 for p in self.folder.rglob('*')))
        self.temporary.cleanup()
    def test_immutable_profile_window_input_loopback_and_command_order(self):
        exe,install,out,renderer=probe.validate(self.args)
        command=probe.launch_command(exe,install,out/'home',self.args)
        for name,value in {'com_gameProfile':'jka-mp','r_fullscreen':'0','r_customWidth':'800','r_customHeight':'600',
            'in_mouse':'0','in_joystick':'0','in_synthetic':'0','net_enabled':'1','net_ip':'127.0.0.1'}.items():
            self.assertEqual(command[command.index(name)+1],value)
        self.assertEqual(command[-4:],['+exec',probe.CONFIG,'+vstr','jka_probeAction'])
        config,shot=probe.configuration('startup-ui',15)
        self.assertIn('wait 15; screenshotPNG '+shot+'; wait 5; quit;',config)
        self.args.mode='connect';self.args.port=27966;probe.validate(self.args)
        command=probe.launch_command(exe,install,out/'home',self.args)
        self.assertEqual(command[-4:],['+exec',probe.CONFIG,'+connect','127.0.0.1:27966'])
        config,shot=probe.configuration('connect',15);self.assertIn('set activeAction ',config)
        self.assertNotIn('in_synthesize',config)
    def test_rejects_ports_wide_architecture_paths_and_overwritten_evidence_before_launch(self):
        for mode,port in [('connect',None),('connect',0),('connect',65536),('startup-ui',27960)]:
            self.args.mode=mode;self.args.port=port
            with self.assertRaises(ValueError):probe.validate(self.args)
        self.args.mode='startup-ui';self.args.port=None
        original=self.args.executable.read_bytes();bad=bytearray(original);struct.pack_into('<H',bad,84,0x8664)
        self.args.executable.write_bytes(bad)
        with self.assertRaises(ValueError):probe.validate(self.args)
        self.args.executable.write_bytes(original)
        self.args.output=self.args.installation/'probe'
        with self.assertRaises(ValueError):probe.validate(self.args)
        self.args.output=self.folder/'evidence';self.args.output.mkdir()
        with self.assertRaises(ValueError):probe.validate(self.args)
    def test_success_preserves_only_logs_hashes_and_engine_capture_not_cached_modules(self):
        before=probe.inventory(self.args.installation)
        with patch.object(probe,'WindowsPrivateProcess',Process),contextlib.redirect_stdout(io.StringIO()):
            self.assertTrue(probe.run(self.args))
        report=json.loads((self.args.output/'result.json').read_text())
        self.assertTrue(report['passed'] and report['homeRemoved'] and report['installationUnchanged'] and report['executableUnchanged'])
        self.assertEqual(probe.inventory(self.args.installation),before)
        self.assertEqual(len(report['moduleCandidates']),1);self.assertEqual(len(report['loadedModuleCaches']),1)
        self.assertEqual(len(report['screenshots']),1);self.assertIsNone(report['visualPassed'])
        self.assertFalse((self.args.output/'home').exists())
        self.assertFalse(list(self.args.output.rglob('*.dll')))
    def test_named_frontier_is_retained_without_false_success(self):
        with patch.object(probe,'WindowsPrivateProcess',Process),patch.object(Process,'frontier',True),contextlib.redirect_stdout(io.StringIO()):
            self.assertFalse(probe.run(self.args))
        report=json.loads((self.args.output/'result.json').read_text())
        self.assertIn('UI_R_REGISTERFONT',report['frontiers'][0]);self.assertFalse(report['passed']);self.assertTrue(report['homeRemoved'])
    def test_connect_requires_active_observation_and_cleans_cache(self):
        self.args.mode='connect';self.args.port=27966
        with patch.object(probe,'WindowsPrivateProcess',Process),contextlib.redirect_stdout(io.StringIO()):self.assertTrue(probe.run(self.args))
        report=json.loads((self.args.output/'result.json').read_text());self.assertTrue(report['activeObserved'])
    def test_cleanup_refusal_still_writes_report_and_preserves_home(self):
        with (patch.object(probe,'WindowsPrivateProcess',Process),
            patch.object(probe,'remove_home',side_effect=RuntimeError('refused fixture link')),
            contextlib.redirect_stdout(io.StringIO())):
            self.assertFalse(probe.run(self.args))
        report=json.loads((self.args.output/'result.json').read_text());self.assertFalse(report['homeRemoved']);self.assertIn('refused fixture link',report['cleanupError'])
    def test_timeout_terminates_owned_child_and_reports_failure(self):
        self.args.timeout=1
        with (patch.object(probe,'WindowsPrivateProcess',Process),patch.object(Process,'running',True),
            patch.object(probe.time,'monotonic',side_effect=[0,3,4]),contextlib.redirect_stdout(io.StringIO())):
            self.assertFalse(probe.run(self.args))
        report=json.loads((self.args.output/'result.json').read_text())
        self.assertTrue(report['forcedStop'] and report['homeRemoved']);self.assertEqual(report['exitCode'],1)
    def test_partial_launch_failure_preserves_home_and_writes_evidence(self):
        with (patch.object(probe,'WindowsPrivateProcess',side_effect=OSError('partial creation fixture')),
            contextlib.redirect_stdout(io.StringIO())):
            self.assertFalse(probe.run(self.args))
        report=json.loads((self.args.output/'result.json').read_text())
        self.assertFalse(report['homeRemoved']);self.assertIn('partial creation fixture',report['error'])


if __name__=='__main__':unittest.main()
