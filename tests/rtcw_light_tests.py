"""Execute RTCW light submission, projection and backend pass selection."""
from pathlib import Path
import os
import re
import shutil
import subprocess
import tempfile
import unittest
from compiler_support import compiler_target_args
from model_shader_tests import function, common as shader_common

ROOT = Path(__file__).resolve().parents[1]

COMMON = r'''
#include "code/qcommon/q_shared.h"
#include "code/qcommon/qfiles.h"
#include "code/qcommon/game_profile.h"
#include "code/renderercommon/tr_rtcw_light_math.h"
#include <array>
#include <cassert>
#include <cstring>
#include <limits>
#include <vector>
#if defined(_WIN32) && (defined(_M_IX86) || defined(__i386__))
static constexpr byte quarterByte=64, halfByte=128, eighthByte=32;
#else
static constexpr byte quarterByte=63, halfByte=127, eighthByte=31;
#endif
static int rejectionChecks;
template<class F> static void Reject(F function) {
    ++rejectionChecks; bool caught=false; try { function(); } catch(int) { caught=true; }
    if(!caught) fprintf(stderr,"Expected rejection %d\n",rejectionChecks);
    assert(caught);
}
'''

SCENE = COMMON + r'''
#define USE_LEGACY_DLIGHTS
struct shader_s { int id; }; using shader_t=shader_s;
struct dlight_t {
    vec3_t origin,color; float radius;
    shader_t *rtcwShader; int rtcwOverdraw;
    int sentinel;
#ifdef USE_PMLIGHT
    int shadowIndex,shadowAtlasBaseFace,shadowSpotSource,shadowSpotSourceIndex;
    float shadowPriorityMultiplier,shadowProjectionFar;
    qboolean shadowEligible;
#endif
};
static struct { dlight_t dlights[64]; } data, *backEndData=&data;
static struct { bool registered=true; int frameCount=1; } tr;
static int r_numdlights;
enum { GLHW_GENERIC, GLHW_RIVA128, GLHW_PERMEDIA2 };
static struct { int hardwareType; } glConfig;
static cvar_t dynamic{}, radiusScale{}; static cvar_t *r_dynamiclight=&dynamic;
#ifdef USE_PMLIGHT
static cvar_t mode{}, intensity{};
static cvar_t *r_dlightScale=&radiusScale, *r_dlightIntensity=&intensity;
static int R_GetDlightMode() { return mode.integer; }
static void R_ApplyDlightColorControls(float *,float *,float *) {}
#endif
static int registrations, lookups;
static bool failLookup;
static void Error(int,const char *,...) { throw 1; }
static cvar_t *Get(const char *name,const char *value,int flags) {
    assert(!std::strcmp(name,"r_dlightScale") && !std::strcmp(value,"1.0") && flags==CVAR_ARCHIVE);
    ++registrations; return &radiusScale;
}
static struct { comGameId_t gameId; decltype(&Error) Error; decltype(&Get) Cvar_Get; } ri{COM_GAME_RTCW_MP,Error,Get};
static bool R_UsesRTCWLighting() { return ri.gameId==COM_GAME_RTCW_MP || ri.gameId==COM_GAME_RTCW_SP; }
static shader_t negative{1}, flame{2};
static shader_t *R_FindRTCWLightShader(const char *name) {
    ++lookups; if(failLookup) throw 2;
    assert(!std::strcmp(name,"negdlightshader") || !std::strcmp(name,"flamedlightshader"));
    return !std::strcmp(name,"negdlightshader") ? &negative : &flame;
}
#include "code/renderercommon/tr_rtcw_light_scene.h"
int main() {
    const vec3_t origin={1,2,3}, color={0.25f,0.5f,1};
    for(auto mode:{COM_GAME_RTCW_SP,COM_GAME_RTCW_MP}) for(int dynamicMode:{-1,0,1,2,3})
    for(float scale:{-1.f,0.f,0.125f,1.f,2.f}) for(uint32_t flags:{0u,1u,2u,10u,11u,0x80000000u,0x40000000u,0xc0000002u}) {
        ri.gameId=mode; dynamic.integer=dynamicMode; radiusScale.value=scale; r_numdlights=0;
        std::memset(&data,0x55,sizeof(data)); const auto saved=data;
        const bool enabled=((flags&0x80000000u) || (dynamicMode!=0 && dynamicMode!=2)) && (mode!=COM_GAME_RTCW_SP || scale>0);
        RE_AddRTCWLightToScene(origin,1000,color[0],color[1],color[2],static_cast<int>(flags));
        assert(r_numdlights==enabled);
        if(!enabled) { assert(!std::memcmp(&data,&saved,sizeof(data))); continue; }
        const auto &light=data.dlights[0];
        assert(!std::memcmp(light.origin,origin,sizeof(origin)) && !std::memcmp(light.color,color,sizeof(color)));
        assert(light.radius==(mode==COM_GAME_RTCW_SP ? 1000*scale : 1000));
        const auto effect=flags&0x3fffffffu;
        assert(light.rtcwShader==(effect==10 ? &negative : effect==11 ? &flame : nullptr));
        assert(light.rtcwOverdraw==((effect==10 || effect==11) ? 0 : int(effect)) && !light.sentinel);
        assert(!std::memcmp(&data.dlights[1],&saved.dlights[1],sizeof(data)-sizeof(dlight_t)));
    }
    assert(registrations==1); ri.gameId=COM_GAME_RTCW_MP; dynamic.integer=1;
    for(int count:{-1,32,64,INT_MAX}) {
        r_numdlights=count; const auto saved=data; RE_AddRTCWLightToScene(origin,1000,1,1,1,0);
        assert(r_numdlights==count && !std::memcmp(&data,&saved,sizeof(data)));
    }
    r_numdlights=0;
    for(int hardware:{GLHW_RIVA128,GLHW_PERMEDIA2}) { glConfig.hardwareType=hardware; RE_AddRTCWLightToScene(origin,1000,1,1,1,0); assert(!r_numdlights); }
    glConfig.hardwareType=GLHW_GENERIC; tr.registered=false; RE_AddRTCWLightToScene(origin,1000,1,1,1,0); assert(!r_numdlights); tr.registered=true;
    for(float bad:{std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN(),FLT_MAX})
        Reject([&]{RE_AddRTCWLightToScene(origin,bad,1,1,1,0);});
    // Retail MP's empty player preview produces an infinite light position.
    // Cull every non-finite axis before scene publication or shader lookup.
    for(int axis=0;axis<3;++axis) for(float bad:{std::numeric_limits<float>::infinity(),
            -std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()}) {
        vec3_t badOrigin={1,2,3}; badOrigin[axis]=bad;
        const auto saved=data; const auto beforeLookups=lookups;
        RE_AddRTCWLightToScene(badOrigin,1000,1,1,1,11);
        assert(!r_numdlights && !std::memcmp(&data,&saved,sizeof(data)) && lookups==beforeLookups);
    }
    Reject([&]{RE_AddRTCWLightToScene(origin,1000,1,1,std::numeric_limits<float>::quiet_NaN(),0);});
    Reject([&]{RE_AddRTCWLightToScene(origin,1000,1,1,1,65);});
    const auto saved=data; failLookup=true; Reject([&]{RE_AddRTCWLightToScene(origin,1000,1,1,1,10);}); failLookup=false;
    assert(!r_numdlights && !std::memcmp(&data,&saved,sizeof(data)));
    for(auto mode:{COM_GAME_QUAKE3,COM_GAME_QUAKELIVE}) { ri.gameId=mode; Reject([&]{RE_AddRTCWLightToScene(origin,1000,1,1,1,0);}); }
#ifdef USE_PMLIGHT
    dynamic.integer=1; intensity.value=2;
    for(auto profile:{COM_GAME_RTCW_SP,COM_GAME_RTCW_MP}) for(int quality:{1,2})
    for(float scale:{0.125f,1.f,2.f}) for(int flags:{0,1,2,10,11}) {
        ri.gameId=profile; mode.integer=quality; radiusScale.value=scale; r_numdlights=0;
        RE_AddRTCWLightToScene(origin,200,.25f,.5f,1,flags);
        assert(r_numdlights==1);
        const auto &light=data.dlights[0];
        const bool material=flags==10 || flags==11;
        assert(light.shadowEligible==!material);
        assert(light.radius==(profile==COM_GAME_RTCW_SP || !material ? 200*scale : 200));
        assert(light.color[0]==(material ? .25f : .5f*(flags+1)));
        if(!material) assert(light.shadowProjectionFar>=light.radius && light.shadowIndex==-1);
    }
    ri.gameId=COM_GAME_RTCW_MP; radiusScale.value=1; intensity.value=1;
    for(float bad:{0.f,-1.f,std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()}) {
        r_numdlights=0; radiusScale.value=bad; const auto saved=data;
        RE_AddRTCWLightToScene(origin,200,1,1,1,0);
        assert(!r_numdlights && !std::memcmp(&data,&saved,sizeof(data)));
    }
    radiusScale.value=1;
    // A complete flicker cycle crossing a projection band never shrinks the
    // shadow range; the submitted illumination still changes on every frame.
    std::memset(rtcwLightShadowHistory,0,sizeof(rtcwLightShadowHistory));
    const float radii[]={125,130,120,150,91,216,100};
    float far=0;
    for(float radius:radii) {
        ++tr.frameCount; r_numdlights=0; RE_AddRTCWLightToScene(origin,radius,1,.5f,.2f,0);
        assert(data.dlights[0].radius==radius && data.dlights[0].shadowProjectionFar>=far);
        far=data.dlights[0].shadowProjectionFar;
    }
    assert(far==256);
    tr.frameCount+=121; r_numdlights=0; RE_AddRTCWLightToScene(origin,100,1,.5f,.2f,0);
    assert(data.dlights[0].shadowProjectionFar==128);
    // Independent origins, bounded eviction, and a restarted renderer's frame clock.
    rtcwLightShadowHistory_t history[2]{};
    const vec3_t other={2,2,3}, third={3,2,3};
    assert(R_RTCWLightShadowProjection(history,2,origin,300,10)==512);
    assert(R_RTCWLightShadowProjection(history,2,other,100,11)==128);
    assert(R_RTCWLightShadowProjection(history,2,origin,90,12)==512);
    assert(R_RTCWLightShadowProjection(history,2,third,100,13)==128);
    assert(R_RTCWLightShadowProjection(history,2,origin,90,14)==512);
    assert(R_RTCWLightShadowProjection(history,2,origin,90,1)==128);
#endif
    // Projection boundaries, original platform byte conversion and malformed vertex isolation.
    const vec3_t zero={0,0,0};
    auto projected=RTCW_ProjectLightVertex(zero,1,color,zero);
    assert(!projected.clip && projected.st[0]==0.5f && projected.st[1]==0.5f);
    assert(projected.color[0]==quarterByte && projected.color[1]==halfByte && projected.color[2]==255 && projected.color[3]==255);
    const vec3_t high={0,0,0.75f}; projected=RTCW_ProjectLightVertex(zero,1,color,high);
    assert(!projected.clip && projected.color[0]==eighthByte && projected.color[1]==quarterByte && projected.color[2]==halfByte);
    for(int axis=0;axis<3;++axis) for(int direction:{-1,1}) {
        vec3_t point={0,0,0}; point[axis]=float(direction*2); projected=RTCW_ProjectLightVertex(zero,1,color,point);
        const int bit=1<<(axis*2+(axis==2 ? direction>0 : direction<0)); assert(projected.clip==bit);
    }
    const vec3_t badOrigin={std::numeric_limits<float>::infinity(),0,0};
    assert(RTCW_ProjectLightVertex(zero,1,color,badOrigin).clip==255);
}
'''

