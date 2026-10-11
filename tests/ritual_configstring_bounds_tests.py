"""Run the Ritual game imports at Alice's and F.A.K.K.2's own bounds.

Retail engines check 1,328 and 1,200 configstrings in their game accessors.
The shared engine's 2,736-entry storage is an internal capacity. These tests
execute the production wrappers, including their light-style offset, and
optionally compare their native guard facts against the legitimate binaries.
"""
from pathlib import Path
import hashlib
import os
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args
from rtcw_server_world_tests import function

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'code/server/server_alice_game.hpp'

PRELUDE = r'''
#include "code/qcommon/q_shared.h"
#include "code/compat/ritual_layout.hpp"
#include <array>
#include <cassert>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <limits>
#include <string>
static const fntech3::ritual::ServerLayout *layout;
const fntech3::ritual::ServerLayout &SV_RitualLayout(){return *layout;}
const char *SV_RitualTitle(){return layout->title;}
struct {const char *configstrings[MAX_GAME_CONFIGSTRINGS];} sv{};
static std::array<std::string, MAX_GAME_CONFIGSTRINGS> stored;
static int writes, lastWrite;
struct Drop{std::string message;};
void QDECL Com_Error(errorParm_t level,const char *format,...){
 assert(level==ERR_DROP);char text[1024];va_list args;va_start(args,format);
 vsnprintf(text,sizeof(text),format,args);va_end(args);throw Drop{text};
}
// Only check the internal capacity here: the production module boundary must
// reject its own title's first invalid index before reaching the shared writer.
void SV_SetConfigstring(int index,const char *value){
 assert(index>=0&&index<MAX_GAME_CONFIGSTRINGS);
 ++writes;lastWrite=index;stored[index]=value;sv.configstrings[index]=stored[index].c_str();
}
'''

CHECKS = r'''
template<class Call> void refused(Call call,const char *operation){
 const auto before=stored;const int previousWrites=writes;bool dropped=false;
 try{call();}catch(const Drop &drop){
  dropped=true;assert(drop.message.find(layout->title)!=std::string::npos);
  assert(drop.message.find(operation)!=std::string::npos);
 }
 assert(dropped&&writes==previousWrites&&stored==before);
}
int main(){
 static_assert(MAX_GAME_CONFIGSTRINGS>FNTECH3_ALICE_MAX_CONFIGSTRINGS);
 static_assert(int(FNTECH3_ALICE_MAX_CONFIGSTRINGS)>int(FNTECH3_FAKK2_MAX_CONFIGSTRINGS));
 for(const auto *selected:{&fntech3::ritual::AliceServerLayout(),&fntech3::ritual::FAKK2ServerLayout()}){
  layout=selected;stored={};writes=0;lastWrite=-1;
  for(int i=0;i<MAX_GAME_CONFIGSTRINGS;++i)sv.configstrings[i]="internal sentinel";
  const int limit=layout->maxConfigstrings;
  for(int index:{0,limit-1}){
   SV_AliceSetConfigstring(index,"kept");assert(lastWrite==index);
   assert(SV_AliceGetConfigstring(index)==sv.configstrings[index]);
   assert(!strcmp(SV_AliceGetConfigstring(index),"kept"));
   SV_AliceSetConfigstring(index,nullptr);assert(!*SV_AliceGetConfigstring(index));
   sv.configstrings[index]=nullptr;assert(!*SV_AliceGetConfigstring(index));
  }
  for(int index:{std::numeric_limits<int>::min(),-1,limit,limit+1,
                MAX_GAME_CONFIGSTRINGS-1,MAX_GAME_CONFIGSTRINGS,std::numeric_limits<int>::max()}){
   refused([&](){(void)SV_AliceGetConfigstring(index);},"read configstring");
   refused([&](){SV_AliceSetConfigstring(index,"forbidden");},"set configstring");
  }
  // 1,200 is a legal Alice slot and the first invalid F.A.K.K.2 slot.
  if(limit==FNTECH3_ALICE_MAX_CONFIGSTRINGS){
   SV_AliceSetConfigstring(FNTECH3_FAKK2_MAX_CONFIGSTRINGS,"Alice-only slot");
   assert(!strcmp(SV_AliceGetConfigstring(FNTECH3_FAKK2_MAX_CONFIGSTRINGS),"Alice-only slot"));
  }
  const int styles=limit-layout->csLightStyles;
  for(int style:{0,styles-1}){
   SV_AliceSetLightStyle(style,"abc");assert(lastWrite==layout->csLightStyles+style);
   assert(!strcmp(SV_AliceGetConfigstring(lastWrite),"abc"));
   SV_AliceSetLightStyle(style,nullptr);assert(!*SV_AliceGetConfigstring(lastWrite));
  }
  for(int style:{std::numeric_limits<int>::min(),-1,styles,styles+1,
                 std::numeric_limits<int>::max()-layout->csLightStyles,std::numeric_limits<int>::max()})
   refused([&](){SV_AliceSetLightStyle(style,"forbidden");},"set light style");
 }
}
'''


