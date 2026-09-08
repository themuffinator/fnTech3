"""Execute source routing against old and new OpenAL capability combinations."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]


class DirectChannelFallbackTests(unittest.TestCase):
    def test_missing_remix_never_requests_dropped_channels(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++ compiler unavailable')
        source = (ROOT / 'code/client/audio/openal/AudioSystemOpenAL.inl').read_text()
        function = source[source.index('void OpenALDevice::SetSourceDirectChannels'):source.index('void OpenALDevice::ConfigureSourceDistance')]
        code = r'''
#include <cassert>
#include <initializer_list>
using ALuint=unsigned;using ALint=int;
constexpr int AL_FALSE=0,AL_TRUE=1,AL_REMIX_UNMATCHED_SOFT=2,AL_DIRECT_CHANNELS_SOFT=0x1033;
int calls=0,errorReads=0,lastMode=-1;
struct Loader {
 bool ready=true;bool Ready()const{return ready;}
 void alSourcei(ALuint source,int property,int mode)const{assert(source==7&&property==AL_DIRECT_CHANNELS_SOFT);++calls;lastMode=mode;}
 void alGetError()const{++errorReads;}
};
struct OpenALDevice {
 Loader loader_;
 struct {bool directChannels=false,directChannelsRemix=false;}capabilities_;
 void SetSourceDirectChannels(ALuint,bool)const;
};
''' + function + r'''
int main(){
 for(bool ready:{false,true})for(bool direct:{false,true})for(bool remix:{false,true})for(bool request:{false,true})for(ALuint source:{0u,7u}){
  OpenALDevice device;device.loader_.ready=ready;device.capabilities_={direct,remix};calls=errorReads=0;lastMode=-1;
  device.SetSourceDirectChannels(source,request);
  if(!ready||!direct||!source){assert(calls==0&&errorReads==0);continue;}
  assert(calls==1&&errorReads==1);
  assert(lastMode==(request&&remix?AL_REMIX_UNMATCHED_SOFT:AL_FALSE));
  assert(lastMode!=AL_TRUE); // This enum drops unmatched input speakers.
 }
}
'''
        with tempfile.TemporaryDirectory() as directory:
            cpp=Path(directory)/'routing.cpp';exe=Path(directory)/'routing.exe';cpp.write_text(code)
            built=subprocess.run([compiler,*compiler_target_args(compiler),'-std=c++17',str(cpp),'-o',str(exe)],capture_output=True,text=True)
            self.assertEqual(built.returncode,0,built.stdout+built.stderr)
            ran=subprocess.run([str(exe)],capture_output=True,text=True)
            self.assertEqual(ran.returncode,0,ran.stdout+ran.stderr)


if __name__=='__main__':
    unittest.main()
