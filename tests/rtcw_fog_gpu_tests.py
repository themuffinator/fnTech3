"""Execute RTCW fog selection, GPU transfers, shader math and real-tokenizer directives."""
from pathlib import Path
import os
import re
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
PREFIX += '\n'.join(re.findall(
    r'^#define GLS_(?:SRCBLEND|DSTBLEND|BLEND_BITS)[^\n]*',
    (ROOT / 'code/renderer/tr_local.h').read_text(), re.M)) + '\n'

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
struct shaderStage_t{qboolean rtcwFog;unsigned stateBits;};
struct shader_t{qboolean isSky,rtcwNoFog;};
static struct{shader_t *shader;}tess;
static struct{qboolean projection2D;struct{fnRTCWFogFrame_t rtcwFog;struct{qboolean snooperView;}rtcw;}refdef;
 struct{float aliceFarDistance;float aliceFarColor[3];qboolean aliceFarFog,aliceFarCull;}viewParms;}backEnd;
static struct{float identityLight;}tr={1.0f};
static qboolean rtcwRadialFogAvailable;
static bool enabled;static int calls;static float fogColor[4],glslColor[4],glslParams[4];
static std::map<int,float> parameters;
static void qglEnable(int cap){assert(cap==GL_FOG);enabled=true;++calls;}
static void qglDisable(int cap){assert(cap==GL_FOG);enabled=false;++calls;}
static void qglFogf(int key,float value){parameters[key]=value;++calls;}
static void qglFogfv(int key,const float *value){assert(key==GL_FOG_COLOR);memcpy(fogColor,value,16);++calls;}
static void qglHint(int target,int mode){assert(target==GL_FOG_HINT&&mode==GL_DONT_CARE);++calls;}
static void GLX_Renderer_SetRTCWFog(const float *color,const float *params){memcpy(glslColor,color,16);memcpy(glslParams,params,16);++calls;}
// tr_ritual_far_fog_render.h's surface test, tested there: whether the
// surface takes the Ritual far-fog pass, its stages then unfogged.
static qboolean ritualPass;
static qboolean RB_RitualFarFogTakesPass(const shader_t *){return ritualPass;}
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
 // American McGee's Alice's far plane: linear from the eye to the distance in
 // its colour times identityLight; a sky only when the plane culls; no 2D.
 ri.gameId=COM_GAME_ALICE;shader={};stage={};backEnd.projection2D=qfalse;rtcwRadialFogAvailable=qtrue;
 RB_RTCWStageFog(&stage);assert(!enabled&&glslColor[3]==0&&!RB_RTCWHasFog());
 backEnd.viewParms.aliceFarFog=qtrue;backEnd.viewParms.aliceFarDistance=1500;tr.identityLight=0.5f;
 backEnd.viewParms.aliceFarColor[0]=0;backEnd.viewParms.aliceFarColor[1]=0.2f;backEnd.viewParms.aliceFarColor[2]=4;
 parameters.clear();RB_RTCWStageFog(&stage);
 assert(RB_RTCWHasFog()&&enabled&&glslColor[3]==1&&parameters[GL_FOG_MODE]==GL_LINEAR);
 assert(parameters[GL_FOG_START]==0&&parameters[GL_FOG_END]==1500&&glslParams[3]==1);
 assert(fogColor[0]==0&&fogColor[1]==0.1f&&fogColor[2]==1&&fogColor[3]==1);
 // A surface taking the far-fog pass draws its stages unfogged; the view
 // keeps its fog for the rest.
 ritualPass=qtrue;RB_RTCWStageFog(&stage);assert(!enabled&&glslColor[3]==0&&RB_RTCWHasFog());
 ritualPass=qfalse;RB_RTCWStageFog(&stage);assert(enabled&&glslColor[3]==1);
 stage.stateBits=GLS_SRCBLEND_ONE|GLS_DSTBLEND_ONE;RB_RTCWStageFog(&stage);
 assert(enabled&&fogColor[1]==0&&fogColor[2]==0);stage={};
 shader.isSky=qtrue;RB_RTCWStageFog(&stage);assert(!enabled);
 backEnd.viewParms.aliceFarCull=qtrue;RB_RTCWStageFog(&stage);assert(enabled);shader.isSky=qfalse;
 backEnd.projection2D=qtrue;RB_RTCWStageFog(&stage);assert(!enabled);backEnd.projection2D=qfalse;
 backEnd.viewParms.aliceFarFog=qfalse;RB_RTCWStageFog(&stage);assert(!enabled&&!RB_RTCWHasFog());
 RB_RTCWFogToggle(qfalse);assert(!enabled);
 // The far-plane state is Alice's alone.
 backEnd.viewParms.aliceFarFog=qtrue;
 for(auto profile:{COM_GAME_QUAKE3,COM_GAME_QUAKELIVE}){
  ri.gameId=profile;calls=0;RB_RTCWStageFog(&stage);RB_RTCWFogToggle(qfalse);assert(calls==0);
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
static struct{fnRTCWFogState_t rtcwFog;struct{int time;}refdef;float identityLight;}tr;
int QDECL Com_sprintf(char *out,int size,const char *format,...){va_list args;va_start(args,format);int n=vsnprintf(out,size,format,args);va_end(args);return n;}
void Q_strncpyz(char *out,const char *in,int size){assert(size>0);snprintf(out,size,"%s",in);}
'''

PARSER_CHECKS = r'''
#include "code/renderercommon/tr_rtcw_shader_fog.h"
static int Parse(const char *keyword,const char *args){return R_RTCWShaderFogDirective(keyword,&args);}
int main(){
 for(auto profile:{COM_GAME_QUAKE3,COM_GAME_QUAKELIVE,COM_GAME_RTCW_SP,COM_GAME_RTCW_MP,COM_GAME_WOLFET}){
  ri.gameId=profile;tr={};shader={};cvars.clear();tr.refdef.time=20;tr.identityLight=0.5f;
  bool rtcw=profile==COM_GAME_RTCW_SP||profile==COM_GAME_RTCW_MP||profile==COM_GAME_WOLFET;
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
   // ET keeps the colour scaled by the identity light; RTCW MP keeps it as written.
   const float scale=profile==COM_GAME_WOLFET?0.5f:1.0f;
   assert(tr.rtcwFog.slots[4].color[0]==0.2f*scale&&tr.rtcwFog.slots[4].color[2]==0.4f*scale);
   assert(tr.rtcwFog.slots[5].color[1]==0.5f*scale&&tr.rtcwFog.slots[4].color[3]==1);
  }
  const auto saved=tr;const auto savedCvars=cvars;
  assert(Parse("waterfogvars","( 0.7 0.8 0.9 ) 0")==1);
  assert(!memcmp(&saved,&tr,sizeof(tr))&&cvars==savedCvars);
 }
}
'''

VK = r'''
#include <cstddef>
struct shaderStage_t { qboolean rtcwFog; unsigned stateBits; };
struct shader_t { qboolean isSky, rtcwNoFog; };
static struct { comGameId_t gameId; } ri;
static struct { shader_t *shader; } tess;
static struct {
 qboolean projection2D;
 struct { fnRTCWFogFrame_t rtcwFog; struct { qboolean snooperView; } rtcw; } refdef;
 struct { float modelMatrix[16]; } or;
} backEnd;
#include "code/renderercommon/tr_rtcw_fog_vk.h"
int main() {
 shader_t shader{}; shaderStage_t stage{}; tess.shader=&shader;
 auto &frame=backEnd.refdef.rtcwFog;
 frame.fog.registered=frame.skyFog.registered=qtrue;
 frame.fog.start=10; frame.fog.end=110; frame.fog.mode=FN_RTCW_FOG_LINEAR;
 frame.skyFog.mode=FN_RTCW_FOG_EXP; frame.skyFog.density=0.1f;
 frame.fog.color[0]=0.25f; frame.fog.color[1]=-1; frame.fog.color[2]=2;
 for(int i=0;i<16;++i) backEnd.or.modelMatrix[i]=float(i+1);
 for(auto profile:{COM_GAME_QUAKE3,COM_GAME_QUAKELIVE,COM_GAME_RTCW_SP,COM_GAME_RTCW_MP}) {
  ri.gameId=profile;
  bool rtcw=profile==COM_GAME_RTCW_SP||profile==COM_GAME_RTCW_MP;
  // Non-RTCW scene capture always produces a disabled snapshot.
  frame.fogEnabled=frame.skyEnabled=rtcw?qtrue:qfalse;
  for(auto sky:{qfalse,qtrue}) for(auto noFog:{qfalse,qtrue})
  for(auto stageFog:{qfalse,qtrue}) for(auto ui:{qfalse,qtrue})
  for(auto snooper:{qfalse,qtrue}) {
   shader.isSky=sky; shader.rtcwNoFog=noFog; stage.rtcwFog=stageFog;
   backEnd.projection2D=ui; backEnd.refdef.rtcw.snooperView=snooper;
   vkUniform_t data; memset(&data,0xa5,sizeof(data));
   const auto held=data; const auto snapshot=frame;
   VK_SetRTCWFogParams(&data,&stage);
   auto expected=FnRTCWFogDraw(&frame,sky,ui,noFog,stageFog,
      profile==COM_GAME_RTCW_SP?snooper:qfalse,qtrue);
   assert(!memcmp(data.rtcwFogColor,expected.color,16));
   assert(!memcmp(data.rtcwFogParams,expected.params,16));
   assert(!memcmp(&snapshot,&frame,sizeof(frame)));
   assert(!memcmp(&held,&data,offsetof(vkUniform_t,rtcwFogColor)));
   for(int row=0;row<3;++row) for(int col=0;col<4;++col)
    assert(data.rtcwFogEye[row][col]==backEnd.or.modelMatrix[col*4+row]);
   // The next 2D draw must overwrite a previously enabled fog UBO.
   backEnd.projection2D=qtrue; VK_SetRTCWFogParams(&data,nullptr);
   assert(data.rtcwFogColor[3]==0);
  }
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
        (ROOT / '.tmp').mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='rtcw-fog-gpu-', dir=ROOT / '.tmp') as tmp:
            source = Path(tmp) / 'probe.cpp'; exe = Path(tmp) / 'probe.exe'
            source.write_text(PREFIX + body)
            built = subprocess.run([compiler, *flags, '-std=c++17', '-fno-operator-names', '-I', str(ROOT), str(source), '-o', str(exe)], cwd=ROOT, capture_output=True, text=True, timeout=60)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            options = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
            ran = subprocess.run([str(exe)], cwd=ROOT, capture_output=True, text=True, timeout=30, **options)
            self.assertEqual(ran.returncode, 0, ran.stdout + ran.stderr)

    def test_pass_selection_and_equations(self):
        self.compile_run(DRAW)

    def test_compiled_shader_visibility_matches_cpu_contract(self):
        # Compile the actual shared GLSL scalar function with its vector type
        # represented as a POD, so shader/CPU equation drift is observable.
        shader = (ROOT / 'code/renderercommon/rtcw_fog.glsl').read_text()
        self.compile_run(r'''
#include <algorithm>
#include <cmath>
struct vec4 { float x,y,z,w; };
static float clamp(float v,float lo,float hi){return std::clamp(v,lo,hi);}
using std::abs;
using std::exp;
''' + shader + r'''
int main(){
 for(float mode:{0.0f,1.0f,2.0f})
 for(float start:{0.0f,10.0f,100.0f})
 for(float end:{0.0f,10.0f,100.0f})
 for(float density:{0.0f,0.01f,1.0f})
 for(float distance:{-1.0f,0.0f,5.0f,10.0f,50.0f,100.0f,10000.0f}){
  fnRTCWFogDraw_t cpu{}; cpu.color[3]=mode;
  cpu.params[0]=start;cpu.params[1]=end;cpu.params[2]=density;
  float gpu=rtcwFogVisibility({0,0,0,mode},{start,end,density,1},distance);
  assert(std::isfinite(gpu));
  assert(fabsf(gpu-FnRTCWFogVisibility(&cpu,distance))<1e-6f);
 }
}
''')

    def test_gl_and_glsl_state_transfer(self):
        self.compile_run(GL)

    def test_vulkan_uniform_transfer_and_layout(self):
        for backend in ('renderervk', 'rendererrtx'):
            with self.subTest(backend=backend):
                source = (ROOT / 'code' / backend / 'vk.h').read_text()
                begin = source.index('typedef struct vkUniform_s {')
                end = source.index('} vkUniform_t;', begin) + len('} vkUniform_t;')
                # 'or' is an engine C field name, a C++ alternative token.
                self.compile_run(source[begin:end] + VK)

    def test_original_profile_shader_fog_directives(self):
        shared = (ROOT / 'code/qcommon/q_shared.c').read_text()
        strings = '\n'.join(function(shared, sig) for sig in ('int Q_stricmpn(', 'int Q_stricmp('))
        math = (ROOT / 'code/qcommon/q_math.c').read_text()
        strings += function(math, 'int Q_isfinite(') + function(math, 'float Q_atof(')
        parser = function((ROOT / 'code/renderer/tr_shader.c').read_text(), 'static qboolean ParseVector(')
        self.compile_run(PARSER + tokenizer() + strings + parser + PARSER_CHECKS)
