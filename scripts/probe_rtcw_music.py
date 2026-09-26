#!/usr/bin/env python3
"""Compare SP music state/fades with unchanged original GPLv3 functions.

Reference functions exist only in disposable licensed fixtures. File decoding,
device clocks and cvars are controlled; this does not establish PCM, buffering,
or original-engine playback parity. No installed content is needed or copied.
"""
from pathlib import Path
import argparse
import datetime
import hashlib
import json
import os
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tests'))
from rtcw_music_tests import STATE
from rtcw_music_fixture import function


REFERENCE = r'''
namespace reference {
enum {MAX_QPATH=64,WAV_FORMAT_PCM=1,QUEUED_PLAY_ONCE=-1,QUEUED_PLAY_ONCE_SILENT=-3};
constexpr bool qtrue=true;
using fileHandle_t=int;
struct streamingSound_t {
    char name[64]{},loop[64]{};
    struct {int format,channels,rate,width,samples;}info{};
    int channel=0,entnum=0,attenuation=0,file=0,kill=0,samples=0,looped=0;
    int fadeStart=0,fadeEnd=0;float fadeStartVol=0,fadeTargetVol=0;
};
static streamingSound_t streamingSounds[1];
static int numStreamingSounds=0,s_soundtime=0,crit=1;
static struct {int speed=48000;}dma;
static struct {
    char nextMusicTrack[64]{};int nextMusicTrackType=0;bool s_soundStarted=true;
    float volStart=1,volCurrent=1,volTarget=1;int volTime1=0,volTime2=0;
}snd;
static struct{int integer=0;}debugMusic;
static auto *s_debugMusic=&debugMusic;
static int waveRate=22050,shortIndex=0,longIndex=0;
static std::string current;
static void Cvar_Set(const char *key,const char *value){assert(std::string(key)=="s_currentMusic");current=value;}
static void Com_Printf(const char *,...){}
static void Com_DPrintf(const char *,...){}
static void Sys_EnterCriticalSection(int){}
static void Sys_LeaveCriticalSection(int){}
static void Sys_EndStreamedFile(int){}
static void Sys_BeginStreamedFile(int,int){}
static void FS_FCloseFile(int){}
static void FS_FOpenFileRead(const char *name,int *file,bool){*file=std::string(name).find("missing")==std::string::npos;shortIndex=longIndex=0;}
static void FS_Read(void *,int,int){}
static int S_FindWavChunk(int,const char *name){return std::string(name)=="data"?4096:1;}
static int FGetLittleShort(int){const int values[]{1,2,4,16};return values[shortIndex++];}
static int FGetLittleLong(int){return longIndex++==0?waveRate:0;}
static void Q_strncpyz(char *out,const char *in,int size){std::snprintf(out,size,"%s",in);}
static int Q_stricmp(const char *a,const char *b){
    assert(a&&b);while(*a && *b) {int x=*a++,y=*b++;if(x>='A'&&x<='Z')x+=32;if(y>='A'&&y<='Z')y+=32;if(x!=y)return x-y;}return *a-*b;
}
static void COM_DefaultExtension(char *name,int size,const char *extension){
    const std::string path=name;auto dot=path.find_last_of('.'),slash=path.find_last_of('/');
    if(dot==std::string::npos || (slash!=std::string::npos && dot<slash))std::snprintf(name,size,"%s%s",path.c_str(),extension);
}
static char *va(const char *,const char *name){static std::string copy;copy=name;return copy.data();}
'''


