"""RTCW one-shot import boundaries, allocation policy and actual backend calls."""
from pathlib import Path
import unittest
from rtcw_music_fixture import function
from rtcw_music_tests import IMPORTS
import rtcw_music_tests

ROOT = Path(__file__).resolve().parents[1]

COMMON = r'''
#include "code/client/audio/shared/RTCWOneShots.h"
#include "code/client/audio/shared/RTCWLoops.h"
#include "code/client/audio/shared/STVEFAmbient.h"
#include <cassert>
#include <climits>
#include <cstring>
#include <vector>
using namespace fntech3::audio::rtcw;
'''

POLICY = COMMON + r'''
int main(){
 for(bool sp:{false,true}){
  OneShots slots;std::vector<int> stopped;
  auto stop=[&](int i){stopped.push_back(i);};
  auto start=[&](int entity,int channel,int sample,int flags,int now){return slots.Start(sp,128,0,now,{entity,channel,sample,flags},stop);};
  assert(start(200,0,1,0,1)==95);
  const int second=start(200,0,1,0,2);assert(second==(sp?94:95));
  assert(slots[second].allocated==(sp?2:1));
  slots.Reset();stopped.clear();
  assert(start(127,3,1,0,1)==95);
  assert(start(127,3,2,0,2)==94);
  assert(stopped.size()==(sp?1:0));
  slots.Reset();stopped.clear();
  assert(start(200,3,1,OneShots::NoCut,1)==95);
  assert(start(200,3,1,OneShots::CutOff,2)==94&&stopped.empty());
  assert(start(200,3,2,OneShots::CutOffAll,3)==93&&stopped.size()==2);
  slots.Reset();stopped.clear();
  assert(start(200,3,1,0,1)==95);
  assert(start(200,3,1,0,20)==95&&slots[95].allocated==1);
  slots.Release(95);slots.Release(95);slots.Release(-1);slots.Release(96);
  assert(start(201,0,1,0,21)==94); // Freed slots join the tail.
  slots.Reset();stopped.clear();
  const int usable=sp?95:96;
  for(int i=0;i<usable;++i)assert(start(200+i,0,i+1,0,i+1)==95-i);
  assert(start(900,0,900,0,1000)==95); // Oldest eligible entity.
  assert(start(220,4,999,0,1001)==75); // Oldest voice of the same entity.
  assert(start(221,7,22,OneShots::NoCut,1002)==74); // Same sample can evict even NoCut.
  for(int i=0;i<96;++i)slots.Release(i);
  assert(start(900,0,900,0,2000)>=0); // Exhaustion followed by release recovers.
  slots.Reset();
  for(int i=0;i<usable;++i)assert(start(0,0,i+1,OneShots::NoCut,1)==95-i);
  assert(start(900,0,900,0,1000)==-1); // No out-of-bounds fallback.
  slots.Reset();
  for(int i=0;i<usable;++i)assert(start(200+i,7,i+1,0,1)==95-i);
  assert(start(900,0,900,0,1000)==-1); // Announcer protection.
  slots.Reset();
  for(int i=0;i<usable;++i)assert(start(200+i,0,i+1,0,10)==95-i);
  assert(start(900,0,900,0,10)==-1); // Strictly older, even at a wrapped clock.
  assert(start(900,0,900,0,INT_MIN)==-1);
 }
 const float zero[3]={},axis[3][3]={{1,0,0},{0,1,0},{0,0,1}},point[3]={0,100,0};
 auto gain=SpatializeOrigin(point,1250,127,zero,axis);assert(gain.left==124&&gain.right==0);
 gain=SpatializeOrigin(zero,1250,127,zero,axis);assert(gain.left==63&&gain.right==63);
 gain=SpatializeOrigin(zero,1250,127,zero,axis,true);assert(gain.left==127&&gain.right==127);
}
'''


