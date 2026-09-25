"""Execute Enemy Territory's light submission against its modules' 32 lights a frame."""
from pathlib import Path
import os
import re
import shutil
import subprocess
import tempfile
import unittest
from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]
RENDERERS = ('renderer', 'renderervk', 'rendererrtx')


def function(text, signature):
    start = text.index(signature)
    return text[start:text.index('\n}', start) + 2] + '\n'


COMMON = r'''
#include "code/qcommon/q_shared.h"
#include "code/qcommon/qfiles.h"
#include "code/qcommon/game_profile.h"
#include <algorithm>
#include <cassert>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <limits>
#include <random>
#include <string>
#include <vector>
'''

SUBMIT = COMMON + r'''
struct dlight_t {
    vec3_t origin,color; float radius; int sentinel;
#ifdef USE_PMLIGHT
    int shadowIndex,shadowAtlasBaseFace,shadowSpotSource,shadowSpotSourceIndex;
    float shadowPriorityMultiplier; qboolean shadowEligible;
#endif
};
static struct { dlight_t dlights[64]; } data, *backEndData=&data;
static struct { bool registered=true; } tr;
static int r_numdlights, r_numModuleDlights;
enum { GLHW_GENERIC, GLHW_RIVA128, GLHW_PERMEDIA2 };
static struct { int hardwareType; } glConfig;
static cvar_t dynamic{}; static cvar_t *r_dynamiclight=&dynamic;
#ifdef USE_PMLIGHT
static cvar_t mode{}; static float colorScale=1;
static int R_GetDlightMode() { return mode.integer; }
static void R_ApplyDlightColorControls(float *r,float *g,float *b) { *r*=colorScale; *g*=colorScale; *b*=colorScale; }
#endif
static std::vector<std::string> printed;
static void Error(int,const char *,...) { throw 1; }
static void Printf(int,const char *format,...) {
    char text[256]; va_list args; va_start(args,format); std::vsnprintf(text,sizeof(text),format,args); va_end(args);
    printed.push_back(text);
}
static struct { comGameId_t gameId; decltype(&Error) Error; decltype(&Printf) Printf; } ri{COM_GAME_WOLFET,Error,Printf};
#include "code/renderercommon/tr_wolfet_light_scene.h"
static const unsigned FORCE=1u<<31, JUNIOR=1u<<30, DIRECTED=1u<<29;
static const vec3_t origin={1,2,3};
static void Reset(int lights=0,int modules=0) { r_numdlights=lights; r_numModuleDlights=modules; std::memset(&data,0x55,sizeof(data)); }
// The light is refused as the original refuses it: no slot, no count.
template<class F> static void Refused(F submit) {
    const int lights=r_numdlights, modules=r_numModuleDlights; const auto saved=data;
    submit(); assert(r_numdlights==lights && r_numModuleDlights==modules && !std::memcmp(&data,&saved,sizeof(data)));
}
// The original would store it, so it takes one of the 32; this renderer draws nothing.
template<class F> static void CountedOnly(F submit) {
    const int lights=r_numdlights, modules=r_numModuleDlights; const auto saved=data;
    submit(); assert(r_numdlights==lights && r_numModuleDlights==modules+1 && !std::memcmp(&data,&saved,sizeof(data)));
}
'''

