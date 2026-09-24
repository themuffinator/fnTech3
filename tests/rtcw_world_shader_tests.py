"""Execute RTCW sky/world material metadata with the engine tokenizer."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest
from compiler_support import compiler_target_args
from rtcw_shader_text_tests import PRELUDE, tokenizer

ROOT = Path(__file__).resolve().parents[1]

CHECKS = r'''
#include <cctype>
#include "code/renderercommon/tr_rtcw_world_shader.h"
int Q_stricmp(const char *a,const char *b) {
 while(*a&&*b&&std::tolower((unsigned char)*a)==std::tolower((unsigned char)*b)){++a;++b;}
 return std::tolower((unsigned char)*a)-std::tolower((unsigned char)*b);
}
int main(){
 for(auto profile:{COM_GAME_QUAKE3,COM_GAME_QUAKELIVE,COM_GAME_RTCW_SP,COM_GAME_RTCW_MP}){
  const bool rtcw=profile==COM_GAME_RTCW_SP||profile==COM_GAME_RTCW_MP;
  fnRTCWWorldShader_t state{};
  for(const char *directive:{"sunshader","SUNSHADER","lightgridmulamb","lightgridmuldir"}){
   const char *text="synthetic/sun next",*before=text;
   if(directive[0]=='l')text=before="2.5 next";
   assert(FnRTCWWorldShaderDirective(profile,directive,&text,&state)==int(rtcw));
   if(rtcw)assert(!strcmp(COM_ParseExt(&text,qtrue),"next"));else assert(text==before);
  }
  if(!rtcw){assert(!state.sunShader[0]&&!state.ambient&&!state.directed);continue;}
  assert(!strcmp(state.sunShader,"synthetic/sun")&&state.ambient==2.5f&&state.directed==2.5f);
  const auto saved=state;
  for(const char *directive:{"sunshader","lightgridmulamb","lightgridmuldir"}){
   for(const char *bad:{"", "\nnext", "}", "{"}){
    const char *p=bad;assert(FnRTCWWorldShaderDirective(profile,directive,&p,&state)==-1);
    assert(p==bad&&!memcmp(&saved,&state,sizeof(state)));
   }
  }
  for(const char *bad:{"NaN", "inf", "-inf", "1e1000", "1x", "word"}){
   const char *p=bad;assert(FnRTCWWorldShaderDirective(profile,"lightgridmulamb",&p,&state)==-1);
   assert(!memcmp(&saved,&state,sizeof(state)));
  }
  for(const char *value:{"0","-2"}){
   const char *p=value;assert(FnRTCWWorldShaderDirective(profile,"lightgridmulamb",&p,&state)==1);
   assert(!memcmp(&saved,&state,sizeof(state)));
  }
  std::string longName(MAX_QPATH,'a');const char *p=longName.c_str();
  assert(FnRTCWWorldShaderDirective(profile,"sunshader",&p,&state)==-1);
  assert(!memcmp(&saved,&state,sizeof(state)));
  p="ignored";const char *before=p;
  assert(!FnRTCWWorldShaderDirective(profile,"other",&p,&state)&&p==before);
  fnRTCWWorldShader_t world{};FnRTCWWorldShaderMerge(&world,&state);
  assert(!memcmp(&world,&state,sizeof(state)));
  fnRTCWWorldShader_t empty{};FnRTCWWorldShaderMerge(&world,&empty);
  assert(!memcmp(&world,&state,sizeof(state))); // Absent values do not reset a sky.
  world.ambient=0.5f;world.directed=2;
  float ambient[]={10,20,30},directed[]={1,2,3};FnRTCWWorldGridScale(&world,ambient,directed);
  assert(ambient[0]==5&&ambient[1]==10&&ambient[2]==15);
  assert(directed[0]==2&&directed[1]==4&&directed[2]==6);
  world={};FnRTCWWorldGridScale(&world,ambient,directed);
  assert(ambient[2]==15&&directed[2]==6); // A new map has neutral multipliers.
 }
}
'''


class RTCWWorldShaderTests(unittest.TestCase):
    def test_world_directives_bounds_and_profile_isolation(self):
        compiler = shutil.which('clang++')
        if not compiler:
            self.skipTest('Clang unavailable')
        target = compiler_target_args(compiler)
        if os.name == 'nt' and not target:
            target = ['-DFNTECH3_Q3_ONLY']
        elif os.name != 'nt':
            target = ['-m32']
        (ROOT / '.tmp').mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='rtcw-world-', dir=ROOT / '.tmp') as temp:
            folder = Path(temp); source = folder / 'probe.cpp'; exe = folder / 'probe.exe'
            source.write_text(PRELUDE + tokenizer() + CHECKS)
            build = subprocess.run([compiler, *target, '-std=c++17', '-O2', '-I', str(ROOT),
                str(source), '-o', str(exe)], cwd=folder, capture_output=True, text=True, timeout=60)
            self.assertEqual(build.returncode, 0, build.stdout + build.stderr)
            run = subprocess.run([str(exe)], cwd=folder, capture_output=True, text=True, timeout=15)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)
        for renderer in ('renderer', 'renderervk', 'rendererrtx'):
            root = ROOT / 'code' / renderer
            self.assertIn('FnRTCWWorldShaderDirective( ri.gameId, token, text, &shader.rtcwWorld )', (root / 'tr_shader.c').read_text())
            bsp = (root / 'tr_bsp.c').read_text()
            self.assertIn('FnRTCWWorldShaderMerge( &tr.rtcwWorld, &shader->rtcwWorld )', bsp)
            self.assertIn('Com_Memset( &tr.rtcwWorld, 0, sizeof( tr.rtcwWorld ) )', bsp)
            self.assertLess(bsp.index('R_LoadLightGrid( &header'), bsp.index('R_FindShader( tr.rtcwWorld.sunShader'))
            self.assertIn('FnRTCWWorldGridScale( &tr.rtcwWorld, ent->ambientLight, ent->directedLight )', (root / 'tr_light.c').read_text())


if __name__ == '__main__':
    unittest.main()
