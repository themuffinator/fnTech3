"""Execute production Q3/QL renderer configuration marshalling."""
from pathlib import Path
import os, shutil, subprocess, tempfile, unittest
ROOT=Path(__file__).resolve().parents[1]
class GlconfigBoundaryTests(unittest.TestCase):
 def test_profile_layout_values_guards_and_output_dimensions(self):
  compiler=shutil.which('clang++') or shutil.which('g++')
  if not compiler:self.skipTest('requires C++ compiler')
  # An x64 shell builds the fixture as the -Dq3-only=true engine is built.
  x64=os.name=="nt" and os.environ.get("VSCMD_ARG_TGT_ARCH","").lower()=="x64"
  target=(["-DFNTECH3_Q3_ONLY"] if x64 else ["--target=i686-pc-windows-msvc"]
   if os.name=="nt" and "clang" in Path(compiler).name else ["-m32"])
  source=(ROOT/'code/client/cl_cgame.cpp').read_text()
  def function(signature):
   start=source.index(signature);return source[start:source.index('\n}',start)+2]
  harness=r'''
#include "qcommon/q_shared.h"
#include "renderercommon/tr_types.h"
#include "compat/q3_game_abi.h"
#include "compat/ql_game_abi.h"
#include <cassert>
#include <cstring>
// Scalar encodings from the pinned renderer headers are part of both ABIs.
static_assert(GLDRV_ICD==0 && GLDRV_STANDALONE==1 && GLDRV_VOODOO==2,"driver ABI");
static_assert(GLHW_GENERIC==0 && GLHW_3DFX_2D3D==1 && GLHW_RIVA128==2 && GLHW_RAGEPRO==3 && GLHW_PERMEDIA2==4,"hardware ABI");
static_assert(TC_NONE==0 && TC_S3TC==1 && TC_S3TC_ARB==2,"compression ABI");
static_assert(qfalse==0 && qtrue==1,"boolean ABI");
struct {glconfig_t glconfig; int captureWidth,captureHeight;} cls;
using qlRetailGlconfig_t=fntech3_ql_glconfig_t;
void Q_strncpyz(char*d,const char*s,int n){assert(n>0);std::strncpy(d,s,n);d[n-1]=0;}
''' + function('void CL_CopyQ3Glconfig(') + '\n' + function('void CL_CopyRetailGlconfig(') + r'''
int main(){
 auto &s=cls.glconfig;
 std::memset(&s,0,sizeof(s));
 std::memset(s.renderer_string,'r',sizeof(s.renderer_string));
 std::memset(s.vendor_string,'v',sizeof(s.vendor_string));
 std::memset(s.version_string,'n',sizeof(s.version_string));
 std::memset(s.extensions_string,'e',sizeof(s.extensions_string));
 s.renderer_string[1023]=s.vendor_string[1023]=s.version_string[1023]=s.extensions_string[8191]=0;
 s.maxTextureSize=8192;s.numTextureUnits=4;s.colorBits=24;s.depthBits=32;s.stencilBits=8;
 s.driverType=GLDRV_ICD;s.hardwareType=GLHW_GENERIC;s.deviceSupportsGamma=qtrue;
 s.textureCompression=TC_NONE;s.textureEnvAddAvailable=qtrue;
 s.vidWidth=1280;s.vidHeight=960;s.windowAspect=4.0f/3.0f;
 s.displayFrequency=144;s.isFullscreen=qfalse;s.stereoEnabled=qtrue;s.smpActive=qtrue;
 struct {unsigned before;fntech3_q3_glconfig_t value;unsigned after;} q3;
 struct {unsigned before;fntech3_ql_glconfig_t value;unsigned after;} ql;
 std::memset(&q3,0xcc,sizeof(q3));std::memset(&ql,0xcc,sizeof(ql));
 CL_CopyQ3Glconfig(&q3.value,&s);
 // Every current Q3 byte must remain identical to its former direct copy.
 static_assert(sizeof(s)==sizeof(q3.value),"fixture baseline changed");
 assert(std::memcmp(&q3.value,&s,sizeof(s))==0);
 assert(q3.before==0xccccccccu && q3.after==0xccccccccu);
 for(int units:{1,4}){
  s.numTextureUnits=units;
  for(int output:{0,640}){
   cls.captureWidth=output;cls.captureHeight=output?480:0;
   CL_CopyRetailGlconfig(&ql.value);
   assert(ql.before==0xccccccccu && ql.after==0xccccccccu);
   assert(ql.value.maxActiveTextures==units && ql.value.multitextureAvailable==(units>1));
   assert(ql.value.vidWidth==(output?640:1280) && ql.value.vidHeight==(output?480:960));
   assert(ql.value.windowAspect==s.windowAspect && ql.value.displayFrequency==144);
   assert(ql.value.isFullscreen==0 && ql.value.stereoEnabled==1);
   assert(std::memcmp(ql.value.renderer_string,s.renderer_string,1024)==0);
   assert(std::memcmp(ql.value.vendor_string,s.vendor_string,1024)==0);
   assert(std::memcmp(ql.value.version_string,s.version_string,1024)==0);
   assert(std::memcmp(ql.value.extensions_string,s.extensions_string,8192)==0);
   assert(ql.value.maxTextureSize==8192 && ql.value.colorBits==24 && ql.value.depthBits==32 && ql.value.stencilBits==8);
   assert(ql.value.driverType==s.driverType && ql.value.hardwareType==s.hardwareType);
   assert(ql.value.deviceSupportsGamma==1 && ql.value.textureCompression==TC_NONE && ql.value.textureEnvAddAvailable==1);
  }
 }
 CL_CopyQ3Glconfig(nullptr,&s);CL_CopyQ3Glconfig(&q3.value,nullptr);CL_CopyRetailGlconfig(nullptr);
}
'''
  harness=harness.replace('#include <cassert>','#include <cassert>\n#include <initializer_list>')
  with tempfile.TemporaryDirectory() as folder:
   cpp=Path(folder)/'probe.cpp';exe=Path(folder)/'probe.exe';cpp.write_text(harness)
   build=subprocess.run([compiler,*target,'-std=c++17','-I',str(ROOT/'code'),str(cpp),'-o',str(exe)],capture_output=True,text=True)
   self.assertEqual(build.returncode,0,build.stdout+build.stderr)
   run=subprocess.run([str(exe)],capture_output=True,text=True,timeout=10)
   self.assertEqual(run.returncode,0,run.stderr)
if __name__=='__main__':unittest.main()
