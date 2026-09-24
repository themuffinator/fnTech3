"""IHV particles: original numeric digest, owned scenes and actual renderer dispatch."""
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args
from mesh_tag_tests import function
from model_shader_tests import PRELUDE

ROOT = Path(__file__).resolve().parents[1]
RENDERERS = ('renderer', 'renderervk', 'rendererrtx')

GEOMETRY = r'''
#include "code/renderercommon/tr_q3_ihv_particles.h"
#include <cassert>
#include <array>
#include <vector>
#include <climits>
static uint32_t seed=0x186297ab;
static uint32_t Random(){seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;return seed;}
static uint64_t digest=14695981039346656037ull;
static void Hash(const void *input,size_t bytes){const auto *p=(const unsigned char*)input;for(size_t i=0;i<bytes;++i){digest^=p[i];digest*=1099511628211ull;}}
int main(){
 unsigned components=0;
 for(unsigned view=0;view<8;++view){
  float origin[3],axis[3][3];
  for(unsigned a=0;a<3;++a){origin[a]=float(int(Random()%4096)-2048)*.25f;for(unsigned j=0;j<3;++j)axis[j][a]=float(int(Random()%513)-256)/256;}
  if(!view){memset(origin,0,sizeof origin);memset(axis,0,sizeof axis);for(int a=0;a<3;++a)axis[a][a]=1;}
  for(int mirror:{0,1})for(unsigned count:{0u,1u,2u,340u,341u,342u,682u,2048u,4096u}){
   std::vector<fnQ3IHVParticle_t> points(count);
   for(auto &p:points){for(float &x:p.origin)x=float(int(Random()%16384)-8192)*.25f;const auto rgba=Random();memcpy(p.rgba,&rgba,4);}
   if(!view&&count>=4){points[0].origin[0]=19.999f;points[1].origin[0]=20;points[2].origin[0]=20.001f;points[3].origin[0]=-20;}
   fnQ3IHVParticleView_t prepared;assert(Q3IHV_ParticleView(origin,axis,mirror,&prepared));
   for(const auto &point:points){float xyz[3][3];assert(Q3IHV_ParticleTriangle(&prepared,point.origin,xyz));
    for(int v=0;v<3;++v){for(int a=0;a<3;++a){Hash(&xyz[v][a],4);++components;}const float uv[2]={v==1?1.125f:0,v==2?1.125f:0};Hash(uv,8);Hash(point.rgba,4);}
   }
  }
 }
 assert(components==1130688&&digest==UINT64_C(0xc4df4de645bbaffb));
 float eye[3]{},axis[3][3]={{1,0,0},{0,1,0},{0,0,1}},point[3]={20,2,3},xyz[3][3];
 fnQ3IHVParticleView_t prepared;assert(Q3IHV_ParticleView(eye,axis,0,&prepared));
 assert(Q3IHV_ParticleTriangle(&prepared,point,xyz));assert(xyz[1][2]==4.62f&&xyz[2][1]==.38f);
 assert(Q3IHV_ParticleView(eye,axis,1,&prepared));assert(Q3IHV_ParticleTriangle(&prepared,point,xyz));assert(xyz[2][1]==3.62f);
 for(uint32_t bad:{0x7f800000u,0xff800000u,0x7fc00123u,0xffc00001u}){
  for(int a=0;a<3;++a){float input[3]={1,2,3};memcpy(input+a,&bad,4);auto before=prepared;
   assert(!Q3IHV_ParticleView(input,axis,0,&prepared)&&!memcmp(&before,&prepared,sizeof before));
   memset(xyz,0x71,sizeof xyz);float old[3][3];memcpy(old,xyz,sizeof old);
   assert(!Q3IHV_ParticleTriangle(&prepared,input,xyz)&&!memcmp(old,xyz,sizeof old));
   for(int row=0;row<3;++row){float axes[3][3];memcpy(axes,axis,sizeof axes);memcpy(&axes[row][a],&bad,4);assert(!Q3IHV_ParticleView(eye,axes,0,&prepared));}
  }
 }
 axis[1][0]=FLT_MAX;assert(!Q3IHV_ParticleView(eye,axis,0,&prepared));axis[1][0]=0;
 axis[0][0]=FLT_MAX;assert(Q3IHV_ParticleView(eye,axis,0,&prepared));point[0]=FLT_MAX;
 memset(xyz,0x71,sizeof xyz);float old[3][3];memcpy(old,xyz,sizeof old);assert(!Q3IHV_ParticleTriangle(&prepared,point,xyz)&&!memcmp(old,xyz,sizeof old));
 assert(!Q3IHV_ParticleView(nullptr,axis,0,&prepared)&&!Q3IHV_ParticleView(eye,nullptr,0,&prepared)&&!Q3IHV_ParticleView(eye,axis,0,nullptr));
 assert(!Q3IHV_ParticleTriangle(nullptr,point,xyz)&&!Q3IHV_ParticleTriangle(&prepared,nullptr,xyz)&&!Q3IHV_ParticleTriangle(&prepared,point,nullptr));
 unsigned char image[1024];Q3IHV_ParticleImage(image);int opaque=0;
 for(int i=0;i<256;++i){assert(image[4*i]==image[4*i+1]&&image[4*i]==image[4*i+2]&&image[4*i]==image[4*i+3]);opaque+=image[4*i]==255;}
 assert(opaque==69&&image[(5*16+5)*4]==255&&!image[0]&&!image[1023]);
}
'''

