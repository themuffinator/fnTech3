"""Execute persistent stream resampling, independent of audio devices."""
from pathlib import Path
import shutil, subprocess, tempfile, unittest
from compiler_support import compiler_target_args
ROOT=Path(__file__).resolve().parents[1]
class StreamResamplerTests(unittest.TestCase):
    def test_upsampling_preserves_tones_without_audible_images(self):
        cc=shutil.which('clang++') or shutil.which('g++')
        if not cc:self.skipTest('C++ compiler unavailable')
        source=r'''
#include "code/client/audio/shared/AudioStreamResampler.h"
#include <cassert>
#include <cmath>
#include <cstdio>
using fntech3::audio::CubicStreamResampler;
int main(){
 constexpr double pi=3.14159265358979323846;
 for(int inputRate:{8000,16000,22050,44100})for(int kind:{0,1,2}){
  if(kind==2&&inputRate!=44100)continue;
  const int outputRate=48000,frames=inputRate/2;
  const double wanted=kind==2?20000:inputRate*(kind==0?.35:.45);
  double image=inputRate-wanted;if(image>outputRate*.5)image=outputRate-image;
  std::vector<float> input(frames),output,part;
  for(int i=0;i<frames;++i)input[i]=float(std::sin(2*pi*wanted*i/inputRate));
  CubicStreamResampler r;assert(r.Configure(inputRate,outputRate,1));
  for(int at=0;at<frames;at+=257){const int n=(std::min)(257,frames-at);assert(r.Push(input.data()+at,n,part));output.insert(output.end(),part.begin(),part.end());}
  assert(r.Finish(part));output.insert(output.end(),part.begin(),part.end());
  assert(output.size()==24000);
  // Joint least-squares sinusoid fit avoids mistaking endpoint/window
  // leakage from the wanted tone for an interpolation image.
  double system[4][5]{};
  for(size_t i=512;i<output.size()-512;++i){
   const double a=2*pi*wanted*i/outputRate,b=2*pi*image*i/outputRate;
   const double basis[]={std::sin(a),std::cos(a),std::sin(b),std::cos(b)};
   for(int row=0;row<4;++row){for(int col=0;col<4;++col)system[row][col]+=basis[row]*basis[col];system[row][4]+=basis[row]*output[i];}
  }
  for(int col=0;col<4;++col){
   int pivot=col;for(int row=col+1;row<4;++row)if(std::fabs(system[row][col])>std::fabs(system[pivot][col]))pivot=row;
   for(int i=0;i<5;++i)std::swap(system[col][i],system[pivot][i]);
   const double divisor=system[col][col];assert(std::fabs(divisor)>1);
   for(int i=col;i<5;++i)system[col][i]/=divisor;
   for(int row=0;row<4;++row)if(row!=col){const double factor=system[row][col];for(int i=col;i<5;++i)system[row][i]-=factor*system[col][i];}
  }
  const double gain=std::hypot(system[0][4],system[1][4]);
  const double unwanted=std::hypot(system[2][4],system[3][4]);
  std::printf("upsample %d -> %d: tone %.1f Hz gain %.9f, image %.1f Hz %.2f dB\n",inputRate,outputRate,wanted,gain,image,20*std::log10(unwanted));std::fflush(stdout);
  assert(std::fabs(gain-1)<.001&&unwanted<.001);
 }
}
'''
        with tempfile.TemporaryDirectory() as temp:
            cpp=Path(temp)/'images.cpp';exe=Path(temp)/'images.exe';cpp.write_text(source)
            result=subprocess.run([cc,*compiler_target_args(cc),'-std=c++17','-O2','-I',str(ROOT),str(cpp),'-o',str(exe)],capture_output=True,text=True)
            self.assertEqual(result.returncode,0,result.stdout+result.stderr)
            result=subprocess.run([str(exe)],capture_output=True,text=True)
            self.assertEqual(result.returncode,0,result.stdout+result.stderr)
            print(result.stdout,end='')

    def test_downsampling_rejects_aliases_and_preserves_audible_tones(self):
        cc=shutil.which('clang++') or shutil.which('g++')
        if not cc:self.skipTest('C++ compiler unavailable')
        source=r'''
#include "code/client/audio/shared/AudioStreamResampler.h"
#include <cassert>
#include <cmath>
#include <cstdio>
using fntech3::audio::CubicStreamResampler;
double tone(int inputRate,int outputRate,double frequency){
 const int frames=inputRate/2;std::vector<float> input(frames),output,part;
 constexpr double pi=3.14159265358979323846;
 for(int i=0;i<frames;++i)input[i]=float(std::sin(2*pi*frequency*i/inputRate));
 CubicStreamResampler r;assert(r.Configure(inputRate,outputRate,1));
 for(int offset=0;offset<frames;offset+=257){
  const int n=(std::min)(257,frames-offset);
  assert(r.Push(input.data()+offset,n,part));output.insert(output.end(),part.begin(),part.end());
 }
 assert(r.Finish(part));output.insert(output.end(),part.begin(),part.end());
 assert(output.size()==size_t((int64_t(frames)*outputRate+inputRate-1)/inputRate));
 double power=0;const size_t margin=512;
 for(size_t i=margin;i<output.size()-margin;++i){assert(std::isfinite(output[i]));power+=double(output[i])*output[i];}
 return std::sqrt(power/(output.size()-2*margin))*std::sqrt(2.0);
}
int main(){
 for(auto rates:{std::pair<int,int>{96000,48000},{48000,24000},{48000,8000},{44100,22050},{48000,44100},{192000,8000}}){
  const double pass=tone(rates.first,rates.second,1000);
  const double high=tone(rates.first,rates.second,rates.second*.45);
  const double stop=tone(rates.first,rates.second,rates.second*.51);
  std::printf("%d -> %d: passband gain %.9f, alias %.2f dB, high-band gain %.9f\n",rates.first,rates.second,pass,20*std::log10(stop),high);
  std::fflush(stdout);assert(std::fabs(pass-1)<.001&&std::fabs(high-1)<.001&&stop<.001);
 }
}
'''
        with tempfile.TemporaryDirectory() as temp:
            cpp=Path(temp)/'quality.cpp';exe=Path(temp)/'quality.exe';cpp.write_text(source)
            result=subprocess.run([cc,*compiler_target_args(cc),'-std=c++17','-O2','-I',str(ROOT),str(cpp),'-o',str(exe)],capture_output=True,text=True)
            self.assertEqual(result.returncode,0,result.stdout+result.stderr)
            result=subprocess.run([str(exe)],capture_output=True,text=True)
            self.assertEqual(result.returncode,0,result.stdout+result.stderr)
            print(result.stdout, end='')

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
  const int margin=rate==output?1:int(std::ceil(64.0*(std::max)(1.0,double(rate)/output)));
  assert(r.BufferedFrames()<=size_t(2*margin+2));offset+=n;
 }
 assert(r.Finish(part));all.insert(all.end(),part.begin(),part.end());assert(r.BufferedFrames()==0);
 assert(r.Finish(part)&&part.empty());return all;
}
int main(){
 for(int channels:{1,2,4,6})for(int rate:{8000,22050,44100,48000,96000})for(int output:{8000,44100,48000}){
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
 assert(r.Configure(1,192000,1));assert(!r.Push(input,32,out)&&out.empty()&&r.BufferedFrames()==0);
 assert(r.Configure(192000,8000,32));assert(!r.Push(input,INT32_MAX,out)&&out.empty()&&r.BufferedFrames()==0);
 assert(r.Configure(INT32_MAX,1,1));assert(r.Push(input,1,out));assert(r.Finish(out)&&out.size()==1&&out[0]==1);
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
        raw_start=backend.index('\tif ( rawPlayer_.Idle() )')
        raw_tick=backend[raw_start:backend.index('\tconst float voiceVolume',raw_start)]
        source=r'''
#include "code/client/audio/shared/AudioStreamResampler.h"
#include <cassert>
#include <cmath>
#include <cstring>
#include <utility>
#include <cstdlib>
#include <new>
bool countIdleAllocations=false;size_t idleAllocations=0;
void*operator new(size_t size){if(countIdleAllocations)++idleAllocations;if(void*p=std::malloc(size))return p;throw std::bad_alloc();}
void operator delete(void*p)noexcept{std::free(p);}
void operator delete(void*p,size_t)noexcept{std::free(p);}
using byte=unsigned char;constexpr int kDefaultStreamChannels=2;
#define S_COLOR_YELLOW ""
float ClampFloat(float v,float lo,float hi){return (std::max)(lo,(std::min)(v,hi));}
short ClampPCM16FromFloat(float v){return short((std::max)(-32768,(std::min)(int(v),32767)));}
short ReadNativePCM16(const byte*p){short value;std::memcpy(&value,p,2);return value;}
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
 bool fail=false,rejectMulti=false;int calls=0,clears=0,multiCalls=0,queued=0;
 int QueuedBufferCount()const{return queued;}
 bool Idle(){return queued==0;}
 // Every short fixture buffer has played by the following engine tick.
 void Update(float){queued=0;}
 bool QueuePCM16(const short*p,int n,int c,int rate){
  ++calls;assert(n>0&&rate>0);if(c>2)++multiCalls;
  if(fail||(rejectMulti&&c>2))return false;
  output.insert(output.end(),p,p+size_t(n)*c);++queued;return true;
 }
 void Clear(){conversion={};output.clear();queued=0;++clears;}
};
struct AudioSystem {
 int backgroundStereoFallbackChannels_=0;
 int outputRate=48000;
 StreamPlayer rawPlayer_;
 int StreamOutputRate()const{return outputRate;}
 bool FlushStream(StreamPlayer&,bool keepTimeline=false);
 bool QueueStreamChunk(StreamPlayer&,int,int,int,int,const byte*,float,int,bool);
 void RawTick(){const bool softMuted=false;const float streamGain=1;
'''+raw_tick+r'''
 }
};
'''+reads+functions+r'''
std::vector<short> run(AudioSystem&s,int block){
 StreamPlayer p;std::vector<short> in(44100*2);
 for(size_t i=0;i<in.size();++i)in[i]=short(8000*std::sin(i*.017));
 for(int i=0;i<44100;i+=block){int n=(std::min)(block,44100-i);assert(s.QueueStreamChunk(p,n,44100,2,2,reinterpret_cast<byte*>(in.data()+i*2),1,2,false));}
 assert(s.FlushStream(p));assert(p.conversion.rate==0);assert(p.output.size()==size_t(s.outputRate)*2);return p.output;
}
int main(){
 AudioSystem s;auto whole=run(s,44100);for(int block:{1,7,8192})assert(run(s,block)==whole);
 // RawSamples lacks EOF. Drive the actual raw-update fragment after every
 // 17-frame chunk and let queued buffers drain between ticks. Repeated
 // tails must not restart the phase and round each chunk independently.
 for(int outputRate:{48000,24000}){
  s.outputRate=outputRate;s.rawPlayer_.Clear();StreamPlayer reference;
  std::vector<short> input(1700*2,1000);
  assert(s.QueueStreamChunk(reference,1700,44100,2,2,reinterpret_cast<byte*>(input.data()),1,2,false));
  for(int chunk=0;chunk<100;++chunk){
   assert(s.QueueStreamChunk(s.rawPlayer_,17,44100,2,2,reinterpret_cast<byte*>(input.data()+chunk*34),1,2,false));
   s.RawTick();
  }
  // Retained left support is not pending PCM. A fully drained raw stream
  // must not copy its conversion history or allocate on every idle update.
  countIdleAllocations=true;idleAllocations=0;
  for(int tick=0;tick<10000;++tick)s.RawTick();
  countIdleAllocations=false;assert(idleAllocations==0);
  assert(s.FlushStream(reference)&&s.FlushStream(s.rawPlayer_));
  assert(s.rawPlayer_.output==reference.output);
  const auto count=s.rawPlayer_.output.size();s.RawTick();s.RawTick();assert(s.rawPlayer_.output.size()==count);
 }
 s.outputRate=48000;
 s.outputRate=24000;whole=run(s,44100);for(int block:{1,7,8192})assert(run(s,block)==whole);s.outputRate=48000;
 // Changing only gain must not reset the phase or add a rounded-up frame
 // for every block. Compare with the same gain envelope in a single buffer.
 for(int outputRate:{48000,24000}){
  s.outputRate=outputRate;StreamPlayer fading,reference;
  std::vector<short> envelope(1700*2),block(17*2,1000);
  for(int chunk=0;chunk<100;++chunk){
   const float gain=chunk%2?.5f:1.0f;
   std::fill_n(envelope.data()+chunk*17*2,17*2,short(1000*gain));
   assert(s.QueueStreamChunk(fading,17,44100,2,2,reinterpret_cast<byte*>(block.data()),gain,2,false));
  }
  assert(s.QueueStreamChunk(reference,1700,44100,2,2,reinterpret_cast<byte*>(envelope.data()),1,2,false));
  assert(s.FlushStream(fading)&&s.FlushStream(reference));
  assert(fading.output==reference.output);
  assert(fading.output.size()==size_t((int64_t(1700)*outputRate+44100-1)/44100)*2);
 }
 s.outputRate=48000;
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
 p.Clear();p.rejectMulti=true;std::vector<short> surround(200*6,1000);
 assert(s.QueueStreamChunk(p,100,44100,2,6,reinterpret_cast<byte*>(surround.data()),1,6,true));assert(p.conversion.outputChannels==2);
 assert(s.QueueStreamChunk(p,100,44100,2,6,reinterpret_cast<byte*>(surround.data()),1,6,true));assert(p.multiCalls==1);assert(s.FlushStream(p));assert(p.output.size()==436);
 p.Clear();p.rejectMulti=false;
 assert(s.QueueStreamChunk(p,100,44100,2,6,reinterpret_cast<byte*>(surround.data()),1,6,false));
 prior=p.output;good=p;int oldClears=p.clears;p.fail=true;
 assert(!s.QueueStreamChunk(p,100,44100,2,6,reinterpret_cast<byte*>(surround.data()),1,6,false));
 assert(p.output==prior&&p.clears==oldClears);
 p.fail=false;
 assert(s.QueueStreamChunk(p,100,44100,2,6,reinterpret_cast<byte*>(surround.data()),1,6,false));
 assert(s.QueueStreamChunk(good,100,44100,2,6,reinterpret_cast<byte*>(surround.data()),1,6,false));
 assert(s.FlushStream(p)&&s.FlushStream(good)&&p.output==good.output);
 // Before playback starts the filter can hold accepted input with no AL
 // buffers yet. A failed stereo fallback must retain that history for retry.
 s.outputRate=24000;p.Clear();p.rejectMulti=true;std::vector<short> first(6,7777);
 assert(s.QueueStreamChunk(p,1,44100,2,6,reinterpret_cast<byte*>(first.data()),1,6,false));
 assert(p.output.empty()&&p.conversion.resampler.BufferedFrames()==1);good=p;p.fail=true;
 assert(!s.QueueStreamChunk(p,200,44100,2,6,reinterpret_cast<byte*>(surround.data()),1,6,false));
 assert(p.conversion.rate==44100&&p.conversion.resampler.BufferedFrames()==1);
 p.fail=false;
 assert(s.QueueStreamChunk(p,200,44100,2,6,reinterpret_cast<byte*>(surround.data()),1,6,false));
 assert(s.QueueStreamChunk(good,200,44100,2,6,reinterpret_cast<byte*>(surround.data()),1,6,false));
 assert(s.FlushStream(p)&&s.FlushStream(good)&&p.output==good.output);s.outputRate=48000;
 p.Clear();assert(s.QueueStreamChunk(p,100,44100,2,2,reinterpret_cast<byte*>(in.data()),1,2,false));
 oldClears=p.clears;s.outputRate=44100;
 assert(s.QueueStreamChunk(p,100,44100,2,2,reinterpret_cast<byte*>(in.data()),1,2,false));assert(p.clears==oldClears+1&&p.output.size()==200);assert(s.FlushStream(p));s.outputRate=48000;
 p.Clear();assert(p.conversion.rate==0);assert(s.FlushStream(p)&&p.output.empty());
 assert(!s.QueueStreamChunk(p,-1,44100,2,2,reinterpret_cast<byte*>(in.data()),1,2,false));
 assert(!s.QueueStreamChunk(p,INT32_MAX/8,44100,2,2,reinterpret_cast<byte*>(in.data()),1,2,false));
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
        self.assertIn('if ( rawPlayer_.Idle() ) FlushStream( rawPlayer_',backend)
if __name__=='__main__':unittest.main()
