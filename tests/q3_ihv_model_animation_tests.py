"""Native MD3 arithmetic and actual GLx/Vulkan/RTX animation/culling dispatch."""
from pathlib import Path
import re
import unittest

from mesh_tag_tests import function
import q3_ihv_entity_renderer_tests as entities
import rtcw_mesh_blink_tests as mesh
import rtcw_player_interaction_tests as interaction

ROOT = Path(__file__).resolve().parents[1]
NUMERIC = '#include "tests/q3_ihv_model_animation_cases.hpp"\n'


def cull_fixture(renderer):
    source = r'''
#include "code/qcommon/q_shared.h"
#include "code/qcommon/qfiles.h"
#include "code/qcommon/game_profile.h"
#include "code/renderercommon/tr_types.h"
#include "code/renderercommon/tr_q3_ihv_model.h"
#include <cassert>
#include <initializer_list>
struct model_t {md3Frame_t frames[2];} model;
struct trRefEntity_t {refEntity_t e;} entity;
static struct {comGameId_t gameId=COM_GAME_QUAKE3_IHV;} ri;
static struct {struct {int c_sphere_cull_md3_in,c_sphere_cull_md3_clip,c_sphere_cull_md3_out;
 int c_box_cull_md3_in,c_box_cull_md3_clip,c_box_cull_md3_out;} pc;} tr;
enum {CULL_IN=0,CULL_CLIP=1,CULL_OUT=2};
static unsigned sphereCalls,boxCalls,sphereReplies[2],boxReply;
static float observedBounds[2][3];
static const md3Frame_t *R_MeshFrame(const model_t *m,int lod,int frame){assert(!lod&&frame>=0&&frame<2);return &m->frames[frame];}
static int R_CullLocalPointAndRadius(const float *origin,float radius){assert(sphereCalls<2);const auto &f=model.frames[sphereCalls?entity.e.oldframe:entity.e.frame];assert(!memcmp(origin,f.localOrigin,12)&&radius==f.radius);return sphereReplies[sphereCalls++];}
static int R_CullLocalBox(float (*bounds)[3]){++boxCalls;memcpy(observedBounds,bounds,24);return boxReply;}
static uint32_t seed=0x13942785;
static uint32_t Random(){seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;return seed;}
static uint64_t digest=14695981039346656037ull;
static void Hash(const void *input,size_t bytes){auto*p=(const unsigned char*)input;for(size_t i=0;i<bytes;++i){digest^=p[i];digest*=1099511628211ull;}}
'''
    text = (ROOT / 'code' / renderer / 'tr_mesh.c').read_text()
    source += function(text, 'static void R_MD3FrameBounds(' if renderer == 'renderer' else 'static void R_MD3ModelBounds(')
    return source + function(text, 'static int R_CullModel(')


CULL_CHECKS = r'''
int main(){
 unsigned cases=0,boxes=0;entity.e.q3IHV.active=1;
 for(unsigned sample=0;sample<512;++sample){
  for(auto &frame:model.frames){for(int a=0;a<3;++a){frame.bounds[0][a]=float(int(Random()%8192)-4096)*.25f;frame.bounds[1][a]=frame.bounds[0][a]+float(Random()%1024)*.25f;frame.localOrigin[a]=float(int(Random()%512)-256);}frame.radius=float(Random()%2048)/4;}
  for(int a=0;a<3;++a){entity.e.origin[a]=float(int(Random()%8192)-4096)/4;entity.e.oldorigin[a]=float(int(Random()%8192)-4096)/4;}
  entity.e.frame=1;
  for(int oldFrame:{0,1})for(unsigned nonNormalized:{0u,1u})for(unsigned flags:{0u,0x40u,0x840u})for(unsigned first:{0u,1u,2u})for(unsigned second:{0u,1u,2u})for(unsigned box:{0u,1u,2u}){
   entity.e.oldframe=oldFrame;entity.e.nonNormalizedAxes=nonNormalized?qtrue:qfalse;entity.e.q3IHV.flags=flags;
   sphereReplies[0]=first;sphereReplies[1]=second;boxReply=box;sphereCalls=boxCalls=0;
   float bounds[2][3];const unsigned actual=R_CullModel(&model,0,&entity,bounds);
   const unsigned expectedSphere=nonNormalized?0:oldFrame==1?1:2;const bool useBox=nonNormalized||first==1||(oldFrame!=1&&first!=second);
   assert(actual==(useBox?box:first)&&sphereCalls==expectedSphere&&boxCalls==(useBox?1u:0u));
   Hash(&actual,4);Hash(&sphereCalls,4);Hash(&boxCalls,4);++cases;
   // Expanded bounds also remain available after a sphere early return.
   float expected[2][3];assert(Q3IHV_ModelBounds(flags,model.frames[oldFrame].bounds,model.frames[1].bounds,entity.e.origin,entity.e.oldorigin,expected));
   assert(!memcmp(bounds,expected,24));
   if(useBox){assert(!memcmp(bounds,observedBounds,24));Hash(observedBounds,24);++boxes;}
  }
 }
 assert(cases==165888&&boxes==129024&&digest==UINT64_C(0x69adefd497b482bd));
 entity.e.nonNormalizedAxes=qtrue;entity.e.q3IHV.flags=0x40;entity.e.origin[0]=1000;entity.e.oldorigin[0]=-1000;
 float bounds[2][3],plain[2][3];assert(Q3IHV_ModelBounds(0,model.frames[1].bounds,model.frames[1].bounds,nullptr,nullptr,plain));
 for(auto profile:{COM_GAME_QUAKE3,COM_GAME_QUAKELIVE,COM_GAME_RTCW_SP,COM_GAME_RTCW_MP,COM_GAME_WOLFET,COM_GAME_STVEF_SP,COM_GAME_STVEF_MP}){
  ri.gameId=profile;R_CullModel(&model,0,&entity,bounds);assert(!memcmp(bounds,plain,24));
 }
 ri.gameId=COM_GAME_QUAKE3_IHV;entity.e.q3IHV.active=0;R_CullModel(&model,0,&entity,bounds);assert(!memcmp(bounds,plain,24));
 entity.e.q3IHV.active=1;entity.e.origin[0]=FLT_MAX;entity.e.oldorigin[0]=-FLT_MAX;sphereCalls=boxCalls=0;
 assert(R_CullModel(&model,0,&entity,bounds)==CULL_OUT&&!sphereCalls&&!boxCalls);
}
'''


