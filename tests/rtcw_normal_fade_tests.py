"""RTCW normal-based alpha, optional shader arguments and renderer dispatch."""
from pathlib import Path
import os
import re
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args
from model_shader_tests import common
from rtcw_shader_text_tests import tokenizer
from mesh_tag_tests import function

ROOT = Path(__file__).resolve().parents[1]
RENDERERS = ('renderer', 'renderervk', 'rendererrtx')
HOST = r'''
#include <cmath>
#include <limits>
static char com_token[MAX_TOKEN_CHARS],com_parsename[MAX_TOKEN_CHARS];
static int com_lines,com_tokenline;
int QDECL Com_sprintf(char *out,int size,const char *format,...) {
 va_list args;va_start(args,format);int n=std::vsnprintf(out,size,format,args);va_end(args);return n;
}
#include "code/renderercommon/tr_rtcw_normal_fade.h"
#include "code/renderercommon/tr_rtcw_normal_fade_parser.h"
struct Fade { color4ub_t constantColor;float rtcwNormalZBounds[2]; };
struct Stage:Fade {Fade bundle[3];};
static struct { int numVertexes;float normal[5][4];struct {color4ub_t colors[5];}svars;}tess;
static struct Entity {refEntity_t e;}entity;
static struct {Entity *currentEntity;}backEnd{&entity};
'''

CHECKS = r'''
int main(){
 const float bounds[]={-1,1};
 refEntity_t e{};e.shader.rgba[3]=255;
 for(int i=0;i<3;++i)e.axis[i][i]=1;
 const auto original=e;
 const float normals[][3]={{0,0,-1},{0,0,-.5f},{1,0,0},{0,0,.5f},{0,0,1}};
 const int expected[]={0,127,255,127,0};
 for(int model:{0,1})for(int i=0;i<5;++i){e.hModel=model;assert(R_RTCWNormalZAlpha(&e,normals[i],bounds,255)==expected[i]);}
 e.hModel=0;assert(!std::memcmp(&e,&original,sizeof(e)));
 e.shader.rgba[3]=128;e.hModel=1;
 assert(R_RTCWNormalZAlpha(&e,normals[2],bounds,255)==128);
 e.hModel=0;assert(R_RTCWNormalZAlpha(&e,normals[2],bounds,255)==255);
 e.hModel=1;
 // A model's axis transforms the rise direction into its local coordinates.
 e.axis[0][0]=0;e.axis[0][2]=1;e.axis[2][2]=0;e.axis[2][0]=-1;
 assert(R_RTCWNormalZAlpha(&e,normals[2],bounds,255)==0);
 assert(R_RTCWNormalZAlpha(&e,normals[0],bounds,255)==128);
 e.hModel=0;e.rtcw.fireRiseDir[0]=1;
 assert(R_RTCWNormalZAlpha(&e,normals[2],bounds,255)==0);
 const float sentinel[][2]={{-1000,1},{-1,-1000},{-1000,-1000}};
 e.rtcw.fireRiseDir[0]=0;
 for(auto &b:sentinel){assert(R_RTCWNormalZAlpha(&e,normals[0],b,1)==255);assert(R_RTCWNormalZAlpha(&e,normals[2],b,1)==192);assert(R_RTCWNormalZAlpha(&e,normals[4],b,1)==128);}
 for(auto b:std::vector<std::vector<float>>{{0,0},{1,-1},{-INFINITY,INFINITY},{NAN,1}})
  assert(R_RTCWNormalZAlpha(&e,normals[2],b.data(),255)==0);
 float invalidNormal[]={0,0,NAN};assert(R_RTCWNormalZAlpha(&e,invalidNormal,bounds,255)==0);
 // Optional alpha/default bounds and braces do not consume the next stage.
 for(const char *input:{"", "\nnext", "}"}){
  const char *p=input;byte alpha=0;float b[2]={0,0};
  assert(R_RTCWParseNormalZFade(&p,&alpha,b)&&alpha==255&&b[0]==-1&&b[1]==1);
  if(*input=='}')assert(p==input);
 }
 for(const char *input:{".5\nnext", ".5 }"}){
  const char *p=input;byte alpha=0;float b[2]={0,0};
  assert(R_RTCWParseNormalZFade(&p,&alpha,b)&&alpha==127&&b[0]==-1&&b[1]==1);
  assert(!strcmp(COM_ParseExt(&p,qtrue),*input=='.'&&strchr(input,'}')?"}":"next"));
 }
 const char *p="1 -1000 .25 }";byte alpha=0;float b[2]={0,0};
 assert(R_RTCWParseNormalZFade(&p,&alpha,b)&&alpha==255&&b[0]==-1000&&b[1]==.25f);
 assert(!strcmp(COM_ParseExt(&p,qtrue),"}"));
 for(const char *bad:{"nan","inf","1junk","-1","2","1e100",".5 -1\n}",".5 -1 }",".5 nan 1"}){
  p=bad;alpha=17;b[0]=18;b[1]=19;assert(!R_RTCWParseNormalZFade(&p,&alpha,b));
  assert(p==bad&&alpha==17&&b[0]==18&&b[1]==19);
 }
 // Actual backend case: preserve RGB and apply the correct texture bundle.
 entity.e={};entity.e.shader.rgba[3]=255;tess.numVertexes=5;
 Stage stage{};stage.constantColor.rgba[3]=255;stage.rtcwNormalZBounds[0]=-1;stage.rtcwNormalZBounds[1]=1;
 for(auto &bundle:stage.bundle){bundle.constantColor.rgba[3]=255;bundle.rtcwNormalZBounds[0]=-1;bundle.rtcwNormalZBounds[1]=1;}
 for(int i=0;i<5;++i){std::memcpy(tess.normal[i],normals[i],sizeof(normals[i]));for(int c=0;c<4;++c)tess.svars.colors[i].rgba[c]=43;}
 Dispatch(&stage);
 for(int i=0;i<5;++i){assert(tess.svars.colors[i].rgba[3]==expected[i]);for(int c=0;c<3;++c)assert(tess.svars.colors[i].rgba[c]==43);}
}
'''


