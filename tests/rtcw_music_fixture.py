"""Controlled codec/device hosts for the actual RTCW backend entrypoints."""
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]


def function(source, signature):
    start = source.index(signature)
    while ';' in source[start:source.index('{', start)]:
        start = source.index(signature, start + len(signature))
    return source[start:source.index('\n}', start) + 2]


COMMON = r'''
#include "code/client/audio/shared/RTCWMusic.h"
#include "code/client/audio/shared/RTCWLoops.h"
#include "code/client/audio/shared/STVEFAmbient.h"
#include "code/client/audio/shared/STVEFLipSync.h"
#include "code/client/audio/shared/AudioMix.h"
#include "code/client/audio/shared/AudioSpeakerLayout.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <limits>
#include <string>
#include <vector>
using byte=unsigned char;
using qboolean=bool;
constexpr bool qfalse=false,qtrue=true;
constexpr int MAX_QPATH=64,MAX_RAW_SAMPLES=64;
#define S_COLOR_YELLOW ""
static bool isSP=true,softMuted=false;
static int Com_GameIsRTCWSP(){return isSP;}
static std::string currentMusic;
static void Cvar_Set(const char *key,const char *value){assert(std::string(key)=="s_currentMusic");currentMusic=value;}
static void Com_Printf(const char *,...){}
static void Com_DPrintf(const char *,...){}
static void Q_strncpyz(char *out,const char *in,int size){std::snprintf(out,size,"%s",in);}
static std::string SafeString(const char *text){return text?text:"";}
static float ClampFloat(float v,float a,float b){return std::clamp(v,a,b);}
struct cvar_t {float value;};
static cvar_t musicVolume{1},effectsVolume{1},voiceVolume{1};
static cvar_t *s_musicVolume=&musicVolume,*s_volume=&effectsVolume,*s_voiceVolume=&voiceVolume;
static fntech3::audio::rtcw::Music music;
static fntech3::audio::rtcw::MasterFade master;
// ET keeps its own master fade beside RTCW's. These probes drive RTCW only, so
// the ET level exists but is never set.
static fntech3::audio::rtcw::MasterFade wolfetMaster;
static int Com_GameIsWolfET(){return 0;}
// Alice's music stream is exercised by its own tests.
static int Com_GameIsAlice(){return 0;}
static auto &S_RTCWMusic(){return music;}
static auto &S_RTCWMasterFade(){return master;}
static auto &S_WolfETMasterFade(){return wolfetMaster;}
static void S_UpdateWolfETMasterFade(){}
static void UpdateWolfETMasterFade(){}
struct snd_info_t {int rate=22050,width=2,channels=2;unsigned speakerMask=0;};
struct snd_stream_t {snd_info_t info;int remaining=1000000;};
static int opened=0,closed=0,reads=0;
static std::vector<std::string> files;
static snd_stream_t *S_CodecOpenStream(const char *name) {
    files.push_back(name);
    if(std::string(name).find("missing")!=std::string::npos)return nullptr;
    auto *stream=new snd_stream_t;++opened;
    if(std::string(name).find("empty")!=std::string::npos)stream->remaining=0;
    if(std::string(name).find("invalid-rate")!=std::string::npos)stream->info.rate=0;
    return stream;
}
static void S_CodecCloseStream(snd_stream_t *stream){assert(stream);++closed;delete stream;}
static int S_CodecReadStream(snd_stream_t *stream,int size,byte *out) {
    ++reads;const int count=std::min(size,stream->remaining);
    std::memset(out,0x7f,count);stream->remaining-=count;return count;
}
'''


