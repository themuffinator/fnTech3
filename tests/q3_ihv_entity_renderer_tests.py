"""Native IHV entity ownership, numeric observations and renderer/ray dispatch.

The original executable is an optional, separately run read-only probe input.
These portable tests retain its numeric digests without proprietary bytes.
"""
from pathlib import Path
import re
import unittest

from mesh_tag_tests import function
import q3_ihv_particles_tests as particles
import rtx_surface_owner_tests

ROOT = Path(__file__).resolve().parents[1]
RENDERERS = particles.RENDERERS

GEOMETRY = r'''
#include "code/renderercommon/tr_q3_ihv_entities.h"
#include "code/renderercommon/tr_q3_ihv_beam.h"
#include <cassert>
#include <initializer_list>
#include <climits>
static uint32_t seed=0x4136289;
static uint32_t Random(){seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;return seed;}
static uint64_t digest=14695981039346656037ull;
static void Hash(const void *input,size_t bytes){auto*p=(const unsigned char*)input;for(size_t i=0;i<bytes;++i){digest^=p[i];digest*=1099511628211ull;}}
int main(){
 for(unsigned i=0;i<18+122880*12;++i)Random();
 unsigned lerps=0;
 for(unsigned sample=0;sample<4096;++sample){
  float origin[3],oldorigin[3],axis[3][3];
  for(float &x:origin)x=float(int(Random()%32768)-16384)*.25f;
  for(float &x:oldorigin)x=float(int(Random()%32768)-16384)*.25f;
  for(auto &row:axis)for(float &x:row)x=float(int(Random()%513)-256)/64;
  for(unsigned flags:{0u,1u,0x40u,0x41u,0x80u,0x400u,0xffffffffu,0x80000040u})for(float fraction:{-2.0f,-.125f,0.0f,.25f,.5f,1.0f,2.0f}){
   float move[3];assert(Q3IHV_EntityOriginLerp(flags,origin,oldorigin,axis,fraction,move));Hash(move,12);++lerps;
  }
 }
 assert(lerps==229376&&digest==UINT64_C(0x172d67dd2e8164e5));
 for(int i=0;i<256*3;++i)Random();digest=14695981039346656037ull;
 unsigned floats=0;
 for(unsigned sample=0;sample<4096;++sample){
  float origin[3],oldorigin[3],vertices[12][3];
  for(float &x:origin)x=float(int(Random()%32768)-16384)/4;
  for(float &x:oldorigin)x=float(int(Random()%32768)-16384)/4;
  if(sample%32==0)memcpy(oldorigin,origin,12);
  for(int i=0;i<5;++i)Random();
  const bool result=!!Q3IHV_BeamVertices(origin,oldorigin,vertices);
  if(sample%32==0){assert(!result);continue;}
  assert(result);for(int i=0;i<14;++i){Hash(vertices[i%12],12);floats+=3;}
 }
 assert(floats==166656&&digest==UINT64_C(0xc8e8eadc64d65e92));
 float origin[3]{},oldorigin[3]={5,7,9},axis[3][3]={{1,0,0},{0,1,0},{0,0,1}};
 float vertices[12][3],prior[12][3];memset(vertices,0x71,sizeof vertices);memcpy(prior,vertices,sizeof prior);
 float move[3]={1,2,3},oldMove[3];memcpy(oldMove,move,12);
 for(uint32_t bad:{0x7f800000u,0xff800000u,0x7fc00001u,0xffc00001u}){
  for(int a=0;a<3;++a){
   float input[3]={1,2,3};memcpy(input+a,&bad,4);
   assert(!Q3IHV_BeamVertices(input,oldorigin,vertices)&&!memcmp(vertices,prior,sizeof prior));
   assert(!Q3IHV_BeamVertices(origin,input,vertices)&&!memcmp(vertices,prior,sizeof prior));
   assert(!Q3IHV_EntityOriginLerp(0x40,input,oldorigin,axis,.5f,move)&&!memcmp(move,oldMove,12));
   assert(!Q3IHV_EntityOriginLerp(0x40,origin,input,axis,.5f,move)&&!memcmp(move,oldMove,12));
   for(int b=0;b<3;++b){float axes[3][3];memcpy(axes,axis,sizeof axes);memcpy(&axes[a][b],&bad,4);
    assert(!Q3IHV_EntityOriginLerp(0x40,origin,oldorigin,axes,.5f,move)&&!memcmp(move,oldMove,12));}
  }
  float fraction;memcpy(&fraction,&bad,4);assert(!Q3IHV_EntityOriginLerp(0x40,origin,oldorigin,axis,fraction,move)&&!memcmp(move,oldMove,12));
 }
 assert(!Q3IHV_BeamVertices(nullptr,oldorigin,vertices)&&!Q3IHV_BeamVertices(origin,nullptr,vertices)&&!Q3IHV_BeamVertices(origin,oldorigin,nullptr));
 assert(!Q3IHV_BeamVertices(origin,origin,vertices)&&!memcmp(prior,vertices,sizeof prior));
 origin[0]=-FLT_MAX;oldorigin[0]=FLT_MAX;
 assert(!Q3IHV_BeamVertices(origin,oldorigin,vertices)&&!memcmp(prior,vertices,sizeof prior));
 assert(!Q3IHV_EntityOriginLerp(0x40,origin,oldorigin,axis,2,move)&&!memcmp(move,oldMove,12));
 assert(!Q3IHV_EntityOriginLerp(0x40,nullptr,oldorigin,axis,1,move));
 assert(!Q3IHV_EntityOriginLerp(0,origin,oldorigin,axis,1,nullptr));
 assert(Q3IHV_EntityOriginLerp(0,nullptr,nullptr,nullptr,0,move));for(float x:move){uint32_t bits;memcpy(&bits,&x,4);assert(!bits);}

}
'''