HOST = r'''
#include "code/renderercommon/tr_q3_ihv_particles.h"
#include <array>
struct image_t {} image;
struct shader_t { int index=7,sort=SS_BLEND0; } shader;
struct Bundle { image_t *image[1]; int tcGen;
#ifdef USE_VULKAN
 int rgbGen,alphaGen;
#endif
};
struct Stage { Bundle bundle[1]; qboolean active; unsigned stateBits;
#ifndef USE_VULKAN
 int rgbGen,alphaGen;
#endif
} stages[1];
static int errors,images;
static void QDECL Error(int,const char*,...){++errors;}
static void QDECL Print(int,const char*,...){}
static struct { comGameId_t gameId; decltype(&Error) Error; decltype(&Print) Printf; } ri{COM_GAME_QUAKE3_IHV,Error,Print};
static struct { bool registered=true; shader_t *ihvParticleShader,*defaultShader=&shader; int currentEntityNum,shiftedEntityNum; struct {int numPolys; srfPoly_t *polys;} refdef; } tr;
struct View { struct {float origin[3],axis[3][3];} orientation; bool mirror; };
static struct { View viewParms; } backEnd;
#define or orientation
static int R_ViewPassIsMirror(const View *view){return view->mirror;}
static image_t *R_CreateImage(const char *name,const char *other,byte *rgba,int width,int height,imgFlags_t flags){
 assert(!strcmp(name,"*particle")&&!other&&width==16&&height==16&&flags==IMGFLAG_NONE);assert(rgba[0]==0&&rgba[340]==255);++images;return &image;
}
static void InitShader(const char *name,int lightmap){assert(!strcmp(name,"<particle>")&&lightmap==LIGHTMAP_NONE);stages[0]={};shader={};}
static shader_t *FinishShader(){return &shader;}
static shader_t *R_GetShaderByHandle(int handle){assert(handle==7);return &shader;}
static srfPoly_t polygons[16];static polyVert_t points[8192];
static struct { srfPoly_t *polys=polygons;polyVert_t *polyVerts=points;struct {int used;} commands; } data,*backEndData=&data;
static int max_polys=16,max_polyverts=8192,r_numpolys,r_numpolyverts,r_firstScenePoly;
static int r_numdlights,r_firstSceneDlight,r_numentities,r_firstSceneEntity,r_firstSceneDrawSurf,rtcwCoronaCount,rtcwCoronaFirst;
static void R_ResetWolfETDecalRequests(){}static void R_ResetWolfETDecalSurfaces(){}static void R_ClearMuzzleFlashLightCandidate(){}
static void R_ResetStaticMapLightFrameCounters(){}static void R_ResetSurfaceLightProxyFrameCounters(){}
static std::vector<const srfPoly_t*> submitted;
static void R_AddDrawSurf(void *surface,shader_t *material,int fog,int light){assert(material==&shader&&fog==0&&!light);submitted.push_back((const srfPoly_t*)surface);}
static struct {int numVertexes,numIndexes,surfType;float xyz[1024][4],normal[1024][4],texCoords[1][1024][2];color4ub_t vertexColors[1024];unsigned indexes[1024];} tess;
struct Vertex {std::array<float,3> xyz;std::array<float,2> uv;uint32_t color;};
static std::vector<Vertex> drawn;
static int flushes,vboFlushes,categories;
static void Flush(){
 for(int i=0;i<tess.numVertexes;++i){assert(tess.indexes[i]==unsigned(i)&&tess.xyz[i][3]==1);drawn.push_back({{tess.xyz[i][0],tess.xyz[i][1],tess.xyz[i][2]},{tess.texCoords[0][i][0],tess.texCoords[0][i][1]},tess.vertexColors[i].u32});}
 assert(tess.numIndexes==tess.numVertexes);if(tess.numVertexes)++flushes;tess.numVertexes=tess.numIndexes=0;
}
static void Overflow(int vertices,int indices){assert(vertices==3&&indices==3);if(tess.numVertexes+vertices>1024||tess.numIndexes+indices>1024)Flush();}
#define RB_CHECKOVERFLOW Overflow
static void VBO_Flush(){++vboFlushes;}
enum { GLX_DYNAMIC_CATEGORY_MASK_PARTICLE=2 };
static void GLX_CompatMarkDynamicCategory(unsigned flags){assert(flags==2);++categories;}
static void Ignore(void*){}
#include "code/renderercommon/tr_q3_ihv_particle_shader.h"
#include "code/renderercommon/tr_q3_ihv_particle_scene.h"
#include "code/renderercommon/tr_q3_ihv_particle_surface.h"
'''

