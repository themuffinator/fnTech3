"""IHV fog parsing and renderer calculations against pinned binary observations."""
import json
import os
from pathlib import Path
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
#include <cmath>
#include "code/renderercommon/tr_q3_ihv_fog.h"
static char com_token[MAX_TOKEN_CHARS],com_parsename[MAX_TOKEN_CHARS];
static int com_lines,com_tokenline;
int QDECL Com_sprintf(char *out,int size,const char *format,...){
 va_list args;va_start(args,format);int n=std::vsnprintf(out,size,format,args);va_end(args);return n;
}
#define Com_Memset memset
'''

RENDER_HOST = r'''
struct shader_t {char name[MAX_QPATH];fogParms_t fogParms;int contentFlags;};
static shader_t shader;
struct world_t {int numfogs;fog_t *fogs;};
static struct {world_t *world;} tr;
static struct {struct {int time;} refdef;struct {float origin[3],axis[3][3],viewOrigin[3];} orientation;} backEnd;
#define or orientation
static struct {int fogNum,numVertexes;float xyz[SHADER_MAX_VERTEXES][4];} tess;
static int retailCalls;
static void RB_CalcFogTexCoords(float *coords){
 ++retailCalls;for(int i=0;i<tess.numVertexes;++i){coords[2*i]=i/8.0f;coords[2*i+1]=1;}
}
static float R_FogFactor(float s,float t){return s*t;}
static qboolean ParseVector(const char **text,int count,float *out){
 if(strcmp(COM_ParseExt(text,qfalse),"("))return qfalse;
 for(int i=0;i<count;++i)if(!R_Q3IHVShaderFloat(text,out+i))return qfalse;
 return !strcmp(COM_ParseExt(text,qfalse),")")?qtrue:qfalse;
}
float Q_atof(const char *value){return std::strtof(value,nullptr);}
'''

CHECKS = r'''
static bool Finite(float value){unsigned bits;memcpy(&bits,&value,4);return (bits&0x7f800000u)!=0x7f800000u;}
static void Near(float got,float want){if(std::fabs(got-want)>0.00001f){fprintf(stderr,"got %g expected %g\n",got,want);abort();}}
static void Reset(){shader={};strcpy(shader.name,"test/fog");}
int main(){
 for(auto profile:{COM_GAME_QUAKE3,COM_GAME_QUAKE3_IHV,COM_GAME_QUAKELIVE,COM_GAME_RTCW_SP,COM_GAME_RTCW_MP,COM_GAME_WOLFET,COM_GAME_STVEF_SP,COM_GAME_STVEF_MP}){
  ri.gameId=profile;Reset();const char *text=" .2 .4 .6 100 -1";const char *start=text;
  assert(R_Q3IHVFogParms(&text)==((profile==COM_GAME_QUAKE3||profile==COM_GAME_QUAKE3_IHV)?1:0));
  if((profile==COM_GAME_QUAKE3||profile==COM_GAME_QUAKE3_IHV)){assert(shader.fogParms.ihv.enabled&&shader.fogParms.ihv.gradient==-1);Near(shader.fogParms.color[1],.4f);}
  else assert(text==start&&!shader.fogParms.ihv.enabled);
  Reset();assert(ParseFog("fogParms ( .2 .4 .6 ) 100\n"));assert(!shader.fogParms.ihv.enabled&&shader.fogParms.depthForOpaque==100);
  for(const char *word:{"fogonly","foggen"}){
   Reset();text=" sin 100 50 .125 .5";start=text;
   assert(R_Q3IHVFogDirective(word,&text)==((profile==COM_GAME_QUAKE3||profile==COM_GAME_QUAKE3_IHV)?1:0));
   if((profile!=COM_GAME_QUAKE3&&profile!=COM_GAME_QUAKE3_IHV))assert(start==text&&!shader.contentFlags&&!shader.fogParms.ihv.enabled);
  }
 }
 ri.gameId=COM_GAME_QUAKE3;Reset();
 assert(ParseFog("fogParms .2 .4 .6 100 32\n"));assert(shader.fogParms.ihv.gradient==32);
 const char *text="triangle 100 50 .125 .5";assert(R_Q3IHVFogDirective("foggen",&text)==1);
 assert(shader.fogParms.ihv.gradient==32&&shader.fogParms.ihv.wave==3);
 assert(ParseFog("fogParms .2 .4 .6 200 -1\n"));assert(shader.fogParms.ihv.wave==3);
 assert(ParseFog("fogParms ( .2 .4 .6 ) 300\n"));assert(!shader.fogParms.ihv.enabled&&!shader.fogParms.ihv.wave);
 for(const char *input:{"","\n.2 .4 .6 100 1",".2 .4 .6 100",".2 .4 .6 100 1.5",".2 .4 .6 100 2147483648",".2 .4 .6 100 -2147483649",".2 .4 .6 100 junk","nan .4 .6 100 1",".2 inf .6 100 1",".2 .4 1.1 100 1","-.1 .4 .6 100 1",".2 .4 .6 1e99 1",".2 .4 .6 nan 1",".2 .4 .6 100 }"}){
  const auto before=shader;const char *p=input;assert(R_Q3IHVFogParms(&p)==-1);assert(p==input&&!memcmp(&shader,&before,sizeof shader));
 }
 for(const char *input:{"","noise 1 2 3 4","sin 1 2 3","sin nan 2 3 4","square 1 2 1e99 4","triangle 1 2 3 inf","sin 1 2 3 }"}){
  const auto before=shader;const char *p=input;assert(R_Q3IHVFogDirective("foggen",&p)==-1);assert(p==input&&!memcmp(&shader,&before,sizeof shader));
 }
 // Unrecognized directives do not even overwrite COM's shared keyword token.
 text="sky another";const char *keyword=COM_ParseExt(&text,qfalse);assert(!R_Q3IHVFogDirective(keyword,&text));assert(!strcmp(keyword,"sky"));
 fog_t fogs[2]{};world_t world{2,fogs};tr.world=&world;tess.numVertexes=6;tess.fogNum=1;
 const float points[6][4]={{0,0,64,0},{25,0,64,0},{25,0,128,0},{25,0,160,0},{0,0,100,0},{100,0,64,0}};
 memcpy(tess.xyz,points,sizeof points);fogs[1].bounds[1][2]=128;
 fogs[1].colorInt.rgba[0]=51;fogs[1].colorInt.rgba[1]=102;fogs[1].colorInt.rgba[2]=153;
 fogs[1].parms.depthForOpaque=100;
 for(auto primary:{COM_GAME_QUAKE3,COM_GAME_QUAKE3_IHV})for(const auto &v:vectors){
  ri.gameId=primary;
  backEnd={};backEnd.refdef.time=v.time;backEnd.or.viewOrigin[2]=v.eye;
  for(int i=0;i<3;++i)backEnd.or.axis[i][i]=1;
  if(v.rotated){float axis[3][3]={{0,0,1},{0,1,0},{-1,0,0}};memcpy(backEnd.or.axis,axis,sizeof axis);backEnd.or.origin[0]=10;backEnd.or.origin[1]=20;backEnd.or.origin[2]=30;}
  fogs[1].parms.ihv={1,v.gradient,v.wave,100,50,.125f,.5f};float factors[6];RB_CalcFogFactors(factors);
  assert(!retailCalls);for(int i=0;i<6;++i)Near(factors[i],v.expected[i]);
  byte colors[24];float coords[12];RB_Q3IHVFogColors(colors,coords);
  for(int i=0;i<6;++i){assert(colors[4*i]==51&&colors[4*i+1]==102&&colors[4*i+2]==153);assert(abs(int(colors[4*i+3])-int(255*v.expected[i]))<=1);assert(coords[2*i]==0&&coords[2*i+1]==0);}
  for(int mode=0;mode<3;++mode){
   memset(colors,200,sizeof colors);if(mode==0)RB_CalcModulateColorsByFog(colors);else if(mode==1)RB_CalcModulateAlphasByFog(colors);else RB_CalcModulateRGBAsByFog(colors);
   for(int i=0;i<6;++i)for(int c=0;c<4;++c){const int expected=(mode==2||(mode==0&&c<3)||(mode==1&&c==3))?int(200*(1-v.expected[i])):200;assert(abs(int(colors[4*i+c])-expected)<=1);}
  }
 }
 for(auto profile:{COM_GAME_QUAKE3,COM_GAME_QUAKE3_IHV,COM_GAME_QUAKELIVE,COM_GAME_RTCW_SP,COM_GAME_RTCW_MP,COM_GAME_WOLFET,COM_GAME_STVEF_SP,COM_GAME_STVEF_MP}){
  ri.gameId=profile;fogs[1].parms.ihv.enabled=(profile==COM_GAME_QUAKE3||profile==COM_GAME_QUAKE3_IHV)?0:1;
  float factors[6];retailCalls=0;RB_CalcFogFactors(factors);assert(retailCalls==1);for(int i=0;i<6;++i)Near(factors[i],i/8.0f);
 }
 ri.gameId=COM_GAME_QUAKE3;fogs[1].parms.ihv.enabled=1;assert(RB_Q3IHVFog(1));
 for(int i:{-1,0,2,INT_MAX})assert(!RB_Q3IHVFog(i));tr.world=nullptr;assert(!RB_Q3IHVFog(1));
 tr.world=&world;world.fogs=nullptr;assert(!RB_Q3IHVFog(1));
 q3IHVFogParms_t extreme{1,1,2,FLT_MAX,FLT_MAX,FLT_MAX,FLT_MAX};
 for(int time:{INT_MIN,0,INT_MAX}){float d=Q3IHV_FogDepth(&extreme,1,time);assert(Finite(d)&&d>=1);}
 extreme.base=-FLT_MAX;extreme.amplitude=0;assert(Q3IHV_FogDepth(&extreme,1,0)==1);
 const float axis[3][3]={{1,0,0},{0,1,0},{0,0,1}},origin[3]={FLT_MAX,FLT_MAX,-FLT_MAX},eye[3]={-FLT_MAX,0,-FLT_MAX},point[3]={FLT_MAX,0,FLT_MAX};
 double plane[4];for(int g:{-1,0,1}){Q3IHV_FogPlane(g,1,-FLT_MAX,FLT_MAX,origin,axis,plane);float f=Q3IHV_FogFactor(plane,eye,point);assert(Finite(f)&&f>=0&&f<=1);}
}
'''


def records(local, names):
    result = ''
    for name in names:
        end = local.index('} ' + name + ';') + len('} ' + name + ';')
        result += local[local.rindex('typedef ', 0, end):end] + '\n'
    return result


def fixture(renderer):
    local = (ROOT / 'code' / renderer / 'tr_local.h').read_text()
    shader = (ROOT / 'code' / renderer / 'tr_shader.c').read_text()
    calc = (ROOT / 'code' / renderer / 'tr_shade_calc.c').read_text()
    # Compile the actual fogParms branch, including its lookahead/fallback and
    # cursor handling. Other shader syntax is outside this controller fixture.
    start = shader.index('else if ( !Q_stricmp( token, "fogParms" ) )')
    end = shader.index('\n\t\t// portal', start)
    parse = 'static qboolean ParseFog(const char *input){const char **text=&input;while(*text){const char *token=COM_ParseExt(text,qtrue);if(!*token)break;'
    parse += shader[start:end].replace('else if', 'if', 1) + 'else return qfalse;}return qtrue;}\n'
    vector_data = json.loads((ROOT / 'tests/fixtures/q3_ihv_fog_vectors.json').read_text())
    vectors = 'struct Vector{int gradient;float eye;int wave,time,rotated;float expected[6];};static const Vector vectors[]={\n'
    for v in vector_data['cases']:
        values = ','.join(f'{float(n):.9e}f' for n in v['factors'])
        vectors += f'{{{v["gradient"]},{float(v["eyeZ"]):.1f}f,{v["wave"]},{v["time"]},{v["rotated"]},{{{values}}}}},\n'
    vectors += '};\n'
    float_parser = function((ROOT / 'code/renderercommon/tr_q3_ihv_shader.h').read_text(), 'static qboolean R_Q3IHVShaderFloat(')
    source = common(renderer) + HOST + records(local, ('fogParms_t', 'fog_t')) + tokenizer() + float_parser
    source += RENDER_HOST + '#include "code/renderercommon/tr_q3_ihv_fog_parser.h"\n'
    # The fogParms branch also reads Q3Test's five-value form.
    source += '#include "code/renderercommon/tr_q3test_shader_dialect.h"\n'
    source += parse + '#include "code/renderercommon/tr_q3_ihv_fog_render.h"\n'
    for name in ('RB_CalcModulateColorsByFog', 'RB_CalcModulateAlphasByFog', 'RB_CalcModulateRGBAsByFog'):
        source += function(calc, 'void ' + name + '(')
    return source + vectors + CHECKS


def draw_fixture(renderer):
    source = common(renderer) + r'''