def fixture(renderer):
    source = particles.fixture(renderer)
    local = (ROOT / 'code' / renderer / 'tr_local.h').read_text()
    enum = re.search(r'typedef enum\s*\{[^{}]*MOD_BAD[^{}]*\}\s*modtype_t;', local)[0]
    source = source.replace('struct image_t {} image;', enum + r'''
struct shader_t;struct aliceSprite_s {shader_t *shader;};
struct model_t {modtype_t type;int index,numLods;char name[MAX_QPATH];struct aliceSprite_s *aliceSprite;};
struct trRefEntity_t {refEntity_t e;qboolean lightingCalculated,intShaderTime;};
static trRefEntity_t entities[2048];
static model_t models[16];
struct image_t {} image;
''')
    source = source.replace('shader_t *ihvParticleShader,*defaultShader=&shader;',
                            'float ihvModelSines[1024];shader_t *ihvParticleShader,*ihvBeamShader,*defaultShader=&shader;image_t *whiteImage=&image;int numModels=1,numSkins=2,numShaders=2;model_t *models[16];void *skins[2];shader_t *shaders[2];')
    source = source.replace('static struct { View viewParms; } backEnd;',
                            'static struct {View viewParms;trRefEntity_t *currentEntity;struct {float axis[3][3];} orientation;struct {int num_entities,numPolys,rdflags;float viewaxis[3][3];trRefEntity_t *entities;srfPoly_t *polys;} refdef;} backEnd;')
    source = source.replace('bool mirror; }', 'bool mirror;int portalView; }')
    source = source.replace('static struct { srfPoly_t *polys=polygons;', 'static struct {trRefEntity_t *entities=::entities; srfPoly_t *polys=polygons;')
    source = source.replace('!strcmp(name,"<particle>")', '(!strcmp(name,"<particle>")||!strcmp(name,"<ihv-beam>"))')
    source = source.replace('assert(vertices==3&&indices==3);', 'assert((vertices==3&&indices==3)||(vertices==12&&indices==36));')
    source = source.replace('assert(flags==2);', 'assert(flags==2||flags==4);')
    source = source.replace('GLX_DYNAMIC_CATEGORY_MASK_PARTICLE=2', 'GLX_DYNAMIC_CATEGORY_MASK_PARTICLE=2,GLX_DYNAMIC_CATEGORY_MASK_BEAM=4')
    source = source.replace('} tess;', 'struct {color4ub_t colors[1][1024];} svars;} tess;')
    source = source.replace('static struct { bool registered=true;',
                            'struct orientationr_t {float origin[3],axis[3][3],modelMatrix[16],viewOrigin[3];};struct viewParms_t {orientationr_t world,orientation;int portalView;};\nstatic struct {viewParms_t viewParms;orientationr_t orientation;trRefEntity_t *currentEntity;model_t *currentModel; bool registered=true;')
    source = source.replace('struct {int numPolys; srfPoly_t *polys;} refdef;',
                            'struct {int numPolys,num_entities,rdflags;trRefEntity_t *entities;float viewaxis[3][3]; srfPoly_t *polys;} refdef;')
    source += r'''
#define MAX_REFENTITIES 2048
enum {PV_NONE=0};
static void R_TrackMuzzleFlashLightEntity(const refEntity_t*){}
static int isnan_fp(const float *value){return !Q3IHV_EntityFinite(value);}
#include "code/renderercommon/tr_q3_ihv_entity_scene.h"
#include "code/renderercommon/tr_q3_ihv_entity_view.h"
#include "code/renderercommon/tr_q3_ihv_beam_shader.h"
#include "code/renderercommon/tr_q3_ihv_beam_surface.h"
#include "code/renderercommon/tr_q3_ihv_model_render.h"
static model_t *R_AllocModel(){if(tr.numModels==16)return nullptr;auto *m=&models[tr.numModels];*m={};m->index=tr.numModels;tr.models[tr.numModels++]=m;return m;}
static int modelReads;
static qhandle_t Missing(const char*,model_t*){++modelReads;return 0;}
static qhandle_t R_RegisterMD3(const char*n,model_t*m){return Missing(n,m);}
static qhandle_t R_RegisterMDC(const char*n,model_t*m){return Missing(n,m);}
static const struct {const char *ext;qhandle_t (*ModelLoader)(const char*,model_t*);} modelLoaders[]={{"md3",Missing}};
static const int numModelLoaders=1;
void Q_strncpyz(char *out,const char *in,int bytes){assert(bytes>0);strncpy(out,in,bytes-1);out[bytes-1]=0;}
int Q_stricmp(const char*a,const char*b){while(*a&&*b&&tolower(*a)==tolower(*b)){++a;++b;}return tolower(*a)-tolower(*b);}
int Q_stricmpn(const char*a,const char*b,int n){
 while(n-- > 0){const int first=tolower(static_cast<unsigned char>(*a++));
  const int second=tolower(static_cast<unsigned char>(*b++));
  if(first!=second)return first-second;if(!first)return 0;}return 0;
}
void Q_strcat(char *out,int bytes,const char *in){assert(strlen(out)+strlen(in)<(size_t)bytes);strcat(out,in);}
const char *COM_GetExtension(const char *name){const auto *dot=strrchr(name,'.');return dot?dot+1:"";}
void COM_StripExtension(const char *in,char *out,int bytes){Q_strncpyz(out,in,bytes);if(auto *dot=strrchr(out,'.'))*dot=0;}
int QDECL Com_sprintf(char *out,int bytes,const char *format,...){va_list args;va_start(args,format);const int result=vsnprintf(out,bytes,format,args);va_end(args);return result;}
static int legacyDraws;
enum {GL_TEXTURE_2D,GL_FLOAT,GL_TRIANGLE_STRIP,CLS_NONE,GLX_STAGE_BEAM_PASS,GLX_LEGACY_DELEGATION_DRAW_ARRAY,GLX_DRAW_NONE,TESS_XYZ=1,TESS_RGBA0=2,DEPTH_RANGE_NORMAL=0};
static void qglDisable(int){}static void qglEnable(int){}static void GL_State(int){}static void qglColor4f(float,float,float,float){}static void GL_ClientState(int,int){}static void qglVertexPointer(int,int,int,const void*){}
static qboolean GLX_CompatStreamDrawBeamsEnabled(){return qfalse;}
static qboolean GLX_CompatTryStreamDrawArrayPass(int,const void*,int,int,int,int){return qfalse;}
static void GLX_CompatDrawArrays(int,int,int,int,int){++legacyDraws;}
static void qglDrawArrays(int,int,int){++legacyDraws;}
static struct {int surface_beam_pipeline;struct {float dynamic_centroid_prev[3];} rt;} vk;
static void GL_Bind(image_t*){}static void vk_bind_pipeline(int){}static void vk_bind_geometry(int){}static void vk_draw_geometry(int,qboolean){++legacyDraws;}
static cvar_t *r_lerpmodels;
static float meshFraction;
float Q_fabs(float value){uint32_t bits;memcpy(&bits,&value,4);bits&=0x7fffffffu;memcpy(&value,&bits,4);return value;}
static void LerpMeshVertexes(md3Surface_t *surface,float fraction){meshFraction=fraction;for(int i=0;i<surface->numVerts;++i)for(int a=0;a<4;++a)tess.xyz[tess.numVertexes+i][a]=float(i+a);}
static qboolean R_ViewPassIsPortal(const viewParms_t *v){return v->portalView?qtrue:qfalse;}
static struct {int integer=1;} drawEntities,*r_drawentities=&drawEntities;
static surfaceType_t entitySurface=SF_ENTITY;
static int meshSubmissions,fogQueries;
static int R_SpriteFogNum(const trRefEntity_t*){++fogQueries;return 0;}
static model_t *R_GetModelByHandle(int h){return tr.models[h];}
// This fixture owns native IHV models; none of its handles owns runtime MDR.
static qboolean R_IsRuntimeMDR(qhandle_t){return qfalse;}
static void R_AddMD3Surfaces(trRefEntity_t*,void*){++meshSubmissions;}
static void R_AddWolfETSurfaces(trRefEntity_t*){assert(0);}
static void R_MDRAddAnimSurfaces(trRefEntity_t*,void*){assert(0);}
static void R_AddIQMSurfaces(trRefEntity_t*,void*){assert(0);}
static void R_AddBrushModelSurfaces(trRefEntity_t*,void*){assert(0);}
// American McGee's Alice's model kinds, unreachable under this fixture's profile.
static qboolean R_LoadAliceTiki(model_t*,const char*){assert(0);return qfalse;}
static qboolean R_LoadAliceSprite(model_t*,const char*){assert(0);return qfalse;}
static void R_AddAliceTikiSurfaces(trRefEntity_t*){assert(0);}
static qboolean R_AliceEntityInView(const trRefEntity_t*){assert(0);return qtrue;}
static void RB_SurfaceAliceSprite(){assert(0);}
// Medal of Honor's model kind and sky-entity view test, unreachable here too.
static void R_AddMOHAATikiSurfaces(trRefEntity_t*){assert(0);}
static qboolean R_MOHAAEntityInView(const trRefEntity_t*){assert(0);return qtrue;}
static void myGlMultMatrix(const float a[16],const float b[16],float out[16]){for(int row=0;row<4;++row)for(int col=0;col<4;++col){out[col*4+row]=0;for(int k=0;k<4;++k)out[col*4+row]+=a[k*4+row]*b[col*4+k];}}
static void RB_SurfaceSprite(){assert(0);}static void RB_SurfaceSplash(){assert(0);}static void RB_SurfaceRailCore(){assert(0);}static void RB_SurfaceRailRings(){assert(0);}static void RB_SurfaceLightningBolt(){assert(0);}static void RB_SurfaceAxis(){assert(0);}
static qboolean RB_SurfaceSTVEF(){assert(0);return qfalse;}
static qboolean RB_SurfaceJKA(){assert(0);return qfalse;}
static unsigned GLX_CompatEntityDynamicCategoryMask(const refEntity_t*){return 4;}
'''
    scene = (ROOT / 'code' / renderer / 'tr_scene.c').read_text()
    source += function(scene, 'void RE_AddRefEntityToScene(')
    source += function((ROOT / 'code' / renderer / 'tr_model.c').read_text(), 'qhandle_t RE_RegisterModel(')
    math = (ROOT / 'code/qcommon/q_math.c').read_text()
    for signature in ['vec_t VectorNormalize(', 'void PerpendicularVector(', 'void ProjectPointOnPlane(',
                      'void RotatePointAroundVector(', 'void MatrixMultiply(']:
        source += function(math, signature)
    surface = (ROOT / 'code' / renderer / 'tr_surface.c').read_text()
    source += function(surface, 'static void RB_SurfaceBeam(')
    source += function(surface, 'static void RB_SurfaceMesh(')
    source += function(surface, 'static void RB_SurfaceEntity(')
    main = (ROOT / 'code' / renderer / 'tr_main.c').read_text()
    source += function(main, 'void R_RotateForEntity(')
    source += function(main, 'static void R_AddEntitySurfaces(')
    return source


