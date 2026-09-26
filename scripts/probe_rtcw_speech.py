#!/usr/bin/env python3
"""Compare SP speech measurements with original/iortcw mixer source.

Unchanged reference functions and their notices exist only in disposable probes.
Compressed-reader fixtures supply decoded PCM; this does not validate codecs,
backend clock alignment, audible output, or facial interpolation in the game.
"""
from __future__ import annotations
import argparse
import datetime
import hashlib
import json
import os
import re
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tests'))
from mesh_tag_tests import function
from rtcw_speech_tests import LEGACY

REFERENCE = r'''
#include <cstdio>
#include <cstdlib>
namespace reference {
using byte=unsigned char;
constexpr int SND_CHUNK_SIZE_FLOAT=512,MAX_CLIENTS=128,CHAN_VOICE=3;
constexpr float TALK_FUTURE_SEC=.25f;
static unsigned char s_entityTalkAmplitude[MAX_CLIENTS];
static int sfxScratchIndex=-1;
static const sfx_t *sfxScratchPointer=nullptr;
static short sfxScratchBuffer[4096];
'''
STREAM = r'''
static int Stream(short sample,int count){
 static struct{int left=0;}s_rawsamples[1][MAX_RAW_SAMPLES];
 static struct{int integer=4;}khz;auto *s_khz=&khz;
 struct{int channel=3,entchannel=3,entnum=7;}owner;auto *ss=&owner;auto *ch=&owner;
 const bool firstPass=true;int si=0,i=0,s=0,stream=0,s_paintedtime=0;
 const int lead=REFERENCE_LEAD;
 int s_rawend[]={lead+count};auto *rawsamples=s_rawsamples[0];
 for(int n=0;n<100;++n)s_rawsamples[0][lead+n].left=int(sample)*256;
'''
CHECKS = r'''
 return s_entityTalkAmplitude[7];
}
}
int main(){
 unsigned cases=0,mismatches=0;
 auto check=[&](int actual,int expected){++cases;if(actual!=expected){if(mismatches<8)std::fprintf(stderr,"%d != %d at %u\n",actual,expected,cases);++mismatches;}};
 std::array<sndBuffer,3>chunks;
 for(int i=0;i<2;++i)chunks[i].next=&chunks[i+1];
 sfx_t sample;sample.soundData=chunks.data();sample.soundLength=3072;
 // Every signed PCM16 value, varied occupancy, and opposite-sign cancellation.
 for(int value=-32768;value<=32767;++value){
  const int count=(value+32768)%101;
  for(int i=0;i<100;++i)chunks[0].sndChunk[i]=i<count?short(value):0;
  reference::S_SetVoiceAmplitudeFrom16(&sample,0,100,7);
  int actual=-1;assert(S_Base_RTCWSpeechWindow(&sample,0,actual));check(actual,reference::s_entityTalkAmplitude[7]);
  for(int i=0;i<100;++i)chunks[0].sndChunk[i]=i%2?short(value):short(value==-32768?32767:-value);
  reference::S_SetVoiceAmplitudeFrom16(&sample,0,100,7);
  assert(S_Base_RTCWSpeechWindow(&sample,0,actual));check(actual,reference::s_entityTalkAmplitude[7]);
  check(count?SpeechStreamSample(value*256):0,reference::Stream(short(value),count));
 }
 // Exact chunk traversal for PCM, decoded ADPCM/wavelet, and mu-law table lookup.
 unsigned random=0x2198ab71u;
 auto next=[&](){random=random*1664525u+1013904223u;return random;};
 for(int method=0;method<4;++method){
  const int capacity=method==1?4096:method?2048:1024;
  sample.soundCompressionMethod=method;sample.soundLength=capacity*3;
  for(int i=0;i<256;++i)mulawToShort[i]=short((i*257)-32768);
  for(int pattern=0;pattern<64;++pattern){
   for(auto &chunk:chunks){
    for(auto &v:chunk.sndChunk)v=short(next()>>16);
    for(auto &v:chunk.decoded)v=short(next()>>16);
   }
   for(int offset=0;offset<sample.soundLength-100;offset+=17){
    reference::sfxScratchPointer=nullptr;reference::sfxScratchIndex=-1;
    if(method==0)reference::S_SetVoiceAmplitudeFrom16(&sample,offset,100,7);
    if(method==1)reference::S_SetVoiceAmplitudeFromADPCM(&sample,offset,100,7);
    if(method==2)reference::S_SetVoiceAmplitudeFromWavelet(&sample,offset,100,7);
    if(method==3)reference::S_SetVoiceAmplitudeFromMuLaw(&sample,offset,100,7);
    int actual=-1;assert(S_Base_RTCWSpeechWindow(&sample,offset,actual));
    check(actual,reference::s_entityTalkAmplitude[7]);
   }
  }
 }
 std::printf("{\"cases\":%u,\"mismatches\":%u}\n",cases,mismatches);
 return mismatches?1:0;
}
'''


