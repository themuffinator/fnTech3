"""Execute speech windows and real backend query/queue code with synthetic PCM."""
from pathlib import Path
import unittest
import rtcw_music_tests
from mesh_tag_tests import function

ROOT = Path(__file__).resolve().parents[1]
COMMON = r'''
#include "code/client/audio/shared/RTCWSpeech.h"
#include <array>
#include <cassert>
#include <climits>
#include <cstring>
#include <limits>
using namespace fntech3::audio::rtcw;
'''

MEASUREMENTS = COMMON + r'''
int main(){
 assert(SpeechLookahead(48000)==12000&&SpeechLookahead(22050)==5512);
 assert(!SpeechLookahead(0)&&!SpeechLookahead(-1)&&!SpeechLookahead(INT_MAX));
 assert(SpeechContribution(5000)==0&&SpeechContribution(-5000)==0);
 assert(SpeechContribution(5001)==4981&&SpeechContribution(-5001)==-4982);
 assert(!SpeechAverage(4999,100)&&SpeechAverage(5000,100)==25);
 assert(!SpeechAverage(-4999,100)&&SpeechAverage(-5000,100)==25);
 assert(SpeechAverage(INT_MIN,100)==255&&SpeechAverage(INT_MAX,100)==255);
 assert(!SpeechAverage(5000,0)&&!SpeechAverage(5000,101));
 assert(SpeechStreamSample(781*256)==0&&SpeechStreamSample(782*256)==25);
 assert(SpeechStreamSample(-782*256)==25&&SpeechStreamSample(INT_MIN)==255);
 assert(!SpeechPCM(nullptr,100));
 std::array<short,1200> pcm{};
 for(int i=0;i<600;++i){pcm[i*2]=short((i*997)%65536-32768);pcm[i*2+1]=32767;}
 SpeechEnvelope env;env.Build(pcm.data(),600,2);
 for(int frame=0;frame<500;++frame){
  short window[100];for(int i=0;i<100;++i)window[i]=pcm[(frame+i)*2];
  int value=-1;assert(env.At(frame,value)&&value==SpeechPCM(window,100));
 }
 int value=71;assert(!env.At(-1,value)&&!env.At(500,value)&&!env.At(LLONG_MAX,value)&&value==71);
 env.Clear();assert(!env.At(0,value)&&value==71);
 for(int channels:{0,9}){env.Build(pcm.data(),600,channels);assert(!env.At(0,value));}
 env.Build(pcm.data(),100,1);assert(!env.At(0,value));
 std::fill(pcm.begin(),pcm.end(),0);pcm[0]=5001;env.Build(pcm.data(),101,1);
 assert(env.At(0,value)&&value==0);pcm[0]=-5001;env.Build(pcm.data(),101,1);
 assert(env.At(0,value)&&value==0);pcm[1]=-5001;env.Build(pcm.data(),101,1);
 assert(env.At(0,value)&&value==49);
 for(int i=0;i<100;++i)pcm[i]=i%2?20000:-20000;
 assert(SpeechPCM(pcm.data(),100)==0); // Signed cancellation, not loudness.

 SpeechQueue queue;std::fill(pcm.begin(),pcm.end(),0);
 pcm[19]=32767; // A right-channel peak must not animate speech.
 queue.Append(pcm.data(),100,2);assert(!queue.At(0));
 pcm[0]=1000;queue.Append(pcm.data(),100,2);
 assert(queue.At(1)==32&&queue.At(99)==32&&queue.At(100)==32&&!queue.At(101));
 assert(!queue.At(-1)&&!queue.At(LLONG_MAX));
 queue.Pop();assert(queue.At(0)==32);queue.Pop();assert(!queue.At(0));queue.Pop();
 pcm[0]=-32768;queue.Append(pcm.data(),1,2);assert(queue.At(0)==255);
 queue.Clear();assert(!queue.At(0));
}
'''