def openal():
    source = (ROOT / 'code/client/audio/openal/AudioSystemBackend.inl').read_text()
    io_start = source.index('struct AudioSystem::RTCWMusicOutput')
    output = source[io_start:source.index('\n};', io_start) + 3]
    methods = '\n'.join(function(source, signature) for signature in (
        'void AudioSystem::CloseBackgroundStream()',
        'fntech3::audio::rtcw::SampleTime AudioSystem::RTCWClock()',
        'void AudioSystem::StartRTCWBackgroundTrack(', 'void AudioSystem::FadeRTCWBackgroundTrack(',
        'void AudioSystem::FadeRTCWAllSounds(', 'void AudioSystem::ClearRTCWMusic()',
        'void AudioSystem::StartBackgroundTrack(', 'void AudioSystem::StopBackgroundTrack()',
        'void AudioSystem::UpdateBackgroundTrack()', 'void AudioSystem::ServiceBackgroundTrack()',
        'void AudioSystem::StopAllSounds()', 'void AudioSystem::ClearRTCWSounds(', 'void AudioSystem::DisableSounds()', 'void AudioSystem::Update( int msec )'))
    return COMMON + r'''
constexpr int kQueuedStreamChunks=4,kMaxStreamChunksPerService=2;
static std::int64_t micros=0;
static std::int64_t Sys_Microseconds(){return micros;}
struct StreamPlayer {
    int queued=0,clears=0;float gain=1;bool allowStart=true;
    void Clear(){queued=0;++clears;}
    void Update(float v,bool allow=true){gain=v;allowStart=allow;}
    int QueuedBufferCount()const{return queued;}
    bool Idle()const{return queued==0;}
};
struct Device {
    struct Caps{std::int64_t deviceClockValue=0;}caps;
    bool RefreshTimingDiagnostics(){return false;}
    const Caps &Capabilities()const{return caps;}
    float gain=1;
    void SetMasterGain(float value){gain=value;}
    void ApplyDopplerState(){}
    bool BeginDeferredUpdates(){return true;}
    void EndDeferredUpdates(){}
};
struct World {int stops=0;void StopAllSounds(){++stops;} void Update(bool){}};
static int SelectStreamOutputChannels(const Device &,int channels){return channels;}
static bool PCMChannelCountCanBeRepresented(int channels){return channels==1 || channels==2;}
static bool PCMFrameByteCount(int frames,int channels,int width,std::size_t &out){out=frames*channels*width;return true;}
class AudioSystem {
public:
    struct RTCWMusicOutput;
    bool started_=true,hardMuted_=false;Device device_;World world_;
    StreamPlayer musicPlayer_,rawPlayer_,voicePlayer_;
    snd_stream_t *backgroundStream_=nullptr;
    std::string backgroundIntro_,backgroundLoop_;
    int backgroundStereoFallbackChannels_=0,queuedCalls=0;
    fntech3::audio::rtcw::SampleClock rtcwClock_;
    void CloseBackgroundStream();
    fntech3::audio::rtcw::SampleTime RTCWClock();
    void StartRTCWBackgroundTrack(const char *,const char *,int);
    void FadeRTCWBackgroundTrack(float,int);
    void FadeRTCWAllSounds(float,int);
    void ClearRTCWMusic();
    void StartBackgroundTrack(const char *,const char *);
    void StopBackgroundTrack();
    void UpdateBackgroundTrack();void ServiceBackgroundTrack();
    void StopAllSounds();void ClearRTCWSounds(qboolean);void DisableSounds();
    void Update(int);bool UpdateDeviceState(int){return true;}
    void ClearRTCWEntityStreams(){} void ServiceRTCWEntityStreams(){}
    bool IsSoftMuted()const{return softMuted;}
    int StreamOutputRate()const{return 48000;}
    bool FlushStream(StreamPlayer &){return true;}
    bool QueueStreamChunk(StreamPlayer &player,int samples,int rate,int width,int channels,const byte *data,float volume,int outputChannels,bool logFallback) {
        assert(samples>0 && rate>0 && width==2 && channels==2 && data && outputChannels==2 && logFallback);
        assert(volume==1);++queuedCalls;++player.queued;return true;
    }
};
''' + output + '\n' + methods + r'''
int main() {
    AudioSystem system;
    system.RTCWClock();micros=1000000;
    system.StartRTCWBackgroundTrack("intro","loop",1000);
    assert(music.Active() && currentMusic=="loop" && files.back()=="intro.wav");
    assert(system.musicPlayer_.gain==0 && !system.musicPlayer_.allowStart);
    system.UpdateBackgroundTrack();assert(system.queuedCalls==2 && system.musicPlayer_.gain==0);
    micros+=250000;system.UpdateBackgroundTrack();
    assert(system.musicPlayer_.gain>0.54f && system.musicPlayer_.gain<0.55f);
    system.StartRTCWBackgroundTrack("queued",nullptr,-2);assert(currentMusic=="queued");
    musicVolume.value=0;system.StopBackgroundTrack();
    system.UpdateBackgroundTrack();assert(!music.Active() && !system.backgroundStream_ && !system.musicPlayer_.queued);
    system.UpdateBackgroundTrack();assert(!music.Active());
    musicVolume.value=1;system.UpdateBackgroundTrack();assert(music.Active() && files.back()=="queued.wav");
    assert(!system.musicPlayer_.queued);
    system.UpdateBackgroundTrack();assert(system.musicPlayer_.queued==2);
    system.FadeRTCWBackgroundTrack(0,0);system.UpdateBackgroundTrack();assert(system.musicPlayer_.gain==0);
    system.UpdateBackgroundTrack();assert(!music.Active() && !system.backgroundStream_);
    system.StartRTCWBackgroundTrack(nullptr,nullptr,-2);
    system.StartRTCWBackgroundTrack("intro","loop",0);
    system.backgroundStream_->remaining=0;system.musicPlayer_.queued=0;system.UpdateBackgroundTrack();
    assert(music.Name()=="loop.wav" && files.back()=="loop.wav");
    system.FadeRTCWAllSounds(0,1000);micros+=500000;
    assert(master.Value(system.RTCWClock())==0.5f);
    softMuted=true;system.UpdateBackgroundTrack();assert(system.musicPlayer_.gain==0);
    softMuted=false;
    system.StartRTCWBackgroundTrack("next",nullptr,-2);system.StopAllSounds();
    assert(!music.Active() && music.Queued().empty() && !system.backgroundStream_);
    system.StartRTCWBackgroundTrack("missing","loop",0);assert(!music.Active() && !system.backgroundStream_);
    system.StartRTCWBackgroundTrack("invalid-rate","loop",0);assert(!music.Active() && !system.backgroundStream_);
    const int previousReads=reads;
    system.StartRTCWBackgroundTrack("empty","empty",0);
    for(int i=0;i<5;++i)system.UpdateBackgroundTrack();
    assert(reads-previousReads==5);
    system.DisableSounds();assert(!music.Active() && !system.backgroundStream_ && system.hardMuted_);
    assert(opened==closed);
    system.hardMuted_=false;master={};effectsVolume.value=0;
    system.StartRTCWBackgroundTrack("intro","loop",0);system.Update(16);
    assert(system.device_.gain==1 && system.musicPlayer_.gain==1);
    system.FadeRTCWAllSounds(0.25f,0);system.Update(16);
    assert(system.device_.gain==1 && system.musicPlayer_.gain==0.25f && system.rawPlayer_.gain==0.25f && system.voicePlayer_.gain==0.25f);
    softMuted=true;system.Update(16);assert(system.musicPlayer_.gain==0 && system.rawPlayer_.gain==0);
    softMuted=false;
    system.FadeRTCWAllSounds(2,0);system.Update(16);
    assert(system.device_.gain==1 && system.musicPlayer_.gain==2 && system.rawPlayer_.gain==2);
    system.StartRTCWBackgroundTrack("preserved",nullptr,-2);
    const auto *preserved=system.backgroundStream_;const int buffered=system.musicPlayer_.queued;
    system.rawPlayer_.queued=3;system.voicePlayer_.queued=2;
    const int stops=system.world_.stops;
    system.ClearRTCWSounds(false);
    assert(system.backgroundStream_==preserved && music.Queued()=="preserved" && music.Active());
    assert(system.musicPlayer_.queued>=buffered && !system.rawPlayer_.queued && !system.voicePlayer_.queued);
    assert(system.world_.stops==stops+1);
    system.started_=false;system.ClearRTCWSounds(true);
    assert(system.backgroundStream_==preserved && system.world_.stops==stops+1);
    system.started_=true;system.ClearRTCWSounds(true);
    assert(!system.backgroundStream_ && music.Queued().empty() && !music.Active() && !system.musicPlayer_.queued);
    system.StopAllSounds();effectsVolume.value=0.6f;
    for(int otherProfile=0;otherProfile<2;++otherProfile) {
        isSP=false;system.hardMuted_=false;
        system.StartBackgroundTrack("other-intro","other-loop");
        assert(files.back()=="other-intro" && system.backgroundLoop_=="other-loop");
        system.UpdateBackgroundTrack();assert(system.musicPlayer_.queued==2);
        system.Update(16);assert(system.device_.gain==0.6f && system.musicPlayer_.gain==1);
        system.StopBackgroundTrack();assert(!system.backgroundStream_ && !system.musicPlayer_.queued);
    }
    assert(opened==closed);
}
'''


