"""Original IHV model light digests, owned BSP data and actual renderer dispatch."""
from pathlib import Path
import unittest

from mesh_tag_tests import function
import rtcw_light_tests as lights
import rtcw_player_interaction_tests as compile_tests

ROOT = Path(__file__).resolve().parents[1]
RENDERERS = ('renderer', 'renderervk', 'rendererrtx')

NUMERIC = r'''
#include "tests/q3_ihv_model_light_cases.hpp"
int main() {
 LightCases::Diffuse([](const float (*axis)[3],const float *ambient,const float *directed,
  const float (*normals)[4],float *direction,unsigned char (*colors)[4]) {
  Q3IHV_LightDirection(axis,direction);
  for(int v=0;v<17;++v)Q3IHV_LightDiffuse(normals[v],direction,ambient,directed,colors[v]);
 });
 LightCases::Dynamic();LightCases::Surface();LightCases::Malformed();
 const auto digest=LightCases::Trees([](const LightCases::Tree &tree,q3IHVModelLightTrace_t &trace){assert(Q3IHV_LightTrace(tree.nodes,&trace));});
 assert(digest==UINT64_C(0xe913a0fd97554969));
}
'''

def renderer_fixture(renderer):
    source = lights.entity_fixture(renderer)
    source += r'''
#include "tests/q3_ihv_model_light_cases.hpp"
static struct { trRefEntity_t *currentEntity; } backEnd;
static struct { float normal[17][4]; int numVertexes; } tess;
static cvar_t celShadows{};static cvar_t *r_celShadingModelShadows=&celShadows;
static int celCalls;
static qboolean R_CelShadingActive(const trRefEntity_t*) {++celCalls;return qfalse;}
static void R_CelQuantizeModelLighting(const trRefEntity_t*,byte*) {++celCalls;}
'''
    # C's enum assignment permits the existing && expression; C++ requires
    # an integer/bool destination. Only its local declaration needs adapting.
    source += function((ROOT/'code'/renderer/'tr_shade_calc.c').read_text(),
                       'static void RB_CalcDiffuseColor_scalar(').replace('qboolean\t\tcelQuantize;', 'bool celQuantize;')
    return source

