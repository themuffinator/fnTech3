"""Run the shared TGA decoder with Academy's measured row orientation."""

import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args


ROOT = Path(__file__).resolve().parents[1]

FIXTURE = r'''
#include <cassert>
#include <cstring>
#include <vector>
#include "code/qcommon/q_shared.h"
#include "code/renderercommon/tr_public.h"
refimport_t ri{};
static std::vector<byte> input;
static std::vector<byte> output;
static bool fileAlive;
static int warnings,allocations;
static int Read(const char *,void **buffer) {
    assert(!fileAlive);fileAlive=true;*buffer=input.data();return int(input.size());
}
static void FreeFile(void *buffer) { assert(fileAlive && buffer==input.data());fileAlive=false; }
static void *Allocate(int length) {
    assert(length>0 && allocations==0);++allocations;
    output.assign(size_t(length)+32,0xa5);return output.data()+16;
}
NORETURN static void QDECL Error(errorParm_t,const char *,...) { throw 1; }
static void QDECL Print(printParm_t,const char *,...) { ++warnings; }
#include "code/renderercommon/tr_image_tga.c"
static void Header(int type,int bits,int attributes) {
    assert(!fileAlive);input.assign(18,0);output.clear();allocations=0;warnings=0;
    input[2]=byte(type);input[12]=2;input[14]=2;input[16]=byte(bits);input[17]=byte(attributes);
}
static void Pixel(int bits,byte value) {
    if(bits==8)input.push_back(value);
    else {
        input.push_back(0);input.push_back(0);input.push_back(value);
        if(bits==32)input.push_back(255);
    }
}
static void Decode(const std::vector<byte> &values) {
    byte *pixels=reinterpret_cast<byte *>(1);int width=-1,height=-1;
    R_LoadTGA("synthetic.tga",&pixels,&width,&height);
    assert(width==2 && height==2 && pixels==output.data()+16 && !fileAlive && allocations==1);
    for(size_t i=0;i<values.size();++i)assert(pixels[i*4]==values[i] && pixels[i*4+3]==255);
    for(int i=0;i<16;++i)assert(output[i]==0xa5 && output[output.size()-1-i]==0xa5);
    allocations=0;
}
static void Reject() {
    byte *pixels=reinterpret_cast<byte *>(1);int width=-1,height=-1;bool rejected=false;
    try {R_LoadTGA("invalid.tga",&pixels,&width,&height);}catch(int) {rejected=true;}
    assert(rejected && !pixels && width==0 && height==0);
    if(!output.empty())for(int i=0;i<16;++i)assert(output[i]==0xa5 && output[output.size()-1-i]==0xa5);
    // Model Com_Error's hunk/renderer cleanup before the next initialization.
    fileAlive=false;output.clear();allocations=0;
}
int main() {
    ri.FS_ReadFile=Read;ri.FS_FreeFile=FreeFile;ri.Malloc=Allocate;ri.Error=Error;ri.Printf=Print;
    for(auto profile:{COM_GAME_JKA_SP,COM_GAME_JKA_MP,COM_GAME_QUAKE3,COM_GAME_QUAKELIVE,
        COM_GAME_RTCW_SP,COM_GAME_RTCW_MP,COM_GAME_WOLFET,COM_GAME_STVEF_SP,COM_GAME_JK2_MP}) {
        ri.gameId=profile;const bool jka=profile==COM_GAME_JKA_SP || profile==COM_GAME_JKA_MP;
        for(int bits:{8,24,32})for(int attributes:{0,8,0x20,0x28}) {
            Header(bits==8?3:2,bits,attributes);
            for(byte value:{1,2,3,4})Pixel(bits,value);
            Decode(jka && (attributes&0x20)?std::vector<byte>{1,2,3,4}:std::vector<byte>{3,4,1,2});
            assert(warnings==((attributes&0x20) && !jka && profile!=COM_GAME_WOLFET?1:0));
        }
        for(int attributes:{0,0x10,0x20,0x30}) {
            Header(10,32,attributes);input.push_back(3);
            for(byte value:{1,2,3,4})Pixel(32,value);
            if(jka && attributes)Reject();else Decode({3,4,1,2});
        }
        Header(2,32,0x20);Pixel(32,1);Reject();
        Header(2,32,0x20);input[12]=0;Reject();
        Header(2,32,0x20);input[0]=255;Reject();
        Header(10,32,0);input.push_back(0x83);Pixel(32,9);Decode({9,9,9,9});
    }
}
'''


class JkaTgaOriginTests(unittest.TestCase):
    def test_decoder_orientation_bounds_and_profile_isolation(self):
        compiler = shutil.which("clang++") or shutil.which("g++")
        if compiler is None:
            self.skipTest("requires a C++17 compiler")
        scratch = ROOT / ".tmp"
        scratch.mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(prefix="jka-tga-origin-", dir=scratch) as folder:
            source = Path(folder) / "tga.cpp"
            executable = Path(folder) / ("tga.exe" if os.name == "nt" else "tga")
            source.write_text(FIXTURE, encoding="utf-8")
            build = subprocess.run([compiler, *compiler_target_args(compiler), "-std=c++17", "-O2",
                "-Wall", "-Wextra", "-Werror", "-Wno-sign-compare", "-I", str(ROOT), str(source),
                "-o", str(executable)], cwd=ROOT, text=True, capture_output=True, timeout=60)
            self.assertEqual(build.returncode, 0, build.stdout + build.stderr)
            run = subprocess.run([str(executable)], cwd=ROOT, text=True, capture_output=True, timeout=30)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)


if __name__ == "__main__":
    unittest.main()
