"""American McGee's Alice's audio as the engine binds it: the legacy mixer's
loop blocks, the song players across a sound-system shutdown, and the order of
S_Update's music request.

Retail alice.exe: S_StartSound hands a sound-info record's loop block to Miles
(0x0046fd79 to 0x0046fd9d); the Miles shutdown (0x0046e260) never calls
MUSIC_StopSong, and a song's saved place (+0x70) is written only by the loader
(0x0044f4c5) and MUSIC_StopSong (0x00471f79, 0x00471f97); S_Update raises the
music's re-apply flag (0x0047112c, 0x0047113f) before MUSIC_Update
(0x00471255). The probes compile the production painter
(code/client/audio/legacy/snd_mix.cpp) and song player
(code/client/audio/snd_alice.cpp) against controlled hosts; no retail file is
read.
"""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest
from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]


def function(source, signature):
    start = source.index(signature)
    while ';' in source[start:source.index('{', start)]:
        start = source.index(signature, start + len(signature))
    return source[start:source.index('\n}', start) + 2]


def between(source, first, last):
    start = source.index(first)
    return source[start:source.index(last, start)]


PAINTER_PRELUDE = r'''
#include "code/client/audio/shared/RTCWMix.h"
#include "code/client/audio/shared/AudioMix.h"
#include "code/client/audio/shared/AliceSound.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstring>
#include <vector>
using byte=unsigned char;
constexpr int PAINTBUFFER_SIZE=16,MAX_CHANNELS=96,SND_CHUNK_SIZE=8,SND_CHUNK_SIZE_FLOAT=4,MAX_RAW_SAMPLES=64;
static int aliceProfile=1;
static int Com_GameIsAlice(){return aliceProfile;}
static int Com_GameIsRTCWSP(){return 0;}
static int Com_GameIsWolfET(){return 0;}
struct cvar_t {float value;int integer;};
static cvar_t volume{1.0f,0},mute{0.0f,0};
static cvar_t *s_volume=&volume,*s_muteWhenUnfocused=&mute,*s_muteWhenMinimized=&mute;
static float S_Base_RTCWMasterGain(){return 1.0f;}
struct portable_samplepair_t {int left=0,right=0;};
static std::array<portable_samplepair_t,PAINTBUFFER_SIZE> paintbuffer;
static portable_samplepair_t s_rawsamples[MAX_RAW_SAMPLES];
static int s_rawend=0,s_paintedtime=0,snd_vol=0;
static bool gw_active=true,gw_minimized=false;
static byte hardware[256],secondary[256];static byte *dma_buffer2=secondary;
static struct {byte *buffer=hardware;int samples=64,samplebits=16,speed=22050;} dma;
struct sndBuffer {short sndChunk[SND_CHUNK_SIZE]{};sndBuffer *next=nullptr;};
struct sfx_t {sndBuffer *soundData=nullptr;int soundLength=0,soundChannels=1,soundCompressionMethod=0;};
struct channel_t {int leftvol=0,rightvol=0,startSample=0;bool doppler=false;float dopplerScale=1,oldDopplerScale=1;
 sfx_t *thesfx=nullptr;int loopBlockStart=0,loopBlockEnd=0;};
static channel_t s_channels[MAX_CHANNELS],loop_channels[MAX_CHANNELS];static int numLoopChannels=0;
static short sfxScratchBuffer[SND_CHUNK_SIZE*4],mulawToShort[256];
static int sfxScratchIndex=-1;static sfx_t *sfxScratchPointer=nullptr;
static void S_AdpcmGetSamples(sndBuffer *chunk,short *out){std::copy_n(chunk->sndChunk,SND_CHUNK_SIZE,out);}
static void decodeWavelet(sndBuffer *chunk,short *out){S_AdpcmGetSamples(chunk,out);}
static void S_PaintVoiceSamples(int,int,portable_samplepair_t *){}
static void S_PaintRTCWEntityStreams(int,int,portable_samplepair_t *){}
static void S_PaintRTCWMusic(int,int,portable_samplepair_t *){}
static std::vector<portable_samplepair_t> rendered;
static void S_TransferPaintBuffer(int end,byte *){
 rendered.insert(rendered.end(),paintbuffer.begin(),paintbuffer.begin()+(end-s_paintedtime));
}
'''

