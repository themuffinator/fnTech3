/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#include "code/qcommon/q_shared.h"
#include <assert.h>
#include <float.h>
#include "code/compat/stvef_game_abi.h"
enum { RT_STVEF_ORIENTEDSPRITE=2,RT_STVEF_ALPHAVERTPOLY=3,RT_STVEF_LINE=9,
 RT_STVEF_ORIENTEDLINE=10,RT_STVEF_LINE2=11,RT_STVEF_BEZIER=12,
 RT_STVEF_CYLINDER=13,RT_STVEF_ELECTRICITY=14,SF_TRIANGLES=1 };
typedef struct {
 int reType;vec3_t axis[3],origin,oldorigin;float radius,rotation;color4ub_t shader;
 struct { float width,width2,stscale,height,bias,deviation;int wrap,taper;
          vec3_t control1,control2;color4ub_t vertexColors[4]; } stvef;
} refEntity_t;
static struct {refEntity_t e;} entity;
typedef struct {struct {vec3_t origin,axis[3];} or;int mirror;} View;
static struct {View viewParms;struct {refEntity_t e;} *currentEntity;} backEnd;
static struct {int surfType,numVertexes,numIndexes;unsigned indexes[1024];
 float xyz[256][4],normal[256][4],texCoords[2][256][2];color4ub_t vertexColors[256];} tess;
const vec3_t vec3_origin={0,0,0};
static qboolean R_ViewPassIsMirror(const View *v) {return v->mirror?qtrue:qfalse;}
static int flushes;
static void CheckIndexes(void) {
 int i;assert(tess.numVertexes>=0&&tess.numVertexes<=256&&tess.numIndexes>=0&&tess.numIndexes<=1024);
 for(i=0;i<tess.numIndexes;++i)assert(tess.indexes[i]<(unsigned)tess.numVertexes);
}
static void CheckOverflow(int vertices,int indexes) {
 assert(vertices>=0&&vertices<=256&&indexes>=0&&indexes<=1024);
 if(tess.numVertexes+vertices>256 || tess.numIndexes+indexes>1024) {
  CheckIndexes();++flushes;memset(&tess,0,sizeof(tess));
 }
}
#define RB_CHECKOVERFLOW(v,i) CheckOverflow(v,i)
static unsigned randomIndex;
static float ProbeRandom(void) { return 2.0f*(((++randomIndex)*1777+123)&32767)/32767.0f-1.0f; }
#undef crandom
#define crandom() ProbeRandom()
/* Actual engine math, quad and shared effect implementations are inserted here. */
#include "stvef_surface_implementation.inc"
static int Bounds(void);

int main(int argc,char **argv) {
 static const int types[]={2,3,9,10,11,12,13,14};
 int type,variant,i,k,base=argc>1?atoi(argv[1]):0;
 backEnd.currentEntity=(void *)&entity;
 if(argc>1&&!strcmp(argv[1],"bounds"))return Bounds();
 assert(base==0||base==7);
 printf("[\n");
 for(type=0;type<8;++type)for(variant=0;variant<7;++variant){
  refEntity_t *e=&entity.e;memset(e,0,sizeof(*e));memset(&tess,0,sizeof(tess));
  e->reType=types[type];
  tess.numVertexes=tess.numIndexes=base;
  for(i=0;i<base;++i){tess.xyz[i][0]=-999;tess.indexes[i]=i;}
  for(i=0;i<3;++i){
   e->axis[i][i]=1;e->origin[i]=10+i*10;e->oldorigin[i]=40+i*5;
   backEnd.viewParms.or.origin[i]=i==0?-7:3;
   for(k=0;k<3;++k)backEnd.viewParms.or.axis[i][k]=(float)(i==k);
  }
  if(variant==2)memcpy(e->oldorigin,e->origin,sizeof(e->origin));
  if(variant==3) {backEnd.viewParms.or.origin[0]=16;backEnd.viewParms.or.origin[1]=20;backEnd.viewParms.or.origin[2]=30;}
  if(variant==4) {backEnd.viewParms.or.origin[0]=-4096;backEnd.viewParms.or.origin[1]=0;backEnd.viewParms.or.origin[2]=0;}
  e->shader.rgba[0]=201;e->shader.rgba[1]=113;e->shader.rgba[2]=57;e->shader.rgba[3]=255;
  e->stvef.width=4;e->stvef.width2=8;e->stvef.stscale=2.5f;
  e->rotation=variant?37:0;e->radius=4;
  for(i=0;i<4;++i)for(k=0;k<4;++k)e->stvef.vertexColors[i].rgba[k]=(byte)(i*51+k*13);
  for(i=0;i<3;++i){e->stvef.control1[i]=20+i*3;e->stvef.control2[i]=30-i*5;}
  e->stvef.height=12;e->stvef.bias=variant==2?.1f:variant==5?40:variant==6?-1:1;e->stvef.wrap=variant;
  e->stvef.deviation=variant?.3f:0;e->stvef.taper=variant;
  backEnd.viewParms.mirror=variant==1;randomIndex=0;
  assert(RB_SurfaceSTVEF());CheckIndexes();assert(!flushes);
  for(i=0;i<base;++i)assert(tess.xyz[i][0]==-999&&tess.indexes[i]==(unsigned)i);
  for(i=base;i<tess.numVertexes;++i)for(k=0;k<3;++k)assert(isfinite(tess.xyz[i][k]));
  printf("%s{\"type\":%d,\"variant\":%d,\"vertices\":[",type||variant?",\n":"",e->reType,variant);
  for(i=base;i<tess.numVertexes;++i){
   float *v=tess.xyz[i],*uv=tess.texCoords[0][i];byte *c=tess.vertexColors[i].rgba;
   printf("%s[%.9g,%.9g,%.9g,%.9g,%.9g,%u,%u,%u,%u]",i>base?",":"",v[0],v[1],v[2],uv[0],uv[1],c[0],c[1],c[2],c[3]);
  }
  printf("],\"indexes\":[");for(i=base;i<tess.numIndexes;++i)printf("%s%u",i>base?",":"",tess.indexes[i]-base);printf("]}");
 }
 printf("\n]\n");return 0;
}