CHECKS = r'''
int main(){
 for(auto profile:{COM_GAME_QUAKE3,COM_GAME_QUAKELIVE,COM_GAME_RTCW_SP,COM_GAME_RTCW_MP,COM_GAME_WOLFET,COM_GAME_STVEF_SP,COM_GAME_STVEF_MP}){
  ri.gameId=profile;R_CreateIHVParticleShader();assert(!images&&!tr.ihvParticleShader);int before=errors;assert(!RE_AddIHVParticlesToScene(0,nullptr)&&errors==before+1&&!r_numpolys);
 }
 ri.gameId=COM_GAME_QUAKE3_IHV;R_CreateIHVParticleShader();assert(images==1&&tr.ihvParticleShader==&shader);
 assert(stages[0].active&&stages[0].bundle[0].image[0]==&image&&stages[0].bundle[0].tcGen==TCGEN_TEXTURE);
 assert(stages[0].stateBits==(GLS_SRCBLEND_ONE|GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA)&&shader.sort==SS_BLEND0);
#ifdef USE_VULKAN
 assert(stages[0].bundle[0].rgbGen==CGEN_EXACT_VERTEX&&stages[0].bundle[0].alphaGen==AGEN_VERTEX);
#else
 assert(stages[0].rgbGen==CGEN_EXACT_VERTEX&&stages[0].alphaGen==AGEN_VERTEX);
#endif
 std::vector<fnQ3IHVParticle_t> input(4096);for(size_t i=0;i<input.size();++i){input[i].origin[0]=float(i)-12;input[i].origin[1]=3;input[i].origin[2]=7;for(int c=0;c<4;++c)input[i].rgba[c]=(byte)(i+c);}
 assert(RE_AddIHVParticlesToScene(0,nullptr)&&!r_numpolys);
 for(int n:{-1,INT_MIN,INT_MAX})assert(!RE_AddIHVParticlesToScene(n,input.data())&&!r_numpolys);
 assert(!RE_AddIHVParticlesToScene(1,nullptr));tr.registered=false;assert(!RE_AddIHVParticlesToScene(1,input.data()));tr.registered=true;
 assert(RE_AddIHVParticlesToScene(4096,input.data())&&r_numpolys==1&&r_numpolyverts==4096);
 assert(polygons[0].numVerts==4096&&polygons[0].surfaceType==SF_IHV_PARTICLES&&polygons[0].verts==points);
 input[0].origin[0]=999;assert(points[0].xyz[0]==-12);RE_ClearScene();assert(r_firstScenePoly==1);
 const auto first=points[0];assert(RE_AddIHVParticlesToScene(1,input.data())&&points[4096].xyz[0]==999&&!memcmp(&first,points,sizeof first));
 const int priorCount=r_numpolys,priorVerts=r_numpolyverts;
 for(uint32_t bad:{0x7f800000u,0xff800000u,0x7fc00001u}){memcpy(&input[1].origin[1],&bad,4);assert(!RE_AddIHVParticlesToScene(2,input.data())&&r_numpolys==priorCount&&r_numpolyverts==priorVerts);}
 input[1].origin[1]=3;max_polys=r_numpolys;assert(!RE_AddIHVParticlesToScene(1,input.data()));max_polys=16;
 max_polyverts=r_numpolyverts;assert(!RE_AddIHVParticlesToScene(1,input.data()));max_polyverts=8192;
 tr.refdef={2,polygons};R_AddPolygonSurfaces();assert(submitted.size()==2&&submitted[0]==polygons&&submitted[1]==polygons+1);
 for(int mirror:{0,1}){
  backEnd={};for(int i=0;i<3;++i)backEnd.viewParms.or.axis[i][i]=1;backEnd.viewParms.mirror=mirror;
  drawn.clear();flushes=0;rb_surfaceTable[SF_IHV_PARTICLES](polygons);Flush();assert(drawn.size()==4096*3&&flushes==13);
  assert(drawn[0].xyz[0]==-12&&drawn[1].xyz[2]==8.5f&&drawn[2].xyz[1]==(mirror?4.5f:1.5f));
  for(size_t i=0;i<drawn.size();++i){assert(drawn[i].uv[0]==(i%3==1?1.125f:0)&&drawn[i].uv[1]==(i%3==2?1.125f:0));assert(drawn[i].color==points[i/3].modulate.u32);}
 }
 backEnd.viewParms.or.origin[0]=-100;drawn.clear();rb_surfaceTable[SF_IHV_PARTICLES](polygons);Flush();assert(drawn[1].xyz[2]!=8.5f);
 for(auto profile:{COM_GAME_QUAKE3,COM_GAME_QUAKELIVE}){ri.gameId=profile;drawn.clear();rb_surfaceTable[SF_IHV_PARTICLES](polygons);Flush();assert(drawn.empty());}
 R_InitNextFrame();assert(!r_numpolys&&!r_numpolyverts&&!r_firstScenePoly);
}
'''