def imports():
    prefix = IMPORTS[:IMPORTS.index('#include "code/client/client_rtcw_audio_compat.hpp"')]
    prefix = prefix.replace('static void S_StartRTCWSound(const float *,int,int,int,int){assert(false);}', '')
    prefix = prefix.replace('static void S_StartRTCWLocalSound(int,int){assert(false);}', '')
    return prefix + r'''
static int entityWord,channelWord,handleWord,flagsWord;static float recorded[3];static const float *input;
static void S_StartRTCWSound(const float *origin,int entity,int channel,int handle,int flags){
 ++operation;input=origin;if(origin)memcpy(recorded,origin,12);
 entityWord=entity;channelWord=channel;handleWord=handle;flagsWord=flags;
}
static void S_StartRTCWLocalSound(int handle,int channel){++operation;handleWord=handle;channelWord=channel;}
#include "code/client/client_rtcw_audio_compat.hpp"
using fntech3::rtcw::module::Call;
template<class F>static void reject(F action){const int before=operation;bool failed=false;try{action();}catch(int){failed=true;}assert(failed&&operation==before);}
int main(){
 for(bool native:{false,true})for(auto id:{COM_GAME_RTCW_SP,COM_GAME_RTCW_MP}){
  profile=id;vm_t vm{native,{}};const float point[]={1,2,3};memcpy(vm.bytes+13,point,12);
  auto ptr=[&](int offset)->intptr_t{return native?reinterpret_cast<intptr_t>(vm.bytes+offset):offset;};
  for(auto call:{Call::CG_S_STARTSOUND,Call::CG_S_STARTSOUNDEX}){
   for(bool fixed:{false,true}){
    const int entity=fixed?INT_MIN:1023;
    intptr_t words[]={0,fixed?ptr(13):0,entity,INT_MAX,INT_MIN,31};
    assert(CL_RTCWAudioCall(&vm,call,words));
    assert(entityWord==entity&&channelWord==INT_MAX&&handleWord==INT_MIN&&flagsWord==(call==Call::CG_S_STARTSOUNDEX?31:0));
    assert(fixed?input&&input!=reinterpret_cast<float *>(vm.bytes+13)&&!memcmp(point,recorded,12):!input);
   }
   for(int entity:{-1,1024,INT_MIN,INT_MAX}){
    intptr_t words[]={0,0,entity,1,1,0};reject([&]{CL_RTCWAudioCall(&vm,call,words);});
   }
   intptr_t words[]={0,ptr(13),0,0,1,0};
   reject([&]{CL_RTCWAudioCall(nullptr,call,words);});
   for(uint32_t bits:{0x7f800000u,0xff800000u,0x7fc00000u,0x7fa00001u}){
    memcpy(vm.bytes+13,&bits,4);reject([&]{CL_RTCWAudioCall(&vm,call,words);});
   }
   memcpy(vm.bytes+13,point,12);
   if(!native){words[1]=245;reject([&]{CL_RTCWAudioCall(&vm,call,words);});}
   words[1]=static_cast<intptr_t>(UINTPTR_MAX-7);reject([&]{CL_RTCWAudioCall(&vm,call,words);});
   words[1]=ptr(13);
   if(sizeof(intptr_t)>4)for(int i=2;i<=(call==Call::CG_S_STARTSOUNDEX?5:4);++i){
    auto saved=words[i];words[i]=static_cast<intptr_t>(INT64_C(1)<<40);reject([&]{CL_RTCWAudioCall(&vm,call,words);});words[i]=saved;
   }
  }
  // Exact-size buffers ensure no optional argument is read for these imports.
  intptr_t plain[]={0,0,0,2,17};assert(CL_RTCWAudioCall(&vm,Call::CG_S_STARTSOUND,plain)&&flagsWord==0);
  for(auto call:{Call::CG_S_STARTLOCALSOUND,Call::UI_S_STARTLOCALSOUND}){
   intptr_t local[]={0,17,7};assert(CL_RTCWAudioCall(&vm,call,local)&&handleWord==17&&channelWord==7);
  }
 }
 for(auto id:{COM_GAME_QUAKE3,COM_GAME_QUAKELIVE})for(auto call:{Call::CG_S_STARTSOUND,Call::CG_S_STARTSOUNDEX,Call::CG_S_STARTLOCALSOUND,Call::UI_S_STARTLOCALSOUND}){
  profile=id;assert(!CL_RTCWAudioCall(nullptr,call,nullptr));
 }
}
'''