CHECKS = r'''
int main(){
 tr.models[0]=models;tr.skins[1]=&image;tr.shaders[1]=&shader;
 for(auto profile:{COM_GAME_QUAKE3,COM_GAME_QUAKELIVE,COM_GAME_RTCW_SP,COM_GAME_RTCW_MP,COM_GAME_WOLFET,COM_GAME_STVEF_SP,COM_GAME_STVEF_MP}){
  ri.gameId=profile;R_CreateIHVBeamShader();assert(!tr.ihvBeamShader);int before=errors;assert(!RE_AddIHVEntitiesToScene(0,nullptr)&&errors==before+1&&!r_numentities);
  tr.numModels=1;int reads=modelReads;assert(!RE_RegisterModel("*beam")&&modelReads>reads);
 }
 ri.gameId=COM_GAME_QUAKE3_IHV;tr.numModels=1;R_CreateIHVBeamShader();assert(tr.ihvBeamShader==&shader);
 assert(stages[0].active&&stages[0].bundle[0].image[0]==&image&&stages[0].bundle[0].tcGen==TCGEN_TEXTURE&&shader.sort==SS_BLEND0);
 assert(stages[0].stateBits==(GLS_SRCBLEND_ONE|GLS_DSTBLEND_ONE));
 const int reads=modelReads,beam=RE_RegisterModel("*beam");assert(beam==1&&models[1].type==MOD_IHV_BEAM&&modelReads==reads);
 assert(RE_RegisterModel("*beam")==beam&&tr.numModels==2&&modelReads==reads);
 assert(!RE_RegisterModel("*BEAM")&&modelReads>reads);
 fnQ3IHVEntity_t input[2]{};input[0].model=beam;input[0].origin[0]=3;input[0].oldorigin[0]=3;input[0].oldorigin[2]=10;
 input[0].frame=3;input[0].oldframe=1;input[0].backlerp=-.5f;input[0].skinNum=-7;input[0].nonNormalizedAxes=1;
 for(int a=0;a<3;++a)input[0].axis[a][a]=1;for(int c=0;c<4;++c)input[0].rgba[c]=byte(c+31);
 input[1]=input[0];input[1].model=0;input[1].skin=input[1].shader=1;input[1].flags=0x80000115;
 assert(RE_AddIHVEntitiesToScene(0,nullptr)&&!r_numentities);
 for(int n:{-1,INT_MIN,INT_MAX})assert(!RE_AddIHVEntitiesToScene(n,input)&&!r_numentities);
 assert(!RE_AddIHVEntitiesToScene(1,nullptr));tr.registered=false;assert(!RE_AddIHVEntitiesToScene(1,input));tr.registered=true;
 assert(RE_AddIHVEntitiesToScene(2,input)&&r_numentities==2);
 auto &e=entities[0].e;assert(e.q3IHV.active&&e.reType==RT_BEAM&&e.hModel==beam&&e.customShader==shader.index);
 assert(e.frame==3&&e.oldframe==1&&e.backlerp==-.5f&&e.skinNum==-7&&e.nonNormalizedAxes&&!memcmp(e.shader.rgba,input[0].rgba,4));
 assert(entities[1].e.q3IHV.flags==0x80000115&&entities[1].e.reType==RT_MODEL&&entities[1].e.hModel==0);
 assert(entities[1].e.renderfx==(RF_FIRST_PERSON|RF_DEPTHHACK)&&entities[1].e.customShader==1&&entities[1].e.customSkin==1);
 input[0].origin[0]=99;assert(e.origin[0]==3);RE_ClearScene();assert(r_firstSceneEntity==2);const auto saved=entities[0];
 assert(RE_AddIHVEntitiesToScene(1,input)&&r_numentities==3&&entities[2].e.origin[0]==99&&!memcmp(&saved,entities,sizeof saved));
 const auto old=entities[3];
 for(int kind=0;kind<3;++kind)for(int value:{-1,INT_MIN,INT_MAX,16}){auto bad=input[1];(kind==0?bad.model:kind==1?bad.skin:bad.shader)=value;input[1]=bad;assert(!RE_AddIHVEntitiesToScene(2,input)&&r_numentities==3&&!memcmp(&old,entities+3,sizeof old));input[1]=input[0];}
 for(int field=0;field<16;++field)for(uint32_t bad:{0x7f800000u,0xff800000u,0x7fc00123u}){
  input[1]=input[0];float *where=field<9?&input[1].axis[field/3][field%3]:field<12?input[1].origin+field-9:field<15?input[1].oldorigin+field-12:&input[1].backlerp;
  memcpy(where,&bad,4);assert(!RE_AddIHVEntitiesToScene(2,input)&&r_numentities==3&&!memcmp(&old,entities+3,sizeof old));
 }
 input[1]=input[0];input[1].flags=0x80;assert(!RE_AddIHVEntitiesToScene(2,input)&&r_numentities==3);input[1].flags=0;
 tr.ihvBeamShader=nullptr;assert(!RE_AddIHVEntitiesToScene(1,input));tr.ihvBeamShader=&shader;
 for(int count:{-1,2049}){r_numentities=count;assert(!RE_AddIHVEntitiesToScene(0,nullptr));}r_numentities=2047;
 assert(!RE_AddIHVEntitiesToScene(2,input)&&r_numentities==2047);assert(RE_AddIHVEntitiesToScene(1,input)&&r_numentities==2048);assert(!RE_AddIHVEntitiesToScene(1,input));
 R_InitNextFrame();assert(!r_numentities&&!r_firstSceneEntity);
 input[0].model=0;input[0].flags=0x800;assert(RE_AddIHVEntitiesToScene(1,input)&&entities[0].e.reType==RT_PORTALSURFACE);
 refEntity_t retail{};retail.q3IHV={0xffffffffu,1};RE_AddRefEntityToScene(&retail,qfalse);assert(!entities[1].e.q3IHV.active&&!entities[1].e.q3IHV.flags);
 float primary[3][3]={{0,1,0},{-1,0,0},{0,0,1}};
 for(int portal:{0,1})for(unsigned high:{0u,0x80000000u,0xffff0000u})for(unsigned flags=0;flags<4096;++flags){
  trRefEntity_t value{};value.e.q3IHV={high|flags,1};const auto before=value;
  const bool visible=!(flags&0x800)&&!((flags&2)&&!portal)&&!((flags&4)&&portal);
  assert(bool(R_Q3IHVEntityView(&value,portal,primary))==visible);
  if(visible&&(flags&0x400))assert(!memcmp(value.e.axis,primary,sizeof primary));else assert(!memcmp(&value,&before,sizeof value));
 }
 for(auto profile:{COM_GAME_QUAKE3,COM_GAME_QUAKELIVE}){ri.gameId=profile;trRefEntity_t value{};value.e.q3IHV={0xc00,1};const auto before=value;assert(R_Q3IHVEntityView(&value,0,primary)&&!memcmp(&before,&value,sizeof value));}
 ri.gameId=COM_GAME_QUAKE3_IHV;trRefEntity_t draw{};draw.e.reType=RT_BEAM;draw.e.q3IHV.active=1;draw.e.customShader=7;draw.e.origin[0]=3;draw.e.oldorigin[0]=3;draw.e.oldorigin[2]=10;backEnd.currentEntity=&draw;
 for(int i=0;i<3;++i){draw.e.axis[i][i]=1;backEnd.or.axis[i][i]=1;}
 tr.refdef.entities=&draw;tr.refdef.num_entities=1;submitted.clear();fogQueries=0;R_AddEntitySurfaces();assert(submitted.size()==1&&!fogQueries);
 draw.e.q3IHV.flags=2;submitted.clear();R_AddEntitySurfaces();assert(submitted.empty());tr.viewParms.portalView=1;R_AddEntitySurfaces();assert(submitted.size()==1);
 draw.e.q3IHV.flags=4;submitted.clear();R_AddEntitySurfaces();assert(submitted.empty());tr.viewParms.portalView=0;R_AddEntitySurfaces();assert(submitted.size()==1);
 draw.e.q3IHV.flags=0x800;submitted.clear();R_AddEntitySurfaces();assert(submitted.empty());draw.e.q3IHV.flags=0;
 viewParms_t view{};orientationr_t rotated{};for(int i=0;i<4;++i)view.world.modelMatrix[i*5]=1;R_RotateForEntity(&draw,&view,&rotated);assert(rotated.modelMatrix[12]==3&&rotated.origin[0]==3);
 ri.gameId=COM_GAME_QUAKE3;R_RotateForEntity(&draw,&view,&rotated);assert(!memcmp(&rotated,&view.world,sizeof rotated));ri.gameId=COM_GAME_QUAKE3_IHV;
 tess={};tess.numVertexes=3;tess.numIndexes=3;RB_SurfaceEntity(&entitySurface);assert(tess.numVertexes==15&&tess.numIndexes==39&&!legacyDraws);
 float points[12][3];assert(Q3IHV_BeamVertices(draw.e.origin,draw.e.oldorigin,points));
 for(int i=0;i<12;++i){assert(!memcmp(tess.xyz[i+3],points[i],12)&&tess.xyz[i+3][3]==1&&tess.vertexColors[i+3].u32==0xff0000ffu);assert(!tess.texCoords[0][i+3][0]&&!tess.texCoords[0][i+3][1]);}
 for(int segment=0;segment<6;++segment){const unsigned a=3+segment*2,b=a+1,c=3+(segment+1)%6*2,d=c+1,expected[]={a,b,c,c,b,d};assert(!memcmp(tess.indexes+3+segment*6,expected,sizeof expected));}
 memcpy(draw.e.oldorigin,draw.e.origin,12);const auto before=tess;RB_SurfaceBeam();assert(!memcmp(&before,&tess,sizeof tess));draw.e.oldorigin[2]=10;
 ri.gameId=COM_GAME_QUAKE3;RB_SurfaceBeam();assert(legacyDraws==1);ri.gameId=COM_GAME_QUAKE3_IHV;draw.e.q3IHV.active=0;RB_SurfaceBeam();assert(legacyDraws==2);draw.e.q3IHV.active=1;
 struct {md3Surface_t surface;int triangle[3];float st[3][2];short vertices[6][4];} mesh{};
 Q3IHV_ModelSines(tr.ihvModelSines);mesh.surface.numFrames=2;mesh.surface.ofsEnd=sizeof mesh;mesh.surface.ofsXyzNormals=offsetof(decltype(mesh),vertices);for(auto &v:mesh.vertices)v[2]=128;mesh.surface.numVerts=3;mesh.surface.numTriangles=1;mesh.surface.ofsTriangles=offsetof(decltype(mesh),triangle);mesh.surface.ofsSt=offsetof(decltype(mesh),st);mesh.triangle[1]=1;mesh.triangle[2]=2;
 draw.e.frame=1;draw.e.oldframe=0;draw.e.q3IHV.flags=0x40;
 for(float fraction:{-2.0f,-.5f,0.0f,.5f,2.0f}){draw.e.backlerp=fraction;tess={};RB_SurfaceMesh(&mesh.surface);assert(tess.numVertexes==3&&tess.numIndexes==3);assert(tess.xyz[0][2]==2+10*fraction);}
 draw.e.oldframe=1;tess={};RB_SurfaceMesh(&mesh.surface);assert(tess.xyz[0][2]==2);
 draw.e.oldframe=0;ri.gameId=COM_GAME_QUAKE3;tess={};RB_SurfaceMesh(&mesh.surface);assert(tess.xyz[0][2]==2);
}
'''


