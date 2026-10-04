"""RTCW entity streaming lifecycle, checked imports, decoder and backend queues."""
from pathlib import Path
import unittest
import rtcw_music_tests
from rtcw_music_tests import IMPORTS
from rtcw_music_fixture import function

ROOT = Path(__file__).resolve().parents[1]

POLICY = r'''
#include "code/client/audio/shared/RTCWEntityStreams.h"
#include <cassert>
#include <climits>
#include <cstring>
#include <vector>
using namespace fntech3::audio::rtcw;
struct Output {
 bool ready=true,fail=false;int rate=22050,closed=0,cleared=0,opened=0;
 std::array<std::string,24> names;std::array<bool,24> files{};
 bool Ready()const{return ready;}
 void Discard(int i){assert(i>0&&i<24);++cleared;}
 void Close(int i,bool discard){files[i]=false;++closed;if(discard)Discard(i);}
 int Open(int i,const char *name){names[i]=name;++opened;files[i]=!fail;return fail?0:rate;}
};
int main(){
 for(bool sp:{false,true}){
  EntityStreams pool(sp);Output out;assert(pool.Limit()==(sp?12:24));
  out.ready=false;assert(!pool.Start(out,"a",nullptr,0,3,1)&&!out.opened);out.ready=true;
  assert(!pool.Start(out,"a",nullptr,1024,3,1)&&!out.opened);
  assert(pool.Start(out,"a",nullptr,0,3,1)==1&&out.names[1]=="a.wav");
  assert(pool[1].active&&pool[1].entity==0&&pool[1].channel==3&&pool[1].attenuation==1);
  assert(pool.Start(out,"b","loop",0,3,-1)==1&&out.opened==2&&!out.cleared);
  pool.EndOfTrack(out,1);assert(out.names[1]==(sp?"loop":"b.wav"));
  pool.Fade(1,.25f,1000,48000);assert(pool.Gain(1,59025)==.625f);
  pool.EndOfTrack(out,1);assert(pool.Gain(1,70050)==.25f);
  assert(pool.Start(out,nullptr,"c",1,0,0)==2&&out.names[2]=="c.wav");
  assert(pool.Start(out,"d",nullptr,1,0,0)==3);
  pool.StopChannel(1,0);assert(!pool[2].stop&&!pool[3].stop);
  pool.StopEntity(out,1);assert(pool[2].stop&&pool[3].stop&&out.cleared==2&&pool[2].active);
  assert(!pool.BeginService(out,2,0)&&!pool.BeginService(out,3,0)&&!pool[2].active);
  assert(pool.Start(out,"global",nullptr,-1,3,1)==2);
  assert(pool.Start(out,"global",nullptr,INT_MIN,3,1)==3);
  pool.StopEntity(out,-1);pool.StopChannel(-1,3);assert(!pool[2].stop);
  pool.StopChannel(0,3);assert(pool[1].stop&&!pool.BeginService(out,1,1)&&!pool[1].active);
  assert(out.cleared==2); // A channel replacement drains queued PCM.
  assert(pool.Start(out,"pending",nullptr,0,3,0)==1);
  out.fail=true;assert(pool.Start(out,"missing",nullptr,0,3,0)==1&&!pool[1].active&&!out.files[1]);
  out.fail=false;assert(pool.Start(out,"pending",nullptr,0,3,0)==1);
  assert(pool.Start(out,nullptr,nullptr,0,3,0)==1&&!pool[1].active&&pool[1].name==".wav");
  pool.Clear(out);for(int i=1;i<pool.Limit();++i)assert(!pool[i].active&&!out.files[i]);
  std::string longName(200,'x');pool.Start(out,longName.c_str(),longName.c_str(),3,3,0);
  assert(pool[1].name==std::string(59,'x')+".wav"&&pool[1].loop==std::string(59,'x'));
  pool.Start(out,pool[1].loop.c_str(),pool[1].name.c_str(),3,3,0);
  assert(pool[1].name==std::string(59,'x')+".wav"&&pool[1].loop==std::string(59,'x'));
  pool.Clear(out);
  for(int i=1;i<pool.Limit();++i)assert(pool.Start(out,"fill",nullptr,i,3,0)==i);
  const auto count=out.opened;assert(!pool.Start(out,"full",nullptr,99,3,0)&&out.opened==count);
  assert(pool.Start(out,"override",nullptr,1,3,0)==1);
  pool.Fade(1,0,0,48000);assert(pool.Gain(1,48000)==0&&pool[1].stop);
  assert(!pool.BeginService(out,1,0)&&!pool[1].active);
  pool.Clear(out);pool.Fade(-1,0,0,10);pool.Fade(24,0,0,10);assert(pool.Gain(0,0)==0);
 }
 const float zero[3]={},side[3]={0,80,0},far[3]={2000,0,0};const float axis[3][3]={{1,0,0},{0,1,0},{0,0,1}};
 auto gains=EntityStreamGains(nullptr,zero,axis,2,16,16);assert(gains.left==131072&&gains.right==131072);
 gains=EntityStreamGains(side,zero,axis,1,1,1);assert(gains.left==256&&gains.right==0);
 gains=EntityStreamGains(side,zero,axis,2,.5f,.25f);assert(gains.left==32&&gains.right==0);
 gains=EntityStreamGains(far,zero,axis,2,1,1);assert(!gains.left&&!gains.right);
 gains=EntityStreamGains(nullptr,zero,axis,NAN,1,1);assert(!gains.left&&!gains.right);
}
'''