#include <cstdint>
#define Com_Memset memset
enum {FP_EQUAL=1,FP_LE=2,GLS_SRCBLEND_SRC_ALPHA=1,GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA=2,GLS_DEPTHFUNC_EQUAL=4};
enum {CLS_NONE=0,CLS_TEXCOORD_ARRAY=1,CLS_COLOR_ARRAY=2,GL_UNSIGNED_BYTE=1,GL_FLOAT=2};
struct Fog {color4ub_t colorInt;};struct World {Fog *fogs;};
struct Shader {int fogPass,cullType,polygonOffset;};
struct Image {int descriptor;};
static Image white{11},fogImage{12};static Fog fogs[2];static World world{fogs};
static struct {World *world;Image *whiteImage,*fogImage;} tr{&world,&white,&fogImage};
static Shader shader{FP_EQUAL,1,0};
static bool ihv,streamed;static int selected,bound,drawn,indexBinds,geometry,retailCoords,ihvColors,state;
static qboolean RB_Q3IHVFog(int n){assert(n==1);return ihv?qtrue:qfalse;}
static void RB_Q3IHVFogColors(byte *colors,float *coords){++ihvColors;colors[0]=51;colors[3]=127;coords[0]=coords[1]=0;}
static void RB_CalcFogTexCoords(float *coords){++retailCoords;coords[0]=.5f;coords[1]=1;}
static void GL_SelectTexture(int unit){selected=unit;}
static void GL_Bind(Image *image){bound=image->descriptor;}
'''
    if renderer == 'renderer':
        source += r'''