def legacy():
    source = (ROOT / 'code/client/audio/legacy/snd_dma.cpp').read_text()
    return COMMON + r'''
using vec3_t=float[3];using sfxHandle_t=int;using qboolean=bool;
constexpr int MAX_CHANNELS=96,MAX_GENTITIES=1024,kMasterVolume=127,START_SAMPLE_IMMEDIATE=-1,ERR_DROP=1;
constexpr bool qtrue=true,qfalse=false;
#define S_COLOR_YELLOW ""
static bool sp=true,rtcw=true,s_soundStarted=true,s_soundMuted=false;
static bool Com_GameIsRTCWSP(){return sp;}static bool Com_GameIsRTCW(){return rtcw;}
// This fixture drives RTCW only; ET's own one-shot path is exercised elsewhere.
static bool Com_GameIsWolfET(){return false;}
static void S_UpdateWolfETMasterFade(){}
static void Com_Error(int,const char *,...){throw 1;}static void Com_Printf(const char *,...){}static void Com_DPrintf(const char *,...){}
static void VectorCopy(const float *in,float *out){memcpy(out,in,12);}
static int now=100;static int Sys_Milliseconds(){return now;}
struct sfx_t {bool inMemory=true;int lastTimeUsed=0;const char *soundName="test";};static sfx_t samples[4];static int s_numSfx=4,loads=0;
static sfx_t *S_KnownSfxAt(int index){return samples+index;}static void S_memoryLoad(sfx_t *sample){++loads;sample->inMemory=true;}
struct channel_t {int allocTime,startSample,entnum,entchannel,leftvol,rightvol,master_vol;bool fixed_origin;float origin[3];sfx_t *thesfx;};
static channel_t s_channels[MAX_CHANNELS],*freelist=nullptr;
static void SetChannelFreeLink(channel_t *channel,channel_t *next){memcpy(channel,&next,sizeof(next));}
static OneShots rtcwOneShots;
static struct {struct {void StopChannel(int,int){}}policy;} rtcwStreams;
static std::array<vec3_t,MAX_GENTITIES> rtcwEntityPositions;
static int listener_number=0;static float listener_origin[3];static std::array<vec3_t,3> listener_axis={{{1,0,0},{0,1,0},{0,0,1}}};
static struct {float origin[3];} loopSounds[MAX_GENTITIES];static struct {int channels=2;} dma;
static struct {int integer=0;} show;static auto *s_show=&show;static int s_paintedtime=0,mixes=0;
static void S_Update_(int){++mixes;}
''' + '\n'.join(function(source, sig) for sig in (
        'static void S_ChannelFree(', 'static void S_ChannelSetup(',
        'static void S_SpatializeRTCWOneShot(', 'static void S_Base_StartRTCWSound(',
        'static void S_Base_StartRTCWLocalSound(', 'static void S_Base_UpdateEntityPosition(',
        'static void S_Base_Update( int msec ) {')) + r'''
int main(){
 for(bool single:{false,true}){
  sp=single;S_ChannelSetup();samples[1].inMemory=false;const int before=loads;
  const float point[]={0,100,0};S_Base_StartRTCWSound(point,-1,2,1,0);
  auto &first=s_channels[95];assert(first.thesfx==samples+1&&first.allocTime==now&&first.startSample==-1&&first.leftvol==124&&first.rightvol==0);
  assert(first.fixed_origin&&!memcmp(first.origin,point,12)&&loads==before+1);
  S_Base_StartRTCWLocalSound(2,3);assert(s_channels[94].entnum==0&&!s_channels[94].fixed_origin&&s_channels[94].leftvol==127&&s_channels[94].rightvol==127);
  S_Base_StartRTCWLocalSound(3,3);assert(s_channels[93].thesfx==samples+3);
  assert((s_channels[94].thesfx==nullptr)==sp);
  S_ChannelFree(&first);assert(!rtcwOneShots[95].active);
  S_Base_StartRTCWSound(nullptr,1023,0,1,0);assert(s_channels[92].entnum==1023);
  const float position[]={0,-100,0};S_Base_UpdateEntityPosition(1023,position);
  std::memset(loopSounds,0,sizeof(loopSounds));S_Base_Update(16);
  assert(s_channels[92].leftvol==0&&s_channels[92].rightvol==124&&mixes>0);
  try{S_Base_StartRTCWSound(nullptr,1024,0,1,0);assert(false);}catch(int){}
  const auto saved=rtcwOneShots[91];S_Base_StartRTCWSound(nullptr,0,0,-1,0);S_Base_StartRTCWSound(nullptr,0,0,4,0);assert(rtcwOneShots[91].active==saved.active);
  s_soundMuted=true;S_Base_StartRTCWSound(nullptr,0,0,1,0);assert(!rtcwOneShots[91].active);s_soundMuted=false;
  S_ChannelSetup();for(int i=0;i<96;++i)assert(!rtcwOneShots[i].active&&!s_channels[i].thesfx);
  assert(!memcmp(rtcwEntityPositions[1023],position,12));
 }
 rtcw=false;const float position[]={5,6,7};S_Base_UpdateEntityPosition(1023,position);
 assert(!memcmp(loopSounds[1023].origin,position,12)&&rtcwEntityPositions[1023][1]==-100);
}
'''


