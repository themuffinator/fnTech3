"""Execute Ogg metadata validation and load cleanup with decoder fault fixtures."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]


class OggPayloadBoundsTests(unittest.TestCase):
    def test_metadata_and_short_decode_cleanup(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++ compiler unavailable')
        source = (ROOT / 'code/client/audio/codecs/snd_codec_ogg.cpp').read_text()
        functions = source[source.index('snd_stream_t *S_OGG_CodecOpenStream(const char *filename)'):source.rindex('#endif // USE_OGG_VORBIS')]
        harness = r'''
#include <cassert>
#include <climits>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <limits>
#include <initializer_list>
#include "code/client/audio/shared/AudioVorbisLayout.h"
using byte=unsigned char;using ogg_int64_t=long long;
struct snd_info_t {int rate,width,channels,samples,size,dataofs;unsigned speakerMask;};
struct snd_stream_t {snd_info_t info{};int pos=0,length=100;void*ptr=nullptr;};
struct OggVorbis_File {int unused;};
struct vorbis_info {long rate;int channels;};
constexpr int OGG_SAMPLEWIDTH=2,ogg_codec=1,S_OGG_Callbacks=1;
int handles=0,zones=0,temps=0,clears=0,decodeBytes=16,openError=0;
bool seekable=true,hasInfo=true;int links=1;ogg_int64_t frames=4;
vorbis_info metadata{48000,2};
bool channelFixture=false;int nextFrame=0;
snd_stream_t*S_CodecUtilOpen(const char*,const int*){++handles;return new snd_stream_t;}
void S_CodecUtilClose(snd_stream_t**s){delete *s;*s=nullptr;--handles;}
struct ScopedZoneMemory {
 void*p=std::calloc(1,sizeof(OggVorbis_File));
 ScopedZoneMemory(){++zones;}~ScopedZoneMemory(){if(p){std::free(p);--zones;}}
 ScopedZoneMemory(const ScopedZoneMemory&)=delete;
 template<class T>T*as(){return static_cast<T*>(p);}
 void*release(){void*r=p;p=nullptr;return r;}
};
ScopedZoneMemory AllocateZoneMemory(int,const char*,const char*,int){return ScopedZoneMemory();}
void Z_Free(void*p){std::free(p);--zones;}
struct ScopedTempMemory {
 void*p;
 explicit ScopedTempMemory(int n){assert(n>=0&&n<=1024);p=std::malloc(n?n:1);++temps;std::memset(p,0xa5,n);}
 ~ScopedTempMemory(){if(p){std::free(p);--temps;}}
 ScopedTempMemory(const ScopedTempMemory&)=delete;
 static ScopedTempMemory Allocate(int n){return ScopedTempMemory(n);}
 template<class T>T*as(){return static_cast<T*>(p);}
 void*release(){void*r=p;p=nullptr;--temps;return r;}
};
int ov_open_callbacks(snd_stream_t*,OggVorbis_File*,void*,int,int){return openError;}
int ov_seekable(OggVorbis_File*){return seekable;}
int ov_streams(OggVorbis_File*){return links;}
vorbis_info*ov_info(OggVorbis_File*,int){return hasInfo?&metadata:nullptr;}
ogg_int64_t ov_pcm_total(OggVorbis_File*,int){return frames;}
int ov_clear(OggVorbis_File*){++clears;return 0;}
int ov_read(OggVorbis_File*,char*out,int bytes,int,int,int,int*){
 if(decodeBytes<0)return -1;
 if(channelFixture){
  int frames=bytes/(metadata.channels*2);if(frames>2)frames=2;
  if(frames>decodeBytes/(metadata.channels*2))frames=decodeBytes/(metadata.channels*2);
  for(int f=0;f<frames;++f)for(int c=0;c<metadata.channels;++c){short value=(16*(nextFrame+f)+c+1)*100;std::memcpy(out+(f*metadata.channels+c)*2,&value,2);}
  int n=frames*metadata.channels*2;nextFrame+=frames;decodeBytes-=n;return n;
 }
 int n=bytes<decodeBytes?bytes:decodeBytes;if(n>3)n=3;
 std::memset(out,0x24,n);decodeBytes-=n;return n;
}
void S_OGG_CodecCloseStream(snd_stream_t*);
struct OggStreamDeleter {void operator()(snd_stream_t*s)const{S_OGG_CodecCloseStream(s);}};
''' + functions + r'''
void clean(){assert(handles==0&&zones==0&&temps==0);}
int main(){
 for(int failure=0;failure<4;++failure){
  openError=failure==0?-1:0;seekable=failure!=1;links=failure==2?2:1;hasInfo=failure!=3;
  assert(!S_OGG_CodecOpenStream("fixture.ogg"));clean();
 }
 openError=0;seekable=true;links=1;hasInfo=true;
 for(int channels:{-1,0,2,INT_MAX})for(ogg_int64_t count:{-1LL,0LL,4LL,536870911LL,536870912LL,2147483648LL,9223372036854775807LL}){
  metadata.channels=channels;frames=count;int before=clears;
  bool valid=channels>0&&channels<=INT_MAX/2&&count>=0&&count<=INT_MAX/(channels*2);
  auto*s=S_OGG_CodecOpenStream("fixture.ogg");assert(bool(s)==valid);
  if(s){assert(s->info.size==count*channels*2&&s->info.speakerMask==3);S_OGG_CodecCloseStream(s);}
  assert(clears==before+1);clean();
 }
 metadata.channels=2;frames=4;
 for(long rate:{-1L,0L}){metadata.rate=rate;assert(!S_OGG_CodecOpenStream("fixture.ogg"));clean();}
 metadata.rate=48000;
 for(int available=-1;available<=16;++available){
  decodeBytes=available;snd_info_t info;std::memset(&info,0x5a,sizeof(info));auto before=info;
  void*p=S_OGG_CodecLoad("fixture.ogg",&info);
  assert(bool(p)==(available==16));
  if(p){assert(info.size==16&&info.samples==4);for(int i=0;i<16;++i)assert(static_cast<byte*>(p)[i]==0x24);std::free(p);}
  else assert(std::memcmp(&info,&before,sizeof(info))==0);
  clean();
 }
 // Decoder packets use Vorbis order. Both streamed and whole loads expose
 // WAVE speaker order, with distinct channel/frame tags preventing a silent swap.
 const unsigned masks[]{0,4,3,7,0x33,0x37,0x3f,0x70f,0x63f};
 const int expected[9][8]={{},{0},{0,1},{0,2,1},{0,1,2,3},{0,2,1,3,4},{0,2,1,5,3,4},{0,2,1,6,5,3,4},{0,2,1,7,5,6,3,4}};
 channelFixture=true;
 for(int channels=1;channels<=8;++channels)for(bool whole:{false,true}){
  metadata.channels=channels;frames=4;decodeBytes=4*channels*2;nextFrame=0;
  snd_info_t info{};char unaligned[66];std::memset(unaligned,0x5a,sizeof(unaligned));void*data;
  if(whole)data=S_OGG_CodecLoad("speakers.ogg",&info);
  else {auto*s=S_OGG_CodecOpenStream("speakers.ogg");assert(s);info=s->info;data=unaligned+1;assert(S_OGG_CodecReadStream(s,info.size,data)==info.size);S_OGG_CodecCloseStream(s);}
  assert(data&&info.speakerMask==masks[channels]);
  for(int f=0;f<4;++f)for(int c=0;c<channels;++c){short value;std::memcpy(&value,static_cast<char*>(data)+(f*channels+c)*2,2);assert(value==(16*f+expected[channels][c]+1)*100);}
  assert(unaligned[0]==0x5a&&unaligned[65]==0x5a);
  if(whole)std::free(data);clean();
 }
 for(const char*name:{"sound/test.UHJ3.ogg","sound/B-Format-2D.ogg","sound/ambi3d.ogg"})for(int channels:{2,3,4}){
  metadata.channels=channels;frames=4;decodeBytes=4*channels*2;nextFrame=0;
  snd_info_t info{};void*data=S_OGG_CodecLoad(name,&info);assert(data&&info.speakerMask==0);
  for(int f=0;f<4;++f)for(int c=0;c<channels;++c){short value;std::memcpy(&value,static_cast<char*>(data)+(f*channels+c)*2,2);assert(value==(16*f+c+1)*100);}
  std::free(data);clean();
 }
 assert(!fntech3::audio::VorbisHasEncodedFieldTag("sound/notuhjx.ogg",3));
 assert(!fntech3::audio::VorbisHasEncodedFieldTag("sound/uhj.ogg",6));
}
'''
        with tempfile.TemporaryDirectory() as directory:
            cpp = Path(directory) / 'ogg.cpp'
            exe = Path(directory) / 'ogg.exe'
            cpp.write_text(harness)
            built = subprocess.run([compiler, *compiler_target_args(compiler), '-std=c++17', '-I', str(ROOT), str(cpp), '-o', str(exe)], capture_output=True, text=True)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            ran = subprocess.run([str(exe)], capture_output=True, text=True)
            self.assertEqual(ran.returncode, 0, ran.stdout + ran.stderr)


if __name__ == '__main__':
    unittest.main()