def legacy():
    source = (ROOT / 'code/client/audio/legacy/snd_dma.cpp').read_text()
    io_start = source.index('struct RTCWMusicOutput')
    output = source[io_start:source.index('\n};', io_start) + 3]
    methods = '\n'.join(function(source, signature) for signature in (
        'static void S_Base_StopBackgroundTrack( void ) {',
        'static void S_OpenBackgroundStream(', 'static void S_Base_StartRTCWBackgroundTrack(',
        'static void S_Base_FadeRTCWBackgroundTrack(', 'static void S_Base_FadeRTCWAllSounds(',
        'extern "C" float S_Base_RTCWMasterGain(', 'static void S_Base_StartBackgroundTrack(',
        'static void S_Base_UpdateBackgroundTrack( void ) {', 'static void S_Base_ClearSoundBuffer( void ) {',
        'static void S_Base_StopAllSounds( void ) {', 'static void S_Base_ClearRTCWSounds('))
    return COMMON + r'''
static bool s_soundStarted=true,s_soundMuted=false;
static snd_stream_t *s_backgroundStream=nullptr;
static std::array<char,MAX_QPATH> s_backgroundLoop;
static int s_soundtime=48000,s_rawend=48000,s_rtcwMusicRawEnd=48000,rawCalls=0;
static float lastVolume=0;
struct portable_samplepair_t{int left=0,right=0;};
static portable_samplepair_t s_rawsamples[MAX_RAW_SAMPLES];
static std::array<portable_samplepair_t,MAX_RAW_SAMPLES> s_rtcwMusicRawSamples;
constexpr int MAX_CHANNELS=96;
struct loopSound_t{bool active=false;};static std::array<loopSound_t,1024> loopSounds;
static fntech3::audio::rtcw::LoopFrame rtcwLoops;
static fntech3::audio::stvef::AmbientLoopFrame stvefAmbientLoops;
static fntech3::audio::stvef::lipsync::Mouths stvefMouths;
struct channel_t{int active=0;};static channel_t loop_channels[MAX_CHANNELS];static int numLoopChannels;
static int channels=0,voiceLanes=0,submits=0;
static void S_ChannelSetup(){channels=0;}static void ResetVoiceLanes(){voiceLanes=0;}
static void S_Base_ClearRTCWEntityStreams(){}
static void SNDDMA_BeginPainting(){}static void SNDDMA_Submit(){++submits;}
static byte hardware[128];
static struct{int speed=48000,samplebits=16,samples=64;byte *buffer=hardware;}dma;
static void S_Base_RawSamplesToBuffer(int &rawEnd,portable_samplepair_t *buffer,int samples,int rate,int width,int channels,const byte *data,float volume) {
    assert(samples>0 && rate>0 && width==2 && channels==2 && data);
    assert(buffer==(isSP?s_rtcwMusicRawSamples.data():s_rawsamples));
    if(rawEnd<s_soundtime)rawEnd=s_soundtime;
    ++rawCalls;lastVolume=volume;rawEnd+=samples*dma.speed/rate;
}
''' + output + '\n' + methods + r'''
int main() {
    S_Base_StartRTCWBackgroundTrack("intro","loop",1000);
    assert(music.Active() && files.back()=="intro.wav" && currentMusic=="loop");
    S_Base_UpdateBackgroundTrack();assert(rawCalls>0 && lastVolume==0);
    s_soundtime+=11025;S_Base_UpdateBackgroundTrack();assert(lastVolume==0.5f);
    S_Base_StartRTCWBackgroundTrack("queued",nullptr,-2);musicVolume.value=0;
    S_Base_StopBackgroundTrack();S_Base_UpdateBackgroundTrack();assert(!music.Active() && !s_backgroundStream && !s_rtcwMusicRawEnd && s_rawend==48000);
    S_Base_UpdateBackgroundTrack();assert(!music.Active());
    musicVolume.value=1;S_Base_UpdateBackgroundTrack();assert(music.Active() && files.back()=="queued.wav");
    S_Base_FadeRTCWBackgroundTrack(0,0);S_Base_UpdateBackgroundTrack();assert(lastVolume==0);
    S_Base_UpdateBackgroundTrack();assert(!music.Active() && !s_backgroundStream);
    S_Base_StartRTCWBackgroundTrack(nullptr,nullptr,-2);
    S_Base_StartRTCWBackgroundTrack("intro","loop",0);s_rtcwMusicRawEnd=s_soundtime;s_backgroundStream->remaining=0;
    S_Base_UpdateBackgroundTrack();assert(music.Name()=="loop.wav");
    S_Base_FadeRTCWAllSounds(0,1000);s_soundtime+=24000;assert(S_Base_RTCWMasterGain()==0.5f);
    S_Base_StopAllSounds();assert(!music.Active() && music.Queued().empty() && !s_backgroundStream);
    S_Base_StartRTCWBackgroundTrack("missing","loop",0);assert(!music.Active() && !s_backgroundStream);
    S_Base_StartRTCWBackgroundTrack("invalid-rate","loop",0);assert(!music.Active() && !s_backgroundStream);
    const int previousReads=reads;
    S_Base_StartRTCWBackgroundTrack("empty","empty",0);s_rtcwMusicRawEnd=s_soundtime;
    for(int i=0;i<5;++i)S_Base_UpdateBackgroundTrack();
    assert(reads-previousReads==5);
    S_Base_StopAllSounds();assert(opened==closed);
    S_Base_StartRTCWBackgroundTrack("intro","loop",0);S_Base_UpdateBackgroundTrack();
    S_Base_StartRTCWBackgroundTrack("preserved",nullptr,-2);
    auto *preserved=s_backgroundStream;const int buffered=s_rtcwMusicRawEnd;
    channels=3;voiceLanes=2;s_rawend=s_soundtime+32;loopSounds[0].active=true;numLoopChannels=1;loop_channels[0].active=1;
    const float zero[3]={0,0,0};rtcwLoops.Append(zero,zero,0,1,255);
    std::memset(hardware,0x42,sizeof(hardware));const int submitted=submits;
    S_Base_ClearRTCWSounds(false);
    assert(s_backgroundStream==preserved && music.Active() && music.Queued()=="preserved" && s_rtcwMusicRawEnd>=buffered);
    assert(!channels && !voiceLanes && !s_rawend && !loopSounds[0].active && !rtcwLoops.Size() && !numLoopChannels && !loop_channels[0].active);
    assert(submits==submitted && hardware[0]==0x42);
    s_soundStarted=false;S_Base_ClearRTCWSounds(true);assert(s_backgroundStream==preserved && submits==submitted);
    s_soundStarted=true;S_Base_ClearRTCWSounds(true);
    assert(!s_backgroundStream && !music.Active() && music.Queued().empty() && !s_rtcwMusicRawEnd && !s_rawend);
    for(byte value:hardware)assert(value==0);assert(submits==submitted+1);
    dma.samplebits=8;dma.samples=128;std::memset(hardware,0x42,sizeof(hardware));S_Base_ClearRTCWSounds(true);
    for(byte value:hardware)assert(value==0x80);
    const int beforeMuted=reads;s_soundMuted=true;S_Base_UpdateBackgroundTrack();assert(reads==beforeMuted);s_soundMuted=false;
    for(int otherProfile=0;otherProfile<2;++otherProfile) {
        isSP=false;s_rawend=s_soundtime;
        S_Base_StartBackgroundTrack("other-intro","other-loop");
        assert(files.back()=="other-intro" && std::string(s_backgroundLoop.data())=="other-loop");
        S_Base_UpdateBackgroundTrack();assert(lastVolume==1 && S_Base_RTCWMasterGain()==1);
        S_Base_StopBackgroundTrack();assert(!s_backgroundStream && !s_rawend);
    }
    assert(opened==closed);
}
'''