def raster_fixture(renderer):
    source = entities.fixture(renderer)
    return source.replace('assert((vertices==3&&indices==3)||(vertices==12&&indices==36));',
                          'assert(vertices>=0&&vertices<=17&&(indices==3||indices==36));')


RASTER_CHECKS = r'''
static uint32_t seed=0x98174235;
static uint32_t Random(){seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;return seed;}
int main(){
 Q3IHV_ModelSines(tr.ihvModelSines);ri.gameId=COM_GAME_QUAKE3_IHV;
 trRefEntity_t entity{};entity.e.q3IHV.active=1;entity.e.frame=1;backEnd.currentEntity=&entity;
 for(int a=0;a<3;++a)backEnd.or.axis[a][a]=1;
 struct {md3Surface_t header;int indices[3];float uv[17][2];short records[34][4];} model{};
 auto &s=model.header;s.numFrames=2;s.numVerts=17;s.numTriangles=1;s.ofsEnd=sizeof model;
 s.ofsXyzNormals=offsetof(decltype(model),records);s.ofsTriangles=offsetof(decltype(model),indices);s.ofsSt=offsetof(decltype(model),uv);
 model.indices[1]=1;model.indices[2]=2;
 cvar_t control{};r_lerpmodels=&control;
 for(unsigned sample=0;sample<512;++sample){
  for(auto &v:model.records)for(short &c:v)c=short(Random());
  for(int a=0;a<3;++a){entity.e.origin[a]=float(int(Random()%8192)-4096)/4;entity.e.oldorigin[a]=float(int(Random()%8192)-4096)/4;}
  for(uint32_t bits:{0u,0x80000000u,1u,0x80000001u,0x3f800000u,0xbf800000u,0x7f7fffffu,0xff7fffffu,0x7f800000u,0xff800000u,0x7fc00123u,0xffc00123u})
  for(unsigned flags:{0u,0x40u})for(float requested:{-.13f,.123f,0.0f,.5f,.99f,1.0f,2.0f}){
   memcpy(&control.value,&bits,4);const float fraction=(bits&0x7fffffffu)!=0&&(bits&0x7fffffffu)<=0x7f800000u?requested:0;
   entity.e.q3IHV.flags=flags;entity.e.backlerp=requested;tess={};tess.numVertexes=2;tess.numIndexes=3;
   for(int v=0;v<19;++v){tess.xyz[v][3]=71;tess.normal[v][3]=72;}
   RB_SurfaceMesh(&s);assert(tess.numVertexes==19&&tess.numIndexes==6&&tess.indexes[3]==2&&tess.indexes[5]==4);
   float move[3];assert(Q3IHV_EntityOriginLerp(flags,entity.e.origin,entity.e.oldorigin,backEnd.or.axis,fraction,move));
   for(int v=0;v<17;++v){float xyz[3],normal[3];assert(Q3IHV_ModelVertex(model.records[v],model.records[17+v],fraction,move,tr.ihvModelSines,xyz,normal));assert(!memcmp(xyz,tess.xyz[v+2],12)&&!memcmp(normal,tess.normal[v+2],12));assert(tess.xyz[v+2][3]==71&&tess.normal[v+2][3]==72);}
  }
 }
 r_lerpmodels=nullptr;const auto valid=s;
 for(int field=0;field<6;++field)for(int bad:{-1,INT_MAX}){
  if(field==4&&bad==INT_MAX)continue; // allocation length is owned by the loader
  s=valid;switch(field){case 0:s.numVerts=bad;break;case 1:s.numTriangles=bad;break;case 2:s.numFrames=bad;break;case 3:s.ofsXyzNormals=bad;break;case 4:s.ofsEnd=bad;break;default:entity.e.frame=bad;break;}
  tess={};const auto before=tess;RB_SurfaceMesh(&s);assert(!tess.numVertexes&&!tess.numIndexes&&!memcmp(tess.xyz,before.xyz,sizeof tess.xyz)&&!memcmp(tess.normal,before.normal,sizeof tess.normal));entity.e.frame=1;
 }
 s=valid;s.ofsEnd=s.ofsXyzNormals+sizeof model.records-1;tess={};RB_SurfaceMesh(&s);assert(!tess.numVertexes&&!tess.numIndexes);
 s=valid;entity.e.q3IHV.flags=0;entity.e.backlerp=FLT_MAX;memset(model.records,0,sizeof model.records);model.records[33][0]=32767;
 tess={};RB_SurfaceMesh(&s);assert(!tess.numVertexes&&!tess.numIndexes);for(const auto &v:tess.xyz)for(float c:v)assert(!c);
 entity.e.frame=entity.e.oldframe=0;entity.e.backlerp=FLT_MAX;tess={};RB_SurfaceMesh(&s);assert(tess.numVertexes==17);
 entity.e.frame=1;entity.e.backlerp=.13f;
 control.value=0;r_lerpmodels=&control;
 for(auto profile:{COM_GAME_QUAKE3,COM_GAME_QUAKELIVE,COM_GAME_RTCW_SP,COM_GAME_RTCW_MP,COM_GAME_WOLFET,COM_GAME_STVEF_SP,COM_GAME_STVEF_MP}){
  ri.gameId=profile;tess={};meshFraction=0;RB_SurfaceMesh(&s);assert(meshFraction==.13f&&tess.numVertexes==17&&tess.xyz[0][2]==2);
 }
}
'''


