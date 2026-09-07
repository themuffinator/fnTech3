"""Exercise real filesystem marker probing, including invalid path encodings."""
from pathlib import Path
import shutil, subprocess, tempfile, unittest
from compiler_support import compiler_target_args
ROOT=Path(__file__).resolve().parents[1]
class ProfileMarkerFilesystemTests(unittest.TestCase):
    def test_filesystem_entries_and_unrepresentable_paths(self):
        compiler=shutil.which('clang++') or shutil.which('g++')
        if not compiler:self.skipTest('C++ compiler unavailable')
        harness=r'''
#include "game_profile.h"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>
int main(int argc,char **argv) {
 assert(argc==2);
 const std::filesystem::path root=std::filesystem::u8path(argv[1]);
 auto probe=[&](const std::filesystem::path &p){return Com_ProbeGameProfileMarkers(p.u8string().c_str());};
 assert(Com_ProbeGameProfileMarkers(nullptr)==0 && Com_ProbeGameProfileMarkers("")==0);
 assert(probe(root)==0);
 std::filesystem::create_directories(root/"baseq3"/"pak0.pk3");
 assert(probe(root)==0); // A directory is not an installation marker.
 std::filesystem::remove(root/"baseq3"/"pak0.pk3");
 std::ofstream(root/"baseq3"/"pak0.pk3").put('x');
 assert(probe(root)==COM_GAME_MARKER_QUAKE3);
 std::ofstream(root/"baseq3"/"pak00.pk3").put('x');
 assert(probe(root)==(COM_GAME_MARKER_QUAKE3|COM_GAME_MARKER_QUAKELIVE));
 std::filesystem::remove(root/"baseq3"/"pak0.pk3");
 assert(probe(root)==COM_GAME_MARKER_QUAKELIVE);
 auto unicode=root/std::filesystem::u8path("caf\xc3\xa9");
 std::filesystem::create_directories(unicode/"baseq3");
 std::ofstream(unicode/"baseq3"/"pak0.pk3").put('x');
 assert(probe(unicode)==COM_GAME_MARKER_QUAKE3);
 // Unrepresentable/missing paths are absent evidence, not exceptions across C ABI.
 const std::string invalid=root.u8string()+"/\xff\xfe";
 assert(Com_ProbeGameProfileMarkers(invalid.c_str())==0);
 const std::string huge(40000,'x');
 assert(Com_ProbeGameProfileMarkers(huge.c_str())==0);
}
'''
        with tempfile.TemporaryDirectory() as folder:
            cpp=Path(folder)/'probe.cpp';exe=Path(folder)/'probe.exe';fixture=Path(folder)/'fixture';fixture.mkdir()
            cpp.write_text(harness)
            subprocess.run([compiler,*compiler_target_args(compiler),'-std=c++17','-I',str(ROOT/'code/qcommon'),str(cpp),str(ROOT/'code/qcommon/game_profile.cpp'),'-o',str(exe)],check=True,capture_output=True,text=True)
            run=subprocess.run([str(exe),str(fixture)],capture_output=True,text=True)
            self.assertEqual(run.returncode,0,run.stdout+run.stderr)
if __name__=='__main__':unittest.main()
