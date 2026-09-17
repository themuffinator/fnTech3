"""Execute RTCW pass fog selection, GL parameter transfer and real-tokenizer directives."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest
from compiler_support import compiler_target_args
from mesh_tag_tests import function
from rtcw_shader_text_tests import tokenizer

ROOT = Path(__file__).resolve().parents[1]
PREFIX = r'''
#include "code/renderercommon/tr_rtcw_fog_gpu.h"
#include "code/qcommon/game_profile.h"
#include <cassert>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <map>
#include <string>
'''

DRAW = r'''
int main(){
 fnRTCWFogFrame_t frame{};frame.fogEnabled=frame.skyEnabled=qtrue;
 frame.fog.registered=frame.skyFog.registered=qtrue;
 frame.fog.mode=FN_RTCW_FOG_LINEAR;frame.fog.start=10;frame.fog.end=110;
 frame.fog.color[0]=-2;frame.fog.color[1]=0.25f;frame.fog.color[2]=7;
 frame.skyFog.mode=FN_RTCW_FOG_EXP;frame.skyFog.density=0.01f;
 const auto saved=frame;
 for(auto sky:{qfalse,qtrue})for(auto ui:{qfalse,qtrue})for(auto noFog:{qfalse,qtrue})
 for(auto stage:{qfalse,qtrue})for(auto snoop:{qfalse,qtrue})for(auto radial:{qfalse,qtrue}){
  auto draw=FnRTCWFogDraw(&frame,sky,ui,noFog,stage,snoop,radial);
  const bool active=!ui&&(!noFog||stage);
  assert((draw.color[3]!=0)==active);
  if(!active){assert(FnRTCWFogVisibility(&draw,500)==1);continue;}
  assert(draw.params[3]==float(radial));
  if(sky){assert(draw.color[3]==2);assert(fabsf(FnRTCWFogVisibility(&draw,100)-expf(-1))<1e-6f);}
  else{
   assert(draw.color[0]==0&&draw.color[1]==0.25f&&draw.color[2]==1&&draw.color[3]==1);
   assert(draw.params[2]==1); // original's zero-density default
   const float start=snoop?110.0f:10.0f,end=snoop?1110.0f:110.0f;
   assert(draw.params[0]==start&&draw.params[1]==end);
   assert(FnRTCWFogVisibility(&draw,start-1)==1&&FnRTCWFogVisibility(&draw,end+1)==0);
   assert(fabsf(FnRTCWFogVisibility(&draw,(start+end)*0.5f)-0.5f)<1e-6f);
  }
  assert(!memcmp(&saved,&frame,sizeof(frame)));
 }
 frame.fog.start=frame.fog.end=12;
 auto draw=FnRTCWFogDraw(&frame,qfalse,qfalse,qfalse,qfalse,qfalse,qfalse);
 assert(FnRTCWFogVisibility(&draw,11)==1&&FnRTCWFogVisibility(&draw,12)==0);
 frame.fogEnabled=qfalse;
 assert(FnRTCWFogDraw(&frame,qfalse,qfalse,qfalse,qfalse,qfalse,qfalse).color[3]==0);
 assert(FnRTCWFogDraw(nullptr,qfalse,qfalse,qfalse,qfalse,qfalse,qfalse).color[3]==0);
}
'''

GL = r'''
#define RENDERER_GLX
enum {GL_FOG=0xb60,GL_FOG_COLOR=0xb66,GL_FOG_MODE=0xb65,GL_FOG_START=0xb63,
 GL_FOG_END=0xb64,GL_FOG_DENSITY=0xb62,GL_FOG_HINT=0xc54,GL_DONT_CARE=0x1100,
 GL_EXP=0x800,GL_LINEAR=0x2601};
static struct{comGameId_t gameId;}ri;
struct shaderStage_t{qboolean rtcwFog;};
struct shader_t{qboolean isSky,rtcwNoFog;};
static struct{shader_t *shader;}tess;
static struct{qboolean projection2D;struct{fnRTCWFogFrame_t rtcwFog;struct{qboolean snooperView;}rtcw;}refdef;}backEnd;
static qboolean rtcwRadialFogAvailable;
static bool enabled;static int calls;static float fogColor[4],glslColor[4],glslParams[4];
static std::map<int,float> parameters;
static void qglEnable(int cap){assert(cap==GL_FOG);enabled=true;++calls;}
static void qglDisable(int cap){assert(cap==GL_FOG);enabled=false;++calls;}
static void qglFogf(int key,float value){parameters[key]=value;++calls;}
static void qglFogfv(int key,const float *value){assert(key==GL_FOG_COLOR);memcpy(fogColor,value,16);++calls;}
static void qglHint(int target,int mode){assert(target==GL_FOG_HINT&&mode==GL_DONT_CARE);++calls;}
static void GLX_Renderer_SetRTCWFog(const float *color,const float *params){memcpy(glslColor,color,16);memcpy(glslParams,params,16);++calls;}
#include "code/renderer/tr_rtcw_fog_gl.h"
int main(){
 shader_t shader{};shaderStage_t stage{};tess.shader=&shader;
 auto &frame=backEnd.refdef.rtcwFog;frame.fogEnabled=frame.skyEnabled=qtrue;
 frame.fog.registered=frame.skyFog.registered=qtrue;
 frame.fog.mode=FN_RTCW_FOG_LINEAR;frame.fog.start=10;frame.fog.end=110;
 frame.fog.color[0]=0.25f;frame.fog.color[1]=0.5f;frame.fog.color[2]=0.75f;
 frame.skyFog.mode=FN_RTCW_FOG_EXP;frame.skyFog.density=0.125f;
 for(auto profile:{COM_GAME_QUAKE3,COM_GAME_QUAKELIVE}){
  ri.gameId=profile;calls=0;RB_RTCWStageFog(&stage);RB_RTCWFogToggle(qfalse);assert(calls==0);
 }
 for(auto profile:{COM_GAME_RTCW_SP,COM_GAME_RTCW_MP}){
  ri.gameId=profile;shader={};stage={};backEnd.projection2D=qfalse;backEnd.refdef.rtcw.snooperView=qfalse;
  for(auto radial:{qfalse,qtrue}){
   rtcwRadialFogAvailable=radial;parameters.clear();RB_RTCWStageFog(&stage);
   assert(enabled&&glslColor[3]==1&&parameters[GL_FOG_MODE]==GL_LINEAR);
   assert(parameters[GL_FOG_START]==10&&parameters[GL_FOG_END]==110&&parameters[GL_FOG_DENSITY]==1);
   assert(fogColor[3]==1&&fogColor[0]==0.25f&&glslParams[3]==float(radial));
   assert(bool(parameters.count(0x855a))==bool(radial));
  }
  shader.rtcwNoFog=qtrue;RB_RTCWStageFog(&stage);assert(!enabled&&glslColor[3]==0);
  stage.rtcwFog=qtrue;RB_RTCWStageFog(&stage);assert(enabled&&glslColor[3]==1);
  RB_RTCWFogToggle(qfalse);assert(!enabled&&glslColor[3]==0);
  RB_RTCWFogToggle(qtrue);assert(enabled); // original plain-light temporary disable/restore
  backEnd.refdef.rtcw.snooperView=qtrue;RB_RTCWStageFog(&stage);
  assert(parameters[GL_FOG_START]==(profile==COM_GAME_RTCW_SP?110:10));
  assert(parameters[GL_FOG_END]==(profile==COM_GAME_RTCW_SP?1110:110));
  shader.isSky=qtrue;RB_RTCWStageFog(&stage);assert(enabled&&glslColor[3]==2&&parameters[GL_FOG_DENSITY]==0.125f);
  backEnd.projection2D=qtrue;RB_RTCWStageFog(&stage);RB_RTCWFogToggle(qtrue);assert(!enabled&&glslColor[3]==0);
 }
}
'''

PARSER = r'''
static char com_token[MAX_TOKEN_CHARS],com_parsename[MAX_TOKEN_CHARS];
static int com_lines,com_tokenline;
static std::map<std::string,std::string> cvars;
static void Set(const char *name,const char *value){cvars[name]=value;}
static void QDECL Print(int,const char *,...){}
static struct{comGameId_t gameId;decltype(&Print)Printf=Print;decltype(&Set)Cvar_Set=Set;}ri;
static struct{qboolean rtcwNoFog;char name[32];}shader;
static struct{fnRTCWFogState_t rtcwFog;struct{int time;}refdef;}tr;
int QDECL Com_sprintf(char *out,int size,const char *format,...){va_list args;va_start(args,format);int n=vsnprintf(out,size,format,args);va_end(args);return n;}
void Q_strncpyz(char *out,const char *in,int size){assert(size>0);snprintf(out,size,"%s",in);}
'''

PARSER_CHECKS = r'''
#include "code/renderercommon/tr_rtcw_shader_fog.h"
static int Parse(const char *keyword,const char *args){return R_RTCWShaderFogDirective(keyword,&args);}
int main(){
 for(auto profile:{COM_GAME_QUAKE3,COM_GAME_QUAKELIVE,COM_GAME_RTCW_SP,COM_GAME_RTCW_MP}){
  ri.gameId=profile;tr={};shader={};cvars.clear();tr.refdef.time=20;
  bool rtcw=profile==COM_GAME_RTCW_SP||profile==COM_GAME_RTCW_MP;
  assert(Parse("NoFog","")==int(rtcw)&&bool(shader.rtcwNoFog)==rtcw);
  assert(Parse("fogparms","( 1 0 0 ) 100")==0);
  if(!rtcw){assert(Parse("fogvars","( 1 0 0 ) 100")==0&&cvars.empty());continue;}
  assert(Parse("skyfogvars","( 0.2 0.3 0.4 ) 0.5")==1);
  assert(tr.rtcwFog.slots[1].registered&&tr.rtcwFog.slots[1].density==0.5f);
  assert(Parse("skyfogvars","( 0 0 0 ) 2")==-1);
  for(const char *bad:{"", "( 1 2 ) 3", "( 1 2 3 )", "( 1 2 3 ) nan", "( 1 2 3 ) -1", "( 1 2 3 ) 2147483647", "( 1 2 3 ) 12oops"}){
   const auto held=tr;const auto heldCvars=cvars;
   assert(Parse("fogvars",bad)==-1);assert(!memcmp(&held,&tr,sizeof(tr))&&heldCvars==cvars);
  }
  assert(Parse("fogvars","( 0.2 0.3 0.4 ) 1000")==1);
  assert(Parse("waterfogvars","( 0.4 0.5 0.6 ) 300")==1);
  if(profile==COM_GAME_RTCW_SP){
   assert(cvars["r_mapFogColor"]=="0 1000 1000.000000 0.200000 0.300000 0.400000 0");
   assert(cvars["r_waterFogColor"]=="0 300 1.100000 0.400000 0.500000 0.600000 200");
   assert(!tr.rtcwFog.slots[4].registered&&!tr.rtcwFog.slots[5].registered);
  }else{
   assert(cvars.empty()&&tr.rtcwFog.slots[4].end==1000&&tr.rtcwFog.slots[5].end==300);
   assert(tr.rtcwFog.selected==4&&tr.rtcwFog.slots[9].finishTime==70);
  }
  const auto saved=tr;const auto savedCvars=cvars;
  assert(Parse("waterfogvars","( 0.7 0.8 0.9 ) 0")==1);
  assert(!memcmp(&saved,&tr,sizeof(tr))&&cvars==savedCvars);
 }
}
'''


class RTCWFogGPUTests(unittest.TestCase):
    def compile_run(self, body):
        compiler = shutil.which('clang++')
        if not compiler:
            self.skipTest('Clang unavailable')
        flags = compiler_target_args(compiler)
        if os.name == 'nt' and not flags:
            flags = ['-DFNTECH3_Q3_ONLY']
        elif os.name != 'nt':
            flags = ['-m32']
        with tempfile.TemporaryDirectory(prefix='rtcw-fog-gpu-', dir=ROOT / '.tmp') as tmp:
            source = Path(tmp) / 'probe.cpp'; exe = Path(tmp) / 'probe.exe'
            source.write_text(PREFIX + body)
            built = subprocess.run([compiler, *flags, '-std=c++17', '-I', str(ROOT), str(source), '-o', str(exe)], cwd=ROOT, capture_output=True, text=True, timeout=60)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            options = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
            ran = subprocess.run([str(exe)], cwd=ROOT, capture_output=True, text=True, timeout=30, **options)
            self.assertEqual(ran.returncode, 0, ran.stdout + ran.stderr)

    def test_pass_selection_and_equations(self):
        self.compile_run(DRAW)

    def test_gl_and_glsl_state_transfer(self):
        self.compile_run(GL)

    def test_original_profile_shader_fog_directives(self):
        shared = (ROOT / 'code/qcommon/q_shared.c').read_text()
        strings = '\n'.join(function(shared, sig) for sig in ('int Q_stricmpn(', 'int Q_stricmp('))
        math = (ROOT / 'code/qcommon/q_math.c').read_text()
        strings += function(math, 'int Q_isfinite(') + function(math, 'float Q_atof(')
        parser = function((ROOT / 'code/renderer/tr_shader.c').read_text(), 'static qboolean ParseVector(')
        self.compile_run(PARSER + tokenizer() + strings + parser + PARSER_CHECKS)
