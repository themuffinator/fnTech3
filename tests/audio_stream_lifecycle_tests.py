"""Exercise the production OpenAL stream and music service with fault fixtures."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args
from rtcw_music_fixture import function

ROOT = Path(__file__).resolve().parents[1]


class AudioStreamLifecycleTests(unittest.TestCase):
    def execute(self, source):
        compiler = shutil.which("clang++") or shutil.which("g++")
        if not compiler:
            self.skipTest("C++ compiler unavailable")
        with tempfile.TemporaryDirectory() as directory:
            cpp = Path(directory) / "stream.cpp"
            exe = Path(directory) / "stream.exe"
            cpp.write_text(source, encoding="utf-8")
            result = subprocess.run(
                [compiler, *compiler_target_args(compiler), "-std=c++17", "-I", str(ROOT),
                 str(cpp), "-o", str(exe)], capture_output=True, text=True,
            )
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            result = subprocess.run([str(exe)], capture_output=True, text=True, timeout=30)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_deferred_start_underrun_queue_failure_and_recycling(self):
        stream = (ROOT / "code/client/audio/openal/AudioSystemStreams.inl").read_text()
        implementation = stream[stream.index("struct StreamConversionState"):
                                stream.index("class VoiceChatPlayer")]
        self.execute(r'''
#include "code/client/audio/shared/AudioStreamResampler.h"
#include "code/client/audio/shared/RTCWSpeech.h"
#include <cassert>
#include <cmath>
#include <deque>
#include <map>
#include <limits>
#include <utility>
using ALuint=unsigned;using ALint=int;using ALenum=int;using ALsizei=int;
enum {AL_NO_ERROR,AL_BUFFER,AL_BUFFERS_PROCESSED,AL_BUFFERS_QUEUED,AL_SOURCE_STATE,
 AL_STOPPED,AL_INITIAL,AL_PLAYING,AL_PAUSED,AL_LOOPING,AL_FALSE,AL_TRUE,
 AL_SOURCE_RELATIVE,AL_POSITION,AL_VELOCITY,AL_GAIN,AL_MAX_GAIN,AL_SAMPLE_OFFSET};
float ClampFloat(float v,float lo,float hi){return (std::max)(lo,(std::min)(v,hi));}
bool Com_GameIsRTCWSP(){return false;}
bool PCMByteCountFitsALsizei(size_t n){return n<=size_t(INT32_MAX)/2;}
struct ALFixture {
 int state=AL_INITIAL,error=0,plays=0,uploads=0;bool failUpload=false,failQueue=false,failQuery=false;
 float gain=1;
 std::deque<ALuint> queue;
 int alGetError(){return std::exchange(error,0);}
 void alSourceStop(ALuint){state=AL_STOPPED;}
 void alSourceRewind(ALuint){state=AL_INITIAL;}
 void alSourcePlay(ALuint){state=queue.empty()?AL_STOPPED:AL_PLAYING;++plays;}
 void alSourcei(ALuint,int param,int value){if(param==AL_BUFFER&&value==0)queue.clear();}
 void alSourcef(ALuint,int param,float value){if(param==AL_GAIN)gain=value;}
 void alSource3f(ALuint,int,float,float,float){}
 void alGetSourcei(ALuint,int param,int*out){
  if(failQuery){error=1;return;}
  if(param==AL_SOURCE_STATE)*out=state;
  else if(param==AL_BUFFERS_PROCESSED)*out=state==AL_STOPPED?int(queue.size()):0;
  else if(param==AL_BUFFERS_QUEUED)*out=int(queue.size());
  else if(param==AL_SAMPLE_OFFSET)*out=0;
 }
 void alSourceUnqueueBuffers(ALuint,int n,ALuint*out){
  if(state!=AL_STOPPED||int(queue.size())<n){error=1;return;}
  for(int i=0;i<n;++i){out[i]=queue.front();queue.pop_front();}
 }
 void alBufferData(ALuint,int,const short*,int,int){++uploads;if(failUpload)error=1;}
 void alSourceQueueBuffers(ALuint,int n,const ALuint*in){
  if(failQueue){error=1;return;}for(int i=0;i<n;++i)queue.push_back(in[i]);
 }
};
struct Pool {
 unsigned next=0;int active=0,limit=INT32_MAX;std::vector<ALuint> free;
 ALuint Acquire(){if(active>=limit)return 0;++active;if(free.empty())return ++next;auto id=free.back();free.pop_back();return id;}
 void Release(ALuint id){assert(active>0&&id);--active;free.push_back(id);}
};
struct OpenALDevice {
 ALFixture al;Pool pool;
 ALFixture&AL(){return al;}Pool&BufferPool(){return pool;}
 bool SupportsPCMChannels(int n){return n==1||n==2;}
 int PCM16FormatForChannels(int n){return n;}
 void SetSourceSpatialize(ALuint,bool){}void SetSourceDirectChannels(ALuint,bool){}
 void ConfigureSourceDistance(ALuint,bool){}
};
''' + implementation + r'''
int main(){
 OpenALDevice device;StreamPlayer stream;short tone[240]{};
 stream.Init(&device,1);stream.deferStart=true;
 assert(stream.QueuePCM16(tone,120,2,48000,false));
 assert(stream.QueuePCM16(tone,120,2,48000,false));
 assert(!stream.Idle()&&stream.QueuedBufferCount()==2&&device.pool.active==2);
 device.pool.limit=2;
 assert(!stream.QueuePCM16(tone,120,2,48000,false));
 assert(stream.QueuedBufferCount()==2&&device.pool.active==2);
 assert(std::fabs(stream.QueuedSeconds()-.005)<1e-9);
 stream.Update(1,false);assert(device.al.plays==0);
 stream.Update(1);assert(device.al.state==AL_PLAYING);
 device.al.state=AL_STOPPED;assert(stream.Idle()&&device.pool.active==0);
 assert(stream.QueuePCM16(tone,120,2,48000,false));
 assert(stream.QueuePCM16(tone,120,2,48000,false));
 assert(!stream.Idle()&&stream.QueuedBufferCount()==2);
 stream.Update(1);assert(device.al.state==AL_PLAYING);
 device.al.failQuery=true;stream.Clear();
 assert(stream.QueuedBufferCount()==0&&stream.QueuedSeconds()==0);
 device.al.failQuery=false;
 stream.Clear();assert(stream.Idle()&&device.pool.active==0&&stream.QueuedSeconds()==0);
 stream.Update(std::numeric_limits<float>::quiet_NaN(),false);
 assert(device.al.gain==0&&std::isfinite(device.al.gain));
 stream.Update(-1,false);assert(device.al.gain==0);
 stream.Update(1,false);assert(device.al.gain==1);
 device.al.failUpload=true;
 assert(!stream.QueuePCM16(tone,120,2,48000)&&device.pool.active==0&&stream.Idle());
 device.al.failUpload=false;device.al.failQueue=true;
 assert(!stream.QueuePCM16(tone,120,2,48000)&&device.pool.active==0&&stream.Idle());
 device.al.failQueue=false;
 assert(!stream.QueuePCM16(nullptr,1,2,48000));
 assert(!stream.QueuePCM16(tone,INT32_MAX,2,48000));
 for(int i=0;i<200;++i){
  assert(stream.QueuePCM16(tone,120,2,48000,false));stream.Update(1);
  device.al.state=AL_STOPPED;assert(stream.Idle()&&device.pool.active==0);
 }
 assert(device.pool.next==2);stream.Shutdown();
 assert(stream.QueuedBufferCount()==0&&stream.QueuedSeconds()==0);
}
''')

    def test_music_eof_drains_and_bad_loop_is_bounded(self):
        backend = (ROOT / "code/client/audio/openal/AudioSystemBackend.inl").read_text()
        implementation = backend[backend.index("void AudioSystem::ServiceBackgroundTrack()"):
                                 backend.index("void AudioSystem::RawSamples(")]
        self.execute(r'''
#include "code/client/audio/shared/AudioSpeakerLayout.h"
#include <array>
#include <cassert>
#include <cstring>
#include <limits>
#include <string>
#include <vector>
using byte=unsigned char;
constexpr int kQueuedStreamChunks=4,kMaxStreamChunksPerService=2;
#define S_COLOR_YELLOW ""
struct Info{int rate=44100,width=2,channels=2;unsigned speakerMask=0;};
struct snd_stream_t{Info info;int bytes=128;};
struct Cvar{float value=1;};Cvar*s_musicVolume=nullptr;
bool Com_GameIsRTCWSP(){return false;}
void Com_Printf(const char*,...){}
bool PCMChannelCountCanBeRepresented(int c){return c==1||c==2;}
bool PCMFrameByteCount(int n,int c,int w,size_t&b){b=size_t(n)*c*w;return true;}
int opens=0,reads=0;
int S_CodecReadStream(snd_stream_t*s,int bytes,byte*out){
 ++reads;
 int n=(std::min)(bytes,s->bytes);s->bytes-=n;std::memset(out,0,n);return n;
}
snd_stream_t*S_CodecOpenStream(const char*){++opens;auto*s=new snd_stream_t;s->bytes=0;return s;}
struct StreamPlayer{int queued=0,frames=0,clears=0;int QueuedBufferCount(){return queued;}
 void Clear(){queued=frames=0;++clears;}};
int SelectStreamOutputChannels(int,int c){return c;}
struct AudioSystem{
 struct RTCWMusicOutput{AudioSystem&system;};
 snd_stream_t*backgroundStream_=nullptr;std::string backgroundIntro_,backgroundLoop_;
 std::vector<byte> backgroundPendingPCM_;
 StreamPlayer musicPlayer_;int device_=0,flushes=0;bool flushFails=false,pending=false,queueFails=false;
 void ServiceBackgroundTrack();
 void CloseBackgroundStream(){delete backgroundStream_;backgroundStream_=nullptr;backgroundPendingPCM_.clear();}
 void StopBackgroundTrack(){CloseBackgroundStream();musicPlayer_.Clear();backgroundIntro_.clear();backgroundLoop_.clear();}
 bool FlushStream(StreamPlayer&,bool=false){if(flushFails)return false;if(pending)++flushes;pending=false;return true;}
 bool QueueStreamChunk(StreamPlayer&p,int n,int,int,int,const byte*,float,int,bool){if(queueFails)return false;++p.queued;p.frames+=n;pending=true;return true;}
};
struct Music{bool BeginService(AudioSystem::RTCWMusicOutput&,float){return true;}
 void EndOfTrack(AudioSystem::RTCWMusicOutput&){};}music;
Music&S_RTCWMusic(){return music;}
''' + implementation + r'''
int main(){
 AudioSystem system;system.backgroundStream_=new snd_stream_t;
 system.ServiceBackgroundTrack();assert(system.musicPlayer_.frames==32);
 system.ServiceBackgroundTrack();
 assert(system.backgroundStream_==nullptr&&system.musicPlayer_.frames==32);
 assert(system.musicPlayer_.clears==0&&system.flushes==1);
 system.StopBackgroundTrack();assert(system.musicPlayer_.frames==0);
 system.backgroundStream_=new snd_stream_t;system.backgroundLoop_="empty.wav";
 system.ServiceBackgroundTrack();
 assert(opens==1&&system.backgroundStream_==nullptr&&system.musicPlayer_.frames==32);
 assert(system.flushes==2);system.StopBackgroundTrack();
 system.backgroundStream_=new snd_stream_t;system.flushFails=true;
 system.ServiceBackgroundTrack();assert(system.backgroundStream_!=nullptr);
 system.ServiceBackgroundTrack();assert(system.backgroundStream_!=nullptr);
 system.flushFails=false;system.ServiceBackgroundTrack();
 assert(system.backgroundStream_==nullptr&&system.musicPlayer_.frames==32);
 system.StopBackgroundTrack();
 system.backgroundStream_=new snd_stream_t;system.queueFails=true;
 const int before=reads;system.ServiceBackgroundTrack();
 assert(reads==before+1&&system.backgroundPendingPCM_.size()==128);
 system.ServiceBackgroundTrack();assert(reads==before+1);
 system.queueFails=false;system.ServiceBackgroundTrack();
 assert(system.backgroundStream_==nullptr&&system.musicPlayer_.frames==32);
 assert(system.backgroundPendingPCM_.empty());system.StopBackgroundTrack();
 system.backgroundStream_=new snd_stream_t;system.queueFails=true;
 system.ServiceBackgroundTrack();system.StopBackgroundTrack();
 assert(system.backgroundPendingPCM_.empty());
}
''')

    def test_pcm_saturation_nan_and_all_pcm8_values(self):
        shared = (ROOT / "code/client/audio/openal/AudioSystemShared.inl").read_text()
        functions = []
        for name in ("static int ClampInt(", "static float ClampFloat(",
                     "static short ClampPCM16FromFloat(",
                     "static short ConvertUnsignedPCM8ToPCM16(",
                     "static short ConvertSignedPCM8ToPCM16("):
            start = shared.index(name)
            functions.append(shared[start:shared.index("\n}", start) + 2])
        self.execute(r'''
#include <cassert>
#include <cmath>
#include <initializer_list>
#include <limits>
using byte=unsigned char;
''' + "\n".join(functions) + r'''
int main(){
 const float nan=std::numeric_limits<float>::quiet_NaN();
 const float inf=std::numeric_limits<float>::infinity();
 assert(ClampFloat(nan,0,8)==0);
 assert(ClampPCM16FromFloat(nan)==0);
 assert(ClampPCM16FromFloat(std::numeric_limits<float>::max())==32767);
 assert(ClampPCM16FromFloat(-std::numeric_limits<float>::max())==-32768);
 assert(ClampPCM16FromFloat(inf)==0&&ClampPCM16FromFloat(-inf)==0);
 for(int value=0;value<256;++value){
  assert(ConvertUnsignedPCM8ToPCM16(byte(value))==(value-128)*256);
  assert(ConvertSignedPCM8ToPCM16(byte(value))==(value<128?value:value-256)*256);
 }
 for(float value:{-32768.f,-32512.5f,-1.5f,0.f,1.5f,32512.5f,32767.f})
  assert(ClampPCM16FromFloat(value)==static_cast<short>(value));
}
''')

    def test_decoded_pcm_keeps_native_byte_order(self):
        shared = (ROOT / "code/client/audio/openal/AudioSystemShared.inl").read_text()
        name = "ReadNativePCM16" if "ReadNativePCM16" in shared else "ReadLittlePCM16"
        read = function(shared, "static short " + name + "(")
        legacy = (ROOT / "code/client/audio/legacy/snd_mem.cpp").read_text()
        resample = function(legacy, "int ResampleSfxRaw(")
        self.execute(r'''
#include <cassert>
#include <cstring>
using byte=unsigned char;
// Simulate a big-endian target's LittleShort conversion. Decoded PCM is
// already native and must not pass through it a second time.
short LittleShort(short value){auto v=static_cast<unsigned short>(value);return short((v>>8)|(v<<8));}
namespace fnql {template<class T>T ReadUnaligned(const void*p){T value;std::memcpy(&value,p,sizeof(value));return value;}}
using fnql::ReadUnaligned;
struct DMA{int speed=48000;}dma;
''' + read + resample + r'''
int main(){
 short samples[]{0x1234,-1234,32767,-32768};byte unaligned[9]{};
 std::memcpy(unaligned+1,samples,8);
 for(int i=0;i<4;++i)assert(''' + name + r'''(unaligned+1+2*i)==samples[i]);
 short out[4]{};
 assert(ResampleSfxRaw(out,2,48000,2,2,unaligned+1)==2);
 for(int i=0;i<4;++i)assert(out[i]==samples[i]);
}
''')

    def test_mp3_streams_keep_decoder_native_pcm(self):
        mp3 = (ROOT / "code/client/audio/codecs/snd_codec_mp3.cpp").read_text()
        functions = function(mp3, "int S_MP3_CodecReadStream(") + function(mp3, "int S_MP3_ReadMemory(")
        self.execute(r'''
#define Q3_BIG_ENDIAN
#include <cassert>
#include <cstring>
short LittleShort(short value){auto v=static_cast<unsigned short>(value);return short((v>>8)|(v<<8));}
struct MiniMp3 {static void Decode(){}};
struct Info {int channels=2,rate=48000;};
struct Frames {int Read(MiniMp3*,void(*)(),int,int,int,short*out,int bytes){
 short samples[]{0x1234,-1234};assert(bytes>=4);std::memcpy(out,samples,4);return 4;
}};
struct Mp3Stream {Frames frames;MiniMp3 codec;};
struct snd_stream_t {Info info;void*ptr;};
struct snd_mp3_memory_t {Frames frames;MiniMp3 codec;Info info;};
''' + functions + r'''
int main(){
 Mp3Stream state;snd_stream_t stream{{},&state};snd_mp3_memory_t memory;
 short samples[2]{};
 assert(S_MP3_CodecReadStream(&stream,4,samples)==4);
 assert(samples[0]==0x1234&&samples[1]==-1234);
 assert(S_MP3_ReadMemory(&memory,4,samples)==4);
 assert(samples[0]==0x1234&&samples[1]==-1234);
}
''')


if __name__ == "__main__":
    unittest.main()
