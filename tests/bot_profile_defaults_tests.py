"""Execute server bot registration for both immutable profiles."""
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
        harness = r'''
#include <cassert>
#include <map>
#include <string>
constexpr int CVAR_CHEAT=512;
bool ql;
bool Com_GameIsQuakeLive(){return ql;}
std::map<std::string,std::string> values,defaults;
void Cvar_Get(const char *name,const char *value,int){
 defaults[name]=value;
 values.emplace(name,value); // Existing explicit value is retained.
}
''' + function + r'''
int main(){
 for(bool profile:{false,true}){
  ql=profile;values.clear();defaults.clear();SV_BotInitCvars();
  assert(values.at("bot_grapple")== (ql?"1":"0"));
  assert(values.at("bot_rocketjump")=="1");
  assert(values.at("bot_enable")=="1");
  values["bot_grapple"]=ql?"0":"1";SV_BotInitCvars();
  assert(values.at("bot_grapple")== (ql?"0":"1"));
  assert(defaults.at("bot_grapple")== (ql?"1":"0"));
 }
}
'''
        with tempfile.TemporaryDirectory(prefix='fntech3-bot-defaults-') as folder:
            cpp = Path(folder) / 'probe.cpp'
            exe = Path(folder) / 'probe.exe'
            cpp.write_text(harness)
            result = subprocess.run([compiler, *compiler_target_args(compiler), '-std=c++17', str(cpp), '-o', str(exe)], capture_output=True, text=True, timeout=60)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            result = subprocess.run([str(exe)], capture_output=True, text=True, timeout=15)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)


if __name__ == '__main__':
    unittest.main()