SUBMIT_CHECKS = SUBMIT + r'''
int main() {
    const float nan=std::numeric_limits<float>::quiet_NaN(), inf=std::numeric_limits<float>::infinity();
    for(auto profile:{COM_GAME_QUAKE3,COM_GAME_QUAKELIVE,COM_GAME_RTCW_SP,COM_GAME_RTCW_MP,COM_GAME_ALICE}) {
        ri.gameId=profile; Reset(); bool caught=false;
        try { RE_AddWolfETLightToScene(origin,320,1,1,1,1,0,0); } catch(int) { caught=true; }
        assert(caught && !r_numdlights && !r_numModuleDlights);
    }
    ri.gameId=COM_GAME_WOLFET;
    for(int dynamicLight:{-1,0,1,2,3}) for(unsigned flags:{0u,FORCE,FORCE|1u,7u}) {
        dynamic.integer=dynamicLight; Reset();
        RE_AddWolfETLightToScene(origin,320,0.5f,1,0.5f,0.25f,0,int(flags));
        // A forced light ignores r_dynamiclight, exactly as the original's does.
        const bool drawn=(flags&FORCE) || dynamicLight!=0;
        assert(r_numdlights==drawn && r_numModuleDlights==drawn);
        if(!drawn) continue;
        const auto &light=data.dlights[0];
        // The brightness scales the colour; the radius stays the radius.
        assert(!std::memcmp(light.origin,origin,sizeof(origin)) && light.radius==320);
        assert(light.color[0]==0.5f && light.color[1]==0.25f && light.color[2]==0.125f && !light.sentinel);
    }
    dynamic.integer=1;
    // The original's own refusals take no slot and no count.
    for(float radius:{0.f,-1.f,-inf}) { Reset(); Refused([&]{RE_AddWolfETLightToScene(origin,radius,1,1,1,1,0,0);}); }
    for(float intensity:{0.f,-2.f,-inf}) { Reset(); Refused([&]{RE_AddWolfETLightToScene(origin,320,intensity,1,1,1,0,0);}); }
    for(int hardware:{GLHW_RIVA128,GLHW_PERMEDIA2}) {
        glConfig.hardwareType=hardware; Reset(); Refused([&]{RE_AddWolfETLightToScene(origin,320,1,1,1,1,0,int(FORCE));});
    }
    glConfig.hardwareType=GLHW_GENERIC;
    dynamic.integer=0; Reset(); Refused([&]{RE_AddWolfETLightToScene(origin,320,1,1,1,1,0,int(JUNIOR|DIRECTED));}); dynamic.integer=1;
    tr.registered=false; Reset(); Refused([&]{RE_AddWolfETLightToScene(origin,320,1,1,1,1,0,0);}); tr.registered=true;
    // The 32 a frame count module lights alone, not the frame's slots.
    for(int modules:{-1,32,33,INT_MAX}) { Reset(0,modules); Refused([&]{RE_AddWolfETLightToScene(origin,320,1,1,1,1,0,0);}); }
    for(int lights:{-1,64,65,INT_MAX}) { Reset(lights,0); Refused([&]{RE_AddWolfETLightToScene(origin,320,1,1,1,1,0,0);}); }
    Reset(40,31); RE_AddWolfETLightToScene(origin,320,1,1,1,1,0,0);
    assert(r_numdlights==41 && r_numModuleDlights==32 && data.dlights[40].radius==320);
    Refused([&]{RE_AddWolfETLightToScene(origin,320,1,1,1,1,0,int(FORCE));});
    // Lights the original stores but this renderer cannot draw still take one
    // of the 32: directed and junior lights, and unrepresentable values the
    // original's comparisons let through.
    printed.clear();
    for(int pass=0;pass<2;++pass) {
        Reset(); CountedOnly([&]{RE_AddWolfETLightToScene(origin,256,0.5f,1,1,1,0,int(DIRECTED));});
        Reset(); CountedOnly([&]{RE_AddWolfETLightToScene(origin,256,0.5f,1,1,1,0,int(JUNIOR|FORCE));});
    }
    assert(printed.size()==2 && printed[0].find("directional")!=std::string::npos &&
           printed[1].find("light-grid")!=std::string::npos);
    for(float bad:{nan,inf}) {
        Reset(); CountedOnly([&]{RE_AddWolfETLightToScene(origin,bad,1,1,1,1,0,0);});
        Reset(); CountedOnly([&]{RE_AddWolfETLightToScene(origin,320,bad,1,1,1,0,0);});
        for(int axis=0;axis<3;++axis) {
            vec3_t at={1,2,3}; at[axis]=bad; Reset(); CountedOnly([&]{RE_AddWolfETLightToScene(at,320,1,1,1,1,0,0);});
            float color[3]={1,1,1}; color[axis]=bad;
            Reset(); CountedOnly([&]{RE_AddWolfETLightToScene(origin,320,1,color[0],color[1],color[2],0,0);});
        }
    }
    Reset(); CountedOnly([&]{RE_AddWolfETLightToScene(nullptr,320,1,1,1,1,0,0);});
    Reset(); CountedOnly([&]{RE_AddWolfETLightToScene(origin,320,FLT_MAX,FLT_MAX,1,1,0,0);});
    // A per-light shader is said to be unsupported once, and the light drawn.
    printed.clear();
    for(int pass=0;pass<3;++pass) { Reset(); RE_AddWolfETLightToScene(origin,320,1,1,1,1,7,0); assert(r_numdlights==1 && r_numModuleDlights==1); }
    assert(printed.size()==1 && printed[0].find("per-light shaders")!=std::string::npos);
    // A frame's 32 are the first 32 the original would store, whatever this
    // renderer draws of them.
    Reset();
    for(int i=0;i<5;++i) RE_AddWolfETLightToScene(origin,256,1,1,1,1,0,int(DIRECTED));
    for(int i=0;i<40;++i) RE_AddWolfETLightToScene(origin,float(100+i),1,1,1,1,0,0);
    assert(r_numModuleDlights==32 && r_numdlights==27 && data.dlights[26].radius==126);
#ifdef USE_PMLIGHT
    for(int dlightMode:{0,1,2}) {
        mode.integer=dlightMode; colorScale=2; Reset();
        RE_AddWolfETLightToScene(origin,320,0.5f,1,0.5f,0.25f,0,0);
        const auto &light=data.dlights[0];
        assert(r_numdlights==1 && light.shadowIndex==-1 && light.shadowAtlasBaseFace==-1 &&
               light.shadowSpotSource==-1 && light.shadowSpotSourceIndex==-1 && light.shadowPriorityMultiplier==1);
        assert(light.shadowEligible==(dlightMode!=0));
        assert(light.color[0]==(dlightMode ? 1.f : 0.5f));
        Reset();
        if(dlightMode) CountedOnly([&]{RE_AddWolfETLightToScene(origin,WORLD_SIZE*2.f,1,1,1,1,0,0);});
        else { RE_AddWolfETLightToScene(origin,WORLD_SIZE*2.f,1,1,1,1,0,0); assert(r_numdlights==1 && r_numModuleDlights==1); }
        colorScale=FLT_MAX; Reset();
        if(dlightMode) CountedOnly([&]{RE_AddWolfETLightToScene(origin,320,4,4,4,4,0,0);});
    }
#endif
    std::puts("ok");
}
'''

