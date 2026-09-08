"""Execute persistent stream resampling, independent of audio devices."""
from pathlib import Path
import shutil, subprocess, tempfile, unittest
from compiler_support import compiler_target_args
ROOT=Path(__file__).resolve().parents[1]
class StreamResamplerTests(unittest.TestCase):
    def test_partitions_endpoints_and_reset(self):
        cc=shutil.which('clang++') or shutil.which('g++')
        if not cc:self.skipTest('C++ compiler unavailable')
        source=r'''
#include "code/client/audio/shared/AudioStreamResampler.h"
#include <cassert>
#include <cmath>
using fntech3::audio::CubicStreamResampler;
std::vector<float> convert(const std::vector<float>&input,int rate,int output,int channels,int block){
 CubicStreamResampler r;assert(r.Configure(rate,output,channels));std::vector<float> all,part;
 for(size_t offset=0;offset<input.size()/channels;){
  int n=static_cast<int>((std::min)(size_t(block),input.size()/channels-offset));
  assert(r.Push(input.data()+offset*channels,n,part));all.insert(all.end(),part.begin(),part.end());
  assert(r.BufferedFrames()<=4);offset+=n;
 }
 assert(r.Finish(part));all.insert(all.end(),part.begin(),part.end());assert(r.BufferedFrames()==0);
 assert(r.Finish(part)&&part.empty());return all;
}
int main(){
 for(int channels:{1,2,6})for(int rate:{8000,22050,44100,48000,96000})for(int output:{8000,44100,48000}){
  int frames=rate/5+7;std::vector<float> in(size_t(frames)*channels);
  for(int i=0;i<frames;++i)for(int c=0;c<channels;++c)in[size_t(i)*channels+c]=float(8192*std::sin((i+c)*.043));
  auto whole=convert(in,rate,output,channels,frames);
  assert(whole.size()==size_t((int64_t(frames)*output+rate-1)/rate)*channels);
  for(int block:{1,2,3,7,257,8192})assert(convert(in,rate,output,channels,block)==whole);
  if(rate==output)assert(whole==in);
 }
 for(int rate:{22050,44100,48000}){
  std::vector<float> in(size_t(rate)*10*2,1234);auto out=convert(in,rate,48000,2,8192);
  assert(out.size()==960000);for(float v:out)assert(v==1234);
 }
 for(int rate:{8000,22050,48000,96000}){
  std::vector<float> single{3000};auto out=convert(single,rate,48000,1,1);
  assert(out.size()==size_t((48000+rate-1)/rate));for(float v:out)assert(v==3000);
 }
 CubicStreamResampler r;std::vector<float> out;float input[4]={1,2,3,4};
 assert(!r.Push(input,4,out));assert(!r.Configure(0,48000,2));assert(!r.Configure(44100,0,2));assert(!r.Configure(44100,48000,33));
 assert(r.Configure(44100,48000,1));assert(r.Push(input,1,out));
 assert(!r.Push(nullptr,2,out));assert(!r.Push(input,-1,out));
 assert(r.Configure(48000,48000,1));assert(r.Push(input,4,out));assert(out==std::vector<float>({1,2,3,4}));
}
'''
        with tempfile.TemporaryDirectory() as temp:
            cpp=Path(temp)/'resampler.cpp';exe=Path(temp)/'resampler.exe';cpp.write_text(source)
            r=subprocess.run([cc,*compiler_target_args(cc),'-std=c++17','-I',str(ROOT),str(cpp),'-o',str(exe)],capture_output=True,text=True)
            self.assertEqual(r.returncode,0,r.stdout+r.stderr)
            r=subprocess.run([str(exe)],capture_output=True,text=True)
            self.assertEqual(r.returncode,0,r.stdout+r.stderr)
    def test_queue_transactions_format_changes_and_stereo_fallback(self):
        cc=shutil.which('clang++') or shutil.which('g++')
        if not cc:self.skipTest('C++ compiler unavailable')
        backend=(ROOT/'code/client/audio/openal/AudioSystemBackend.inl').read_text()
        streams=(ROOT/'code/client/audio/openal/AudioSystemStreams.inl').read_text()
        state=streams[streams.index('struct StreamConversionState'):streams.index('class StreamPlayer')]
        reads=backend[backend.index('static float ReadStreamPCM16Sample'):backend.index('int AudioSystem::StreamOutputRate')]
        functions=backend[backend.index('static std::vector<short> ConvertResampledFrames'):backend.index('bool AudioSystem::Init')]
        source=r'''
#include "code/client/audio/shared/AudioStreamResampler.h"
#include <cassert>
#include <cmath>
#include <cstring>
#include <utility>
using byte=unsigned char;constexpr int kDefaultStreamChannels=2;
#define S_COLOR_YELLOW ""
float ClampFloat(float v,float lo,float hi){return (std::max)(lo,(std::min)(v,hi));}
short ClampPCM16FromFloat(float v){return short((std::max)(-32768,(std::min)(int(v),32767)));}
short ReadLittlePCM16(const byte*p){return short(unsigned(p[0])|(unsigned(p[1])<<8));}
short ConvertUnsignedPCM8ToPCM16(byte p){return short((int(p)-128)*256);}
short ConvertSignedPCM8ToPCM16(byte p){return short(static_cast<signed char>(p)*256);}
bool PCMChannelCountCanBeRepresented(int c){return c==1||c==2||c==6;}
bool PCMFrameByteCount(int n,int c,int w,size_t&b){b=size_t(n)*c*w;return true;}
bool PCMByteCountFitsALsizei(size_t count){return count<=size_t(INT32_MAX)/2;}
void DownmixFrameToStereoFloat(const float*p,int c,float g,float&l,float&r){assert(c==1||c==6);l=p[0]*g;r=p[c==1?0:1]*g;}
const char*PCMChannelLayoutName(int){return "fixture";}
void Com_DPrintf(const char*,...){}
'''+state+r'''
struct StreamPlayer {
 StreamConversionState conversion;
 std::vector<short> output;
 bool fail=false,rejectMulti=false;int calls=0,clears=0,multiCalls=0;
 bool QueuePCM16(const short*p,int n,int c,int rate){
  ++calls;assert(n>0&&rate>0);if(c>2)++multiCalls;
  if(fail||(rejectMulti&&c>2))return false;
  output.insert(output.end(),p,p+size_t(n)*c);return true;
 }
 void Clear(){conversion={};output.clear();++clears;}
};
struct AudioSystem {
 int backgroundStereoFallbackChannels_=0;
 int outputRate=48000;
 int StreamOutputRate()const{return outputRate;}
 bool FlushStream(StreamPlayer&);
 bool QueueStreamChunk(StreamPlayer&,int,int,int,int,const byte*,float,int,bool);
};
'''+reads+functions+r'''
std::vector<short> run(AudioSystem&s,int block){
 StreamPlayer p;std::vector<short> in(44100*2);
 for(size_t i=0;i<in.size();++i)in[i]=short(8000*std::sin(i*.017));
 for(int i=0;i<44100;i+=block){int n=(std::min)(block,44100-i);assert(s.QueueStreamChunk(p,n,44100,2,2,reinterpret_cast<byte*>(in.data()+i*2),1,2,false));}
 assert(s.FlushStream(p));assert(p.conversion.rate==0);assert(p.output.size()==96000);return p.output;
}
int main(){
 AudioSystem s;auto whole=run(s,44100);for(int block:{1,7,8192})assert(run(s,block)==whole);
 std::vector<short> in(100*2,1000);StreamPlayer p;
 assert(s.QueueStreamChunk(p,100,44100,2,2,reinterpret_cast<byte*>(in.data()),1,2,false));
 auto good=p;auto prior=p.output;p.fail=true;
 assert(!s.QueueStreamChunk(p,100,44100,2,2,reinterpret_cast<byte*>(in.data()),1,2,false));assert(p.output==prior);
 p.fail=false;assert(s.QueueStreamChunk(p,100,44100,2,2,reinterpret_cast<byte*>(in.data()),1,2,false));
 assert(s.QueueStreamChunk(good,100,44100,2,2,reinterpret_cast<byte*>(in.data()),1,2,false));
 assert(s.FlushStream(p)&&s.FlushStream(good));assert(p.output==good.output);
 p.Clear();assert(s.QueueStreamChunk(p,1,44100,2,2,reinterpret_cast<byte*>(in.data()),1,2,false));
 auto before=p.output;p.fail=true;assert(!s.FlushStream(p)&&p.conversion.rate==44100&&p.output==before);p.fail=false;assert(s.FlushStream(p));assert(p.output.size()==4);
 p.Clear();assert(s.QueueStreamChunk(p,100,44100,2,2,reinterpret_cast<byte*>(in.data()),1,2,false));
 // A rate change finishes the old segment once, then starts the new rate.
 assert(s.QueueStreamChunk(p,100,22050,2,2,reinterpret_cast<byte*>(in.data()),1,2,false));assert(s.FlushStream(p));
 assert(p.output.size()==size_t(109+218)*2);
 p.Clear();assert(s.QueueStreamChunk(p,100,44100,2,2,reinterpret_cast<byte*>(in.data()),1,2,false));
 assert(s.QueueStreamChunk(p,100,44100,2,1,reinterpret_cast<byte*>(in.data()),.5,2,false));assert(s.FlushStream(p));assert(p.output.size()==218*2);
 for(size_t i=0;i<109*2;++i)assert(p.output[i]==1000);for(size_t i=109*2;i<p.output.size();++i)assert(p.output[i]==500);
 p.Clear();p.rejectMulti=true;std::vector<short> surround(100*6,1000);
 assert(s.QueueStreamChunk(p,100,44100,2,6,reinterpret_cast<byte*>(surround.data()),1,6,true));assert(p.conversion.outputChannels==2);
 assert(s.QueueStreamChunk(p,100,44100,2,6,reinterpret_cast<byte*>(surround.data()),1,6,true));assert(p.multiCalls==1);assert(s.FlushStream(p));assert(p.output.size()==436);
 p.Clear();assert(s.QueueStreamChunk(p,100,44100,2,2,reinterpret_cast<byte*>(in.data()),1,2,false));
 int oldClears=p.clears;s.outputRate=44100;
 assert(s.QueueStreamChunk(p,100,44100,2,2,reinterpret_cast<byte*>(in.data()),1,2,false));assert(p.clears==oldClears+1&&p.output.size()==200);assert(s.FlushStream(p));s.outputRate=48000;
 p.Clear();assert(p.conversion.rate==0);assert(s.FlushStream(p)&&p.output.empty());
 assert(!s.QueueStreamChunk(p,-1,44100,2,2,reinterpret_cast<byte*>(in.data()),1,2,false));
 assert(!s.QueueStreamChunk(p,1,44100,3,2,reinterpret_cast<byte*>(in.data()),1,2,false));
}
'''
        with tempfile.TemporaryDirectory() as temp:
            cpp=Path(temp)/'queue.cpp';exe=Path(temp)/'queue.exe';cpp.write_text(source)
            r=subprocess.run([cc,*compiler_target_args(cc),'-std=c++17','-I',str(ROOT),str(cpp),'-o',str(exe)],capture_output=True,text=True)
            self.assertEqual(r.returncode,0,r.stdout+r.stderr)
            r=subprocess.run([str(exe)],capture_output=True,text=True)
            self.assertEqual(r.returncode,0,r.stdout+r.stderr)
        clear=streams[streams.index('void StreamPlayer::Clear()'):streams.index('void StreamPlayer::Shutdown()')]
        self.assertLess(clear.index('conversion = {};'),clear.index('if ( device_ == nullptr'))
        self.assertIn('if ( rawPlayer_.Idle() ) FlushStream( rawPlayer_ );',backend)
if __name__=='__main__':unittest.main()