BACKEND = COMMON + r'''
#define SHADER_MAX_VERTEXES 16
#define SHADER_MAX_INDEXES 48
using glIndex_t=uint32_t;
enum { GLS_SRCBLEND_DST_COLOR=1, GLS_DSTBLEND_ONE=2, GLS_DEPTHFUNC_EQUAL=4,
       GLS_SRCBLEND_ZERO=8, GL_FLOAT=10, GL_UNSIGNED_BYTE=11, CLS_NONE=0,
       CLS_TEXCOORD_ARRAY=1, CLS_COLOR_ARRAY=2, VK_DESC_TEXTURE0=0,
       TESS_RGBA0=1, TESS_ST0=2, DEPTH_RANGE_NORMAL=0 };
struct textureBundle_t { int id; };
struct shaderStage_t { unsigned stateBits; textureBundle_t bundle[1]; };
struct shader_t { int numUnfoggedPasses,cullType,polygonOffset; shaderStage_t *stages[4]; };
struct dlight_t { vec3_t transformed,color; float radius; shader_t *rtcwShader; int rtcwOverdraw; };
static struct {
    struct { int num_dlights; dlight_t dlights[32]; struct { bool snooperView; } rtcw; } refdef;
    struct { int c_totalIndexes,c_dlightIndexes,c_dlightVertexes; } pc;
} backEnd;
static struct { int *dlightImage; } tr;
static struct {
    int numVertexes,numIndexes; uint32_t dlightBits; vec4_t xyz[SHADER_MAX_VERTEXES]; glIndex_t indexes[SHADER_MAX_INDEXES];
    shader_t *shader; float dlightColors[SHADER_MAX_VERTEXES][4];
    struct { vec2_t texcoords[2][SHADER_MAX_VERTEXES]; vec2_t *texcoordPtr[2];
             struct { byte rgba[4]; } colors[2][SHADER_MAX_VERTEXES]; } svars;
} tess;
struct Draw { int texture; unsigned state; std::vector<glIndex_t> indexes; };
static std::vector<Draw> draws;
static int texture; static unsigned state; static const glIndex_t *indices; static int indexCount;
static const float *glCoordinates; static const void *glColors; static int glColorType;
static void GL_SelectTexture(int unit) { assert(unit==0); }
static void R_BindAnimatedImage(const textureBundle_t *bundle) { texture=bundle->id; }
static void GL_Bind(int *) { texture=0; }
static void GL_State(unsigned bits) { state=bits; }
static void GL_ClientState(int,int) {}
static void qglTexCoordPointer(int count,int type,int stride,const void *data) {
    assert(count==2 && type==GL_FLOAT && !stride); glCoordinates=static_cast<const float *>(data);
}
static void qglColorPointer(int count,int type,int stride,const void *data) {
    assert(count==4 && !stride); glColors=data; glColorType=type;
}
static void Record(int count,const glIndex_t *indexes) {
    draws.push_back({texture,state,{indexes,indexes+count}});
}
static void R_DrawElements(int count,const glIndex_t *indexes) {
    assert(glCoordinates && glColors); Record(count,indexes);
}
// Fog enable/disable and GL/GLSL parameter transfer execute in rtcw_fog_gpu_tests.
static void RB_RTCWFogToggle(qboolean) {}
struct Vk_Pipeline_Def { unsigned state_bits; };
struct vkUniform_t { vec4_t texFactors; qboolean fog; };
static void VK_SetRTCWFogParams(vkUniform_t *data,const shaderStage_t *stage) {
    assert(stage); data->fog=qtrue;
}
static uint32_t VK_PushUniform(const vkUniform_t *data) {
    assert(data->texFactors[0]==1); return 0;
}
static struct { uint32_t dlight_pipelines[2][3][2]; } vk;
static void vk_get_pipeline_def(uint32_t,Vk_Pipeline_Def *def) { def->state_bits=0; }
static uint32_t vk_find_pipeline_ext(uint32_t,const Vk_Pipeline_Def *def,qboolean) { return def->state_bits; }
static void vk_bind_pipeline(uint32_t bits) { state=bits; }
static void vk_bind_index_ext(int count,const glIndex_t *indexes) { indices=indexes; indexCount=count; }
static void vk_bind_geometry(int flags) { assert(flags==(TESS_RGBA0|TESS_ST0)); }
static void vk_draw_geometry(int range,qboolean indexed) { assert(range==DEPTH_RANGE_NORMAL && indexed); Record(indexCount,indices); }
struct vk_material_t { int descriptor; };
static void vk_material_init(vk_material_t *material) { material->descriptor=-1; }
static int R_AnimatedImageDescriptor(const textureBundle_t *bundle) { return bundle->id; }
static int R_ImageDescriptor(int *) { return 0; }
static void vk_material_set_descriptor(vk_material_t *material,int slot,int descriptor) { assert(slot==VK_DESC_TEXTURE0); material->descriptor=descriptor; }
static void vk_bind_material(const vk_material_t *material) { texture=material->descriptor; }
#include "code/renderercommon/tr_rtcw_light_backend.h"
int main() {
    shader_t surface{}; tess.shader=&surface;
    tess.numVertexes=6; tess.numIndexes=6; tess.dlightBits=1;
    const float points[6][3]={{-.2f,-.2f,0},{.2f,-.2f,0},{0,.2f,0},{2,0,0},{3,0,0},{2,1,0}};
    for(int i=0;i<6;++i) { std::memcpy(tess.xyz[i],points[i],sizeof(vec3_t)); tess.indexes[i]=i; }
    auto &light=backEnd.refdef.dlights[0]; light.radius=1; VectorSet(light.color,.25f,.5f,1);
    backEnd.refdef.num_dlights=1;
    for(int repeat:{0,1,2,64}) {
        draws.clear(); backEnd.pc={}; light.rtcwOverdraw=repeat;
        assert(RB_RTCWProjectLights()); assert(draws.size()==size_t(repeat+1));
        for(const auto &draw:draws) {
            assert(draw.texture==0 && draw.state==(GLS_SRCBLEND_DST_COLOR|GLS_DSTBLEND_ONE|GLS_DEPTHFUNC_EQUAL));
            assert(draw.indexes==std::vector<glIndex_t>({0,1,2}));
        }
        assert(backEnd.pc.c_dlightIndexes==3*(repeat+1) && backEnd.pc.c_totalIndexes==3*(repeat+1));
        assert(backEnd.pc.c_dlightVertexes==6);
    }
#ifdef USE_VULKAN
    assert(tess.svars.colors[0][0].rgba[0]==quarterByte && tess.svars.colors[0][0].rgba[1]==halfByte);
#elif defined(FNTECH3_RTCW_LIGHT_FLOAT_COLORS)
    assert(glColorType==GL_FLOAT && tess.dlightColors[0][0]==quarterByte/255.f);
#else
    assert(glColorType==GL_UNSIGNED_BYTE && rtcwLightColors[0][0]==quarterByte);
#endif
    shaderStage_t stages[3]={{GLS_SRCBLEND_ZERO,{{1}}},{GLS_DSTBLEND_ONE,{{2}}},{GLS_SRCBLEND_DST_COLOR,{{3}}}};
    shader_t material{3,0,0,{&stages[0],&stages[1],&stages[2],nullptr}}; light.rtcwShader=&material;
    draws.clear(); assert(RB_RTCWProjectLights()); assert(draws.size()==3);
    for(int i=0;i<3;++i) assert(draws[i].texture==i+1 && draws[i].state==(stages[i].stateBits|GLS_DEPTHFUNC_EQUAL));
    // Per-pixel traversal supplies a light directly, without a legacy mask.
    draws.clear(); tess.dlightBits=0; dlight_t selected=light;
    assert(RB_RTCWProjectLightsForLight(&selected) && draws.size()==3);
    tess.dlightBits=1;
    draws.clear(); backEnd.refdef.rtcw.snooperView=true; assert(!RB_RTCWProjectLights() && draws.empty()); backEnd.refdef.rtcw.snooperView=false;
    // No pass can include a non-finite vertex or invalid index.
    tess.xyz[0][0]=std::numeric_limits<float>::infinity(); assert(!RB_RTCWProjectLights() && draws.empty());
    tess.xyz[0][0]=points[0][0]; tess.indexes[0]=UINT32_MAX; assert(!RB_RTCWProjectLights() && draws.empty()); tess.indexes[0]=0;
    // Bit 31 is unsigned; the final original RTCW light remains addressable.
    backEnd.refdef.dlights[31]=light; backEnd.refdef.num_dlights=32; tess.dlightBits=UINT32_C(1)<<31;
    assert(RB_RTCWProjectLights() && draws.size()==3);
    draws.clear(); tess.numVertexes=SHADER_MAX_VERTEXES+1; assert(!RB_RTCWProjectLights() && draws.empty());
}
'''


