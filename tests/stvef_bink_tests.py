"""Execute Bink framing and the installed-runtime boundary without retail inputs."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]

FIXTURE = r'''
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>
#include <utility>
#include <vector>
#include "code/compat/stvef_bink_file.hpp"
using namespace fntech3::stvef::bink;
static void Put(std::vector<std::uint8_t>& b, size_t at, std::uint32_t value) {
 for(int i=0;i<4;++i)b.at(at+i)=static_cast<std::uint8_t>(value>>(8*i));
}
static std::vector<std::uint8_t> File(unsigned tracks=0,unsigned frames=3) {
 const unsigned index=44+tracks*12, first=index+(frames+1)*4, frameSize=tracks*8+4;
 std::vector<std::uint8_t>b(first+frames*frameSize);
 Put(b,0,0x694b4942);Put(b,4,b.size()-8);Put(b,8,frames);Put(b,12,frameSize);Put(b,16,frames);
 Put(b,20,512);Put(b,24,384);Put(b,28,15);Put(b,32,1);Put(b,40,tracks);
 for(unsigned t=0;t<tracks;++t){Put(b,44+t*4,8192);Put(b,44+tracks*4+t*4,44100);Put(b,44+tracks*8+t*4,5+t*2);}
 for(unsigned f=0;f<=frames;++f)Put(b,index+f*4,(first+f*frameSize)|(f==0?1:0));
 for(unsigned f=0;f<frames;++f)for(unsigned t=0;t<tracks;++t){Put(b,first+f*frameSize+t*8,4);Put(b,first+f*frameSize+t*8+4,4096);}
 return b;
}
'''

FRAMING = r'''
int main() {
 FileInfo sentinel{11,22,33,44,55,66,77}, info=sentinel;
 auto Reject=[&](const std::vector<std::uint8_t>& b){info=sentinel;assert(!Inspect(b.data(),b.size(),false,info));assert(!memcmp(&info,&sentinel,sizeof(info)));};
 for(unsigned tracks:{0,1,2,16}){
  auto b=File(tracks);assert(Inspect(b.data(),b.size(),false,info));
  assert(info.width==512&&info.height==384&&info.frames==3&&info.rate==15&&info.rateDivisor==1);
  assert(info.tracks==tracks&&info.soundTrack==(tracks?5:0));
  assert(Inspect(b.data(),b.size(),true,info));assert(info.soundTrack==(tracks>1?7:tracks?5:0));
 }
 auto valid=File(2);
 for(size_t length=0;length<valid.size();++length){auto b=valid;b.resize(length);Reject(b);}
 for(auto item:std::vector<std::pair<unsigned,unsigned>>{{0,0},{4,0},{8,0},{8,1000001},{12,0},{12,0xffffffff},{16,4},
   {20,0},{20,2049},{24,0},{24,2049},{28,0},{28,241},{28,1000001},{32,0},{32,16},{40,17},{44,0},{44,0xffffffff},{52,0}}){
  auto b=valid;Put(b,item.first,item.second);Reject(b);
 }
 for(unsigned scale=0;scale<8;++scale){auto b=File();Put(b,36,scale<<28);
  if(scale>5){Reject(b);continue;}
  assert(Inspect(b.data(),b.size(),false,info));
  assert(info.width==(scale>=3?1024:512));
  assert(info.height==((scale==1||scale==2||scale>=4)?768:384));
 }
 auto b=File();Put(b,20,2048);Put(b,36,3u<<28);Reject(b);
 b=File();Put(b,24,2048);Put(b,36,1u<<28);Reject(b);
 const unsigned index=68,first=index+16;
 for(unsigned value:{0u,first-2,first,static_cast<unsigned>(valid.size()+2),0xffffffffu}){
  b=valid;Put(b,index+4,value);Reject(b);
 }
 b=valid;Put(b,index,first-2);Reject(b);
 b=valid;Put(b,index+12,valid.size()-2);Reject(b);
 b=valid;Put(b,12,19);Reject(b);
 for(unsigned value:{1u,3u,100u,0xffffffffu}){b=valid;Put(b,first,value);Reject(b);}
 b=valid;Put(b,first+4,8193);Reject(b);
 b=valid;Put(b,first+8,0xffffffff);Reject(b); // Second track must fit too.
 b=valid;Put(b,first+8,8);Reject(b); // Audio consuming the entire frame leaves no video.
 assert(!Inspect(nullptr,44,false,info));
 assert(!Inspect(valid.data(),MaxFileBytes+1,false,info));
 // Deterministic mutations exercise every header/index/packet byte. Accepted
 // mutations need not decode; the native bitstreams are outside this parser.
 std::uint32_t seed=0x12345678;
 for(unsigned n=0;n<10000;++n){b=valid;seed=seed*1664525+1013904223;b[seed%b.size()]^=static_cast<std::uint8_t>(seed>>24);Inspect(b.data(),b.size(),false,info);}
}
'''

NATIVE = r'''
static void *Sys_LoadLibrary(const char *);
static void *Sys_LoadFunction(void *,const char *);
static void Sys_UnloadLibrary(void *);
// Exercise the same independently declared ABI with recording functions.
// No platform DLL, compressed movie or proprietary header is needed.
#if !defined(_WIN32)
#define _WIN32
#define __stdcall
#endif
#if !defined(_M_IX86)
#define _M_IX86
#endif
#include "code/client/cinematic_bink.hpp"
static int loads,unloads,opened,closed,waits,decoded,copied,advances,soundCalls,vol,pauses,pauseState;
static bool failLoad,failOpen,badMetadata,waiting,skipFrame,soundSuccess=true;
static const char *missing=nullptr;
static std::uint32_t selected;
static NativePrefix native;
static NativePrefix *__stdcall Open(const void *data,std::uint32_t flags){
 assert(flags==0x04004000&&data);++opened;
 if(failOpen)return nullptr;
 native={512,384,3,1,0,15,1};if(badMetadata)native.width=1024;return &native;
}
static void __stdcall Close(NativePrefix *p){assert(p==&native);++closed;}
static std::int32_t __stdcall Wait(NativePrefix *p){assert(p==&native);++waits;return waiting;}
static std::int32_t __stdcall Decode(NativePrefix *p){assert(p==&native);++decoded;return skipFrame;}
static void __stdcall Next(NativePrefix *p){assert(p==&native);++advances;++p->frame;}
static void __stdcall Copy(NativePrefix *p,void *pixels,std::int32_t pitch,std::uint32_t rows,std::uint32_t x,std::uint32_t y,std::uint32_t flags){
 assert(p==&native&&pitch==2048&&rows==384&&!x&&!y&&flags==0x70000004);++copied;
 memset(pixels,static_cast<int>(p->frame),pitch*rows);
}
static void __stdcall Track(std::uint32_t track){selected=track;}
static void *__stdcall DirectSound(std::uint32_t value){assert(!value);return reinterpret_cast<void *>(2);}
static std::int32_t __stdcall Sound(void *(__stdcall *factory)(std::uint32_t),std::uint32_t value){assert(factory==DirectSound&&!value);++soundCalls;return soundSuccess;}
static void __stdcall Volume(NativePrefix *p,std::int32_t value){assert(p==&native);vol=value;}
static std::int32_t __stdcall Pause(NativePrefix *p,std::int32_t value){assert(p==&native);++pauses;pauseState=value;return value;}
static const char *__stdcall Error(){return "native error";}
static void *Sys_LoadLibrary(const char *path){assert(!strcmp(path,"installation/binkw32.dll"));++loads;return failLoad?nullptr:reinterpret_cast<void *>(1);}
static void Sys_UnloadLibrary(void *lib){assert(lib==reinterpret_cast<void *>(1));++unloads;}
static void *Sys_LoadFunction(void *lib,const char *name){
 assert(lib==reinterpret_cast<void *>(1));if(missing&&!strcmp(missing,name))return nullptr;
#define EXPORT(symbol,fn) if(!strcmp(name,symbol))return reinterpret_cast<void *>(fn)
 EXPORT("_BinkOpen@8",Open);EXPORT("_BinkClose@4",Close);EXPORT("_BinkWait@4",Wait);EXPORT("_BinkDoFrame@4",Decode);
 EXPORT("_BinkNextFrame@4",Next);EXPORT("_BinkCopyToBuffer@28",Copy);EXPORT("_BinkSetSoundTrack@4",Track);
 EXPORT("_BinkSetSoundSystem@8",Sound);EXPORT("_BinkOpenDirectSound@4",DirectSound);EXPORT("_BinkSetVolume@8",Volume);EXPORT("_BinkGetError@0",Error);
 EXPORT("_BinkPause@8",Pause);
#undef EXPORT
 assert(false);return nullptr;
}
int main(){
 const char *path="installation/binkw32.dll";
 Movie movie;
 assert(!movie.Open({},path,false,false)&&!loads&&!movie.Pixels());
 failLoad=true;assert(!movie.Open(File(),path,false,false)&&loads==1&&!unloads);failLoad=false;
 for(const char *name:{"_BinkOpen@8","_BinkClose@4","_BinkWait@4","_BinkDoFrame@4","_BinkNextFrame@4","_BinkCopyToBuffer@28",
  "_BinkSetSoundTrack@4","_BinkSetSoundSystem@8","_BinkOpenDirectSound@4","_BinkSetVolume@8","_BinkPause@8","_BinkGetError@0"}){
  missing=name;int before=unloads;assert(!movie.Open(File(),path,false,false)&&unloads==before+1);movie.Close();assert(unloads==before+1);
 }
 missing=nullptr;failOpen=true;assert(!movie.Open(File(),path,false,false)&&std::string(movie.Error())=="native error");
 movie.Close();failOpen=false;badMetadata=true;
 int before=closed;assert(!movie.Open(File(),path,false,false)&&closed==before+1);movie.Close();assert(closed==before+1);badMetadata=false;
 assert(movie.Open(File(2),path,true,true)&&movie.Sound()&&soundCalls==1&&selected==7&&!movie.Pixels());
 movie.Volume(-1);assert(vol==0);movie.Volume(std::numeric_limits<float>::quiet_NaN());assert(vol==0);
 movie.Volume(0.5f);assert(vol==16384);movie.Volume(100);assert(vol==32768);
 waiting=true;assert(movie.Step(100)==Movie::Result::Frame&&decoded==1&&!waits&&native.frame==2&&movie.Pixels()[0]==1);
 movie.Pause(true,100);movie.Pause(true,200);
 assert(pauses==1&&pauseState==1&&movie.Step(1100)==Movie::Result::Waiting&&decoded==1&&!waits);
 movie.Pause(false,1100);movie.Pause(false,1101);assert(pauses==2&&!pauseState);
 assert(movie.Step(101)==Movie::Result::Waiting&&waits==1&&decoded==1);
 waiting=false;skipFrame=true;assert(movie.Step(167)==Movie::Result::Waiting&&native.frame==3&&copied==1&&movie.Pixels()[0]==1);
 skipFrame=false;assert(movie.Step(234)==Movie::Result::Frame&&native.frame==3&&copied==2&&movie.Pixels()[0]==3);
 assert(movie.Step(300)==Movie::Result::Waiting);assert(movie.Step(301)==Movie::Result::End);
 assert(movie.Rewind()&&native.frame==1&&!movie.Pixels());
 movie.Step(0xffffff00u);movie.Step(0xffffff50u);movie.Step(0xfffffff0u);
 // Pause the last frame across clock wrap; its remaining duration survives.
 movie.Pause(true,0xfffffff5u);assert(movie.Step(0x800)==Movie::Result::Waiting);
 movie.Pause(false,0x3ddu);
 assert(movie.Step(0x408)==Movie::Result::Waiting&&movie.Step(0x428)==Movie::Result::End);
 movie.Pause(true,0x500);assert(movie.Rewind());assert(movie.Step(0x600)==Movie::Result::Frame);
 movie.Pause(true,0x600);
 before=closed;movie.Close();movie.Close();assert(closed==before+1&&!movie.Pixels()&&!movie.Sound());
 assert(movie.Step(0)==Movie::Result::End&&!movie.Rewind());
 before=pauses;movie.Pause(true,0);movie.Pause(false,0);assert(pauses==before);
 assert(movie.Open(File(1),path,true,false)&&!movie.Sound()&&selected==0xffffffffu&&soundCalls==1);
 soundSuccess=false;assert(movie.Open(File(1),path,true,true)&&!movie.Sound()&&selected==0xffffffffu&&soundCalls==2);
 skipFrame=true;assert(movie.Step(0)==Movie::Result::End&&!movie.Pixels()&&*movie.Error());
 movie.Close();assert(unloads==loads-1);
}
'''


class STVEFBinkTests(unittest.TestCase):
    def execute(self, body, sanitize=False):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++17 compiler unavailable')
        (ROOT / '.tmp').mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='stvef-bink-', dir=ROOT / '.tmp') as folder:
            source = Path(folder) / 'probe.cpp'
            binary = Path(folder) / ('probe.exe' if os.name == 'nt' else 'probe')
            source.write_text(FIXTURE + body, encoding='utf-8')
            options = ['-fsanitize=address,undefined', '-fno-sanitize-recover=all'] if sanitize and os.name != 'nt' else []
            built = subprocess.run([compiler, *compiler_target_args(compiler), *options, '-std=c++17', '-O1',
                                    '-I', str(ROOT), str(source), '-o', str(binary)],
                                   cwd=ROOT, capture_output=True, text=True, timeout=120)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            creation = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
            ran = subprocess.run([str(binary)], cwd=ROOT, capture_output=True, text=True, timeout=30, **creation)
            self.assertEqual(ran.returncode, 0, ran.stdout + ran.stderr)

    def test_framing_track_selection_scaling_and_malformed_files(self):
        self.execute(FRAMING, sanitize=True)

    def test_runtime_lifecycle_clock_volume_skips_and_partial_initialization(self):
        self.execute(NATIVE)

    def test_other_platforms_report_unavailable_without_loading_a_runtime(self):
        self.execute(r'''
#undef _WIN32
#undef _M_IX86
#undef __i386__
#include "code/client/cinematic_bink.hpp"
int main(){
 Movie movie;assert(!movie.Open(File(),"unavailable.dll",false,false));
 assert(std::string(movie.Error()).find("Windows x86")!=std::string::npos);
 assert(!movie.Pixels()&&!movie.Sound()&&!movie.Rewind()&&movie.Step(0)==Movie::Result::End);
 movie.Close();movie.Close();
}
''')


if __name__ == '__main__':
    unittest.main()
