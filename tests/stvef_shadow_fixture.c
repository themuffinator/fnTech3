/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
typedef int qboolean;
typedef unsigned char GLboolean;
typedef float vec3_t[3];
#define qfalse 0
#ifndef PROBE_NO_PMLIGHT
#define USE_PMLIGHT 1
#endif
enum { COM_GAME_STVEF_SP=5, COM_GAME_STVEF_MP=6 };
#define VectorCopy(a,b) ((b)[0]=(a)[0],(b)[1]=(a)[1],(b)[2]=(a)[2])
#define VectorScale(a,s,b) ((b)[0]=(a)[0]*(s),(b)[1]=(a)[1]*(s),(b)[2]=(a)[2]*(s))
#define VectorMA(a,s,b,c) ((c)[0]=(a)[0]+(s)*(b)[0],(c)[1]=(a)[1]+(s)*(b)[1],(c)[2]=(a)[2]+(s)*(b)[2])
#define DotProduct(a,b) ((a)[0]*(b)[0]+(a)[1]*(b)[1]+(a)[2]*(b)[2])
static struct { int gameId; } ri;
static struct { int stencilBits; } glConfig={8};
static struct { int integer; } shadows={2},*r_shadows=&shadows;
static int dlightMode;
static int R_GetDlightMode(void){
#ifdef USE_PMLIGHT
return dlightMode;
#else
return 0;
#endif
}
static struct entity_s { struct { float shadowPlane; } e; vec3_t lightDir,shadowLightDir; } entity;
static struct { struct {vec3_t origin,axis[3];} or; struct entity_s *currentEntity; } backEnd;
static struct { int numVertexes; float xyz[16][4]; } tess;

#include "stvef_shadow_implementation.inc"

int main(int argc,char **argv) {
    const float vertices[][3]={{-8,-4,0},{16,-4,12},{4,20,32},{0,0,-7}};
    const float directions[][3]={{.2f,.4f,.8f},{.2f,.4f,.1f},{.2f,.4f,-.8f},{0,0,0},{0,0,1},{1,0,0}};
    int type,variant,i,k;
    if(argc==2&&!strcmp(argv[1],"empty")) {
        float original[16][4];
        backEnd.currentEntity=&entity;
        memset(tess.xyz,0x5a,sizeof(tess.xyz));memcpy(original,tess.xyz,sizeof(original));
        tess.numVertexes=0;RB_ShadowTessEnd();
        if(memcmp(original,tess.xyz,sizeof(original)))return 2;
        tess.numVertexes=4;glConfig.stencilBits=0;RB_ShadowTessEnd();
        if(memcmp(original,tess.xyz,sizeof(original)))return 3;
        puts("empty and disabled stencil passed");return 0;
    }
    if(argc!=3)return 1;
    ri.gameId=atoi(argv[1]);dlightMode=atoi(argv[2]);
    backEnd.currentEntity=&entity;
    printf("[\n");
    for(type=0;type<2;++type)for(variant=0;variant<12;++variant) {
        memset(&tess,0,sizeof(tess));tess.numVertexes=4;
        memset(&backEnd.or,0,sizeof(backEnd.or));backEnd.or.origin[2]=24;
        entity.e.shadowPlane=variant<6?0:7;
        for(i=0;i<3;++i)backEnd.or.axis[i][i]=1;
        if(variant>=6) {
            const float axis[3][3]={{.8f,0,.6f},{0,1,0},{-.6f,0,.8f}};
            memcpy(backEnd.or.axis,axis,sizeof(axis));
        }
        memcpy(entity.lightDir,directions[variant%6],12);
        memcpy(entity.shadowLightDir,entity.lightDir,12);
        for(i=0;i<4;++i)memcpy(tess.xyz[i],vertices[i],12);
        if(type)RB_ShadowTessEnd();else RB_ProjectionShadowDeform();
        printf("%s{\"type\":\"%s\",\"variant\":%d,\"vertices\":[",type||variant?",\n":"",type?"stencil":"projection",variant);
        for(i=0;i<4;++i) {
            printf("%s[",i?",":"");
            for(k=0;k<3;++k)printf("%s%.9g",k?",":"",tess.xyz[i+(type?4:0)][k]);
            printf("]");
        }
        printf("]}");
    }
    printf("\n]\n");return 0;
}
