"""Execute shared WAV payload paths with bounded short-read/error fixtures."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]


class WavPayloadTests(unittest.TestCase):
    def test_load_and_stream_report_only_available_payload(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++ compiler unavailable')
        source = (ROOT / 'code/client/audio/codecs/snd_codec_wav.cpp').read_text()
        conversion = source[source.index('short ConvertWavSampleToPCM16'):source.index('/*\n=================\nS_ReadRIFFHeader')]
        load = source[source.index('extern "C" void *S_WAV_CodecLoad'):source.index('/*\n=================\nS_WAV_CodecOpenStream')]
        stream = source[source.index('extern "C" int S_WAV_CodecReadStream'):]
        harness = r'''
#include <algorithm>
#include <array>
#include <cassert>
#include <climits>
#include <cstring>
#include <cstdlib>
#include <vector>
using byte=unsigned char;
constexpr int kWavFormatPCM=1,kWavFormatIEEEFloat=3,qtrue=1;
#define S_COLOR_RED ""
struct WavSourceFormat {int format=kWavFormatPCM,width=0;bool needsConversion=false;};
struct WavStreamState {int format,width;};
struct snd_info_t {int rate=44100,width=0,channels=1,samples=0,size=0,dataofs=0;};
struct snd_stream_t {int file=1;snd_info_t info;int pos=0;void*ptr=nullptr;};
std::vector<byte> payload;
std::size_t cursor=0;
int readCalls=0,readLimit=INT_MAX,allocations=0,openHandles=0;
WavSourceFormat selected;
snd_info_t description;
int FileRead(int,void*dst,int bytes){
 assert(bytes>=0);++readCalls;
 if(readLimit<0)return -1;
 int n=std::min({bytes,readLimit,static_cast<int>(payload.size()-cursor)});
 if(n)std::memcpy(dst,payload.data()+cursor,n);
 cursor+=n;return n;
}
struct ScopedFileHandle {
 bool opened=false;
 ~ScopedFileHandle(){if(opened)--openHandles;}
 explicit operator bool()const{return opened;}
 int get()const{return 1;}
};
int OpenFileRead(const char*,ScopedFileHandle&f,int){f.opened=true;++openHandles;return static_cast<int>(payload.size());}
struct ScopedTempMemory {
 void*p;
 explicit ScopedTempMemory(int n):p(std::malloc(n?n:1)){++allocations;std::memset(p,0xa5,n);}
 ScopedTempMemory(const ScopedTempMemory&)=delete;
 ~ScopedTempMemory(){if(p){std::free(p);--allocations;}}
 static ScopedTempMemory Allocate(int n){assert(n>=0&&n<100000);return ScopedTempMemory(n);}
 template<class T>T*as(){return static_cast<T*>(p);}
 void*release(){auto*r=p;p=nullptr;--allocations;return r;}
};
bool S_ReadRIFFHeader(int,int,snd_info_t*i,WavSourceFormat*s){*i=description;*s=selected;return true;}
void Com_Printf(const char*,...){ }
void S_ByteSwapRawSamples(int samples,int width,int channels,byte*){assert(samples>=0&&width>0&&channels>0);}
''' + conversion + load + stream + r'''
void reset(){cursor=0;readCalls=0;readLimit=INT_MAX;assert(allocations==0&&openHandles==0);}
int main(){
 WavSourceFormat pcm8{kWavFormatPCM,1,true};
 for(int value=0;value<256;++value){byte b=static_cast<byte>(value);assert(ConvertWavSampleToPCM16(&b,pcm8)==(value-128)*256);}
 for(int width:{1,2,3,4})for(int channels:{1,2}){
  selected={width==4?kWavFormatIEEEFloat:kWavFormatPCM,width,width>2||(width==1&&channels>1)};
  description={44100,selected.needsConversion?2:width,channels,8,8*channels*(selected.needsConversion?2:width),0};
  std::vector<byte> full(8*channels*width,0);
  for(std::size_t i=0;i<full.size();i+=width){
   if(width==4){float f=.5f;std::memcpy(full.data()+i,&f,4);}
   else full[i+width-1]=0x40;
  }
  payload=full;reset();snd_info_t info;
  auto*loaded=static_cast<byte*>(S_WAV_CodecLoad("fixture.wav",&info));assert(loaded);
  std::vector<byte> expected(loaded,loaded+info.size);std::free(loaded);
  assert(cursor==full.size()&&allocations==0&&openHandles==0);
  if(!selected.needsConversion)assert(expected==full);
  else for(int i=0;i<info.size;i+=2){short v;std::memcpy(&v,expected.data()+i,2);assert(v==(width==1?-16384:(width==3?16384:16383)));}
  // Every truncation must reject a whole load without leaking either buffer.
  for(std::size_t n=0;n<full.size();++n){
   payload.assign(full.begin(),full.begin()+n);reset();
   assert(S_WAV_CodecLoad("truncated.wav",&info)==nullptr);
   assert(allocations==0&&openHandles==0);
  }
  payload=full;reset();readLimit=-1;
  assert(S_WAV_CodecLoad("error.wav",&info)==nullptr);assert(allocations==0&&openHandles==0);
  WavStreamState state{selected.format,width};
  auto makeStream=[&](){snd_stream_t s;s.info=description;s.ptr=selected.needsConversion?&state:nullptr;return s;};
  // Valid repeated reads reconstruct exactly the whole-load output.
  payload=full;reset();auto s=makeStream();std::vector<byte> actual;
  while(s.pos<s.info.size){std::array<byte,16>b{};int n=S_WAV_CodecReadStream(&s,4,b.data());assert(n>0&&n<=4);actual.insert(actual.end(),b.begin(),b.begin()+n);}
  assert(actual==expected);assert(S_WAV_CodecReadStream(&s,4,actual.data())==0);
  // Truncation can return complete available source samples, never invented data.
  for(std::size_t n=0;n<full.size();++n){
   payload.assign(full.begin(),full.begin()+n);reset();s=makeStream();std::array<byte,128>b;b.fill(0xa5);
   int got=S_WAV_CodecReadStream(&s,description.size,b.data());
   int want=!selected.needsConversion?static_cast<int>(n):static_cast<int>(n/width)*2;
   assert(got==want&&s.pos==want);
   assert(std::equal(b.begin(),b.begin()+got,expected.begin()));
   assert(std::all_of(b.begin()+got,b.end(),[](byte v){return v==0xa5;}));
   assert(S_WAV_CodecReadStream(&s,8,b.data())==0&&s.pos==want);
  }
  payload=full;reset();s=makeStream();std::array<byte,128>b;b.fill(0xa5);
  for(int request:{0,-1,INT_MIN})assert(S_WAV_CodecReadStream(&s,request,b.data())==0);
  assert(readCalls==0&&s.pos==0);
  readLimit=-1;assert(S_WAV_CodecReadStream(&s,description.size,b.data())==0&&s.pos==0);
  assert(std::all_of(b.begin(),b.end(),[](byte v){return v==0xa5;}));
 }
}
'''
        with tempfile.TemporaryDirectory() as directory:
            cpp=Path(directory)/'wav.cpp'
            exe=Path(directory)/'wav.exe'
            cpp.write_text(harness)
            result=subprocess.run([compiler,*compiler_target_args(compiler),'-std=c++17',str(cpp),'-o',str(exe)],capture_output=True,text=True)
            self.assertEqual(result.returncode,0,result.stdout+result.stderr)
            result=subprocess.run([str(exe)],capture_output=True,text=True)
            self.assertEqual(result.returncode,0,result.stdout+result.stderr)


if __name__=='__main__':
    unittest.main()
