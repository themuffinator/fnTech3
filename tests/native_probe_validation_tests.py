"""Check that the native ABI probe rejects malformed transfers."""
from pathlib import Path
import shutil, subprocess, tempfile, unittest
from compiler_support import compiler_target_args
ROOT=Path(__file__).resolve().parents[1]
class NativeProbeValidationTests(unittest.TestCase):
 def test_guards_and_configstring_validation(self):
  compiler=shutil.which('clang++') or shutil.which('g++')
  if not compiler:self.skipTest('C++ compiler required')
  s=(ROOT/'tests/fixtures/native_cgame_abi_probe.c').read_text()
  a=s.index('static unsigned int Probe_U32(');b=s.index('static void __cdecl Probe_Init',a)
  fixture='#include <cassert>\n#include <cstring>\n'+s[a:b]+r'''
void put(unsigned char*p,unsigned n){for(int i=0;i<4;++i)p[i]=static_cast<unsigned char>(n>>(8*i));}
int main(){
 unsigned char storage[20116];std::memset(storage,0xa5,sizeof(storage));
 unsigned char *state=storage+8;std::memset(state,0,20100);put(state+20096,1);
 assert(Probe_Guards(storage,20100) && Probe_GameStateValid(state));
 storage[7]=0;assert(!Probe_Guards(storage,20100));storage[7]=0xa5;
 storage[20108]=0;assert(!Probe_Guards(storage,20100));storage[20108]=0xa5;
 put(state+20096,0);assert(!Probe_GameStateValid(state));
 put(state+20096,16001);assert(!Probe_GameStateValid(state));
 put(state+20096,4);state[4097]='o';state[4098]='k';put(state+4092,1);
 assert(Probe_GameStateValid(state));
 put(state+4092,4);assert(!Probe_GameStateValid(state));
 put(state+4092,0xffffffffu);assert(!Probe_GameStateValid(state));
 put(state+4092,1);state[4099]='x';assert(!Probe_GameStateValid(state));
 state[4099]=0;state[4096]='x';assert(!Probe_GameStateValid(state));
 std::memset(state,0xa5,20100);state[0]=0;assert(!Probe_GameStateValid(state));
}
'''
  with tempfile.TemporaryDirectory() as folder:
   cpp=Path(folder)/'probe.cpp';exe=Path(folder)/'probe.exe';cpp.write_text(fixture)
   p=subprocess.run([compiler,*compiler_target_args(compiler),'-std=c++17',str(cpp),'-o',str(exe)],capture_output=True,text=True)
   self.assertEqual(p.returncode,0,p.stdout+p.stderr)
   p=subprocess.run([str(exe)],capture_output=True,text=True,timeout=10)
   self.assertEqual(p.returncode,0,p.stderr)
if __name__=='__main__':unittest.main()