CODEC = r'''
#include <array>
#include <vector>
#include <string>
#include <cassert>
#include <cstring>
#include <cstdint>
struct snd_info_t {int rate=22050,width=2,channels=2;};
struct snd_stream_t {snd_info_t info;int remaining=40000;std::string name;};
static int opens=0,closes=0,reads=0;static std::vector<std::string> names;
static snd_stream_t *S_CodecOpenStream(const char *name){
 names.push_back(name);if(std::string(name).find("missing")!=std::string::npos)return nullptr;
 auto *stream=new snd_stream_t;stream->name=name;++opens;
 if(stream->name.find("empty")!=std::string::npos)stream->remaining=0;
 if(stream->name.find("short")!=std::string::npos)stream->remaining=12;
 if(stream->name.find("bad-rate")!=std::string::npos)stream->info.rate=0;
 if(stream->name.find("bad-width")!=std::string::npos)stream->info.width=3;
 if(stream->name.find("bad-channels")!=std::string::npos)stream->info.channels=3;
 return stream;
}
static void S_CodecCloseStream(snd_stream_t *stream){assert(stream);++closes;delete stream;}
static int S_CodecReadStream(snd_stream_t *stream,int bytes,void *data){
 ++reads;if(stream->name.find("broken")!=std::string::npos)return -1;
 if(stream->name.find("partial")!=std::string::npos)return 3;
 if(stream->name.find("oversized")!=std::string::npos)return bytes+4;
 const int count=std::min(bytes,stream->remaining);assert(count>=0&&count%4==0);
 for(int i=0;i<count;i+=4){auto *out=static_cast<unsigned char *>(data)+i;out[0]=0xe8;out[1]=0x03;out[2]=0x30;out[3]=0xf8;}
 stream->remaining-=count;return count;
}
#include "code/client/audio/shared/RTCWStreamFiles.h"
'''