class RTCWNormalFadeTests(unittest.TestCase):
    def test_parser_alpha_and_backend_dispatch(self):
        compiler = shutil.which('clang++')
        if not compiler:
            self.skipTest('Clang unavailable')
        target = compiler_target_args(compiler)
        if os.name == 'nt' and not target:
            target = ['-DFNTECH3_Q3_ONLY']
        elif os.name != 'nt':
            target = ['-m32']
        with tempfile.TemporaryDirectory(prefix='rtcw-normal-fade-', dir=ROOT / '.tmp') as temporary:
            for renderer in RENDERERS:
                with self.subTest(renderer=renderer):
                    shader = (ROOT / 'code' / renderer / 'tr_shader.c').read_text()
                    parser = function(shader, 'static qboolean ParseStage(')
                    self.assertIn('R_RTCWParseNormalZFade(', parser)
                    needs = function(shader, 'static void DetectNeeds(')
                    self.assertIn('AGEN_RTCW_NORMALZFADE', needs)
                    self.assertIn('rtcwNormalZBounds', function(shader, 'static qboolean CollapseMultitexture(')
                                  if renderer == 'renderer' else shader[shader.index('nonIdenticalColors = qfalse;'):shader.index('if ( nonIdenticalColors )')])
                    shade = (ROOT / 'code' / renderer / 'tr_shade.c').read_text()
                    first = shade.index('case AGEN_RTCW_NORMALZFADE:')
                    body = shade[first:shade.index('case AGEN_PORTAL:', first)]
                    body = body.replace('case AGEN_RTCW_NORMALZFADE:', 'case 1:')
                    dispatch = 'static void Dispatch(const Stage *pStage){int i,b=1;auto *dest=tess.svars.colors;switch(1){' + body + '}}\n'
                    if renderer == 'renderer':
                        self.assertIn('AGEN_RTCW_NORMALZFADE ) return qfalse', function(shade, 'static qboolean GLX_StreamDrawStageMaterialAllowed('))
                    cpp = Path(temporary) / f'{renderer}.cpp'
                    executable = cpp.with_suffix('.exe')
                    cpp.write_text(common(renderer) + HOST + tokenizer() + dispatch + CHECKS, encoding='utf-8')
                    build = subprocess.run([compiler, *target, '-std=c++17', '-O2', '-I', str(ROOT), str(cpp), '-o', str(executable)],
                                           cwd=ROOT, capture_output=True, text=True, timeout=60)
                    self.assertEqual(build.returncode, 0, build.stdout + build.stderr)
                    run = subprocess.run([str(executable)], cwd=ROOT, capture_output=True, text=True, timeout=30)
                    self.assertEqual(run.returncode, 0, run.stdout + run.stderr)


if __name__ == '__main__':
    unittest.main()