def openal():
    world = (ROOT / 'code/client/audio/openal/AudioSystemWorld.inl').read_text()
    return COMMON + r'''
using sfxHandle_t=int;using ALint=int;constexpr int kMaxVoices=96,AL_NO_ERROR=0,AL_SOURCE_STATE=1,AL_PLAYING=2,AL_PAUSED=3;static bool sp;static int Sys_Milliseconds(){return 100;}static bool Com_GameIsRTCWSP(){return sp;}static bool Com_GameIsWolfET(){return false;}
struct SoundSample{};
struct Device {unsigned next=1;bool fail=false;int state=AL_PLAYING;unsigned AcquireVoiceSource(){return fail?0:next++;}void CreateVoiceFilters(unsigned &,unsigned &){};
 Device &AL(){return *this;}int alGetError(){return 0;}void alGetSourcei(unsigned,int,int *out){*out=state;}};
struct SoundVoice{unsigned source=0,directFilter=0,sendFilter=0;bool rtcwOneShot=false,active=false;int releaseCount=0;void Release(Device &){source=0;active=false;rtcwOneShot=false;++releaseCount;}};
struct Q3SoundWorld {
 Device device;Device *device_=&device;std::array<SoundVoice,96> oneShots_;OneShots rtcwOneShots_;int listenerNumber_=0,plays=0,slot=-1,age=0;
 void StartRTCWSound(int,int,int,SoundSample *,const float *,int);
 void CleanupFinishedVoices();
 void PlayOneShot(SoundVoice *voice,int,int,int,SoundSample *,const float *,float,int now){++plays;slot=int(voice-oneShots_.data());age=now;voice->active=true;}
};
''' + function(world, 'void Q3SoundWorld::StartRTCWSound(') + function(world, 'void Q3SoundWorld::CleanupFinishedVoices(') + r'''
int main(){
 for(bool single:{false,true}){
  sp=single;Q3SoundWorld world;SoundSample sample;
  world.StartRTCWSound(200,3,1,&sample,nullptr,0);assert(world.plays==1&&world.slot==95&&world.oneShots_[95].rtcwOneShot&&world.age==100);
  const auto source=world.oneShots_[95].source;
  world.StartRTCWSound(200,3,1,&sample,nullptr,0);assert(world.plays==2&&world.slot==95&&world.oneShots_[95].source==source);
  world.StartRTCWSound(200,3,2,&sample,nullptr,8);assert(world.plays==3&&world.slot==94&&world.oneShots_[95].releaseCount==1);
  world.device.fail=true;world.StartRTCWSound(200,0,1,&sample,nullptr,0);assert(world.plays==3&&!world.rtcwOneShots_[93].active&&!world.oneShots_[93].active);
  world.device.fail=false;world.StartRTCWSound(200,0,1,&sample,nullptr,0);assert(world.plays==4&&world.slot==92);
  world.StartRTCWSound(200,0,1,nullptr,nullptr,0);assert(world.plays==4);
  world.CleanupFinishedVoices();assert(world.oneShots_[94].active&&world.rtcwOneShots_[94].active);
  world.device.state=AL_PAUSED;world.CleanupFinishedVoices();assert(world.oneShots_[94].active);
  world.device.state=0;world.CleanupFinishedVoices();
  for(int i=0;i<96;++i)assert(!world.oneShots_[i].active&&!world.rtcwOneShots_[i].active);
 }
}
'''