def fixture(renderer):
    local = (ROOT / 'code' / renderer / 'tr_local.h').read_text()
    common = (ROOT / 'code' / renderer / 'tr_common.h').read_text()
    source = PRELUDE + ('#define RENDERER_GLX\n' if renderer == 'renderer' else '#define USE_VULKAN\n')
    source += '#define USE_VBO\n'
    for token, name in [('SF_BAD', 'surfaceType_t'), ('SS_BAD', 'shaderSort_t'),
                        ('AGEN_IDENTITY', 'alphaGen_t'), ('CGEN_BAD', 'colorGen_t'),
                        ('TCGEN_BAD', 'texCoordGen_t')]:
        source += re.search(r'typedef enum\s*\{[^{}]*' + token + r'[^{}]*\}\s*' + name + ';', local)[0] + '\n'
    source += re.search(r'typedef enum\s*\{[^{}]*IMGFLAG_NONE[^{}]*\}\s*imgFlags_t;', common)[0] + '\n'
    source += '\n'.join(re.findall(r'^#define (?:GLS_|LIGHTMAP_)[^\n]+', local + '\n' + common, re.M)) + '\n'
    source += '#define REFENTITYNUM_WORLD 2047\n#define QSORT_REFENTITYNUM_SHIFT 7\n'
    source += re.search(r'typedef struct srfPoly_s\s*\{[^{}]*\}\s*srfPoly_t;', local)[0] + '\n'
    source += HOST
    scene = (ROOT / 'code' / renderer / 'tr_scene.c').read_text()
    for signature in ['void R_InitNextFrame(', 'void RE_ClearScene(', 'void R_AddPolygonSurfaces(']:
        source += function(scene, signature)
    surface = (ROOT / 'code' / renderer / 'tr_surface.c').read_text()
    table = surface[surface.index('void (*rb_surfaceTable'):]
    table = table[:table.index('};') + 2]
    table = re.sub(r'RB_Surface(?!IHVParticles)\w+|RB_MDRSurfaceAnim|RB_IQMSurfaceAnim', 'Ignore', table)
    return source + table + '\n'


