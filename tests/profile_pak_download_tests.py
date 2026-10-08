"""Execute profile-owned retail-pak classification and missing-pak planning."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]


class ProfilePakDownloadTests(unittest.TestCase):
    def test_retail_exclusions_and_missing_pak_plans(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++ compiler unavailable')
        source = (ROOT / 'code/qcommon/files.c').read_text()

        def function(signature):
            start = source.index(signature)
            return source[start:source.index('\n}', start) + 2]

        harness = r'''
#include <cassert>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <string>
#include <algorithm>
#include <cctype>
using qboolean=bool;
constexpr bool qtrue=true,qfalse=false;
#define BASEGAME "baseq3"
#define BASETA "missionpack"
#define NUM_ID_PAKS 9
#define NUM_TA_PAKS 4
#define MAX_OSPATH 256
#define MAX_ZPATH 256
bool ql,localExists,jka,alice;
bool Com_GameIsQuakeLive(){return ql;}
bool Com_GameIsQuake3(){return !ql;}
bool Com_GameIsWolfET(){return false;}
bool Com_GameIsJKA(){return jka;}
bool Com_GameIsAlice(){return alice;}
struct comGameProfile_t{const char*baseGame;const char*officialPakStem;};
comGameProfile_t activeProfile{BASEGAME,nullptr};
const comGameProfile_t*Com_GameProfile(){return &activeProfile;}
int Q_stricmp(const char*a,const char*b){
 std::string x(a),y(b);
 for(auto&c:x)c=std::tolower(static_cast<unsigned char>(c));
 for(auto&c:y)c=std::tolower(static_cast<unsigned char>(c));
 return x.compare(y);
}
int Q_stricmpn(const char*a,const char*b,int n){
 return Q_stricmp(std::string(a).substr(0,size_t(n)).c_str(),std::string(b).substr(0,size_t(n)).c_str());
}
const char*va(const char*f,...){
 static char text[2048];va_list args;va_start(args,f);
 vsnprintf(text,sizeof(text),f,args);va_end(args);return text;
}
void Com_sprintf(char*d,int n,const char*f,...){va_list args;va_start(args,f);vsnprintf(d,n,f,args);va_end(args);}
void Com_Printf(const char*,...){}
void Q_strncpyz(char*d,const char*s,int n){snprintf(d,n,"%s",s);}
void Q_strcat(char*d,int n,const char*s){size_t used=strlen(d);assert(used<size_t(n));strncat(d,s,n-used-1);}
bool COM_CompareExtension(const char*s,const char*ext){size_t a=strlen(s),b=strlen(ext);return a>=b&&!Q_stricmp(s+a-b,ext);}
bool FS_StripExt(char*s,const char*ext){if(!COM_CompareExtension(s,ext))return false;s[strlen(s)-strlen(ext)]=0;return true;}
bool FS_SV_FileExists(const char*){return localExists;}
struct pack_t{int checksum;};
struct searchpath_t{pack_t*pack;searchpath_t*next;};
searchpath_t*fs_searchpaths=nullptr;
int fs_numServerReferencedPaks=1;
int fs_serverReferencedPaks[]={0x12345678};
const char*fs_serverReferencedPakNames[]={nullptr};
'''
        for signature in (
            'qboolean FS_FilenameCompare(', 'static qboolean FS_CheckDirTraversal(',
            'static const char *FS_GetPackExtension(', 'static qboolean FS_StripPackExtension(',
            'qboolean FS_idPak(', 'qboolean FS_OfficialPak(', 'qboolean FS_ComparePaks(',
        ):
            harness += '\n' + function(signature)
        harness += r'''
int main(){
 for(ql=false;;ql=true){
  for(int i=0;i<9;++i){
   const std::string name="baseq3/pak"+std::to_string(i);
   assert(FS_idPak(name.c_str(),BASEGAME,NUM_ID_PAKS));
   const std::string mission="missionpack/pak"+std::to_string(i);
   assert(FS_idPak(mission.c_str(),BASETA,NUM_TA_PAKS)==(i<4));
  }
  for(const char*name:{"baseq3/pak00","BASEQ3/PAK00","baseq3\\pak00"})
   assert(FS_idPak(name,BASEGAME,NUM_ID_PAKS)==ql);
  for(const char*name:{"mods/pak00","baseq3/pak000","baseq3/pak09","baseq3/pak00extra","baseq3/pak00.pk3"})
   assert(!FS_idPak(name,BASEGAME,NUM_ID_PAKS));
  assert(!FS_idPak("missionpack/pak00",BASETA,NUM_TA_PAKS));
  // The profile-owned policy answers the download paths. Quake Live adds its
  // two-digit retail pak; both profiles share Quake's base directory and so
  // keep refusing Team Arena's numbered set.
  for(const char*name:{"baseq3/pak0","baseq3/pak8","missionpack/pak0","missionpack/pak3"})
   assert(FS_OfficialPak(name));
  assert(FS_OfficialPak("baseq3/pak00")==ql);
  assert(!FS_OfficialPak("missionpack/pak4"));
  for(const char*name:{"baseq3/pak9","baseq3/mp_bin","etmain/pak0","mods/pak0",""})
   assert(!FS_OfficialPak(name));
  assert(!FS_OfficialPak(static_cast<const char*>(nullptr)));
  for(const char*name:{"baseq3/pak00","baseq3/pak00.pk3","baseq3/pak00.pak"}){
   fs_serverReferencedPakNames[0]=name;
   const std::string remote=std::string(name)+(strchr(name,'.')?"":".pk3");
   char output[1024];
   for(bool diagnostic:{false,true}){
    localExists=false;
    assert(FS_ComparePaks(output,sizeof(output),!diagnostic)==!ql);
    assert(std::string(output)==(ql?"":diagnostic?remote+"\n":"@"+remote+"@"+remote));
   }
   localExists=true;
   assert(FS_ComparePaks(output,sizeof(output),true)==!ql);
   if(!ql)assert(std::string(output).find(".12345678.")!=std::string::npos);
   pack_t present{0x12345678};searchpath_t path{&present,nullptr};fs_searchpaths=&path;
   assert(!FS_ComparePaks(output,sizeof(output),true));
   fs_searchpaths=nullptr;localExists=false;
  }
  for(const char*name:{"baseq3/pak0","baseq3/pak8.pk3","missionpack/pak3.pk3","../baseq3/pak00.pk3"}){
   fs_serverReferencedPakNames[0]=name;char output[1024];
   assert(!FS_ComparePaks(output,sizeof(output),true));
  }
  fs_serverReferencedPakNames[0]="mods/pak00.pk3";char output[1024];
  assert(FS_ComparePaks(output,sizeof(output),true));
  assert(std::string(output)=="@mods/pak00.pk3@mods/pak00.pk3");
  char tiny[8];assert(!FS_ComparePaks(tiny,sizeof(tiny),true)&&tiny[0]==0);
  if(ql)break;
 }
 // Jedi Academy names its own retail paks assets0-assets8 in base/, and its
 // own rule refuses that set under missionpack as well. Quake's numbered
 // names are not its paks, so a mod may still offer them.
 ql=false;jka=true;activeProfile={"base","assets"};
 for(int i=0;i<9;++i){
  assert(FS_OfficialPak(("base/assets"+std::to_string(i)).c_str()));
  assert(FS_OfficialPak(("missionpack/assets"+std::to_string(i)).c_str()));
  assert(!FS_OfficialPak(("base/pak"+std::to_string(i)).c_str()));
 }
 for(const char*name:{"BASE/ASSETS0","base\\assets8"})assert(FS_OfficialPak(name));
 for(const char*name:{"base/assets9","base/assets00","base/assets0extra","mods/assets0",
  "base/assets0.pk3","japlus/assets1","missionpack/pak0",""})
  assert(!FS_OfficialPak(name));
 // A server's own asset pak is refused before any download plan is made.
 fs_serverReferencedPakNames[0]="base/assets3.pk3";char jkaOutput[1024];
 assert(!FS_ComparePaks(jkaOutput,sizeof(jkaOutput),true));
 fs_serverReferencedPakNames[0]="japlus/assets3.pk3";
 assert(FS_ComparePaks(jkaOutput,sizeof(jkaOutput),true));
 activeProfile={BASEGAME,nullptr};jka=false;
 // Alice's ten retail paks in base/: the numbered ones and the suffixed
 // texture-size and language sets. A mod's pak of the same name, an empty
 // suffix and another stem stay downloadable.
 alice=true;activeProfile={"base",nullptr};
 for(const char*name:{"base/pak0","base/Pak2","base/Pak3","base/pak1_small","base/pak1_large",
  "base/pak4_english","base/pak4_french","base/pak4_german","base/pak4_spanish","base/pak5_dutch",
  "BASE\\PAK4_ENGLISH","base\\pak1_large"})
  assert(FS_OfficialPak(name));
 for(const char*name:{"base/pak1_","base/pak9_extra","base/pak10_x","base/pakx_y","mods/pak4_english",
  "base/zpak4_english","base/assets1_large",""})
  assert(!FS_OfficialPak(name));
 fs_serverReferencedPakNames[0]="base/pak4_english.pk3";char aliceOutput[1024];
 assert(!FS_ComparePaks(aliceOutput,sizeof(aliceOutput),true));
 // Every other profile keeps offering a suffixed name.
 alice=false;
 assert(!FS_OfficialPak("base/pak4_english"));
 activeProfile={BASEGAME,nullptr};
}
'''
        with tempfile.TemporaryDirectory() as folder:
            cpp = Path(folder) / 'probe.cpp'
            exe = Path(folder) / 'probe.exe'
            cpp.write_text(harness)
            build = subprocess.run([compiler, *compiler_target_args(compiler), '-std=c++17',
                                    str(cpp), '-o', str(exe)], capture_output=True, text=True)
            self.assertEqual(build.returncode, 0, build.stdout + build.stderr)
            run = subprocess.run([str(exe)], capture_output=True, text=True)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)


if __name__ == '__main__':
    unittest.main()