def facade():
    source = (ROOT/'code/client/audio/snd_main.cpp').read_text()
    return COMMON + r'''
using vec3_t=float[3];using sfxHandle_t=int;constexpr int ERR_DROP=1;
static bool rtcw=true;static bool Com_GameIsRTCW(){return rtcw;}static void Com_Error(int,const char *){throw 1;}
static int starts=0,locals=0,generic=0,flagWord=0;
static void start(const float *,int,int,int,int flags){++starts;flagWord=flags;}
static void local(int,int){++locals;}
static struct {decltype(&start) StartRTCWSound=nullptr;decltype(&local) StartRTCWLocalSound=nullptr;}si;
static void S_StartSoundVolume(float *,int,int,int,float){++generic;}
static void S_StartLocalSoundVolume(int,int,float){++generic;}
''' + '\n'.join(function(source,sig) for sig in (
        'extern "C" void S_StartRTCWSound(', 'extern "C" void S_StartRTCWLocalSound(',
        'extern "C" void S_StartSound(', 'extern "C" void S_StartLocalSound(')) + r'''
int main(){
 S_StartSound(nullptr,0,0,1);S_StartLocalSound(1,3);assert(!starts&&!locals&&!generic);
 si={start,local};S_StartSound(nullptr,0,0,1);S_StartLocalSound(1,3);
 assert(starts==1&&locals==1&&!generic&&flagWord==0);
 S_StartRTCWSound(nullptr,0,0,1,31);assert(starts==2&&flagWord==31);
 rtcw=false;S_StartSound(nullptr,0,0,1);S_StartLocalSound(1,3);assert(generic==2&&starts==2&&locals==1);
 try{S_StartRTCWSound(nullptr,0,0,1,0);assert(false);}catch(int){}
 try{S_StartRTCWLocalSound(1,0);assert(false);}catch(int){}
 assert(starts==2&&locals==1);
}
'''


def spatial():
    source = (ROOT/'code/client/audio/openal/AudioSystemWorld.inl').read_text()
    scope = source[source.index('void Q3SoundWorld::ApplyVoice('):]
    declarations = scope[scope.index('\tconst bool listenerAttached'):scope.index('\tconst bool stereoSample')]
    block = scope[scope.index('\tSpatialParams spatial;'):scope.index('\n\tfloat gain =')]
    return COMMON + r'''
struct SpatialParams {float gain=0,pan=0;};
static bool IsLocalOnlyChannel(int channel){return channel==1||channel==6||channel==7;}
static int generic=0;static SpatialParams ComputeSpatialParams(const float *,const float *,const float (*)[3],int,float,float){++generic;return {0.42f,0.1f};}
static SpatialParams calculate(bool shot,bool loop,int entity,int channel,const float *voiceOrigin){
 struct {bool rtcwOneShot,rtcwLoop;int entnum,entchannel;Loop rtcwLoopData;bool stvefAmbient;int stvefVolume;}
  voice{shot,loop,entity,channel,{},false,0};
 voice.rtcwLoopData.volume=255;
 const int listenerNumber_=0;const float referenceScale=1,rangeScale=1;
 struct {float value[3]{};const float *Data()const{return value;}}listenerOrigin_;
 const float listenerAxis_[3][3]={{1,0,0},{0,1,0},{0,0,1}};
''' + declarations + block + r'''
 return spatial;
}
int main(){
 const float point[]={0,100,0};
 for(int channel=0;channel<8;++channel){
  auto gain=calculate(true,false,200,channel,point);assert(gain.gain==124.f/127.f&&gain.pan==-1&&!generic);
  gain=calculate(true,false,0,channel,point);assert(gain.gain==1&&gain.pan==0&&!generic);
 }
 auto gain=calculate(false,true,0,0,point);assert(gain.gain==44.f/127.f&&gain.pan==0&&!generic);
 gain=calculate(false,false,200,0,point);assert(gain.gain==0.42f&&generic==1);
 gain=calculate(false,false,200,1,point);assert(gain.gain==1&&generic==1);
}
'''


class RTCWOneShotTests(unittest.TestCase):
    compile_run = rtcw_music_tests.RTCWMusicTests.compile_run

    def test_policy_replacement_exhaustion_and_recovery(self):
        self.compile_run(POLICY)

    def test_exact_native_and_qvm_arguments(self):
        self.compile_run(imports())

    def test_actual_legacy_start_and_channel_lifetime(self):
        self.compile_run(legacy())

    def test_actual_openal_start_and_source_failure(self):
        self.compile_run(openal())

    def test_facade_profile_and_disabled_backend(self):
        self.compile_run(facade())

    def test_actual_openal_spatial_gain_selection(self):
        self.compile_run(spatial())


if __name__ == '__main__':
    unittest.main()