def fixture(text):
    helpers = '\n'.join(function(text, 'void S_SetVoiceAmplitudeFrom' + name + '(')
                        for name in ('16', 'ADPCM', 'Wavelet', 'MuLaw'))
    original = 'if ( firstPass && ss->channel == CHAN_VOICE' in text
    anchor = 'if ( firstPass && ' + ('ss->channel' if original else 'ch->entchannel') + ' == CHAN_VOICE'
    start = text.index(anchor); opening = text.index('{', start); depth = 1; end = opening + 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}'); end += 1
    stream = text[start:end]
    lookahead = re.search(r'#define TALK_FUTURE_SEC\s+([.0-9]+)', text).group(1)
    reference = REFERENCE.replace('TALK_FUTURE_SEC=.25f', 'TALK_FUTURE_SEC=' + lookahead + 'f')
    prelude = STREAM.replace('REFERENCE_LEAD', 'int(TALK_FUTURE_SEC * ' + ('4' if original else '22') + ' * 1000)')
    notice = text[:text.index('*/') + 2]
    return notice + '\n' + LEGACY[:LEGACY.index('int main()')] + reference + helpers + prelude + stream + CHECKS


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sp-source', type=Path, required=True)
    parser.add_argument('--iortcw-source', type=Path)
    parser.add_argument('--report', type=Path, required=True)
    parser.add_argument('--compiler', default=shutil.which('clang++'))
    parser.add_argument('--target')
    parser.add_argument('--sanitize', action='store_true')
    args = parser.parse_args()
    report_path = args.report.resolve(); report_path.relative_to(ROOT)
    report_path.parent.mkdir(parents=True, exist_ok=True)
    target = ['--target=' + args.target] if args.target else (['-m32'] if os.name != 'nt' else [])
    triple = subprocess.check_output([args.compiler, *target, '-dumpmachine'], cwd=ROOT, text=True).strip()
    report = dict(kind=__doc__, dateUtc=datetime.datetime.now(datetime.timezone.utc).isoformat(),
                  target=triple, compiler=args.compiler, sanitized=args.sanitize,
                  implementationSha256={name: digest(ROOT / name) for name in (
                      'code/client/audio/shared/RTCWSpeech.h', 'code/client/audio/legacy/snd_rtcw_voice.hpp',
                      'tests/rtcw_speech_tests.py', 'scripts/probe_rtcw_speech.py')}, results=[])
    references = [('original', args.sp_source / 'src/client/snd_mix.c')]
    if args.iortcw_source:
        references.append(('iortcw', args.iortcw_source / 'SP/code/client/snd_mix.c'))
    for name, path in references:
        with tempfile.TemporaryDirectory(prefix='speech-reference-', dir=report_path.parent) as temp:
            folder = Path(temp); source = folder / 'probe.cpp'; source.write_text(fixture(path.read_text()), encoding='utf-8')
            executable = folder / ('probe.exe' if os.name == 'nt' else 'probe')
            command = [args.compiler, *target, '-std=c++17', '-O1', '-I', str(ROOT), str(source), '-o', str(executable)]
            if args.sanitize:
                command += ['-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-g']
            build = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, timeout=90)
            result = dict(reference=name, source=str(path), sourceSha256=digest(path),
                          buildExitCode=build.returncode, buildOutput=build.stdout + build.stderr, passed=False)
            if build.returncode == 0:
                environment = os.environ.copy()
                if args.sanitize:
                    environment.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1', UBSAN_OPTIONS='halt_on_error=1')
                options = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
                run = subprocess.run([str(executable)], cwd=ROOT, capture_output=True, text=True, timeout=45, env=environment, **options)
                result.update(exitCode=run.returncode, diagnostics=run.stderr,
                              observations=json.loads(run.stdout) if run.returncode == 0 else None)
                result['passed'] = run.returncode == 0 and result['observations']['cases'] > 250000 and result['observations']['mismatches'] == 0
            report['results'].append(result)
            report_path.write_text(json.dumps(report, indent=2) + '\n')
            print(name, 'PASS' if result['passed'] else 'FAIL', result.get('observations'), flush=True)
    return 0 if all(row['passed'] for row in report['results']) else 1


if __name__ == '__main__':
    raise SystemExit(main())