LEGACY = COMMON + r'''
constexpr int SND_CHUNK_SIZE=1024,MAX_RAW_SAMPLES=16384,START_SAMPLE_IMMEDIATE=INT_MAX;
struct sndBuffer{short sndChunk[1024]{};sndBuffer *next=nullptr;int size=2048;short decoded[4096]{};};
struct sfx_t{sndBuffer *soundData=nullptr;int soundLength=0,soundChannels=1,soundCompressionMethod=0;};
struct Channel{int entnum=0,entchannel=0,startSample=0,leftvol=127,rightvol=127;sfx_t *thesfx=nullptr;};
static std::array<Channel,96>s_channels;
static bool sp=true,s_soundStarted=true,s_soundMuted=false;
static bool Com_GameIsRTCWSP(){return sp;}
static int s_soundtime=1000;static struct{int speed=1000;}dma;
static short mulawToShort[256];
static void S_AdpcmGetSamples(sndBuffer *c,short *out){std::copy(c->decoded,c->decoded+4096,out);}
static void decodeWavelet(sndBuffer *c,short *out){std::copy(c->decoded,c->decoded+c->size,out);}
struct Raw{int end=0;bool buffered=false;struct{int left=0,right=0;}samples[16384];};
static std::array<Raw,24>rtcwStreamRaw;
struct Policy{struct Stream{int entity=0,channel=0;};std::array<Stream,24>streams;
 int Limit()const{return 12;}const Stream &operator[](int i)const{return streams[i];}};
static struct{Policy policy;}rtcwStreams;
#include "code/client/audio/legacy/snd_rtcw_voice.hpp"
int main(){
 std::array<sndBuffer,3>chunks;
 for(int i=0;i<2;++i)chunks[i].next=&chunks[i+1];
 int amp=17;assert(!S_Base_RTCWSpeechWindow(nullptr,0,amp)&&amp==17);
 sfx_t sample;sample.soundData=chunks.data();sample.soundLength=3072;
 for(auto &c:chunks)std::fill(std::begin(c.sndChunk),std::end(c.sndChunk),short(-7000));
 assert(S_Base_RTCWSpeechWindow(&sample,980,amp)&&amp==255);
 sample.soundChannels=2;sample.soundLength=1536;
 for(auto &c:chunks)for(int i=0;i<1024;i+=2){c.sndChunk[i]=0;c.sndChunk[i+1]=32767;}
 assert(S_Base_RTCWSpeechWindow(&sample,480,amp)&&amp==0);
 chunks[1].sndChunk[0]=-6000;assert(S_Base_RTCWSpeechWindow(&sample,480,amp)&&amp==29);
 amp=41;assert(!S_Base_RTCWSpeechWindow(&sample,1436,amp)&&amp==41);
 chunks[0].next=nullptr;assert(!S_Base_RTCWSpeechWindow(&sample,480,amp)&&amp==41);chunks[0].next=&chunks[1];
 assert(!S_Base_RTCWSpeechWindow(&sample,LLONG_MAX,amp)&&!S_Base_RTCWSpeechWindow(&sample,-1,amp));
 for(int method:{1,2,3}){
  sample.soundChannels=1;sample.soundCompressionMethod=method;sample.soundLength=(method==1?4096:2048)*3;
  for(auto &c:chunks){std::fill(std::begin(c.decoded),std::end(c.decoded),short(0));std::memset(c.sndChunk,0,sizeof(c.sndChunk));}
  chunks[1].decoded[0]=-6000;mulawToShort[0]=0;mulawToShort[1]=-6000;
  reinterpret_cast<unsigned char *>(chunks[1].sndChunk)[0]=1;
  assert(S_Base_RTCWSpeechWindow(&sample,(method==1?4096:2048)-50,amp)&&amp==29);
  sample.soundChannels=2;assert(!S_Base_RTCWSpeechWindow(&sample,0,amp));
 }
 sample.soundChannels=1;sample.soundCompressionMethod=2;chunks[0].size=4096;
 assert(!S_Base_RTCWSpeechWindow(&sample,0,amp));chunks[0].size=20;
 assert(!S_Base_RTCWSpeechWindow(&sample,0,amp));chunks[0].size=2048;
 sample.soundCompressionMethod=4;assert(!S_Base_RTCWSpeechWindow(&sample,0,amp));
 sample.soundCompressionMethod=0;sample.soundLength=3072;
 auto &voice=s_channels[0];voice.entnum=7;voice.entchannel=3;voice.startSample=900;voice.thesfx=&sample;
 std::fill(std::begin(chunks[0].sndChunk),std::end(chunks[0].sndChunk),short(-7000));
 assert(S_Base_RTCWVoiceAmplitude(7)==255&&!S_Base_RTCWVoiceAmplitude(8));
 for(int bad:{INT_MIN,-1,128,INT_MAX})assert(!S_Base_RTCWVoiceAmplitude(bad));
 sp=false;assert(!S_Base_RTCWVoiceAmplitude(7));sp=true;
 s_soundMuted=true;assert(!S_Base_RTCWVoiceAmplitude(7));s_soundMuted=false;
 s_soundStarted=false;assert(!S_Base_RTCWVoiceAmplitude(7));s_soundStarted=true;
 voice.startSample=START_SAMPLE_IMMEDIATE;assert(!S_Base_RTCWVoiceAmplitude(7));voice.startSample=900;
 voice.leftvol=voice.rightvol=0;assert(!S_Base_RTCWVoiceAmplitude(7));voice.leftvol=127;
 auto &stream=rtcwStreams.policy.streams[1];stream={7,3};auto &raw=rtcwStreamRaw[1];raw.buffered=true;raw.end=1500;
 raw.samples[1250].left=1000*256;voice.thesfx=nullptr;assert(S_Base_RTCWVoiceAmplitude(7)==32);
 raw.samples[1250].right=32767*256;assert(S_Base_RTCWVoiceAmplitude(7)==32);
 voice.thesfx=&sample;assert(S_Base_RTCWVoiceAmplitude(7)==255);
 std::fill(std::begin(chunks[0].sndChunk),std::end(chunks[0].sndChunk),short(0));assert(!S_Base_RTCWVoiceAmplitude(7));
 sample.soundLength=400;assert(S_Base_RTCWVoiceAmplitude(7)==32); // Too-short SFX leaves stream value.
 raw.end=1200;assert(!S_Base_RTCWVoiceAmplitude(7));
 raw.end=1000+MAX_RAW_SAMPLES+1;assert(!S_Base_RTCWVoiceAmplitude(7));
 s_soundtime=INT_MAX-100;raw.end=int32_t(uint32_t(s_soundtime)+500u);
 raw.samples[(uint32_t(s_soundtime)+250)&16383].left=-2000*256;assert(S_Base_RTCWVoiceAmplitude(7)==64);
 raw.buffered=false;assert(!S_Base_RTCWVoiceAmplitude(7));
}
'''


