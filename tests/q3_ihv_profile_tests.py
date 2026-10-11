"""IHV loose-install identity and root-module containment without executing DLLs."""
from pathlib import Path
import os
import shutil
import struct
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args
from profile_marker_filesystem_tests import metadata_pak

ROOT = Path(__file__).resolve().parents[1]


def module_metadata(symbol):
    data = bytearray(0x400)
    def word(at, value): struct.pack_into('<H', data, at, value)
    def dword(at, value): struct.pack_into('<I', data, at, value)
    word(0, 0x5a4d); dword(60, 0x80); dword(0x80, 0x4550)
    word(0x84, 0x14c); word(0x86, 1); word(0x94, 224); word(0x96, 0x2000)
    word(0x98, 0x10b); dword(0x98+60, 0x200); dword(0x98+92, 1)
    dword(0x98+96, 0x1100); dword(0x98+100, 0x80)
    dword(0x178+12, 0x1000); dword(0x178+16, 0x200); dword(0x178+20, 0x200)
    dword(0x300+20, 1); dword(0x300+24, 1)
    dword(0x300+28, 0x1140); dword(0x300+32, 0x1130); dword(0x300+36, 0x1138)
    dword(0x330, 0x1150); dword(0x340, 0x1000)
    name = symbol.encode()+b'\0'; data[0x350:0x350+len(name)] = name
    return bytes(data)


class IhvProfileFilesystemTests(unittest.TestCase):
    def test_loose_native_identity_and_containment(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler: self.skipTest('C++ compiler unavailable')
        harness = r'''
#include "game_manager.h"
#include "q3_ihv_install.hpp"
#include <cstdlib>
#include <cstring>
#define CHECK(x) do { if(!(x)) return __LINE__; } while(0)
int main(int argc,char **argv) {
 CHECK(argc==3);
 const auto expected=static_cast<unsigned>(std::strtoul(argv[2],nullptr,10));
 CHECK(Com_ProbeGameProfileMarkers(argv[1])==expected);
 const auto *ihv=Com_GameProfileById(COM_GAME_QUAKE3_IHV);
 CHECK(Com_GameManagerCanActivate(ihv));
 comGameMarkerInputs_t input{"auto",expected,0,0};
 if(expected==257) CHECK(Com_DetectGameProfileMarkers(&input).status==COM_GAME_DETECTION_AMBIGUOUS);
 if(expected==256) {
  CHECK(fntech3::q3ihv::install::Recognizes(std::filesystem::u8path(argv[1])));
  CHECK(Com_GameManagerInstallationMatches(ihv,expected));
  char path[4096]="retained";
  CHECK(Com_GameManagerResolveRootModule(ihv,argv[1],"qagamex86.dll","x86",".dll",path,sizeof(path)));
  CHECK(!Com_GameManagerResolveRootModule(ihv,argv[1],"../qagamex86.dll","x86",".dll",path,sizeof(path)));
  CHECK(!Com_GameManagerResolveRootModule(ihv,argv[1],"qagamex86.dll","x86_64",".dll",path,sizeof(path)));
 }
}
'''
        scratch = ROOT/'.tmp'; scratch.mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='ihv-identity-', dir=scratch) as folder:
            folder = Path(folder); source = folder/'probe.cpp'; exe = folder/'probe.exe'
            source.write_text(harness)
            subprocess.run([compiler, *compiler_target_args(compiler), '-std=c++17', '-I', str(ROOT/'code/qcommon'),
                            str(source), str(ROOT/'code/qcommon/game_profile.cpp'), '-o', str(exe)],
                           check=True, capture_output=True, text=True, cwd=ROOT)
            fixture = folder/'fixture'; maps = fixture/'baseq3/maps'; maps.mkdir(parents=True)
            def check(expected, installation=None):
                result = subprocess.run([str(exe), str(installation or fixture), str(expected)], capture_output=True, text=True, cwd=ROOT)
                self.assertEqual(result.returncode, 0, result.stdout+result.stderr+f' (expected marker {expected})')
            config = fixture/'baseq3/default.cfg'; config.write_text('// Synthetic config\n')
            qagame = fixture/'qagamex86.dll'; cgame = fixture/'cgamex86.dll'
            mapfile = maps/'ihv_test1.bsp'; bsp = b'IBSP'+struct.pack('<I', 43)+bytes(112)
            qa = module_metadata('GetGameAPI'); cg = module_metadata('GetCGameAPI')
            mapfile.write_bytes(bsp); check(0)
            qagame.write_bytes(qa); check(0)
            cgame.write_bytes(cg); check(256)
            config.write_bytes(b''); check(0); config.write_text('// Synthetic config\n')
            for malformed in (b'', bsp[:-1], bsp[:4]+struct.pack('<I', 46)+bsp[8:],
                              bsp[:8]+struct.pack('<II', 0xfffffff0, 32)+bsp[16:]):
                mapfile.write_bytes(malformed); check(0)
            mapfile.write_bytes(bsp)
            for malformed in (b'', b'MZ', qa[:-1], module_metadata('vmMain'), module_metadata('GetGameAPIExtra')):
                qagame.write_bytes(malformed); check(0)
            qagame.write_bytes(qa)
            cgame.write_bytes(module_metadata('GetGameAPI')); check(0); cgame.write_bytes(cg)
            config.unlink(); config.mkdir(); check(0); config.rmdir(); config.write_text('// Synthetic config\n')
            pak = fixture/'baseq3/pak0.pk3'; pak.write_bytes(metadata_pak('q3')); check(257); pak.unlink()
            check(256)
            outside = folder/'outside.dll'; outside.write_bytes(qa); qagame.unlink()
            try:
                os.symlink(outside, qagame)
            except OSError:
                pass # Hosts without symlink privilege still run all regular-file cases.
            else:
                try: check(0)
                finally: qagame.unlink()
            qagame.write_bytes(qa); check(256)
            if os.name == 'nt':
                # Junctions need no symlink privilege and expose MinGW's
                # canonical() retaining the link spelling on Windows.
                linked = folder/'linked-install'; linked.mkdir()
                (linked/'qagamex86.dll').write_bytes(qa)
                (linked/'cgamex86.dll').write_bytes(cg)
                junction = linked/'baseq3'
                for target, expected in ((folder/'outside-baseq3', 0), (linked/'kept-baseq3', 256)):
                    (target/'maps').mkdir(parents=True)
                    (target/'default.cfg').write_text('// Synthetic config\n')
                    (target/'maps/ihv_test1.bsp').write_bytes(bsp)
                    self.assertTrue(target.resolve().is_relative_to(folder.resolve()))
                    self.assertTrue(junction.parent.resolve().is_relative_to(folder.resolve()))
                    made = subprocess.run(['cmd', '/d', '/c', 'mklink', '/J', str(junction), str(target)],
                                          capture_output=True, text=True, creationflags=subprocess.CREATE_NO_WINDOW)
                    self.assertEqual(made.returncode, 0, made.stdout + made.stderr)
                    try:
                        self.assertTrue(os.lstat(junction).st_file_attributes & 0x400)
                        check(expected, linked)
                    finally:
                        # Remove only the verified junction, never its target.
                        os.rmdir(junction)


if __name__ == '__main__': unittest.main()
