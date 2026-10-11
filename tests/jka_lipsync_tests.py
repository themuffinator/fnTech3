"""Academy facial values, pinned to retail x86 preprocessing/body observations."""
from jka_cgame_spatial_sound_tests import JKACgameSpatialSoundTests as _CompileFixture, ROOT
from mesh_tag_tests import function
import unittest
_compile_and_run = _CompileFixture.compile_and_run
del _CompileFixture

KERNEL = r'''
#include "code/client/audio/shared/JKALipSync.h"
#include <cassert>
#include <array>
#include <cstring>
#include <vector>
namespace lip=fntech3::audio::jka::lipsync;
int main(){
 const lip::Thresholds t;
 assert(lip::Level(0,0,t)==4&&lip::Level(0,128,t)==-1);
 for(auto c:{std::array<int,3>{9,18,1},{64,16,2},{49,7,3},{64,8,4}})
  assert(lip::Level(c[0],float(c[1]),t)==c[2]);
 assert(lip::EnergySample(-32768)==16384&&lip::EnergySample(32767)==16129);
 // Native-body oracle executed from the legitimate jamp.exe on 2026-10-07:
 // amplitude 10<<8, peak50. Here include a peak-setting sample not selected
 // by the envelope's 100-sample strides, so PCM obtains that measured peak.
 for(auto c:{std::array<int,3>{50,0,-1},{100,0,-1},{101,0,2},{200,0,2},
  {201,0,1},{1000,0,1},{1001,1000,-1},{1100,1000,-1},{1101,1000,2}}){
  std::vector<short> pcm(c[0],2560);pcm[1]=12800;
  lip::Envelope e;assert(e.Build(pcm.data(),int(pcm.size()),1,22050,false,99,t));
  assert(e.AtSample(c[1])==c[2]);
 }
 std::vector<short> pcm(4000,0);
 pcm[0]=2560;pcm[100]=2560;pcm[200]=2560;pcm[300]=2560;pcm[400]=2560;pcm[500]=2560;
 lip::Envelope mpeg;assert(mpeg.Build(pcm.data(),4000,1,22050,true,50,t));
 assert(mpeg.AtMilliseconds(0)==1&&mpeg.AtMilliseconds(26)==1&&mpeg.AtMilliseconds(27)==-1);
 assert(mpeg.AtMilliseconds(1000)==0&&mpeg.AtMilliseconds(0xffffffffu)==0);
 // PCM and MPEG cache selection is a size decision for sound/chars only,
 // using native-rate decoded bytes, even when playback resamples44.1kHz.
 assert(lip::StreamedMpeg("SOUND/CHARS/a.wav",100,100000,44100,31756));
 assert(!lip::StreamedMpeg("sound/player/a.mp3",100,100000,44100,31756));
 assert(!lip::StreamedMpeg("sound/char",100,100000,44100,31756));
 assert(!lip::StreamedMpeg("sound/chars/a.wav",100,14776,44100,31756));
 assert(lip::StreamedMpeg("sound/chars/a.wav",99,14776,44100,31756));
 std::array<unsigned char,128> tag{};
 assert(lip::TaggedPeak(tag.data(),128)==128);
 std::memcpy(tag.data(),"TAG",3);std::memcpy(tag.data()+97,"#MAXVOL 50.25",13);
 assert(lip::TaggedPeak(tag.data(),128)==50.25f);
 std::memset(tag.data()+104,0,24);tag[104]='0';assert(lip::TaggedPeak(tag.data(),128)==0);
 assert(lip::TaggedPeak(tag.data(),127)==128);
 // Legacy MPEG really steps50samples although its bound counter steps100.
 std::vector<short> legacy(1200,0);legacy[50]=2560;
 auto at=[&](int i)->short{return i>=0&&i<int(legacy.size())?legacy[i]:0;};
 assert(lip::Legacy(at,0,1200,false,10,t)==-1);
 assert(lip::Legacy(at,0,1200,true,10,t)==1);
 assert(lip::Legacy(at,1201,1200,true,10,t)==0);
 lip::Mouths mouths;int measures=0;
 mouths.Write(3,-1);assert(mouths.Read(3)==-1);
 auto measure=[&]{++measures;return 2;};
 assert(mouths.MeasureLegacy(3,0,0,1,measure)==2);mouths.Mixed(1);
 assert(mouths.MeasureLegacy(3,0,2,801,measure)==2&&measures==1);
 assert(mouths.MeasureLegacy(3,0,2,802,measure)==2&&measures==2);
 mouths.Clear();assert(mouths.Read(3)==0);
 assert(mouths.MeasureLegacy(3,0,2,802,measure)==2);
 mouths.Reset();assert(mouths.Read(3)==0);
 lip::Envelope invalid;
 assert(!invalid.Build(nullptr,10,1,22050,false,128,t));
 assert(!invalid.Build(pcm.data(),10,2,22050,false,128,t));
 assert(!invalid.Build(pcm.data(),10,1,0,false,128,t));
 lip::Thresholds bad=t;const unsigned nan=0x7fc00001u;std::memcpy(&bad.closed,&nan,4);
 assert(!invalid.Build(pcm.data(),10,1,22050,false,128,bad));
 // Exact2:1 resampling selects the same analysis bytes and timeline.
 std::vector<short> high(pcm.size()*2);
 for(unsigned i=0;i<pcm.size();++i){high[2*i]=pcm[i];high[2*i+1]=32767;}
 lip::Envelope rate44;assert(rate44.Build(high.data(),int(high.size()),1,44100,true,50,t));
 for(unsigned ms=0;ms<200;++ms)assert(rate44.AtMilliseconds(ms)==mpeg.AtMilliseconds(ms));
 // Retail's fixed8bit cursor at48kHz selects source652 for output300;
 // an exact rational resampler would select653 and miss this impulse.
 std::vector<short> unusual(5000,0);unusual[652]=32767;
 lip::Envelope rate48;assert(rate48.Build(unusual.data(),5000,1,48000,true,50,t));
 assert(rate48.AtMilliseconds(0)==4);
 // PCM computes its own peak, so an entirely silent waveform has peak0.
 std::vector<short> silence(2000,0);
 lip::Envelope quiet;assert(quiet.Build(silence.data(),2000,1,22050,false,128,t));
 assert(quiet.AtMilliseconds(0)==4);
}
'''

