#!/usr/bin/env python3
"""Compare entity stream allocation, replacement, SP entity stops and fades.

Unchanged original GPLv3 functions are compiled only in disposable fixtures.
Codec reads and pending-stop service are controlled; PCM, original decoding,
EOF loop transitions, hardware timing and live speech are outside this oracle.
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
from rtcw_music_fixture import function
from rtcw_entity_stream_tests import POLICY

REFERENCE = r'''
namespace reference {
constexpr int MAX_STREAMING_SOUNDS=kSP?12:24,MAX_QPATH=64,CHAN_AUTO=0,CA_ACTIVE=8,WAV_FORMAT_PCM=1;
constexpr bool qtrue=true,qfalse=false;
using fileHandle_t=int;
struct streamingSound_t{
 char name[64]{},loop[64]{};int file=0,entnum=0,channel=0,attenuation=0,kill=0,samples=0;
 struct{int format=0,channels=0,rate=0,width=0,samples=0;}info;
 float fadeStartVol=0,fadeTargetVol=0;int fadeStart=0,fadeEnd=0;
};
static streamingSound_t streamingSounds[24];static int s_rawend[24];
static int crit=1,numStreamingSounds=1,s_soundtime=48000;
static bool s_soundStarted=true,s_soundMuted=false;
static struct{bool s_soundStarted=true,s_soundMute=false;}snd;
static struct{int state=CA_ACTIVE;}cls;
static struct{int integer=0;}muted,debug;static auto *s_mute=&muted,*s_debugMusic=&debug;
static std::vector<std::string> files(1);static int shortIndex=0,longIndex=0,waveRate=22050;
static void Com_Printf(const char *,...){}static void Com_DPrintf(const char *,...){}
static void Sys_EnterCriticalSection(int){}static void Sys_LeaveCriticalSection(int){}
static void Sys_BeginStreamedFile(int,int){}static void Sys_EndStreamedFile(int){}static void FS_FCloseFile(int){}
static void FS_FOpenFileRead(const char *name,int *file,bool){
 *file=0;shortIndex=longIndex=0;if(std::string(name).find("missing")!=std::string::npos)return;
 files.push_back(name);*file=int(files.size()-1);
}
static void FS_Read(void *,int,int){}
static int S_FindWavChunk(int,const char *name){return std::string(name)=="data"?16000:1;}
static int FGetLittleShort(int){const int values[]{1,2,4,16};return values[shortIndex++];}
static int FGetLittleLong(int){return longIndex++==0?waveRate:0;}
static void Q_strncpyz(char *out,const char *in,int bytes){std::snprintf(out,bytes,"%s",in);}
static void COM_DefaultExtension(char *out,int size,const char *extension){
 const std::string name=out;const auto dot=name.find_last_of('.'),slash=name.find_last_of('/');
 if(dot==std::string::npos||(slash!=std::string::npos&&dot<slash))std::snprintf(out,size,"%s%s",name.c_str(),extension);
}
'''

CHECKS = r'''
}
static unsigned states=0,starts=0,stops=0,fadeValues=0;
struct Host:Output{
 std::array<int,24> buffered{};
 int Open(int i,const char *name){fail=std::string(name).find("missing")!=std::string::npos;return Output::Open(i,name);}
 void Discard(int i){buffered[i]=0;Output::Discard(i);}
};
static void reset(){
 for(int i=0;i<24;++i){reference::streamingSounds[i]={};reference::s_rawend[i]=100;}
 reference::numStreamingSounds=1;reference::files.resize(1);reference::s_soundtime=48000;
}
static void compare(const EntityStreams &pool,const Host &host){
 for(int i=1;i<pool.Limit();++i){
  const auto &a=pool[i];const auto &b=reference::streamingSounds[i];
  assert(a.active==bool(b.file)&&a.loop==b.loop&&host.buffered[i]==reference::s_rawend[i]);
  if(a.active){
   assert(a.entity==b.entnum&&a.channel==b.channel&&a.attenuation==b.attenuation&&a.rate==b.info.rate&&a.stop==bool(b.kill));
   assert(a.name==reference::files[b.file]);
   if(kSP){assert(a.name==b.name&&a.fade.start==b.fadeStart&&a.fade.end==b.fadeEnd&&a.fade.from==b.fadeStartVol&&a.fade.target==b.fadeTargetVol);}
  }
 }++states;
}
static void start(EntityStreams &pool,Host &host,const char *intro,const char *loop,int entity,int channel,int attenuation){
 pool.Start(host,intro,loop,entity,channel,attenuation);
 reference::S_StartStreamingSound(intro,loop,entity,channel,attenuation);++starts;compare(pool,host);
}
static void service(EntityStreams &pool,Host &host){
 for(int i=1;i<pool.Limit();++i){
  pool.BeginService(host,i,1);auto &s=reference::streamingSounds[i];
  if(s.file&&s.kill){s.file=0;s.kill=0;--reference::numStreamingSounds;}
 }compare(pool,host);
}
int main(){
 std::string longName(180,'x');const char *names[]{nullptr,"","speech","voice.wav","dir.with.dot/speech","missing",longName.c_str()};
 for(int entity:{INT_MIN,-1,0,1,127,1023})for(int channel:{INT_MIN,-1,0,2,3,7,INT_MAX})
 for(auto intro:names)for(auto loop:names){
  reset();EntityStreams pool(kSP);Host host;host.buffered.fill(100);
  start(pool,host,intro,loop,entity,channel,1);
  start(pool,host,"second",nullptr,entity,channel,-1);
  start(pool,host,intro,loop,entity,0,0);
  start(pool,host,nullptr,nullptr,entity,channel,0);
  for(int i=1;i<=pool.Limit();++i)start(pool,host,"fill",nullptr,500+i,3,0);
  start(pool,host,"replacement","repeat",501,3,1);
  if(kSP){
#if RTCW_REFERENCE_SP
   for(int e:{-1,0,entity,501,1023}){pool.StopEntity(host,e);reference::S_StopEntStreamingSound(e);++stops;compare(pool,host);}
#endif
  }
  service(pool,host);
 }
#if RTCW_REFERENCE_SP
 for(int rate:{8000,11025,22050,44100,48000,96000,192000})for(int clock:{0,100,48000,200000000})
 for(int duration:{-2,0,1,333,1000})for(float target:{-1.f,0.f,.25f,1.f,2.f,16.f}){
  reset();EntityStreams pool(true);Host host;host.rate=reference::waveRate=rate;host.buffered.fill(100);
  start(pool,host,"voice",nullptr,1,3,1);reference::s_soundtime=clock;
  pool.Fade(1,target,duration,clock);reference::S_FadeStreamingSound(target,duration,1);compare(pool,host);
  for(int delta:{0,1,100,11025,48000}){
   reference::s_soundtime=clock+delta;const float actual=pool.Gain(1,clock+delta),expected=reference::S_GetStreamingFade(reference::streamingSounds+1);
   assert(!std::memcmp(&actual,&expected,sizeof(float)));++fadeValues;compare(pool,host);
  }
 }
#endif
 std::printf("{\"starts\":%u,\"states\":%u,\"entityStops\":%u,\"fadeValues\":%u}\n",starts,states,stops,fadeValues);
}
'''


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sp-source', type=Path, required=True)
    parser.add_argument('--mp-source', type=Path, required=True)
    parser.add_argument('--report', type=Path, required=True)
    parser.add_argument('--compiler', default=shutil.which('clang++'))
    parser.add_argument('--target')
    parser.add_argument('--sanitize', action='store_true')
    args = parser.parse_args()
    report_path = args.report.resolve(); report_path.relative_to(ROOT)
    report_path.parent.mkdir(parents=True, exist_ok=True)
    report = dict(scope=__doc__, utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),
                  sanitized=args.sanitize, implementationSha256={name: sha(ROOT / name) for name in (
                      'code/client/audio/shared/RTCWEntityStreams.h', 'code/client/audio/shared/RTCWMusic.h',
                      'code/client/audio/shared/RTCWLoops.h', 'tests/rtcw_entity_stream_tests.py',
                      'scripts/probe_rtcw_entity_streams.py')}, results=[])
    target = ['--target=' + args.target] if args.target else ([] if os.name == 'nt' else ['-m32'])
    for mode, root in [('sp', args.sp_source), ('mp', args.mp_source)]:
        source_path = root / 'src/client/snd_dma.c'; source = source_path.read_text()
        notice = source[:source.index('*/') + 2]; assert 'either version 3' in notice
        sp = mode == 'sp'
        fixture = notice + '\n#include <cstdio>\n' + POLICY[:POLICY.index('int main()')]
        fixture += f'constexpr bool kSP={str(sp).lower()};\n#define RTCW_REFERENCE_SP {int(sp)}\n' + REFERENCE
        for signature in ('void S_StartStreamingSound(', 'void S_StopStreamingSound('):
            fixture += function(source, signature) + '\n'
        if sp:
            for signature in ('void S_StopEntStreamingSound(', 'void S_FadeStreamingSound(', 'float S_GetStreamingFade('):
                fixture += function(source, signature) + '\n'
        fixture += CHECKS
        row = dict(reference='original-' + mode, source=str(source_path), sourceSha256=sha(source_path), passed=False)
        with tempfile.TemporaryDirectory(prefix='entity-stream-reference-', dir=report_path.parent) as temporary:
            folder = Path(temporary); cpp = folder / 'probe.cpp'; cpp.write_text(fixture)
            executable = folder / ('probe.exe' if os.name == 'nt' else 'probe')
            command = [args.compiler, *target, '-std=c++17', '-O1', '-fno-fast-math', '-D_CRT_SECURE_NO_WARNINGS',
                       '-I', str(ROOT), str(cpp), '-o', str(executable)]
            if os.name != 'nt': command += ['-msse2', '-mfpmath=sse']
            if args.sanitize: command += ['-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-g']
            row['compileCommand'] = command
            built = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, timeout=90)
            row.update(buildExitCode=built.returncode, buildOutput=built.stdout + built.stderr)
            if built.returncode == 0:
                environment = os.environ.copy()
                if args.sanitize: environment.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1', UBSAN_OPTIONS='halt_on_error=1')
                options = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
                run = subprocess.run([str(executable)], cwd=ROOT, capture_output=True, text=True, timeout=90, env=environment, **options)
                row.update(exitCode=run.returncode, diagnostics=run.stderr,
                           observations=json.loads(run.stdout) if run.returncode == 0 else None, passed=run.returncode == 0)
        report['results'].append(row)
        print(mode, 'PASS' if row['passed'] else 'FAIL', row.get('observations'))
        if not row['passed']: print(row.get('diagnostics', row['buildOutput'])[:5000])
    report_path.write_text(json.dumps(report, indent=2) + '\n')
    return 0 if all(row['passed'] for row in report['results']) else 1


if __name__ == '__main__':
    raise SystemExit(main())