#define RENDERER_GLX
typedef Fog fog_t;
static struct {int fogNum=1,numVertexes=1,numIndexes=3;Shader *shader=&::shader;
 struct {color4ub_t colors[3];float texcoords[1][3][2];} svars;int indexes[3];} tess;
static void GL_ClientState(int,int){}
static void qglColorPointer(int,int,int,void *){}
static void qglTexCoordPointer(int,int,int,void *){}
static void GL_State(int value){state=value;}
static qboolean GLX_TryStreamDrawFogPass(const decltype(tess)*){if(streamed){++drawn;return qtrue;}return qfalse;}
static void R_DrawElements(int,int*){++drawn;}
'''
        source += function((ROOT / 'code' / renderer / 'tr_shade.c').read_text(), 'static void RB_FogPass(')
        invocation = 'RB_FogPass();'
        checks = 'assert(tess.svars.colors[0].rgba[3]==(ihv?127:255));'
    else:
        source += r'''
#define USE_VULKAN
#define USE_FOG_ONLY
enum {TYPE_SIGNLE_TEXTURE=1,TESS_ST0=1,TESS_RGBA0=2,DEPTH_RANGE_NORMAL=0,VK_DESC_FOG_ONLY=4};
struct Vk_Pipeline_Def {int shader_type,state_bits,face_culling,polygon_offset;};
static Vk_Pipeline_Def definition;
static struct {int fogNum=1,numVertexes=1;Shader *shader=&::shader;
 struct {color4ub_t colors[1][3];float texcoords[1][3][2];float (*texcoordPtr[1])[2];} svars;} tess;