RAY_CHECKS = r'''
static uint32_t seed=0x71639185;
static uint32_t Random(){seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;return seed;}
int main(){
 Q3IHV_ModelSines(tr.ihvModelSines);FnMdc_InitNormals(&r_mdcNormals);ri.gameId=COM_GAME_QUAKE3_IHV;
 for(int i=0;i<12;++i){tr.shaders[i]=&shaderTable[i];shaderTable[i].index=i;}
 struct Surface {md3Surface_t header;short vertices[6][4];float uv[6];int indices[3];};
 struct {md3Header_t header;Surface s;} model{};
 model.header.numFrames=2;model.header.numSurfaces=1;model.header.ofsSurfaces=offsetof(decltype(model),s);
 auto &s=model.s.header;s.numFrames=2;s.numVerts=3;s.numTriangles=1;s.ofsEnd=sizeof(Surface);
 s.ofsXyzNormals=offsetof(Surface,vertices);s.ofsSt=offsetof(Surface,uv);s.ofsTriangles=offsetof(Surface,indices);model.s.indices[1]=1;model.s.indices[2]=2;
 testModel.type=MOD_MESH;testModel.md3[0]=&model.header;entity.e.hModel=11;entity.e.q3IHV.active=1;entity.e.frame=1;entity.e.oldframe=0;
 entity.e.customShader=1;for(int a=0;a<3;++a)entity.e.axis[a][a]=1;
 backEnd.refdef.viewaxis[0][1]=1;backEnd.refdef.viewaxis[1][0]=-1;backEnd.refdef.viewaxis[2][2]=1;
 cvar_t control{};r_lerpmodels=&control;
 for(unsigned sample=0;sample<512;++sample){
  for(auto &v:model.s.vertices)for(short &c:v)c=short(Random());
  for(int a=0;a<3;++a){entity.e.origin[a]=float(int(Random()%8192)-4096)/4;entity.e.oldorigin[a]=float(int(Random()%8192)-4096)/4;}
  for(uint32_t bits:{0u,0x80000000u,1u,0x80000001u,0x3f800000u,0xbf800000u,0x7f7fffffu,0xff7fffffu,0x7f800000u,0xff800000u,0x7fc00123u,0xffc00123u})
  for(unsigned flags:{0u,0x40u,0x440u})for(float requested:{-.13f,.123f,0.0f,.5f,.99f,1.0f,2.0f}){
   memcpy(&control.value,&bits,4);const float fraction=(bits&0x7fffffffu)!=0&&(bits&0x7fffffffu)<=0x7f800000u?requested:0;
   entity.e.q3IHV.flags=flags;entity.e.backlerp=requested;rtxRtCpuGeometry_t geometry{};
   assert(vk_rt_append_md3_entity_geometry(&geometry,&entity)&&geometry.numVertices==3&&geometry.numTriangles==1);
   float move[3];const auto *axis=(flags&0x400)?backEnd.refdef.viewaxis:entity.e.axis;
   assert(Q3IHV_EntityOriginLerp(flags,entity.e.origin,entity.e.oldorigin,axis,fraction,move));
   for(int v=0;v<3;++v){float xyz[3],normal[3],world[3],worldNormal[3];assert(Q3IHV_ModelVertex(model.s.vertices[v],model.s.vertices[v+3],fraction,move,tr.ihvModelSines,xyz,normal));vk_rt_transform_point(&entity,xyz,world);vk_rt_transform_direction(&entity,normal,worldNormal);assert(!memcmp(world,geometry.vertices[v].xyz,12)&&!memcmp(worldNormal,geometry.vertices[v].normal,12));}
  }
 }
 r_lerpmodels=nullptr;entity.e.q3IHV.flags=0;entity.e.backlerp=FLT_MAX;memset(model.s.vertices,0,sizeof model.s.vertices);model.s.vertices[5][0]=32767;
 rtxRtCpuGeometry_t failed{};assert(!vk_rt_append_md3_entity_geometry(&failed,&entity)&&!failed.numVertices&&!failed.numTriangles);
 entity.e.backlerp=0;s.ofsEnd=s.ofsXyzNormals+sizeof model.s.vertices-1;
 assert(!vk_rt_append_md3_entity_geometry(&failed,&entity)&&!failed.numVertices);
}
'''


