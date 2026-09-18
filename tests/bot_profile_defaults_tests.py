"""Execute server bot registration for all immutable profiles."""
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path
from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]


class BotProfileDefaultsTests(unittest.TestCase):
    def test_grapple_default_and_explicit_override(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++ compiler required')
        source = (ROOT / 'code/server/sv_bot.cpp').read_text()
        start = source.index('void SV_BotInitCvars(void)')
        function = source[start:source.index('\n}', start) + 2]
        catalog = (ROOT / 'code/qcommon/game_profile.cpp').read_text()
        catalog = catalog[catalog.index('const comGameProfile_t profiles[]'):catalog.index('\nstatic_assert(')]
        harness = r'''
#include "code/qcommon/game_profile.h"
#include <cassert>
#include <map>
#include <string>
constexpr int CVAR_CHEAT=512;
const comGameProfile_t *profile;
const comGameProfile_t *Com_GameProfile(){return profile;}
int Com_GameIsQuakeLive(){return profile->id==COM_GAME_QUAKELIVE;}
std::map<std::string,std::string> values,defaults;
void Cvar_Get(const char *name,const char *value,int){
 defaults[name]=value;
 values.emplace(name,value); // Existing explicit value is retained.
}
''' + catalog + function + r'''
int main(){
 for(const auto &p:profiles){
  profile=&p;bool ql=Com_GameIsQuakeLive(),disabled=p.id==COM_GAME_RTCW_MP||p.id==COM_GAME_WOLFET||p.id==COM_GAME_STVEF_SP;
  values.clear();defaults.clear();SV_BotInitCvars();
  assert(values.at("bot_grapple")== (ql?"1":"0"));
  assert(values.at("bot_rocketjump")=="1");
  assert(values.at("bot_enable")== (disabled?"0":"1"));
  values["bot_enable"]=disabled?"1":"0";
  values["bot_grapple"]=ql?"0":"1";SV_BotInitCvars();
  assert(values.at("bot_grapple")== (ql?"0":"1"));
  assert(defaults.at("bot_grapple")== (ql?"1":"0"));
  assert(values.at("bot_enable")== (disabled?"1":"0"));
  assert(defaults.at("bot_enable")== (disabled?"0":"1"));
 }
}
'''
        with tempfile.TemporaryDirectory(prefix='fntech3-bot-defaults-') as folder:
            cpp = Path(folder) / 'probe.cpp'
            exe = Path(folder) / 'probe.exe'
            cpp.write_text(harness)
            result = subprocess.run([compiler, *compiler_target_args(compiler), '-std=c++17', '-I', str(ROOT), str(cpp), '-o', str(exe)], cwd=ROOT, capture_output=True, text=True, timeout=60)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            result = subprocess.run([str(exe)], capture_output=True, text=True, timeout=15)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)


if __name__ == '__main__':
    unittest.main()