class RitualConfigstringBoundsTests(unittest.TestCase):
    def test_production_imports_keep_each_titles_bounds_and_reject_overflow(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++ compiler unavailable')
        source = SOURCE.read_text(encoding='utf-8')
        bodies = '\n'.join(function(source, signature) for signature in (
            'void QDECL SV_AliceSetConfigstring(',
            'const char *QDECL SV_AliceGetConfigstring(',
            'void QDECL SV_AliceSetLightStyle('))
        scratch = ROOT / '.tmp/ubertools-round-2026-10-02/ritual-contracts'
        scratch.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='configstring-', dir=scratch) as folder:
            cpp = Path(folder) / 'probe.cpp'
            cpp.write_text(PRELUDE + bodies + CHECKS, encoding='utf-8')
            binary = Path(folder) / ('probe.exe' if os.name == 'nt' else 'probe')
            built = subprocess.run([compiler, *compiler_target_args(compiler), '-std=c++17',
                                    '-O2', '-Wall', '-Wextra', '-Werror', '-I', str(ROOT),
                                    str(cpp), '-o', str(binary)], cwd=ROOT,
                                   capture_output=True, text=True, timeout=180)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            options = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
            ran = subprocess.run([str(binary)], cwd=ROOT, capture_output=True,
                                 text=True, timeout=60, **options)
            self.assertEqual(ran.returncode, 0, ran.stdout + ran.stderr)

    def native_guards(self, path, sha256, pinned):
        if not path.is_file():
            self.skipTest(f'legitimate reference binary unavailable: {path}')
        import pefile
        self.assertEqual(hashlib.sha256(path.read_bytes()).hexdigest(), sha256)
        image = pefile.PE(str(path), fast_load=True)
        data = image.get_memory_mapped_image()
        base = image.OPTIONAL_HEADER.ImageBase
        for address, expected in pinned.items():
            expected = bytes.fromhex(expected)
            self.assertEqual(data[address-base:address-base+len(expected)], expected, hex(address))

    def test_alice_retail_accessors_and_light_style_offset(self):
        self.native_guards(Path(os.environ.get('FNTECH3_ALICE_EXE', r'E:\Games\Alice\alice.exe')),
                           '8aa22917b7707f0e1a7c4dbe9a3c38332b1873ae2059174ac5329de26921fc18', {
            0x00454a57: '85 db 7c 08 81 fb 30 05 00 00 7c 10',  # set: nonnegative and < 1328
            0x00454b05: '85 f6 7c 08 81 fe 30 05 00 00 7c 10',  # get: the same bound
            0x00454c88: '81 c1 60 03 00 00 50 51 e8 bb fd ff ff',  # style + 864 -> setter
        })

    def test_fakk2_retail_accessors_and_light_style_offset(self):
        self.native_guards(Path(os.environ.get('FNTECH3_FAKK2_EXE', str(
            ROOT / '.tmp/fakk2-patch/files/Program Executable Files/fakk2.exe'))),
                           '681bc51c3343d01d772c5ae4aae7523ae4547350b243c875156e138043335785', {
            0x004458c7: '85 db 7c 08 81 fb b0 04 00 00 7c 10',  # set: nonnegative and < 1200
            0x00445975: '85 f6 7c 08 81 fe b0 04 00 00 7c 10',  # get: the same bound
            0x00445af8: '81 c1 e0 02 00 00 50 51 e8 bb fd ff ff',  # style + 736 -> setter
        })


if __name__ == '__main__':
    unittest.main()