RAY_CHECKS = r'''
int main() {
    /* The path tracer leaves IHV beams, whatever entity carries them, to the
     * raster overlay, which draws the geometry RB_SurfaceBeam builds. */
    ri.gameId = COM_GAME_QUAKE3_IHV;
    Model( MOD_IHV_BEAM )->e.q3IHV.active = 1;
    assert( RT_EntityClass( &entities[0] ) < 0 );
    assert( vk_rt_surface_owner( 0, &tracedShader, 0, SF_ENTITY ) == RT_SURFACE_RASTER );
    Model( MOD_MESH )->e.reType = RT_BEAM;
    entities[0].e.q3IHV.active = 1;
    assert( RT_EntityClass( &entities[0] ) < 0 );
    assert( vk_rt_surface_owner( 0, &tracedShader, 0, SF_ENTITY ) == RT_SURFACE_RASTER );
    /* Its models are traced exactly when raster draws them in the primary
     * view (Q3IHV_EntityVisible); a hidden body is kept for its shadow, and a
     * portal marker never enters the scene. */
    for ( uint32_t flags = 0; flags < 4096; ++flags ) {
        Model( MOD_MESH )->e.q3IHV.active = 1;
        entities[0].e.q3IHV.flags = flags;
        const int cls = RT_EntityClass( &entities[0] );
        assert( ( cls == 0 || cls == 1 ) == ( Q3IHV_EntityVisible( flags, PV_NONE ) != 0 ) );
        if ( flags & Q3IHV_ENTITY_PORTAL ) assert( cls < 0 );
        else if ( flags & Q3IHV_ENTITY_THIRD_PERSON ) assert( cls == 2 );
        else if ( flags & Q3IHV_ENTITY_FIRST_PERSON ) assert( cls == 1 && !Q3IHV_EntityVisible( flags, PV_PORTAL ) );
        else assert( cls == 0 );
        assert( vk_rt_surface_owner( 0, &tracedShader, 0, SF_MD3 ) ==
            ( cls < 0 ? RT_SURFACE_RASTER : RT_SURFACE_TRACED ) );
        /* the record means nothing to other profiles, or unmarked entities */
        ri.gameId = COM_GAME_QUAKE3;
        assert( RT_EntityClass( &entities[0] ) == 0 );
        ri.gameId = COM_GAME_QUAKE3_IHV;
        entities[0].e.q3IHV.active = 0;
        assert( RT_EntityClass( &entities[0] ) == 0 );
    }
    return 0;
}
'''


