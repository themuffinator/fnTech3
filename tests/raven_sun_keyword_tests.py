"""Raven's general shader keywords sun, lightColor and noTC in all three renderers.

All four Jedi engines read a general `sun` as q3map_sun under a second name:
six words -- colour, intensity, degrees, elevation -- each read with
COM_ParseExt( text, qfalse ) and atof, a missing one as 0, and the rest of the
line left to the keyword loop (jamp.exe 1.01 0x4b51a1, jasp.exe 1.01
0x47b312, jk2mp.exe 0x48ccbc, jk2sp.exe 0x47cab4). lightColor ( r g b ) is
skipped by three of them and read with ParseVector by jk2mp.exe, which fails
the shader when the vector is malformed; Outcast reads noTC as Academy does.
Elite Force and the id engines know only q3map_sun, so the other profiles are
left as they were. The fixture compiles each renderer's own branches, its sun
conversion and ParseVector with the engine's tokenizer and runs them under
every profile. The shader lines are written for this test (record
docs/fntech3/2026-10-15-raven-sun-keyword.md).
"""
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

HOST = r'''
#include <cmath>
#include <cstdlib>
static char com_token[MAX_TOKEN_CHARS],com_parsename[MAX_TOKEN_CHARS];
static int com_lines,com_tokenline;
tokenType_t com_tokentype;
int QDECL Com_sprintf(char *out,int size,const char *format,...){
 va_list args;va_start(args,format);int n=std::vsnprintf(out,size,format,args);va_end(args);return n;
}
static struct{char name[MAX_QPATH];qboolean jkaNoTC;}shader;
'''

# A stand-in for ParseShader's keyword loop: its locals, the noTC directive
# where ParseShader calls it, the sliced branches, and two plain keywords so
# that what a branch leaves on the line or the next line can be seen.
LOOP_HEAD = r'''
struct Parsed{bool ok,sun,noTC,extended;vec3_t color,direction,light;float intensity,deviance;int samples,skyparms;char unknown[64];};
static Parsed parsed;
static qboolean ParseGeneral(const char **text){
 qboolean skySunValid=qfalse;
 vec3_t skySunColor={0.0f,0.0f,0.0f};
 vec3_t skySunDirection={0.0f,0.0f,0.0f};
 vec3_t skySunLight={0.0f,0.0f,0.0f};
 float skySunIntensity=0.0f;
 float skySunDeviance=0.0f;
 int skySunSamples=1;
 qboolean skySunExtended=qfalse;
 const char *token;
 parsed=Parsed{};shader={};
 while(1){
  token=COM_ParseComplex(text,qtrue);
  if(!token[0])return qfalse;
  if(R_JKAShaderNoTCDirective(token,&shader.jkaNoTC))continue;
  if(token[0]=='}')break;
'''

LOOP_TAIL = r'''
  else if(!Q_stricmp(token,"skyparms")){parsed.skyparms++;SkipRestOfLine(text);continue;}
  else if(!Q_stricmp(token,"surfaceparm")){COM_ParseExt(text,qfalse);continue;}
  else{Q_strncpyz(parsed.unknown,token,sizeof(parsed.unknown));return qfalse;}
 }
 parsed.ok=true;parsed.sun=skySunValid;parsed.noTC=shader.jkaNoTC;
 VectorCopy(skySunColor,parsed.color);VectorCopy(skySunDirection,parsed.direction);VectorCopy(skySunLight,parsed.light);
 parsed.intensity=skySunIntensity;parsed.deviance=skySunDeviance;parsed.samples=skySunSamples;parsed.extended=skySunExtended;
 return qtrue;
}
'''

