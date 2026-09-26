#!/usr/bin/env python3
"""Compare defined RTCW one-shot allocation behavior with unchanged original functions.

The original GPLv3 implementation is compiled only in a disposable fixture with
its notices. This proves channel policy, not PCM output or streaming behavior.
Reference cases keep at least two free nodes to avoid its broken exhausted-list
and out-of-bounds fallback paths. Engine-only tests cover safe exhaustion.
"""
from pathlib import Path
import argparse
import datetime
import hashlib
import json
import os
import re
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]

HOST = r'''
#include "code/client/audio/shared/RTCWOneShots.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <cstdint>
namespace reference {
using vec3_t=float[3];using sfxHandle_t=int;using qboolean=int;
constexpr int qfalse=0,qtrue=1,MAX_CHANNELS=96,MAX_GENTITIES=1024,MAX_STREAMING_SOUNDS=12,ERR_DROP=1;
constexpr int CHAN_AUTO=0,CHAN_WEAPON=2,CHAN_ANNOUNCER=7,SOUND_RANGE_DEFAULT=1250,START_SAMPLE_IMMEDIATE=-1;
constexpr int SND_OKTOCUT=1,SND_REQUESTCUT=2,SND_CUTOFF=4,SND_CUTOFF_ALL=8,SND_NOCUT=16;
#define S_COLOR_YELLOW ""
#define Com_Memset std::memset
static int now=100,listener_number=0,s_paintedtime=0;
static int Sys_Milliseconds(){return now;}
static void Com_Printf(const char *,...){}static void Com_DPrintf(const char *,...){}
static void Com_Error(int,const char *,...){throw 1;}
static void VectorCopy(const float *in,float *out){std::memcpy(out,in,12);}
static void S_SpatializeOrigin(const float *,int,int *left,int *right,float){*left=63;*right=63;}
struct sfx_t {int lastTimeUsed=0;const char *soundName="synthetic";};static sfx_t s_knownSfx[64];
struct channel_t {int allocTime,startSample,entnum,entchannel,flags,master_vol,leftvol,rightvol;float origin[3];bool fixed_origin,doppler,threadReady;sfx_t *thesfx;};
static channel_t s_channels[MAX_CHANNELS];
static struct {int integer=0;} show;static auto *s_show=&show;
static struct {int file=0,entnum=0,channel=0,kill=0;} streamingSounds[MAX_STREAMING_SOUNDS];
'''

COMPARE = r'''
}
static uint32_t seed=0x12345678;
static unsigned randomWord(){seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;return seed;}
static int comparisons=0,starts=0,releases=0;
static void compare(const fntech3::audio::rtcw::OneShots &slots){
 for(int i=0;i<96;++i){
  const auto &actual=slots[i];const auto &expected=reference::s_channels[i];
  if(actual.active!=(expected.thesfx!=nullptr)|| (actual.active &&
   (actual.entity!=expected.entnum||actual.channel!=expected.entchannel||actual.flags!=expected.flags||
    actual.handle!=expected.thesfx-reference::s_knownSfx||actual.allocated!=expected.allocTime))){
   std::fprintf(stderr,"channel mismatch at event=%d slot=%d actual=%d/%d/%d/%d/%d/%d reference=%d/%d/%d/%d/%d/%d\n",starts,i,
    int(actual.active),actual.entity,actual.channel,actual.handle,actual.flags,actual.allocated,
    int(expected.thesfx!=nullptr),expected.entnum,expected.entchannel,expected.thesfx?int(expected.thesfx-reference::s_knownSfx):-1,expected.flags,expected.allocTime);std::abort();
  }
 }++comparisons;
}
int main(){
 using namespace fntech3::audio::rtcw;
 auto start=[](OneShots &slots,int entity,int channel,int handle,int flags){
  float point[3]={};
  slots.Start(kSP,128,reference::listener_number,reference::now,{entity,channel,handle,flags},[](int){});
  reference::S_ThreadStartSoundEx(point,entity,channel,handle,flags);++starts;compare(slots);
 };
 for(int entity:{-1,0,127,128,200,1023})for(int channel=0;channel<8;++channel)
 for(int oldFlags=0;oldFlags<32;++oldFlags)for(int flags=0;flags<32;++flags)for(bool same:{false,true}){
  OneShots slots;reference::S_ChannelSetup();reference::now=100;
  start(slots,entity,channel,1,oldFlags);++reference::now;
  start(slots,entity,channel,same?1:2,flags);
 }
 for(int scene=0;scene<5000;++scene){
  OneShots slots;reference::S_ChannelSetup();reference::now=100;reference::listener_number=int(randomWord()%128);
  for(int step=0;step<60;++step){
   reference::now+=int(randomWord()%4);
   if(randomWord()%4==0){
    const int index=int(randomWord()%96);if(slots[index].active){slots.Release(index);reference::S_ChannelFree(reference::s_channels+index);++releases;compare(slots);}
   } else {
    const int entity=int(randomWord()%256),channel=int(randomWord()%8),handle=int(randomWord()%64),flags=int(randomWord()%64);
    start(slots,entity,channel,handle,flags);
   }
  }
 }
 std::printf("{\"starts\":%d,\"releases\":%d,\"stateComparisons\":%d}\n",starts,releases,comparisons);
}
'''