BACKENDS = r'''
#include "code/client/audio/shared/JKALipSync.h"
#include <cassert>
#include <vector>
#include <cstdint>
namespace lip=fntech3::audio::jka::lipsync;
struct Drop{};
constexpr int ERR_DROP=1,START_SAMPLE_IMMEDIATE=0x7fffffff;
enum {qfalse,qtrue};
void Com_Error(int,const char*,...){throw Drop{};}
bool S_JKAIsVoiceChannel(int channel){return channel==3||channel==12||channel==20;}
lip::Thresholds thresholds;
lip::Thresholds S_JKAThresholds(){return thresholds;}
static int now=0;
int Sys_Milliseconds(){return now;}
using ALint=int;constexpr int AL_NO_ERROR=0,AL_PLAYING=1,AL_SOURCE_STATE=2;
struct AL {int state=AL_PLAYING;int alGetError(){return 0;}void alGetSourcei(int,int,int *out){*out=state;}};
struct Device {struct AL api;struct AL &AL(){return api;}};
struct SoundSample {lip::Envelope envelope;const lip::Envelope &JKAMouth()const{return envelope;}};
struct SoundVoice {bool active=false;int source=0,entchannel=0,entnum=0;SoundSample *sample=nullptr;unsigned jkaSpeechStart=0;};
struct Q3SoundWorld {Device *device_;SoundVoice oneShots_[4];lip::Mouths jkaMouths_;void MeasureJKAMouths();};
struct sfx_t {int soundChannels=1,soundCompressionMethod=0,soundLength=1200,jkaMpeg=qfalse;float jkaPeak=50;
 std::vector<short> pcm=std::vector<short>(1200,2560);};
struct channel_t {sfx_t *thesfx=nullptr;int entchannel=0,startSample=0,entnum=0;};
channel_t s_channels[4];int s_soundtime=1;lip::Mouths jkaMouths;
short S_STVEFSPSampleAt(const sfx_t *s,int at){return at>=0&&at<int(s->pcm.size())?s->pcm[at]:0;}
'''

BACKEND_CHECKS = r'''
int main(){
 Device device;Q3SoundWorld world{};world.device_=&device;
 SoundSample sample;std::vector<short> pcm(3000,2560);
 assert(sample.envelope.Build(pcm.data(),3000,1,22050,true,50,{}));
 world.MeasureJKAMouths();assert(world.jkaMouths_.Read(3)==0);
 world.oneShots_[0]={true,1,3,3,&sample,100};now=100;
 world.MeasureJKAMouths();assert(world.jkaMouths_.Read(3)==1);
 device.api.state=0;world.MeasureJKAMouths();assert(world.jkaMouths_.Read(3)==0);
 device.api.state=AL_PLAYING;world.oneShots_[0].entchannel=2;
 world.MeasureJKAMouths();assert(world.jkaMouths_.Read(3)==0);
 world.oneShots_[0].entchannel=20;now=1100;world.MeasureJKAMouths();assert(world.jkaMouths_.Read(3)==0);
 now=100;sample.envelope.Clear();bool caught=false;
 try{world.MeasureJKAMouths();}catch(Drop){caught=true;}assert(caught);
 sfx_t legacy;s_channels[0]={&legacy,3,0,3};
 S_Base_MeasureJKAMouths(0);assert(jkaMouths.Read(3)==1);
 s_channels[0].entchannel=2;S_Base_MeasureJKAMouths(0);assert(jkaMouths.Read(3)==0);
 s_channels[0].entchannel=12;s_channels[0].startSample=START_SAMPLE_IMMEDIATE;
 S_Base_MeasureJKAMouths(0);assert(jkaMouths.Read(3)==0);
 s_channels[0].startSample=0;s_soundtime=2000;S_Base_MeasureJKAMouths(2000);assert(jkaMouths.Read(3)==0);
 const unsigned nan=0x7fc00001u;std::memcpy(&thresholds.closed,&nan,4);caught=false;
 try{S_Base_MeasureJKAMouths(0);}catch(Drop){caught=true;}assert(caught);
}
'''