FRAME_CHECKS = SUBMIT + r'''
static const vec3_t lightOrigin={1,2,3};
// One frame's plan: each scene's module submissions, the engine's budget in
// it, and its portal views (siblings, or each nested in the one before).
struct Submission { float radius; unsigned flags; };
struct PlannedScene { std::vector<Submission> submissions; int engine,portals; bool nested; };
struct Outcome { std::vector<int> counted,drawn; int engineSlots,fullRefusals; };
// The original's rule: the first 32 lights past its own early refusals.
static std::vector<int> Retail(const std::vector<PlannedScene> &plan,int dynamicLight) {
    std::vector<int> stored; int index=0;
    for(const auto &scene:plan) for(const auto &light:scene.submissions) {
        if(int(stored.size())<32 && light.radius>0 && ((light.flags&FORCE) || dynamicLight!=0)) stored.push_back(index);
        ++index;
    }
    return stored;
}
// The rule before this change: a light was refused once the frame held 32
// slots of any kind; directed and junior lights took none; the engine's
// lights took any free slot, even at r_dynamiclight 0, where the scene's
// lights were then cleared; a portal view copied all it had room for.
static std::vector<int> OldDrawn(const std::vector<PlannedScene> &plan,int dynamicLight) {
    std::vector<int> drawn; int count=0, index=0;
    for(const auto &scene:plan) {
        const int first=count;
        for(const auto &light:scene.submissions) {
            if(count<32 && light.radius>0 && ((light.flags&FORCE) || dynamicLight!=0) && !(light.flags&(JUNIOR|DIRECTED))) {
                drawn.push_back(index); ++count;
            }
            ++index;
        }
        count+=std::min(scene.engine,64-count);
        int parent=count-first;
        for(int portal=0;portal<scene.portals;++portal) {
            const int copies=std::min(parent,64-count);
            count+=copies;
            if(scene.nested) parent=copies;
        }
    }
    return drawn;
}
static Outcome RunFrame(const std::vector<PlannedScene> &plan,int dynamicLight,bool engine) {
    Outcome out{}; r_numdlights=r_numModuleDlights=0; int moduleSlots=0, index=0;
    for(const auto &scene:plan) {
        const int first=r_numdlights;
        for(const auto &light:scene.submissions) {
            const int modules=r_numModuleDlights, count=r_numdlights;
            RE_AddWolfETLightToScene(lightOrigin,light.radius,1,1,1,1,0,int(light.flags));
            const bool counted=r_numModuleDlights==modules+1, drawn=r_numdlights==count+1;
            assert(!drawn || counted);
            const bool original=light.radius>0 && ((light.flags&FORCE) || dynamicLight!=0);
            // A light the original accepts is refused only when the frame's
            // array is full, and only portal copies of module lights fill it.
            if(original && modules<32 && !counted) assert(count>=64 && moduleSlots>int(out.drawn.size()));
            if(original && modules<32 && !counted) ++out.fullRefusals;
            if(counted) out.counted.push_back(index);
            if(drawn) { ++moduleSlots; out.drawn.push_back(index); }
            ++index;
        }
        const int modules=r_numdlights-first;
        if(engine && dynamicLight!=0) {
            const int end=RTCW_EngineLightEnd(64,r_numModuleDlights,first);
            for(int i=0;i<scene.engine && r_numdlights<end;++i) data.dlights[r_numdlights++].sentinel=-1;
        }
        const int set=r_numdlights-first;
        assert(set>=modules && set<=RTCW_SCENE_LIGHTS && r_numdlights<=64);
        int parent=set;
        for(int portal=0;portal<scene.portals;++portal) {
            const int copies=RTCW_PortalLightCopies(parent,modules,r_numdlights,64,
                RTCW_EngineLightEnd(64,r_numModuleDlights,r_numdlights));
            assert(copies>=0 && copies<=parent && r_numdlights+copies<=64);
            const int moduleCopies=std::min(copies,std::min(parent,modules));
            assert(moduleCopies==std::min(std::min(parent,modules),64-r_numdlights));
            moduleSlots+=moduleCopies; r_numdlights+=copies;
            if(scene.nested) parent=copies;
        }
        // The engine takes no slot a module light may still claim: its own
        // and its copies stay within 32, plus the slots of counted lights
        // this renderer did not draw.
        assert(r_numdlights-moduleSlots<=32+(r_numModuleDlights-int(out.drawn.size())));
    }
    out.engineSlots=r_numdlights-moduleSlots;
    return out;
}
int main() {
    std::mt19937 rng(0x5e7e7u);
    int portalFrames=0,engineFrames=0,skippedFrames=0,forcedFrames=0,limitedFrames=0,oldDrawnPastRetail=0;
    for(int frame=0;frame<20000;++frame) {
        const int dynamicLight=rng()%4==0 ? 0 : 1; dynamic.integer=dynamicLight;
        std::vector<PlannedScene> plan(1+rng()%4);
        const bool portals=rng()%3==0;
        for(auto &scene:plan) {
            scene.submissions.resize(rng()%41);
            for(auto &light:scene.submissions) {
                const unsigned roll=rng()%100;
                light.radius=roll<3 ? 0.f : 100.f+float(rng()%400);
                light.flags=(rng()%4==0 ? FORCE : 0u)|(roll>=95 ? DIRECTED : 0u)|(roll>=3 && roll<5 ? JUNIOR : 0u);
            }
            scene.engine=int(rng()%33); scene.portals=portals ? int(rng()%4) : 0; scene.nested=rng()&1;
        }
        const auto retail=Retail(plan,dynamicLight);
        const auto with=RunFrame(plan,dynamicLight,true), without=RunFrame(plan,dynamicLight,false);
        assert(!without.engineSlots);
        if(!portals) {
            // Exactly the original's first 32, whatever the engine added.
            assert(with.counted==retail && without.counted==retail && with.drawn==without.drawn);
        }
        for(int index:with.counted) assert(std::find(retail.begin(),retail.end(),index)!=retail.end());
        // No light the rule before this change drew is refused now, unless
        // the original refuses it too.
        for(int index:OldDrawn(plan,dynamicLight)) {
            const bool original=std::find(retail.begin(),retail.end(),index)!=retail.end();
            assert(std::find(with.drawn.begin(),with.drawn.end(),index)!=with.drawn.end() || !original);
            oldDrawnPastRetail+=!original;
        }
        if(!dynamicLight) assert(!with.engineSlots);
        if(!portals) assert(!with.fullRefusals);
        portalFrames+=portals; engineFrames+=with.engineSlots>0; forcedFrames+=!dynamicLight;
        skippedFrames+=with.drawn.size()<with.counted.size();
        limitedFrames+=with.counted.size()!=retail.size();
    }
    assert(portalFrames>5000 && engineFrames>12000 && skippedFrames>5000 && forcedFrames>3000);
    std::printf("{\"portalFrames\":%d,\"engineFrames\":%d,\"skippedFrames\":%d,\"limitedFrames\":%d,\"oldDrawnPastRetail\":%d}\n",
        portalFrames,engineFrames,skippedFrames,limitedFrames,oldDrawnPastRetail);
}
'''
RENDERER_PRELUDE = COMMON + r'''
#include "code/renderercommon/tr_rtcw_engine_lights.h"
#define USE_PMLIGHT
struct dlight_t { int id; };
static struct { dlight_t dlights[64]; } data, *backEndData=&data;
struct viewParms_t { dlight_t *dlights; unsigned num_dlights; };
struct portalViewPlan_t { viewParms_t viewParms,parentParms; };
static struct { struct { struct { qboolean snooperView; } rtcw; int num_moduleDlights; int num_dlights; int rdflags; } refdef; } tr;
static int r_numdlights, r_numModuleDlights, r_firstSceneDlight;
enum { GLHW_GENERIC, GLHW_RIVA128, GLHW_PERMEDIA2 };
static struct { int hardwareType; } glConfig;
static cvar_t dynamic{}, mode{}; static cvar_t *r_dynamiclight=&dynamic;
static struct { comGameId_t gameId; } ri;
static int R_GetDlightMode() { return mode.integer; }
'''

