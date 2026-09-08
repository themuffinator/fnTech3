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
constexpr int kWavFormatPCM=1,kWavFormatIEEEFloat=3,kWavFormatExtensible=0xfffe,FS_SEEK_CUR=1;
struct WavSourceFormat {int format=1,width=0;bool needsConversion=false;};
struct snd_info_t {int rate,width,channels,samples,size,dataofs;};
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
bool parse(Bytes bytes,bool expected,int mode=0,int fail=-1,int length=-1){
 input=bytes;pos=reads=seeks=0;seekMode=mode;failRead=fail;
 snd_info_t info{1,2,3,4,5,6},before=info;WavSourceFormat source{7,8,true};
 bool ok=S_ReadRIFFHeader(1,length==-1?static_cast<int>(input.size()):length,&info,&source);
 assert(ok==expected);
 if(!ok){assert(std::memcmp(&info,&before,sizeof(info))==0);assert(source.format==7&&source.width==8&&source.needsConversion);}
 else{assert(info.rate==22050&&info.channels>0&&info.width>0&&info.size>=0&&info.dataofs==0);}
 return ok;
}
int main(){
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
 for(auto f:{fmt(1,0),fmt(1,32768),fmt(1,1,32),fmt(3,1,16),fmt(2)})parse(riff(body(f)),false);
 for(unsigned rate:{0u,0x80000000u,0xffffffffu}){auto f=fmt();set32(f,4,rate);parse(riff(body(f)),false);}
 auto extended=fmt(0xfffe);u16(extended,22);u16(extended,16);u32(extended,0);u16(extended,1);append(extended,Bytes(14));
 parse(riff(body(extended)),true);
 for(int n=16;n<40;++n)parse(riff(body(Bytes(extended.begin(),extended.begin()+n))),false);
 auto odd=fmt();odd.push_back(0);Bytes b=chunk("JUNK",{1,2,3});append(b,chunk("zero",{}));append(b,body(odd));auto skipped=riff(b);
 parse(skipped,true,0);parse(skipped,true,1); // loose/ZIP seek conventions.
 parse(skipped,false,2);parse(skipped,false,3);
 parse(riff(body(odd)),false,2); // fmt extension seek failure.
 auto missingPad=riff(body(fmt(),{1,2,3}));missingPad.pop_back();set32(missingPad,4,missingPad.size()-8);parse(missingPad,true);
 auto missingJunkPad=chunk("JUNK",{1,2,3});missingJunkPad.pop_back();parse(riff(missingJunkPad),false);
 auto empty=riff(body(fmt(),{}));parse(empty,true);
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
