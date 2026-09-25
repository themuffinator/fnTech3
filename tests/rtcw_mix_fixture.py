"""Run the actual legacy PCM, raw-ring and SP mix stages without an audio device."""
from rtcw_music_fixture import COMMON, ROOT, function


def mixer():
    mix = (ROOT / 'code/client/audio/legacy/snd_mix.cpp').read_text()
    dma = (ROOT / 'code/client/audio/legacy/snd_dma.cpp').read_text()
    methods = '\n'.join(function(mix, name) for name in (
        'static void S_PaintChannelFrom16_scalar(', 'static void S_PaintChannelFrom16(',
        'static void S_PaintChannelFromWavelet(', 'static void S_PaintChannelFromADPCM(',
        'static void S_PaintChannelFromMuLaw(', 'extern "C" void S_PaintChannels('))
    return COMMON + r'''
#include <climits>
// The painter reads American McGee's Alice's loop blocks; these probes never set one.
#include "code/client/audio/shared/AliceSound.h"
constexpr int PAINTBUFFER_SIZE=8,MAX_CHANNELS=96,SND_CHUNK_SIZE=8,SND_CHUNK_SIZE_FLOAT=4;
struct portable_samplepair_t {int left=0,right=0;};
static void S_PaintRTCWEntityStreams(int,int,portable_samplepair_t *){}
static std::array<portable_samplepair_t,PAINTBUFFER_SIZE> paintbuffer;
static portable_samplepair_t s_rawsamples[MAX_RAW_SAMPLES];
static std::array<portable_samplepair_t,MAX_RAW_SAMPLES> s_rtcwMusicRawSamples;
static int s_rawend=0,s_rtcwMusicRawEnd=0,s_paintedtime=0,s_soundtime=0,snd_vol=0;
static bool s_soundStarted=true,s_soundMuted=false,gw_active=true,gw_minimized=false,gw_offscreen=false;
static struct {int integer=0;} mute;
static auto *s_muteWhenUnfocused=&mute,*s_muteWhenMinimized=&mute;
static byte hardware[128],secondary[128];static byte *dma_buffer2=secondary;
static struct {byte *buffer=hardware;int samples=64,samplebits=16,speed=48000;} dma;
struct sndBuffer {short sndChunk[SND_CHUNK_SIZE]{};sndBuffer *next=nullptr;};
struct sfx_t {sndBuffer *soundData=nullptr;int soundLength=0,soundChannels=1,soundCompressionMethod=0;};
struct channel_t {int leftvol=0,rightvol=0,startSample=0;bool doppler=false;float dopplerScale=1,oldDopplerScale=1;sfx_t *thesfx=nullptr;int loopBlockStart=0,loopBlockEnd=0;};
static channel_t s_channels[MAX_CHANNELS],loop_channels[MAX_CHANNELS];static int numLoopChannels=0;
static short sfxScratchBuffer[SND_CHUNK_SIZE*4],mulawToShort[256];
static int sfxScratchIndex=-1;static sfx_t *sfxScratchPointer=nullptr;
// Codec output is controlled; the production channel painters perform all mixing.
static void S_AdpcmGetSamples(sndBuffer *chunk,short *out){std::copy_n(chunk->sndChunk,SND_CHUNK_SIZE,out);}
static void decodeWavelet(sndBuffer *chunk,short *out){S_AdpcmGetSamples(chunk,out);}
static float gain=1;
static float S_Base_RTCWMasterGain(){return gain;}
static portable_samplepair_t voiceSample;
static void S_PaintVoiceSamples(int first,int end,portable_samplepair_t *out){
 for(int i=0;i<end-first;++i){out[i].left=fntech3::audio::AddMixSample(out[i].left,voiceSample.left);out[i].right=fntech3::audio::AddMixSample(out[i].right,voiceSample.right);}
}
static std::vector<portable_samplepair_t> rendered;
static void S_TransferPaintBuffer(int end,byte *){rendered.insert(rendered.end(),paintbuffer.begin(),paintbuffer.begin()+end-s_paintedtime);}
''' + function(dma, 'extern "C" void S_PaintRTCWMusic(') + '\n' + methods + '\n' + function(dma, 'static void S_Base_RawSamplesToBuffer(') + r'''
static int effect(int sample,int volume){return int(std::floor(double(sample)*volume/256.0));}
int main(){
 using namespace fntech3::audio;
 for(int value:{INT_MIN,-32768,-1,0,1,32767,INT_MAX})for(float fade:{-1.f,0.f,0.001f,0.333f,1.f,2.f,16.f}){
  const int expected=int(std::clamp(double(value)*std::max(fade,0.f),double(INT_MIN),double(INT_MAX)));
  assert(rtcw::ScaleStreamSample(value,fade)==expected);
 }
 assert(rtcw::PlaybackGain(NAN)==0 && rtcw::EffectsMixVolume(NAN,1)==0);
 for(int previous:{INT_MIN,-10000,0,10000,INT_MAX})for(int sample:{-32768,-1,0,1,32767})for(int volume:{0,255,256,65280,130560}){
  const double expected=std::clamp(double(previous)+std::floor(double(sample)*volume/256.0),double(INT_MIN),double(INT_MAX));
  assert(MixPCM16(previous,sample,volume)==int(expected));
 }
 assert(AddFractionalMixSample(INT_MAX,1)==INT_MAX && AddFractionalMixSample(INT_MIN,-1)==INT_MIN);
 assert(AddFractionalMixSample(123,NAN)==123);
 sndBuffer one,loop;one.sndChunk[0]=32767;one.sndChunk[1]=-32768;one.sndChunk[2]=1234;loop.sndChunk[0]=-32768;
 sfx_t oneSfx{&one,3,1,0},loopSfx{&loop,1,1,0};
 s_channels[0]={255,127,100,false,1,1,&oneSfx};loop_channels[0]={44,89,0,false,1,1,&loopSfx};numLoopChannels=1;
 voiceSample={128,-128};
 for(bool sp:{false,true})for(float volume:{0.f,0.333f,1.f,2.f})for(float fade:{0.f,0.125f,0.333f,0.5f,1.f,2.f,16.f}){
  isSP=sp;effectsVolume.value=volume;gain=fade;s_paintedtime=100;s_rawend=s_rtcwMusicRawEnd=103;
  for(int i=100;i<103;++i){s_rawsamples[i&(MAX_RAW_SAMPLES-1)]={4096,-8192};s_rtcwMusicRawSamples[i&(MAX_RAW_SAMPLES-1)]={8192,-4096};}
  rendered.clear();S_PaintChannels(103);assert(rendered.size()==3);
  const int mixVolume=sp?int(int(volume*256)*std::min(fade,1.f)):int(volume*255);
  assert(snd_vol==mixVolume);
  for(int i=0;i<3;++i){
   const int left=int((4096+128+(sp?8192:0))*(sp?fade:1.f));
   const int right=int((-8192-128-(sp?4096:0))*(sp?fade:1.f));
   assert(rendered[i].left==left+effect(one.sndChunk[i],255*mixVolume)+effect(-32768,44*mixVolume));
   assert(rendered[i].right==right+effect(one.sndChunk[i],127*mixVolume)+effect(-32768,89*mixVolume));
  }
 }
 // Exercise every actual compressed-channel painter with extreme decoded PCM.
 snd_vol=512;channel_t loud{255,255};
 for(auto painter:{S_PaintChannelFromADPCM,S_PaintChannelFromWavelet}){
  paintbuffer.fill({});sfxScratchIndex=-1;sfxScratchPointer=nullptr;
  painter(&loud,&oneSfx,3,0,0);
  for(int i=0;i<3;++i)assert(paintbuffer[i].left==effect(one.sndChunk[i],255*512));
 }
 mulawToShort[0]=32767;std::memset(loop.sndChunk,0,sizeof(loop.sndChunk));paintbuffer.fill({});
 S_PaintChannelFromMuLaw(&loud,&loopSfx,1,0,0);assert(paintbuffer[0].left==effect(32767,255*512));
 // Music ring tails, wrap, saturation, empty/stale cursors and partial blocks.
 const int first=INT_MAX-1;const int end=int32_t(uint32_t(first)+4u);
 for(int i=0;i<4;++i)s_rtcwMusicRawSamples[(uint32_t(first)+i)&(MAX_RAW_SAMPLES-1)]={10,-20};
 s_rtcwMusicRawEnd=end;paintbuffer.fill({});paintbuffer[0]={INT_MAX,INT_MIN};
 S_PaintRTCWMusic(first,end,paintbuffer.data());
 assert(paintbuffer[0].left==INT_MAX&&paintbuffer[0].right==INT_MIN);
 assert(paintbuffer[3].left==10&&paintbuffer[3].right==-20&&paintbuffer[4].left==0);
 s_rtcwMusicRawEnd=101;paintbuffer.fill({});S_PaintRTCWMusic(100,104,paintbuffer.data());assert(paintbuffer[1].left==0);
 for(int stale:{0,99,100,100+MAX_RAW_SAMPLES+1}){s_rtcwMusicRawEnd=stale;paintbuffer.fill({});S_PaintRTCWMusic(100,104,paintbuffer.data());assert(paintbuffer[0].left==0);}
 S_PaintRTCWMusic(100,99,nullptr);
 // The converter accepts unaligned PCM and keeps independent destination rings.
 std::array<byte,17> bytes{};const short pcm[]={32767,-32768,1,-1,1000,-1000,42,-42};std::memcpy(bytes.data()+1,pcm,sizeof(pcm));
 s_soundtime=INT_MAX-1;int cursor=s_soundtime;std::array<portable_samplepair_t,MAX_RAW_SAMPLES> raw{};
 S_Base_RawSamplesToBuffer(cursor,raw.data(),4,48000,2,2,bytes.data()+1,32);
 assert(uint32_t(cursor)==uint32_t(s_soundtime)+4u);
 for(int i=0;i<4;++i){const auto &sample=raw[(uint32_t(s_soundtime)+i)&(MAX_RAW_SAMPLES-1)];assert(sample.left==pcm[i*2]*8192&&sample.right==pcm[i*2+1]*8192);}
 for(int badRate:{0,-1,192001}){const int saved=cursor;S_Base_RawSamplesToBuffer(cursor,raw.data(),4,badRate,2,2,bytes.data()+1,1);assert(cursor==saved);}
 for(float invalid:{NAN,INFINITY}){const int saved=cursor;S_Base_RawSamplesToBuffer(cursor,raw.data(),4,48000,2,2,bytes.data()+1,invalid);assert(cursor==saved);}
 s_soundtime=0;cursor=0;S_Base_RawSamplesToBuffer(cursor,raw.data(),4,1,2,2,bytes.data()+1,1);assert(cursor==MAX_RAW_SAMPLES);
 S_Base_RawSamplesToBuffer(cursor,raw.data(),4,48000,2,2,bytes.data()+1,1);assert(cursor==MAX_RAW_SAMPLES);
 for(int channels:{1,2})for(int rate:{22050,48000,96000}){
  cursor=0;S_Base_RawSamplesToBuffer(cursor,raw.data(),4,rate,2,channels,bytes.data()+1,0.5f);
  const float scale=float(rate)/48000;int i=0;
  while(float(i)*scale<4){const int src=int(float(i)*scale)*channels;assert(raw[i].left==pcm[src]*128&&raw[i].right==pcm[src+(channels==2?1:0)]*128);++i;}
  assert(cursor==i);
 }
 const byte pcm8[]={0,127,128,255};
 for(int channels:{1,2}){
  cursor=0;S_Base_RawSamplesToBuffer(cursor,raw.data(),4/channels,48000,1,channels,pcm8,1);
  for(int i=0;i<cursor;++i){const int a=channels==2?int(static_cast<signed char>(pcm8[i*2])):int(pcm8[i])-128;
   const int b=channels==2?int(static_cast<signed char>(pcm8[i*2+1])):a;assert(raw[i].left==a*65536&&raw[i].right==b*65536);}
 }
}
'''