static struct {unsigned fog_pipelines[2][3][2];} vk;
static int uniform,pipeline,pushes;
struct vk_material_t {int descriptor;};
static void vk_material_init(vk_material_t *material){*material={};}
static void vk_material_set_descriptor(vk_material_t *material,int,int d){material->descriptor=d;}
static void vk_bind_material(vk_material_t *material){bound=material->descriptor;}
static void vk_update_descriptor(int,int d){bound=d;}
static unsigned vk_find_pipeline_ext(unsigned,const Vk_Pipeline_Def *def,qboolean use){assert(use);definition=*def;return 101;}
static void vk_bind_pipeline(unsigned p){pipeline=p;}
static void vk_bind_index(){++indexBinds;}
static void vk_bind_geometry(int flags){geometry=flags;}
static void vk_draw_geometry(int,qboolean){++drawn;}
static void VK_SetFogParams(int *,int *stage){++retailCoords;*stage=1;}
static unsigned VK_PushUniform(int *){++pushes;return 0;}
#include "code/renderercommon/tr_q3_ihv_fog_vk.h"
'''
        source += '#ifdef USE_VULKAN\n' + function((ROOT / 'code' / renderer / 'tr_shade.c').read_text(), 'static void RB_FogPass(')
        invocation = 'RB_FogPass(rebind?qtrue:qfalse);'
        checks = r'''
