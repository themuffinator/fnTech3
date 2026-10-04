"""Execute MOHAA's production music stream against bounded codec fixtures."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest
from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]
PRELUDE = r'''
#include "code/client/audio/shared/AudioStreamResampler.h"
#include "code/client/audio/shared/AliceMusic.h"
#include <cassert>
#include <cmath>
#include <cstring>
#include <string>
constexpr int kMixRate=44100,kMixChannels=2,kChunkFrames=4096;
namespace music=fntech3::audio::alice;
struct snd_info_t {int rate=0,width=0,channels=0,samples=0;};
struct snd_stream_t {snd_info_t info;size_t at=0;};
static int rate=44100,channels=2,reads=0,opens=0,closes=0,maxReads=10000;
static std::vector<short> pcm;
static snd_stream_t *S_CodecOpenStream(const char *){
 ++opens;return new snd_stream_t{{rate,2,channels,int(pcm.size()/channels)},0};
}
static void S_CodecCloseStream(snd_stream_t *s){++closes;delete s;}
static int S_CodecReadStream(snd_stream_t *s,int bytes,void *out){
 assert(++reads<=maxReads);const size_t count=(std::min)(size_t(bytes)/sizeof(short),pcm.size()-s->at);
 if(count)std::memcpy(out,pcm.data()+s->at,count*sizeof(short));s->at+=count;return int(count*sizeof(short));
}
'''


class MOHAAAudioStreamTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.source = (ROOT / 'code/client/cl_mohaa_sound.cpp').read_text(encoding='utf-8')
        start = cls.source.index('class Track {')
        cls.track = cls.source[start:cls.source.index('/* ---- the soundtrack', start)]

    def compile_run(self, source):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++ compiler unavailable')
        with tempfile.TemporaryDirectory() as temp:
            folder = Path(temp)
            cpp, exe = folder / 'mohaa.cpp', folder / 'mohaa.exe'
            cpp.write_text(source, encoding='utf-8')
            built = subprocess.run([compiler, *compiler_target_args(compiler), '-std=c++17', '-O1',
                                    '-I', str(ROOT), str(cpp), '-o', str(exe)],
                                   cwd=ROOT, capture_output=True, text=True, timeout=120)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            options = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
            run = subprocess.run([str(exe)], cwd=ROOT, capture_output=True, text=True, timeout=30, **options)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)

    def test_empty_and_fully_skipped_loops_stop_without_reopening_forever(self):
        self.compile_run(PRELUDE + self.track + r'''
int main(){
 for(int count:{0,32}){
  pcm.assign(size_t(count)*channels,1234);reads=opens=closes=0;maxReads=8;
  Track track;assert(track.Open("fixture",true,100));float out[2]{};track.Mix(1,out,1);
  assert(!track.Playing()&&out[0]==0&&out[1]==0&&opens==closes);
 }
}
''')

    def test_natural_endings_and_short_loops_retain_pcm_and_exact_counts(self):
        self.compile_run(PRELUDE + self.track + r'''
int main(){
 for(int frequency:{8000,22050,44100,48000})for(int count:{0,1,2,3,17,100})for(int block:{1,7,257}){
  rate=frequency;reads=0;pcm.assign(size_t(count)*channels,1234);
  Track track;assert(track.Open("fixture",false,0));const int length=(count*44100+rate-1)/rate;
  std::vector<float> out(size_t(length+4)*2);
  for(int at=0;at<length+4;at+=block)track.Mix(1,out.data()+size_t(at)*2,(std::min)(block,length+4-at));
  assert(!track.Playing());for(int i=0;i<length*2;++i)assert(out[i]==1234);
  for(size_t i=size_t(length)*2;i<out.size();++i)assert(out[i]==0);
 }
 for(int frequency:{8000,22050,44100,48000}){
  rate=frequency;reads=0;pcm.assign(128,1234);Track track;assert(track.Open("fixture",true,0));
  std::vector<float> out(2000);track.Mix(1,out.data(),1000);assert(track.Playing());
  for(float v:out)assert(v==1234);
 }
}
''')

    def test_positions_and_remaining_follow_playback_instead_of_decode_ahead(self):
        self.compile_run(PRELUDE + self.track + r'''
int main(){
 for(int frequency:{8000,22050,44100,48000}){
  rate=frequency;reads=0;pcm.assign(size_t(rate)*channels,1234);
  Track track;assert(track.Open("fixture",false,100));float out[2]{};
  for(int i=1;i<=200;++i){
   track.Mix(1,out,1);const int position=100+int(int64_t(i)*rate/44100);
   assert(track.Position()==position&&track.RemainingMs()==int((int64_t(rate)-position)*1000/rate));
  }
 }
}
''')

    def test_mix_pcm_rejects_null_saturates_finite_values_and_handles_unaligned_output(self):
        start = self.source.index('int MixRead( snd_stream_t *, int bytes, void *buffer ) {')
        conversion = self.source[start:self.source.index('\nvoid MixClose', start)]
        self.compile_run(r'''
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <vector>
using byte=unsigned char;struct snd_stream_t {};
constexpr int kMixChannels=2;
namespace music{constexpr int NumPlayers=2;}
static float values[]={std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity(),
 -std::numeric_limits<float>::infinity(),1e20f,-1e20f,123.9f};
struct FakeTrack{bool Playing(){return false;}void Mix(float,float *out,int frames){
 for(int i=0;i<frames*2;++i)out[i]+=values[i%6];}};
static struct{FakeTrack tracks[2];}songs;
static struct{struct Slot{float gain=1;};Slot SlotAt(int){return {};}}soundtrack;
static FakeTrack triggered;
static std::vector<float> mixScratch;
static bool tracksActive=true;
static bool AnythingPlays(){return tracksActive;}
''' + conversion + r'''
int main(){
 assert(MixRead(nullptr,12,nullptr)==0);byte output[14];std::memset(output,0x4f,sizeof(output));
 assert(MixRead(nullptr,-1,output)==0&&MixRead(nullptr,3,output)==0);
 assert(MixRead(nullptr,12,output+1)==12);const short expected[]={0,0,0,32767,-32768,123};
 assert(output[0]==0x4f&&output[13]==0x4f);
 for(size_t i=0;i<6;++i){short value;std::memcpy(&value,output+1+i*2,sizeof(value));assert(value==expected[i]);}
 tracksActive=false;std::memset(output,0x4f,sizeof(output));
 assert(MixRead(nullptr,12,output+1)==0);for(byte value:output)assert(value==0x4f);
}
''')

    def test_failed_or_incompatible_loop_reopen_drains_accepted_tail(self):
        prelude = PRELUDE.replace('static std::vector<short> pcm;', 'static std::vector<short> pcm;static bool failOpen=false,changeFormat=false;')
        prelude = prelude.replace(
            '++opens;return new snd_stream_t{{rate,2,channels,int(pcm.size()/channels)},0};',
            '++opens;if(opens>1&&failOpen)return nullptr;return new snd_stream_t{{opens>1&&changeFormat?48000:rate,2,channels,int(pcm.size()/channels)},0};')
        self.compile_run(prelude + self.track + r'''
int main(){
 for(bool incompatible:{false,true}){
  opens=closes=reads=0;rate=22050;pcm.assign(200,1234);failOpen=!incompatible;changeFormat=incompatible;
  Track track;assert(track.Open("fixture",true,0));float frame[2]{};
  for(int i=0;i<200;++i){frame[0]=frame[1]=0;assert(track.Playing());track.Mix(1,frame,1);assert(frame[0]==1234&&frame[1]==1234);}
  assert(!track.Playing()&&opens==2&&closes==(incompatible?2:1));
  assert(track.Position()==100&&track.RemainingMs()==0);
 }
}
''')
