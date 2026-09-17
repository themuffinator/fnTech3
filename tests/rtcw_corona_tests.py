"""Execute owned RTCW corona scenes, original fade policy and backend quad submission."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest
from compiler_support import compiler_target_args

ROOT=Path(__file__).resolve().parents[1]
HOST=r'''
#include "code/qcommon/q_shared.h"
#include "code/qcommon/game_profile.h"
#include "code/renderercommon/tr_rtcw_corona.h"
#include <cassert>
#include <cstring>
#include <climits>
#include <array>
#include <vector>
#define Com_Memset memset
#define Com_Memcpy memcpy
struct shader_t { int id; };
struct fog_t { vec3_t bounds[2]; };
struct world_t { int numfogs; fog_t *fogs; };
struct dlight_t { vec3_t origin,color; };
struct refdef_t { int time,numRTCWCoronas;fnRTCWCorona_t rtcwCoronas[32];unsigned num_dlights;dlight_t *dlights; };
struct view_t { int frameCount,frameSceneNum,portalView,viewportX,viewportY,viewportWidth,viewportHeight;float projectionMatrix[16]; };
static struct { world_t *world;refdef_t refdef;float identityLight;shader_t *flareShader,*rtcwSpotFlareShader;int registered; } tr;
static struct { refdef_t refdef;view_t viewParms;struct {float modelMatrix[16];}or;struct {int c_flareAdds,c_flareTests,c_flareRenders;}pc; } backEnd;
static void Error(int,const char *,...){throw 7;}
static struct { comGameId_t gameId;decltype(&Error) Error; }ri{COM_GAME_RTCW_SP,Error};
struct cvar {int integer;float value;};static cvar flares{1,1},fade{5,5},size{40,40};
static cvar *r_flares=&flares,*r_flareFade=&fade,*r_flareSize=&size;
static void R_TransformModelToClip(const vec3_t point,const float *,const float *,vec4_t eye,vec4_t clip){
 eye[2]=point[2];clip[0]=point[0];clip[1]=point[1];clip[2]=point[2];clip[3]=1000;
}
static void R_TransformClipToWindow(const vec4_t clip,const view_t *,vec4_t,vec4_t window){window[0]=320+clip[0];window[1]=240+clip[1];}
static int chosenShader,chosenFog,ends;
struct Quad {float x,y,w,h;color4ub_t color;int shader,fog;};static std::vector<Quad> quads;
static void RB_BeginSurface(shader_t *shader,int fog){chosenShader=shader->id;chosenFog=fog;}
static void RB_AddQuadStamp2(float x,float y,float w,float h,float s1,float t1,float s2,float t2,color4ub_t color){
 assert(s1==0&&t1==0&&s2==1&&t2==1);quads.push_back({x,y,w,h,color,chosenShader,chosenFog});
}
static void RB_EndSurface(){++ends;}
#include "code/renderercommon/tr_rtcw_corona_scene.h"
#include "code/renderercommon/tr_rtcw_corona_backend.h"
template<class F>void Reject(F action){bool caught=false;try{action();}catch(int){caught=true;}assert(caught);}
'''

SCENE=r'''
int main(){
 fnRTCWCorona_t corona{{1,2,3},{0.25f,0.5f,1},2,-7,3};
 for(auto profile:{COM_GAME_RTCW_SP,COM_GAME_RTCW_MP}){
  ri.gameId=profile;rtcwCoronaCount=rtcwCoronaFirst=0;tr.registered=0;
  RE_AddRTCWCoronaToScene(&corona);assert(rtcwCoronaCount==0);tr.registered=1;
  for(int i=0;i<40;++i){corona.id=i;RE_AddRTCWCoronaToScene(&corona);}assert(rtcwCoronaCount==32);
  R_CopyRTCWCoronasToScene();assert(tr.refdef.numRTCWCoronas==32&&rtcwCoronaFirst==32);
  const auto saved=tr.refdef;corona.color[0]=0.75f;assert(tr.refdef.rtcwCoronas[0].color[0]==0.25f);
  R_CopyRTCWCoronasToScene();assert(tr.refdef.numRTCWCoronas==0);
  rtcwCoronaCount=rtcwCoronaFirst=0;RE_AddRTCWCoronaToScene(&corona);rtcwCoronaFirst=rtcwCoronaCount;
  corona.id=99;RE_AddRTCWCoronaToScene(&corona);R_CopyRTCWCoronasToScene();assert(tr.refdef.numRTCWCoronas==1&&tr.refdef.rtcwCoronas[0].id==99);
  assert(saved.rtcwCoronas[0].color[0]==0.25f);corona.color[0]=0.25f;
  Reject([&]{RE_AddRTCWCoronaToScene(nullptr);});corona.origin[1]=NAN;Reject([&]{RE_AddRTCWCoronaToScene(&corona);});corona.origin[1]=2;
  corona.scale=INFINITY;Reject([&]{RE_AddRTCWCoronaToScene(&corona);});corona.scale=2;
 }
 for(auto profile:{COM_GAME_QUAKE3,COM_GAME_QUAKELIVE}){
  ri.gameId=profile;Reject([&]{RE_AddRTCWCoronaToScene(&corona);});R_CopyRTCWCoronasToScene();assert(tr.refdef.numRTCWCoronas==0);
 }
}
'''

BACKEND=r'''
int main(){
 shader_t normal{17},spot{18};tr.flareShader=&normal;tr.rtcwSpotFlareShader=&spot;tr.identityLight=0.5f;
 fog_t fogs[2]{};for(int i=0;i<3;++i){fogs[1].bounds[0][i]=-999;fogs[1].bounds[1][i]=999;}
 world_t world{2,fogs};tr.world=&world;
 backEnd.viewParms={10,1,0,20,30,640,480,{}};backEnd.refdef.time=1000;
 backEnd.refdef.numRTCWCoronas=1;
 fnRTCWCorona_t base{{0,0,-100},{1,0.5f,0.25f},2,37,1};
 auto frame=[&](int number,int time,int flags){backEnd.viewParms.frameCount=number;backEnd.refdef.time=time;backEnd.refdef.rtcwCoronas[0]=base;backEnd.refdef.rtcwCoronas[0].flags=flags;quads.clear();};
 for(auto profile:{COM_GAME_RTCW_SP,COM_GAME_RTCW_MP}){
  ri.gameId=profile;R_ClearRTCWCoronaFades();frame(10,1000,1);assert(RB_PrepareRTCWCoronas());RB_DrawRTCWCoronas();
  assert(quads.size()==1&&quads[0].color.rgba[3]==1); // 1ms first visible fade
  frame(11,1100,1);assert(RB_PrepareRTCWCoronas());RB_DrawRTCWCoronas();
  assert(quads.size()==1&&quads[0].color.rgba[3]==128);
  assert(quads[0].shader==17&&quads[0].fog==1&&quads[0].color.rgba[0]==127);
  const float radius=640*(80.0f/640+8.0f/100);assert(fabsf(quads[0].w-2*radius)<1e-4f&&fabsf(quads[0].x-(340-radius))<1e-4f);
  frame(12,1300,2);assert(RB_PrepareRTCWCoronas());RB_DrawRTCWCoronas();
  assert(quads.size()==1&&quads[0].shader==(profile==COM_GAME_RTCW_SP?18:17));
  assert(quads[0].color.rgba[3]==(profile==COM_GAME_RTCW_SP?253:255));
  frame(13,1600,0);bool shown=RB_PrepareRTCWCoronas();assert(shown==(profile==COM_GAME_RTCW_MP));
  frame(14,1900,0);assert(!RB_PrepareRTCWCoronas());
  // A skipped frame restarts a new visible fade.
  frame(17,2200,1);assert(RB_PrepareRTCWCoronas());RB_DrawRTCWCoronas();assert(quads.back().color.rgba[3]==1);
  // Other views retain separate IDs/fades.
  backEnd.viewParms.portalView=1;frame(18,2300,1);assert(RB_PrepareRTCWCoronas());RB_DrawRTCWCoronas();assert(quads.size()==1&&quads[0].color.rgba[3]==1);
  backEnd.viewParms.portalView=0;
 }
 ri.gameId=COM_GAME_RTCW_SP;R_ClearRTCWCoronaFades();frame(30,3000,1);
 backEnd.refdef.rtcwCoronas[0].origin[0]=1001;assert(!RB_PrepareRTCWCoronas());
 frame(31,3010,1);backEnd.refdef.rtcwCoronas[0].origin[2]=1;assert(!RB_PrepareRTCWCoronas());
 frame(32,3020,1);backEnd.refdef.rtcwCoronas[0].origin[0]=NAN;assert(!RB_PrepareRTCWCoronas());
 // Dlight/corona modes retain RTCW's numeric meanings, without lens artifacts.
 dlight_t light{{0,0,-50},{1,1,1}};backEnd.refdef.num_dlights=1;backEnd.refdef.dlights=&light;
 for(int mode:{1,2,3}){R_ClearRTCWCoronaFades();flares.integer=mode;frame(40,4000,1);assert(RB_PrepareRTCWCoronas());RB_DrawRTCWCoronas();assert(quads.size()==(mode==3?2u:1u));}
 flares.integer=1;backEnd.refdef.num_dlights=0;
 // The last allocated fade draws first, including ID 0 and negative IDs.
 R_ClearRTCWCoronaFades();frame(50,5000,1);backEnd.refdef.numRTCWCoronas=2;backEnd.refdef.rtcwCoronas[1]=base;
 backEnd.refdef.rtcwCoronas[0].id=0;backEnd.refdef.rtcwCoronas[1].id=-1;backEnd.refdef.rtcwCoronas[1].color[0]=0.25f;
 assert(RB_PrepareRTCWCoronas());RB_DrawRTCWCoronas();assert(quads.size()==2&&quads[0].color.rgba[0]==31&&quads[1].color.rgba[0]==127);
 // Bounded persistent pool, including different scenes and portals.
 R_ClearRTCWCoronaFades();backEnd.refdef.numRTCWCoronas=1;frame(60,6000,1);
 for(int i=0;i<128;++i){backEnd.refdef.rtcwCoronas[0].id=i;RB_QueueRTCWCorona(&backEnd.refdef.rtcwCoronas[0]);}
 backEnd.refdef.rtcwCoronas[0].id=999;RB_QueueRTCWCorona(&backEnd.refdef.rtcwCoronas[0]);
 int count=0;for(const auto &state:rtcwCoronaFades){count+=state.used;assert(state.corona.id!=999);}assert(count==128);
 for(auto profile:{COM_GAME_QUAKE3,COM_GAME_QUAKELIVE}){ri.gameId=profile;const auto old=backEnd.pc;quads.clear();assert(!RB_PrepareRTCWCoronas());RB_DrawRTCWCoronas();assert(quads.empty()&&!memcmp(&old,&backEnd.pc,sizeof(old)));}
 assert(FnRTCWCoronaVisible(0,2)==0&&FnRTCWCoronaVisible(1,2)==1);
 assert(FnRTCWCoronaFade(INT64_MAX,1,5)==1&&FnRTCWCoronaFade(INT64_MAX,0,5)==0);
  assert(FnRTCWCoronaByte(NAN)==0&&FnRTCWCoronaByte(-1)==0&&FnRTCWCoronaByte(3)==255);
 ri.gameId=COM_GAME_RTCW_SP;R_ClearRTCWCoronaFades();frame(100,10000,1);
 backEnd.viewParms.viewportX=INT_MAX;assert(!RB_PrepareRTCWCoronas());backEnd.viewParms.viewportX=20;
 frame(101,10010,1);backEnd.refdef.rtcwCoronas[0].scale=1e38f;
 assert(RB_PrepareRTCWCoronas());RB_DrawRTCWCoronas();assert(quads.empty());
}
'''


class RTCWCoronaTests(unittest.TestCase):
    def compile_run(self, body):
        compiler=shutil.which('clang++')
        if not compiler:self.skipTest('Clang unavailable')
        flags=compiler_target_args(compiler)
        if os.name=='nt' and not flags:flags=['-DFNTECH3_Q3_ONLY']
        elif os.name!='nt':flags=['-m32']
        with tempfile.TemporaryDirectory(prefix='rtcw-corona-',dir=ROOT/'.tmp') as tmp:
            source=Path(tmp)/'probe.cpp';exe=Path(tmp)/'probe.exe';source.write_text(HOST+body)
            built=subprocess.run([compiler,*flags,'-std=c++17','-fno-operator-names','-I',str(ROOT),str(source),'-o',str(exe)],cwd=ROOT,capture_output=True,text=True,timeout=60)
            self.assertEqual(built.returncode,0,built.stdout+built.stderr)
            options={'creationflags':subprocess.CREATE_NO_WINDOW} if os.name=='nt' else {}
            ran=subprocess.run([str(exe)],cwd=ROOT,capture_output=True,text=True,timeout=30,**options)
            self.assertEqual(ran.returncode,0,ran.stdout+ran.stderr)

    def test_scene_capacity_ownership_and_reset(self):self.compile_run(SCENE)
    def test_visibility_fade_projection_and_quad_output(self):self.compile_run(BACKEND)


if __name__=='__main__':unittest.main()
