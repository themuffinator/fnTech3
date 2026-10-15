"""Academy's oneShotAnimMap and clampAnimMap stages in all three renderers."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args
from mesh_tag_tests import function
from model_shader_tests import common
from rtcw_shader_text_tests import tokenizer

ROOT = Path(__file__).resolve().parents[1]
RENDERERS = ('renderer', 'renderervk', 'rendererrtx')
BINDERS = {'renderer': 'void R_BindAnimatedImage( const textureBundle_t *bundle )',
           'renderervk': 'static VkDescriptorSet R_AnimatedImageDescriptor( const textureBundle_t *bundle )',
           'rendererrtx': 'static void R_BindAnimatedImage( const textureBundle_t *bundle )'}

# Stand-ins for the renderer state the sliced parser branch and frame
# selection read. No retail text is used: the stage lines below are written
# for this test.
HOST = r'''
static char com_token[MAX_TOKEN_CHARS],com_parsename[MAX_TOKEN_CHARS];
static int com_lines,com_tokenline;
int QDECL Com_sprintf(char *out,int size,const char *format,...){
 va_list args;va_start(args,format);int n=std::vsnprintf(out,size,format,args);va_end(args);return n;
}
#include <algorithm>
#include <cstdlib>
float Q_atof(const char *text){return (float)std::atof(text);}
#include "code/renderercommon/tr_jka_shader.h"
#ifndef LIGHTMAP_INDEX_NONE
#define LIGHTMAP_INDEX_NONE 0
#endif
#define MAX_IMAGE_ANIMATIONS 64
#define MAX_IMAGE_ANIMATIONS_VQ3 8
#define MAX_IMAGE_ANIMATIONS_RTCW 16
static qboolean s_extendedShader;
static imgFlags_t &operator|=(imgFlags_t &a,imgFlags_t b){a=static_cast<imgFlags_t>((int)a|(int)b);return a;}
static qboolean R_AliceShaderDialect(){return ri.gameId==COM_GAME_ALICE||ri.gameId==COM_GAME_FAKK2||ri.gameId==COM_GAME_MOHAA?qtrue:qfalse;}
static qboolean R_WolfensteinShaderDialect(){return ri.gameId==COM_GAME_RTCW_SP||ri.gameId==COM_GAME_RTCW_MP||ri.gameId==COM_GAME_WOLFET?qtrue:qfalse;}
struct textureBundle_t{image_t *image[MAX_IMAGE_ANIMATIONS];int numImageAnimations;double imageAnimationSpeed;
 int videoMapHandle,lightmap;qboolean isVideoMap;
 unsigned isScreenMap:1,dlight:1,aliceAnimMapOnce:1,aliceFrameFromEntity:1,jkaOneShotAnimMap:1;float mohaaAnimPhase;};
struct shaderStage_t{textureBundle_t bundle[2];};
static struct{int bundle;}aliceStage;
static struct{char name[MAX_QPATH];qboolean noMipMaps,noPicMip,noLightScale;}shader;
static image_t frames[MAX_IMAGE_ANIMATIONS];
static imgFlags_t seen[MAX_IMAGE_ANIMATIONS];
static int loads;
static image_t *R_FindShaderImage(const char *,imgFlags_t flags){seen[loads]=flags;return &frames[loads++];}
typedef image_t *VkDescriptorSet;
static image_t *bound;
static void GL_Bind(image_t *image){bound=image;}
static VkDescriptorSet R_ImageDescriptor(image_t *image){return image;}
static void vk_update_descriptor(int,VkDescriptorSet){}
#define VK_DESC_TEXTURE_BASE 0
static struct{struct{VkDescriptorSet color_descriptor;}screenMap;}vk;
static struct{int currenttmu;}glState;
static struct{image_t *whiteImage,*blackImage;}tr;
struct Entity{struct{struct{int frame;}alice;}e;};
static struct{qboolean screenMapDone;struct{struct{qboolean snooperView;}rtcw;}refdef;Entity *currentEntity;}backEnd;
static struct{double shaderTime;}tess;
static void Cinematic(int){}
'''

CHECKS = r'''
static bool Parse(shaderStage_t &target,const char *line){
 shaderStage_t *stage=&target;loads=0;const char *cursor=line;const char **text=&cursor;
 const char *token=COM_ParseExt(text,qfalse);
 if(false){}
@BRANCH@
 else return false;
 return true;
}
static std::string Frames(int count){
 std::string text;for(int n=0;n<count;++n)text+=" gfx/own/frame"+std::to_string(n);return text;
}
static int Index(const textureBundle_t &bundle,double time){
 tess.shaderTime=time;bound=nullptr;image_t *image=Bind(&bundle);
 for(int n=0;n<bundle.numImageAnimations;++n)if(image==bundle.image[n])return n;
 return -1;
}
int main(){
 for(int id=COM_GAME_AUTO+1;id<COM_GAME_COUNT;++id){
  ri.gameId=static_cast<comGameId_t>(id);
  const bool academy=ri.gameId==COM_GAME_JKA_SP||ri.gameId==COM_GAME_JKA_MP;
  const int plain=std::min(40,academy?32:R_AliceShaderDialect()?64:R_WolfensteinShaderDialect()?16:8);
  shaderStage_t stage{};
  // Retail spells it oneshotanimmap in its scripts; the match ignores case.
  for(const char *word:{"oneShotAnimMap","oneshotanimmap","ONESHOTANIMMAP"}){
   stage={};const std::string line=std::string(word)+" 7"+Frames(8);
   assert(Parse(stage,line.c_str())==academy);
   if(academy){
    const textureBundle_t &bundle=stage.bundle[0];
    assert(bundle.jkaOneShotAnimMap&&bundle.numImageAnimations==8&&bundle.imageAnimationSpeed==7.0);
    for(int n=0;n<8;++n)assert(!(seen[n]&IMGFLAG_CLAMPTOEDGE)&&bundle.image[n]==&frames[n]);
   }else{
    assert(!stage.bundle[0].jkaOneShotAnimMap&&stage.bundle[0].numImageAnimations==0&&loads==0);
   }
  }
  // clampAnimMap loads its frames clamped and loops.
  stage={};assert(Parse(stage,("clampanimmap 2"+Frames(3)).c_str())==academy);
  if(academy){
   assert(!stage.bundle[0].jkaOneShotAnimMap&&stage.bundle[0].numImageAnimations==3);
   for(int n=0;n<3;++n)assert(seen[n]&IMGFLAG_CLAMPTOEDGE);
  }
  // Plain animMap writes the one-shot bit clear, keeps repeat addressing and
  // the profile's frame bound: Academy's 32, every other profile unchanged.
  stage={};stage.bundle[0].jkaOneShotAnimMap=1;
  assert(Parse(stage,("animMap 8"+Frames(40)).c_str()));
  assert(!stage.bundle[0].jkaOneShotAnimMap&&stage.bundle[0].numImageAnimations==plain&&loads==plain);
  for(int n=0;n<plain;++n)assert(!(seen[n]&IMGFLAG_CLAMPTOEDGE));
  stage={};assert(!Parse(stage,"oneShotAnimMap")&&stage.bundle[0].numImageAnimations==0);
  if(academy)assert(stage.bundle[0].jkaOneShotAnimMap);
  else assert(!stage.bundle[0].jkaOneShotAnimMap);
  assert(!Parse(stage,"animMapOnce 7 a b")&&!Parse(stage,"clampMap a"));
 }
 // Frame selection: frames advance with the stage's shader time; a one-shot
 // bundle then holds its last frame, any other bundle wraps.
 ri.gameId=COM_GAME_JKA_MP;
 textureBundle_t bundle{};bundle.numImageAnimations=8;bundle.imageAnimationSpeed=7;
 for(int n=0;n<8;++n)bundle.image[n]=&frames[n];
 const double times[]={-1.0,0.0,0.1,0.5,1.0,1.15,1.2,10.0,1000.0};
 const int looped[]={0,0,0,3,7,0,0,6,0};
 const int once[]={0,0,0,3,7,7,7,7,7};
 for(int n=0;n<9;++n){
  bundle.jkaOneShotAnimMap=0;assert(Index(bundle,times[n])==looped[n]);
  bundle.jkaOneShotAnimMap=1;assert(Index(bundle,times[n])==once[n]);
 }
 // Alice's animMapOnce is unchanged beside it.
 bundle.jkaOneShotAnimMap=0;bundle.aliceAnimMapOnce=1;ri.gameId=COM_GAME_ALICE;
 assert(Index(bundle,10.0)==7&&Index(bundle,0.5)==3);
 bundle.numImageAnimations=1;bundle.jkaOneShotAnimMap=1;assert(Index(bundle,10.0)==0);
}
'''


class JKAOneShotAnimMapTests(unittest.TestCase):
    def test_parser_and_frame_selection_in_each_renderer(self):
        compiler = shutil.which('clang++')
        if not compiler:
            self.skipTest('Clang unavailable')
        target = compiler_target_args(compiler)
        if os.name == 'nt' and not target:
            target = ['-DFNTECH3_Q3_ONLY']
        elif os.name != 'nt':
            target = ['-m32']
        (ROOT / '.tmp').mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='jka-oneshot-', dir=ROOT / '.tmp') as temp:
            folder = Path(temp)
            for renderer in RENDERERS:
                with self.subTest(renderer=renderer):
                    source = (ROOT / 'code' / renderer / 'tr_shader.c').read_text(encoding='utf-8')
                    stage = function(source, 'static qboolean ParseStage(')
                    start = stage.index('else if ( !Q_stricmp( token, "animMap" )')
                    branch = stage[start:stage.index('else if ( !Q_stricmp( token, "videoMap" ) )', start)]
                    shade = (ROOT / 'code' / renderer / 'tr_shade.c').read_text(encoding='utf-8')
                    binder = function(shade, BINDERS[renderer])
                    binder = binder.replace('ri.CIN_RunCinematic', 'Cinematic').replace('ri.CIN_UploadCinematic', 'Cinematic')
                    if renderer == 'renderervk':
                        bind = 'static image_t *Bind(const textureBundle_t *bundle){return R_AnimatedImageDescriptor(bundle);}\n'
                    else:
                        bind = 'static image_t *Bind(const textureBundle_t *bundle){R_BindAnimatedImage(bundle);return bound;}\n'
                    cpp = folder / f'{renderer}.cpp'
                    exe = folder / f'{renderer}.exe'
                    cpp.write_text(common(renderer) + HOST + tokenizer() + binder + '\n' + bind +
                                   CHECKS.replace('@BRANCH@', branch), encoding='utf-8')
                    built = subprocess.run([compiler, *target, '-std=c++17', '-O2', '-I', str(ROOT), str(cpp), '-o', str(exe)],
                                           cwd=ROOT, capture_output=True, text=True, timeout=120)
                    self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
                    options = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
                    run = subprocess.run([str(exe)], cwd=ROOT, capture_output=True, text=True, timeout=30, **options)
                    self.assertEqual(run.returncode, 0, run.stdout + run.stderr)

    def test_retail_measurements_stay_named(self):
        header = (ROOT / 'code/renderercommon/tr_jka_shader.h').read_text(encoding='utf-8')
        for address in ('0x4b426c', '0x47a42e', '0x4ac3d0', '0x472fd0'):
            self.assertIn(address, header)
        self.assertIn('#define MAX_IMAGE_ANIMATIONS_JKA 32', header)
        for renderer in RENDERERS:
            with self.subTest(renderer=renderer):
                local = (ROOT / 'code' / renderer / 'tr_local.h').read_text(encoding='utf-8')
                self.assertIn('unsigned int\tjkaOneShotAnimMap : 1;', local)


if __name__ == '__main__':
    unittest.main()