def facade():
    source = (ROOT / 'code/client/audio/snd_main.cpp').read_text()
    start = source.index('struct DisabledRTCWMusicOutput')
    output = source[start:source.index('\n};', start) + 3]
    methods = '\n'.join(function(source, signature) for signature in (
        'extern "C" void S_StartRTCWBackgroundTrack(', 'extern "C" void S_StartBackgroundTrack(',
        'extern "C" void S_FadeRTCWStreamingSound(', 'extern "C" void S_FadeRTCWAllSounds(',
        'extern "C" void S_ClearRTCWLoopingSounds('))
    return COMMON + r'''
static auto &rtcwMusic=music;
constexpr int ERR_DROP=1;
[[noreturn]] static void Com_Error(int,const char *){throw 1;}
static int operation=0,fadeTime=0;
static float fadeVolume=0;
static int framesCleared=0;static void S_ClearLoopingSoundsFrame(){++framesCleared;}
static void ClearRTCWSounds(qboolean music){operation=music?6:5;}
static void StartSP(const char *a,const char *b,int time){operation=1;fadeTime=time;assert(a && b);}
static void StartOther(const char *,const char *){operation=2;}
static void StreamFade(float volume,int time){operation=3;fadeVolume=volume;fadeTime=time;}
static void AllFade(float volume,int time){operation=4;fadeVolume=volume;fadeTime=time;}
static struct {
    decltype(&StartSP) StartRTCWBackgroundTrack=nullptr;
    decltype(&StartOther) StartBackgroundTrack=nullptr;
    decltype(&StreamFade) FadeRTCWBackgroundTrack=nullptr;
    decltype(&AllFade) FadeRTCWAllSounds=nullptr;
    decltype(&ClearRTCWSounds) ClearRTCWSounds=nullptr;
    void (*FadeRTCWEntityStream)(float,int,int)=nullptr;
}si;
''' + output + methods + r'''
int main() {
    S_StartRTCWBackgroundTrack("queued",nullptr,-2);
    assert(currentMusic=="queued" && music.Queued()=="queued" && !music.Active());
    S_StartRTCWBackgroundTrack("intro","loop",0);assert(!music.Active());
    S_FadeRTCWAllSounds(0,100);S_FadeRTCWStreamingSound(0,100,0);
    for(int mode:{-1,0,1,2,3})S_ClearRTCWLoopingSounds(mode);assert(framesCleared==5);
    si={StartSP,StartOther,StreamFade,AllFade,ClearRTCWSounds};
    for(int mode:{-1,0,1,2,3}){
        operation=0;const int frames=framesCleared;S_ClearRTCWLoopingSounds(mode);
        assert(framesCleared==frames+1 && operation==(mode==1?5:mode==2?6:0));
    }
    S_StartBackgroundTrack("intro","loop");assert(operation==1 && fadeTime==0);
    S_StartRTCWBackgroundTrack("intro","loop",500);assert(operation==1 && fadeTime==500);
    S_FadeRTCWStreamingSound(0.5f,1000,0);assert(operation==3 && fadeVolume==0.5f && fadeTime==1000);
    for(int unallocated:{-1,1,11,12,INT32_MAX}) {
        operation=0;S_FadeRTCWStreamingSound(0.5f,1000,unallocated);assert(!operation);
    }
    S_FadeRTCWAllSounds(0.25f,100);assert(operation==4 && fadeVolume==0.25f && fadeTime==100);
    for(float invalid:{std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN(),17.f,-17.f}) {
        operation=0;
        try{S_FadeRTCWAllSounds(invalid,1);assert(false);}catch(int){}
        try{S_FadeRTCWStreamingSound(invalid,1,0);assert(false);}catch(int){}
        assert(!operation);
    }
    isSP=false;S_StartBackgroundTrack("intro","loop");assert(operation==2);
    operation=0;
    try{S_StartRTCWBackgroundTrack("intro","loop",0);assert(false);}catch(int){}
    try{S_FadeRTCWAllSounds(0,1);assert(false);}catch(int){}
    try{S_ClearRTCWLoopingSounds(2);assert(false);}catch(int){}
    assert(!operation);
}
'''