class Q3IHVEntityRendererTests(unittest.TestCase):
    def compile_run(self, source):
        particles.Q3IHVParticlesTests.compile_run(self, source)

    def test_original_numeric_digests_and_malformed_geometry(self):
        self.compile_run(GEOMETRY)

    def test_owned_batches_flags_registration_and_raster_dispatch(self):
        for renderer in RENDERERS:
            with self.subTest(renderer=renderer):
                self.compile_run(fixture(renderer) + CHECKS)

    def test_ray_leaves_beams_to_raster_and_traces_models_raster_draws(self):
        rtx_surface_owner_tests.compile_run(self, rtx_surface_owner_tests.host() + RAY_CHECKS)

    def test_every_renderer_wires_owned_entry_and_view_selection(self):
        for renderer in RENDERERS:
            with self.subTest(renderer=renderer):
                main = (ROOT / 'code' / renderer / 'tr_main.c').read_text()
                self.assertIn('R_Q3IHVEntityView( ent, R_ViewPassIsPortal( &tr.viewParms ), tr.refdef.viewaxis )', main)
                self.assertIn('re.AddIHVEntitiesToScene = RE_AddIHVEntitiesToScene;',
                              (ROOT / 'code' / renderer / 'tr_init.c').read_text())


if __name__ == '__main__':
    unittest.main()