PAINTER_MAIN = r'''
// Twelve frames across two chunks; frame k holds 1000*(k+1), so what was
// painted names the frame it came from.
static sndBuffer first,second;
static sfx_t hum{&first,12,1,0};
static int Painted(int frame){return int(std::floor(double(1000*(frame+1))*(255*255)/256.0));}
static void Paint(channel_t channel,int from,int to){
 loop_channels[0]=channel;numLoopChannels=1;
 s_paintedtime=from;rendered.clear();
 S_PaintChannels(to);
 assert(int(rendered.size())==to-from&&s_paintedtime==to);
}
int main(){
 for(int k=0;k<8;++k)first.sndChunk[k]=short(1000*(k+1));
 for(int k=0;k<4;++k)second.sndChunk[k]=short(1000*(k+9));
 first.next=&second;
 channel_t loop;loop.leftvol=loop.rightvol=255;loop.thesfx=&hum;
 // With a block [4, 10): the first ten frames, then 4 to 9 again and again,
 // across both paint-buffer passes.
 loop.loopBlockStart=4;loop.loopBlockEnd=10;
 Paint(loop,0,30);
 for(int t=0;t<30;++t){
  const int frame=t<10?t:4+(t-10)%6;
  assert(frame==fntech3::audio::alice::sfx::LoopBlockOffset(unsigned(t),4,10));
  assert(rendered[t].left==Painted(frame)&&rendered[t].right==Painted(frame));
 }
 // A voice started earlier keeps its place in the block.
 loop.startSample=95;
 Paint(loop,100,120);
 for(int t=100;t<120;++t)assert(rendered[t-100].left==Painted(fntech3::audio::alice::sfx::LoopBlockOffset(unsigned(t-95),4,10)));
 // A block past the sample's end loops the whole sample.
 loop.startSample=0;loop.loopBlockEnd=13;
 Paint(loop,0,30);
 for(int t=0;t<30;++t)assert(rendered[t].left==Painted(t%12));
 // Another profile's loop never takes a block, whatever the channel holds.
 aliceProfile=0;loop.loopBlockEnd=10;
 Paint(loop,0,30);
 for(int t=0;t<30;++t)assert(rendered[t].left==Painted(t%12));
 // Without a block the loop is the engine's own, sample-long cycle.
 aliceProfile=1;loop.loopBlockStart=loop.loopBlockEnd=0;loop.startSample=3;
 Paint(loop,10,40);
 for(int t=10;t<40;++t)assert(rendered[t-10].left==Painted((t-3)%12));
 return 0;
}
'''

PLAYER_PRELUDE = r'''
#define USE_CODEC_MP3 1
#include "code/client/audio/shared/AliceMusic.h"
#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <map>
#include <vector>
using byte=unsigned char;
using namespace fntech3::audio::alice;
constexpr int kMixRate=44100;
constexpr int kMixChannels=2;
using Image=std::vector<std::uint8_t>;
struct snd_info_t {int rate=0,width=0,channels=0,samples=0,size=0,dataofs=0;};
// A decoder that decodes silence and counts what the player opens and closes.
struct snd_mp3_memory_t {int id=0;const byte *image=nullptr;};
static int opened=0,closed=0;
static std::vector<int> closedIds;
static snd_mp3_memory_t *S_MP3_OpenMemory(const byte *image,int length,snd_info_t *info){
 if(!image||length<=0)return nullptr;
 info->rate=44100;info->width=2;info->channels=2;info->samples=44100*60;
 return new snd_mp3_memory_t{opened++,image};
}
static int S_MP3_MemoryStartOffset(const snd_mp3_memory_t *){return 0;}
static int S_MP3_SeekFileOffset(snd_mp3_memory_t *,int){return 0;}
static int S_MP3_ReadMemory(snd_mp3_memory_t *,int bytes,void *pcm){std::memset(pcm,0,size_t(bytes));return bytes;}
static void S_MP3_CloseMemory(snd_mp3_memory_t *memory){++closed;closedIds.push_back(memory->id);delete memory;}
'''

PLAYER_MAIN = r'''
int main(){
 songImages.assign(3,Image(64,1));
 Song song;
 SongPlayer player;
 // Song 0 on player 0, then stopped without `restart`: its decoder waits.
 assert(player.Start(0,song,0,false)&&player.Playing(0)&&opened==1);
 player.Stop(0,0,false);
 assert(!player.Playing(0)&&closed==0);
 // Song 1 sounds on player 1 when the sound system goes away.
 assert(player.Start(1,song,1,true)&&player.Playing(1)&&opened==2);
 player.CloseLive();
 assert(!player.Playing(0)&&!player.Playing(1)&&closed==1&&closedIds.back()==1);
 // After the restart song 0 resumes on its own decoder; nothing is opened.
 assert(player.Start(0,song,0,false)&&player.Playing(0)&&opened==2&&closed==1);
 // Song 1 kept no place, so it opens again from its start offset.
 assert(player.Start(1,song,1,true)&&player.Playing(1)&&opened==3);
 // A song stopped with `restart` keeps nothing.
 player.Stop(1,1,true);
 assert(closed==2);
 // The songs going away close every decoder, the parked ones too.
 player.Stop(0,0,false);
 assert(player.Start(1,song,2,false)&&opened==4&&closed==2);
 player.Clear();
 assert(closed==4&&!player.Playing(0)&&!player.Playing(1));
 // Nothing is left parked: song 0 opens anew.
 assert(player.Start(0,song,0,false)&&opened==5);
 player.Clear();
 assert(closed==5);
 return 0;
}
'''


class AliceAudioBackendTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        audio = ROOT / 'code/client/audio'
        cls.mix = (audio / 'legacy/snd_mix.cpp').read_text(encoding='utf-8')
        cls.dma = (audio / 'legacy/snd_dma.cpp').read_text(encoding='utf-8')
        cls.alice = (audio / 'snd_alice.cpp').read_text(encoding='utf-8')
        cls.main = (audio / 'snd_main.cpp').read_text(encoding='utf-8')

    def compile_run(self, source):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++ compiler unavailable')
        target = compiler_target_args(compiler)
        (ROOT / '.tmp').mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='alice-audio-backend-', dir=ROOT / '.tmp') as temp:
            folder = Path(temp)
            cpp, exe = folder / 'probe.cpp', folder / 'probe.exe'
            cpp.write_text(source, encoding='utf-8')
            built = subprocess.run([compiler, *target, '-std=c++17', '-O1', '-I', str(ROOT), str(cpp), '-o', str(exe)],
                                   cwd=ROOT, capture_output=True, text=True, timeout=120)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            options = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
            run = subprocess.run([str(exe)], cwd=ROOT, capture_output=True, text=True, timeout=30, **options)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)

    def test_legacy_painter_repeats_the_loop_block(self):
        painters = '\n'.join(function(self.mix, name) for name in (
            'static void S_PaintChannelFrom16_scalar(', 'static void S_PaintChannelFrom16(',
            'static void S_PaintChannelFromWavelet(', 'static void S_PaintChannelFromADPCM(',
            'static void S_PaintChannelFromMuLaw(', 'extern "C" void S_PaintChannels('))
        self.compile_run(PAINTER_PRELUDE + painters + PAINTER_MAIN)

    def test_legacy_voice_carries_its_block_to_the_loop_channel(self):
        voice = between(self.dma, 'struct AliceLegacyVoice {', '\n};')
        self.assertIn('int blockStart = 0;', voice)
        self.assertIn('int blockEnd = 0;', voice)
        block = function(self.dma, 'void LoopBlock( int voice, int startFrame, int endFrame ) override {')
        self.assertIn('endFrame > v.sample->soundLength', block)
        loops = function(self.dma, 'static void S_AddAliceLoopSounds( void ) {')
        self.assertIn('channel.loopBlockStart = v.blockStart;', loops)
        self.assertIn('channel.loopBlockEnd = v.blockEnd;', loops)
        # Prepare stops the voice first, which forgets the block.
        prepare = function(self.dma, 'bool Prepare( int voice, int sfx ) override {')
        self.assertLess(prepare.index('Stop( voice );'), prepare.index('aliceVoices[voice].sample = sample;'))
        self.assertIn('aliceVoices[voice] = AliceLegacyVoice{};', function(self.dma, 'void Stop( int voice ) override {'))

    def test_song_player_shutdown_keeps_parked_places(self):
        player = between(self.alice, '/* ---- one decoded track', 'struct ConsoleLog final')
        self.compile_run(PLAYER_PRELUDE + player + PLAYER_MAIN)

    def test_shutdown_and_song_changes_use_their_own_release(self):
        shutdown = function(self.alice, 'extern "C" void S_AliceSoundShutdown( void ) {')
        self.assertIn('player.CloseLive();', shutdown)
        self.assertNotIn('player.Clear();', shutdown)
        self.assertIn('player.Clear();', function(self.alice, 'void ClearSongs( const std::vector<Song> &old ) {'))

    def test_update_asks_the_music_before_the_music_update(self):
        update = function(self.alice, 'extern "C" void S_AliceSoundUpdate( void ) {')
        request = update.index('if ( fade.active || volumeModified ) music.Reapply();')
        self.assertLess(update.index('fade.Update( cls.realtime );'), request)
        self.assertLess(update.index('volumeModified = true;'), request)
        frame = function(self.main, 'extern "C" void S_Update( int msec )')
        self.assertLess(frame.index('S_AliceSoundUpdate();'), frame.index('S_AliceMusicUpdate();'))


if __name__ == '__main__':
    unittest.main()