def default_sound():
    legacy = (ROOT / 'code/client/audio/legacy/snd_dma.cpp').read_text()
    world = (ROOT / 'code/client/audio/openal/AudioSystemWorld.inl').read_text()
    backend = (ROOT / 'code/client/audio/openal/AudioSystemBackend.inl').read_text()
    return COMMON + r'''
#include "code/client/audio/shared/RTCWDefaultSound.h"
#include <cmath>
#include <string>
#include <map>
static bool sp=true,rtcw=true;static int audible=0;
static bool Com_GameIsRTCWSP(){return sp;}static bool Com_GameIsRTCW(){return rtcw;}
static int Cvar_VariableIntegerValue(const char *name){assert(std::string(name)=="s_defaultsound");return audible;}
constexpr bool qfalse=false,qtrue=true;
struct sndBuffer{short sndChunk[1024];sndBuffer *next;};
struct sfx_t{sndBuffer *soundData=nullptr;int soundLength=0,soundChannels=0,soundCompressionMethod=7;
 bool defaultSound=false,inMemory=false;char soundName[64]{};};
static std::array<sfx_t,2>s_knownSfx;static std::array<sfx_t *,2>sfxHash;
static int s_numSfx=0,allocated=0,ordinaryRegistrations=0,codecLoads=0;static bool s_soundMuted=true;
static sndBuffer chunks[8];static sndBuffer *SND_malloc(){return &chunks[allocated++];}
static void SND_setup(){}
static sfx_t *S_KnownSfxAt(int index){assert(index==0);return &s_knownSfx[0];}
static void Q_strncpyz(char *out,const char *in,size_t size){assert(std::strlen(in)<size);std::strcpy(out,in);}
static void S_Base_RegisterSound(const char *name,bool){assert(std::string(name)=="sound/feedback/hit.wav");++ordinaryRegistrations;++s_numSfx;}
static bool S_LoadSound(sfx_t *){++codecLoads;return false;}
static int Q_stricmp(const char *a,const char *b){return std::strcmp(a,b);}
static void Com_DPrintf(const char *,...){}
#define S_COLOR_YELLOW ""
''' + '\n'.join(function(legacy, anchor) for anchor in (
        'static void S_Base_LoadRTCWDefaultSound(', 'static void S_Base_BeginRegistration(',
        'static void S_memoryLoad( sfx_t *sfx ) {')) + r'''
namespace openal {
struct OpenALDevice{};enum class AudioSampleEncoding{PCM};struct AudioSampleFormat{AudioSampleEncoding encoding;int channels;};
struct SoundSample{
 std::string name_;bool defaultSample_=false,missing_=false,loaded_=false,fail=false;int durationMs_=0;
 std::vector<short> uploaded;
 SoundSample(std::string name,int):name_(name){}
 bool UploadPCM(OpenALDevice &,const std::vector<short> &pcm,AudioSampleFormat f,int rate){assert(f.channels==1&&rate==22050);uploaded=pcm;durationMs_=int(pcm.size())*1000/rate;return !fail;}
 bool GenerateFallbackTone(OpenALDevice &);
 bool EnsureLoaded(OpenALDevice &d,bool fallback){assert(fallback);return GenerateFallbackTone(d);}
};
''' + function(world, 'bool SoundSample::GenerateFallbackTone(') + r'''
static std::string NormalizeSoundName(const char *name){return name;}
struct AudioSystem{
 bool hardMuted_=true;std::deque<SoundSample>samples_;std::map<std::string,int>sampleLookup_;OpenALDevice device_;
 struct Library{void EnsureLoaded(){}int Find(const std::string &){return 0;}}weaponSoundShaders_;
 void BeginRegistration();
};
''' + function(backend, 'void AudioSystem::BeginRegistration()') + r'''
}
int main(){
 for(bool mode:{false,true})for(int signal:{0,1}){
  sp=mode;audible=signal;s_numSfx=0;S_Base_BeginRegistration();
  auto &sample=s_knownSfx[0];const int frames=sp&&!audible?8:512;
  assert(!s_soundMuted&&sample.soundLength==frames&&sample.soundChannels==1&&sample.soundCompressionMethod==0);
  assert(sample.defaultSound&&sample.inMemory&&!sample.soundData->next);
  for(int i=0;i<1024;++i)assert(sample.soundData->sndChunk[i]==(audible&&i<frames?i:0));
  assert(codecLoads==0&&ordinaryRegistrations==0&&std::string(sample.soundName)==DefaultSoundName);
  const int before=allocated;S_Base_BeginRegistration();assert(allocated==before);
  sample.inMemory=false;sample.soundData=nullptr;S_memoryLoad(&sample);assert(allocated==before+1&&sample.inMemory&&sample.soundLength==frames&&!codecLoads);
  openal::AudioSystem s;s.BeginRegistration();assert(!s.hardMuted_&&s.samples_.size()==1&&s.sampleLookup_.at(DefaultSoundName)==0);
  const auto &loaded=s.samples_[0];assert(loaded.loaded_&&loaded.defaultSample_&&!loaded.missing_&&int(loaded.uploaded.size())==frames);
  for(int i=0;i<frames;++i)assert(loaded.uploaded[i]==(audible?i:0));
  s.BeginRegistration();assert(s.samples_.size()==1);
  s.samples_[0].fail=true;assert(!s.samples_[0].GenerateFallbackTone(s.device_)&&s.samples_[0].missing_&&!s.samples_[0].defaultSample_);
 }
 rtcw=false;s_numSfx=0;S_Base_BeginRegistration();assert(ordinaryRegistrations==1&&codecLoads==0);
 openal::AudioSystem s;s.BeginRegistration();assert(s.sampleLookup_.at("sound/feedback/hit.wav")==0);
 assert(s.samples_[0].uploaded.size()==5512&&s.samples_[0].durationMs_==250&&s.samples_[0].uploaded[1]!=0);
}
'''


