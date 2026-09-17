"""RTCW material compression directives, image flags and profile isolation."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest
from compiler_support import compiler_target_args
from mesh_tag_tests import function
from model_shader_tests import common

ROOT = Path(__file__).resolve().parents[1]
RENDERERS = ('renderer', 'renderervk', 'rendererrtx')

HOST = r'''
static cvar_t compression{};static cvar_t *r_ext_compressed_textures=&compression;
static struct{int rtcwCompression;}shader;
static imgFlags_t received;
static image_t image{"synthetic",nullptr,IMGFLAG_NONE};
static image_t *R_FindImageFile(const char *,imgFlags_t flags){received=flags;return &image;}
#include "code/renderercommon/tr_rtcw_shader_material.h"
'''

CHECKS = r'''
int main(){
 for(auto profile:{COM_GAME_QUAKE3,COM_GAME_QUAKELIVE,COM_GAME_RTCW_SP,COM_GAME_RTCW_MP}){
  ri.gameId=profile;const bool rtcw=profile==COM_GAME_RTCW_SP||profile==COM_GAME_RTCW_MP;
  for(const char *token:{"nocompress","NOCOMPRESS","allowcompress","AllowCompress"}){
   shader.rtcwCompression=17;
   assert(bool(R_RTCWShaderCompressionDirective(token,&shader.rtcwCompression))==rtcw);
   assert(shader.rtcwCompression==(rtcw?(token[0]=='n'||token[0]=='N'?-1:1):17));
  }
  for(const char *token:{"","nocompression","compress","nofog","{","}"}){
   shader.rtcwCompression=13;assert(!R_RTCWShaderCompressionDirective(token,&shader.rtcwCompression)&&shader.rtcwCompression==13);
  }
  for(int mode:{0,1,2})for(int policy:{-1,0,1})
  for(const char *name:{"textures/stone.tga","models/weapons/skin.tga","textures/skies/cloud.tga","*lightmap0","textures/Weapons/skin.tga"}){
   compression.integer=mode;shader.rtcwCompression=policy;
   for(auto flags:{IMGFLAG_NONE,IMGFLAG_CLAMPTOEDGE,IMGFLAG_MIPMAP,IMGFLAG_NO_COMPRESSION}){
    assert(R_FindShaderImage(name,flags)==&image);
    const bool exempt=!strncmp(name,"*lightmap",9)||strstr(name,"weapons")||strstr(name,"skies");
    const bool blocked=rtcw&&(policy<0||(mode==2&&policy!=1)||exempt);
    assert(int(received)==(int(flags)|(blocked?IMGFLAG_NO_COMPRESSION:0)));
   }
  }
  shader.rtcwCompression=0;R_RTCWShaderCompressionDirective("nocompress",&shader.rtcwCompression);
  if(rtcw)assert(shader.rtcwCompression==-1);
  shader={}; // InitShader zero-initializes the next material.
  compression.integer=1;R_FindShaderImage("textures/next.tga",IMGFLAG_NONE);assert(received==IMGFLAG_NONE);
 }
}
'''


class RTCWShaderMaterialTests(unittest.TestCase):
    def test_directives_and_image_flags_in_each_renderer(self):
        compiler = shutil.which('clang++')
        if not compiler:
            self.skipTest('Clang unavailable')
        target = compiler_target_args(compiler)
        if os.name == 'nt' and not target:
            target = ['-DFNTECH3_Q3_ONLY']
        elif os.name != 'nt':
            target = ['-m32']
        with tempfile.TemporaryDirectory(prefix='rtcw-material-', dir=ROOT / '.tmp') as temp:
            folder = Path(temp)
            for renderer in RENDERERS:
                with self.subTest(renderer=renderer):
                    source = (ROOT / 'code' / renderer / 'tr_shader.c').read_text()
                    # All map, clampmap, animmap, sky and implicit-image paths
                    # must use the policy, not just the UI path in the live probe.
                    self.assertEqual(source.count('R_FindImageFile('), 1)
                    self.assertEqual(source.count('R_FindShaderImage('), 7)
                    parser = function(source, 'static qboolean ParseShader(')
                    self.assertIn('R_RTCWShaderCompressionDirective( token, &shader.rtcwCompression )', parser)
                    initializer = function(source, 'static void InitShader(')
                    self.assertIn('Com_Memset( &shader, 0, sizeof( shader ) )', initializer)
                    # Keep each executable distinct: Windows may retain a
                    # completed image briefly while the next renderer links.
                    cpp = folder / f'{renderer}.cpp'; exe = folder / f'{renderer}.exe'
                    cpp.write_text(common(renderer) + HOST + function(source, 'static image_t *R_FindShaderImage(') + CHECKS,
                                   encoding='utf-8')
                    built = subprocess.run([compiler, *target, '-std=c++17', '-O2', '-I', str(ROOT), str(cpp), '-o', str(exe)],
                                           cwd=ROOT, capture_output=True, text=True, timeout=60)
                    self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
                    options = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
                    run = subprocess.run([str(exe)], cwd=ROOT, capture_output=True, text=True, timeout=30, **options)
                    self.assertEqual(run.returncode, 0, run.stdout + run.stderr)


if __name__ == '__main__':
    unittest.main()