DECODER = CODEC + r'''
using namespace fntech3::audio::rtcw;
struct Output{
 bool ready=true,prepare=true,submit=true;int capacity=1000,submitted=0,clears=0,finishes=0;
 bool Ready()const{return ready;}bool Prepare(int){return prepare;}
 void Clear(int){++clears;}void Finish(int){++finishes;}
 int FramesNeeded(int,int){return capacity;}
 bool Submit(int i,int frames,int rate,int width,int channels,const unsigned char *data){
  assert(i>0&&i<24&&rate==22050&&width==2&&channels==2&&data[0]==0xe8);submitted+=frames;return submit;
 }
};
int main(){
 EntityStreamFiles streams;Output out;
 streams.Start(out,"speech",nullptr,1,3,0);int before=reads;streams.Service(out,1);assert(reads-before==2&&out.submitted==2000);
 out.capacity=0;before=reads;streams.Service(out,1);assert(reads==before);
 out.capacity=1000;streams.Service(out,0);assert(reads==before);
 streams.policy.StopChannel(1,3);streams.Service(out,0);assert(!streams.policy[1].active&&opens==closes&&!out.clears);
 streams.Start(out,"speech",nullptr,1,3,0);streams.StopEntity(out,1);assert(out.clears==1);
 streams.Service(out,0);assert(opens==closes);
 for(auto name:{"bad-rate","bad-width","bad-channels","missing"}){
  streams.Start(out,name,nullptr,1,3,0);assert(!streams.policy[1].active&&opens==closes);
 }
 for(auto name:{"broken","partial","oversized"}){
  streams.Start(out,name,nullptr,1,3,0);streams.Service(out,1);assert(streams.policy[1].stop);
  streams.Service(out,0);assert(!streams.policy[1].active&&opens==closes);
 }
 streams.Start(out,"short",nullptr,1,3,0);const int submitted=out.submitted;streams.Service(out,1);
 assert(out.submitted-submitted==3&&streams.policy[1].stop);streams.Service(out,0);assert(opens==closes);
 for(bool sp:{false,true}){
  streams.Clear(out);streams.SetProfile(sp);streams.Start(out,"empty","empty",1,3,0);
  before=reads;for(int i=0;i<100;++i)streams.Service(out,1);assert(reads-before==100);
 }
 streams.Clear(out);assert(opens==closes);
 out.prepare=false;streams.Start(out,"speech",nullptr,1,3,0);assert(opens==closes&&!streams.policy[1].active);
 out.prepare=true;out.submit=false;streams.Start(out,"speech",nullptr,1,3,0);streams.Service(out,1);
 assert(streams.policy[1].stop);streams.Clear(out);assert(opens==closes);
 out.ready=false;before=opens;streams.Start(out,"speech",nullptr,1,3,0);streams.Service(out,1);assert(opens==before);
}
'''

def imports():
    prefix = IMPORTS[:IMPORTS.index('#include "code/client/client_rtcw_audio_compat.hpp"')]
    prefix = prefix.replace('static void S_StartRTCWEntityStream(const char *,const char *,int,int,int){assert(false);}', r'''
static int entity,channel,attenuation;
static void S_StartRTCWEntityStream(const char *a,const char *b,int e,int c,int n){operation=8;introName=a;loopName=b;entity=e;channel=c;attenuation=n;}
''').replace('static void S_StopRTCWEntityStream(int){assert(false);}', 'static void S_StopRTCWEntityStream(int e){operation=9;entity=e;}')
    return prefix + r'''
#include "code/client/client_rtcw_audio_compat.hpp"
using fntech3::rtcw::module::Call;
template<class F>void rejected(F f){const int before=operation;bool caught=false;try{f();}catch(int){caught=true;}assert(caught&&operation==before);}
int main(){
 for(bool native:{false,true})for(auto id:{COM_GAME_RTCW_SP,COM_GAME_RTCW_MP}){
  vm_t vm{native,{}};profile=id;std::memcpy(vm.bytes+13,"intro",6);std::memcpy(vm.bytes+101,"loop",5);
  auto ptr=[&](int n)->intptr_t{return native?reinterpret_cast<intptr_t>(vm.bytes+n):n;};
  for(int e:{INT_MIN,-1,0,1023})for(int c:{INT_MIN,0,3,INT_MAX})for(int n:{INT_MIN,0,1,INT_MAX}){
   intptr_t args[]{0,ptr(13),ptr(101),e,c,n};
   assert(CL_RTCWAudioCall(&vm,Call::CG_S_STARTSTREAMINGSOUND,args));assert(operation==8&&entity==e&&channel==c&&attenuation==n);
   assert(introName=="intro"&&loopName=="loop");
   if(id==COM_GAME_RTCW_SP){intptr_t stop[]{0,e};assert(CL_RTCWAudioCall(&vm,Call::CG_S_STOPSTREAMINGSOUND,stop)&&operation==9&&entity==e);}
   else assert(!CL_RTCWAudioCall(nullptr,Call::CG_S_STOPSTREAMINGSOUND,nullptr));
  }
  intptr_t empty[]{0,0,0,-1,0,0};assert(CL_RTCWAudioCall(&vm,Call::CG_S_STARTSTREAMINGSOUND,empty));assert(introName.empty()&&loopName.empty());
  for(int e:{1024,INT_MAX}){intptr_t args[]{0,ptr(13),ptr(101),e,3,1};rejected([&]{CL_RTCWAudioCall(&vm,Call::CG_S_STARTSTREAMINGSOUND,args);});}
  intptr_t args[]{0,ptr(13),ptr(101),1,3,1};rejected([&]{CL_RTCWAudioCall(nullptr,Call::CG_S_STARTSTREAMINGSOUND,args);});
  if(sizeof(intptr_t)>4)for(int word:{3,4,5}){args[word]=static_cast<intptr_t>(INT64_C(1)<<40);rejected([&]{CL_RTCWAudioCall(&vm,Call::CG_S_STARTSTREAMINGSOUND,args);});args[word]=0;}
  if(!native){args[1]=255;vm.bytes[255]='x';rejected([&]{CL_RTCWAudioCall(&vm,Call::CG_S_STARTSTREAMINGSOUND,args);});}
 }
 for(auto id:{COM_GAME_QUAKE3,COM_GAME_QUAKELIVE}){profile=id;assert(!CL_RTCWAudioCall(nullptr,Call::CG_S_STARTSTREAMINGSOUND,nullptr));}
}
'''