UPLOAD = r'''
#include "code/client/audio/shared/JKALipSync.h"
#include <cassert>
#include <cstdint>
#include <vector>
namespace lip=fntech3::audio::jka::lipsync;
struct Drop{};constexpr int ERR_DROP=1;
void Com_Error(int,const char*,...){throw Drop{};}
struct cvar_t{float value;};
cvar_t *s_threshold1=nullptr,*s_threshold2=nullptr,*s_threshold3=nullptr,*s_threshold4=nullptr;
bool Com_GameIsJKAMP(){return true;}
bool Com_GameIsRTCWSP(){return false;}
bool Com_GameIsSTVEFSP(){return false;}
using ALenum=int;using ALsizei=int;using ALuint=unsigned;
constexpr int AL_NO_ERROR=0;
struct AudioSampleFormat{int channels=1;};
bool PCMByteCountFitsALsizei(std::size_t count){return count<100000;}
struct Api{
 int creates=0,uploads=0,deletes=0;
 int alGetError(){return 0;}
 void alGenBuffers(int,ALuint *buffer){++creates;*buffer=41;}
 void alBufferData(ALuint,int,const void*,ALsizei,int){++uploads;}
 void alDeleteBuffers(int,ALuint*){++deletes;}
};
struct OpenALDevice{
 Api api;Api &AL(){return api;}
 bool SupportsSampleFormat(const AudioSampleFormat&){return true;}
 int PCM16FormatForSampleFormat(const AudioSampleFormat&){return 1;}
};
struct OtherEnvelope{void Build(const short*,int,int){}};
struct SoundSample{
 ALuint buffer_=0;AudioSampleFormat format_;int channels_=0,rate_=0,frames_=0,durationMs_=0;
 OtherEnvelope speech_,stvefTopBytes_;lip::Envelope jkaMouth_;bool jkaMpeg_=false;float jkaPeak_=128;
 bool UploadPCM(OpenALDevice&,const std::vector<short>&,const AudioSampleFormat&,int);
};
'''

UPLOAD_CHECKS = r'''
int main(){
 const unsigned nan=0x7fc00001u;cvar_t invalid{};std::memcpy(&invalid.value,&nan,4);s_threshold1=&invalid;
 OpenALDevice device;SoundSample sample;bool threw=false,loaded=false;int destroyed=0;
 try{
  struct Owner{int &destroyed;~Owner(){++destroyed;}} owner{destroyed};
  std::vector<short> pcm(2000,2560);
  loaded=sample.UploadPCM(device,pcm,{},22050);
 }catch(Drop){threw=true;}
 // Invalid facial metadata cannot longjmp out of decoded audio's owner scope.
 assert(!threw&&loaded&&destroyed==1);
 assert(device.api.creates==1&&device.api.uploads==1&&device.api.deletes==0&&sample.buffer_==41);
 assert(!sample.jkaMouth_.Ready());
}
'''


class JKALipSyncTests(unittest.TestCase):
    compile_and_run = _compile_and_run
    def test_retail_preprocessor_vectors_and_lifetimes(self):
        self.compile_and_run(KERNEL)

    def test_production_backend_measurements(self):
        world=(ROOT/'code/client/audio/openal/AudioSystemWorld.inl').read_text()
        legacy=(ROOT/'code/client/audio/legacy/snd_dma.cpp').read_text()
        self.compile_and_run(BACKENDS + function(world,'void Q3SoundWorld::MeasureJKAMouths()') +
                             function(legacy,'static void S_Base_MeasureJKAMouths(') + BACKEND_CHECKS)

    def test_invalid_thresholds_preserve_sample_load_ownership(self):
        world=(ROOT/'code/client/audio/openal/AudioSystemWorld.inl').read_text()
        local=(ROOT/'code/client/audio/snd_local.h').read_text()
        self.compile_and_run(UPLOAD + function(local,'extern "C++" inline fntech3::audio::jka::lipsync::Thresholds S_JKAThresholds(') +
                             function(world,'bool SoundSample::UploadPCM(') + UPLOAD_CHECKS)


if __name__ == '__main__':
    unittest.main()