def legacy_mix():
    from rtcw_mix_fixture import mixer
    return mixer()


def openal_gains():
    device = (ROOT / 'code/client/audio/openal/AudioSystemOpenAL.inl').read_text()
    streams = (ROOT / 'code/client/audio/openal/AudioSystemStreams.inl').read_text()
    world = (ROOT / 'code/client/audio/openal/AudioSystemWorld.inl').read_text()
    first = world.index('\tgain = ClampFloat( gain * gainScale')
    gain_block = world[first:world.index('\tpitch = ClampFloat(', first)]
    return COMMON + r'''
using ALuint=unsigned;using ALint=int;
constexpr int AL_MAX_GAIN=1,AL_GAIN=2,AL_DIRECT_FILTER=3,AL_FILTER_NULL=0,AL_AUXILIARY_SEND_FILTER=4,AL_EFFECTSLOT_NULL=0;
constexpr int AL_SOURCE_STATE=5,AL_PLAYING=6,AL_NO_ERROR=0;
enum class AudioFilterKind{None};
struct AudioFilterSettings {AudioFilterKind kind=AudioFilterKind::None;float gain=0;};
struct VoiceRouteCache{bool valid=false;float maxGain=1,routedGain=1;AudioFilterSettings directTone,sendTone;};
static bool FilterSettingsNearlyEqual(const AudioFilterSettings &,const AudioFilterSettings &){return true;}
static bool FilterHasAudibleEffect(const AudioFilterSettings &){return false;}
struct API{
 mutable float maximum=1,gain=1;mutable int starts=0,writes=0;
 void alSourcef(unsigned,int what,float value)const{++writes;if(what==AL_MAX_GAIN)maximum=value;else {assert(what==AL_GAIN);gain=value;}}
 void alSourcei(unsigned,int,int)const{}void alSource3i(unsigned,int,unsigned,int,unsigned)const{}
 int alGetError()const{return AL_NO_ERROR;}void alGetSourcei(unsigned,int,int *value)const{*value=0;}
 void alSourcePlay(unsigned)const{++starts;}
};
struct OpenALDevice {
 API loader_;bool efxAvailable_=false,reverbEnabled_=false;unsigned auxEffectSlot_=0;
 API &AL(){return loader_;}
 bool ConfigureFilter(unsigned,const AudioFilterSettings &)const{return false;}
 void ApplyVoiceRouting(ALuint,ALuint,ALuint,float,float,const AudioFilterSettings &,const AudioFilterSettings &,VoiceRouteCache &)const;
};
struct StreamPlayer {
 OpenALDevice *device_=nullptr;unsigned source_=1;int queuedBuffers_=1,reclaims=0;
 void ReclaimProcessedBuffers(){++reclaims;}void Update(float,bool=true);
};
static float EffectGain(float gain,bool loop,float volume,float fade){
 struct {bool rtcwLoop;float volumeScale=1;}voice{loop};
 const float gainScale=1,horizonGain=1;effectsVolume.value=volume;master.current=fade;
''' + gain_block + r'''
 return gain;
}
''' + function(device, 'void OpenALDevice::ApplyVoiceRouting(') + '\n' + function(streams, 'void StreamPlayer::Update(') + r'''
int main(){
 OpenALDevice device;AudioFilterSettings tone;
 for(bool sp:{false,true})for(bool loop:{false,true})for(float volume:{0.f,0.333f,1.f,2.f})for(float fade:{0.f,0.125f,0.333f,0.5f,1.f,2.f,16.f}){
  isSP=sp;const float actual=EffectGain(255.f/127.f,loop,volume,fade);
  const float base=loop&&sp?255.f/127.f:2.f;
  const float expected=base*(sp?float(int(int(volume*256.f)*std::min(fade,1.f)))/256.f:1.f);
  assert(actual==expected);
  VoiceRouteCache cache;device.ApplyVoiceRouting(1,0,0,actual,1,tone,tone,cache);
  assert(device.loader_.gain==expected && device.loader_.maximum==std::max(1.f,expected));
  const int writes=device.loader_.writes;device.ApplyVoiceRouting(1,0,0,actual,1,tone,tone,cache);assert(writes==device.loader_.writes);
  device.ApplyVoiceRouting(1,0,0,0,1,tone,tone,cache);assert(device.loader_.gain==0&&device.loader_.maximum==1);
 }
 StreamPlayer stream;stream.device_=&device;
 for(bool sp:{false,true})for(float gain:{0.f,0.5f,1.f,2.f,16.f,32.f,256.f,512.f}){
  isSP=sp;device.loader_.maximum=1;stream.Update(gain);
  assert(device.loader_.gain==gain&&device.loader_.maximum==(sp?std::max(1.f,gain):1.f));
 }
 isSP=true;stream.Update(NAN);assert(device.loader_.gain==0&&device.loader_.maximum==1);
 stream.Update(1024);assert(device.loader_.gain==512&&device.loader_.maximum==512);
 const int starts=device.loader_.starts;stream.Update(1,false);assert(device.loader_.starts==starts);
 stream.device_=nullptr;stream.Update(1);assert(device.loader_.starts==starts);
}
'''