def entity_fixture(renderer):
    header=(ROOT/'code'/renderer/'tr_local.h').read_text()
    source=(ROOT/'code'/renderer/'tr_light.c').read_text()
    fixture=COMMON+r'''
#include <cmath>
#include "code/renderercommon/tr_q3_ihv_model_light.h"
#define DLIGHT_AT_RADIUS 16
#define DLIGHT_MINIMUM_RADIUS 16
enum { RF_MINLIGHT=1, RF_LIGHTING_ORIGIN=2, RDF_NOWORLDMODEL=1, RDF_SNOOPERVIEW=4 };
struct dlight_t {
    vec3_t origin,color; float radius; bool linear;
    union { void *rtcwShader; void *dlshader; };
};
struct trRefdef_t { int rdflags,num_dlights; dlight_t *dlights; struct { bool snooperView; } rtcw; };
struct trRefEntity_t {
    struct {
        int renderfx; vec3_t origin,lightingOrigin,axis[3];
        struct { float hilightIntensity; int entityNum; } rtcw;
        struct { qboolean fullbright, darken; } stvef;
        fnQ3IHVEntityState_t q3IHV;
        float hilightIntensity; int entityNum; // Original-reference view, fixture only.
    } e;
    qboolean lightingCalculated; vec3_t ambientLight,directedLight,lightDir,shadowLightDir; int ambientLightInt;
};
static struct { const void *lightGridData; q3IHVModelLightNode_t *q3IHVModelLight; } world;
static struct { decltype(world) *world; float identityLight; int identityLightByte; vec3_t sunDirection; } tr{&world,1,255,{0,0,1}};
static struct { comGameId_t gameId; } ri;
static cvar_t mode{},shadow{},debug{};
static cvar_t *r_dlightMode=&mode,*r_shadows=&shadow,*r_debugLight=&debug;
static bool qglGenProgramsARB=true;
static int gridCalls,logCalls;
static void LogLight(trRefEntity_t *) { ++logCalls; }
static void R_SetupEntityLightingGrid(trRefEntity_t *entity) {
    ++gridCalls; VectorSet(entity->ambientLight,10,20,30); VectorSet(entity->directedLight,40,50,60); VectorSet(entity->lightDir,0,0,1);
}
float VectorNormalize(vec3_t v) {
    const float length=VectorLength(v);
    if(length) for(int axis=0;axis<3;++axis) v[axis]/=length;
    return length;
}
#define myftol(x) ((int)(x))
#include "code/renderercommon/tr_q3_ihv_model_light_render.h"
'''
    fixture+=function(header,'static ID_INLINE qboolean R_UsesRTCWLighting(')+'\n'
    fixture+=function(source if renderer=='renderer' else header,
                      'int R_GetDlightMode(' if renderer=='renderer' else 'static ID_INLINE int R_GetDlightMode(')+'\n'
    fixture+=function(source,'void R_SetupEntityLighting(')+'\n'
    return fixture


