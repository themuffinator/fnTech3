"""Exercise IHV material parsing/sky geometry with the real engine tokenizer."""
import os
from pathlib import Path
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
#include <cfloat>
#include <fstream>
static imgFlags_t &operator|=(imgFlags_t &a,imgFlags_t b){return a=imgFlags_t(int(a)|int(b));}
static imgFlags_t operator|(imgFlags_t a,imgFlags_t b){return imgFlags_t(int(a)|int(b));}
static char com_token[MAX_TOKEN_CHARS],com_parsename[MAX_TOKEN_CHARS];
static int com_lines,com_tokenline;
int QDECL Com_sprintf(char *out,int size,const char *format,...){
 va_list args;va_start(args,format);int n=std::vsnprintf(out,size,format,args);va_end(args);return n;
}
static cvar_t neat{},floorCvar{};
static cvar_t *r_neatsky=&neat;
static struct{cvar_t *drawSkyFloor;}qlRendererCvars{&floorCvar};
static struct{image_t *defaultImage;}tr;
static std::vector<std::string> paths;
static std::vector<int> flagsSeen;
static image_t images[6],fallback;
static int missing=-1,initialized;
static float cloudHeight;
static image_t *R_FindShaderImage(const char *name,imgFlags_t flags){
 const int i=int(paths.size());assert(i<6);paths.emplace_back(name);flagsSeen.push_back(flags);
 return i==missing?nullptr:images+i;
}
static void R_InitSkyTexCoords(float height){cloudHeight=height;++initialized;}
'''

SKY_HOST = r'''
#define HALF_SKY_SUBDIVISIONS 4
static float sky_mins[2][6],sky_maxs[2][6];
static vec3_t s_skyPoints[9][9];
static vec2_t s_cloudTexCoords[6][9][9];
static std::vector<int> faces,minimum;
static int currentFace;
static void MakeSkyVec(float,float,int face,vec3_t){currentFace=face;}
static void FillSkySide(int mins[2],int[2],float [9][9][2]){
 faces.push_back(currentFace);minimum.push_back(mins[1]);
}
'''

CHECKS = r'''
static void Reset(){
 shader={};paths.clear();flagsSeen.clear();initialized=0;missing=-1;neat={};
 tr.defaultImage=&fallback;COM_BeginParseSession("IHV test");
}
static int Parse(const char *keyword,const char *arguments){
 const char *text=arguments;return R_Q3IHVSkyDirective(keyword,&text);
}
int main(int argc,char **argv){
 for(auto profile:{COM_GAME_QUAKE3,COM_GAME_QUAKE3_IHV,COM_GAME_QUAKELIVE,COM_GAME_RTCW_SP,COM_GAME_RTCW_MP,COM_GAME_WOLFET,COM_GAME_STVEF_SP,COM_GAME_STVEF_MP}){
  ri.gameId=profile;const bool q3=(profile==COM_GAME_QUAKE3||profile==COM_GAME_QUAKE3_IHV);Reset();
  for(const char *word:{"fromClient","FROMCLIENT"}){
   assert(bool(R_Q3IHVFromClient(word))==q3);
   assert(ParseColor(word,true)==(q3?CGEN_ENTITY:-1));
   assert(ParseColor(word,false)==(q3?AGEN_ENTITY:-1));
  }
  assert(ParseColor("entity",true)==CGEN_ENTITY && ParseColor("entity",false)==AGEN_ENTITY);
  for(const char *word:{"depthMask","DEPTHMASK"})assert(ParseDepth(word)==(q3?0x100:0));
  assert(ParseDepth("depthwrite")==0x100 && !ParseDepth("depthmaskExtra"));
  assert(!R_Q3IHVFromClient("fromClients") && !R_Q3IHVFromClient(""));
  for(const char *word:{"lightning","LIGHTNING"}){
   const char *p=" untouched";const char *saved=p;
   assert(R_Q3IHVSkyDirective(word,&p)==int(q3) && p==saved);
   assert(shader.sky.ihvLightning==int(q3) && !shader.isSky);
   assert(R_Q3IHVSeparateSkyStages()==int(q3));Reset();
  }
  for(int clouds:{0,1,2})for(int lightning:{0,1}){
   shader.sky.ihvClouds=clouds;shader.sky.ihvLightning=lightning;
   assert(bool(R_Q3IHVSeparateSkyStages())==(q3&&(clouds||lightning)));
  }
  Reset();
  for(const char *word:{"fogparms","skyparms","fogonly","foggen","not-a-keyword"}){
   const char *p=" untouched";const char *saved=p;assert(!R_Q3IHVSkyDirective(word,&p)&&p==saved);
  }
  const char *p=" 400 ignored // compiler parameters\n following";
  const char *saved=p;assert(R_Q3IHVSkyDirective("surfacelight",&p)==int(q3));
  if(q3)assert(!strcmp(COM_ParseExt(&p,qtrue),"following"));else assert(p==saved);
  assert(Parse("sky","env/example")==int(q3));
  assert(paths.size()==(q3?6u:0u));
  assert(Parse("cloudparms","128 full")==int(q3));
  assert(initialized==int(q3)&&shader.sky.ihvClouds==(q3?1:0));
  if(!q3)continue;
  const char *suffix[]={"rt","bk","lf","ft","up","dn"};
  for(int i=0;i<6;++i){
   assert(paths[i]==std::string("env/example_")+suffix[i]+".tga");
   assert(flagsSeen[i]==(IMGFLAG_MIPMAP|IMGFLAG_PICMIP|IMGFLAG_CLAMPTOEDGE));
   assert(shader.sky.outerbox[i]==images+i);
  }
  assert(shader.isSky&&shader.sky.cloudHeight==128&&cloudHeight==128);
  for(const char *bad:{"", "128", "128\nnext", "128 }", "128 bogus", "-1 full", "65537 full", "nan full", "inf full", "1e99 full", "1junk full"}){
   Reset();assert(Parse("cloudparms",bad)==-1);assert(!shader.isSky&&!initialized&&!shader.sky.ihvClouds);
  }
  for(const char *bad:{"", "\nother", "{", "}"}){Reset();assert(Parse("sky",bad)==-1&&paths.empty());}
  Reset();assert(Parse("sky",std::string(MAX_QPATH,'x').c_str())==-1&&paths.empty());
  for(int side=0;side<6;++side){Reset();missing=side;assert(Parse("sky","env/test")==1);assert(shader.sky.outerbox[side]==&fallback);}
  for(int nomip:{0,1})for(int nice:{0,1}){
   Reset();shader.noPicMip=nomip;neat.integer=nice;
   assert(Parse("cloudparms","256 half")==1);assert(Parse("sky","env/test")==1);
   assert(shader.sky.cloudHeight==256&&shader.sky.ihvClouds==2);
   for(int flags:flagsSeen)assert(flags==(nice?IMGFLAG_CLAMPTOEDGE:IMGFLAG_CLAMPTOEDGE|IMGFLAG_MIPMAP|(nomip?0:IMGFLAG_PICMIP)));
  }
  Reset();assert(Parse("cloudparms","0 full")==1&&!initialized&&shader.sky.cloudHeight==0);
 }
 ri.gameId=COM_GAME_QUAKE3;
 for(const char *bad:{"", "}", "{", "nan", "-inf", "1e500", "no", "2x", "\n1"}){
  const char *p=bad;float value=42;assert(!R_Q3IHVShaderFloat(&p,&value)&&value==42&&p==bad);
 }
 // Execute the renderer's actual tessellation, including QL's bottom-face option.
 for(int mode:{0,1,2})for(int drawFloor:{0,1}){
  Reset();shader.sky.ihvClouds=mode;floorCvar.integer=drawFloor;faces.clear();minimum.clear();
  for(int i=0;i<6;++i)for(int j=0;j<2;++j){sky_mins[j][i]=-1;sky_maxs[j][i]=1;}
  FillCloudBox(&shader);assert(faces.size()==(mode!=2&&drawFloor?6u:5u));
  for(size_t i=0;i<faces.size();++i)assert(minimum[i]==(mode==2&&faces[i]<4?-1:-4));
 }
 // No arbitrary malformed file is repaired, even with the right name/size.
 std::string unrelated(8672,' ');const auto original=unrelated;
 assert(!R_Q3IHVRepairShaderText(unrelated.data(),unrelated.size(),"scripts/common.shader")&&unrelated==original);
 for(int size:{-1,0,5,8671,8673})assert(!R_Q3IHVRepairShaderText(unrelated.data(),size,"scripts/common.shader"));
 if(argc==2){
  std::ifstream input(argv[1],std::ios::binary);assert(input);
  const std::string source{std::istreambuf_iterator<char>(input),std::istreambuf_iterator<char>()};
  assert(source.size()==8672);
  for(auto profile:{COM_GAME_QUAKE3,COM_GAME_QUAKE3_IHV,COM_GAME_QUAKELIVE,COM_GAME_RTCW_SP,COM_GAME_RTCW_MP}){
   ri.gameId=profile;auto copy=source;
   assert(bool(R_Q3IHVRepairShaderText(copy.data(),copy.size(),"scripts/common.shader"))==((profile==COM_GAME_QUAKE3||profile==COM_GAME_QUAKE3_IHV)));
   if((profile!=COM_GAME_QUAKE3&&profile!=COM_GAME_QUAKE3_IHV)){assert(copy==source);continue;}
   int count=0;bool light5=false,light6=false;const char *p=copy.c_str();
   while(p){const char *token=COM_ParseExt(&p,qtrue);if(!*token)break;
    light5|=!strcmp(token,"textures/common/light5");light6|=!strcmp(token,"textures/common/light6");
    assert(!strcmp(COM_ParseExt(&p,qtrue),"{")&&SkipBracedSection(&p,1));++count;
   }
   assert(count>30&&light5&&light6);assert(!R_Q3IHVRepairShaderText(copy.data(),copy.size(),"scripts/common.shader"));
   for(size_t i=0;i<source.size();++i){auto corrupt=source;corrupt[i]^=1;assert(!R_Q3IHVRepairShaderText(corrupt.data(),corrupt.size(),"scripts/common.shader"));}
   copy=source;assert(!R_Q3IHVRepairShaderText(copy.data(),copy.size(),"scripts/other.shader")&&copy==source);
  }
 }
}
'''


class Q3IHVShaderTests(unittest.TestCase):
    def test_materials_and_cloud_geometry(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++ compiler unavailable')
        flags = compiler_target_args(compiler)
        if os.name == 'nt' and not flags:
            flags = ['-DFNTECH3_Q3_ONLY']
        (ROOT / '.tmp').mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='q3-ihv-shader-', dir=ROOT / '.tmp') as temp:
            for renderer in RENDERERS:
                with self.subTest(renderer=renderer):
                    local = (ROOT / 'code' / renderer / 'tr_local.h').read_text()
                    records = ''
                    for name in ('skyParms_t', 'colorGen_t', 'alphaGen_t'):
                        end = local.index('} ' + name + ';') + len('} ' + name + ';')
                        records += local[local.rindex('typedef ', 0, end):end] + '\n'
                    source = (ROOT / 'code' / renderer / 'tr_shader.c').read_text()
                    branches = re.findall(r'else if \( !Q_stricmp\( token, "entity" \) \|\| R_Q3IHVFromClient\( token \) \)\s*\{[^{}]*\}', source)
                    self.assertEqual(len(branches), 2)
                    color = 'static int ParseColor(const char *token,bool rgb){\n'
                    for rgb, branch in zip(('true', 'false'), branches):
                        branch = re.sub(r'stage->(?:bundle\[0\]\.)?(?:rgbGen|alphaGen) = ([A-Z_]+);', r'return \1;', branch)
                        color += 'if(rgb==' + rgb + '){' + branch.replace('else if', 'if', 1) + '}\n'
                    color += 'return -1;}\n'
                    depth = re.search(r'else if \( !Q_stricmp\( token, "depthwrite" \) \|\| R_Q3IHVDepthMask\( token \) \)\s*\{[^{}]*\}', source)[0]
                    color += '''static int ParseDepth(const char *text){
                        const int GLS_DEPTHMASK_TRUE=0x100;int depthMaskBits=0;qboolean depthMaskExplicit=qfalse;
                        while(text){const char *token=COM_ParseExt(&text,qtrue);if(!*token)break;
                    ''' + depth.replace('else if', 'if', 1) + '''else return 0;}
                        return depthMaskExplicit?depthMaskBits:0;}\n'''
                    cpp = Path(temp) / (renderer + '.cpp')
                    exe = Path(temp) / (renderer + ('.exe' if os.name == 'nt' else ''))
                    cpp.write_text(common(renderer) + HOST + records +
                        'struct shader_t{skyParms_t sky;int noPicMip;bool isSky;};static shader_t shader;\n' +
                        tokenizer() + '\n#include "code/renderercommon/tr_q3_ihv_shader.h"\n' +
                        '#include "code/renderercommon/tr_q3_ihv_shader_text.h"\n' + color + SKY_HOST +
                        function((ROOT / 'code' / renderer / 'tr_sky.c').read_text(), 'static void FillCloudBox(') + CHECKS)
                    build = subprocess.run([compiler, *flags, '-std=c++17', '-O2', '-ffast-math', '-I', str(ROOT), str(cpp), '-o', str(exe)],
                                           cwd=ROOT, capture_output=True, text=True, timeout=60)
                    self.assertEqual(build.returncode, 0, build.stdout + build.stderr)
                    args = [str(exe)]
                    if directory := os.environ.get('FNTECH3_IHV_SHADER_DIRECTORY'):
                        args.append(str(Path(directory) / 'common.shader'))
                    run = subprocess.run(args, cwd=ROOT, capture_output=True, text=True, timeout=30)
                    self.assertEqual(run.returncode, 0, run.stdout + run.stderr)


if __name__ == '__main__':
    unittest.main()
