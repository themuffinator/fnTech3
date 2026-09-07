"""Execute UI/cgame pure-mode selection for each profile and startup mode."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
from compiler_support import compiler_target_args
ROOT = Path(__file__).resolve().parents[1]

class PureModuleSelectionTests(unittest.TestCase):
    def test_profile_modes(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++ compiler unavailable')
        functions=[]
        for filename,entry,module in [('cl_ui.cpp','void CL_InitUI( void )','VM_UI'),('cl_cgame.cpp','void CL_InitCGame( void )','VM_CGAME')]:
            source=(ROOT/'code/client'/filename).read_text()
            start=source.index('if ( cl_connectedToPureServer )',source.index(entry))
            opening=source.index('{',start); depth=1; end=opening+1
            while depth:
                depth += (source[end]=='{') - (source[end]=='}'); end+=1
            functions.append('int select_'+module+'(int interpret) {\n'+source[start:end]+'\nreturn interpret;\n}')
        harness=r'''
#include <cassert>
enum {VMI_NATIVE, VMI_BYTECODE, VMI_COMPILED, VMI_PINNED_NATIVE, VM_UI, VM_CGAME};
bool q3, pinned, cl_connectedToPureServer;
int queries;
bool Com_GameIsQuake3() {return q3;}
bool VM_HasPinnedNativeModule(int) {++queries;return pinned;}
'''+ '\n'.join(functions)+r'''
int main() {
 for(int profile=0;profile<2;++profile)
 for(int pure=0;pure<2;++pure)
 for(int pin=0;pin<2;++pin)
 for(int mode=VMI_NATIVE;mode<=VMI_PINNED_NATIVE;++mode)
 for(auto select : {select_VM_UI,select_VM_CGAME}) {
  q3=profile;cl_connectedToPureServer=pure;pinned=pin;queries=0;
  int expected=mode;
  if(pure && q3) expected=VMI_COMPILED;
  else if(pure && mode!=VMI_COMPILED && mode!=VMI_BYTECODE)
   expected=pin ? VMI_PINNED_NATIVE : VMI_COMPILED;
  assert(select(mode)==expected);
  assert(queries==(pure && !q3 && mode!=VMI_COMPILED && mode!=VMI_BYTECODE ? 1:0));
 }
}
'''
        harness='#include <initializer_list>\n'+harness
        with tempfile.TemporaryDirectory() as folder:
            cpp=Path(folder)/'probe.cpp';exe=Path(folder)/'probe.exe'
            cpp.write_text(harness)
            subprocess.run([compiler,*compiler_target_args(compiler),'-std=c++17',str(cpp),'-o',str(exe)],check=True,capture_output=True,text=True)
            subprocess.run([str(exe)],check=True,capture_output=True,text=True)

if __name__=='__main__': unittest.main()