def legacy():
    source = (ROOT / 'code/client/audio/legacy/snd_dma.cpp').read_text()
    return CODEC + r'''
#include "code/client/audio/shared/AudioMix.h"
using byte=unsigned char;using vec3_t=float[3];
constexpr int CA_ACTIVE=8,MAX_RAW_SAMPLES=64,PAINTBUFFER_SIZE=32;
static bool s_soundStarted=true,s_soundMuted=false,isSP=true,isRTCW=true;
static bool Com_GameIsRTCWSP(){return isSP;}static bool Com_GameIsRTCW(){return isRTCW;}
static int s_soundtime=100;static struct{int state=CA_ACTIVE;}cls;
static struct{int speed=48000,channels=2;}dma;
struct portable_samplepair_t{int left=0,right=0;};
static float listener_origin[3];static std::array<vec3_t,3> listener_axis={{{1,0,0},{0,1,0},{0,0,1}}};
static std::array<vec3_t,1024> rtcwEntityPositions;
static struct{float value=1;}volume;static auto *s_volume=&volume;
static void Com_DPrintf(const char *,...){}
''' + function(source, 'static void S_Base_RawSamplesToBuffer(') + r'''
#include "code/client/audio/legacy/snd_rtcw_streams.hpp"
int main(){
 for(bool sp:{false,true}){
  isSP=sp;rtcwStreams.SetProfile(sp);s_soundtime=100;volume.value=1;
  cls.state=0;int before=opens;S_Base_StartRTCWEntityStream("speech",nullptr,3,3,0);assert(opens==before);cls.state=CA_ACTIVE;
  s_soundMuted=true;S_Base_StartRTCWEntityStream("speech",nullptr,3,3,0);assert(opens==before);s_soundMuted=false;
  S_Base_StartRTCWEntityStream("speech",nullptr,3,3,0);S_Base_UpdateRTCWEntityStreams();
  assert(rtcwStreamRaw[1].buffered&&rtcwStreamRaw[1].end>s_soundtime);
  portable_samplepair_t paint[32]{};S_PaintRTCWEntityStreams(100,104,paint);
  assert(paint[0].left==1000*256&&paint[3].right==-2000*256);
  rtcwEntityPositions[3][1]=80;S_Base_StartRTCWEntityStream("speech",nullptr,3,3,1);
  std::memset(paint,0,sizeof(paint));S_PaintRTCWEntityStreams(100,104,paint);
  assert(paint[0].left==1000*256&&!paint[0].right);
  S_Base_StopRTCWEntityStream(3);assert(!rtcwStreamRaw[1].buffered);
  std::memset(paint,0,sizeof(paint));S_PaintRTCWEntityStreams(100,104,paint);assert(!paint[0].left&&!paint[0].right);
  S_Base_UpdateRTCWEntityStreams();assert(opens==closes);
  S_Base_StartRTCWEntityStream("short",nullptr,3,3,0);S_Base_UpdateRTCWEntityStreams();
  assert(rtcwStreams.policy[1].stop);S_Base_UpdateRTCWEntityStreams();assert(!rtcwStreams.policy[1].active);
  std::memset(paint,0,sizeof(paint));S_PaintRTCWEntityStreams(100,104,paint);assert(paint[0].left==1000*256);
  // Finished file tails drain; explicit whole-stream clears discard them.
  S_Base_ClearRTCWEntityStreams();assert(!rtcwStreamRaw[1].buffered&&opens==closes);
  S_Base_StartRTCWEntityStream("speech",nullptr,3,3,0);S_Base_UpdateRTCWEntityStreams();
  if(sp){S_Base_FadeRTCWEntityStream(0,0,1);std::memset(paint,0,sizeof(paint));S_PaintRTCWEntityStreams(100,104,paint);assert(!paint[0].left);}
  S_Base_ClearRTCWEntityStreams();assert(opens==closes);
 }
 // Safe signed-clock wrap and saturated accumulation, using real queued PCM.
 isSP=true;s_soundtime=INT32_MAX-1;S_Base_StartRTCWEntityStream("speech",nullptr,3,3,0);S_Base_UpdateRTCWEntityStreams();
 portable_samplepair_t paint[32]{};paint[0]={INT32_MAX,INT32_MIN};
 S_PaintRTCWEntityStreams(s_soundtime,int32_t(uint32_t(s_soundtime)+4u),paint);
 assert(paint[0].left==INT32_MAX&&paint[0].right==INT32_MIN&&paint[3].left==256000);
 isRTCW=false;std::memset(paint,0,sizeof(paint));S_PaintRTCWEntityStreams(s_soundtime,int32_t(uint32_t(s_soundtime)+4u),paint);assert(!paint[0].left);
 S_Base_ClearRTCWEntityStreams();assert(opens==closes);
}
'''