CHECKS = r'''
static void ResetReference(int rate,int now,bool ready=true) {
    reference::streamingSounds[0]={};reference::snd={};reference::numStreamingSounds=0;
    reference::s_soundtime=now;reference::waveRate=rate;reference::current.clear();reference::snd.s_soundStarted=ready;
}
static void Compare(const Music &music,const Output &out) {
    const auto &ss=reference::streamingSounds[0];
    assert(music.Name()==ss.name && music.Loop()==ss.loop && out.current==reference::current);
    assert(music.Queued()==reference::snd.nextMusicTrack && music.QueueType()==reference::snd.nextMusicTrackType);
    assert(music.Active()==bool(ss.file));
    if(music.Active()) {
        const auto &fade=music.FadeState();
        if(fade.start!=ss.fadeStart || fade.end!=ss.fadeEnd || fade.from!=ss.fadeStartVol || fade.target!=ss.fadeTargetVol) {
            std::fprintf(stderr,"stream fade rate=%d now=%d: implementation=%lld/%lld/%g/%g reference=%d/%d/%g/%g\n",reference::waveRate,reference::s_soundtime,
                (long long)fade.start,(long long)fade.end,fade.from,fade.target,ss.fadeStart,ss.fadeEnd,ss.fadeStartVol,ss.fadeTargetVol);
            std::exit(1);
        }
    }
}
int main() {
    unsigned starts=0,fades=0,queues=0,masterValues=0;
    for(int rate:{11025,22050,44100,48000,96000})
    for(int now:{0,1,48000,4194304}) for(int duration:{0,1,999,1000,10000})
    for(const char *intro:{"","intro","a.b/intro","intro.wav","missing"})
    for(const char *loop:{"","loop","ONETIMEONLY"}) for(bool ready:{false,true}) {
        Music music;Output out;out.rate=rate;out.now=now;out.ready=ready;out.fail=std::string(intro)=="missing";
        ResetReference(rate,now,ready);
        music.Start(out,intro,loop,duration);reference::S_StartBackgroundTrack(intro,loop,duration);
        Compare(music,out);++starts;
    }
    for(int rate:{11025,22050,44100,48000,96000}) for(int at:{0,100,500,1000,2000})
    for(float target:{-1.f,0.f,0.25f,0.5f,1.f,2.f}) for(int duration:{0,1,500,1000}) {
        Music music;Output out;out.rate=rate;out.now=48000;ResetReference(rate,48000);
        music.Start(out,"intro","loop",1000);reference::S_StartBackgroundTrack("intro","loop",1000);
        out.now+=at*rate/1000;reference::s_soundtime=int(out.now);
        music.Fade(target,duration,out.now);reference::S_FadeStreamingSound(target,duration,0);Compare(music,out);
        for(int delta:{0,1,rate/4,rate/2,rate}) {
            reference::s_soundtime=int(out.now)+delta;
            assert(music.Gain(out.now+delta)==reference::S_GetStreamingFade(&reference::streamingSounds[0]));++fades;
        }
    }
    for(int type:{-1,-2,-3,-4,INT_MIN}) for(const char *next:{"","queued"}) for(bool ready:{false,true}) {
        Music music;Output out;ResetReference(22050,48000);out.ready=true;
        music.Start(out,"intro","loop",0);reference::S_StartBackgroundTrack("intro","loop",0);
        out.ready=ready;reference::snd.s_soundStarted=ready;
        music.Start(out,next,"",type);reference::S_StartBackgroundTrack(next,"",type);Compare(music,out);++queues;
        if(ready && next[0]) {
            music.Stop();music.BeginService(out,1);reference::streamingSounds[0].file=0;
            music.BeginService(out,1);reference::S_CheckForQueuedMusic();Compare(music,out);++queues;
            if(!music.Active()) {music.BeginService(out,1);reference::S_CheckForQueuedMusic();Compare(music,out);++queues;}
        }
    }
    for(int rate:{11025,22050,44100,48000,96000}) for(int now:{0,1,48000,4194304})
    for(float initial:{0.f,0.25f,0.5f,1.f}) for(float target:{0.f,0.125f,0.75f,1.f,2.f})
    for(int duration:{-1,0,1,500,1000}) {
        MasterFade fade;fade.current=initial;ResetReference(rate,now);reference::dma.speed=rate;reference::snd.volCurrent=initial;
        fade.Set(target,duration,now,rate);reference::S_FadeAllSounds(target,duration);
        if(fade.start!=reference::snd.volTime1 || fade.end!=reference::snd.volTime2) {
            std::fprintf(stderr,"master endpoint rate=%d now=%d duration=%d: implementation=%lld reference=%d\n",rate,now,duration,(long long)fade.end,reference::snd.volTime2);
            return 1;
        }
        assert(fade.from==reference::snd.volStart && fade.target==reference::snd.volTarget && fade.current==reference::snd.volCurrent);
        ++masterValues;
    }
    std::printf("{\"starts\":%u,\"fadeValues\":%u,\"queueTransitions\":%u,\"masterRequests\":%u}\n",starts,fades,queues,masterValues);
}
'''


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sp-source', type=Path, required=True)
    parser.add_argument('--report', type=Path, required=True)
    parser.add_argument('--compiler', default=shutil.which('clang++'))
    parser.add_argument('--target')
    parser.add_argument('--sanitize', action='store_true')
    args = parser.parse_args()
    report_path = args.report.resolve(); report_path.relative_to(ROOT)
    report_path.parent.mkdir(parents=True, exist_ok=True)
    source_path = args.sp_source / 'src/client/snd_dma.c'
    source = source_path.read_text(encoding='utf-8')
    notice = source[:source.index('*/') + 2]
    assert 'either version 3' in notice
    fixture = notice + '\n#include <cstdio>\n' + STATE[:STATE.index('int main()')] + REFERENCE
    for signature in ('void S_StartBackgroundTrack(', 'void S_FadeAllSounds(', 'void S_FadeStreamingSound(',
                      'float S_GetStreamingFade(', 'int S_CheckForQueuedMusic('):
        fixture += function(source, signature) + '\n'
    fixture += '}\n' + CHECKS
    target = ['--target=' + args.target] if args.target else ([] if os.name == 'nt' else ['-m32'])
    triple = subprocess.check_output([args.compiler, *target, '-dumpmachine'], cwd=ROOT, text=True).strip()
    report = dict(scope=__doc__, dateUtc=datetime.datetime.now(datetime.timezone.utc).isoformat(),
                  compiler=args.compiler, target=triple, sanitized=args.sanitize,
                  source=str(source_path), sourceSha256=sha(source_path),
                  implementationSha256=sha(ROOT / 'code/client/audio/shared/RTCWMusic.h'),
                  floatingPoint='binary32 source arithmetic; Linux x86 fixture uses SSE2 to exclude x87 excess precision', passed=False)
    with tempfile.TemporaryDirectory(prefix='music-reference-', dir=report_path.parent) as temporary:
        folder = Path(temporary); cpp = folder / 'probe.cpp'; cpp.write_text(fixture, encoding='utf-8')
        executable = folder / ('probe.exe' if os.name == 'nt' else 'probe')
        command = [args.compiler, *target, '-std=c++17', '-O1', '-D_CRT_SECURE_NO_WARNINGS', '-I', str(ROOT), str(cpp), '-o', str(executable)]
        if os.name != 'nt': command += ['-msse2', '-mfpmath=sse']
        if args.sanitize: command += ['-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-g']
        build = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, timeout=90)
        report.update(buildExitCode=build.returncode, buildOutput=build.stdout + build.stderr)
        if build.returncode == 0:
            environment = os.environ.copy()
            if args.sanitize: environment.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1', UBSAN_OPTIONS='halt_on_error=1')
            options = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
            run = subprocess.run([str(executable)], cwd=ROOT, capture_output=True, text=True, timeout=60, env=environment, **options)
            report.update(exitCode=run.returncode, diagnostics=run.stderr, observations=json.loads(run.stdout) if run.returncode == 0 else None)
            report['passed'] = run.returncode == 0
    report_path.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print('original sp music', 'PASS' if report['passed'] else 'FAIL', report.get('observations'))
    if not report['passed']: print(report.get('diagnostics', report['buildOutput'])[:4000])
    return 0 if report['passed'] else 1


if __name__ == '__main__': raise SystemExit(main())
