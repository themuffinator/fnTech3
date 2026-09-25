"""Execute production WAV container parsing against malformed RIFF fixtures."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]


class WavHeaderBoundsTests(unittest.TestCase):
    def test_riff_chunks_and_short_reads_are_bounded(self):
        cc = shutil.which('clang++') or shutil.which('g++')
        if not cc:
            self.skipTest('C++ compiler unavailable')
        text = (ROOT/'code/client/audio/codecs/snd_codec_wav.cpp').read_text()
        # The format tags and the parsed source description are production's,
        # as is the frame count of an IMA ADPCM block (Medal of Honor's).
        formats = text[text.index('constexpr int kWavFormatPCM'):
                       text.index('};', text.index('struct WavSourceFormat {')) + 2]
        block_frames = text.index('int ImaAdpcmBlockFrames(')
        block_frames = text[block_frames:text.index('\n}\n', block_frames) + 3]
        helpers = text[text.index('unsigned int WavLittleLong'):text.index('/*\n=================\nS_ByteSwapRawSamples')]
        parser = text[text.index('qboolean S_ReadRIFFHeader'):text.index('\n} // namespace')]
        source = r'''
#include <algorithm>
#include <array>
#include <cassert>
#include <climits>
#include <cstring>
#include <vector>
using byte=unsigned char;using fileHandle_t=int;using qboolean=bool;
constexpr bool qfalse=false,qtrue=true;
constexpr int FS_SEEK_CUR=1;
''' + formats + '\n' + block_frames + r'''
int mohaa=0;
int Com_GameIsMOHAA(){return mohaa;}
struct snd_info_t {int rate,width,channels,samples,size,dataofs;unsigned int speakerMask=0;};
std::vector<byte> input;int pos=0,reads=0,seeks=0,failRead=-1,seekMode=0;
int FileRead(int,void*out,int n){
 assert(n>=0&&n<=40);++reads;
 int available=static_cast<int>(input.size())-pos;
 int got=std::min(n,available);
 if(reads==failRead)got=std::max(0,got-1);
 if(got)std::memcpy(out,input.data()+pos,got);
 pos+=got;return got;
}
int FS_FTell(int){return pos;}
int FS_Seek(int,int n,int){
 assert(n>=0);++seeks;
 if(seekMode==2)return -1;
 if(seekMode==3)return n; // ZIP-like success result without moving.
 if(n>static_cast<int>(input.size())-pos)return -1;
 pos+=n;return seekMode==1?n:0;
}
''' + helpers + parser + r'''
using Bytes=std::vector<byte>;
void u16(Bytes&b,unsigned v){b.push_back(v&255);b.push_back((v>>8)&255);}
void u32(Bytes&b,unsigned v){for(int i=0;i<4;++i)b.push_back((v>>(8*i))&255);}
void set32(Bytes&b,int offset,unsigned v){for(int i=0;i<4;++i)b[offset+i]=(v>>(8*i))&255;}
void tag(Bytes&b,const char*t){b.insert(b.end(),t,t+4);}
Bytes chunk(const char*t,Bytes data){Bytes b;tag(b,t);u32(b,data.size());b.insert(b.end(),data.begin(),data.end());if(data.size()&1)b.push_back(0);return b;}
void append(Bytes&b,const Bytes&v){b.insert(b.end(),v.begin(),v.end());}
Bytes fmt(int format=1,int channels=1,int bits=16){Bytes b;u16(b,format);u16(b,channels);u32(b,22050);u32(b,22050*channels*(bits/8));u16(b,channels*(bits/8));u16(b,bits);return b;}
Bytes riff(Bytes body){Bytes b;tag(b,"RIFF");u32(b,body.size()+4);tag(b,"WAVE");append(b,body);return b;}
Bytes body(Bytes format,Bytes samples=Bytes(16,0)){auto b=chunk("fmt ",format);append(b,chunk("data",samples));return b;}
snd_info_t lastInfo;WavSourceFormat lastSource;
bool parse(Bytes bytes,bool expected,int mode=0,int fail=-1,int length=-1,int expectedWidth=0){
 input=bytes;pos=reads=seeks=0;seekMode=mode;failRead=fail;
 snd_info_t info{1,2,3,4,5,6},before=info;WavSourceFormat source{7,8,true,9,10,11,12};
 bool ok=S_ReadRIFFHeader(1,length==-1?static_cast<int>(input.size()):length,&info,&source);
 assert(ok==expected);
 if(!ok){assert(std::memcmp(&info,&before,sizeof(info))==0);assert(source.format==7&&source.width==8&&source.needsConversion);
  assert(source.blockAlign==9&&source.samplesPerBlock==10&&source.channels==11&&source.dataSize==12);}
 else{if(expectedWidth)assert(info.width==expectedWidth);assert(info.rate==22050&&info.channels>0&&info.width>0&&info.size>=0&&info.dataofs==0);
 unsigned expectedMask=bytes.size()>=60 && bytes[20]==0xfe && bytes[21]==0xff ? WavLittleLong(bytes.data()+40) : 0;assert(info.speakerMask==expectedMask);
 // Only an IMA ADPCM source carries block fields.
 if(source.format!=0x11)assert(!source.blockAlign&&!source.samplesPerBlock&&!source.channels&&!source.dataSize);
 lastInfo=info;lastSource=source;}
 return ok;
}
// An IMA ADPCM fmt: its 20 bytes end with the extension size and the frames a block holds.
Bytes adpcm(int channels,int blockAlign,int samplesPerBlock,int bits=4,Bytes extra={}){
 Bytes b;u16(b,0x11);u16(b,channels);u32(b,22050);u32(b,11100*channels);u16(b,blockAlign);u16(b,bits);
 u16(b,2+static_cast<unsigned>(extra.size()));u16(b,samplesPerBlock);append(b,extra);return b;
}
int main(){
 // Every PCM, float and extensible rule holds alike whether or not the
 // profile is Medal of Honor's.
 for(int profile:{0,1}){mohaa=profile;
 auto valid=riff(body(fmt()));parse(valid,true);
 for(std::size_t n=0;n<valid.size();++n)parse(Bytes(valid.begin(),valid.begin()+n),false);
 for(int failure=1;failure<=4;++failure)parse(valid,false,0,failure);
 for(unsigned value:{0u,3u,0x7fffffffu,0xffffffffu}){auto v=valid;set32(v,4,value);parse(v,false);}
 for(int offset:{0,8}){auto v=valid;v[offset]='X';parse(v,false);}
 auto outside=valid;set32(outside,4,28);parse(outside,false); // data exists only outside RIFF.
 auto trailing=valid;trailing.insert(trailing.end(),32,0xff);parse(trailing,true);
 parse(valid,false,0,-1,-2);
 for(const char*t:{"fmt ","JUNK","data"})for(unsigned n:{0x7fffffffu,0x80000000u,0xffffffffu}){
  Bytes b;if(std::strcmp(t,"data")==0)b=chunk("fmt ",fmt());tag(b,t);u32(b,n);append(b,Bytes(16));parse(riff(b),false);
 }
 for(int n=0;n<16;++n)parse(riff(body(Bytes(n))),false);
 for(int width:{8,16,24})for(int channels:{1,2})parse(riff(body(fmt(1,channels,width))),true);
 parse(riff(body(fmt(3,2,32))),true);
 parse(riff(body(fmt(1,2,8))),true,0,-1,-1,2);
 parse(riff(body(fmt(1,1,8))),true,0,-1,-1,1);
 // A physically reported large PCM8 payload must not overflow decoded size.
 auto huge=riff(body(fmt(1,2,8),{}));set32(huge,4,0x40000024u);set32(huge,40,0x40000000u);
 parse(huge,false,0,-1,0x4000002c);

 for(auto f:{fmt(1,0),fmt(1,32768),fmt(1,1,32),fmt(3,1,16),fmt(2)})parse(riff(body(f)),false);
 for(unsigned rate:{0u,0x80000000u,0xffffffffu}){auto f=fmt();set32(f,4,rate);parse(riff(body(f)),false);}
 auto extended=fmt(0xfffe);u16(extended,22);u16(extended,16);u32(extended,0);u16(extended,1);append(extended,{0,0,0,0,0x10,0,0x80,0,0,0xaa,0,0x38,0x9b,0x71});
 parse(riff(body(extended)),true);
 for(unsigned mask:{0u,3u,5u,0x3fu,0x80000000u}){auto f=extended;set32(f,20,mask);parse(riff(body(f)),true);}
 // Valid precision may be smaller than the byte-aligned container, never larger.
 for(int bits:{8,16,24,32}){
  auto f=extended;f[14]=bits;f[24]=bits==32?3:1;
  for(int precision:{0,1,bits-1,bits}){f[18]=precision;f[19]=0;parse(riff(body(f)),true);}
  for(unsigned precision:{static_cast<unsigned>(bits+1),255u,256u,65535u}){
   f[18]=precision&255;f[19]=(precision>>8)&255;parse(riff(body(f)),false);
  }
 }

 for(int i=26;i<40;++i){auto f=extended;f[i]^=1;parse(riff(body(f)),false);}
 for(unsigned count:{0u,1u,21u,23u,65535u}){auto f=extended;f[16]=count&255;f[17]=(count>>8)&255;parse(riff(body(f)),false);}
 auto longer=extended;longer[16]=24;append(longer,{0,0});parse(riff(body(longer)),true);
 auto floating=extended;floating[24]=3;floating[14]=32;floating[18]=32;parse(riff(body(floating)),true);
 auto pcm24=extended;pcm24[14]=24;pcm24[18]=20;parse(riff(body(pcm24)),true);
 auto unknown=extended;unknown[24]=2;parse(riff(body(unknown)),false);

 for(int n=16;n<40;++n)parse(riff(body(Bytes(extended.begin(),extended.begin()+n))),false);
 auto odd=fmt();odd.push_back(0);Bytes b=chunk("JUNK",{1,2,3});append(b,chunk("zero",{}));append(b,body(odd));auto skipped=riff(b);
 parse(skipped,true,0);parse(skipped,true,1); // loose/ZIP seek conventions.
 parse(skipped,false,2);parse(skipped,false,3);
 parse(riff(body(odd)),false,2); // fmt extension seek failure.
 auto missingPad=riff(body(fmt(),{1,2,3}));missingPad.pop_back();set32(missingPad,4,missingPad.size()-8);parse(missingPad,true);
 auto missingJunkPad=chunk("JUNK",{1,2,3});missingJunkPad.pop_back();parse(riff(missingJunkPad),false);
 auto empty=riff(body(fmt(),{}));parse(empty,true);
 }

 // IMA ADPCM (tag 0x11) is Medal of Honor's alone. Two full 36-byte mono
 // blocks of 65 frames and a 10-byte tail of 13 describe 143 16-bit frames.
 const auto mono=riff(body(adpcm(1,36,65),Bytes(36*2+10,0)));
 const auto stereo=riff(body(adpcm(2,72,65),Bytes(72+8,0)));
 mohaa=0;parse(mono,false);parse(stereo,false);
 mohaa=1;
 parse(mono,true);
 assert(lastInfo.channels==1&&lastInfo.width==2&&lastInfo.samples==143&&lastInfo.size==286&&!lastInfo.speakerMask);
 assert(lastSource.format==0x11&&lastSource.width==0&&lastSource.needsConversion);
 assert(lastSource.blockAlign==36&&lastSource.samplesPerBlock==65&&lastSource.channels==1&&lastSource.dataSize==82);
 // A stereo tail of just its two headers is one frame.
 parse(stereo,true);
 assert(lastInfo.channels==2&&lastInfo.samples==66&&lastInfo.size==264&&lastSource.dataSize==80);
 assert(lastSource.blockAlign==72&&lastSource.samplesPerBlock==65&&lastSource.channels==2);
 // A stereo tail's channels take four data bytes in turn, so 14 or 20 of them
 // leave the second channel 6 or 8: 13 or 17 frames, not 15 or 21.
 parse(riff(body(adpcm(2,72,65),Bytes(72+8+14,0))),true);assert(lastInfo.samples==78&&lastInfo.size==312);
 parse(riff(body(adpcm(2,72,65),Bytes(72+8+20,0))),true);assert(lastInfo.samples==82&&lastInfo.size==328);
 // A whole block is whole turns of every channel: 76 stereo bytes are not,
 // whichever frame count they claim.
 for(int frames:{65,69})parse(riff(body(adpcm(2,76,frames))),false);
 // A tail shorter than its header decodes to nothing; one of exactly a header to one frame.
 parse(riff(body(adpcm(1,36,65),Bytes(36+3,0))),true);assert(lastInfo.samples==65);
 parse(riff(body(adpcm(1,36,65),Bytes(4,0))),true);assert(lastInfo.samples==1&&lastInfo.size==2);
 for(int n:{0,1,3})parse(riff(body(adpcm(1,36,65),Bytes(n,0))),false);
 // Extra fmt bytes, even or odd, are skipped by either seek convention.
 for(const Bytes &extra:{Bytes{1,2},Bytes{1}}){
  auto longer=riff(body(adpcm(1,36,65,4,extra),Bytes(36,0)));
  parse(longer,true,0);parse(longer,true,1);assert(lastInfo.samples==65);parse(longer,false,2);
 }
 for(std::size_t n=0;n<mono.size();++n)parse(Bytes(mono.begin(),mono.begin()+n),false);
 for(int failure=1;failure<=5;++failure)parse(mono,false,0,failure);
 // Four-bit samples, one or two channels, and a block past its headers
 // whose size and frame count agree.
 for(int bits:{0,3,8,16})parse(riff(body(adpcm(1,36,65,bits))),false);
 for(int channels:{0,3})parse(riff(body(adpcm(channels,36,65))),false);
 for(int align:{0,4})parse(riff(body(adpcm(1,align,1))),false);
 parse(riff(body(adpcm(2,8,1))),false);
 for(int frames:{0,64,66,65535})parse(riff(body(adpcm(1,36,frames))),false);
 // The fmt must hold the frame count.
 parse(riff(body(fmt(0x11,1,4))),false);
 {auto f=fmt(0x11,1,4);u16(f,0);parse(riff(body(f)),false);}
 // A declared payload whose decoded size would overflow is refused.
 auto huge=riff(body(adpcm(1,36,65),{}));set32(huge,4,0x40000028u);set32(huge,44,0x40000000u);
 parse(huge,false,0,-1,0x40000030);
}
'''
        with tempfile.TemporaryDirectory() as temp:
            cpp=Path(temp)/'wav_header.cpp';exe=Path(temp)/'wav_header.exe';cpp.write_text(source)
            result=subprocess.run([cc,*compiler_target_args(cc),'-std=c++17',str(cpp),'-o',str(exe)],capture_output=True,text=True)
            self.assertEqual(result.returncode,0,result.stdout+result.stderr)
            result=subprocess.run([str(exe)],capture_output=True,text=True)
            self.assertEqual(result.returncode,0,result.stdout+result.stderr)


if __name__=='__main__':
    unittest.main()
