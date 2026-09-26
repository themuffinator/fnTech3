#!/usr/bin/env python3
"""Compare RTCW loop submissions and stereo gains with original SP or MP source.

Reference functions and their GPLv3 notices remain in disposable fixtures.
No sound files are required; this does not establish device/PCM or timing parity.
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

PRELUDE = r'''
#include "code/client/audio/shared/RTCWLoops.h"
#include <cassert>
#include <climits>
#include <cstdint>
#include <cstdio>
#include <cstring>
namespace reference {
using vec_t=float;using vec3_t=float[3];using sfxHandle_t=int;using qboolean=int;
constexpr int qfalse=0,qtrue=1,ERR_DROP=1,CA_ACTIVE=5,MAX_LOOP_SOUNDS=128,SOUND_RANGE_DEFAULT=1250;
struct sfx_t {int inMemory=1,soundLength=1;const char *soundName="synthetic";};
static sfx_t s_knownSfx[2];static int s_numSfx=2;
static int s_soundStarted=1,s_soundMuted=0,numLoopSounds=0;
static struct {int state=CA_ACTIVE;} cls;
static void Com_Error(int,const char *,...){throw 1;}
static void S_memoryLoad(sfx_t *s){s->inMemory=1;}
static float listener_origin[3],listener_axis[3][3];
static struct {int channels=2;} dma;
static void VectorSubtract(const float *a,const float *b,float *out){for(int i=0;i<3;++i)out[i]=a[i]-b[i];}
static void VectorCopy(const float *in,float *out){std::memcpy(out,in,12);}
static float DotProduct(const float *a,const float *b){return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];}
'''

COMPARE = r'''
} // reference
static std::uint32_t randomState=0x19810627;
static float coordinate(){randomState^=randomState<<13;randomState^=randomState>>17;randomState^=randomState<<5;return float(int(randomState%16001)-8000)/4;}
int main(){
 using namespace fntech3::audio::rtcw;
 int submissions=0,channels=0,shots=0;
 for(float fade:kFades)for(int range:{0,1,80,500,1250,INT_MAX})for(int volume:{INT_MIN,-1,0,1,128,255,256,257,511,512,INT_MAX}){
  reference::master=fade;LoopFrame frame;reference::numLoopSounds=0;std::memset(reference::loopSounds,0,sizeof(reference::loopSounds));
  for(int i=0;i<141;++i){
   float origin[]={float(i),float(-i),float(i%7)},velocity[]={100,0,-100};
   const int index=frame.Append(origin,velocity,range,1,volume,fade);
   reference::S_AddLoopingSound(i%3,origin,velocity,range,1,volume);
   assert(frame.Size()==std::size_t(reference::numLoopSounds));
   if(index>=0){const auto &a=frame[index];const auto &b=reference::loopSounds[index];
    assert(!std::memcmp(a.origin.data(),b.origin,12)&&!std::memcmp(a.velocity.data(),b.velocity,12));
    // Compare stored IEEE encodings; x87 expression casts may retain extra precision.
    if(!(std::memcmp(&a.range,&b.range,sizeof(float))==0&&a.volume==b.vol&&int(a.loudUnderwater)==b.loudUnderWater)){
     std::fprintf(stderr,"submission mismatch range=%d volume=%d slot=%d actual=%.9g/%d/%d reference=%.9g/%d/%d\n",range,volume,index,double(float(a.range)),a.volume,int(a.loudUnderwater),double(b.range),b.vol,b.loudUnderWater);return 1;
    }
    assert(b.sfx==reference::s_knownSfx+1&&a.handle==1);
   }++submissions;
  }
 }
 for(float fade:kFades)for(int range:{1,80,500,1250,4000,INT_MAX})for(int inputVolume:{1,64,128,255})for(int mono:{0,1})
 for(int orientation=0;orientation<4;++orientation)for(int point=0;point<1024;++point){
  const int volume=int(float(inputVolume)*fade);Loop loop;loop.range=range;loop.volume=volume;
  for(int i=0;i<3;++i){loop.origin[i]=coordinate();reference::listener_origin[i]=coordinate();}
  if(point<3)loop.origin={reference::listener_origin[0],reference::listener_origin[1],reference::listener_origin[2]};
  std::memset(reference::listener_axis,0,sizeof(reference::listener_axis));
  reference::listener_axis[2][2]=1;
  const int axis=orientation&1;const float sign=orientation<2?1.0f:-1.0f;
  reference::listener_axis[0][axis]=sign;reference::listener_axis[1][1-axis]=axis?-sign:sign;
  reference::dma.channels=mono?1:2;
  int left,right;reference::S_SpatializeOrigin(loop.origin.data(),90,&left,&right,float(range));
  left=int(float(volume)*float(left)/256.0);right=int(float(volume)*float(right)/256.0);
  left=std::min(left,255);right=std::min(right,255);
  const auto actual=SpatializeLoop(loop,reference::listener_origin,reference::listener_axis,mono!=0);
  if(actual.left!=left||actual.right!=right){
   std::fprintf(stderr,"gain mismatch range=%d volume=%d mono=%d point=%d actual=%d/%d reference=%d/%d\n",range,volume,mono,point,actual.left,actual.right,left,right);return 1;
  }
  // Original one-shots always use 1250; only loops expose an arbitrary range.
  if(range==1250){
  reference::S_SpatializeOrigin(loop.origin.data(),127,&left,&right,float(range));
  const auto shot=SpatializeOrigin(loop.origin.data(),float(range),127,reference::listener_origin,reference::listener_axis,mono!=0);
  if(shot.left!=left||shot.right!=right){
   std::fprintf(stderr,"one-shot gain mismatch range=%d point=%d actual=%d/%d reference=%d/%d\n",range,point,shot.left,shot.right,left,right);return 1;
  }++shots;
  }++channels;
 }
 std::printf("{\"submissionComparisons\":%d,\"stereoGainComparisons\":%d,\"oneShotGainComparisons\":%d}\n",submissions,channels,shots);
}
'''


def function(text, signature):
    start = text.index(signature)
    return text[start:text.index('\n}', start) + 2]


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    selection=parser.add_mutually_exclusive_group(required=True)
    selection.add_argument('--mp-source', type=Path)
    selection.add_argument('--sp-source', type=Path)
    parser.add_argument('--target')
    parser.add_argument('--sanitize', action='store_true')
    parser.add_argument('--compiler', default=shutil.which('clang++'))
    parser.add_argument('--report', type=Path, required=True)
    args = parser.parse_args()
    report_path = args.report.resolve(); report_path.relative_to(ROOT)
    report_path.parent.mkdir(parents=True, exist_ok=True)
    root = (args.sp_source or args.mp_source) / 'src'
    sound = root / 'client/snd_dma.c'; types = root / 'client/snd_local.h'; math = root / 'game/q_math.c'
    text = sound.read_text(); math_text = math.read_text()
    source = text[:text.index('*/') + 2] + '\n' + math_text[:math_text.index('*/') + 2] + '\n' + PRELUDE
    source += re.search(r'typedef struct loopSound_s\s*\{[^{}]*\}\s*loopSound_t;', types.read_text())[0]
    if args.sp_source:
        source += '''
static struct {int s_soundStarted=1,s_soundMute=0,s_numSfx=2,numLoopSounds=0;float volCurrent=1;loopSound_t loopSounds[MAX_LOOP_SOUNDS]{};} snd;
static auto &loopSounds=snd.loopSounds;
static auto &numLoopSounds=snd.numLoopSounds;
static auto &master=snd.volCurrent;
'''
        # The unused MP globals from PRELUDE would conflict with these aliases.
        source=source.replace('s_soundMuted=0,numLoopSounds=0;', 's_soundMuted=0;')
        mixer=(root / 'client/snd_mix.c').read_text()
        begin=mixer.index('\tif ( s_mute->value )',mixer.index('void S_PaintChannels('))
        block=mixer[begin:mixer.index('\n\t//Com_Printf',begin)]
        source += '\nstatic int snd_vol;struct cvar{float value;};static cvar effects{1},mute{0};static auto *s_volume=&effects,*s_mute=&mute;\n'
        source += 'static int EffectsVolume(float volume,float fade){effects.value=volume;snd.volCurrent=fade;\n'+block+'\nreturn snd_vol;\n}\n'
    else:
        source += '\nstatic loopSound_t loopSounds[MAX_LOOP_SOUNDS];static float master=1;\n'
    fades='{0.f,0.001f,0.125f,0.333f,0.5f,0.999f,1.f,2.f,16.f}' if args.sp_source else '{1.f}'
    source=source.replace('namespace reference {','static const float kFades[]='+fades+';\nnamespace reference {')
    source += function(math_text, 'vec_t VectorNormalize(') + '\n' + function(math_text, 'void VectorRotate(') + '\n'
    source += function(text, 'void S_SpatializeOrigin(') + '\n#define UNDERWATER_BIT 8\n'
    source += function(text, 'void S_AddLoopingSound(') + '\n' + COMPARE
    if args.sp_source:
        source=source.replace(' int submissions=0,channels=0,shots=0;', ''' int submissions=0,channels=0,shots=0;
 for(float fade:kFades)for(int level=0;level<=1024;++level){
  const float volume=float(level)/512.f;
  assert(EffectsMixVolume(volume,fade)==reference::EffectsVolume(volume,fade));
 }
''')
    target = [f'--target={args.target}'] if args.target else ([] if os.name == 'nt' else ['-m32'])
    flags = ['-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-g'] if args.sanitize else []
    report = dict(utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),
        scope='Original '+('SP' if args.sp_source else 'MP')+' loop submissions, fades and stereo gains; no output-device or PCM phase evidence',
        effectsVolumeComparisons=9225 if args.sp_source else 0,
        target=subprocess.check_output([args.compiler, *target, '-dumpmachine'], cwd=ROOT, text=True).strip(),
        sanitized=args.sanitize, implementationSha256=sha(ROOT / 'code/client/audio/shared/RTCWLoops.h'),
        mixPolicySha256=sha(ROOT / 'code/client/audio/shared/RTCWMix.h'),
        referenceSources=[dict(path=str(path), sha256=sha(path)) for path in
            (sound, types, math, root/'client/cl_cgame.c') + ((root/'client/snd_mix.c',) if args.sp_source else ())])
    with tempfile.TemporaryDirectory(prefix='loop-reference-', dir=report_path.parent) as directory:
        folder = Path(directory); cpp = folder / 'probe.cpp'; exe = folder / 'probe.exe'
        cpp.write_text(source, encoding='utf-8')
        command = [args.compiler, *target, *flags, '-std=c++17', '-O2', '-fno-fast-math',
                   '-I', str(ROOT), str(cpp), '-o', str(exe)]
        built = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, timeout=60)
        report.update(compileCommand=command, buildExitCode=built.returncode)
        if built.returncode:
            report['buildOutput'] = built.stdout + built.stderr
        else:
            options = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
            run = subprocess.run([str(exe)], cwd=ROOT, capture_output=True, text=True, timeout=30, **options)
            report.update(exitCode=run.returncode, stderr=run.stderr)
            if not run.returncode: report.update(json.loads(run.stdout))
            else: report['stdout'] = run.stdout
        report['passed'] = not built.returncode and report.get('exitCode') == 0
    report_path.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print('PASS' if report['passed'] else 'FAIL', report.get('submissionComparisons', 0), 'submissions,',
          report.get('stereoGainComparisons', 0), 'stereo gains')
    return 0 if report['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