ENTITY_CHECKS=r'''
int main() {
    for(auto profile:{COM_GAME_QUAKE3,COM_GAME_QUAKELIVE,COM_GAME_RTCW_SP,COM_GAME_RTCW_MP})
    for(int value:{-1,0,1,2,3}) for(bool hardware:{false,true}) {
        ri.gameId=profile; mode.integer=value; qglGenProgramsARB=hardware;
#ifdef USE_PMLIGHT
        const bool allowed=true
#ifdef TEST_GLX
            && hardware
#endif
            ;
        assert(R_GetDlightMode()==(allowed ? value : 0));
#else
        assert(R_GetDlightMode()==0);
#endif
        trRefdef_t view{}; view.rdflags=RDF_NOWORLDMODEL;
        trRefEntity_t entity{}; entity.e.axis[0][0]=entity.e.axis[1][1]=entity.e.axis[2][2]=1;
        entity.e.rtcw.hilightIntensity=0.25f;
        R_SetupEntityLighting(&view,&entity);
        assert(entity.lightingCalculated && entity.ambientLight[0]==182 && entity.ambientLight[1]==182 && entity.ambientLight[2]==182);
        const auto saved=entity; R_SetupEntityLighting(&view,&entity); assert(!std::memcmp(&entity,&saved,sizeof(entity)));
        entity={}; entity.e.axis[0][0]=entity.e.axis[1][1]=entity.e.axis[2][2]=1;
        R_SetupEntityLighting(&view,&entity);
        assert(entity.ambientLight[0]==(R_UsesRTCWLighting() ? 150 : 182));
        if(R_UsesRTCWLighting()) {
            // The snooper retains RTCW's model lighting while its world light passes are suppressed.
            dlight_t light{}; light.radius=16; VectorSet(light.color,1,1,1);
            view.rtcw.snooperView=true; view.num_dlights=1; view.dlights=&light;
            entity={}; entity.e.axis[0][0]=entity.e.axis[1][1]=entity.e.axis[2][2]=1;
            R_SetupEntityLighting(&view,&entity);
            assert(entity.ambientLight[0]==245 && entity.directedLight[0]==166);
        }
    }
    // Elite Force brightens (0x10) or darkens (0x1) by 100 in place of the
    // minimum add of 32; brightening wins, and darkening stops at zero.
    for(auto profile:{COM_GAME_STVEF_SP,COM_GAME_STVEF_MP}) {
        ri.gameId=profile; mode.integer=0;
        trRefdef_t view{}; view.rdflags=RDF_NOWORLDMODEL;
        const struct { bool fullbright,darken; float ambient; } cases[]={
            {false,false,182},{true,false,250},{false,true,50},{true,true,250}};
        for(const auto &c:cases) {
            trRefEntity_t entity{}; entity.e.axis[0][0]=entity.e.axis[1][1]=entity.e.axis[2][2]=1;
            entity.e.stvef.fullbright=c.fullbright ? qtrue : qfalse; entity.e.stvef.darken=c.darken ? qtrue : qfalse;
            R_SetupEntityLighting(&view,&entity);
            assert(entity.ambientLight[0]==c.ambient && entity.ambientLight[1]==c.ambient && entity.ambientLight[2]==c.ambient);
        }
        // From the light grid's 10, 20, 30.
        world.lightGridData=&world; view.rdflags=0;
        trRefEntity_t entity{}; entity.e.axis[0][0]=entity.e.axis[1][1]=entity.e.axis[2][2]=1;
        entity.e.stvef.darken=qtrue; R_SetupEntityLighting(&view,&entity);
        assert(entity.ambientLight[0]==0 && entity.ambientLight[1]==0 && entity.ambientLight[2]==0);
        entity={}; entity.e.axis[0][0]=entity.e.axis[1][1]=entity.e.axis[2][2]=1;
        entity.e.stvef.fullbright=qtrue; R_SetupEntityLighting(&view,&entity);
        assert(entity.ambientLight[0]==110 && entity.ambientLight[1]==120 && entity.ambientLight[2]==130);
        world.lightGridData=nullptr;
    }
}
'''