CHECKS = r'''
static bool Parse(const char *body){const char *p=body;return ParseGeneral(&p)==qtrue;}
static bool Near(float a,float b){return std::fabs(a-b)<1e-4f;}
static void Direction(float yaw,float elevation){
 const float y=yaw/180.0f*(float)M_PI,e=elevation/180.0f*(float)M_PI;
 assert(Near(parsed.direction[0],std::cos(y)*std::cos(e)));
 assert(Near(parsed.direction[1],std::sin(y)*std::cos(e)));
 assert(Near(parsed.direction[2],std::sin(e)));
}
int main(){
 for(int id=COM_GAME_QUAKE3;id<COM_GAME_COUNT;++id){
  ri.gameId=static_cast<comGameId_t>(id);
  const bool jedi=id==COM_GAME_JKA_SP||id==COM_GAME_JKA_MP||id==COM_GAME_JK2_SP||id==COM_GAME_JK2_MP;
  for(const char *word:{"sun","SUN","Sun","q3map_sun","Q3MAP_SUN"})assert(bool(R_JediSunKeyword(word))==jedi);
  for(const char *word:{"suns","su","sunshader","q3map_sunExt","q3map_sunExt2",""})assert(!R_JediSunKeyword(word));
  for(const char *word:{"lightColor","lightcolor","LIGHTCOLOR"})assert(bool(R_JediLightColorKeyword(word))==jedi);
  assert(!R_JediLightColorKeyword("q3map_lightColor"));
  assert(R_JKAShaderImageFlags(IMGFLAG_NONE,qtrue)==(jedi?IMGFLAG_NO_COMPRESSION:IMGFLAG_NONE));
  assert(R_JKAShaderImageFlags(IMGFLAG_MIPMAP,qfalse)==IMGFLAG_MIPMAP);

  // a duel sky's lines: the sun, then noTC and skyparms on lines of their own
  const char *sky="sun 0.8 0.8 0.95 160 140 80\nnotc\nskyparms textures/skies/hoth 512 -\n}";
  if(!jedi){
   // the other profiles' parsers are left as they were: these branches take
   // none of Raven's words
   assert(!Parse(sky)&&!std::strcmp(parsed.unknown,"sun"));
   assert(!Parse("lightColor ( 0 0 1 )\n}")&&!std::strcmp(parsed.unknown,"lightColor"));
   assert(!Parse("notc\n}")&&!std::strcmp(parsed.unknown,"notc"));
   // their q3map_sun keeps its own branch, which skips the rest of the line
   assert(Parse("q3map_sun 1 1 0.5 150 75 60 notc\nskyparms - 512 -\n}"));
   assert(parsed.sun&&!parsed.noTC&&parsed.skyparms==1&&Near(parsed.intensity,150));
   Direction(75,60);
   continue;
  }
  assert(Parse(sky));
  assert(parsed.sun&&parsed.noTC&&parsed.skyparms==1);
  // the colour keeps the q3map_sun branch's rule: largest component 1
  assert(Near(parsed.color[0],0.8f/0.95f)&&Near(parsed.color[1],0.8f/0.95f)&&Near(parsed.color[2],1.0f));
  assert(Near(parsed.intensity,160.0f));
  for(int i=0;i<3;++i)assert(Near(parsed.light[i],parsed.color[i]*160.0f));
  Direction(140,80);
  // q3map_sun is the same branch in these profiles
  assert(Parse("q3map_sun 1 .85 0.5 65 -30 60\n}")&&parsed.sun);
  assert(Near(parsed.color[1],0.85f)&&Near(parsed.color[2],0.5f)&&Near(parsed.intensity,65));
  Direction(-30,60);
  // a 0..255 colour scales to the same colour
  assert(Parse("sun 255 255 255 355 345 85\n}")&&parsed.sun);
  assert(Near(parsed.color[0],1)&&Near(parsed.color[1],1)&&Near(parsed.color[2],1)&&Near(parsed.intensity,355));
  // no intensity: the shader loads without a sun
  assert(Parse("sun 0.933333 1 0.8 0 0 0\n}")&&!parsed.sun);
  assert(Parse("sun 255 255 255 0 345 85\n}")&&!parsed.sun);
  assert(Parse("sun 0 0 0 100 0 90\n}")&&!parsed.sun);
  // the rest of the line goes back to the keyword loop
  assert(Parse("sun 1 1 1 100 45 60 notc\n}")&&parsed.sun&&parsed.noTC);
  assert(!Parse("sun 1 1 1 100 45 60 extra\n}")&&!std::strcmp(parsed.unknown,"extra"));
  // a short line reads its missing words as 0, and once a read has crossed
  // the line, the next line's words
  assert(Parse("sun 1 0.5 0.25 100 45\nskyparms - 512 -\n}")&&parsed.sun&&parsed.skyparms==1);
  Direction(45,0);
  assert(Parse("sun 1 1 1 100\nnotc\nskyparms - 512 -\n}"));
  assert(parsed.sun&&!parsed.noTC&&parsed.skyparms==1);
  Direction(0,0);
  // a closing brace can be one of the six words, as in retail, and a line
  // that runs off the text ends the shader without its brace
  assert(Parse("sun 1 1 1 100 }\n}")&&parsed.sun);
  Direction(0,0);
  assert(!Parse("sun 1 1 1 100 }\n")&&!parsed.unknown[0]);
  assert(!Parse("sun 1 1 1")&&!parsed.unknown[0]);
  // lightColor: jk2mp.exe reads a bracketed vector, the others skip the line
  for(const char *line:{"lightColor ( 0 0 1 )\nskyparms - 512 -\n}","lightcolor\t( 1 0.501961 0 )\nskyparms - 512 -\n}"}){
   assert(Parse(line)&&parsed.skyparms==1);
  }
  for(const char *line:{"lightColor 0 0 1\nskyparms - 512 -\n}","lightColor ( 0 0 )\nskyparms - 512 -\n}","lightColor\nskyparms - 512 -\n}"}){
   const bool ok=Parse(line);
   if(id==COM_GAME_JK2_MP)assert(!ok&&!parsed.unknown[0]);
   else assert(ok&&parsed.skyparms==1);
  }
  // a sun after a q3map2 extended sun is a plain sun
  assert(Parse("q3map_sunExt 1 1 1 100 45 60 3 8\nsun 1 1 1 50 0 90\n}")&&parsed.sun&&Near(parsed.intensity,50));
  RTX_EXTENDED_CHECK
 }
}
'''


