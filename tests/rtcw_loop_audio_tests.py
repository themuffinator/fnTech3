"""Execute RTCW loop policy, exact module arguments and real backend entrypoints."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest
from compiler_support import compiler_target_args
from rtcw_music_fixture import function
from rtcw_music_tests import IMPORTS

ROOT = Path(__file__).resolve().parents[1]

COMMON = r'''
#include "code/client/audio/shared/RTCWLoops.h"
#include "code/client/audio/shared/RTCWMusic.h"
#include "code/client/audio/shared/STVEFAmbient.h"
#include "code/client/audio/shared/JKAAmbient.h"
#include <cassert>
#include <climits>
#include <cfloat>
#include <cstring>
#include <vector>
using namespace fntech3::audio::rtcw;
static bool isSP=false;
static bool Com_GameIsRTCWSP(){return isSP;}
static MasterFade master;
static auto &S_RTCWMasterFade(){return master;}
static const float zero[3]={0,0,0};
static float axis[3][3]={{1,0,0},{0,1,0},{0,0,1}};
'''

CORE = COMMON + r'''
int main(){
 assert(LoopSampleOffset(0,22050,11025)==0);
 assert(LoopSampleOffset(250000,22050,11025)==5512);
 assert(LoopSampleOffset(750000,22050,11025)==5512);
 assert(LoopSampleOffset(1000000,48000,48000)==0);
 assert(LoopSampleOffset(INT64_MAX,192000,1)==0);
 assert(LoopSampleOffset(INT64_MAX,1,1000)==854);
 assert(LoopSampleOffset(-1,48000,1000)==0&&LoopSampleOffset(100,0,1000)==0);
 assert(LoopSampleOffset(100,192001,1000)==0&&LoopSampleOffset(100,48000,0)==0);
 LoopFrame frame;float origin[3]={0,0,0},velocity[3]={1,2,3};
 assert(frame.Append(origin,velocity,0,1,0)==-1&&frame.Size()==0);
 for(int i=0;i<128;++i){
  origin[0]=float(i);
  assert(frame.Append(origin,velocity,i?i:0,1,i==1?-1:255)==i);
 }
 assert(frame.Size()==128&&!frame.Accepts(255));
 assert(frame.Append(origin,velocity,0,2,255)==-1);
 assert(frame[0].range==1250&&frame[0].volume==255);
 assert(frame[1].volume==0&&frame[1].range==1);
 assert(frame[0].origin[0]==0&&frame[127].origin[0]==127);
 assert(frame[0].velocity[2]==3);
 frame.Clear();assert(frame.Size()==0);
 for(int volume:{INT_MIN,-1,0,1,128,255,256,257,511,512,INT_MAX}){
  const int slot=frame.Append(zero,velocity,0,2,volume);
  if(!volume){assert(slot==-1);continue;}
  assert(frame[slot].volume==std::clamp(volume,0,255));
  assert(frame[slot].loudUnderwater==((static_cast<unsigned>(volume)&256u)!=0));
 }
 Loop loop;loop.volume=255;
 auto gains=SpatializeLoop(loop,zero,axis);assert(gains.left==44&&gains.right==44);
 gains=SpatializeLoop(loop,zero,axis,true);assert(gains.left==89&&gains.right==89);
 loop.origin={0,100,0};gains=SpatializeLoop(loop,zero,axis);assert(gains.left==87&&gains.right==0);
 loop.origin={0,-100,0};gains=SpatializeLoop(loop,zero,axis);assert(gains.left==0&&gains.right==87);
 loop.origin={0,1330,0};gains=SpatializeLoop(loop,zero,axis);assert(gains.left==0&&gains.right==0);
 loop.origin={FLT_MAX,FLT_MAX,FLT_MAX};gains=SpatializeLoop(loop,zero,axis);assert(gains.left==0&&gains.right==0);
 loop.origin={0,0,0};loop.range=0;gains=SpatializeLoop(loop,zero,axis);assert(gains.left==0&&gains.right==0);
}
'''


def imports():
    prefix = IMPORTS[:IMPORTS.index('#include "code/client/client_rtcw_audio_compat.hpp"')]
    prefix = prefix.replace('static void S_AddRTCWLoopingSound(const float *,const float *,int,int,int){assert(false);}', '')
    return prefix + r'''
static int calls,rangeWord,handleWord,volumeWord;
static float recordedOrigin[3],recordedVelocity[3];
static const void *originPointer,*velocityPointer;
static void S_AddRTCWLoopingSound(const float *origin,const float *velocity,int range,int handle,int volume){
 ++calls;rangeWord=range;handleWord=handle;volumeWord=volume;
 std::memcpy(recordedOrigin,origin,12);std::memcpy(recordedVelocity,velocity,12);
 originPointer=origin;velocityPointer=velocity;
}
#include "code/client/client_rtcw_audio_compat.hpp"
using fntech3::rtcw::module::Call;
template<class F>static void reject(F action){int saved=calls;bool failed=false;try{action();}catch(int){failed=true;}assert(failed&&saved==calls);}
int main(){
 for(bool native:{false,true})for(auto id:{COM_GAME_RTCW_MP,COM_GAME_RTCW_SP}){
  profile=id;vm_t vm{native,{}};
  auto ptr=[&](int offset)->intptr_t{return native?reinterpret_cast<intptr_t>(vm.bytes+offset):offset;};
  float origin[3]={1,2,3},velocity[3]={4,5,6};
  memcpy(vm.bytes+13,origin,12);memcpy(vm.bytes+41,velocity,12);
  for(auto call:{Call::CG_S_ADDLOOPINGSOUND,Call::CG_S_ADDREALLOOPINGSOUND}){
   if(id==COM_GAME_RTCW_SP && call==Call::CG_S_ADDREALLOOPINGSOUND){assert(!CL_RTCWAudioCall(nullptr,call,nullptr));continue;}
   for(int entity:{INT_MIN,-1,0,1024,INT_MAX})for(int volume:{INT_MIN,0,255,256,INT_MAX}){
    intptr_t words[]={0,entity,ptr(13),ptr(41),1250,17,volume};
    const int before=calls;assert(CL_RTCWAudioCall(&vm,call,words)&&calls==before+1);
    assert(rangeWord==1250&&handleWord==17&&volumeWord==volume);
    assert(!memcmp(origin,recordedOrigin,12)&&!memcmp(velocity,recordedVelocity,12));
    assert(originPointer!=vm.bytes+13&&velocityPointer!=vm.bytes+41);
   }
   for(int word:{2,3}){
    intptr_t words[]={0,0,ptr(13),ptr(41),0,17,255};words[word]=0;
    reject([&]{CL_RTCWAudioCall(&vm,call,words);});
    words[word]=static_cast<intptr_t>(UINTPTR_MAX-7);reject([&]{CL_RTCWAudioCall(&vm,call,words);});
    if(!native){words[word]=245;reject([&]{CL_RTCWAudioCall(&vm,call,words);});}
   }
   for(int part=0;part<6;++part)for(uint32_t bits:{0x7f800000u,0xff800000u,0x7fc00000u,0x7fa00001u}){
    unsigned char *where=vm.bytes+(part<3?13+4*part:41+4*(part-3));memcpy(where,&bits,4);
    intptr_t words[]={0,0,ptr(13),ptr(41),0,17,255};reject([&]{CL_RTCWAudioCall(&vm,call,words);});
    memcpy(vm.bytes+13,origin,12);memcpy(vm.bytes+41,velocity,12);
   }
   intptr_t words[]={0,0,ptr(13),ptr(41),-1,17,255};reject([&]{CL_RTCWAudioCall(&vm,call,words);});
   if(sizeof(intptr_t)>4)for(int word:{1,4,5,6}){
    words[4]=0;const auto saved=words[word];words[word]=static_cast<intptr_t>(INT64_C(0x100000000));
    reject([&]{CL_RTCWAudioCall(&vm,call,words);});words[word]=saved;
   }
  }
 }
 for(auto id:{COM_GAME_QUAKE3,COM_GAME_QUAKELIVE})for(auto call:{Call::CG_S_ADDLOOPINGSOUND,Call::CG_S_ADDREALLOOPINGSOUND}){
  profile=id;assert(!CL_RTCWAudioCall(nullptr,call,nullptr));
 }
}
'''


def legacy():
    source = (ROOT / 'code/client/audio/legacy/snd_dma.cpp').read_text()
    prelude = COMMON + r'''
using sfxHandle_t=int;using vec3_t=float[3];using qboolean=bool;
constexpr bool qfalse=false,qtrue=true;constexpr int ERR_DROP=1,CA_ACTIVE=5,MAX_CHANNELS=96,kMasterVolume=127;
static void Com_Error(int,const char *,...){throw 1;}
static bool s_soundStarted=true,s_soundMuted=false;
static struct {int state=CA_ACTIVE;} cls;
struct sfx_t {bool inMemory=true;int soundLength=100,lastTimeUsed=0;};
static sfx_t samples[3];static int s_numSfx=3,s_soundtime=42,loads;
static sfx_t *S_KnownSfxAt(int handle){assert(handle>=0&&handle<3);return samples+handle;}
static void S_memoryLoad(sfx_t *sample){sample->inMemory=true;++loads;}
static LoopFrame rtcwLoops;
// Elite Force's ambient loops share the frame's clearing.
static fntech3::audio::stvef::AmbientLoopFrame stvefAmbientLoops;
// So do Jedi Academy's.
static fntech3::audio::jka::AmbientLoops jkaAmbientLoops;
struct loopSound_t {bool active=false;};static std::array<loopSound_t,1024> loopSounds;
static float listener_origin[3]={0,0,0};static std::array<vec3_t,3> listener_axis{{{1,0,0},{0,1,0},{0,0,1}}};
static struct {int channels=2;int speed=22050;} dma;
struct channel_t {int master_vol,leftvol,rightvol,startSample; sfx_t *thesfx;bool doppler;};
static channel_t loop_channels[MAX_CHANNELS];static int numLoopChannels;
'''
    for name in ('static void S_Base_AddRTCWLoopingSound(', 'static void S_AddRTCWLoopSounds(',
                 'static void S_Base_ClearLoopingSoundsFrame('):
        prelude += function(source, name) + '\n'
    return prelude + r'''
int main(){
 S_Base_AddRTCWLoopingSound(zero,zero,0,1,255);
 S_Base_AddRTCWLoopingSound(zero,zero,0,1,255);
 S_AddRTCWLoopSounds();assert(numLoopChannels==2); // Duplicate handles keep distinct submissions.
 for(int i=0;i<2;++i)assert(loop_channels[i].leftvol==44&&loop_channels[i].rightvol==44&&!loop_channels[i].doppler);
 assert(samples[1].lastTimeUsed==42);
 S_Base_ClearLoopingSoundsFrame();S_AddRTCWLoopSounds();assert(!numLoopChannels&&!rtcwLoops.Size());
 samples[1].inMemory=false;S_Base_AddRTCWLoopingSound(zero,zero,0,1,255);assert(loads==1);
 for(int i=1;i<128;++i)S_Base_AddRTCWLoopingSound(zero,zero,0,1,255);
 S_Base_AddRTCWLoopingSound(zero,zero,0,INT_MAX,255);assert(rtcwLoops.Size()==128);
 S_AddRTCWLoopSounds();assert(numLoopChannels==MAX_CHANNELS);
 S_Base_ClearLoopingSoundsFrame();S_Base_AddRTCWLoopingSound(zero,zero,0,INT_MAX,0);assert(!rtcwLoops.Size());
 for(int gate=0;gate<3;++gate){
  s_soundStarted=gate!=0;s_soundMuted=gate==1;cls.state=gate==2?0:CA_ACTIVE;
  S_Base_AddRTCWLoopingSound(zero,zero,0,INT_MAX,255);assert(!rtcwLoops.Size());
 }
 s_soundStarted=true;s_soundMuted=false;cls.state=CA_ACTIVE;
 bool failed=false;try{S_Base_AddRTCWLoopingSound(zero,zero,0,INT_MAX,255);}catch(int){failed=true;}assert(failed);
 samples[1].soundLength=0;failed=false;try{S_Base_AddRTCWLoopingSound(zero,zero,0,1,255);}catch(int){failed=true;}assert(failed);
 samples[1].soundLength=100;
 for(bool sp:{false,true})for(float fade:{0.f,0.001f,0.125f,0.333f,0.5f,0.999f,1.f,2.f,16.f}){
  isSP=sp;master.current=fade;S_Base_ClearLoopingSoundsFrame();
  S_Base_AddRTCWLoopingSound(zero,zero,0,1,255);assert(rtcwLoops.Size()==1);
  const int captured=int(255.f*(sp?fade:1.f));assert(rtcwLoops[0].volume==captured);
  master.current=1;S_AddRTCWLoopSounds();const int gain=std::min(255,int(captured*45.f/256.f));
  assert(numLoopChannels==(gain?1:0));if(gain)assert(loop_channels[0].leftvol==gain&&loop_channels[0].rightvol==gain);
 }
}
'''


def openal():
    world = (ROOT / 'code/client/audio/openal/AudioSystemWorld.inl').read_text()
    backend = (ROOT / 'code/client/audio/openal/AudioSystemBackend.inl').read_text()
    prelude = COMMON + r'''
using sfxHandle_t=int;using qboolean=bool;
constexpr bool qfalse=false,qtrue=true;constexpr int MAX_GENTITIES=1024,ENTITYNUM_NONE=1023,CHAN_AUTO=0,ERR_DROP=1,CA_ACTIVE=5;
constexpr int AL_BUFFER=1,AL_LOOPING=2,AL_SAMPLE_OFFSET=3,AL_TRUE=1;
constexpr float kDefaultSoundShaderScale=1;
using ALuint=unsigned;
static void Com_Error(int,const char *,...){throw 1;}
static struct {int state=CA_ACTIVE;} cls;
static struct {int integer=1;} doppler;static auto *s_doppler=&doppler;
static float VectorLengthSquared(const float *v){return v[0]*v[0]+v[1]*v[1]+v[2]*v[2];}
static std::int64_t micros=250000;
static auto Sys_Microseconds(){return micros;}
static bool Com_GameIsRTCW(){return true;}
static bool Com_GameIsJKAMP(){return false;}
static bool Com_GameIsSTVEFSP(){return false;}
struct Device{
 struct Caps{std::int64_t deviceClockValue=0;} caps;
 struct Api{std::vector<int> offsets;void alSourcei(unsigned,int what,int value){if(what==AL_SAMPLE_OFFSET)offsets.push_back(value);}} api;
 Api &AL(){return api;}bool RefreshTimingDiagnostics(){return false;}const Caps &Capabilities()const{return caps;}
 void CreateVoiceFilters(unsigned &,unsigned &){}
};
struct SoundSample{
 bool loaded=false;bool EnsureLoaded(Device &,bool){loaded=true;return true;}
 int Rate()const{return 22050;}int Frames()const{return 11025;}unsigned Buffer()const{return 1;}
 struct Settings{bool enabled=false;float rangeScale=1;} settings;
 const Settings &Shader()const{return settings;}
};
using SoundShaderSettings=SoundSample::Settings;
static float DistanceBetweenPoints(const float *a,const float *b){float v[3]={a[0]-b[0],a[1]-b[1],a[2]-b[2]};return std::sqrt(VectorLengthSquared(v));}
static float AudibilityHorizonDistance(float){return 1250;}
struct Vec{float xyz[3]{};void Set(const float *v){memcpy(xyz,v,12);}const float *Data()const{return xyz;}};
enum class VoiceKind{Loop};
struct SoundVoice{
 bool active=false,looping=false,occlusionInitialized=false,fixedOrigin=false,killWhenUnrefreshed=false,refreshedThisFrame=false,doppler=false,rtcwLoop=false,stvefAmbient=false;
 int stvefVolume=0;
 int handle=0,entnum=0,entchannel=0,lastOcclusionSmoothTime=0,nextEnvironmentUpdateTime=0;float occlusion=0,occlusionTarget=0;
 unsigned source=0,directFilter=0,sendFilter=0;
 SoundSample *sample=nullptr;VoiceKind kind=VoiceKind::Loop;Vec origin,velocity;Loop rtcwLoopData;
 void ReleaseSource(Device &){source=0;}
 void Release(Device &){*this=SoundVoice{};}
};
struct Q3SoundWorld{
 Device device;Device *device_=&device;std::array<SoundVoice,1024> loopVoices_;std::array<SoundVoice,2> oneShots_;
 LoopFrame rtcwLoops_;fntech3::audio::stvef::AmbientLoopFrame stvefAmbientLoops_;
 fntech3::audio::jka::AmbientLoops jkaAmbientLoops_;
 bool loopFrameClearPending_=false;std::array<Vec,1024> entityOrigins_;
 Vec listenerOrigin_;int listenerNumber_=0;float listenerAxis_[3][3]={{1,0,0},{0,1,0},{0,0,1}};
 unsigned nextSource=1;int applied=0;
 bool AcceptsRTCWLoop(int v)const{return rtcwLoops_.Accepts(v);}
 void AddRTCWLoopingSound(const float *,const float *,int,int,int,SoundSample *,float);
 void AddLoopingSound(int,const float *,const float *,int,SoundSample *,qboolean);
 void ClearLoopingSoundsFrame();
 void Update(qboolean);
 void CleanupFinishedVoices(){}void RefreshEnvironment(){}
 void MeasureJKAMouths(){assert(false);}
 void ApplyVoice(SoundVoice &voice,qboolean){assert(voice.source&&voice.rtcwLoop);++applied;}
 unsigned AllocateVoiceSourceForLoop(){return nextSource++;}
};
struct AudioSystem{
 bool started_=true,hardMuted_=false;Device device_;Q3SoundWorld world_;SoundSample samples[3];int lookups=0;
 SoundSample *GetSample(int h){++lookups;return h>=0&&h<3?samples+h:nullptr;}
 void AddRTCWLoopingSound(const float *,const float *,int,int,int);
};
'''
    # Update keeps Elite Force single player's loops by that profile's law.
    for signature in ('void Q3SoundWorld::AddLoopingSound(', 'void Q3SoundWorld::AddRTCWLoopingSound(',
                      'void Q3SoundWorld::ClearLoopingSoundsFrame(',
                      'static fntech3::audio::stvef::Span STVEFVoiceSpan(', 'static bool STVEFHeardAtListener(',
                      'static bool STVEFLoopSilent(', 'void Q3SoundWorld::Update('):
        prelude += function(world, signature) + '\n'
    prelude += function(backend, 'void AudioSystem::AddRTCWLoopingSound(') + '\n'
    return prelude + r'''
int main(){
 AudioSystem audio;float velocity[]={100,0,0};
 audio.AddRTCWLoopingSound(zero,velocity,0,1,255);audio.AddRTCWLoopingSound(zero,velocity,321,1,128);
 assert(audio.world_.rtcwLoops_.Size()==2);
 for(int i=0;i<2;++i){const auto &v=audio.world_.loopVoices_[i];assert(v.active&&v.refreshedThisFrame&&v.killWhenUnrefreshed&&v.rtcwLoop&&!v.doppler&&v.entnum==ENTITYNUM_NONE);}
 assert(audio.world_.loopVoices_[0].rtcwLoopData.range==1250&&audio.world_.loopVoices_[1].rtcwLoopData.range==321);
 audio.world_.Update(qfalse);assert(audio.world_.applied==2);
 assert(audio.world_.device.api.offsets==std::vector<int>({5512,5512}));
 const unsigned initialSource=audio.world_.loopVoices_[0].source;
 audio.world_.ClearLoopingSoundsFrame();audio.AddRTCWLoopingSound(zero,velocity,0,1,255);
 audio.world_.Update(qfalse);assert(audio.world_.loopVoices_[0].source==initialSource&&!audio.world_.loopVoices_[1].active);
 audio.world_.ClearLoopingSoundsFrame();float distant[3]={FLT_MAX,0,0};
 audio.AddRTCWLoopingSound(distant,velocity,0,1,255);audio.world_.Update(qfalse);
 assert(audio.world_.loopVoices_[0].active&&!audio.world_.loopVoices_[0].source);
 audio.world_.ClearLoopingSoundsFrame();micros=500000;
 audio.AddRTCWLoopingSound(zero,velocity,0,1,255);audio.world_.Update(qfalse);
 assert(audio.world_.loopVoices_[0].source!=initialSource&&audio.world_.device.api.offsets.back()==0);
 audio.world_.ClearLoopingSoundsFrame();audio.world_.Update(qfalse);assert(!audio.world_.loopVoices_[0].active);
 audio.world_.ClearLoopingSoundsFrame();assert(!audio.world_.rtcwLoops_.Size()&&!audio.world_.loopVoices_[0].refreshedThisFrame);
 for(int i=0;i<128;++i)audio.AddRTCWLoopingSound(zero,velocity,0,1,255);
 int lookups=audio.lookups;audio.AddRTCWLoopingSound(zero,velocity,0,INT_MAX,255);assert(audio.lookups==lookups);
 audio.world_.ClearLoopingSoundsFrame();audio.AddRTCWLoopingSound(zero,velocity,0,INT_MAX,0);assert(audio.lookups==lookups);
 for(int gate=0;gate<3;++gate){audio.started_=gate!=0;audio.hardMuted_=gate==1;cls.state=gate==2?0:CA_ACTIVE;
  audio.AddRTCWLoopingSound(zero,velocity,0,INT_MAX,255);assert(audio.lookups==lookups);}
 audio.started_=true;audio.hardMuted_=false;cls.state=CA_ACTIVE;
 bool failed=false;try{audio.AddRTCWLoopingSound(zero,velocity,0,INT_MAX,255);}catch(int){failed=true;}assert(failed);
 for(bool sp:{false,true})for(float fade:{0.f,0.001f,0.125f,0.333f,0.5f,0.999f,1.f,2.f,16.f}){
  isSP=sp;master.current=fade;audio.world_.ClearLoopingSoundsFrame();
  audio.AddRTCWLoopingSound(zero,velocity,0,1,255);assert(audio.world_.rtcwLoops_.Size()==1);
  const int captured=int(255.f*(sp?fade:1.f));assert(audio.world_.loopVoices_[0].rtcwLoopData.volume==captured);
  master.current=1;audio.world_.Update(qfalse);
  const bool audible=captured*45/256>0;
  assert((audio.world_.loopVoices_[0].source!=0)==audible);
 }
}
'''


class RTCWLoopAudioTests(unittest.TestCase):
    def compile_run(self, code):
        compiler = shutil.which('clang++')
        if not compiler:
            self.skipTest('Clang unavailable')
        target = compiler_target_args(compiler)
        if os.name == 'nt' and not target:
            target = ['-DFNTECH3_Q3_ONLY']
        elif os.name != 'nt':
            target = ['-m32']
        (ROOT / '.tmp').mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='rtcw-loops-', dir=ROOT / '.tmp') as temp:
            folder = Path(temp); source = folder / 'probe.cpp'; executable = folder / 'probe.exe'
            source.write_text(code, encoding='utf-8')
            built = subprocess.run([compiler, *target, '-std=c++17', '-O2', '-fno-fast-math', '-I', str(ROOT),
                                    str(source), '-o', str(executable)], cwd=ROOT, capture_output=True, text=True, timeout=60)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            options = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
            run = subprocess.run([str(executable)], cwd=ROOT, capture_output=True, text=True, timeout=30, **options)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)

    def test_frame_capacity_volume_range_and_gain(self):
        self.compile_run(CORE)

    def test_native_and_qvm_loop_imports(self):
        self.compile_run(imports())

    def test_actual_legacy_loop_entrypoints(self):
        self.compile_run(legacy())

    def test_actual_openal_loop_entrypoints(self):
        self.compile_run(openal())


if __name__ == '__main__':
    unittest.main()
