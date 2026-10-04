"""RTCW material compression directives, image flags and profile isolation."""
from pathlib import Path
import os
import re
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

HOST = r'''
static cvar_t compression{};static cvar_t *r_ext_compressed_textures=&compression;
static cvar_t picmip{},characterMip{},filter{};
static cvar_t *r_picmip=&picmip,*r_picmip2=&characterMip,*r_picmipFilter=&filter;
static struct{int rtcwCompression;qboolean jkaNoTC;char name[MAX_QPATH];
 unsigned noPicMip:1,noMipMaps:1,rtcwCharacterMip:1,rtcwPicMipFiltered:1;}shader;
static imgFlags_t received;
static image_t image{"synthetic",nullptr,IMGFLAG_NONE};
static image_t *R_FindImageFile(const char *,imgFlags_t flags){received=flags;return &image;}
static char com_token[MAX_TOKEN_CHARS],com_parsename[MAX_TOKEN_CHARS];
static int com_lines,com_tokenline;
int Q_stricmpn(const char *a,const char *b,int n){
 while(n-- > 0){int d=std::tolower((unsigned char)*a)-std::tolower((unsigned char)*b);
 if(d || !*a || !*b)return d;++a;++b;}return 0;
}
int QDECL Com_sprintf(char *out,int size,const char *format,...) {
 va_list args;va_start(args,format);int n=std::vsnprintf(out,size,format,args);va_end(args);return n;
}
#include "code/renderercommon/tr_rtcw_image_mip.h"
#include "code/renderercommon/tr_rtcw_shader_material.h"
#include "code/renderercommon/tr_jka_shader.h"
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
  // Character images keep an independent mip selector; this never writes either cvar.
  shader.rtcwCharacterMip=1;
  for(int world:{-1,0,1,16,INT_MAX})for(int character:{INT_MIN,0,2,16,INT_MAX}){
   picmip.integer=world;characterMip.integer=character;
   for(auto flags:{IMGFLAG_NONE,IMGFLAG_MIPMAP,IMGFLAG_PICMIP}){
    R_FindShaderImage("models/character.tga",flags);
    const bool selected=profile==COM_GAME_RTCW_SP && (flags&IMGFLAG_PICMIP);
    assert(int(received)==(int(flags)|(selected?IMGFLAG_RTCW_CHARACTER_MIP:0)));
    const int level=selected?character:world;
    assert(R_ImagePicMipLevel(received)==(level<0?0:level>16?16:level));
    assert(picmip.integer==world && characterMip.integer==character);
   }
  }
  r_picmip2=nullptr;picmip.integer=3;
  assert(R_ImagePicMipLevel(IMGFLAG_RTCW_CHARACTER_MIP)==3);r_picmip2=&characterMip;
  // The path filter may exclude models, but SP's character selector must still work.
  // Explicit nopicmip/nomipmaps wins in either order.
  for(int mask:{0,1,2,15})for(const char *text:{"picmip2", "nopicmip picmip2", "picmip2 nopicmip", "nomipmaps picmip2", "picmip2 nomipmaps"}){
   shader={};Q_strncpyz(shader.name,"models/test",sizeof(shader.name));filter.integer=mask;
   R_ApplyShaderPicMipFilter();
   const bool accepted=ParseMipDirectives(text);
   assert(accepted==(profile==COM_GAME_RTCW_SP));
   if(accepted){
    assert(shader.rtcwCharacterMip && !shader.rtcwPicMipFiltered);
    assert(bool(shader.noPicMip)==bool(strstr(text,"nopicmip")||strstr(text,"nomipmaps")));
   }
  }
  for(const char *text:{"nomipmap", "NOMIPMAP"}){
   shader={};
   assert(ParseMipDirectives(text)==(profile==COM_GAME_RTCW_MP));
   assert(bool(shader.noMipMaps)==(profile==COM_GAME_RTCW_MP));
   assert(bool(shader.noPicMip)==(profile==COM_GAME_RTCW_MP));
  }
  // Contract table includes the original asymmetry: mapnocomp follows negotiated
  // compression availability, even if the compression cvar later becomes zero.
  const char *tokens[]={"map16","map32","mapcomp","mapnocomp","animmapcomp","animmapnocomp"};
  for(int bits:{0,16,24,32})for(int available:{0,1,2})for(int enabled:{0,1,2}){
   const bool selected[]={bits<=16,bits>16,bool(available&&enabled),!available,bool(available&&enabled),!available};
   for(int i=0;i<6;++i){
    const auto expected=!rtcw?RTCW_MAP_UNCHANGED:selected[i]?(i<4?RTCW_MAP_STATIC:RTCW_MAP_ANIMATED):(i<4?RTCW_MAP_SKIP_STATIC:RTCW_MAP_SKIP_ANIMATED);
    assert(R_RTCWMapDirective(tokens[i],bits,available,enabled)==expected);
   }
  }
  for(const char *name:{"map","animmap","clampmap","map64","picmip2","","}"})
   assert(R_RTCWMapDirective(name,32,1,1)==RTCW_MAP_UNCHANGED);
  assert(R_RTCWMapDirective("MaPcOmP",32,1,1)==(rtcw?RTCW_MAP_STATIC:RTCW_MAP_UNCHANGED));
 }
 // Real-tokenizer consumption must preserve stage boundaries and following commands.
 for(const char *input:{" texture.tga rgbGen identity", " \"texture with spaces.tga\" rgbGen identity"}){
  const char *p=input;assert(R_RTCWSkipMapArguments(&p,qfalse));assert(!strcmp(COM_ParseExt(&p,qtrue),"rgbGen"));
 }
 for(const char *input:{" 8 frame1 frame2\n rgbGen identity", " 8 frame1 frame2 //comment\n rgbGen identity"}){
  const char *p=input;assert(R_RTCWSkipMapArguments(&p,qtrue));assert(!strcmp(COM_ParseExt(&p,qtrue),"rgbGen"));
 }
 const char *p="8 frame } next";assert(R_RTCWSkipMapArguments(&p,qtrue));assert(!strcmp(COM_ParseExt(&p,qtrue),"}"));
 for(auto animated:{qfalse,qtrue})for(const char *bad:{"", "\nnext", "}", "{"}){
  p=bad;assert(!R_RTCWSkipMapArguments(&p,animated));
  if(*bad=='}'||*bad=='{')assert(p==bad);
 }
 p="8 } next";assert(!R_RTCWSkipMapArguments(&p,qtrue));assert(!strcmp(COM_ParseExt(&p,qtrue),"}"));
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
        (ROOT / '.tmp').mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='rtcw-material-', dir=ROOT / '.tmp') as temp:
            folder = Path(temp)
            for renderer in RENDERERS:
                with self.subTest(renderer=renderer):
                    source = (ROOT / 'code' / renderer / 'tr_shader.c').read_text()
                    # All map, clampmap, animmap, sky and implicit-image paths
                    # must use the policy, not just the UI path in the live probe.
                    self.assertEqual(source.count('R_FindImageFile('), 1)
                    # rendererrtx also finds the path tracer's companion
                    # material maps and the compilers' light images through
                    # the policy
                    self.assertEqual(source.count('R_FindShaderImage('), 10 if renderer == 'rendererrtx' else 8)
                    parser = function(source, 'static qboolean ParseShader(')
                    self.assertIn('R_RTCWShaderCompressionDirective( token, &shader.rtcwCompression )', parser)
                    initializer = function(source, 'static void InitShader(')
                    self.assertIn('Com_Memset( &shader, 0, sizeof( shader ) )', initializer)
                    stage = function(source, 'static qboolean ParseStage(')
                    self.assertLess(stage.index('R_RTCWMapDirective('), stage.index('!Q_stricmp( token, "map" )'))
                    self.assertIn('R_RTCWSkipMapArguments(', stage)
                    image_source = (ROOT / 'code' / renderer / 'tr_image.c').read_text()
                    self.assertNotIn('>>= r_picmip->integer', image_source)
                    self.assertIn('>>= R_ImagePicMipLevel( image->flags )', image_source)
                    # Execute the actual general-parser branches, including explicit
                    # opt-out ordering, with the same tokenizer as the engine.
                    start = parser.index('else if ( ri.gameId == COM_GAME_RTCW_SP && !Q_stricmp( token, "picmip2" ) )')
                    end = parser.index('else if ( !Q_stricmp( token, "novlcollapse" ) )', start)
                    branches = parser[start:end].replace('else if', 'if', 1)
                    mip_parser = '''static bool ParseMipDirectives(const char *text){
                      while(text){const char *token=COM_ParseExt(&text,qtrue);if(!*token)return true;
                    ''' + branches + '''else return false;}return true;}'''
                    path_helpers = '\n'.join(function(source, name) for name in (
                        'static qboolean R_ShaderPathStartsWith(', 'static qboolean R_ShaderPicMipAllowed(',
                        'static void R_ApplyShaderPicMipFilter('))
                    # C permits returning a logical expression as this enum; C++
                    # needs the explicit enum values in the execution fixture.
                    path_helpers = re.sub(r'return (!Q_stricmpn[^\n]+);', r'return \1 ? qtrue : qfalse;', path_helpers)
                    defines = '\n'.join(line for line in source.splitlines() if line.startswith('#define PICMIP_FILTER_')) + '\n'
                    # Keep each executable distinct: Windows may retain a
                    # completed image briefly while the next renderer links.
                    cpp = folder / f'{renderer}.cpp'; exe = folder / f'{renderer}.exe'
                    cpp.write_text(common(renderer) + HOST + tokenizer() + defines + path_helpers + mip_parser +
                                   function(source, 'static image_t *R_FindShaderImage(') + CHECKS,
                                   encoding='utf-8')
                    built = subprocess.run([compiler, *target, '-std=c++17', '-O2', '-I', str(ROOT), str(cpp), '-o', str(exe)],
                                           cwd=ROOT, capture_output=True, text=True, timeout=60)
                    self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
                    options = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
                    run = subprocess.run([str(exe)], cwd=ROOT, capture_output=True, text=True, timeout=30, **options)
                    self.assertEqual(run.returncode, 0, run.stdout + run.stderr)


if __name__ == '__main__':
    unittest.main()
