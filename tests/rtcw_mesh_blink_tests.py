"""Execute each renderer's mesh traversal for RTCW head/eyelid materials."""
from pathlib import Path
import unittest

from mds_renderer_tests import renderer_fixture
from mdc_ray_geometry_tests import FIXTURE as RAY_FIXTURE
from mesh_tag_tests import function
import rtcw_player_interaction_tests as interaction

ROOT = Path(__file__).resolve().parents[1]


def fixture(renderer, mesh=None):
    source = renderer_fixture(renderer)
    source = source.replace('struct shader_t { bool defaultShader; int index; }',
                            'struct shader_t { bool defaultShader; int index; const char *name="test"; }')
    source = source.replace('struct skin_t { int numSurfaces;',
                            'struct skin_t { const char *name="test"; int numSurfaces;')
    source = source.replace('int numSkins; shader_t *defaultShader; } tr{2, shaderTable};',
                            'int numSkins; shader_t *defaultShader; model_t *currentModel; } tr{2, shaderTable, &testModel};')
    if mesh is None:
        mesh = (ROOT / 'code' / renderer / 'tr_mesh.c').read_text()
    start = mesh.index('\tif ( !numSurfaces ) return;')
    end = mesh.index('R_MeshShaderIndex( &surface, ent->e.skinNum )', start)
    selection = mesh[start:mesh.index('\n\t\t}', end) + len('\n\t\t}')]
    return source + r'''
static std::vector<std::pair<std::string,int>> drawn;
static void Select(trRefEntity_t *ent) {
 int lod=0,i=0,numSurfaces=R_MeshSurfaceCount(tr.currentModel,lod);
 bool casterContext=false,shadowCtx=false;
 meshSurfaceView_t surface{}; shader_t *shader;
''' + selection + r'''
  drawn.emplace_back(surface.name,shader->index);
  R_MeshNextSurface(&surface);
 }
}
'''


CHECKS = r'''
int main() {
 for(int i=0;i<12;++i) shaderTable[i].index=i;
 struct { md3Header_t header; md3Surface_t surfaces[2]; } md3{};
 md3.header.numSurfaces=2; md3.header.ofsSurfaces=offsetof(decltype(md3),surfaces);
 mdcModel_t mdc{}; mdcMeshSurface_t mdcSurfaces[2]{};
 mdc.view.numSurfaces=2; mdc.surfaces=mdcSurfaces;
 mdsModel_t mds{}; mdsMeshSurface_t mdsSurfaces[2]{};
 mds.view.numSurfaces=2; mds.surfaces=mdsSurfaces;
 for(int i=0;i<2;++i) {
  const char *name=i?"h_blink":"h_eyes";
  std::strcpy(md3.surfaces[i].name,name); md3.surfaces[i].ofsEnd=sizeof(md3Surface_t);
  std::strcpy(mdcSurfaces[i].name,name);
  std::strcpy(mdsSurfaces[i].name,name); mdsSurfaces[i].shader=7;
 }
 for(auto type:{MOD_MESH,MOD_MDC,MOD_MDS}) {
  testModel={}; testModel.type=type; testModel.md3[0]=&md3.header;
  testModel.mdc[0]=&mdc; testModel.mds=&mds;
  for(auto game:{COM_GAME_QUAKE3,COM_GAME_QUAKELIVE,COM_GAME_RTCW_SP,COM_GAME_RTCW_MP}) {
   ri.gameId=game;
   for(auto blink:{qfalse,qtrue}) {
    entity.e={}; entity.e.customSkin=1; entity.e.rtcw.blink=blink;
    skin.numSurfaces=3; skin.surfaces[0]={"h_eyes",&shaderTable[8]};
    skin.surfaces[1]={"h_eyes_b",&shaderTable[9]}; skin.surfaces[2]={"h_blink",&shaderTable[11]};
    drawn.clear(); Select(&entity);
    const size_t count=game==COM_GAME_RTCW_SP&&!blink?1:2;
    assert(drawn.size()==count && drawn[0].first=="h_eyes");
    assert(drawn[0].second==(game==COM_GAME_RTCW_MP&&blink?9:8));
    if(count==2) assert(drawn[1].first=="h_blink" && drawn[1].second==11);
    // Missing MP alternate falls back to the normal eyes; custom shaders win.
    skin.surfaces[1].shader=tr.defaultShader; drawn.clear(); Select(&entity);
    assert(drawn[0].second==8);
    entity.e.customShader=10; drawn.clear(); Select(&entity);
    assert(drawn.size()==count); for(const auto &surface:drawn) assert(surface.second==10);
    // Absent/invalid custom skins preserve format-specific shader fallback.
    entity.e.customShader=0;
    for(int handle:{-1,0,2,INT_MAX}) {
     entity.e.customSkin=handle; drawn.clear(); Select(&entity);
     assert(drawn.size()==count);
     for(const auto &surface:drawn) assert(surface.second==(type==MOD_MDS?7:0));
    }
   }
  }
 }
}
'''