RENDERER_CHECKS = r'''
int main() {
 ri.gameId=COM_GAME_QUAKE3_IHV;trRefEntity_t entity{};entity.e.q3IHV.active=1;
 for(int a=0;a<3;++a)entity.e.axis[a][a]=1;
 entity.e.origin[2]=64;entity.e.lightingOrigin[2]=-100;entity.e.renderfx=RF_LIGHTING_ORIGIN;
 const byte pixels[3]={32,64,128};q3IHVModelLightFace_t face{};face.plane[2]=1;face.width=face.height=1;face.pixels=pixels;
 const q3IHVModelLightFace_t *faces[1]={&face};q3IHVModelLightNode_t node{};node.faces=faces;node.numFaces=1;world.q3IHVModelLight=&node;
 dlight_t dynamic{};dynamic.origin[2]=64;dynamic.radius=16;dynamic.color[0]=1;
 trRefdef_t view{};view.dlights=&dynamic;view.num_dlights=1;
 world.lightGridData=pixels;tr.identityLight=.25f;tr.identityLightByte=63;
 for(uint32_t flags:{0u,1u,0x10u,0x40u,0x80000000u}) {
  entity.e.q3IHV.flags=flags;entity.lightingCalculated=qfalse;R_SetupEntityLighting(&view,&entity);
  const float expected[3]={flags?80.f:48.f,flags?96.f:64.f,flags?160.f:128.f};
  assert(!memcmp(entity.ambientLight,expected,12)&&!memcmp(entity.directedLight,expected,12)&&!gridCalls&&!logCalls);
  float direction[3];Q3IHV_LightDirection(entity.e.axis,direction);assert(!memcmp(direction,entity.lightDir,12));
  for(int a=0;a<3;++a)assert(((byte*)&entity.ambientLightInt)[a]==(byte)expected[a]);
  assert(((byte*)&entity.ambientLightInt)[3]==255);
  const auto saved=entity;R_SetupEntityLighting(&view,&entity);assert(!memcmp(&saved,&entity,sizeof entity));
 }
 view.num_dlights=0;view.rdflags=RDF_NOWORLDMODEL;entity.lightingCalculated=qfalse;R_SetupEntityLighting(&view,&entity);
 for(float value:entity.ambientLight)assert(value==255);
 view.rdflags=0;world.q3IHVModelLight=nullptr;entity.lightingCalculated=qfalse;R_SetupEntityLighting(&view,&entity);
 for(float value:entity.ambientLight)assert(value==255);
 // Legacy API entities in the native profile retain the existing dispatch.
 entity.e.q3IHV.active=0;entity.lightingCalculated=qfalse;R_SetupEntityLighting(&view,&entity);assert(gridCalls==1);
 entity.e.q3IHV.active=1;
 for(auto profile:{COM_GAME_QUAKE3,COM_GAME_QUAKELIVE,COM_GAME_RTCW_SP,COM_GAME_RTCW_MP,COM_GAME_WOLFET,COM_GAME_STVEF_SP,COM_GAME_STVEF_MP}) {
  ri.gameId=profile;entity.lightingCalculated=qfalse;int before=gridCalls;R_SetupEntityLighting(&view,&entity);assert(gridCalls==before+1);
 }
 ri.gameId=COM_GAME_QUAKE3_IHV;backEnd.currentEntity=&entity;tess.numVertexes=17;
 LightCases::Diffuse([&](const float (*axis)[3],const float *ambient,const float *directed,const float (*normals)[4],float *direction,unsigned char (*colors)[4]) {
  Q3IHV_LightDirection(axis,entity.lightDir);memcpy(direction,entity.lightDir,12);
  memcpy(entity.ambientLight,ambient,12);memcpy(entity.directedLight,directed,12);memcpy(tess.normal,normals,sizeof tess.normal);
  RB_CalcDiffuseColor_scalar(colors[0]);
 });
 assert(!celCalls);
 // The original ambient-only path truncates and wraps negatives; it never reads the retail packed value.
 entity.ambientLightInt=0;entity.ambientLight[0]=-1.99f;entity.ambientLight[1]=256.99f;entity.ambientLight[2]=127.99f;
 memset(tess.normal,0,sizeof tess.normal);tess.numVertexes=1;byte color[8]={0,0,0,0,99,99,99,99};RB_CalcDiffuseColor_scalar(color);
 assert(color[0]==255&&color[1]==0&&color[2]==127&&color[3]==255&&color[4]==99);
 for(auto profile:{COM_GAME_QUAKE3,COM_GAME_QUAKELIVE,COM_GAME_RTCW_SP,COM_GAME_RTCW_MP,COM_GAME_WOLFET,COM_GAME_STVEF_SP,COM_GAME_STVEF_MP}) {
  ri.gameId=profile;RB_CalcDiffuseColor_scalar(color);assert(!color[0]&&!color[1]&&!color[2]&&!color[3]);
 }
}
'''

