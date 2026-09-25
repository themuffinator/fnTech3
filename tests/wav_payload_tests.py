"""Execute shared WAV payload paths with bounded short-read/error fixtures."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]


class WavPayloadTests(unittest.TestCase):
    def test_pcm16_endian_conversion_covers_every_channel(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++ compiler unavailable')
        source = (ROOT / 'code/client/audio/codecs/snd_codec_wav.cpp').read_text()
        swap = source[source.index('void S_ByteSwapRawSamples'):source.index('short ConvertWavSampleToPCM16')]
        harness = r'''#include <array>
#include <cassert>
#include <cstring>
using byte=unsigned char;
bool bigEndian=false;
short LittleShort(short value){
 if(!bigEndian)return value;
 unsigned short bits;std::memcpy(&bits,&value,2);
 bits=static_cast<unsigned short>((bits>>8)|(bits<<8));
 std::memcpy(&value,&bits,2);return value;
}
''' + swap + r'''
int main(){
 for(bool endian:{false,true})for(int channels:{1,2,3,4,6,8})for(int frames:{0,1,7}){
  bigEndian=endian;
  std::array<short,58> values;values.fill(0x1234);
  S_ByteSwapRawSamples(frames,2,channels,reinterpret_cast<byte*>(values.data()+1));
  for(int i=0;i<58;++i)assert(values[i]==((i>0&&i<=frames*channels&&endian)?0x3412:0x1234));
  values.fill(0x1234);
  S_ByteSwapRawSamples(frames,1,channels,reinterpret_cast<byte*>(values.data()+1));
  for(auto value:values)assert(value==0x1234);
 }
}
'''
        with tempfile.TemporaryDirectory() as directory:
            cpp=Path(directory)/'endian.cpp'
            exe=Path(directory)/'endian.exe'
            cpp.write_text(harness)
            result=subprocess.run([compiler,*compiler_target_args(compiler),'-std=c++17',str(cpp),'-o',str(exe)],capture_output=True,text=True)
            self.assertEqual(result.returncode,0,result.stdout+result.stderr)
            result=subprocess.run([str(exe)],capture_output=True,text=True)
            self.assertEqual(result.returncode,0,result.stdout+result.stderr)

    def test_load_and_stream_report_only_available_payload(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++ compiler unavailable')
        source = (ROOT / 'code/client/audio/codecs/snd_codec_wav.cpp').read_text()
        # The format tags, the source and stream records and Medal of Honor's
        # IMA ADPCM decoder are production's.
        formats = source[source.index('constexpr int kWavFormatPCM'):
                         source.index('};', source.index('struct WavStreamState {')) + 2]
        decoder = source[source.index('constexpr int kImaIndexTable'):source.index('unsigned int WavLittleLong')]
        swap = source[source.index('void S_ByteSwapRawSamples'):source.index('short ConvertWavSampleToPCM16')]
        conversion = source[source.index('short ConvertWavSampleToPCM16'):source.index('/*\n=================\nS_ReadRIFFHeader')]
        load = source[source.index('extern "C" void *S_WAV_CodecLoad'):source.index('/*\n=================\nS_WAV_CodecOpenStream')]
        stream = source[source.index('extern "C" int S_WAV_CodecReadStream'):]
        harness = r'''
#include <algorithm>
#include <array>
#include <cassert>
#include <climits>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <vector>
using byte=unsigned char;
constexpr int qtrue=1;
#define S_COLOR_RED ""
''' + formats + '\n' + decoder + r'''
struct snd_info_t {int rate=44100,width=0,channels=1,samples=0,size=0,dataofs=0;unsigned int speakerMask=0;};
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
// Each block carries 16 guard bytes past its size, filled like the rest.
struct ScopedTempMemory {
 void*p;
 explicit ScopedTempMemory(int n):p(std::malloc(n+16)){++allocations;std::memset(p,0xa5,n+16);}
 ScopedTempMemory(const ScopedTempMemory&)=delete;
 ~ScopedTempMemory(){if(p){std::free(p);--allocations;}}
 static ScopedTempMemory Allocate(int n){assert(n>=0&&n<100000);return ScopedTempMemory(n);}
 template<class T>T*as(){return static_cast<T*>(p);}
 void*release(){auto*r=p;p=nullptr;--allocations;return r;}
};
bool S_ReadRIFFHeader(int,int,snd_info_t*i,WavSourceFormat*s){*i=description;*s=selected;return true;}
void Com_Printf(const char*,...){ }
bool bigEndian=false;
short LittleShort(short value){
 if(!bigEndian)return value;
 unsigned short bits;std::memcpy(&bits,&value,2);
 bits=static_cast<unsigned short>((bits>>8)|(bits<<8));
 std::memcpy(&value,&bits,2);return value;
}
''' + swap + conversion + load + stream + r'''
void reset(){cursor=0;readCalls=0;readLimit=INT_MAX;assert(allocations==0&&openHandles==0);}
bool guarded(const byte *block,int size){return std::all_of(block+size,block+size+16,[](byte v){return v==0xa5;});}
// An independent reading of the IMA ADPCM block for the checks below: each
// channel's 4-byte header (its first sample and step index), then 4-byte runs
// of each channel in turn, two samples a byte, low nibble first.
std::vector<short> ImaReference(const std::vector<byte> &data,int blockAlign,int channels,int frames){
 static const int steps[89]={7,8,9,10,11,12,13,14,16,17,19,21,23,25,28,31,34,37,41,45,
  50,55,60,66,73,80,88,97,107,118,130,143,157,173,190,209,230,253,279,307,
  337,371,408,449,494,544,598,658,724,796,876,963,1060,1166,1282,1411,1552,1707,
  1878,2066,2272,2499,2749,3024,3327,3660,4026,4428,4871,5358,5894,6484,7132,7845,
  8630,9493,10442,11487,12635,13899,15289,16818,18500,20350,22385,24623,27086,29794,32767};
 static const int shift[8]={-1,-1,-1,-1,2,4,6,8};
 std::vector<short> out(static_cast<std::size_t>(frames)*channels,0);
 int done=0;
 for(std::size_t start=0;start<data.size()&&done<frames;start+=blockAlign){
  const int bytes=static_cast<int>(std::min<std::size_t>(blockAlign,data.size()-start));
  if(bytes<4*channels)break;
  // Each channel's data bytes, counted one at a time: the channels take four
  // bytes in turn, so in a short block a later channel can hold fewer, and a
  // frame is only whole where every channel has its sample.
  int least=INT_MAX;
  for(int c=0;c<channels;++c){
   int held=0;
   for(int at=4*channels;at<bytes;++at)if((at-4*channels)/4%channels==c)++held;
   least=std::min(least,held);
  }
  const int count=std::min(1+2*least,frames-done);
  for(int c=0;c<channels;++c){
   const byte *h=data.data()+start+4*c;
   int sample=static_cast<short>(h[0]|(h[1]<<8)),index=std::min<int>(h[2],88);
   out[static_cast<std::size_t>(done)*channels+c]=static_cast<short>(sample);
   for(int n=0;n+1<count;++n){
    const std::size_t at=start+4*channels+(n/8)*4*channels+4*c+(n%8)/2;
    assert(at<start+bytes);
    const int code=n%2?data[at]>>4:data[at]&15,step=steps[index];
    int delta=step>>3;if(code&1)delta+=step>>2;if(code&2)delta+=step>>1;if(code&4)delta+=step;
    sample=std::max(-32768,std::min(32767,code&8?sample-delta:sample+delta));
    index=std::max(0,std::min(88,index+shift[code&7]));
    out[static_cast<std::size_t>(done+1+n)*channels+c]=static_cast<short>(sample);
   }
  }
  done+=count;
 }
 return out;
}
int main(){
 // 20-bit PCM in a 24-bit container is left-aligned, with four zero low bits.
 WavSourceFormat pcm20{kWavFormatPCM,3,true};
 for(int value:{-524288,-262144,-1,0,1,262144,524287}){
  const unsigned packed=static_cast<unsigned>(value*16);
  byte bytes[]{static_cast<byte>(packed),static_cast<byte>(packed>>8),static_cast<byte>(packed>>16)};
  const int expected=value>=0?value/16:-((-value+15)/16);
  assert(ConvertWavSampleToPCM16(bytes,pcm20)==expected);
 }
 WavSourceFormat pcm8{kWavFormatPCM,1,true};
 for(int value=0;value<256;++value){byte b=static_cast<byte>(value);assert(ConvertWavSampleToPCM16(&b,pcm8)==(value-128)*256);}
 for(int width:{1,2,3,4})for(int channels:{1,2,3,6,8}){
  selected={width==4?kWavFormatIEEEFloat:kWavFormatPCM,width,width>2||(width==1&&channels>1)};
  description={44100,selected.needsConversion?2:width,channels,8,8*channels*(selected.needsConversion?2:width),0};
  std::vector<byte> full(8*channels*width,0);
  for(std::size_t i=0;i<full.size();i+=width){
   if(width==4){float f=.5f;std::memcpy(full.data()+i,&f,4);}
   else full[i+width-1]=0x40;
  }
  payload=full;reset();snd_info_t info;
  auto*loaded=static_cast<byte*>(S_WAV_CodecLoad("fixture.wav",&info));assert(loaded&&guarded(loaded,info.size));
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
  // Positive short reads may split a 24-bit or float scalar at any byte.
  // They must not discard that prefix or shift the following samples.
  for(int limit=1;limit<=9;++limit)for(int request:{2,6,38,128}){
   payload=full;reset();readLimit=limit;s=makeStream();actual.clear();
   while(s.pos<s.info.size){
    std::array<byte,128> part;part.fill(0xa5);
    int got=S_WAV_CodecReadStream(&s,request,part.data());
    assert(got>0&&got<=request&&got%2==0);
    actual.insert(actual.end(),part.begin(),part.begin()+got);
    assert(std::all_of(part.begin()+got,part.end(),[](byte v){return v==0xa5;}));
   }
   assert(actual==expected&&cursor==full.size());
  }
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
 // An even byte request need not end at an interleaved frame boundary.
 bigEndian=true;
 for(int channels:{2,3,6,8}){
  selected={kWavFormatPCM,2,false};
  description={44100,2,channels,8,16*channels,0};
  payload.assign(description.size,0);
  for(int i=0;i<description.size;i+=2){payload[i]=0x12;payload[i+1]=0x34;}
  reset();snd_info_t info;
  auto*loaded=static_cast<byte*>(S_WAV_CodecLoad("endian.wav",&info));assert(loaded);
  for(int i=0;i<info.size;i+=2)assert(loaded[i]==0x34&&loaded[i+1]==0x12);
  std::free(loaded);reset();snd_stream_t s;s.info=description;
  std::array<byte,128> out;out.fill(0xa5);
  const int request=2*(channels+1);
  assert(S_WAV_CodecReadStream(&s,request,out.data())==request);
  for(int i=0;i<request;i+=2)assert(out[i]==0x34&&out[i+1]==0x12);
  for(int i=request;i<128;++i)assert(out[i]==0xa5);
  for(int limit=1;limit<=5;++limit){
   reset();readLimit=limit;s.pos=0;out.fill(0xa5);
   assert(S_WAV_CodecReadStream(&s,request,out.data())==request);
   for(int i=0;i<request;i+=2)assert(out[i]==0x34&&out[i+1]==0x12);
   for(int i=request;i<128;++i)assert(out[i]==0xa5);
  }
 }
 bigEndian=false;

 // Medal of Honor's IMA ADPCM: a load decodes every block it reads, whole or
 // short; a truncated or failed read loads nothing; frames the header counts
 // past the data are silence; and nothing lands past the buffer. Two 36-byte
 // mono blocks and a 10-byte tail are 143 frames; two 72-byte stereo blocks
 // and a tail of their two headers are 131. Stereo tails of 14 and 20 data
 // bytes give the second channel 6 and 8 bytes to the first's 8 and 12, so
 // they end at the second channel's last sample: 143 and 147 frames.
 std::uint32_t seed=0x2468ace1;
 auto nextRandom=[&seed](){seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;return seed;};
 struct Ima {int channels,blockAlign,frames;std::size_t bytes;};
 for(const Ima &ima:{Ima{1,36,143,82},Ima{2,72,131,152},Ima{2,72,143,166},Ima{2,72,147,172}}){
  std::vector<byte> data(ima.bytes);for(byte &b:data)b=static_cast<byte>(nextRandom());
  const int perBlock=1+(ima.blockAlign-4*ima.channels)*2/ima.channels;
  for(int missing:{0,5}){
   selected={kWavFormatIMAADPCM,0,true,ima.blockAlign,perBlock,ima.channels,static_cast<int>(ima.bytes)};
   const int frames=ima.frames+missing;
   description={22050,2,ima.channels,frames,frames*2*ima.channels,0};
   const std::vector<short> reference=ImaReference(data,ima.blockAlign,ima.channels,frames);
   payload=data;reset();snd_info_t info;
   auto*loaded=static_cast<byte*>(S_WAV_CodecLoad("adpcm.wav",&info));
   assert(loaded&&info.size==description.size&&guarded(loaded,info.size));
   assert(cursor==data.size()&&allocations==0&&openHandles==0);
   assert(std::memcmp(loaded,reference.data(),info.size)==0);
   std::free(loaded);
   for(std::size_t n=0;n<data.size();++n){
    payload.assign(data.begin(),data.begin()+n);reset();
    assert(S_WAV_CodecLoad("truncated.wav",&info)==nullptr);
    assert(allocations==0&&openHandles==0);
   }
   payload=data;reset();readLimit=-1;
   assert(S_WAV_CodecLoad("error.wav",&info)==nullptr);assert(allocations==0&&openHandles==0);

   // The stream reads the decode made when it opened, as far as it goes and
   // never from the file.
   std::vector<short> decoded=reference;
   for(int available:{description.size,description.size-6})for(int request:{2,6,38,128}){
    WavStreamState state{kWavFormatIMAADPCM,0,decoded.data(),available};
    reset();snd_stream_t s;s.info=description;s.ptr=&state;std::vector<byte> actual;
    for(;;){
     std::array<byte,128> part;part.fill(0xa5);
     const int got=S_WAV_CodecReadStream(&s,request,part.data());
     if(!got)break;
     assert(got>0&&got<=request&&got%2==0);
     actual.insert(actual.end(),part.begin(),part.begin()+got);
     assert(std::all_of(part.begin()+got,part.end(),[](byte v){return v==0xa5;}));
    }
    assert(static_cast<int>(actual.size())==available&&s.pos==available&&readCalls==0);
    assert(std::memcmp(actual.data(),decoded.data(),available)==0);
   }
  }
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
