"""Run the production MDR loader, lifetime registry and scene guard for every backend."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest
from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]
BACKENDS = ('renderer', 'renderervk', 'rendererrtx')

PRELUDE = r'''
#define _CRT_SECURE_NO_WARNINGS 1
#include "code/qcommon/q_shared.h"
#include "code/qcommon/qfiles.h"
#include "code/qcommon/game_profile.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>
#include <float.h>
enum { MOD_BAD, MOD_MDR, SF_MDR, LIGHTMAP_NONE = -1, MAX_MOD_KNOWN = 16, MAX_REFENTITIES = 4 };
typedef struct { int type,dataSize,numLods,index; void *modelData; char name[MAX_QPATH]; } model_t;
typedef struct { int defaultShader,index; } shader_t;
static struct { qboolean registered; int numModels; model_t *models[MAX_MOD_KNOWN]; } tr;
static struct { struct { struct { qhandle_t hModel; } e; } entities[MAX_REFENTITIES]; } sceneStorage, *backEndData=&sceneStorage;
static int r_numentities,r_firstSceneEntity;
static char r_mdcNormals;
static unsigned char *allocations[128];
static int allocationCount,shaderCount,flushes,flushReset,filenameReads;
static unsigned char *flushExpect;
static const unsigned char *flushSaved;
static size_t flushBytes;
static void Print(int level,const char *format,...) { (void)level;(void)format; }
static void *Allocate(int bytes,ha_pref preference) {
    void *out;assert(bytes>0 && preference==h_low && allocationCount<128);
    out=calloc(1,(size_t)bytes);assert(out);allocations[allocationCount++]=(unsigned char *)out;return out;
}
static struct { void (*Printf)(int,const char *,...); void *(*Hunk_Alloc)(int,ha_pref); comGameId_t gameId; } ri={Print,Allocate,COM_GAME_JKA_MP};
static shader_t *R_FindShader(const char *name,int lightmap,qboolean raw) {
    static shader_t shader={0,42};assert(name && memchr(name,0,64) && lightmap==LIGHTMAP_NONE && raw);++shaderCount;return &shader;
}
char *Q_strlwr(char *s) { char *p;for(p=s;*p;++p)if(*p>='A' && *p<='Z')*p+='a'-'A';return s; }
void NORETURN QDECL Com_Error(errorParm_t level,const char *format,...) { (void)level;(void)format;abort(); }
void Com_Printf(const char *format,...) { (void)format; }
int Q_stricmpn(const char *a,const char *b,int n) { for(int i=0;i<n;++i) { unsigned char x=(unsigned char)a[i],y=(unsigned char)b[i];if(x>='A' && x<='Z')x+=32;if(y>='A' && y<='Z')y+=32;if(x!=y)return (int)x-y;if(!x)return 0; }return 0; }
static void FnMdc_InitNormals(void *out) { assert(out==&r_mdcNormals); }
void R_ModelInit(void);
void R_IssueRuntimeMDRCommands(void) {
    ++flushes;
    if(flushExpect)assert(!memcmp(flushExpect,flushSaved,flushBytes));
    if(flushReset) { flushReset=0;R_ModelInit(); }
}
'''

CHECKS = r'''
#include "code/rendererrtx/rtx_mdr_geometry.h"
static unsigned char input[2048];
static uint32_t length,frames,lod,surface,vertices,triangles;
static const float identity[24]={1,0,0,0,0,1,0,0,0,0,1,0, 1,0,0,0,0,1,0,0,0,0,1,0};
static const float posed[24]={0,-1,0,10,1,0,0,20,0,0,1,30, 2,0,0,0,0,2,0,0,0,0,2,0};
static const vec3_t lo={-1,-1,-1},hi={32,32,40};
static void Put32(byte *p,uint32_t value) { int i;for(i=0;i<4;++i)p[i]=(byte)(value>>(i*8)); }
static void PutFloat(byte *p,float value) { uint32_t bits;memcpy(&bits,&value,4);Put32(p,bits); }
static void Make(void) {
    byte *s;uint32_t v,o;
    frames=104;lod=frames+56+96;surface=lod+12;vertices=surface+168;triangles=vertices+64+44+44;length=triangles+12;
    memset(input,0,sizeof(input));Put32(input,MDR_IDENT);Put32(input+4,MDR_VERSION);memcpy(input+8,"owned",6);
    Put32(input+72,1);Put32(input+76,2);Put32(input+80,frames);Put32(input+84,1);Put32(input+88,lod);
    Put32(input+96,length);Put32(input+100,length);
    for(v=0;v<3;++v) { PutFloat(input+frames+v*4,-64);PutFloat(input+frames+12+v*4,64); }
    PutFloat(input+frames+36,112);memcpy(input+frames+56,identity,sizeof(identity));
    Put32(input+lod,1);Put32(input+lod+4,12);Put32(input+lod+8,length-lod);
    s=input+surface;Put32(s,SF_MDR);memcpy(s+4,"BODY",5);memcpy(s+68,"textures/synthetic",19);
    Put32(s+136,0u-surface);Put32(s+140,3);Put32(s+144,vertices-surface);Put32(s+148,1);Put32(s+152,triangles-surface);
    Put32(s+160,length-surface);Put32(s+164,length-surface);o=vertices;
    for(v=0;v<3;++v) {
        uint32_t w,count=v?1:2;PutFloat(input+o+8,1);PutFloat(input+o+12,(float)v*.5f);Put32(input+o+20,count);
        for(w=0;w<count;++w) {
            byte *weight=input+o+24+w*20;Put32(weight,w);PutFloat(weight+4,count==1?1:w?.75f:.25f);
            PutFloat(weight+8,1);PutFloat(weight+12,2+(float)v);PutFloat(weight+16,3);
        }
        o+=24+count*20;
    }
    Put32(input+triangles,0);Put32(input+triangles+4,2);Put32(input+triangles+8,1);
}
static void RejectInput(uint32_t offset,uint32_t value) {
    int allocationsBefore=allocationCount,shadersBefore=shaderCount,modelsBefore=tr.numModels,flushBefore=flushes;
    byte bad[2048];memcpy(bad,input,length);Put32(bad+offset,value);
    assert(!RE_RegisterRuntimeMDR(100,bad,(int)length));
    assert(allocationCount==allocationsBefore && shaderCount==shadersBefore && tr.numModels==modelsBefore && flushes==flushBefore);
}
static void CheckRollback(qhandle_t handle,uint64_t nonce,const float *palette,int count,const vec3_t mins,const vec3_t maxs) {
    model_t *model=tr.models[handle];byte before[2048];assert(model->dataSize<(int)sizeof(before));
    if(model->dataSize)memcpy(before,model->modelData,(size_t)model->dataSize);
    assert(!RE_UpdateRuntimeMDRPose(handle,nonce,palette,count,mins,maxs));
    if(model->dataSize)assert(!memcmp(before,model->modelData,(size_t)model->dataSize));
}
static void PoseCheck(qhandle_t handle) {
    model_t *model=tr.models[handle];mdrHeader_t *header=(mdrHeader_t *)model->modelData;
    mdrFrame_t *frame=(mdrFrame_t *)((byte *)header+header->ofsFrames);
    mdrBone_t palette[MDR_MAX_BONES];const mdrLOD_t *level;const mdrSurface_t *s;const mdrVertex_t *v;
    vec3_t position,normal;byte geometryBefore[2048];size_t geometryBytes=(size_t)header->ofsEnd-header->ofsLODs;
    memcpy(geometryBefore,(byte *)header+header->ofsLODs,geometryBytes);
    assert(RE_UpdateRuntimeMDRPose(handle,1,posed,2,lo,hi));
    assert(!memcmp(geometryBefore,(byte *)header+header->ofsLODs,geometryBytes));
    assert(RTX_MDRPreparePose(header,(size_t)model->dataSize,0,0,qfalse,0,palette));
    level=RTX_MDRGetLOD(header,0);assert(level && level->numSurfaces==1);
    s=(const mdrSurface_t *)((const byte *)level+level->ofsSurfaces);
    assert(RTX_MDRValidateSurface(s,(size_t)s->ofsEnd,2));
    v=(const mdrVertex_t *)((const byte *)s+s->ofsVerts);assert(RTX_MDRSkinVertex(v,palette,position,normal));
    assert(position[0]==3.5f && position[1]==8.25f && position[2]==12.75f);
    assert(normal[0]==0 && normal[1]==0 && normal[2]==1.75f && v->texCoords[0]==0);
    assert(frame->localOrigin[0]==15.5f && frame->localOrigin[2]==19.5f);
    assert((double)frame->radius*frame->radius>=16.5*16.5*2+20.5*20.5);
    assert(s->shaderIndex==42 && ((mdrTriangle_t *)((byte *)s+s->ofsTriangles))->indexes[1]==2);
}
int main(void) {
    qhandle_t handle,newHandle;mdrHeader_t *header;model_t *model;mdrFrame_t *frame;byte sourceBefore[2048],saved[2048];
    float badPalette[24];vec3_t badBounds;int before;
    tr.registered=qtrue;R_ModelInit();Make();memcpy(sourceBefore,input,length);
    assert(tr.numModels==1 && tr.models[0]->type==MOD_BAD);
    assert(!RE_RegisterRuntimeMDR(0,input,(int)length));assert(!RE_RegisterRuntimeMDR(1,NULL,(int)length));
    assert(!RE_RegisterRuntimeMDR(1,input,-1));assert(!RE_RegisterRuntimeMDR(1,input,FN_RUNTIME_MDR_MAX_BYTES+1));
    for(uint32_t n=0;n<length;++n)assert(!RE_RegisterRuntimeMDR(1,input,(int)n));
    RejectInput(72,2);RejectInput(84,2);RejectInput(92,1);RejectInput(80,0u-frames);RejectInput(100,length+4);
    RejectInput(frames,0x7fc00000);RejectInput(frames+36,0x7f800000);RejectInput(frames+56,0xff800000);
    RejectInput(vertices,0x7fc00000);RejectInput(vertices+12,0x7f800000);RejectInput(vertices+28,0xbf800000);
    RejectInput(vertices+32,0x7fc00000);RejectInput(vertices+24,2);RejectInput(triangles,3);
    handle=RE_RegisterRuntimeMDR(1,input,(int)length);assert(handle==1 && !memcmp(sourceBefore,input,length));
    model=tr.models[handle];header=(mdrHeader_t *)model->modelData;frame=(mdrFrame_t *)((byte *)header+header->ofsFrames);
    before=allocationCount;
    assert((byte *)header!=input && R_IsRuntimeMDR(handle) && !RegisterFilenameProbe(model->name));
    assert(!RegisterFilenameProbe("@runtime/mdr_guess.mdr") && !RegisterFilenameProbe("@RUNTIME/mdr_guess.mdr"));
    assert(allocationCount==before && filenameReads==0);
    strcpy(tr.models[0]->name,"unused");
    { model_t *ordinary=R_AllocModel();ordinary->type=MOD_MDR;strcpy(ordinary->name,"ordinary.mdr");assert(RegisterFilenameProbe("ordinary.mdr")==ordinary->index);assert(!R_IsRuntimeMDR(ordinary->index));CheckRollback(ordinary->index,1,posed,2,lo,hi); }
    before=allocationCount;assert(!RE_RegisterRuntimeMDR(1,input,(int)length) && allocationCount==before);
    CheckRollback(handle,2,posed,2,lo,hi);CheckRollback(handle,1,NULL,2,lo,hi);CheckRollback(handle,1,posed,1,lo,hi);
    CheckRollback(handle,1,posed,2,NULL,hi);CheckRollback(handle,1,posed,2,hi,lo);
    for(int n=0;n<24;++n) { memcpy(badPalette,posed,sizeof(posed));Put32((byte *)&badPalette[n],0x7fc00000);CheckRollback(handle,1,badPalette,2,lo,hi); }
    memcpy(badBounds,lo,sizeof(lo));Put32((byte *)&badBounds[1],0xff800000);CheckRollback(handle,1,posed,2,badBounds,hi);
    { const vec3_t hugeLo={-FLT_MAX,-FLT_MAX,-FLT_MAX},hugeHi={FLT_MAX,FLT_MAX,FLT_MAX};CheckRollback(handle,1,posed,2,hugeLo,hugeHi); }
    { int field=header->numFrames;header->numFrames=2;CheckRollback(handle,1,posed,2,lo,hi);header->numFrames=field; }
    { int field=header->ofsFrames;header->ofsFrames=INT_MAX;CheckRollback(handle,1,posed,2,lo,hi);header->ofsFrames=field; }
    { int field=model->dataSize;--model->dataSize;CheckRollback(handle,1,posed,2,lo,hi);model->dataSize=field; }
    { void *pointer=model->modelData;model->modelData=input;assert(!RE_UpdateRuntimeMDRPose(handle,1,posed,2,lo,hi));model->modelData=pointer; }
    before=flushes;backEndData->entities[0].e.hModel=handle;r_numentities=1;r_firstSceneEntity=0;
    CheckRollback(handle,1,posed,2,lo,hi);assert(flushes==before);
    r_firstSceneEntity=1;memcpy(saved,header,(size_t)model->dataSize);flushExpect=(byte *)header;flushSaved=saved;flushBytes=(size_t)model->dataSize;
    PoseCheck(handle);flushExpect=NULL;assert(flushes==before+1);
    r_firstSceneEntity=0;before=flushes;assert(RE_UpdateRuntimeMDRPose(handle,1,posed,2,lo,hi) && flushes==before);
    CheckRollback(handle,1,identity,2,lo,hi);assert(flushes==before);
    backEndData->entities[0].e.hModel=2;assert(RE_UpdateRuntimeMDRPose(handle,1,identity,2,lo,hi) && flushes==before+1);
    r_firstSceneEntity=-1;CheckRollback(handle,1,posed,2,lo,hi);r_firstSceneEntity=0;
    backEndData=NULL;CheckRollback(handle,1,posed,2,lo,hi);backEndData=&sceneStorage;
    r_numentities=r_firstSceneEntity=0;memcpy(saved,header,(size_t)model->dataSize);flushReset=1;
    assert(!RE_UpdateRuntimeMDRPose(handle,1,posed,2,lo,hi));assert(!memcmp(saved,header,(size_t)model->dataSize));
    assert(!R_IsRuntimeMDR(handle) && !RE_UpdateRuntimeMDRPose(handle,1,posed,2,lo,hi));
    newHandle=RE_RegisterRuntimeMDR(2,input,(int)length);assert(newHandle==handle);
    assert(!RE_UpdateRuntimeMDRPose(newHandle,1,posed,2,lo,hi));assert(RE_UpdateRuntimeMDRPose(newHandle,2,posed,2,lo,hi));
    before=allocationCount;flushReset=1;assert(!RE_RegisterRuntimeMDR(3,input,(int)length));assert(allocationCount==before+1 && tr.numModels==1);
    newHandle=RE_RegisterRuntimeMDR(4,input,(int)length);assert(newHandle==1);
    tr.registered=qfalse;assert(!RE_UpdateRuntimeMDRPose(newHandle,2,identity,2,lo,hi));assert(!RE_RegisterRuntimeMDR(3,input,(int)length));
    assert(!RE_UpdateRuntimeMDRPose(0,2,posed,2,lo,hi) && !RE_UpdateRuntimeMDRPose(MAX_MOD_KNOWN,2,posed,2,lo,hi));
    { rtxRtPackedVertex_t v[2];uint32_t initial;
      memset(v,0,sizeof(v));v[0].xyz[0]=v[1].xyz[0]=1;v[0].normal[2]=v[1].normal[2]=1;v[0].tangent[0]=v[1].tangent[0]=1;
      initial=RT_HashVertices(2166136261u,v,2);assert(RT_HashVertices(2166136261u,v,2)==initial);
      for(int word=4;word<12;++word) { byte *p=(byte *)&v[1]+word*4;PutFloat(p,2);assert(RT_HashVertices(2166136261u,v,2)!=initial);memcpy(&v[1],&v[0],sizeof(v[0])); }
      assert(RT_HashVertices(2166136261u,v,2)==initial);
    }
    tr.registered=qtrue;while(tr.numModels<MAX_MOD_KNOWN)assert(R_AllocModel());before=allocationCount;
    assert(!RE_RegisterRuntimeMDR(5,input,(int)length) && allocationCount==before);
    r_runtimeMdrGeneration=UINT64_MAX;R_ModelInit();assert(!r_runtimeMdrGeneration && !RE_RegisterRuntimeMDR(6,input,(int)length));
    (void)frame;for(int i=0;i<allocationCount;++i)free(allocations[i]);return 0;
}
'''

FRAME_PRELUDE = r'''
#include "code/qcommon/q_shared.h"
#include <assert.h>
#define USE_VULKAN 1
enum { RC_SET_COLOR, RC_DRAW_QUAD, RC_WOLFET_2D_POLYS, RC_STVEF_SCISSOR,
    RC_STRETCH_PIC, RC_DRAW_SURFS, RC_SCREENSHOT, RC_DRAW_BUFFER, RC_SWAP_BUFFERS,
    RC_FINISHBLOOM, RC_MENU_BLUR, RC_COLORMASK, RC_CLEARDEPTH, RC_CLEARCOLOR,
    RC_ALICE_DEBUG_LINES, RC_END_OF_LIST, R_SCREENSHOT_LOST_TO_MINIMIZE };
typedef struct { int used; unsigned char cmds[256]; } renderCommandList_t;
static struct { renderCommandList_t commands; } storage, *backEndData=&storage;
static struct { qboolean registered; } tr;
static struct { struct { int msec; } pc; int screenshotMask; void *stvefThumbnail;
    qboolean levelshotPending,screenshotCubeActive,screenshotCubeFrontPending,
        screenshotCubeFailed,throttle; char screenshotPNG[4],screenshotTGA[4],screenshotJPG[4],screenshotBMP[4]; } backEnd;
static struct { int integer; } skipValue, *r_skipBackEnd=&skipValue;
static int opened,ends,begins,drains,waits,graphEnds,pose,drawn[16],draws,pending,minimized;
static int Milliseconds(void) { return 1; }
static qboolean Minimized(void) { return minimized ? qtrue:qfalse; }
static void Set(const char *name,const char *value) { (void)name;(void)value; }
static struct { int (*Milliseconds)(void); qboolean (*CL_IsMinimized)(void);
    void (*Cvar_Set)(const char *,const char *); } ri={Milliseconds,Minimized,Set};
static void RB_EndSurface(void) { if(pending) { assert(opened);pending=0;++drains; } }
static void vk_end_frame(void) { if(opened) { opened=0;++ends; } }
static void vk_wait_idle(void) { ++waits; }
static void RB_FrameGraph_EndLifecycle(qboolean completed) { (void)completed;++graphEnds; }
static qboolean RE_CanMinimize(void) { return qtrue; }
static void R_ReportUnwrittenScreenshots(int mask,const char *png,const char *tga,const char *jpg,
    const char *bmp,qboolean level,qboolean cube,int reason) {
    (void)mask;(void)png;(void)tga;(void)jpg;(void)bmp;(void)level;(void)cube;(void)reason;
}
#define SKIP(name) static const void *name(const void *p) { return (const int *)p+1; }
SKIP(RB_SetColor) SKIP(RB_DrawQuad) SKIP(RB_DrawWolfET2DPolys) SKIP(RB_SetSTVEFScissor)
SKIP(RB_TakeScreenshotCmd) SKIP(RB_FinishBloom) SKIP(RB_MenuBlur) SKIP(RB_ColorMask)
SKIP(RB_ClearDepth) SKIP(RB_ClearColor) SKIP(RB_AliceDebugLines)
static const void *RB_DrawBuffer(const void *p) { assert(!opened);opened=1;++begins;return (const int *)p+1; }
static const void *RB_StretchPic(const void *p) { assert(opened);++pending;return (const int *)p+1; }
static const void *RB_DrawSurfs(const void *p) { assert(opened && draws<16);drawn[draws++]=pose;return (const int *)p+1; }
static const void *RB_SwapBuffers(const void *p) { assert(opened);RB_EndSurface();vk_end_frame();return (const int *)p+1; }
'''

FRAME_CHECKS = r'''
static void Queue(int id) {
    renderCommandList_t *list=&storage.commands;assert(list->used+8<(int)sizeof(list->cmds));
    memcpy(list->cmds+list->used,&id,4);list->used+=4;
}
int main(void) {
    tr.registered=qtrue;pose=1;Queue(RC_DRAW_BUFFER);
    R_IssueRuntimeMDRCommands();assert(opened && begins==1 && !ends && !waits && !graphEnds);
    Queue(RC_DRAW_SURFS);Queue(RC_STRETCH_PIC);
    R_IssueRuntimeMDRCommands();assert(draws==1 && drawn[0]==1 && drains==1 && opened && !graphEnds);
    pose=2;Queue(RC_DRAW_SURFS);R_IssueRuntimeMDRCommands();
    assert(draws==2 && drawn[1]==2 && opened && !ends && !waits);
    R_IssueRuntimeMDRCommands();assert(opened && begins==1 && !ends && !graphEnds);
    Queue(RC_SWAP_BUFFERS);R_IssueRenderCommands(qtrue);
    assert(!opened && ends==1 && graphEnds==EXPECT_GRAPH_ENDS);
    /* Existing full drains still submit and wait, including after a partial drain. */
    Queue(RC_DRAW_BUFFER);Queue(RC_DRAW_SURFS);R_IssuePendingRenderCommands();
    assert(!opened && begins==2 && ends==2 && waits==1 && draws==3);
    assert(graphEnds==2*EXPECT_GRAPH_ENDS);
    tr.registered=qfalse;Queue(RC_DRAW_BUFFER);R_IssueRuntimeMDRCommands();
    assert(storage.commands.used==4 && !opened);storage.commands.used=0;
    tr.registered=qtrue;skipValue.integer=1;Queue(RC_DRAW_BUFFER);R_IssueRuntimeMDRCommands();
    assert(!storage.commands.used && !opened && begins==2);skipValue.integer=0;
    minimized=1;Queue(RC_DRAW_BUFFER);R_IssueRuntimeMDRCommands();assert(!opened && !storage.commands.used);
    minimized=0;backEnd.throttle=qtrue;Queue(RC_DRAW_BUFFER);R_IssueRuntimeMDRCommands();assert(!opened);
    backEnd.throttle=qfalse;Queue(RC_DRAW_BUFFER);R_IssueRuntimeMDRCommands();assert(opened && begins==3);
    Queue(RC_SWAP_BUFFERS);R_IssueRenderCommands(qtrue);assert(!opened && ends==3);
    (void)RB_TakeScreenshotCmd;(void)RB_FrameGraph_EndLifecycle;return 0;
}
'''


def function(source, signature):
    start = source.index(signature)
    return source[start:source.index('\n}', start) + 2]


class RuntimeMdrTests(unittest.TestCase):
    def test_pose_drain_keeps_vulkan_frame_open(self):
        compiler = shutil.which('clang') or shutil.which('gcc')
        if not compiler:
            self.skipTest('C compiler unavailable')
        scratch = ROOT / '.tmp'
        scratch.mkdir(exist_ok=True)
        for backend in BACKENDS[1:]:
            cmds = (ROOT / f'code/{backend}/tr_cmds.c').read_text(encoding='utf-8')
            render = (ROOT / f'code/{backend}/tr_backend.c').read_text(encoding='utf-8')
            fixture = (FRAME_PRELUDE
                + function(render, 'static void RB_ExecuteRenderCommandsInternal(')
                + function(render, 'void RB_ExecuteRenderCommands(')
                + function(render, 'void RB_ExecuteRuntimeMDRCommands(')
                + function(cmds, 'static void R_IssueRenderCommands(')
                + function(cmds, 'void R_IssuePendingRenderCommands(')
                + function(cmds, 'void R_IssueRuntimeMDRCommands(')
                + f'\n#define EXPECT_GRAPH_ENDS {int(backend == "rendererrtx")}\n' + FRAME_CHECKS)
            with tempfile.TemporaryDirectory(prefix='runtime-mdr-frame-', dir=scratch) as folder:
                path = Path(folder)
                source = path / 'frame.c'
                executable = path / ('frame.exe' if os.name == 'nt' else 'frame')
                source.write_text(fixture, encoding='utf-8')
                args = [compiler, '-std=c11', '-Wall', '-Wextra', '-Werror', '-O2',
                    *compiler_target_args(compiler), '-I', str(ROOT), str(source), '-o', str(executable)]
                with self.subTest(backend=backend):
                    build = subprocess.run(args, cwd=ROOT, text=True, capture_output=True, timeout=60)
                    self.assertEqual(build.returncode, 0, build.stdout + build.stderr)
                    result = subprocess.run([str(executable)], cwd=ROOT, text=True, capture_output=True, timeout=15)
                    self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_production_registry_pose_and_scene_guard(self):
        compiler = shutil.which('clang') or shutil.which('gcc')
        if not compiler:
            self.skipTest('C compiler unavailable')
        scratch = ROOT / '.tmp'
        scratch.mkdir(exist_ok=True)
        vk = (ROOT / 'code/rendererrtx/vk.c').read_text(encoding='utf-8')
        struct_start = vk.index('typedef struct {\n\tfloat xyz[4];')
        packed_vertex = vk[struct_start:vk.index('} rtxRtPackedVertex_t;', struct_start) + len('} rtxRtPackedVertex_t;')]
        rt = (ROOT / 'code/rendererrtx/rt/rt_backend.c').read_text(encoding='utf-8')
        shared = (ROOT / 'code/qcommon/q_shared.c').read_text(encoding='utf-8')
        format_functions = ('\n#ifdef _WIN32\n'
            + function(shared, 'int Q_vsnprintf( char *str, size_t size, const char *format, va_list ap )')
            + '\n#endif\n' + function(shared, 'int QDECL Com_sprintf( char *dest, int size, const char *fmt, ...)'))
        hash_functions = (packed_vertex + '\n#define Com_Memcpy memcpy\n'
            + function(rt, 'static uint32_t RT_HashWords(')
            + function(rt, 'static uint32_t RT_HashVertices('))
        for backend in BACKENDS:
            model = (ROOT / f'code/{backend}/tr_model.c').read_text(encoding='utf-8')
            scene = (ROOT / f'code/{backend}/tr_scene.c').read_text(encoding='utf-8')
            cached_start = model.index('if ( !Q_stricmpn( name, "@runtime/", 9 ) ) return 0;')
            cached = model[cached_start:model.index('\n\t// allocate a new model_t', cached_start)]
            fixture = (PRELUDE + format_functions + function(model, 'model_t *R_AllocModel( void )')
                + function(scene, 'qboolean R_RuntimeMDRSceneUsesModel( qhandle_t handle )')
                + '\n#include "code/renderercommon/tr_mdr_model.h"\n'
                + '#include "code/renderercommon/tr_runtime_mdr.h"\n'
                + function(model, 'void R_ModelInit( void )')
                # Execute the actual preallocation filename path. Its fallthrough
                # is instrumented with real allocation and an FS-read witness.
                + '\nstatic qhandle_t RegisterFilenameProbe(const char *name) { model_t *mod;qhandle_t hModel;'
                + cached + '\nmod=R_AllocModel();++filenameReads;return mod?mod->index:0;}\n'
                + hash_functions + CHECKS)
            with tempfile.TemporaryDirectory(prefix='runtime-mdr-', dir=scratch) as folder:
                path = Path(folder)
                source = path / 'runtime.c'
                source.write_text(fixture, encoding='utf-8')
                for fast_math in (False, True):
                    # Each build links its own executable: Windows can keep the
                    # one just run locked, so relinking it fails with LNK1104.
                    name = 'runtime-fast' if fast_math else 'runtime'
                    executable = path / (name + '.exe' if os.name == 'nt' else name)
                    with self.subTest(backend=backend, fast_math=fast_math):
                        args = [compiler, '-std=c11', '-Wall', '-Wextra', '-Werror', '-O2',
                            *compiler_target_args(compiler), '-I', str(ROOT), str(source), '-o', str(executable)]
                        if os.name != 'nt':
                            args.append('-lm')
                        if fast_math:
                            args.append('-ffast-math')
                        build = subprocess.run(args, cwd=ROOT, text=True, capture_output=True, timeout=60)
                        self.assertEqual(build.returncode, 0, build.stdout + build.stderr)
                        result = subprocess.run([str(executable)], cwd=ROOT, text=True, capture_output=True, timeout=15)
                        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_engine_export_bindings(self):
        public = (ROOT / 'code/renderercommon/tr_public.h').read_text(encoding='utf-8')
        self.assertRegex(public, r'REF_API_VERSION\s+34')
        self.assertLess(public.index('(*GetTextureCapabilities)'), public.index('(*RegisterRuntimeMDR)'))
        self.assertLess(public.index('(*RegisterRuntimeMDR)'), public.index('(*UpdateRuntimeMDRPose)'))
        for backend in BACKENDS:
            source = (ROOT / f'code/{backend}/tr_init.c').read_text(encoding='utf-8')
            self.assertIn('re.RegisterRuntimeMDR = RE_RegisterRuntimeMDR;', source)
            self.assertIn('re.UpdateRuntimeMDRPose = RE_UpdateRuntimeMDRPose;', source)


if __name__ == '__main__':
    unittest.main()