class RTCWMeshBlinkTests(unittest.TestCase):
    def test_md3_mdc_mds_surface_submission_and_skin_selection(self):
        for renderer in ('renderer', 'renderervk', 'rendererrtx'):
            with self.subTest(renderer=renderer):
                interaction.RTCWPlayerInteractionTests.compile_and_run(self, fixture(renderer) + CHECKS)

    def test_native_ray_md3_eyelids_and_mp_skin_alternates(self):
        source = fixture('rendererrtx')
        source += '\nfnMdcNormals_t r_mdcNormals;\n'
        source = source.replace('model_t *currentModel; } tr{2, shaderTable, &testModel};',
                                'model_t *currentModel; int numShaders=12; shader_t *shaders[12]{}; } tr{2, shaderTable, &testModel};')
        callbacks = RAY_FIXTURE[RAY_FIXTURE.index('enum { RTX_RT_MATFLAG_MASKED'):RAY_FIXTURE.index('#include "code/renderercommon/tr_mdc_ray_geometry.h"')]
        callbacks = callbacks.replace('vertices[3]', 'vertices[6]').replace('triangles[1][3]', 'triangles[2][3]')
        callbacks = callbacks.replace('geometry->numVertices < 3', 'geometry->numVertices < 6')
        callbacks = callbacks.replace('!geometry->numTriangles && a < 3 && b < 3 && c < 3',
                                      'geometry->numTriangles < 2 && a < 6 && b < 6 && c < 6')
        source += callbacks + '\nfloat Com_Clamp(float lo,float hi,float value) { return std::clamp(value,lo,hi); }\n'
        vk = (ROOT / 'code/rendererrtx/vk.c').read_text()
        for signature in ('static void vk_rt_decode_md3_normal(', 'static const shader_t *vk_rt_resolve_md3_shader(',
                          'static qboolean vk_rt_append_md3_entity_geometry( rtxRtCpuGeometry_t *geometry, const trRefEntity_t *ent )\n{'):
            source += function(vk[vk.rindex(signature):], signature) + '\n'
        source += r'''
int main() {
 FnMdc_InitNormals(&r_mdcNormals);
 for(int i=0;i<12;++i) { shaderTable[i].index=i; tr.shaders[i]=&shaderTable[i]; }
 struct Surface { md3Surface_t header; md3XyzNormal_t vertices[3]; float uv[6]; int triangle[3]; };
 struct { md3Header_t header; Surface surfaces[2]; } model{};
 model.header.numFrames=1; model.header.numSurfaces=2; model.header.ofsSurfaces=offsetof(decltype(model),surfaces);
 for(int i=0;i<2;++i) {
  auto &s=model.surfaces[i]; std::strcpy(s.header.name,i?"h_eyes":"h_blink");
  s.header.numVerts=3; s.header.numTriangles=1; s.header.ofsEnd=sizeof(Surface);
  s.header.ofsXyzNormals=offsetof(Surface,vertices); s.header.ofsSt=offsetof(Surface,uv);
  s.header.ofsTriangles=offsetof(Surface,triangle); s.triangle[1]=1; s.triangle[2]=2;
 }
 testModel.type=MOD_MESH; testModel.md3[0]=&model.header;
 entity.e.hModel=11; entity.e.customSkin=1;
 skin.numSurfaces=3; skin.surfaces[0]={"h_blink",&shaderTable[7]};
 skin.surfaces[1]={"h_eyes",&shaderTable[8]}; skin.surfaces[2]={"h_eyes_b",&shaderTable[9]};
 for(auto game:{COM_GAME_QUAKE3,COM_GAME_QUAKELIVE,COM_GAME_RTCW_SP,COM_GAME_RTCW_MP}) {
  ri.gameId=game;
  for(auto blink:{qfalse,qtrue}) {
   entity.e.rtcw.blink=blink; rtxRtCpuGeometry_t geometry{};
   assert(vk_rt_append_md3_entity_geometry(&geometry,&entity));
   const unsigned count=game==COM_GAME_RTCW_SP&&!blink?1:2;
   assert(geometry.numVertices==3*count && geometry.numTriangles==count && geometry.sourceEntityCount==count);
   assert(materialHandle==(game==COM_GAME_RTCW_MP&&blink?9:8));
  }
 }
}
'''
        interaction.RTCWPlayerInteractionTests.compile_and_run(self, source)


if __name__ == '__main__':
    unittest.main()