def openal():
    return CODEC + r'''
#include "code/client/audio/shared/RTCWSpeech.h"
#include <algorithm>
#include <cmath>
using byte=unsigned char;using ALuint=unsigned;
constexpr int CA_ACTIVE=8,AL_MAX_GAIN=1,kMaxStreamBuffers=128;
static bool isSP=true,isRTCW=true,muted=false;static std::int64_t now=48000;
static bool Com_GameIsRTCWSP(){return isSP;}static bool Com_GameIsRTCW(){return isRTCW;}
static struct{int state=CA_ACTIVE;}cls;static struct{float value=1;}volume;static auto *s_volume=&volume;
static void Com_DPrintf(const char *,...){}
static fntech3::audio::rtcw::MasterFade master;
static auto &S_RTCWMasterFade(){return master;}
struct Device{
 struct Pool{int free=100;int FreeCount()const{return free;}int TotalCount()const{return 128;}}pool;
 int allocated=0;bool fail=false;unsigned next=0;
 int FreeVoiceCount()const{return 96-allocated;}unsigned AcquireVoiceSource(){if(fail)return 0;++allocated;return ++next;}
 void ReleaseVoiceSource(unsigned source){assert(source&&allocated>0);--allocated;}
 Pool &BufferPool(){return pool;}Device &AL(){return *this;}
 void alSourcef(unsigned,int,float){}
};
struct StreamPlayer{
 bool deferStart=false,trackRTCWSpeech=false;int queued=0;float gain=1;unsigned source=0;std::vector<short> pcm;
 void Init(Device *,unsigned s){source=s;Clear();}void Clear(){queued=0;pcm.clear();}
 void Shutdown(){Clear();source=0;}void Update(float g,bool=true){gain=g;}
 int QueuedBufferCount()const{return queued;}bool Idle()const{return !queued;}
};
struct World{
 float listener[3]{},axis[3][3]{{1,0,0},{0,1,0},{0,0,1}},origin[3]{0,80,0};
 auto EntityStreamGains(const fntech3::audio::rtcw::EntityStream &s,float v,float f,float m)const{
  return fntech3::audio::rtcw::EntityStreamGains(s.attenuation&&s.entity>=0?origin:nullptr,listener,axis,v,f,m);
 }
};
static float ReadStreamPCM16Sample(const byte *data,size_t sample,int width,int channels){
 if(width==2){short value;memcpy(&value,data+sample*2,2);return value;}
 return (channels==1?int(data[sample])-128:int(static_cast<signed char>(data[sample])))*256;
}
class AudioSystem{
public:
 struct RTCWEntityOutput;struct EntityStreamLane{std::array<StreamPlayer,2>players;std::array<ALuint,2>sources{};};
 std::array<EntityStreamLane,24>entityStreamLanes_;fntech3::audio::rtcw::EntityStreamFiles entityStreams_;
 Device device_;World world_;bool started_=true,hardMuted_=false,queueFail=false;
 std::int64_t RTCWClock(){return now;}bool IsSoftMuted(){return muted;}
 bool FlushStream(StreamPlayer &,bool=false){return true;}
 bool QueueStreamChunk(StreamPlayer &p,int frames,int rate,int width,int channels,const byte *data,float gain,int out,bool){
  assert(p.source&&p.deferStart&&frames>0&&rate==22050&&width==2&&channels==2&&out==2&&gain==1);
  if(queueFail)return false;++p.queued;const auto before=p.pcm.size();p.pcm.resize(before+frames*2);memcpy(p.pcm.data()+before,data,frames*4);return true;
 }
 void StartRTCWEntityStream(const char *,const char *,int,int,int);void StopRTCWEntityStream(int);
 void FadeRTCWEntityStream(float,int,int);void ClearRTCWEntityStreams();void ServiceRTCWEntityStreams();
};
#include "code/client/audio/openal/AudioSystemRTCWStreams.inl"
int main(){
 for(bool sp:{false,true}){
  AudioSystem s;isSP=sp;s.entityStreams_.SetProfile(sp);volume.value=1;
  s.StartRTCWEntityStream("speech",nullptr,3,3,0);assert(s.device_.allocated==2);
  s.ServiceRTCWEntityStreams();auto &lane=s.entityStreamLanes_[1];
  assert(lane.players[0].queued==2&&lane.players[1].queued==2);
  assert(lane.players[0].pcm[0]==1000&&!lane.players[0].pcm[1]&&!lane.players[1].pcm[0]&&lane.players[1].pcm[1]==-2000);
  assert(lane.players[0].gain==1&&lane.players[1].gain==1);
  s.StartRTCWEntityStream("speech",nullptr,3,3,1);s.ServiceRTCWEntityStreams();
  assert(lane.players[0].gain==1&&!lane.players[1].gain&&lane.players[0].queued==(sp?4:2));
  assert(lane.players[0].trackRTCWSpeech==sp&&!lane.players[1].trackRTCWSpeech);
  volume.value=2;s.ServiceRTCWEntityStreams();assert(lane.players[0].gain==(sp?1.f:.5f));
  muted=true;s.ServiceRTCWEntityStreams();assert(!lane.players[0].gain);muted=false;
  assert(lane.players[0].queued==(sp?8:2));
  s.ServiceRTCWEntityStreams();assert(lane.players[0].queued==(sp?8:2));
  s.StopRTCWEntityStream(3);assert(!s.device_.allocated&&!lane.players[0].queued);
  volume.value=0;s.ServiceRTCWEntityStreams();assert(opens==closes);
  volume.value=1;s.StartRTCWEntityStream("short",nullptr,3,3,0);s.ServiceRTCWEntityStreams();
  assert(s.entityStreams_.policy[1].stop&&lane.players[0].queued==1);
  s.ServiceRTCWEntityStreams();assert(!s.entityStreams_.policy[1].active&&lane.players[0].queued==1);
  lane.players[0].Clear();lane.players[1].Clear();s.ServiceRTCWEntityStreams();assert(!s.device_.allocated);
  s.queueFail=true;s.StartRTCWEntityStream("speech",nullptr,3,3,0);s.ServiceRTCWEntityStreams();
  assert(s.entityStreams_.policy[1].stop&&!s.device_.allocated);s.ServiceRTCWEntityStreams();assert(opens==closes);s.queueFail=false;
  s.device_.fail=true;s.StartRTCWEntityStream("speech",nullptr,3,3,0);assert(!s.entityStreams_.policy[1].active&&!s.device_.allocated);s.device_.fail=false;
  s.StartRTCWEntityStream("speech",nullptr,3,3,0);s.ServiceRTCWEntityStreams();
  if(sp){s.FadeRTCWEntityStream(0,0,1);s.ServiceRTCWEntityStreams();assert(!lane.players[0].gain&&!s.entityStreams_.policy[1].active);}
  s.ClearRTCWEntityStreams();assert(opens==closes&&!s.device_.allocated);
 }
}
'''