def function(text, name):
    match = re.search(r'(?:void|channel_t\*)\s+' + name + r'\([^;{}]*\)\s*\{', text)
    if not match:
        raise ValueError(name)
    return text[match.start():text.index('\n}', match.start()) + 2]


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    modes = parser.add_mutually_exclusive_group(required=True)
    modes.add_argument('--sp-source', type=Path)
    modes.add_argument('--mp-source', type=Path)
    parser.add_argument('--target')
    parser.add_argument('--sanitize', action='store_true')
    parser.add_argument('--compiler', default=shutil.which('clang++'))
    parser.add_argument('--report', type=Path, required=True)
    args = parser.parse_args()
    report_path = args.report.resolve(); report_path.relative_to(ROOT)
    report_path.parent.mkdir(parents=True, exist_ok=True)
    root = (args.sp_source or args.mp_source) / 'src'
    sound = root / 'client/snd_dma.c'; original = sound.read_text()
    source = original[:original.index('*/')+2] + '\n#include <cstdlib>\n' + HOST
    if args.sp_source:
        source += '''
constexpr int MAX_CLIENTS=128;
static struct {int s_soundStarted=1,s_soundMute=0,s_numSfx=64;channel_t *freelist=nullptr,*endflist=nullptr;float entityPositions[1024][3]{};}snd;
'''
    else:
        source += '''
constexpr int MAX_CLIENTS=64;
static int s_soundStarted=1,s_soundMuted=0,s_numSfx=64;
static channel_t *freelist=nullptr,*endflist=nullptr;static float entityPositions[1024][3];
'''
    for name in ('S_ChannelFree', 'S_ChannelMalloc', 'S_ChannelSetup', 'S_ThreadStartSoundEx'):
        source += '\n' + function(original, name)
    source = source.replace('namespace reference {', 'constexpr bool kSP=' + ('true' if args.sp_source else 'false') + ';\nnamespace reference {')
    source += COMPARE
    target = [f'--target={args.target}'] if args.target else ([] if os.name == 'nt' else ['-m32'])
    flags = ['-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-g'] if args.sanitize else []
    report = dict(utc=datetime.datetime.now(datetime.timezone.utc).isoformat(), mode='sp' if args.sp_source else 'mp',
        scope='Defined one-shot cutoff, reuse, FIFO allocation and age; reference exhaustion excluded',
        target=subprocess.check_output([args.compiler, *target, '-dumpmachine'], cwd=ROOT, text=True).strip(),
        sanitized=args.sanitize, implementationSha256=sha(ROOT / 'code/client/audio/shared/RTCWOneShots.h'),
        references=[dict(path=str(path), sha256=sha(path)) for path in (sound,root/'client/snd_local.h',root/'game/q_shared.h')])
    with tempfile.TemporaryDirectory(prefix='shot-reference-', dir=report_path.parent) as temporary:
        folder = Path(temporary); cpp = folder/'probe.cpp'; exe = folder/'probe.exe'; cpp.write_text(source, encoding='utf-8')
        command = [args.compiler, *target, *flags, '-std=c++17', '-O2', '-fno-strict-aliasing', '-I', str(ROOT), str(cpp), '-o', str(exe)]
        built = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, timeout=60)
        report.update(compileCommand=command, buildExitCode=built.returncode)
        if built.returncode:
            report['buildOutput'] = built.stdout + built.stderr
        else:
            options = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
            run = subprocess.run([str(exe)], cwd=ROOT, capture_output=True, text=True, timeout=60, **options)
            report.update(exitCode=run.returncode, stderr=run.stderr)
            if not run.returncode: report.update(json.loads(run.stdout))
            else: report['stdout'] = run.stdout
        report['passed'] = not built.returncode and report.get('exitCode') == 0
    report_path.write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')
    print('PASS' if report['passed'] else 'FAIL', report.get('starts',0), 'starts,', report.get('stateComparisons',0), 'state comparisons')
    return 0 if report['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
