"""Execute RTCW music state, backend integration and exact module arguments."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest
from compiler_support import compiler_target_args
import rtcw_music_fixture

ROOT = Path(__file__).resolve().parents[1]


STATE = r'''
#include "code/client/audio/shared/RTCWMusic.h"
#include <algorithm>
#include <cassert>
#include <climits>
#include <cstring>
#include <vector>
using namespace fntech3::audio::rtcw;
struct Output {
    bool ready=true, opened=false, fail=false;
    int rate=22050, closes=0, clears=0, opens=0;
    SampleTime now=48000;
    std::string file, current;
    bool Ready()const{return ready;}
    SampleTime Clock()const{return now;}
    void CurrentMusic(const char *name){current=name;}
    void Close(bool clear){opened=false;++closes;clears+=clear;}
    int Open(const char *name){++opens;file=name;opened=!fail;return fail?0:rate;}
};
int main() {
    Music music; Output out;
    music.Start(out,"sound/music/intro","sound/music/loop",1000);
    assert(music.Active() && out.opened && out.file=="sound/music/intro.wav");
    assert(out.current=="sound/music/loop" && !out.clears);
    assert(music.Gain(out.now)==0);
    out.now+=11025; assert(music.Gain(out.now)==0.5f);
    out.now+=11025; assert(music.Gain(out.now)==1.0f);
    music.Start(out,"stinger","ONETIMEONLY",0);
    assert(music.Loop()=="sound/music/loop" && out.current=="sound/music/loop");
    music.EndOfTrack(out);
    assert(music.Name()=="sound/music/loop.wav" && music.Loop()=="sound/music/loop");
    music.Fade(0.25f,1000,out.now);
    out.now+=22050; assert(music.Gain(out.now)==0.25f);
    music.EndOfTrack(out); assert(music.Gain(out.now)==0.25f); // Further loops preserve fade.
    music.Start(out,"queued","",-1);
    assert(music.QueueType()==-1 && music.Queued()=="queued");
    const auto previous=music.Name();
    music.Stop(); assert(!music.BeginService(out,0)); // Stops run even while muted.
    assert(!music.Active() && out.clears==1);
    assert(!music.BeginService(out,0) && !music.Active());
    assert(!music.BeginService(out,1));
    assert(music.Active() && music.Name()=="queued.wav" && music.Loop()==previous);
    assert(music.Queued()=="queued" && music.QueueType()==0);
    music.Start(out,"takeover",nullptr,-2);
    assert(out.current=="takeover");
    music.EndOfTrack(out); assert(!music.Active() && out.clears==2);
    assert(!music.BeginService(out,1) && music.Loop()=="takeover");
    music.Start(out,"fallback",nullptr,-3);
    music.EndOfTrack(out);
    assert(!music.BeginService(out,1) && !music.Active() && music.QueueType()==0);
    assert(!music.BeginService(out,1) && music.Active() && music.Loop()=="fallback");
    music.Start(out,nullptr,nullptr,-2);
    assert(music.Queued().empty() && music.Loop().empty() && music.QueueType()==0);
    music.EndOfTrack(out); assert(out.current.empty());
    assert(!music.BeginService(out,1) && !music.Active());
    const int count=out.opens;
    music.BeginService(out,1); assert(out.opens==count);
    out.fail=true; music.Start(out,"missing","loop",0);
    assert(!music.Active() && !out.opened && out.current.empty());
    out.ready=false; music.Start(out,"saved",nullptr,-2);
    assert(out.current=="saved" && music.Queued()=="saved");
    music.Start(out,"not-started","bad",0); assert(music.Name()=="missing.wav");
    out.ready=true;out.fail=false;music.BeginService(out,1);
    assert(music.Name()=="saved.wav" && music.Active());
    music.Clear(out); assert(!music.Active() && music.Queued().empty() && !out.opened);
    assert(music.Name()=="saved.wav" && music.Loop()=="saved");
    music.Start(out,"stinger","onetimeonly",0); assert(music.Loop()=="saved");
    music.Clear(out);
    music.Start(out,nullptr,nullptr,0); assert(music.Name()==".wav" && !music.Active());
    std::string longName(200,'x');
    music.Start(out,longName.c_str(),longName.c_str(),0);
    assert(music.Name()==std::string(59,'x')+".wav" && music.Loop()==std::string(59,'x'));
    assert(out.current==std::string(63,'x'));
    music.Start(out,"dir.with.dot/file",nullptr,0); assert(music.Name()=="dir.with.dot/file.wav");
    music.Start(out,"dir/file.ogg",nullptr,0); assert(music.Name()=="dir/file.ogg");
    music.Start(out,"x",nullptr,-123); music.EndOfTrack(out); music.BeginService(out,1);
    assert(music.Name()=="x.wav" && music.Loop()=="x");
    music.Fade(0,0,out.now); assert(music.Gain(out.now)==0);
    assert(!music.BeginService(out,0) && !music.Active());

    StreamFade fade;
    bool stop=false;
    fade.Start(0,22050,1000); assert(fade.Value(48000,stop)==1 && !stop);
    fade.Start(1000,22050,1000); assert(fade.end==23050);
    assert(fade.Value(12025,stop)==0.5f);
    assert(fade.Set(0,1000,12025,22050));
    assert(fade.from==0.5f && fade.Value(23050,stop)==0.25f && !stop);
    // Restarting a downward fade uses its elapsed fraction (original quirk).
    assert(fade.Set(0,1000,23050,22050)); assert(fade.from==0.5f);
    assert(fade.Value(45100,stop)==0 && stop);
    const auto before=fade;
    for(float bad:{NAN,INFINITY,-INFINITY,17.0f,-17.0f}) {
        assert(!fade.Set(bad,1000,48000,22050));
        assert(!std::memcmp(&fade,&before,sizeof(fade)));
    }
    MasterFade master; assert(master.Set(0,1000,48000,48000));
    assert(master.Value(48000)==1 && master.Value(72000)==0.5f && master.Value(96000)==0);
    assert(master.Set(0.75f,0,96000,48000) && master.Value(96000)==0.75f);
    assert(master.Set(1,1000,96000,48000) && master.Value(120000)==0.875f);
    assert(!master.Set(NAN,1000,120000,48000) && master.target==1);
    assert(master.Set(0,-1,120000,48000) && master.Value(120000)==0);
    assert(FadeEnd(LLONG_MAX,INT_MAX,INT_MAX)==LLONG_MAX);
    assert(FadeEnd(LLONG_MIN,INT_MAX,INT_MIN)==LLONG_MIN);
    SampleClock clock;
    assert(clock.Advance(0,true,48000)==0);
    assert(clock.Advance(1000000,true,48000)==48000);
    assert(clock.Advance(1500000,true,22050)==59025);
    assert(clock.Advance(1700000,false,22050)==59025); // Clock source switch rebases.
    assert(clock.Advance(1800000,false,22050)==61230);
    clock.Pause(); assert(clock.Advance(999999999,false,48000)==61230);
    assert(clock.Advance(1000999999,false,48000)==109230);
    assert(clock.Advance(-1,false,48000)==109230);
    for(int i=0;i<10000;++i) clock.Advance(LLONG_MAX,(i%2)!=0,192000);
}
'''


IMPORTS = r'''
#include "code/compat/rtcw_module_abi.h"
#include <array>
#include <cassert>
#include <climits>
#include <cstring>
#include <string>
static comGameId_t profile=COM_GAME_RTCW_SP;
int Com_GameIsRTCW(){return profile==COM_GAME_RTCW_SP || profile==COM_GAME_RTCW_MP;}
int Com_GameIsRTCWSP(){return profile==COM_GAME_RTCW_SP;}
// These probes drive RTCW only; ET's own fade is exercised by its own tests.
int Com_GameIsWolfET(){return 0;}
constexpr int ERR_DROP=1;
static void Com_Error(int,const char *){throw 1;}
struct vm_t { bool native; unsigned char bytes[256]; };
static void bounds(vm_t *vm,intptr_t address,std::size_t size) {
    if(!vm || (!vm->native && (address<0 || static_cast<std::size_t>(address)>sizeof(vm->bytes) || size>sizeof(vm->bytes)-address))) throw 1;
}
#define VM_CHECKBOUNDS(vm,address,size) bounds(vm,address,size)
static void *VM_ExplicitArgPtr(vm_t *vm,intptr_t address) {return vm->native?reinterpret_cast<void *>(address):vm->bytes+address;}
static int operation,fadeTime,streamNumber;static float fadeVolume; static std::string introName,loopName;
static void S_StartRTCWBackgroundTrack(const char *intro,const char *loop,int time){operation=1;introName=intro;loopName=loop;fadeTime=time;}
static void S_StartBackgroundTrack(const char *intro,const char *loop){operation=2;introName=intro;loopName=loop;}
static void S_StopBackgroundTrack(){operation=3;}
static void S_FadeRTCWStreamingSound(float volume,int time,int stream){operation=4;fadeVolume=volume;fadeTime=time;streamNumber=stream;}
static void S_FadeRTCWAllSounds(float volume,int time){operation=5;fadeVolume=volume;fadeTime=time;}
static void S_ClearLoopingSoundsFrame(){operation=6;}
static void S_ClearRTCWLoopingSounds(int mode){operation=7;fadeTime=mode;}
static void S_UpdateEntityPosition(int,const float *){assert(false);}
static void S_Respatialize(int,const float *,float (*)[3],int){assert(false);}
static void S_AddRTCWLoopingSound(const float *,const float *,int,int,int){assert(false);}
static void S_StartRTCWSound(const float *,int,int,int,int){assert(false);}
static void S_StartRTCWLocalSound(int,int){assert(false);}
static void S_StartRTCWEntityStream(const char *,const char *,int,int,int){assert(false);}
static void S_StopRTCWEntityStream(int){assert(false);}
#include "code/client/client_rtcw_audio_compat.hpp"
using fntech3::rtcw::module::Call;
int main() {
    for(bool native:{false,true}) for(auto id:{COM_GAME_RTCW_SP,COM_GAME_RTCW_MP}) {
        vm_t vm{native,{}};profile=id;std::memcpy(vm.bytes+13,"intro",6);
        std::memcpy(vm.bytes+101,"loop",5);
        intptr_t intro=native?reinterpret_cast<intptr_t>(vm.bytes+13):13;
        intptr_t loop=native?reinterpret_cast<intptr_t>(vm.bytes+101):101;
        for(auto call:{Call::UI_S_STARTBACKGROUNDTRACK,Call::CG_S_STARTBACKGROUNDTRACK}) {
            if(id==COM_GAME_RTCW_SP) {intptr_t args[]{0,intro,loop,-2};assert(CL_RTCWAudioCall(&vm,call,args));assert(operation==1 && fadeTime==-2);}
            else {intptr_t args[]{0,intro,loop};assert(CL_RTCWAudioCall(&vm,call,args));assert(operation==2);}
            assert(introName=="intro" && loopName=="loop");
        }
        intptr_t stop[]{0};assert(CL_RTCWAudioCall(&vm,Call::UI_S_STOPBACKGROUNDTRACK,stop) && operation==3);
        assert(CL_RTCWAudioCall(&vm,Call::CG_S_STOPBACKGROUNDTRACK,stop));
        // MP clear and both profiles' stop imports consume no arguments.
        operation=0;
        if(id==COM_GAME_RTCW_MP) {
            assert(CL_RTCWAudioCall(&vm,Call::CG_S_CLEARLOOPINGSOUNDS,stop) && operation==6);
        } else for(int mode:{INT_MIN,-1,0,1,2,3,INT_MAX}) {
            intptr_t clear[]{0,mode};
            assert(CL_RTCWAudioCall(&vm,Call::CG_S_CLEARLOOPINGSOUNDS,clear));
            assert(operation==7 && fadeTime==mode);
        }
        operation=0;
        assert(CL_RTCWAudioCall(&vm,Call::CG_S_STOPLOOPINGSOUND,stop));
        assert(operation==0);
        if(id==COM_GAME_RTCW_SP) {
            for(auto call:{Call::UI_S_FADESTREAMINGSOUND,Call::CG_S_FADESTREAMINGSOUND}) {
                intptr_t fade[]{0,0x3f000000,1000,0};assert(CL_RTCWAudioCall(&vm,call,fade));
                assert(operation==4 && fadeVolume==0.5f && fadeTime==1000 && streamNumber==0);
            }
            for(auto call:{Call::UI_S_FADEALLSOUNDS,Call::CG_S_FADEALLSOUNDS}) {
                intptr_t fade[]{0,0x3e800000,0};assert(CL_RTCWAudioCall(&vm,call,fade));
                assert(operation==5 && fadeVolume==0.25f && fadeTime==0);
            }
            intptr_t null[]{0,0,0,0};assert(CL_RTCWAudioCall(&vm,Call::UI_S_STARTBACKGROUNDTRACK,null));
            assert(introName.empty() && loopName.empty());
        } else {intptr_t minimal[]{0};assert(!CL_RTCWAudioCall(&vm,Call::UI_S_FADEALLSOUNDS,minimal));}
        if(!native) {
            vm.bytes[255]='x'; intptr_t bad[]{0,255,loop,0}; operation=0;
            try{CL_RTCWAudioCall(&vm,Call::UI_S_STARTBACKGROUNDTRACK,bad);assert(false);}catch(int){}
            assert(!operation);
        }
        if(sizeof(intptr_t)>4 && id==COM_GAME_RTCW_SP) {
            intptr_t clear[]{0,static_cast<intptr_t>(INT64_C(1)<<40)};operation=0;
            try{CL_RTCWAudioCall(&vm,Call::CG_S_CLEARLOOPINGSOUNDS,clear);assert(false);}catch(int){}
            assert(!operation);
            intptr_t bad[]{0,intro,loop,static_cast<intptr_t>(INT64_C(1)<<40)};operation=0;
            try{CL_RTCWAudioCall(&vm,Call::UI_S_STARTBACKGROUNDTRACK,bad);assert(false);}catch(int){}
            assert(!operation);
        }
    }
    profile=COM_GAME_QUAKE3;intptr_t none[]{0};assert(!CL_RTCWAudioCall(nullptr,Call::UI_S_STARTBACKGROUNDTRACK,none));
    profile=COM_GAME_QUAKELIVE;assert(!CL_RTCWAudioCall(nullptr,Call::UI_S_STARTBACKGROUNDTRACK,none));
}
'''


class RTCWMusicTests(unittest.TestCase):
    def compile_run(self, source, definitions=()):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler: self.skipTest('C++ compiler unavailable')
        flags = compiler_target_args(compiler)
        if os.name == 'nt' and not flags: flags += ['-DFNTECH3_Q3_ONLY']
        with tempfile.TemporaryDirectory(prefix='rtcw-music-', dir=ROOT / '.tmp') as temporary:
            path = Path(temporary) / 'probe.cpp'; path.write_text(source)
            executable = Path(temporary) / ('probe.exe' if os.name == 'nt' else 'probe')
            build = subprocess.run([compiler, '-std=c++17', *flags, *definitions, '-I', str(ROOT), str(path), '-o', str(executable)],
                                   cwd=ROOT, capture_output=True, text=True, timeout=60)
            self.assertEqual(build.returncode, 0, build.stdout + build.stderr)
            run = subprocess.run([str(executable)], cwd=ROOT, capture_output=True, text=True, timeout=20)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)

    def test_music_lifecycle_queue_fades_and_clock(self):
        self.compile_run(STATE)

    def test_exact_sp_mp_native_and_qvm_arguments(self):
        self.compile_run(IMPORTS)

    def test_legacy_audio_build_keeps_ieee_guards(self):
        makefile = (ROOT / 'Makefile').read_text()
        self.assertIn('AUDIO_FP_CFLAGS = $(if $(filter snd_main.cpp snd_dma.cpp snd_mix.cpp AudioSystem.cpp,$(notdir $<)),-fno-fast-math)', makefile)
        self.assertIn('$(CXX) $(CXXFLAGS) $(AUDIO_FP_CFLAGS)', makefile)
        self.compile_run(STATE, ('-O2', '-ffast-math', '-fno-fast-math'))

    def test_actual_backend_music_entrypoints(self):
        for backend in (rtcw_music_fixture.openal, rtcw_music_fixture.legacy):
            with self.subTest(backend=backend.__name__): self.compile_run(backend())

    def test_audio_facade_validation_and_disabled_backend(self):
        self.compile_run(rtcw_music_fixture.facade())

    def test_legacy_master_mix_bounds(self):
        self.compile_run(rtcw_music_fixture.legacy_mix())

    def test_openal_effect_and_stream_gain_routing(self):
        self.compile_run(rtcw_music_fixture.openal_gains())


if __name__ == '__main__': unittest.main()