RENDERER_CHECKS = r'''
static bool Clears() { return CLEAR_CONDITION; }
static const comGameId_t profiles[]={COM_GAME_QUAKE3,COM_GAME_QUAKELIVE,COM_GAME_RTCW_SP,COM_GAME_RTCW_MP,COM_GAME_WOLFET,COM_GAME_ALICE};
int main() {
    for(auto profile:profiles) {
        ri.gameId=profile;
        const bool rtcw=profile==COM_GAME_RTCW_SP || profile==COM_GAME_RTCW_MP, et=profile==COM_GAME_WOLFET;
        assert(bool(R_ReservesModuleLights())==(rtcw || et) && bool(R_UsesRTCWLighting())==rtcw);
        for(int dlightMode:{0,1,2}) for(int dynamicLight:{0,1,2,3}) for(int snooper:{0,1})
        for(int modules:{0,10,31,32}) for(int first:{0,20,40}) {
            mode.integer=dlightMode; dynamic.integer=dynamicLight; tr.refdef.rtcw.snooperView=snooper ? qtrue : qfalse;
            r_numModuleDlights=modules; r_firstSceneDlight=first;
            const int end=R_EngineSceneLightEnd();
            // ET's engine lights keep its modules' frame in every shared mode,
            // and join no scene at r_dynamiclight 0, where only forced module
            // lights are drawn. Every other profile but RTCW keeps the frame.
            if(et) assert(end==(dynamicLight ? std::min(32+modules,first+32) : 0));
            else if(!rtcw) assert(end==64);
        }
        // r_dynamiclight 0 clears a scene's lights only where the modules have
        // no forced lights of their own; these cards clear every scene's.
        for(int dynamicLight:{0,1,2}) for(int hardware:{GLHW_GENERIC,GLHW_RIVA128,GLHW_PERMEDIA2}) {
            dynamic.integer=dynamicLight; glConfig.hardwareType=hardware;
            assert(Clears()==(hardware==GLHW_PERMEDIA2 || (!dynamicLight && !rtcw && !et)));
        }
        glConfig.hardwareType=GLHW_GENERIC;
    }
#ifdef CHECK_PORTAL_PLAN
    // A portal view copies its parent's module lights while the frame has
    // room, and ET's engine lights only within their bound.
    for(auto profile:profiles) {
        ri.gameId=profile;
        const bool reserves=profile==COM_GAME_RTCW_SP || profile==COM_GAME_RTCW_MP || profile==COM_GAME_WOLFET;
        for(int parent:{0,1,12,20}) for(int sceneModules:{0,5,12}) for(int used:{20,40,58,64}) for(int modules:{0,12,31,32}) {
            portalViewPlan_t plan{};
            plan.parentParms.dlights=data.dlights; plan.parentParms.num_dlights=unsigned(parent);
            tr.refdef.num_moduleDlights=sceneModules; r_numdlights=used; r_numModuleDlights=modules;
            R_PortalCopyDlightsForPlan(&plan);
            const int copies=r_numdlights-used, available=64-used;
            const int expected=!parent || available<=0 ? 0 : reserves ?
                std::max(0,RTCW_PortalLightCopies(parent,sceneModules,used,64,RTCW_EngineLightEnd(64,modules,used))) :
                std::min(parent,available);
            assert(copies==expected && int(plan.viewParms.num_dlights)==copies);
            assert(copies ? plan.viewParms.dlights==&data.dlights[used] : !plan.viewParms.dlights);
        }
    }
#endif
    std::puts("ok");
}
'''