def material_fixture(renderer):
    source=(ROOT/'code'/renderer/'tr_shader.c').read_text()
    start=source.index('\tvertexLightMode =',source.index('// Vertex-light mode must consume'))
    end=source.index('\n\t/* RTCW asks for a diffuse-lit copy',start)
    collapse=source[start:end]
    multi_start=source.index('\tif ( !shader.rtcwLightMaterial && !R_Q3IHVSeparateSkyStages() && r_ext_multitexture->integer )')
    multitexture=source[multi_start:source.index('\n\t}',multi_start)+3]
    color='rgbGen' if renderer=='renderer' else 'bundle[0].rgbGen'
    alpha='alphaGen' if renderer=='renderer' else 'bundle[0].alphaGen'
    return shader_common(renderer)+'\n#define RGB(s) ((s).'+color+')\n#define ALPHA(s) ((s).'+alpha+')\n'+r'''
enum { CGEN_IDENTITY,AGEN_SKIP,GLS_SRCBLEND_DST_COLOR=1,GLS_DSTBLEND_ZERO=2,
    GLS_DEPTHMASK_TRUE=4,GLS_DEPTHTEST_DISABLE=8,GLS_DEPTHFUNC_EQUAL=16,GLS_BLEND_BITS=3 };
struct shaderStage_t {
    int rgbGen,alphaGen; unsigned stateBits; qboolean active;
    struct { image_t *image[1]; int numImageAnimations,rgbGen,alphaGen,dlight; } bundle[1];
};
static shaderStage_t stages[4];
static struct { qboolean noVLcollapse,rtcwLightMaterial; bool ihvModelLightmap;
    struct {int ihvClouds,ihvLightning;} sky; } shader;
'''+function((ROOT/'code/renderercommon/tr_q3_ihv_shader.h').read_text(), 'static qboolean R_Q3IHVSeparateSkyStages(')+r'''

static image_t white{}; static struct { image_t *whiteImage; } tr{&white};
static struct { int hardwareType; } glConfig;
static cvar_t vertex{},fullscreen{},multi{};
static cvar_t *r_vertexLight=&vertex,*r_ext_multitexture=&multi;
static struct { cvar_t *uiFullscreen; } qlRendererCvars{&fullscreen};
static int vertexCollapses,multiCollapses;
static void VertexLightingCollapse() { ++vertexCollapses; }
template<class... T> static int CollapseMultitexture(T...) { ++multiCollapses; return 1; }
static int Finish(int stage) {
    bool vertexLightMode; qboolean hasLightmapStage=qfalse; int i;
'''+collapse+'\n'+multitexture+r'''
    return stage;
}
int main() {
    for(auto profile:{COM_GAME_RTCW_SP,COM_GAME_RTCW_MP}) {
        ri.gameId=profile; shader.rtcwLightMaterial=qtrue;
        vertex.integer=0; multi.integer=1;
        stages[0]={}; stages[1]={}; stages[2]={};
        assert(Finish(3)==3 && !vertexCollapses && !multiCollapses);
        stages[0].bundle[0].image[0]=tr.whiteImage; RGB(stages[0])=CGEN_IDENTITY; ALPHA(stages[0])=AGEN_SKIP;
        stages[1].stateBits=GLS_SRCBLEND_DST_COLOR|GLS_DSTBLEND_ZERO;
        assert(Finish(2)==2); // Lighting uses authored textures/blends, so white-stage algebra cannot remove a pass.
        vertex.integer=1;
        vertexCollapses=0; assert(Finish(3)==(profile==COM_GAME_RTCW_SP ? 1 : 3));
        assert(vertexCollapses==(profile==COM_GAME_RTCW_SP)); // Original MP disables vertex-light collapse.
        shader.noVLcollapse=qtrue; vertexCollapses=0; assert(Finish(3)==3 && !vertexCollapses); shader.noVLcollapse=qfalse;
        fullscreen.integer=1; assert(Finish(3)==3); fullscreen.integer=0;
    }
    // IHV cloud colors belong to separate authored stages, even when their
    // generic color generators would allow a retail optimization.
    ri.gameId=COM_GAME_QUAKE3; shader.rtcwLightMaterial=qfalse;
    for(int mode:{1,2,3}) {
        shader.sky.ihvClouds=mode&1; shader.sky.ihvLightning=mode&2;
        for(int lighting:{0,1}) {
            vertex.integer=lighting; multi.integer=1; vertexCollapses=multiCollapses=0;
            stages[0]={}; stages[1]={}; stages[2]={};
            stages[0].bundle[0].image[0]=tr.whiteImage; RGB(stages[0])=CGEN_IDENTITY; ALPHA(stages[0])=AGEN_SKIP;
            stages[1].stateBits=GLS_SRCBLEND_DST_COLOR|GLS_DSTBLEND_ZERO;
            assert(Finish(2)==2 && !vertexCollapses && !multiCollapses);
            std::swap(stages[0],stages[1]);
            assert(Finish(2)==2 && !vertexCollapses && !multiCollapses);
        }
    }
    shader.sky={};
    // Ordinary Q3/QL shader optimization remains active.
    for(auto profile:{COM_GAME_QUAKE3,COM_GAME_QUAKELIVE}) {
        ri.gameId=profile; shader.rtcwLightMaterial=qfalse;
        vertex.integer=1; vertexCollapses=multiCollapses=0; assert(Finish(3)==1 && vertexCollapses==1);
        vertex.integer=0; multi.integer=0; stages[0]={}; stages[1]={};
        stages[0].bundle[0].image[0]=tr.whiteImage; RGB(stages[0])=CGEN_IDENTITY; ALPHA(stages[0])=AGEN_SKIP;
        stages[1].stateBits=GLS_SRCBLEND_DST_COLOR|GLS_DSTBLEND_ZERO;
        assert(Finish(2)==1);
        stages[0]={}; stages[1]={}; stages[2]={}; multi.integer=1; multiCollapses=0;
        assert(Finish(3)==2 && multiCollapses==1);
    }
}
'''