RAY_HOST = r'''
#include "code/rendererrtx/rtx_scene_signature.h"
#include "code/renderercommon/tr_q3_ihv_model.h"
static cvar_t *r_lerpmodels;
enum { RTX_RT_MATFLAG_MASKED=1,RTX_RT_MAX_LIGHT_COORDINATE=1048576 };
static struct {struct {float dynamic_centroid_prev[3];} rt;} vk;
struct rtxRtCpuGeometry_t {
 uint32_t numVertices=0,sourceTriangleCount=0,maskedTriangleCount=0,sourceEffectCount=0;
 struct {unsigned flags=1;} materials[1];std::vector<Vertex> vertices;std::vector<unsigned> indices;
};
static int failure;
static qboolean vk_rt_checked_u32_multiply(uint32_t a,uint32_t b,uint32_t *out){if(b&&a>UINT32_MAX/b)return qfalse;*out=a*b;return qtrue;}
static qboolean vk_rt_cpu_geometry_find_or_add_material(rtxRtCpuGeometry_t*,const shader_t *material,uint32_t *out){assert(material==&shader);*out=0;return failure==1?qfalse:qtrue;}
static void vk_rt_apply_effect_material_tuning(rtxRtCpuGeometry_t*,uint32_t material,qboolean blend,qboolean decal){assert(!material&&blend&&!decal);}
static qboolean vk_rt_cpu_geometry_reserve(rtxRtCpuGeometry_t*,uint32_t vertices,uint32_t indices){assert(vertices==3&&indices==3);return failure==2?qfalse:qtrue;}
static qboolean vk_rt_cpu_geometry_add_vertex(rtxRtCpuGeometry_t *geometry,const vec3_t xyz,const vec3_t,const vec4_t,const vec2_t uv,uint32_t color,uint32_t,void*){
 if(failure==3)return qfalse;geometry->vertices.push_back({{xyz[0],xyz[1],xyz[2]},{uv[0],uv[1]},color});++geometry->numVertices;return qtrue;
}
static qboolean vk_rt_cpu_geometry_add_index(rtxRtCpuGeometry_t *geometry,uint32_t index){if(failure==4)return qfalse;geometry->indices.push_back(index);return qtrue;}
static qboolean vk_rt_entity_visible_in_current_view(const trRefEntity_t*){return qtrue;}
static qboolean vk_rt_model_casts_in_current_view(const trRefEntity_t*){return qtrue;}
static qboolean vk_rt_append_poly_geometry(rtxRtCpuGeometry_t*,const srfPoly_t*);
#include "code/renderercommon/tr_q3_ihv_particle_ray.h"
'''

RAY_CHECKS = r'''
int main(){
 R_CreateIHVParticleShader();for(int a=0;a<3;++a)backEnd.viewParms.or.axis[a][a]=1;
 std::vector<fnQ3IHVParticle_t> input(4096);for(size_t i=0;i<input.size();++i){input[i].origin[0]=float(i)-100;input[i].origin[1]=3;input[i].origin[2]=7;for(int c=0;c<4;++c)input[i].rgba[c]=(byte)(i+c);}
 for(int count:{0,1,2,340,341,342,4096})for(int mirror:{0,1}){
  R_InitNextFrame();assert(RE_AddIHVParticlesToScene(count,input.data()));
  if(!count){rtxRtCpuGeometry_t empty;assert(vk_rt_append_poly_geometry(&empty,nullptr)&&empty.vertices.empty());continue;}
  auto *poly=polygons;backEnd.viewParms.mirror=mirror;drawn.clear();rb_surfaceTable[SF_IHV_PARTICLES](poly);Flush();
  rtxRtCpuGeometry_t geometry;assert(vk_rt_append_poly_geometry(&geometry,poly));assert(geometry.numVertices==count*3u&&geometry.sourceTriangleCount==(unsigned)count&&geometry.maskedTriangleCount==(unsigned)count);
  assert(geometry.vertices.size()==drawn.size()&&geometry.indices.size()==drawn.size());
  for(size_t i=0;i<drawn.size();++i){assert(geometry.indices[i]==i);assert(geometry.vertices[i].xyz==drawn[i].xyz&&geometry.vertices[i].uv==drawn[i].uv&&geometry.vertices[i].color==drawn[i].color);}
 }
 for(failure=1;failure<=4;++failure){rtxRtCpuGeometry_t geometry;assert(!vk_rt_append_poly_geometry(&geometry,polygons));}failure=0;
 ri.gameId=COM_GAME_QUAKE3;rtxRtCpuGeometry_t wrong;assert(!vk_rt_append_poly_geometry(&wrong,polygons));ri.gameId=COM_GAME_QUAKE3_IHV;
 backEnd.refdef.numPolys=1;backEnd.refdef.polys=polygons;float centroid[3];
 const auto baseline=vk_rt_estimate_dynamic_scene_signature(centroid,nullptr,nullptr,1);
 assert(vk_rt_estimate_dynamic_scene_signature(centroid,nullptr,nullptr,1)==baseline);
 for(int a=0;a<3;++a){backEnd.viewParms.or.origin[a]+=.001f;assert(vk_rt_estimate_dynamic_scene_signature(centroid,nullptr,nullptr,1)!=baseline);backEnd.viewParms.or.origin[a]-=.001f;}
 for(int a=0;a<3;++a)for(int b=0;b<3;++b){const float old=backEnd.viewParms.or.axis[a][b];backEnd.viewParms.or.axis[a][b]+=.001f;assert(vk_rt_estimate_dynamic_scene_signature(centroid,nullptr,nullptr,1)!=baseline);backEnd.viewParms.or.axis[a][b]=old;}
 backEnd.viewParms.mirror=!backEnd.viewParms.mirror;assert(vk_rt_estimate_dynamic_scene_signature(centroid,nullptr,nullptr,1)!=baseline);
 // Ordinary world-space polygons keep their existing view-independent signature.
 polygons[0].surfaceType=SF_POLY;const auto regular=vk_rt_estimate_dynamic_scene_signature(centroid,nullptr,nullptr,1);backEnd.viewParms.or.origin[0]+=100;
 assert(vk_rt_estimate_dynamic_scene_signature(centroid,nullptr,nullptr,1)==regular);
}
'''


