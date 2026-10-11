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
int Com_GameIsJK2(){return profile==COM_GAME_JK2_SP||profile==COM_GAME_JK2_MP;}
int Com_GameIsJK2SP(){return profile==COM_GAME_JK2_SP;}
static std::string language;
const char *Cvar_VariableString(const char *name){return std::strcmp(name,"s_language")?"":language.c_str();}
const char *Q_stristr(const char *s,const char *find){for(;*s;++s){std::size_t i=0;while(find[i]&&std::tolower(static_cast<unsigned char>(s[i]))==std::tolower(static_cast<unsigned char>(find[i])))++i;if(!find[i])return s;}return nullptr;}
struct snd_info_t{};struct snd_stream_t{int identity;};
struct snd_codec_t{const char *ext;void *(*load)(const char *,snd_info_t *);snd_stream_t *(*open)(const char *);snd_codec_t *next;};
static bool wavExists,oggExists,mp3Exists=true;static snd_stream_t wavData{1},mp3Data{2},oggData{3};
static std::vector<std::string> attempts,voices; // voices: the voice files present, any format
static bool Voice(const char *path){for(const auto &voice:voices)if(voice==path)return true;return false;}
static void *WavLoad(const char *path,snd_info_t *){attempts.emplace_back(path);return (wavExists&&!std::strcmp(path,"sound/test.wav"))||Voice(path)?&wavData:nullptr;}
static void *Mp3Load(const char *path,snd_info_t *){attempts.emplace_back(path);return (mp3Exists&&!std::strcmp(path,"sound/test.mp3"))||Voice(path)?&mp3Data:nullptr;}
static void *OggLoad(const char *path,snd_info_t *){attempts.emplace_back(path);return (oggExists&&!std::strcmp(path,"sound/test.ogg"))||Voice(path)?&oggData:nullptr;}
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
        function(source,'bool S_STVEFGermanVoice(')+'\n'+function(source,'bool S_JK2SoundFile(')+'\n'+ \
        function(source,'bool S_JK2ForeignVoice(')+'\n'+function(source,'bool S_JKAForeignVoice(')+'\n'+ \
        function(source,'extern "C" void S_CodecInit(')+'\n'+function(source,'extern "C" void *S_CodecLoad(') + r'''
using Names=std::vector<std::string>;
// Outcast's order for a sound cut to `stem`: retail's .wav, then .mp3, then Fn's Ogg Vorbis.
static Names OutcastAttempts(const std::string &stem,bool mp3Codec,const void *&result){
 Names names{stem+".wav"};result=nullptr;
 const bool test=stem=="sound/test";
 if(test&&wavExists){result=&wavData;return names;}
 if(mp3Codec){names.push_back(stem+".mp3");if(test&&mp3Exists){result=&mp3Data;return names;}}
 names.push_back(stem+".ogg");if(test&&oggExists)result=&oggData;
 return names;
}
// Outcast's order for a voice cut to `stem`, up to the first file present.
static Names Reads(const std::string &stem,bool mp3Codec,bool &found){
 Names names;found=false;
 for(const char *ext:{".wav",".mp3",".ogg"}){
  if(!mp3Codec&&!std::strcmp(ext,".mp3"))continue;
  names.push_back(stem+ext);
  if(Voice(names.back().c_str())){found=true;break;}
 }
 return names;
}
// A voice name, its cut English stem and its German and French stems (none
// without "chars"), and whether multiplayer finds "chars" as it is written.
struct VoiceCase{const char *name,*english,*german,*french;bool multiplayer;};
static const VoiceCase voiceCases[]={
 {"sound/chars/kyle/misc/death1.mp3","sound/chars/kyle/misc/death1","sound/chr_d/kyle/misc/death1","sound/chr_f/kyle/misc/death1",true},
 {"sound/chars/mothma/misc/40MOM038","sound/chars/mothma/misc/40MOM038","sound/chr_d/mothma/misc/40MOM038","sound/chr_f/mothma/misc/40MOM038",true},
 {"sound/chars/chars/x.wav","sound/chars/chars/x","sound/chr_d/chars/x","sound/chr_f/chars/x",true}, // the first only
 {"sound/xcharsx/y.wav","sound/xcharsx/y","sound/xchr_dx/y","sound/xchr_fx/y",true}, // anywhere in the name
 {"sound/CHARS/x.wav","sound/CHARS/x","sound/chr_d/x","sound/chr_f/x",false}, // single player lowercases first
 {"sound/weapons/saber/saberon.wav","sound/weapons/saber/saberon",nullptr,nullptr,false}};
static void CheckVoices(bool outcast,bool academy,bool mp3Codec){
 snd_info_t info;wavExists=mp3Exists=oggExists=false;
 for(const char *setting:{"","english","ENGLISH","DEUTSCH","deutsch","Deutsch ","FRANCAIS","francais","german","italiano"}){
  language=setting;
  const bool german=!Q_stricmp(setting,"DEUTSCH"),french=!Q_stricmp(setting,"FRANCAIS");
  for(const auto &voice:voiceCases)for(int files=0;files<8;++files){
   // German and French recordings as .wav (1) or .mp3 (2), whatever the
   // setting; the English one as .mp3 (4), as Outcast ships most voices.
   voices.clear();
   for(const char *stem:{voice.german,voice.french}){
    if(stem&&(files&1))voices.push_back(std::string(stem)+".wav");
    if(stem&&(files&2))voices.push_back(std::string(stem)+".mp3");
   }
   if(files&4)voices.push_back(std::string(voice.english)+".mp3");
   attempts.clear();void *generic=S_CodecGetSound(voice.name,&info);const Names genericAttempts=attempts;
   attempts.clear();const int before=warnings;void *loaded=S_CodecLoad(voice.name,&info);
   assert(warnings-before==(loaded?0:1)); // The foreign attempt is quiet; a missing voice warns once.
   if(!outcast){assert(academy||(loaded==generic&&attempts==genericAttempts));continue;} // Academy's: CheckAcademyVoices.
   const char *foreign=german?voice.german:french?voice.french:nullptr;
   const bool substitutes=foreign&&(profile==COM_GAME_JK2_SP||voice.multiplayer);
   Names expected;bool found=false;
   if(substitutes)expected=Reads(foreign,mp3Codec,found);
   if(!found){const Names english=Reads(voice.english,mp3Codec,found);expected.insert(expected.end(),english.begin(),english.end());}
   assert(attempts==expected&&(loaded!=nullptr)==found);
  }
 }
 language.clear();voices.clear();
 if(!outcast||!mp3Codec)return;
 // Spelled out once: Mon Mothma's German line, then her English one.
 language="deutsch";voices={"sound/chr_d/mothma/misc/40MOM038.mp3","sound/chars/mothma/misc/40MOM038.mp3"};
 attempts.clear();assert(S_CodecLoad("sound/chars/mothma/misc/40MOM038",&info)==&mp3Data);
 assert(attempts==Names({"sound/chr_d/mothma/misc/40MOM038.wav","sound/chr_d/mothma/misc/40MOM038.mp3"}));
 voices={"sound/chars/mothma/misc/40MOM038.mp3"};attempts.clear();
 assert(S_CodecLoad("sound/chars/mothma/misc/40MOM038",&info)==&mp3Data);
 assert(attempts==Names({"sound/chr_d/mothma/misc/40MOM038.wav","sound/chr_d/mothma/misc/40MOM038.mp3",
  "sound/chr_d/mothma/misc/40MOM038.ogg","sound/chars/mothma/misc/40MOM038.wav","sound/chars/mothma/misc/40MOM038.mp3"}));
 language.clear();voices.clear();
}
// The codec list's reads for `name` up to the first file present: the name when
// a codec takes its extension, then the name without it (or whole, when none
// does) with each other codec's: .wav, .mp3, then Fn's Ogg Vorbis.
static Names CodecReads(const std::string &name,bool mp3Codec,bool &found){
 Names names;found=false;
 const auto dot=name.rfind('.');const std::string ext=dot==std::string::npos?"":name.substr(dot+1);
 const bool taken=ext=="wav"||ext=="ogg"||(ext=="mp3"&&mp3Codec);
 if(taken){names.push_back(name);if(Voice(name.c_str())){found=true;return names;}}
 for(const char *codec:{"wav","mp3","ogg"}){
  if((!mp3Codec&&!std::strcmp(codec,"mp3"))||(taken&&ext==codec))continue;
  names.push_back((taken?name.substr(0,dot):name)+"."+codec);
  if(Voice(names.back().c_str())){found=true;break;}
 }
 return names;
}
// An Academy voice name as a module asks for it, and its German, French and
// Spanish names (none without "chars").
struct AcademyCase{const char *name,*german,*french,*spanish;};
static const AcademyCase academyCases[]={
 {"sound/chars/kyle/misc/death1.mp3","sound/chr_d/kyle/misc/death1.mp3","sound/chr_f/kyle/misc/death1.mp3","sound/chr_e/kyle/misc/death1.mp3"},
 {"sound/chars/kyle/misc/taunt1","sound/chr_d/kyle/misc/taunt1","sound/chr_f/kyle/misc/taunt1","sound/chr_e/kyle/misc/taunt1"},
 {"sound/chars/boba/jethover.wav","sound/chr_d/boba/jethover.wav","sound/chr_f/boba/jethover.wav","sound/chr_e/boba/jethover.wav"},
 {"sound/CHARS/Kyle/x.wav","sound/chr_d/Kyle/x.wav","sound/chr_f/Kyle/x.wav","sound/chr_e/Kyle/x.wav"}, // any case; the rest keeps its own
 {"sound/Chars/chars/x.wav","sound/chr_d/chars/x.wav","sound/chr_f/chars/x.wav","sound/chr_e/chars/x.wav"}, // the first only
 {"sound/xcharsx/y.wav","sound/xchr_dx/y.wav","sound/xchr_fx/y.wav","sound/xchr_ex/y.wav"}, // anywhere in the name
 {"sound/weapons/saber/saberon.wav",nullptr,nullptr,nullptr}};
static std::string WithExtension(const std::string &name,const char *ext){
 const auto dot=name.rfind('.'),slash=name.rfind('/');
 return (dot!=std::string::npos&&(slash==std::string::npos||dot>slash)?name.substr(0,dot):name)+ext;
}
// Academy's voices, both profiles: with s_language DEUTSCH, FRANCAIS or ESPANOL
// in any case, the first "chars" in any case becomes chr_d, chr_f or chr_e, and
// that name is read quietly through the codec list before the English one.
static void CheckAcademyVoices(bool outcast,bool academy,bool mp3Codec){
 if(outcast)return; // Outcast's own rule: CheckVoices.
 snd_info_t info;wavExists=mp3Exists=oggExists=false;
 for(const char *setting:{"","english","ENGLISH","DEUTSCH","deutsch","Deutsch","FRANCAIS","francais","ESPANOL","espanol",
  "Espanol","espanol ","espa\xf1ol","german","spanish","italiano"}){
  language=setting;
  const char *AcademyCase::*field=!Q_stricmp(setting,"DEUTSCH")?&AcademyCase::german:!Q_stricmp(setting,"FRANCAIS")?
   &AcademyCase::french:!Q_stricmp(setting,"ESPANOL")?&AcademyCase::spanish:nullptr;
  for(const auto &voice:academyCases)for(int files=0;files<64;++files){
   // The German (1), French (2), Spanish (4) and English (8) recordings, each
   // an .mp3, or a .wav where 16 (the foreign ones) or 32 (the English) says.
   voices.clear();
   const char *const foreign[]={voice.german,voice.french,voice.spanish};
   for(int i=0;i<3;++i)if(foreign[i]&&(files&(1<<i)))voices.push_back(WithExtension(foreign[i],files&16?".wav":".mp3"));
   if(files&8)voices.push_back(WithExtension(voice.name,files&32?".wav":".mp3"));
   attempts.clear();void *generic=S_CodecGetSound(voice.name,&info);const Names genericAttempts=attempts;
   attempts.clear();const int before=warnings;void *loaded=S_CodecLoad(voice.name,&info);
   assert(warnings-before==(loaded?0:1)); // The foreign reads are quiet; a missing voice warns once.
   const char *name=academy&&field?voice.*field:nullptr;
   if(!name){assert(loaded==generic&&attempts==genericAttempts);continue;}
   bool found=false;Names expected=CodecReads(name,mp3Codec,found);
   if(!found){const Names english=CodecReads(voice.name,mp3Codec,found);expected.insert(expected.end(),english.begin(),english.end());}
   assert(attempts==expected&&(loaded!=nullptr)==found);
  }
 }
 language.clear();voices.clear();
 if(!academy||!mp3Codec)return;
 // Spelled out once: Kyle's Spanish taunt, then his English one when the Spanish is missing.
 language="espanol";voices={"sound/chr_e/kyle/misc/taunt1.mp3","sound/chars/kyle/misc/taunt1.mp3"};
 attempts.clear();assert(S_CodecLoad("sound/chars/kyle/misc/taunt1",&info)==&mp3Data);
 assert(attempts==Names({"sound/chr_e/kyle/misc/taunt1.wav","sound/chr_e/kyle/misc/taunt1.mp3"}));
 voices={"sound/chars/kyle/misc/taunt1.mp3"};attempts.clear();
 assert(S_CodecLoad("sound/chars/kyle/misc/taunt1",&info)==&mp3Data);
 assert(attempts==Names({"sound/chr_e/kyle/misc/taunt1.wav","sound/chr_e/kyle/misc/taunt1.mp3",
  "sound/chr_e/kyle/misc/taunt1.ogg","sound/chars/kyle/misc/taunt1.wav","sound/chars/kyle/misc/taunt1.mp3"}));
 language.clear();voices.clear();
}
int main(){
 for(unsigned int p=COM_GAME_AUTO;p<COM_GAME_COUNT;++p){
  profile=static_cast<comGameId_t>(p);S_CodecInit();
  assert(codecs==&wav_codec);
  bool allowed=profile==COM_GAME_STVEF_SP||profile==COM_GAME_STVEF_MP||profile==COM_GAME_ALICE||
   profile==COM_GAME_FAKK2||profile==COM_GAME_MOHAA||profile==COM_GAME_JKA_SP||profile==COM_GAME_JKA_MP||
   profile==COM_GAME_JK2_SP||profile==COM_GAME_JK2_MP;
  const bool outcast=profile==COM_GAME_JK2_SP||profile==COM_GAME_JK2_MP;
  const bool academy=profile==COM_GAME_JKA_SP||profile==COM_GAME_JKA_MP;
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
  // S_CodecLoad: Outcast cuts every sound name at the first '.' of its file
  // name and reads the .wav first, so a .wav beside an .mp3 wins whichever
  // the name asks for. Every other profile loads the name as given.
  const std::string fits="sound/"+std::string(53,'x'),tooLong="sound/"+std::string(54,'x');
  const std::pair<std::string,std::string> names[]={{"sound/test.mp3","sound/test"},{"sound/test","sound/test"},
   {"sound/test.wav","sound/test"},{"sound/test.x.wav","sound/test"},{"sound/test.ogg","sound/test"},
   {"sound/test.","sound/test"},{"sound/a.b/test.wav","sound/a.b/test"},{"sound\\a.b\\test.mp3","sound\\a.b\\test"},
   {fits+".mp3",fits},{tooLong,""}};
  for(const auto &[name,stem]:names)for(int files=0;files<8;++files){
   wavExists=files&1;mp3Exists=(files&2)!=0;oggExists=(files&4)!=0;
   attempts.clear();void *generic=S_CodecGetSound(name.c_str(),&info);const Names genericAttempts=attempts;
   attempts.clear();const int beforeLoad=warnings;void *loaded=S_CodecLoad(name.c_str(),&info);
   assert(warnings-beforeLoad==(loaded?0:1)); // A missing sound still warns once.
   if(!outcast||stem.empty()){assert(loaded==generic&&attempts==genericAttempts);continue;}
   const void *expected=nullptr;const Names order=OutcastAttempts(stem,allowed,expected);
   assert(attempts==order&&loaded==expected);
  }
  if(outcast){
   wavExists=mp3Exists=true;attempts.clear();assert(S_CodecLoad("sound/test.mp3",&info)==&wavData);
   assert(attempts==Names({"sound/test.wav"}));
  }
  CheckVoices(outcast,academy,allowed);
  CheckAcademyVoices(outcast,academy,allowed);
  wavExists=false;mp3Exists=oggExists=true;
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