# The whole production parser (alice_shader_parse_tests' host), one sky
# definition at a time under every profile.
HOST_CHECKS = r'''
static int failures;
#define CHECK(test) do { if(!(test)) { fprintf(stderr,"failed line %d: %s (%s)\n",__LINE__,#test,hostWarning);++failures; } } while(0)
static qboolean Parse(comGameId_t game,const char *text) {
 HostInit(game);return HostParse("textures/test/sky",text,text+strlen(text));
}
static int HostChecks(void) {
 const char *sky="{\nqer_editorimage textures/skies/test\nsun 0.8 0.8 0.95 160 140 80\nsurfaceparm sky\n"
  "surfaceparm noimpact\nsurfaceparm nomarks\nnotc\nq3map_nolightmap\nskyParms textures/skies/test 512 -\n}\n";
 const char *tinted="{\nlightcolor\t( 1 0.501961 0 )\nsun 1 0.627451 0.258824 100 46 40\nsurfaceparm sky\n"
  "skyParms textures/skies/test 512 -\n}\n";
 const char *bare="{\nlightColor 1 0.5 0\nsun 1 1 1 100 0 90\nsurfaceparm sky\nskyParms textures/skies/test 512 -\n}\n";
 comGameId_t game;
 for(game=COM_GAME_QUAKE3;game<COM_GAME_COUNT;game=(comGameId_t)(game+1)) {
  const qboolean jedi=game==COM_GAME_JKA_SP||game==COM_GAME_JKA_MP||game==COM_GAME_JK2_SP||game==COM_GAME_JK2_MP;
  const qboolean ok=Parse(game,sky);
  if(!jedi) {
   /* every other profile still refuses the word, as its retail engine would */
   CHECK(!ok && strstr(hostWarning,"'sun'"));
   CHECK(!Parse(game,tinted) && strstr(hostWarning,"'lightcolor'"));
   continue;
  }
  CHECK(ok && !hostWarning[0]);
  CHECK(shader.isSky && shader.skySunValid && shader.jkaNoTC);
  CHECK(fabsf(shader.skySunIntensity-160.0f)<1e-3f);
  CHECK(fabsf(shader.skySunColor[0]-0.8f/0.95f)<1e-5f && fabsf(shader.skySunColor[2]-1.0f)<1e-5f);
  {
   const float y=140.0f/180.0f*(float)M_PI,e=80.0f/180.0f*(float)M_PI;
   CHECK(fabsf(shader.skySunDirection[0]-cosf(y)*cosf(e))<1e-4f);
   CHECK(fabsf(shader.skySunDirection[1]-sinf(y)*cosf(e))<1e-4f);
   CHECK(fabsf(shader.skySunDirection[2]-sinf(e))<1e-4f);
  }
  CHECK(Parse(game,tinted) && !hostWarning[0] && shader.skySunValid && fabsf(shader.skySunIntensity-100.0f)<1e-3f);
  if(game==COM_GAME_JK2_MP) CHECK(!Parse(game,bare) && strstr(hostWarning,"missing parenthesis"));
  else CHECK(Parse(game,bare) && shader.skySunValid);
 }
 return failures?1:0;
}
'''


