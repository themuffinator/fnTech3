"""Execute profile-owned appearance registration and the model command."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
from compiler_support import compiler_target_args
from rtcw_server_world_tests import function

ROOT = Path(__file__).resolve().parents[1]


class ProfileAppearanceTests(unittest.TestCase):
    def test_defaults_keys_archive_policy_and_existing_user_choices(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++17 compiler required')
        catalog = (ROOT / 'code/qcommon/game_profile.cpp').read_text()
        client = (ROOT / 'code/client/cl_main.cpp').read_text()
        source = r'''
#include "code/qcommon/game_profile.h"
#include <array>
#include <cassert>
#include <cstring>
#include <map>
#include <string>
constexpr int CVAR_USERINFO=2,CVAR_ARCHIVE=1,CVAR_ARCHIVE_ND=CVAR_ARCHIVE|0x00400000,MAX_CVAR_VALUE_STRING=256;
struct Value{std::string value,reset;int flags;};
static std::map<std::string,Value> cvars;
static const comGameProfile_t *profile;
const comGameProfile_t *Com_GameProfile(){return profile;}
static Value *Cvar_Get(const char *name,const char *value,int flags){
 auto result=cvars.emplace(name,Value{value,value,flags});result.first->second.flags|=flags;
 result.first->second.reset=value;return &result.first->second;
}
static void Cvar_Set(const char *name,const char *value){assert(cvars.count(name));cvars.at(name).value=value;}
static void Cvar_VariableStringBuffer(const char *name,char *out,int capacity){auto s=cvars.at(name).value;
 assert(s.size()<size_t(capacity));std::memcpy(out,s.c_str(),s.size()+1);}
static const char *argument="";static int messages;
static const char *Cmd_Argv(int i){assert(i==1);return argument;}
static void Com_Printf(const char*,const char*){++messages;}
'''
        source += catalog[catalog.index('const comGameProfile_t profiles[]'):catalog.index('\nstatic_assert(')]
        source += function(client, 'static void CL_RegisterAppearanceCvars(')
        source += function(client, 'static void CL_SetModel_f(')
        source += r'''
int main(){
 for(unsigned i=0;i<4;++i){profile=&profiles[i];bool rtcw=i>=2;const char *head=rtcw?"head":"headmodel";
  cvars.clear();CL_RegisterAppearanceCvars();assert(cvars.size()==(rtcw?3:6));
  assert(cvars.at("model").value==(i==2?"bj2":i==3?"multi":"sarge"));
  assert(cvars.at(head).value==(rtcw?"default":"sarge"));
  assert(cvars.at(rtcw?"color":"color1").value=="4");
  assert(cvars.at("model").flags==(CVAR_USERINFO|(rtcw?CVAR_ARCHIVE:CVAR_ARCHIVE_ND)));
  assert(cvars.at(head).flags==cvars.at("model").flags);
  if(!rtcw){assert(cvars.at("team_model").value=="sarge"&&cvars.at("team_headmodel").value=="sarge");
   assert(cvars.at("color2").value=="5");}
  cvars.at("model").value="custom/body";cvars.at(head).value="custom/head";
  CL_RegisterAppearanceCvars();assert(cvars.at("model").value=="custom/body"&&cvars.at(head).value=="custom/head");
  argument="chosen";CL_SetModel_f();assert(cvars.at("model").value=="chosen");
  assert(cvars.at(head).value==(rtcw?"custom/head":"chosen"));
  auto before=cvars;argument="";int old=messages;CL_SetModel_f();assert(messages==old+1&&cvars.size()==before.size());
  for(const auto &pair:before){auto &actual=cvars.at(pair.first);
   assert(actual.value==pair.second.value&&actual.reset==pair.second.reset&&actual.flags==pair.second.flags);}
 }
}
'''
        with tempfile.TemporaryDirectory(prefix='profile-appearance-') as directory:
            cpp = Path(directory) / 'probe.cpp'; exe = Path(directory) / 'probe.exe'
            cpp.write_text(source, encoding='utf-8')
            built = subprocess.run([compiler, *compiler_target_args(compiler), '-std=c++17', '-O2',
                                    '-I', str(ROOT), str(cpp), '-o', str(exe)],
                                   cwd=ROOT, capture_output=True, text=True, timeout=60)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            run = subprocess.run([str(exe)], cwd=ROOT, capture_output=True, text=True, timeout=20)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)


if __name__ == '__main__':
    unittest.main()