class Q3IHVModelAnimationTests(unittest.TestCase):
    def test_original_numeric_vectors_and_malformed_inputs(self):
        interaction.RTCWPlayerInteractionTests.compile_and_run(self, NUMERIC)

    def test_actual_renderer_animation_and_staged_surfaces(self):
        for renderer in entities.RENDERERS:
            with self.subTest(renderer=renderer):
                interaction.RTCWPlayerInteractionTests.compile_and_run(self, raster_fixture(renderer) + RASTER_CHECKS)

    def test_original_culling_digest_in_every_renderer(self):
        for renderer in entities.RENDERERS:
            with self.subTest(renderer=renderer):
                interaction.RTCWPlayerInteractionTests.compile_and_run(self, cull_fixture(renderer) + CULL_CHECKS)

    def test_ray_animation_transform_normals_and_staging(self):
        interaction.RTCWPlayerInteractionTests.compile_and_run(self, mesh.ray_fixture() + RAY_CHECKS)

    def test_native_control_registration(self):
        for renderer in entities.RENDERERS:
            register = function((ROOT/'code'/renderer/'tr_init.c').read_text(), 'static void R_Register( void )')
            statement = re.search(r'r_lerpmodels\s*=\s*[^;]+;', register)[0]
            source = r'''
#include "code/qcommon/q_shared.h"
#include "code/qcommon/game_profile.h"
#include <cassert>
#include <cstring>
#include <initializer_list>
static cvar_t control,*r_lerpmodels;static int calls;
static cvar_t *Get(const char *name,const char *value,int flags){++calls;assert(!strcmp(name,"r_lerpmodels")&&!strcmp(value,"1")&&!flags);return &control;}
static struct {comGameId_t gameId;decltype(&Get) Cvar_Get;} ri{COM_GAME_QUAKE3_IHV,Get};
static void Register(){
''' + statement + r'''
}
int main(){
 Register();assert(calls==1&&r_lerpmodels==&control);
 for(auto profile:{COM_GAME_QUAKE3,COM_GAME_QUAKELIVE,COM_GAME_RTCW_SP,COM_GAME_RTCW_MP,COM_GAME_WOLFET,COM_GAME_STVEF_SP,COM_GAME_STVEF_MP}){ri.gameId=profile;Register();assert(calls==1&&!r_lerpmodels);}
}
'''
            interaction.RTCWPlayerInteractionTests.compile_and_run(self, source)


if __name__ == '__main__':
    unittest.main()