class RavenSunKeywordTests(unittest.TestCase):
    def test_whole_parser_under_every_profile(self):
        from alice_shader_parse_tests import RENDERERS as HOST_RENDERERS, build_host
        (ROOT / '.tmp').mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='raven-sun-host-', dir=ROOT / '.tmp') as temp:
            for renderer in HOST_RENDERERS:
                with self.subTest(renderer=renderer):
                    exe = build_host(Path(temp), f'host-{renderer}', HOST_CHECKS, renderer=renderer)
                    options = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
                    run = subprocess.run([str(exe)], cwd=ROOT, capture_output=True, text=True, timeout=30, **options)
                    self.assertEqual(run.returncode, 0, run.stdout + run.stderr)

    def test_branches_follow_retail_in_each_renderer(self):
        compiler = shutil.which('clang++')
        if not compiler:
            self.skipTest('Clang unavailable')
        target = compiler_target_args(compiler)
        if os.name == 'nt' and not target:
            target = ['-DFNTECH3_Q3_ONLY']
        elif os.name != 'nt':
            target = ['-m32']
        shared = (ROOT / 'code/qcommon/q_shared.c').read_text(encoding='utf-8')
        math = (ROOT / 'code/qcommon/q_math.c').read_text(encoding='utf-8')
        helpers = ''.join(function(text, signature) for text, signature in (
            (shared, 'float Com_Clamp( float min, float max, float value )'),
            (shared, 'char *COM_ParseComplex( const char **data_p, qboolean allowLineBreaks )'),
            (math, 'vec_t VectorNormalize( vec3_t v )'),
            (math, 'static int Q_isfinite( float f )'),
            (math, 'float Q_atof( const char *str )')))
        (ROOT / '.tmp').mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='raven-sun-', dir=ROOT / '.tmp') as temp:
            folder = Path(temp)
            for renderer in RENDERERS:
                with self.subTest(renderer=renderer):
                    source = (ROOT / 'code' / renderer / 'tr_shader.c').read_text(encoding='utf-8')
                    parser = function(source, 'static qboolean ParseShader(')
                    start = parser.index('else if ( R_JediSunKeyword( token ) )')
                    generic = parser.index('else if ( !Q_stricmp( token, "q3map_sun" ) ||', start)
                    end = parser.index('else if ( !Q_stricmp( token, "deformVertexes" ) )', generic)
                    # Raven's branches come before the q3map_sun branch and the
                    # q3map prefix, and the sun branch leaves the line alone
                    self.assertLess(end, parser.index('!Q_stricmpn( token, "q3map", 5 )'))
                    sun = parser[start:parser.index('R_JediLightColorKeyword( token )', start)]
                    self.assertNotIn('SkipRestOfLine', sun)
                    self.assertIn('R_JediSunParms( text, sunParms );', sun)
                    branches = parser[start:end].replace('else if', 'if', 1)
                    checks = CHECKS.replace('RTX_EXTENDED_CHECK', (
                        'assert(parsed.deviance==0.0f&&parsed.samples==1&&!parsed.extended);'
                        if renderer == 'rendererrtx' else ''))
                    cpp = folder / f'{renderer}.cpp'
                    exe = folder / f'{renderer}.exe'
                    cpp.write_text(
                        common(renderer) + HOST + tokenizer() + helpers +
                        '#include "code/renderercommon/tr_jka_shader.h"\n' +
                        function(source, 'static qboolean ParseVector(') +
                        function(source, 'static qboolean SkySunFromParms(') +
                        function(source, 'static qboolean ParseSkySunParms(') +
                        LOOP_HEAD + branches + LOOP_TAIL + checks, encoding='utf-8')
                    built = subprocess.run([compiler, *target, '-std=c++17', '-I', str(ROOT), str(cpp), '-o', str(exe)],
                                           cwd=ROOT, capture_output=True, text=True, timeout=90)
                    self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
                    options = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
                    run = subprocess.run([str(exe)], cwd=ROOT, capture_output=True, text=True, timeout=30, **options)
                    self.assertEqual(run.returncode, 0, run.stdout + run.stderr)


if __name__ == '__main__':
    unittest.main()
