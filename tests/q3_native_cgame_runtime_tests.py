"""Opt-in legacy Q3 native cgame/UI contract checks using isolated assets/home."""
import hashlib,json,os,shutil,subprocess,tempfile,time,unittest
from pathlib import Path
from windows_private_process import WindowsPrivateProcess
from native_cgame_abi_probe_runtime_tests import find_vsdevcmd
ROOT=Path(__file__).resolve().parents[1]
SOURCE=ROOT/'tests/fixtures/q3_native_cgame_probe.c'
UI_SOURCE=ROOT/'tests/fixtures/q3_native_ui_probe.c'
class Q3NativeCgameRuntimeTests(unittest.TestCase):
 def test_legacy_module_contract(self):
  value=os.environ.get('FNTECH3_Q3_CGAME_RUNTIME_EXE','')
  if os.name!='nt' or not value:self.skipTest('set FNTECH3_Q3_CGAME_RUNTIME_EXE for Windows x86 runtime check')
  exe=Path(value).resolve(strict=True)
  assets=Path(os.environ.get('FNTECH3_Q3_RETAIL_PATH','C:/Program Files (x86)/Steam/steamapps/common/Quake 3 Arena'))
  if not (assets/'baseq3/pak0.pk3').is_file():self.skipTest('legitimate Q3 installation required')
  vs=find_vsdevcmd()
  if not vs:self.skipTest('MSVC x86 fixture compiler required')
  with tempfile.TemporaryDirectory(prefix='fntech3-q3-native-') as temporary:
   home=Path(temporary);game=home/'baseq3';game.mkdir();dll=game/'cgamex86.dll'
   for source, name in [(SOURCE,'cgamex86.dll'),(UI_SOURCE,'uix86.dll')]:
    build=f'call "{vs}" -arch=x86 -host_arch=x64 >nul && cl /nologo /TC /LD /O2 /Fo:"{home / (name+".obj")}" /Fe:"{game/name}" "{source}"'
    result=subprocess.run(build,shell=True,cwd=home,capture_output=True,text=True,creationflags=subprocess.CREATE_NO_WINDOW,timeout=30)
    self.assertEqual(result.returncode,0,result.stdout+result.stderr)
   values={'com_gameProfile':'q3','fs_basepath':str(assets),'fs_homepath':str(home),'vm_cgame':'0','vm_ui':'0','sv_pure':'0','r_fullscreen':'0','r_windowedMode':'-1','r_windowedWidth':'640','r_windowedHeight':'480','cl_renderer':'glx','in_mouse':'0','in_joystick':'0','in_nograb':'1','s_initsound':'0','com_steamIntegration':'0','fs_skipWorkshop':'1','cl_webuiEnable':'0','logfile':'2','net_ip':'127.0.0.1','net_port':'0','cl_motd':'0','cl_allowDownload':'0','com_maxfpsUnfocused':'60','com_maxfpsMinimized':'60','activeAction':'wait 80; fntech3_q3_probe; fntech3_q3_ui_probe; wait 3; quit'}
   command=[str(exe)]
   for k,v in values.items():command+=['+set',k,v]
   command+=['+devmap','q3dm6']
   process=WindowsPrivateProcess(command,exe.parent,home/'stdout.log')
   code=None; error=None
   try:
    code=process.wait(30)
   except subprocess.TimeoutExpired as exc:
    error=str(exc)
   finally:
    forced=process.poll() is None;process.close()
   log=(game/'qconsole.log').read_text(errors='replace') if (game/'qconsole.log').is_file() else ''
   output=os.environ.get('FNTECH3_Q3_CGAME_PROBE_OUTPUT','')
   if output:
    out=Path(output);out.mkdir(parents=True,exist_ok=False);shutil.copyfile(game/'qconsole.log',out/'console.log');shutil.copyfile(home/'stdout.log',out/'stdout.log')
    data={'command':command,'exitCode':code,'error':error,'forcedStop':forced,'privateDesktop':process.name,'desktopSwitched':False,'engineSha256':hashlib.sha256(exe.read_bytes()).hexdigest(),'fixtureSha256':hashlib.sha256(dll.read_bytes()).hexdigest(),'fixtureSourceSha256':hashlib.sha256(SOURCE.read_bytes()).hexdigest(),'uiFixtureSha256':hashlib.sha256((game/'uix86.dll').read_bytes()).hexdigest(),'uiFixtureSourceSha256':hashlib.sha256(UI_SOURCE.read_bytes()).hexdigest(),'commit':subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),'markers':[l for l in log.splitlines() if 'Q3 native probe:' in l or 'Q3 UI native probe:' in l]}
    (out/'manifest.json').write_text(json.dumps(data,indent=2)+'\n')
   self.assertIsNone(error,log[-4000:]);self.assertEqual(code,0,log[-4000:]);self.assertFalse(forced)
   self.assertIn('VM_LoadDll(cgame) found legacy vmMain',log)
   for marker in ['glconfig','gamestate','usercmd','snapshot','frame','command']:self.assertIn('Q3 native probe: '+marker+' ok',log,log[-4000:])
   self.assertIn('VM_LoadDll(ui) found legacy vmMain',log)
   for marker in ['api','glconfig','clientstate','configstring','command','shutdown']:self.assertIn('Q3 UI native probe: '+marker+' ok',log,log[-4000:])
   self.assertFalse(any('FAILED' in line for line in log.splitlines() if 'Q3 native probe:' in line or 'Q3 UI native probe:' in line),log[-4000:])
if __name__=='__main__':unittest.main()
