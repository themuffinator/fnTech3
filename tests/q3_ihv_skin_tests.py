"""Exercise IHV ordered skins through actual mesh and RTX material selection."""
from pathlib import Path
import unittest

from mesh_tag_tests import function
from rtcw_mesh_blink_tests import fixture
import rtcw_player_interaction_tests as interaction

ROOT = Path(__file__).resolve().parents[1]

CHECKS = r'''
int main() {
 for(int i=0;i<12;++i)shaderTable[i].index=i;
 struct { md3Header_t header; md3Surface_t surfaces[5]; } model{};
 model.header.numSurfaces=5;model.header.ofsSurfaces=offsetof(decltype(model),surfaces);
 for(auto &surface:model.surfaces){std::strcpy(surface.name,"deliberately-unmatched");surface.ofsEnd=sizeof(md3Surface_t);}
 testModel.type=MOD_MESH;testModel.md3[0]=&model.header;
 entity.e={};entity.e.customSkin=1;entity.e.skinNum=99;
 skin.numSurfaces=3;skin.surfaces[0]={"not-a-surface",&shaderTable[7]};
 skin.surfaces[1]={"another",&shaderTable[8]};skin.surfaces[2]={"third",&shaderTable[9]};
 ri.gameId=COM_GAME_QUAKE3_IHV;drawn.clear();Select(&entity);
 assert(drawn.size()==5);for(size_t i=0;i<5;++i)assert(drawn[i].second==7+(int)(i%3));
 entity.e.customShader=10;drawn.clear();Select(&entity);
 assert(drawn.size()==5);for(auto &surface:drawn)assert(surface.second==10);
 entity.e.customShader=0;
 for(int count:{0,1,2,3}) {
  skin.numSurfaces=count;drawn.clear();Select(&entity);
  assert(drawn.size()==5);for(size_t i=0;i<5;++i)assert(drawn[i].second==(count?7+(int)(i%count):0));
 }
 skin.numSurfaces=3;
 for(auto game:{COM_GAME_QUAKE3,COM_GAME_QUAKELIVE,COM_GAME_RTCW_SP,COM_GAME_RTCW_MP,COM_GAME_WOLFET,COM_GAME_STVEF_SP,COM_GAME_STVEF_MP}){
  ri.gameId=game;drawn.clear();Select(&entity);
  assert(drawn.size()==5);for(auto &surface:drawn)assert(surface.second==0);
 }
}
'''

RAY_CHECKS = r'''
int main() {
 for(int i=0;i<12;++i){shaderTable[i].index=i;tr.shaders[i]=&shaderTable[i];}
 md3Surface_t surface{};std::strcpy(surface.name,"unmatched");
 entity.e={};entity.e.customSkin=1;entity.e.skinNum=99;
 skin.numSurfaces=3;skin.surfaces[0]={"ignored",&shaderTable[7]};
 skin.surfaces[1]={"ignored",&shaderTable[8]};skin.surfaces[2]={"ignored",&shaderTable[9]};
 ri.gameId=COM_GAME_QUAKE3_IHV;
 for(int i=0;i<100;++i)assert(vk_rt_resolve_md3_shader(&entity,&surface,i)->index==7+i%3);
 entity.e.customShader=10;
 for(int i=0;i<5;++i)assert(vk_rt_resolve_md3_shader(&entity,&surface,i)->index==10);
 entity.e.customShader=0;
 for(auto game:{COM_GAME_QUAKE3,COM_GAME_QUAKELIVE,COM_GAME_RTCW_SP,COM_GAME_RTCW_MP,COM_GAME_WOLFET,COM_GAME_STVEF_SP,COM_GAME_STVEF_MP}){
  ri.gameId=game;for(int i=0;i<5;++i)assert(vk_rt_resolve_md3_shader(&entity,&surface,i)==tr.defaultShader);
 }
 struct { md3Surface_t surface; md3Shader_t shaders[2]; } embedded{};
 embedded.surface.numShaders=2;embedded.surface.ofsShaders=offsetof(decltype(embedded),shaders);
 embedded.shaders[0].shaderIndex=7;embedded.shaders[1].shaderIndex=8;
 entity.e.customSkin=0;
 for(int index:{INT_MIN,-7,-1,0,1,2,INT_MAX}){
  entity.e.skinNum=index;const int expected=index<0?7:7+index%2;
  assert(vk_rt_resolve_md3_shader(&entity,&embedded.surface,0)->index==expected);
 }
}
'''


class Q3IHVSkinTests(unittest.TestCase):
    def test_all_renderers_select_skin_by_surface_ordinal(self):
        for renderer in ('renderer', 'renderervk', 'rendererrtx'):
            with self.subTest(renderer=renderer):
                interaction.RTCWPlayerInteractionTests.compile_and_run(self, fixture(renderer) + CHECKS)

    def test_rtx_ray_material_matches_raster_surface_selection(self):
        source = fixture('rendererrtx')
        source = source.replace('model_t *currentModel; } tr{2, shaderTable, &testModel};',
                                'model_t *currentModel; int numShaders=12; shader_t *shaders[12]{}; } tr{2, shaderTable, &testModel};')
        vk = (ROOT / 'code/rendererrtx/vk.c').read_text()
        signature = 'static const shader_t *vk_rt_resolve_md3_shader('
        source += function(vk[vk.rindex(signature):], signature)
        interaction.RTCWPlayerInteractionTests.compile_and_run(self, source + RAY_CHECKS)


if __name__ == '__main__':
    unittest.main()
