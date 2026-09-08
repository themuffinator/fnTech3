"""Execute decoded-PCM speaker routing independently of an audio device."""
from pathlib import Path
import shutil, subprocess, tempfile, unittest
from compiler_support import compiler_target_args
ROOT=Path(__file__).resolve().parents[1]
class SpeakerLayoutTests(unittest.TestCase):
    def test_explicit_speakers_and_bounds(self):
        cc=shutil.which('clang++') or shutil.which('g++')
        if not cc: self.skipTest('C++ compiler unavailable')
        code=r'''#include "code/client/audio/shared/AudioSpeakerLayout.h"
#include <cassert>
#include <climits>
#include <cstdlib>
#include <array>
using namespace fntech3::audio;
int main(){
 for(int channels:{1,2,3,4,6,7,8})for(int width:{1,2})for(int capacity:{0,1,15,30000,32768}){
  int bytes=WholeFrameReadBytes(capacity,width,channels);
  assert(bytes<=capacity && bytes%(width*channels)==0 && capacity-bytes<width*channels);
 }
 assert(WholeFrameReadBytes(32768,2,6)==32760);
 assert(WholeFrameReadBytes(INT_MAX,2,INT_MAX)==0);
 assert(WholeFrameReadBytes(32768,3,2)==0);

 for(int channels:{1,2,4,6,7,8}){
  assert(!NeedsSpeakerDownmix(0,channels));
  assert(!NeedsSpeakerDownmix(DefaultSpeakerMask(channels),channels));
  assert(NeedsSpeakerDownmix(DefaultSpeakerMask(channels),channels,false)==(channels>2));
 }
 assert(NeedsSpeakerDownmix(5,2));
 short frame[]{0,10000};auto bytes=reinterpret_cast<const unsigned char*>(frame);
 assert(DownmixSpeakerBuffer(bytes,4,0,1,2,2,5)==DownmixSpeakers(bytes,1,2,2,5));
 assert(DownmixSpeakerBuffer(bytes,4,2,1,2,1,4)==DownmixSpeakers(bytes+2,1,2,1,4));
 for(int length:{-1,0,1,2,3})assert(DownmixSpeakerBuffer(bytes,length,0,1,2,2,5).empty());
 for(int offset:{-1,1,4,INT_MAX})assert(DownmixSpeakerBuffer(bytes,4,offset,1,2,2,5).empty());
 assert(DownmixSpeakerBuffer(bytes,4,0,INT_MAX,2,2,5).empty());
 assert(DownmixSpeakerBuffer(bytes,4,0,1,2,INT_MAX,5).empty());
 assert(DownmixSpeakerBuffer(nullptr,4,0,1,2,2,5).empty());
 assert((DownmixSpeakers(bytes,1,2,2,3)==std::vector<short>{0,10000}));
 auto center=DownmixSpeakers(bytes,1,2,2,5);assert(center[0]==center[1]&&std::abs(center[0]-7071)<=1);
 // Missing mask bits leave channels unassigned; excess high bits are ignored.
 assert((DownmixSpeakers(bytes,1,2,2,1)==std::vector<short>{0,0}));
 assert((DownmixSpeakers(bytes,1,2,2,7)==std::vector<short>{0,10000}));
 const std::array<std::array<short,2>,18> expected{{
  {10000,0},{0,10000},{7071,7071},{2500,2500},{5000,0},{0,5000},
  {7071,0},{0,7071},{3500,3500},{5000,0},{0,5000},{3500,3500},
  {5000,0},{3500,3500},{0,5000},{5000,0},{3500,3500},{0,5000}}};
 short tone=10000;
 for(unsigned bit=0;bit<32;++bit){
  auto out=DownmixSpeakers(reinterpret_cast<const unsigned char*>(&tone),1,2,1,1u<<bit);
  assert(out.size()==2);
  assert(std::abs(out[0]-(bit<18?expected[bit][0]:0))<=1&&std::abs(out[1]-(bit<18?expected[bit][1]:0))<=1);
 }
 std::array<short,18> loud;loud.fill(32767);
 auto out=DownmixSpeakers(reinterpret_cast<const unsigned char*>(loud.data()),1,2,18,0x3ffff);
 assert(out[0]==32767&&out[1]==32767);
 loud.fill(-32768);out=DownmixSpeakers(reinterpret_cast<const unsigned char*>(loud.data()),1,2,18,0x3ffff);
 assert(out[0]==-32768&&out[1]==-32768);
 unsigned char silence=128;assert((DownmixSpeakers(&silence,1,1,1,4)==std::vector<short>{0,0}));
 unsigned char peak=255;assert((DownmixSpeakers(&peak,1,1,1,1)==std::vector<short>{32512,0}));
 std::vector<short> frames;for(int i=0;i<41;++i){frames.push_back(i);frames.push_back(i*100);}
 auto full=DownmixSpeakers(reinterpret_cast<const unsigned char*>(frames.data()),41,2,2,5);
 for(int chunk:{1,3,7,40}){
  std::vector<short> joined;
  for(int i=0;i<41;i+=chunk){int n=41-i<chunk?41-i:chunk;auto part=DownmixSpeakers(reinterpret_cast<const unsigned char*>(frames.data()+i*2),n,2,2,5);joined.insert(joined.end(),part.begin(),part.end());}
  assert(full==joined);
 }
 assert(DownmixSpeakers(nullptr,1,2,2,5).empty());
 assert(DownmixSpeakers(bytes,INT_MAX,2,2,5).empty());
 assert(DownmixSpeakers(bytes,1,3,2,5).empty());
 assert(DownmixSpeakers(bytes,1,2,0,5).empty());
 assert(DownmixSpeakers(bytes,1,2,2,0).empty());
 assert(DownmixSpeakers(bytes,0,2,2,5).empty());
}
'''
        with tempfile.TemporaryDirectory() as directory:
            cpp=Path(directory)/'speaker.cpp';exe=Path(directory)/'speaker.exe';cpp.write_text(code)
            result=subprocess.run([cc,*compiler_target_args(cc),'-std=c++17','-I',str(ROOT),str(cpp),'-o',str(exe)],capture_output=True,text=True)
            self.assertEqual(result.returncode,0,result.stdout+result.stderr)
            result=subprocess.run([str(exe)],capture_output=True,text=True)
            self.assertEqual(result.returncode,0,result.stdout+result.stderr)
    def test_loaded_sound_routing_metadata_is_bounded(self):
        cc=shutil.which('clang++') or shutil.which('g++')
        if not cc:self.skipTest('C++ compiler unavailable')
        source=(ROOT/'code/client/audio/openal/AudioSystemWorld.inl').read_text()
        fragment=source[source.index('\tconst bool route ='):source.index('\tconst AudioSampleFormat sourceFormat =',source.index('\tconst bool route ='))]
        code=r'''#include "code/client/audio/shared/AudioSpeakerLayout.h"
#include <cassert>
#include <climits>
using byte=unsigned char;
struct Info {int size,dataofs,samples,width,channels;unsigned speakerMask;};
struct Buffer {unsigned char bytes[8]{};const void*get()const{return bytes;}};
void probe(Info info,bool valid){Buffer data;
''' + fragment + r'''
 assert(!routed.empty()==valid);
 assert(info.size==(valid?4:0));
}
int main(){
 probe({4,0,1,2,2,5},true);
 probe({8,4,1,2,2,5},true);
 probe({INT_MAX,0,INT_MAX,1,1,1},false);
 probe({3,0,1,2,2,5},false);
 probe({4,-1,1,2,2,5},false);
 probe({4,INT_MAX,1,2,2,5},false);
 probe({4,0,1,3,2,5},false);
}
'''
        with tempfile.TemporaryDirectory() as directory:
            cpp=Path(directory)/'loaded.cpp';exe=Path(directory)/'loaded.exe';cpp.write_text(code)
            result=subprocess.run([cc,*compiler_target_args(cc),'-std=c++17','-I',str(ROOT),str(cpp),'-o',str(exe)],capture_output=True,text=True)
            self.assertEqual(result.returncode,0,result.stdout+result.stderr)
            result=subprocess.run([str(exe)],capture_output=True,text=True)
            self.assertEqual(result.returncode,0,result.stdout+result.stderr)
if __name__=='__main__':unittest.main()
