"""ET profile isolation and original-engine synthetic packet regressions."""
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
from types import SimpleNamespace
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'scripts'))
import probe_rtcw_messages as messages

PROFILE = r'''
#include "code/qcommon/game_manager.h"
#include <cassert>
#include <cstring>
int main() {
    const auto *et = Com_GameManagerFindProfile("WolfET");
    assert(et && et->id == COM_GAME_WOLFET && et->titleId == COM_GAME_TITLE_WOLFET);
    assert(et == Com_GameManagerFindProfile("et") && et == Com_GameManagerFindProfile("enemy-territory"));
    assert(!std::strcmp(et->baseGame,"etmain") && !std::strcmp(et->steamAppId,"1873030"));
    assert(!et->usesSteam && !et->supportsWorkshop);
    assert(!std::strcmp(Com_GameManagerConfigFile(et,0),"etconfig.cfg"));
    assert(!std::strcmp(Com_GameManagerConfigFile(et,1),"etconfig_server.cfg"));
    assert(et->maxClients == 64 && et->maxConfigstrings == 1024 && et->playerConfigstringBase == 689);
    assert(et->collisionModelHandleCount == 512);
    for (int protocol = 0; protocol < 100; ++protocol)
        assert(!!Com_GameProfileAllowsProtocol(et,protocol) == (protocol == 84));
    assert(Com_GameProfileAllowsModuleApi(et,COM_MODULE_API_LEGACY_NATIVE));
    assert(!Com_GameProfileAllowsModuleApi(et,COM_MODULE_API_QVM));
    assert(!Com_GameProfileAllowsModuleApi(et,COM_MODULE_API_STRUCTURED_NATIVE));
    assert(Com_GameManagerBspFormat(et,47) == COM_GAME_BSP_WOLFET);
    assert(Com_GameManagerBspFormat(et,46) == COM_GAME_BSP_UNSUPPORTED);
    char name[64] = "unchanged";
    assert(Com_GameManagerNativeModuleFilename(et,"qagame","i386",".dll",0,name,64));
    assert(!std::strcmp(name,"qagame_mp_x86.dll"));
    assert(Com_GameManagerPureModuleFilename(et,0,"x86",".so",name,64));
    assert(!std::strcmp(name,"cgame.mp.i386.so"));
    assert(!Com_GameManagerNativeModuleFilename(et,"../ui","x86",".dll",0,name,64));
    assert(!std::strcmp(name,"cgame.mp.i386.so"));
    assert(Com_GameManagerInstallationMatches(et,COM_GAME_MARKER_WOLFET));
    assert(!Com_GameManagerInstallationMatches(et,COM_GAME_MARKER_WOLFET | COM_GAME_MARKER_RTCW_MP));
    comGameMarkerInputs_t mixed{"auto",COM_GAME_MARKER_WOLFET | COM_GAME_MARKER_QUAKE3,0,0};
    assert(Com_DetectGameProfileMarkers(&mixed).status == COM_GAME_DETECTION_AMBIGUOUS);
    mixed.basePathMarkers = COM_GAME_MARKER_WOLFET;
    assert(Com_DetectGameProfileMarkers(&mixed).profile == et);
    assert(Com_GameManagerCanActivate(et) && Com_ActivateGameProfile(COM_GAME_WOLFET));
    assert(Com_GameIsWolfET() && !Com_GameIsRTCW() && !Com_GameIsQuake3());
    assert(!Com_ActivateGameProfile(COM_GAME_QUAKE3));
    assert(Com_GameProfile() == et);

}
'''


class WolfETTests(unittest.TestCase):
    def test_immutable_profile_in_every_build(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler: self.skipTest('C++17 compiler unavailable')
        scratch = ROOT/'.tmp/wolfet'; scratch.mkdir(parents=True,exist_ok=True)
        with tempfile.TemporaryDirectory(dir=scratch) as temporary:
            folder = Path(temporary); source = folder/'profile.cpp'; source.write_text(PROFILE)
            options = {'creationflags':subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
            x64 = os.name == 'nt' and os.environ.get('VSCMD_ARG_TGT_ARCH', '').lower() == 'x64'
            target = ['--target=i686-pc-windows-msvc'] if os.name == 'nt' and 'clang' in compiler and not x64 else []
            for index,macros in enumerate(([], ['FNTECH3_RTCW_DEVELOPMENT'], ['FNTECH3_WOLFET_DEVELOPMENT'],
                    ['FNTECH3_WOLFET_DEVELOPMENT','DEDICATED'],
                    ['FNTECH3_WOLFET_DEVELOPMENT','DEDICATED','FNTECH3_Q3_ONLY'])):
                with self.subTest(macros=macros):
                    executable = folder/(f'profile-{index}.exe' if os.name == 'nt' else f'profile-{index}')
                    build = subprocess.run([compiler,*target,'-std=c++17','-I',str(ROOT),
                        *['-D'+m for m in macros+(['FNTECH3_Q3_ONLY'] if x64 and 'FNTECH3_Q3_ONLY' not in macros else [])],
                        str(source),str(ROOT/'code/qcommon/game_profile.cpp'),'-o',str(executable)],
                        cwd=ROOT,capture_output=True,text=True,timeout=90)
                    self.assertEqual(build.returncode,0,build.stdout+build.stderr)
                    result = subprocess.run([str(executable)],cwd=ROOT,capture_output=True,text=True,timeout=20,**options)
                    self.assertEqual(result.returncode,0,result.stdout+result.stderr)

    def test_reference_generated_packet_fixtures(self):
        if not shutil.which('clang++') or not shutil.which('clang'): self.skipTest('Clang unavailable')
        previous = messages.DRIVER, messages.TEMP_ROOT
        messages.DRIVER = ROOT/'tests/fixtures/wolfet_message_probe.cpp'
        messages.TEMP_ROOT = ROOT/'.tmp/wolfet'
        messages.TEMP_ROOT.mkdir(parents=True,exist_ok=True)
        try:
            with tempfile.TemporaryDirectory(dir=messages.TEMP_ROOT) as temporary:
                folder = Path(temporary)
                x64 = os.name == 'nt' and os.environ.get('VSCMD_ARG_TGT_ARCH', '').lower() == 'x64'
                args = SimpleNamespace(cc='clang',cxx='clang++',
                    target='i686-pc-windows-msvc' if os.name == 'nt' and not x64 else None)
                executable = messages.build(args,None,False,folder)
                corpus = json.loads((ROOT/'tests/fixtures/wolfet_message_vectors.json').read_text())
                for case in corpus:
                    for key in ('old','new','packet','decoded'): case[key] = bytes.fromhex(case[key])
                expected = [{key:case[key] for key in ('bits','packet','read_bits','decoded')} for case in corpus]
                inputs,outputs = folder/'input.bin',folder/'output.bin'
                messages.request_file(inputs,corpus)
                messages.run([str(executable),'encode',str(inputs),str(outputs)])
                messages.compare(expected,messages.read_replies(outputs),corpus)
                messages.request_file(inputs,corpus,expected)
                messages.run([str(executable),'decode',str(inputs),str(outputs)])
                messages.compare(expected,messages.read_replies(outputs),corpus)
        finally:
            messages.DRIVER, messages.TEMP_ROOT = previous


if __name__ == '__main__': unittest.main()