def ray_fixture():
    source = fixture('rendererrtx')
    source = source.replace('static struct { View viewParms; } backEnd;',
                            'struct trRefEntity_t {refEntity_t e;};\nstatic struct {View viewParms;struct {int num_entities,numPolys;trRefEntity_t *entities;srfPoly_t *polys;float viewaxis[3][3];} refdef;} backEnd;')
    source += RAY_HOST
    source += function((ROOT / 'code/qcommon/q_math.c').read_text(), 'vec_t VectorNormalize(')
    vk = (ROOT / 'code/rendererrtx/vk.c').read_text()
    for signature in ['static qboolean vk_rt_append_poly_geometry(',
                      'static uint32_t vk_rt_hash_mix_u32(', 'static uint32_t vk_rt_hash_quantized_float(',
                      'static qboolean vk_rt_float_in_finite_range(', 'static qboolean vk_rt_vec3_in_finite_range(',
                      'static uint32_t vk_rt_estimate_dynamic_scene_signature(']:
        source += function(vk[vk.rindex(signature):], signature)
    return source


class Q3IHVParticlesTests(unittest.TestCase):
    def compile_run(self, source):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++17 compiler unavailable')
        flags = compiler_target_args(compiler)
        if os.name == 'nt' and not flags:
            flags = ['-DFNTECH3_Q3_ONLY']
        (ROOT / '.tmp').mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='q3-ihv-particles-', dir=ROOT / '.tmp') as temp:
            cpp, exe = Path(temp) / 'probe.cpp', Path(temp) / 'probe.exe'
            cpp.write_text(source)
            built = subprocess.run([compiler, *flags, '-std=c++17', '-fno-operator-names', '-O2', '-ffast-math',
                                    '-I', str(ROOT), str(cpp), '-o', str(exe)], cwd=ROOT, capture_output=True, text=True, timeout=60)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            ran = subprocess.run([str(exe)], cwd=ROOT, capture_output=True, text=True, timeout=30)
            self.assertEqual(ran.returncode, 0, ran.stdout + ran.stderr)

    def test_original_geometry_digest_and_malformed_inputs(self):
        self.compile_run(GEOMETRY)

    def test_owned_scene_lifetime_material_and_backend_dispatch(self):
        for renderer in RENDERERS:
            with self.subTest(renderer=renderer):
                self.compile_run(fixture(renderer) + CHECKS)

    def test_ray_geometry_matches_raster_and_tracks_view_changes(self):
        self.compile_run(ray_fixture() + RAY_CHECKS)


if __name__ == '__main__':
    unittest.main()