class WolfETLightTests(unittest.TestCase):
    def compile_run(self, source, definitions=()):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++ compiler unavailable')
        flags = compiler_target_args(compiler)
        if os.name == 'nt' and not flags:
            flags += ['-DFNTECH3_Q3_ONLY']
        (ROOT / '.tmp').mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='wolfet-light-', dir=ROOT / '.tmp') as temporary:
            folder = Path(temporary)
            path = folder / 'probe.cpp'
            path.write_text(source)
            executable = folder / ('probe.exe' if os.name == 'nt' else 'probe')
            build = subprocess.run([compiler, '-std=c++17', *flags, *definitions, '-I', str(ROOT), str(path),
                                    '-o', str(executable)], cwd=ROOT, capture_output=True, text=True, timeout=60)
            self.assertEqual(build.returncode, 0, build.stdout + build.stderr)
            run = subprocess.run([str(executable)], cwd=ROOT, capture_output=True, text=True, timeout=60)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)
            return run.stdout

    def test_submission_keeps_the_originals_acceptance(self):
        for definitions in ((), ('-DUSE_PMLIGHT',)):
            with self.subTest(definitions=definitions):
                self.assertIn('ok', self.compile_run(SUBMIT_CHECKS, definitions))

    def test_engine_lights_leave_the_modules_their_frame(self):
        self.assertIn('portalFrames', self.compile_run(FRAME_CHECKS, ('-DUSE_PMLIGHT',)))

    def test_each_renderer_bounds_engine_lights_and_keeps_forced_lights(self):
        plan = (ROOT / 'code/renderercommon/tr_portal_plan.h').read_text()
        copy = function(plan, 'static void R_PortalCopyDlightsForPlan(')
        for renderer in RENDERERS:
            local = (ROOT / 'code' / renderer / 'tr_local.h').read_text()
            scene = (ROOT / 'code' / renderer / 'tr_scene.c').read_text()
            render = function(scene, 'void RE_RenderScene(')
            clear = re.search(r'\n\tif \( ([^\n]*) \) \{\n\t\ttr\.refdef\.num_dlights = 0;', render.replace('\r\n', '\n'))
            self.assertIsNotNone(clear, renderer)
            source = (RENDERER_PRELUDE +
                      function(local, 'static ID_INLINE qboolean R_UsesRTCWLighting(') +
                      function(local, 'static ID_INLINE qboolean R_ReservesModuleLights(') +
                      function(scene, 'static int R_EngineSceneLightEnd(') +
                      (copy if renderer != 'rendererrtx' else '') +
                      RENDERER_CHECKS.replace('CLEAR_CONDITION', clear[1]))
            definitions = ('-DCHECK_PORTAL_PLAN',) if renderer != 'rendererrtx' else ()
            with self.subTest(renderer=renderer):
                self.assertIn('ok', self.compile_run(source.replace('ID_INLINE', 'inline'), definitions))
                # ET's light submission counts on the frame's module counter,
                # which every frame resets, and is compiled into every renderer.
                self.assertIn('r_numModuleDlights = 0;', function(scene, 'void R_InitNextFrame('))
                self.assertLess(scene.index('int\t\t\tr_numModuleDlights;'),
                                scene.index('#include "../renderercommon/tr_wolfet_light_scene.h"'))
        # The RTX mirror view copies lights under the same rule as the others.
        mirror = function((ROOT / 'code/rendererrtx/tr_main.c').read_text(), 'static qboolean R_MirrorViewBySurface(')
        self.assertIn('if ( R_ReservesModuleLights() ) {', mirror)
        self.assertIn('RTCW_EngineLightEnd( (int)ARRAY_LEN( backEndData->dlights ), r_numModuleDlights, r_numdlights )',
                      mirror)


if __name__ == '__main__':
    unittest.main()