assert(indexBinds==rebind);assert(pipeline==(ihv?101:0));
assert(pushes==(ihv?0:1));if(ihv){assert(geometry==(TESS_ST0|TESS_RGBA0));
 assert(definition.state_bits==state&&definition.shader_type==TYPE_SIGNLE_TEXTURE&&definition.face_culling==cull&&definition.polygon_offset==offset);
 assert(tess.svars.texcoordPtr[0]==tess.svars.texcoords[0]&&tess.svars.colors[0][0].rgba[3]==127);}
'''
    source += r'''
int main(){fogs[1].colorInt.rgba[3]=255;
 for(int legacy=0;legacy<2;++legacy)for(int equal=0;equal<2;++equal)for(int rebind=0;rebind<2;++rebind)
 for(int cull=0;cull<3;++cull)for(int offset=0;offset<2;++offset){
  ihv=legacy;streamed=rebind;shader={equal?FP_EQUAL:FP_LE,cull,offset};
  drawn=bound=indexBinds=geometry=retailCoords=ihvColors=0;
  state=GLS_SRCBLEND_SRC_ALPHA|GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA|(equal?GLS_DEPTHFUNC_EQUAL:0);
#ifdef USE_VULKAN
  pushes=0;
#endif
''' + invocation + r'''
  assert(bound==(ihv?11:12)&&selected==0&&drawn==1);
  assert(ihvColors==(ihv?1:0)&&retailCoords==(ihv?0:1));
''' + checks + '}}\n'
    return source


def light_fixture(renderer):
    source = common(renderer) + r'''
#define USE_PMLIGHT
#define USE_LEGACY_DLIGHTS
static bool qglGenProgramsARB=true;
static struct {qboolean q3IHVFog;} world;
static struct {decltype(world) *world;} tr{&world};
static cvar_t mode;static cvar_t *r_dlightMode=&mode;
'''
    path = ROOT / 'code' / renderer / ('tr_light.c' if renderer == 'renderer' else 'tr_local.h')
    signature = 'int R_GetDlightMode(' if renderer == 'renderer' else 'static ID_INLINE int R_GetDlightMode('
    source += function(path.read_text(), signature)
    source += r'''
int main(){
 for(auto profile:{COM_GAME_QUAKE3,COM_GAME_QUAKE3_IHV,COM_GAME_QUAKELIVE,COM_GAME_RTCW_SP,COM_GAME_RTCW_MP,COM_GAME_WOLFET,COM_GAME_STVEF_SP,COM_GAME_STVEF_MP})
 for(int value:{-1,0,1,2,3})for(bool hardware:{false,true})for(bool loaded:{false,true})for(bool legacyFog:{false,true}){
  ri.gameId=profile;mode.integer=value;qglGenProgramsARB=hardware;
  tr.world=loaded?&world:nullptr;world.q3IHVFog=legacyFog?qtrue:qfalse;
  int expected=value;
  if((profile==COM_GAME_QUAKE3||profile==COM_GAME_QUAKE3_IHV)&&loaded&&legacyFog)expected=0;
'''
    if renderer == 'renderer':
        source += 'if(!hardware)expected=0;\n'
    return source + 'assert(R_GetDlightMode()==expected);}}\n'


def loader_fixture(renderer):
    local = (ROOT / 'code' / renderer / 'tr_local.h').read_text()
    bsp = (ROOT / 'code' / renderer / 'tr_bsp.c').read_text()
    start = bsp.index('\t\tout->parms = shader->fogParms;')
    end_text = 'out->color[ n ] = (float) out->colorInt.rgba[ n ] / 255.0f;'
    end = bsp.index(end_text, start) + len(end_text)
    source = common(renderer) + HOST + records(local, ('fogParms_t', 'fog_t'))
    source += r'''
static struct {float identityLight;}tr;
static struct {qboolean q3IHVFog;}s_worldData;
struct Shader {fogParms_t fogParms;};
static void Load(fog_t *out,Shader *shader,float *fogColor){int n;
''' + bsp[start:end] + r'''
}
int main(){
 for(auto profile:{COM_GAME_QUAKE3,COM_GAME_QUAKE3_IHV,COM_GAME_QUAKELIVE,COM_GAME_RTCW_SP,COM_GAME_RTCW_MP,COM_GAME_WOLFET,COM_GAME_STVEF_SP,COM_GAME_STVEF_MP})
 for(int ihv:{0,1}){
  ri.gameId=profile;tr.identityLight=.5f;s_worldData={};Shader shader{};shader.fogParms.ihv.enabled=ihv;
  shader.fogParms.ihv.gradient=-1;shader.fogParms.depthForOpaque=300;float color[3]={.4f,.8f,1.0f};fog_t fog{};
  Load(&fog,&shader,color);const bool enabled=(profile==COM_GAME_QUAKE3||profile==COM_GAME_QUAKE3_IHV)&&ihv;
  assert(bool(s_worldData.q3IHVFog)==enabled);assert(fog.parms.depthForOpaque==300&&fog.parms.ihv.gradient==-1);
  assert(fog.colorInt.rgba[0]==(enabled?102:51)&&fog.colorInt.rgba[1]==(enabled?204:102));
  assert(fog.colorInt.rgba[2]==(enabled?255:127)&&fog.colorInt.rgba[3]==255);
 }
}
'''
    return source


class Q3IHVFogTests(unittest.TestCase):
    def compile_fixture(self, make_fixture):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++ compiler unavailable')
        flags = compiler_target_args(compiler)
        if os.name == 'nt' and not flags:
            flags = ['-DFNTECH3_Q3_ONLY']
        (ROOT / '.tmp').mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='q3-ihv-fog-', dir=ROOT / '.tmp') as temp:
            for renderer in RENDERERS:
                with self.subTest(renderer=renderer):
                    cpp = Path(temp) / (renderer + '.cpp')
                    exe = cpp.with_suffix('.exe' if os.name == 'nt' else '.bin')
                    cpp.write_text(make_fixture(renderer))
                    build = subprocess.run([compiler, *flags, '-std=c++17', '-fno-operator-names', '-O2', '-ffast-math', '-I', str(ROOT), str(cpp), '-o', str(exe)],
                                           cwd=ROOT, capture_output=True, text=True, timeout=60)
                    self.assertEqual(build.returncode, 0, build.stdout + build.stderr)
                    run = subprocess.run([str(exe)], cwd=ROOT, capture_output=True, text=True, timeout=30)
                    self.assertEqual(run.returncode, 0, run.stdout + run.stderr)

    def test_reference_density_parser_and_transparency(self):
        self.compile_fixture(fixture)

    def test_draw_paths_preserve_vertex_density_and_retail_fallback(self):
        self.compile_fixture(draw_fixture)

    def test_classic_lighting_is_scoped_to_ihv_fog_worlds(self):
        self.compile_fixture(light_fixture)

    def test_loader_preserves_original_fog_color_and_parameters(self):
        self.compile_fixture(loader_fixture)


if __name__ == '__main__':
    unittest.main()