static void Begin(int type) {
 refEntity_t *e=&entity.e;int i;
 memset(e,0,sizeof(*e));memset(&tess,0,sizeof(tess));memset(&backEnd.viewParms,0,sizeof(backEnd.viewParms));flushes=0;
 e->reType=type;e->origin[0]=10;e->origin[1]=20;e->origin[2]=30;
 e->oldorigin[0]=40;e->oldorigin[1]=45;e->oldorigin[2]=50;
 for(i=0;i<3;++i){e->axis[i][i]=1;backEnd.viewParms.or.axis[i][i]=1;}
 e->radius=4;e->stvef.width=4;e->stvef.width2=8;e->stvef.stscale=2.5f;e->stvef.height=12;e->stvef.bias=1;
 e->shader.rgba[0]=201;e->shader.rgba[1]=113;e->shader.rgba[2]=57;e->shader.rgba[3]=117;
}
static int Bounds(void) {
 static const int types[]={2,3,9,10,11,12,13,14};
 static const float biases[]={NAN,INFINITY,-INFINITY,FLT_MAX,-FLT_MAX};
 static const float lengths[]={NAN,INFINITY,-INFINITY,2147483648.0f,4294967296.0f,FLT_MAX};
 int type,i,k;
 for(type=0;type<8;++type){
  Begin(types[type]);tess.numVertexes=253;tess.numIndexes=3;tess.indexes[0]=0;tess.indexes[1]=1;tess.indexes[2]=2;
  assert(RB_SurfaceSTVEF()&&flushes==1&&tess.numVertexes>0);CheckIndexes();
  for(i=0;i<tess.numVertexes;++i)for(k=0;k<3;++k)assert(isfinite(tess.xyz[i][k]));
  if(types[type]==9||types[type]==10||types[type]==11||types[type]==12||types[type]==14)
   for(i=0;i<tess.numVertexes;++i)assert(tess.vertexColors[i].rgba[3]==117);
 }
 for(i=0;i<5;++i){
  Begin(RT_STVEF_CYLINDER);entity.e.stvef.bias=biases[i];assert(RB_SurfaceSTVEF());CheckIndexes();
  assert(tess.numVertexes==((i==1||i==3)?32:7)*4);
 }
 for(i=0;i<6;++i){
  Begin(RT_STVEF_ELECTRICITY);VectorClear(entity.e.origin);VectorClear(entity.e.oldorigin);entity.e.oldorigin[0]=lengths[i];
  assert(RB_SurfaceSTVEF()&&!tess.numVertexes&&!tess.numIndexes&&!flushes);
 }
 /* Leave room for the engine normalizer's reciprocal-rounding difference. */
 Begin(RT_STVEF_ELECTRICITY);VectorClear(entity.e.origin);VectorClear(entity.e.oldorigin);entity.e.oldorigin[0]=2147482624.0f;
 assert(RB_SurfaceSTVEF()&&tess.numVertexes==64);CheckIndexes();
 Begin(-1);assert(!RB_SurfaceSTVEF()&&!tess.numVertexes&&!tess.numIndexes);
 puts("bounds passed");return 0;
}