def commands():
    source = (ROOT / 'code/client/audio/snd_main.cpp').read_text()
    prefix = r'''
#include <vector>
#include <string>
#include <cstdlib>
#include <cerrno>
#include <climits>
#include <cassert>
static std::vector<std::string> args;static int calls=0,entity,channel,attenuation;static std::string intro,loop;
static int Cmd_Argc(){return int(args.size());}static const char *Cmd_Argv(int i){return args.at(i).c_str();}
static void Com_Printf(const char *,...){}
static void S_StartRTCWEntityStream(const char *a,const char *b,int e,int c,int n){++calls;intro=a;loop=b?b:"";entity=e;channel=c;attenuation=n;}
static void S_StopRTCWEntityStream(int e){++calls;entity=e;}
'''
    return prefix + '\n'.join(function(source, signature) for signature in (
        'static bool S_RTCWStreamInteger(', 'static void S_StreamingSound_f(', 'static void S_StopStreamingSound_f(')) + r'''
int main(){
 args={"streamingsound","file"};S_StreamingSound_f();assert(calls==1&&intro=="file"&&loop.empty()&&entity==-1&&channel==0&&attenuation==0);
 args={"streamingsound","a","1000","3","1","b"};S_StreamingSound_f();assert(calls==2&&intro=="a"&&loop=="b"&&entity==1000&&channel==3&&attenuation==1);
 for(auto text:{"", "x", "1x", "1024", "2147483648", "99999999999999999999999999999"}){
  args={"streamingsound","a",text,"3","1"};S_StreamingSound_f();assert(calls==2);
  args={"stopstreamingsound",text};S_StopStreamingSound_f();assert(calls==2);
 }
 for(auto list:{std::vector<std::string>{"streamingsound"},{"streamingsound","a","1"},{"streamingsound","a","1","3"}}){args=list;S_StreamingSound_f();assert(calls==2);}
 args={"stopstreamingsound","-1"};S_StopStreamingSound_f();assert(calls==3&&entity==-1);
 args={"stopstreamingsound","1000"};S_StopStreamingSound_f();assert(calls==4&&entity==1000);
}
'''

class RTCWEntityStreamTests(unittest.TestCase):
    compile_run = rtcw_music_tests.RTCWMusicTests.compile_run

    def test_stream_pool_lifetime_and_gains(self):
        self.compile_run(POLICY, ('-O2', '-fno-fast-math'))

    def test_codec_lifetime_bounds_failures_and_work_limit(self):
        self.compile_run(DECODER, ('-O2', '-fno-fast-math'))

    def test_exact_native_and_qvm_stream_imports(self):
        self.compile_run(imports())

    def test_actual_legacy_stream_queue_stop_fade_and_mix(self):
        self.compile_run(legacy(), ('-O2', '-fno-fast-math'))

    def test_actual_openal_stream_pair_lifetime_and_gain(self):
        self.compile_run(openal(), ('-O2', '-fno-fast-math'))

    def test_console_stream_commands_validate_integers(self):
        self.compile_run(commands())


if __name__ == '__main__':
    unittest.main()