def openal():
    streams = (ROOT / 'code/client/audio/openal/AudioSystemStreams.inl').read_text()
    world = (ROOT / 'code/client/audio/openal/AudioSystemWorld.inl').read_text()
    backend = (ROOT / 'code/client/audio/openal/AudioSystemBackend.inl').read_text()
    return COMMON + r'''
#include "code/client/audio/shared/RTCWLoops.h"
#include <map>
using ALuint=unsigned;using ALint=int;using ALsizei=int;using ALenum=int;
constexpr int AL_NO_ERROR=0,AL_BUFFERS_PROCESSED=1,AL_BUFFERS_QUEUED=2,AL_BUFFER=3,
 AL_SOURCE_RELATIVE=4,AL_TRUE=1,AL_POSITION=5,AL_VELOCITY=6,AL_GAIN=7,AL_SOURCE_STATE=8,
 AL_PLAYING=9,AL_SAMPLE_OFFSET=10,AL_MAX_GAIN=11,AL_STOPPED=12;
namespace fntech3::audio{struct CubicStreamResampler{};}
static bool sp=true;
static bool Com_GameIsRTCWSP(){return sp;}
static float ClampFloat(float value,float low,float high){return std::clamp(value,low,high);}
static bool PCMByteCountFitsALsizei(size_t count){return count<=size_t(INT_MAX)/2;}
struct OpenALDevice {
 struct Source{int state=AL_STOPPED,offset=0,processed=0;std::deque<unsigned>queue;};
 struct Pool{unsigned next=0;int acquired=0,released=0;bool fail=false;
  unsigned Acquire(){if(fail)return 0;++acquired;return ++next;}void Release(unsigned b){assert(b);++released;}}pool;
 std::map<unsigned,Source> sources;std::map<unsigned,int>frames;
 int error=0;bool failQueue=false,failUpload=false,failUnqueue=false,failOffset=false;
 auto &BufferPool(){return pool;}auto &AL(){return *this;}
 bool SupportsPCMChannels(int c){return c==1||c==2;}
 int PCM16FormatForChannels(int c){return SupportsPCMChannels(c)?c:0;}
 int alGetError(){int e=error;error=0;return e;}
 void alGetSourcei(unsigned s,int p,int *v){auto &a=sources[s];
  if(p==AL_BUFFERS_PROCESSED)*v=a.processed;
  else if(p==AL_BUFFERS_QUEUED)*v=int(a.queue.size());
  else if(p==AL_SOURCE_STATE)*v=a.state;
  else if(p==AL_SAMPLE_OFFSET){if(failOffset)error=1;else *v=a.offset;}
 }
 void alSourceUnqueueBuffers(unsigned s,int n,unsigned *b){assert(n==1);if(failUnqueue){error=1;return;}
  auto &a=sources[s];assert(!a.queue.empty());*b=a.queue.front();a.queue.pop_front();
  a.offset=std::max(0,a.offset-frames[*b]);a.processed=std::max(0,a.processed-1);
 }
 void alSourceQueueBuffers(unsigned s,int n,const unsigned *b){assert(n==1);if(failQueue){error=1;return;}sources[s].queue.push_back(*b);}
 void alBufferData(unsigned b,int format,const short *,int bytes,int){if(failUpload){error=1;return;}frames[b]=bytes/(2*format);}
 void alSourceStop(unsigned s){auto &a=sources[s];a.state=AL_STOPPED;a.processed=int(a.queue.size());a.offset=0;}
 void alSourcePlay(unsigned s){sources[s].state=AL_PLAYING;}
 void alSourcei(unsigned s,int p,int){if(p==AL_BUFFER)sources[s].queue.clear();}
 void alSourcef(unsigned,int,float){}void alSource3f(unsigned,int,float,float,float){}
 void SetSourceSpatialize(unsigned,bool){}void SetSourceDirectChannels(unsigned,bool){}
 void ConfigureSourceDistance(unsigned,bool){}
};
''' + streams[:streams.index('class VoiceChatPlayer')] + r'''
struct Vec3{float value[3]{};const float *Data()const{return value;}};
struct SoundSample{
 SpeechEnvelope envelope;int rate=1000;
 int Rate()const{return rate;}
 bool RTCWSpeechAmplitude(int64_t frame,int &a)const{return envelope.At(frame,a);}
};
struct SoundVoice{bool active=false,rtcwOneShot=true,fixedOrigin=false;unsigned source=1;
 SoundSample *sample=nullptr;int entnum=7,entchannel=3;Vec3 origin;};
struct Q3SoundWorld{
 OpenALDevice *device_=nullptr;std::array<SoundVoice,96>oneShots_;
 std::array<Vec3,1024>entityOrigins_;int listenerNumber_=7;Vec3 listenerOrigin_;
 float listenerAxis_[3][3]{{1,0,0},{0,1,0},{0,0,1}};
 int RTCWVoiceAmplitude(int,int)const;
};
''' + function(world, 'int Q3SoundWorld::RTCWVoiceAmplitude(') + r'''
struct AudioSystem{
 bool started_=true,hardMuted_=false;
 struct Policy{struct Stream{int entity=0,channel=0;};std::array<Stream,24>slots;
  int Limit()const{return 12;}const Stream &operator[](int i)const{return slots[i];}};
 struct {Policy policy;}entityStreams_;
 struct Lane{std::array<StreamPlayer,2>players;};std::array<Lane,24>entityStreamLanes_;
 Q3SoundWorld world_;
 int RTCWVoiceAmplitude(int);
};
''' + function(backend, 'int AudioSystem::RTCWVoiceAmplitude(') + r'''
int main(){
 OpenALDevice d;StreamPlayer p;p.trackRTCWSpeech=true;p.Init(&d,1);
 short quiet[1200]{},tone[1200];std::fill(std::begin(tone),std::end(tone),short(1000));
 assert(!p.RTCWSpeechAmplitude());
 assert(p.QueuePCM16(quiet,200,2,1000)&&p.QueuePCM16(tone,200,2,1000));
 assert(p.QueuedBufferCount()==2&&p.RTCWSpeechAmplitude()==32);
 d.sources[1].offset=100;assert(p.RTCWSpeechAmplitude()==32);
 d.sources[1].offset=151;assert(!p.RTCWSpeechAmplitude());
 d.sources[1].offset=210;d.sources[1].processed=1;
 assert(!p.RTCWSpeechAmplitude()&&p.QueuedBufferCount()==1&&d.sources[1].offset==10);
 p.Clear();assert(!p.RTCWSpeechAmplitude()&&p.Idle()&&d.pool.acquired==d.pool.released);
 assert(p.QueuePCM16(tone,400,2,1000)&&p.RTCWSpeechAmplitude()==32);
 d.failQueue=true;assert(!p.QueuePCM16(quiet,400,2,1000));d.failQueue=false;
 d.failUpload=true;assert(!p.QueuePCM16(quiet,400,2,1000));d.failUpload=false;
 d.pool.fail=true;assert(!p.QueuePCM16(quiet,400,2,1000));d.pool.fail=false;
 assert(p.QueuedBufferCount()==1&&p.RTCWSpeechAmplitude()==32);
 d.failOffset=true;assert(!p.RTCWSpeechAmplitude());d.failOffset=false;
 d.sources[1].state=AL_STOPPED;assert(!p.RTCWSpeechAmplitude());d.sources[1].state=AL_PLAYING;
 assert(p.QueuePCM16(quiet,400,2,1000));
 d.sources[1].offset=410;d.sources[1].processed=1;d.failUnqueue=true;
 assert(!p.RTCWSpeechAmplitude()&&p.QueuedBufferCount()==2);d.failUnqueue=false;
 assert(!p.RTCWSpeechAmplitude()&&p.QueuedBufferCount()==1&&d.sources[1].offset==10);
 p.Clear();assert(!p.RTCWSpeechAmplitude()&&d.pool.acquired==d.pool.released);
 p.Shutdown();assert(!p.RTCWSpeechAmplitude());p.Clear();
 p.Init(&d,1);p.trackRTCWSpeech=false;assert(p.QueuePCM16(tone,400,2,1000)&&!p.RTCWSpeechAmplitude());p.Clear();

 AudioSystem s;s.world_.device_=&d;SoundSample sample;sample.envelope.Build(tone,600,2);
 auto &voice=s.world_.oneShots_[0];voice.active=true;voice.sample=&sample;
 d.sources[1].state=AL_PLAYING;d.sources[1].offset=0;
 assert(s.RTCWVoiceAmplitude(7)==0); // Samples below 5000 do not animate one-shots.
 std::fill(std::begin(tone),std::end(tone),short(-7000));sample.envelope.Build(tone,600,2);
 assert(s.RTCWVoiceAmplitude(7)==255);
 d.sources[1].offset=251;assert(!s.RTCWVoiceAmplitude(7));d.sources[1].offset=0;
 voice.entchannel=2;assert(!s.RTCWVoiceAmplitude(7));voice.entchannel=3;
 voice.active=false;assert(!s.RTCWVoiceAmplitude(7));voice.active=true;
 voice.rtcwOneShot=false;assert(!s.RTCWVoiceAmplitude(7));voice.rtcwOneShot=true;
 s.world_.listenerNumber_=0;s.world_.entityOrigins_[7].value[0]=10000;
 assert(!s.RTCWVoiceAmplitude(7));voice.fixedOrigin=true;assert(s.RTCWVoiceAmplitude(7)==255);voice.fixedOrigin=false;
 s.world_.listenerNumber_=7;
 auto &stream=s.entityStreamLanes_[1].players[0];stream.trackRTCWSpeech=true;stream.Init(&d,2);
 s.entityStreams_.policy.slots[1]={7,3};std::fill(std::begin(tone),std::end(tone),short(2000));
 assert(stream.QueuePCM16(tone,400,2,1000));assert(s.RTCWVoiceAmplitude(7)==255);
 voice.active=false;assert(s.RTCWVoiceAmplitude(7)==64);
 s.hardMuted_=true;assert(!s.RTCWVoiceAmplitude(7));s.hardMuted_=false;
 s.started_=false;assert(!s.RTCWVoiceAmplitude(7));s.started_=true;
 sp=false;assert(!s.RTCWVoiceAmplitude(7));sp=true;
 for(int entity:{-1,0,8,128,INT_MAX})assert(!s.RTCWVoiceAmplitude(entity));
 stream.Clear();assert(!s.RTCWVoiceAmplitude(7));
 assert(d.pool.acquired==d.pool.released);
}
'''


class RTCWSpeechTests(unittest.TestCase):
    compile_run = rtcw_music_tests.RTCWMusicTests.compile_run

    def test_pcm_threshold_rounding_envelope_and_stream_boundaries(self):
        self.compile_run(MEASUREMENTS)

    def test_actual_dma_query_and_compressed_chunk_reader(self):
        self.compile_run(LEGACY)

    def test_actual_openal_queue_offsets_failures_and_query_precedence(self):
        self.compile_run(openal())

    def test_rtcw_default_handle_pcm_registration_reload_and_other_profiles(self):
        self.compile_run(default_sound())


if __name__ == '__main__':
    unittest.main()