BSP = r'''
#include "tests/q3_ihv_model_light_bsp_fixture.hpp"
int main(){
 using namespace LightWorldFixture;
 struct Data { dsurface_t face; drawVert_t vertices[3]; byte pixels[128*128*3]; } data{};
 fileBase=(byte*)&data;lump_t faceLump{0,sizeof data.face},vertexLump{offsetof(Data,vertices),sizeof data.vertices},pixelLump{offsetof(Data,pixels),sizeof data.pixels};
 data.face.surfaceType=LittleLong(MST_PLANAR);data.face.numVerts=LittleLong(3);data.face.lightmapNum=LittleLong(0);
 data.face.lightmapX=LittleLong(7);data.face.lightmapY=LittleLong(11);data.face.lightmapWidth=data.face.lightmapHeight=LittleLong(2);
 data.face.lightmapVecs[0][0]=data.face.lightmapVecs[1][1]=LittleFloat(16);
 data.vertices[0].normal[2]=LittleFloat(1);data.vertices[0].xyz[2]=LittleFloat(8);
 const int offset=(11*128+7)*3;data.pixels[offset]=31;data.pixels[offset+1]=57;data.pixels[offset+2]=193;
 cplane_t plane{};plane.normal[2]=1;plane.dist=8;mnode_t engineNodes[3]{};
 engineNodes[0].contents=-1;engineNodes[0].plane=&plane;engineNodes[0].children[0]=&engineNodes[1];engineNodes[0].children[1]=&engineNodes[2];
 msurface_t surface{&shader};msurface_t *mark=&surface;engineNodes[1].nummarksurfaces=1;engineNodes[1].firstmarksurface=&mark;
 auto load=[&](){s_worldData={3,1,1,engineNodes,&surface,&mark,nullptr};R_LoadQ3IHVModelLighting(&faceLump,&vertexLump,&pixelLump);};
 load();assert(s_worldData.q3IHVModelLight);
 const float origin[3]={0,0,16};q3IHVModelLightTrace_t trace;Q3IHV_LightTraceInit(origin,&trace);
 data.pixels[offset]=0;assert(Q3IHV_LightTrace(s_worldData.q3IHVModelLight,&trace));assert(trace.distance==8&&trace.color[0]==31&&trace.color[1]==57&&trace.color[2]==193);
 shader.ihvModelLightmap=qfalse;load();Q3IHV_LightTraceInit(origin,&trace);assert(Q3IHV_LightTrace(s_worldData.q3IHVModelLight,&trace)&&!trace.sampled);shader.ihvModelLightmap=qtrue;
 for(auto profile:{COM_GAME_QUAKE3,COM_GAME_QUAKELIVE,COM_GAME_RTCW_SP,COM_GAME_RTCW_MP,COM_GAME_WOLFET,COM_GAME_STVEF_SP,COM_GAME_STVEF_MP}) {ri.gameId=profile;auto before=allocations.size();load();assert(!s_worldData.q3IHVModelLight&&allocations.size()==before);}
 ri.gameId=COM_GAME_QUAKE3_IHV;
 auto reject=[&](){bool caught=false;try{load();}catch(const std::runtime_error&){caught=true;}assert(caught&&!s_worldData.q3IHVModelLight);};
 data.face.lightmapWidth=LittleLong(122);reject();data.face.lightmapWidth=LittleLong(2);
 data.face.firstVert=LittleLong(3);reject();data.face.firstVert=0;
 data.face.lightmapNum=LittleLong(1);reject();data.face.lightmapNum=0;
 data.face.lightmapVecs[0][0]=LittleFloat(1e-40f);reject();data.face.lightmapVecs[0][0]=LittleFloat(16);
 --pixelLump.filelen;reject();++pixelLump.filelen;
 data.face.lightmapWidth=0;data.face.lightmapHeight=0;data.face.lightmapX=LittleLong(128);data.face.lightmapY=LittleLong(128);load();
 Q3IHV_LightTraceInit(origin,&trace);assert(Q3IHV_LightTrace(s_worldData.q3IHVModelLight,&trace)&&!trace.sampled);
 allocations.clear();
}
'''

class Q3IHVModelLightingTests(unittest.TestCase):
    def test_original_numeric_surface_and_tree_digests(self):
        compile_tests.RTCWPlayerInteractionTests.compile_and_run(self, NUMERIC)

    def test_actual_entity_and_diffuse_paths_in_every_renderer(self):
        for renderer in RENDERERS:
            with self.subTest(renderer=renderer):
                compile_tests.RTCWPlayerInteractionTests.compile_and_run(self, renderer_fixture(renderer) + RENDERER_CHECKS)

    def test_owned_lightmap_loader_and_malformed_bounds(self):
        compile_tests.RTCWPlayerInteractionTests.compile_and_run(self, BSP)

    def test_lifetime_and_shader_stage_capture(self):
        for renderer in RENDERERS:
            bsp = (ROOT/'code'/renderer/'tr_bsp.c').read_text()
            load = function(bsp, 'void RE_LoadWorldMap( const char *name ) {')
            self.assertLess(load.index('tr.world = NULL'), load.index('R_LoadQ3IHVModelLighting('))
            self.assertLess(load.index('R_LoadNodesAndLeafs('), load.index('R_LoadQ3IHVModelLighting('))
            self.assertLess(load.index('R_LoadQ3IHVModelLighting('), load.index('tr.world = &s_worldData'))
            shader = function((ROOT/'code'/renderer/'tr_shader.c').read_text(), 'static shader_t *FinishShader(')
            self.assertIn('shader.ihvModelLightmap = ri.gameId == COM_GAME_QUAKE3_IHV && hasLightmapStage;', shader)
            self.assertLess(shader.index('shader.ihvModelLightmap ='), shader.index('VertexLightingCollapse();'))

if __name__ == '__main__':
    unittest.main()
