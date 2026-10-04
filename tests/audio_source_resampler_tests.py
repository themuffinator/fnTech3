"""Exercise source-quality selection and production optional-API fallback."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]


class SourceResamplerTests(unittest.TestCase):
    def compile_run(self, code):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++ compiler unavailable')
        with tempfile.TemporaryDirectory() as directory:
            cpp = Path(directory) / 'source.cpp'
            exe = Path(directory) / 'source.exe'
            cpp.write_text(code)
            built = subprocess.run([compiler, *compiler_target_args(compiler), '-std=c++17',
                                    '-Wall', '-Wextra', '-Werror', '-I', str(ROOT),
                                    str(cpp), '-o', str(exe)], capture_output=True, text=True)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            ran = subprocess.run([str(exe)], capture_output=True, text=True)
            self.assertEqual(ran.returncode, 0, ran.stdout + ran.stderr)

    def test_runtime_order_names_and_quality_tiers(self):
        self.compile_run(r'''
#include "code/client/audio/shared/AudioSourceResampler.h"
#include <cassert>
#include <algorithm>
using fntech3::audio::SelectSourceResampler;
int main() {
 const std::vector<std::string> names={"Nearest","Cubic Spline","11th order Sinc (fast)",
  "11th order Sinc","23rd order Sinc (fast)","23rd order Sinc","47th order Sinc"};
 assert(SelectSourceResampler(names,"fast")==3);
 assert(SelectSourceResampler(names,"balanced")==5);
 assert(SelectSourceResampler(names,"BEST")==6);
 assert(SelectSourceResampler(names,nullptr)==5);
 assert(SelectSourceResampler(names,"library")==-1);
 assert(SelectSourceResampler(names,"unknown")==-1);
 auto reversed=names;std::reverse(reversed.begin(),reversed.end());
 assert(reversed[size_t(SelectSourceResampler(reversed,"balanced"))]=="23rd order Sinc");
 assert(SelectSourceResampler({"BSinc12","24-point Sinc","48-point Sinc"},"balanced")==1);
 assert(SelectSourceResampler({"48-point Sinc","32-point Sinc"},"balanced")==1);
 assert(SelectSourceResampler({"12-point Sinc","8-point Sinc"},"balanced")==0);
 assert(SelectSourceResampler({"Cubic Spline","Linear","Gaussian","Sinc","9999999999999-point Sinc",
  "0-point Sinc","1-point Sinc",std::string(300,'1')+" Sinc"},"best")==-1);
 assert(SelectSourceResampler(std::vector<std::string>(129,"24-point Sinc"),"balanced")==-1);
 assert(SelectSourceResampler({},"balanced")==-1);
}
''')

    def test_production_probe_failures_reset_and_source_application(self):
        source = (ROOT / 'code/client/audio/openal/AudioSystemOpenAL.inl').read_text()
        functions = source[source.index('void OpenALDevice::DiscoverSourceResamplers() {'):
                           source.index('void OpenALDevice::PrintCapabilityMatrix() const {')]
        reset = source[source.index('void OpenALDevice::ResetSource( ALuint source ) const {'):
                       source.index('bool OpenALDevice::InitEFX() {')]
        self.assertIn('SetSourceResampler( source );', reset)
        self.compile_run(r'''
#include "code/client/audio/shared/AudioSourceResampler.h"
#include <cassert>
#include <cstring>
using ALint=int;using ALenum=int;using ALsizei=int;using ALuint=unsigned;using ALchar=char;
using LPALGETSTRINGISOFT=const ALchar*(*)(ALenum,ALsizei);
constexpr int AL_NO_ERROR=0,AL_NUM_RESAMPLERS_SOFT=1,AL_DEFAULT_RESAMPLER_SOFT=2,
 AL_RESAMPLER_NAME_SOFT=3,AL_SOURCE_RESAMPLER_SOFT=4;
#define S_COLOR_YELLOW ""
int error=0,allocations=0,deletions=0,sets=0,warnings=0,lastIndex=-1;
int count=3,defaultIndex=0,brokenName=-1;
bool queryFail=false,allocationFail=false,setFail=false,readFail=false,procMissing=false;
std::vector<std::string> names={"Cubic Spline","11th order Sinc","23rd order Sinc"};
struct Cvar {const char*string="balanced";} request;
Cvar*s_alSourceResampler=&request;
const char*CvarStringOrDefault(Cvar*value,const char*fallback){return value?value->string:fallback;}
int Q_stricmp(const char*a,const char*b){return fntech3::audio::SourceResamplerLower(a).compare(fntech3::audio::SourceResamplerLower(b));}
template<class...Args>void Com_Printf(const char*,Args...){++warnings;}
template<class...Args>void Com_DPrintf(const char*,Args...){++warnings;}
const ALchar*GetStringi(ALenum property,ALsizei index){assert(property==AL_RESAMPLER_NAME_SOFT);
 if(index==brokenName){return nullptr;}
 return names[size_t(index)].c_str();}
struct Loader {
 bool ready=true;bool Ready()const{return ready;}
 int alGetError()const{const int result=error;error=0;return result;}
 void alGetIntegerv(int property,int*out)const{*out=property==AL_NUM_RESAMPLERS_SOFT?count:defaultIndex;if(queryFail)error=1;}
 void alGenSources(int n,ALuint*out)const{assert(n==1);++allocations;*out=9;if(allocationFail)error=1;}
 void alDeleteSources(int n,const ALuint*source)const{assert(n==1&&*source==9);++deletions;}
 void alSourcei(ALuint source,int property,int index)const{assert((source==9||source==7)&&property==AL_SOURCE_RESAMPLER_SOFT);
  ++sets;lastIndex=index;if(setFail)error=1;}
 void alGetSourcei(ALuint source,int property,int*out)const{assert(source==9&&property==AL_SOURCE_RESAMPLER_SOFT);*out=lastIndex;if(readFail)error=1;}
};
struct OpenALDevice {
 Loader loader_;bool extension=true;
 struct {bool sourceResampler=false;int sourceResamplerIndex=-1,defaultSourceResamplerIndex=-1;
  std::string sourceResamplerName,defaultSourceResamplerName;LPALGETSTRINGISOFT alGetStringiSOFT=nullptr;}capabilities_;
 bool HasALExtension(const char*)const{return extension;}
 void*GetALProcAddress(const char*)const{return procMissing?nullptr:reinterpret_cast<void*>(&GetStringi);}
 void DiscoverSourceResamplers();void SetSourceResampler(ALuint)const;
};
''' + functions + r'''
int main() {
 OpenALDevice device;
 device.DiscoverSourceResamplers();assert(device.capabilities_.sourceResamplerIndex==2);
 assert(device.capabilities_.sourceResamplerName==names[2]&&device.capabilities_.defaultSourceResamplerName==names[0]);
 assert(allocations==1&&deletions==1&&sets==1&&error==0);
 sets=0;device.SetSourceResampler(7);assert(sets==1&&lastIndex==2);
 device.SetSourceResampler(0);assert(sets==1);
 device.loader_.ready=false;device.SetSourceResampler(7);assert(sets==1);device.loader_.ready=true;
 setFail=true;device.SetSourceResampler(7);assert(error==0&&warnings==1);setFail=false;
 for(int failure=0;failure<10;++failure) {
  count=3;defaultIndex=0;queryFail=allocationFail=setFail=readFail=procMissing=false;brokenName=-1;device.extension=true;
  switch(failure) {
   case 0:device.extension=false;break;case 1:procMissing=true;break;case 2:count=129;break;
   case 3:count=0;break;case 4:defaultIndex=3;break;case 5:queryFail=true;break;
   case 6:brokenName=1;break;case 7:allocationFail=true;break;case 8:setFail=true;break;case 9:readFail=true;break;
  }
  device.DiscoverSourceResamplers();assert(device.capabilities_.sourceResamplerIndex==-1);
  assert(device.capabilities_.sourceResamplerName.empty()&&allocations==deletions&&error==0);
  const int previousSets=sets;device.SetSourceResampler(7);
  if(device.capabilities_.defaultSourceResamplerIndex>=0){assert(sets==previousSets+1&&lastIndex==0);}
  else{assert(sets==previousSets);}
  assert(error==0);
 }
 count=3;defaultIndex=0;queryFail=allocationFail=setFail=readFail=procMissing=false;brokenName=-1;device.extension=true;
 request.string="library";const int previousAllocations=allocations;device.DiscoverSourceResamplers();
 assert(device.capabilities_.sourceResamplerIndex==-1&&allocations==previousAllocations);
 assert(device.capabilities_.defaultSourceResamplerName==names[0]);
 device.SetSourceResampler(7);assert(lastIndex==0); // Reused sources restore the library choice.
 request.string="best";device.DiscoverSourceResamplers();assert(device.capabilities_.sourceResamplerIndex==2);
 request.string="fast";device.DiscoverSourceResamplers();assert(device.capabilities_.sourceResamplerIndex==1);
 assert(allocations==deletions);
}
''')


if __name__ == '__main__':
    unittest.main()