class RTCWLightTests(unittest.TestCase):
    def compile_run(self, source, definitions=()):
        compiler=shutil.which('clang++') or shutil.which('g++')
        if not compiler: self.skipTest('C++ compiler unavailable')
        flags=compiler_target_args(compiler)
        if os.name=='nt' and not flags: flags+=['-DFNTECH3_Q3_ONLY']
        (ROOT/'.tmp').mkdir(parents=True,exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='rtcw-light-',dir=ROOT/'.tmp') as temporary:
            folder=Path(temporary); path=folder/'probe.cpp'; path.write_text(source)
            executable=folder/('probe.exe' if os.name=='nt' else 'probe')
            build=subprocess.run([compiler,'-std=c++17',*flags,*definitions,'-I',str(ROOT),str(path),'-o',str(executable)],
                                 cwd=ROOT,capture_output=True,text=True,timeout=60)
            self.assertEqual(build.returncode,0,build.stdout+build.stderr)
            run=subprocess.run([str(executable)],cwd=ROOT,capture_output=True,text=True,timeout=20)
            self.assertEqual(run.returncode,0,run.stdout+run.stderr)

    def test_scene_parameters_and_projection(self):
        self.compile_run(SCENE)

    def test_per_pixel_scene_and_flickering_shadow_projection(self):
        self.compile_run(SCENE,('-DUSE_PMLIGHT',))

    def test_rtcw_models_reach_each_shadow_backend(self):
        prelude=COMMON+r'''
enum surfaceType_t { SF_BAD, SF_FACE, SF_TRIANGLES, SF_MDS, SF_MDC, SF_MD3, SF_MDR, SF_IQM, SF_GRID };
enum { SS_OPAQUE=3, GLS_ATEST_BITS=7 };
struct shaderStage_t { unsigned stateBits; };
struct shader_t { shader_t *remappedShader; int sort,isSky,polygonOffset,numDeforms,surfaceFlags,numUnfoggedPasses; shaderStage_t *stages[2]; };
static cvar_t curves{}; static cvar_t *r_nocurves=&curves;
enum { MOD_BAD, MOD_MESH, MOD_MDS, MOD_MDC, MOD_MDR, MOD_IQM, MOD_BRUSH, RT_MODEL,
       REFENTITYNUM_WORLD=1023, RF_NOSHADOW=1, RF_FIRST_PERSON=2, RF_DEPTHHACK=4 };
struct model_t { int type; } model;
struct trRefEntity_t { struct { int reType,renderfx,hModel; } e; } entity;
static struct { struct { int num_entities; trRefEntity_t *entities; } refdef; } backEnd;
static const model_t *R_GetModelByHandle(int handle) { return handle==1 ? &model : nullptr; }
'''
        checks=r'''
int main() {
    shader_t shader{}; shader.sort=SS_OPAQUE;
    for(auto type:{SF_MDS,SF_MDC,SF_MD3,SF_MDR,SF_IQM,SF_FACE,SF_TRIANGLES,SF_GRID}) {
        assert(RB_DlightShadowCasterAllowed(&shader,&type));
        shaderStage_t alpha{GLS_ATEST_BITS}; shader.stages[0]=&alpha; shader.numUnfoggedPasses=1;
        assert(!RB_DlightShadowCasterAllowed(&shader,&type)); shader.numUnfoggedPasses=0;
        shader.surfaceFlags=SURF_NODLIGHT; assert(!RB_DlightShadowCasterAllowed(&shader,&type)); shader.surfaceFlags=0;
        shader.numDeforms=1; assert(!RB_DlightShadowCasterAllowed(&shader,&type)); shader.numDeforms=0;
        shader.sort=SS_OPAQUE+1; assert(!RB_DlightShadowCasterAllowed(&shader,&type)); shader.sort=SS_OPAQUE;
    }
    surfaceType_t type=SF_MDS; shader_t hidden{}; hidden.sort=SS_OPAQUE+1;
    shader.remappedShader=&hidden; assert(!RB_DlightShadowCasterAllowed(&shader,&type));
    assert(!RB_DlightShadowCasterAllowed(nullptr,&type) && !RB_DlightShadowCasterAllowed(&shader,nullptr));
    entity.e.reType=RT_MODEL; entity.e.hModel=1;
    backEnd.refdef.num_entities=1; backEnd.refdef.entities=&entity;
    for(int kind:{MOD_MDS,MOD_MDC,MOD_MESH,MOD_MDR,MOD_IQM,MOD_BRUSH}) {
        model.type=kind; assert(RB_DlightShadowEntityCasterAllowed(0));
        for(int flag:{RF_NOSHADOW,RF_FIRST_PERSON,RF_DEPTHHACK}) {
            entity.e.renderfx=flag; assert(!RB_DlightShadowEntityCasterAllowed(0));
        }
        entity.e.renderfx=0;
    }
    assert(RB_DlightShadowEntityCasterAllowed(REFENTITYNUM_WORLD));
    assert(!RB_DlightShadowEntityCasterAllowed(-1) && !RB_DlightShadowEntityCasterAllowed(1));
    model.type=MOD_BAD; assert(!RB_DlightShadowEntityCasterAllowed(0));
    entity.e.hModel=0; assert(!RB_DlightShadowEntityCasterAllowed(0));
}
'''
        for renderer in ('renderer','renderervk','rendererrtx'):
            source=(ROOT/'code'/renderer/'tr_backend.c').read_text()
            body=''.join(function(source,name) for name in (
                'static qboolean RB_ShadowShaderHasAlphaTest(',
                'static const shader_t *RB_DlightShadowEffectiveShader(',
                'static qboolean RB_DlightShadowCasterAllowed(',
                'static qboolean RB_DlightShadowEntityCasterAllowed('))
            with self.subTest(renderer=renderer): self.compile_run(prelude+body+checks)

    def test_legacy_build_keeps_light_boundaries_in_ieee_mode(self):
        makefile=(ROOT/'Makefile').read_text()
        self.assertIn('RENDERER_FP_CFLAGS = $(if $(filter tr_scene.c tr_shade.c,$(notdir $<)),-fno-fast-math)',makefile)
        for recipe in ('DO_REND_CC','DO_GLX_REND_CC','DO_RENDV_CC','DO_RENDRTX_CC'):
            body=re.search(r'define '+recipe+r'\n(.*?)\nendef',makefile,re.S)[1]
            self.assertLess(body.index('$(CFLAGS)'),body.index('$(RENDERER_FP_CFLAGS)'))
        self.compile_run(SCENE,('-O2','-ffast-math','-fno-fast-math'))

    def test_projected_passes_for_each_backend(self):
        for flags in (('-DFNTECH3_RTCW_LIGHT_FLOAT_COLORS',), ('-DUSE_VULKAN',),
                      ('-DUSE_VULKAN','-DFNTECH3_RTCW_LIGHT_RTX'), ()):
            with self.subTest(definitions=flags): self.compile_run(BACKEND,flags)

    def test_profile_mode_and_model_lighting(self):
        for renderer in ('renderer','renderervk','rendererrtx'):
            for pmlight in (False,True):
                flags=[]
                if renderer=='renderer': flags+=['-DTEST_GLX']
                if pmlight: flags+=['-DUSE_PMLIGHT']
                with self.subTest(renderer=renderer,pmlight=pmlight):
                    self.compile_run(entity_fixture(renderer)+ENTITY_CHECKS,flags)

    def test_material_pass_preservation(self):
        for renderer in ('renderer','renderervk','rendererrtx'):
            with self.subTest(renderer=renderer): self.compile_run(material_fixture(renderer))


if __name__=='__main__': unittest.main()
