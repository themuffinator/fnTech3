"""Execute codec registration and fallback for every immutable game profile."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]
TASK=ROOT/'.tmp/tests/audio-codec-profile'
from compiler_support import compiler_target_args
from rtcw_music_fixture import function

def fixture():
    source=(ROOT/'code/client/audio/codecs/snd_codec.cpp').read_text()
    facade=(ROOT/'code/client/audio/snd_main.cpp').read_text()
    return r'''
#include "code/qcommon/q_shared.h"
#include "code/qcommon/game_profile.h"
#include <array>
#include <cassert>
#include <cctype>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#define USE_OGG_VORBIS 1
static comGameId_t profile;
static bool jkaQuietSoundRegistration;
void S_ClearJKAMusicPreload(){}
static int warnings;
struct Drop{};
void QDECL Com_Error(errorParm_t,const char *,...){throw Drop{};}
int Com_GameIsSTVEF(){return profile==COM_GAME_STVEF_SP||profile==COM_GAME_STVEF_MP;}
int Com_GameIsRitual(){return profile==COM_GAME_ALICE||profile==COM_GAME_FAKK2;}
int Com_GameIsMOHAA(){return profile==COM_GAME_MOHAA;}
int Com_GameIsJKASP(){return profile==COM_GAME_JKA_SP;}
int Com_GameIsJKAMP(){return profile==COM_GAME_JKA_MP;}
struct snd_info_t{};struct snd_stream_t{int identity;};
struct snd_codec_t{const char *ext;void *(*load)(const char *,snd_info_t *);snd_stream_t *(*open)(const char *);snd_codec_t *next;};
static bool wavExists,oggExists;static snd_stream_t wavData{1},mp3Data{2},oggData{3};
static std::vector<std::string> attempts;
static void *WavLoad(const char *path,snd_info_t *){attempts.emplace_back(path);return wavExists&&!std::strcmp(path,"sound/test.wav")?&wavData:nullptr;}
static void *Mp3Load(const char *path,snd_info_t *){attempts.emplace_back(path);return !std::strcmp(path,"sound/test.mp3")?&mp3Data:nullptr;}
static void *OggLoad(const char *path,snd_info_t *){attempts.emplace_back(path);return oggExists&&!std::strcmp(path,"sound/test.ogg")?&oggData:nullptr;}
static snd_stream_t *WavOpen(const char *path){return static_cast<snd_stream_t *>(WavLoad(path,nullptr));}
static snd_stream_t *Mp3Open(const char *path){return static_cast<snd_stream_t *>(Mp3Load(path,nullptr));}
static snd_stream_t *OggOpen(const char *path){return static_cast<snd_stream_t *>(OggLoad(path,nullptr));}
static snd_codec_t wav_codec{"wav",WavLoad,WavOpen,nullptr},mp3_codec{"mp3",Mp3Load,Mp3Open,nullptr},ogg_codec{"ogg",OggLoad,OggOpen,nullptr};
static snd_codec_t *codecs;
void Q_strncpyz(char *out,const char *in,int size){assert(size>0);std::snprintf(out,std::size_t(size),"%s",in);}
int Q_stricmp(const char *a,const char *b){while(*a&&*b&&std::tolower(static_cast<unsigned char>(*a))==std::tolower(static_cast<unsigned char>(*b))){++a;++b;}return std::tolower(static_cast<unsigned char>(*a))-std::tolower(static_cast<unsigned char>(*b));}
const char *COM_GetExtension(const char *name){auto *dot=std::strrchr(name,'.');return dot?dot+1:name+std::strlen(name);}
void COM_StripExtension(const char *in,char *out,int size){Q_strncpyz(out,in,size);if(auto *dot=std::strrchr(out,'.'))*dot=0;}
int QDECL Com_sprintf(char *out,int size,const char *format,...){va_list args;va_start(args,format);const int result=std::vsnprintf(out,std::size_t(size),format,args);va_end(args);return result;}
void QDECL Com_DPrintf(const char *format,...){if(std::strstr(format,"WARNING:"))++warnings;}
''' + function(facade,'extern "C" void S_JKASetSoundRegistrationQuiet(')+'\n'+ \
        function(facade,'extern "C" qboolean S_SoundRegistrationWarningsEnabled(')+'\n'+ \
        function(source,'void S_CodecRegister(')+'\n'+function(source,'void *S_CodecGetSound(')+'\n'+ \
        function(source,'extern "C" void S_CodecInit(') + r'''
int main(){
 for(unsigned int p=COM_GAME_AUTO;p<COM_GAME_COUNT;++p){
  profile=static_cast<comGameId_t>(p);S_CodecInit();
  assert(codecs==&wav_codec);
  bool allowed=profile==COM_GAME_STVEF_SP||profile==COM_GAME_STVEF_MP||profile==COM_GAME_ALICE||
   profile==COM_GAME_FAKK2||profile==COM_GAME_MOHAA||profile==COM_GAME_JKA_SP||profile==COM_GAME_JKA_MP;
#ifndef USE_CODEC_MP3
  allowed=false;
#endif
  assert(wav_codec.next==(allowed?&mp3_codec:&ogg_codec));
  if(allowed)assert(mp3_codec.next==&ogg_codec);assert(!ogg_codec.next);
  snd_info_t info;wavExists=oggExists=false;attempts.clear();
  assert(S_CodecGetSound("sound/test.wav",&info)==(allowed?&mp3Data:nullptr));
  if(allowed)assert(attempts==std::vector<std::string>({"sound/test.wav","sound/test.mp3"}));
  else for(const auto &attempt:attempts)assert(attempt!="sound/test.mp3");
  assert(S_CodecGetSound("sound/test.wav",nullptr)==(allowed?&mp3Data:nullptr));
  assert(S_CodecGetSound("sound/test.mp3",&info)==(allowed?&mp3Data:nullptr));
  wavExists=true;attempts.clear();assert(S_CodecGetSound("sound/test.wav",&info)==&wavData);
  assert(attempts==std::vector<std::string>({"sound/test.wav"}));
  wavExists=false;oggExists=true;assert(S_CodecGetSound("sound/test",&info)==(allowed?&mp3Data:&oggData));
  // Quiet registration retains the same missing result and codec attempts.
  // Every other profile rejects the setter and retains its diagnostics.
  bool refused=false;try{S_JKASetSoundRegistrationQuiet(qtrue);}catch(Drop){refused=true;}
  assert(refused==(profile!=COM_GAME_JKA_MP));
  attempts.clear();
  const int before=warnings;assert(!S_CodecGetSound("sound/absent.wav",&info));
  assert(warnings-before==(profile==COM_GAME_JKA_MP?0:1));
  const auto missingAttempts=attempts;attempts.clear();
  const int beforeOpen=warnings;assert(!S_CodecGetSound("sound/absent.wav",nullptr));
  assert(warnings==beforeOpen+1&&attempts==missingAttempts); // Streaming failures stay diagnostic.
  if(profile==COM_GAME_JKA_MP){
   assert(!S_SoundRegistrationWarningsEnabled());S_JKASetSoundRegistrationQuiet(qfalse);
   const int beforeReset=warnings;assert(!S_CodecGetSound("sound/absent.wav",&info));assert(warnings==beforeReset+1);
   assert(S_SoundRegistrationWarningsEnabled());
  }
 }
}
'''

class AudioCodecProfileTests(unittest.TestCase):
    def test_profile_codec_list_load_and_stream_fallback(self):
        compiler=shutil.which('clang++') or shutil.which('g++')
        if not compiler:self.skipTest('C++ compiler unavailable')
        self.assertTrue(TASK.resolve().is_relative_to(ROOT.resolve()))
        TASK.mkdir(parents=True,exist_ok=True)
        output=Path(tempfile.mkdtemp(dir=TASK,prefix='codec-registration-')).absolute()
        try:
            src=output/'codec.cpp';src.write_text(fixture())
            for enabled in (False,True):
                exe=output/('codec-'+str(int(enabled))+('.exe' if os.name=='nt' else ''))
                flags=['-DUSE_CODEC_MP3=1'] if enabled else []
                command=[compiler,*compiler_target_args(compiler),'-std=c++17',*flags,'-I',str(ROOT),str(src),'-o',str(exe)]
                built=subprocess.run(command,cwd=ROOT,text=True,capture_output=True,timeout=90)
                self.assertEqual(built.returncode,0,built.stdout+built.stderr)
                ran=subprocess.run([str(exe)],cwd=ROOT,text=True,capture_output=True,timeout=20)
                self.assertEqual(ran.returncode,0,ran.stdout+ran.stderr)
        finally:
            self.assertEqual(output.resolve(),output)
            self.assertTrue(output.is_relative_to(TASK.absolute()))
            for directory,directories,names in os.walk(output,followlinks=False):
                for path in [Path(directory),*(Path(directory)/name for name in directories+names)]:
                    self.assertFalse(path.is_symlink() or getattr(path.stat(),'st_file_attributes',0)&0x400)
            shutil.rmtree(output)

if __name__=='__main__':unittest.main()
