/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later.
 * Independently generated inputs and original-function digests; no assets. */
#include "code/renderercommon/tr_q3_ihv_model.h"
#include <cassert>
#include <initializer_list>
#include <cstdio>
static uint32_t seed=0x13462587;
static uint32_t Random(){seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;return seed;}
static uint64_t digest=14695981039346656037ull;
static void Hash(const void *input,size_t bytes){auto*p=(const unsigned char*)input;for(size_t i=0;i<bytes;++i){digest^=p[i];digest*=1099511628211ull;}}
int main(){
 float sines[1024];Q3IHV_ModelSines(sines);Hash(sines,sizeof sines);assert(digest==UINT64_C(0x6685f509b8dc9008));digest=14695981039346656037ull;
 unsigned cases=0,components=0;
 for(unsigned sample=0;sample<512;++sample)for(unsigned count:{0u,1u,3u,17u}){
  int16_t packed[34*4];for(unsigned k=0;k<count*8;++k)packed[k]=static_cast<int16_t>(Random());
  float move[3];for(float &v:move)v=float(int(Random()%32768)-16384)/8;
  for(float fraction:{-.13f,.123f,0.0f,.5f,.99f,1.0f,2.0f}){
   ++cases;for(unsigned vertex=0;vertex<count;++vertex){
    float xyz[4]={0,0,0,71},normal[4]={0,0,0,72};
    assert(Q3IHV_ModelVertex(packed+vertex*4,packed+(count+vertex)*4,fraction,move,sines,xyz,normal));
    assert(xyz[3]==71&&normal[3]==72);Hash(xyz,12);Hash(normal,12);components+=6;
   }
  }
 }
 assert(cases==14336&&components==451584&&digest==UINT64_C(0xfd2298c8d50fd698));
 int16_t before[4]={-32768,17,-120,17000},after[4]={32767,100,-111,-32000};
 float move[3]={2,5,7},xyz[3]={1,2,3},normal[3]={4,5,6},oldXYZ[3],oldNormal[3];memcpy(oldXYZ,xyz,12);memcpy(oldNormal,normal,12);
 for(uint32_t bits:{0x7f800000u,0xff800000u,0x7fc00123u}){
  float bad;memcpy(&bad,&bits,4);assert(!Q3IHV_ModelVertex(before,after,bad,move,sines,xyz,normal));
  for(int a=0;a<3;++a){float badMove[3]={2,5,7};memcpy(badMove+a,&bits,4);assert(!Q3IHV_ModelVertex(before,after,.5f,badMove,sines,xyz,normal));}
  assert(!memcmp(xyz,oldXYZ,12)&&!memcmp(normal,oldNormal,12));
 }
 assert(!Q3IHV_ModelVertex(before,after,FLT_MAX,move,sines,xyz,normal)&&!memcmp(xyz,oldXYZ,12)&&!memcmp(normal,oldNormal,12));
 assert(!Q3IHV_ModelVertex(nullptr,after,.5f,move,sines,xyz,normal));
 assert(!Q3IHV_ModelVertex(before,nullptr,.5f,move,sines,xyz,normal));
 assert(!Q3IHV_ModelVertex(before,after,.5f,nullptr,sines,xyz,normal));
 assert(!Q3IHV_ModelVertex(before,after,.5f,move,nullptr,xyz,normal));
 assert(!Q3IHV_ModelVertex(before,after,.5f,move,sines,nullptr,normal));
 assert(!Q3IHV_ModelVertex(before,after,.5f,move,sines,xyz,nullptr));
 assert(Q3IHV_ModelVertex(nullptr,after,0,move,sines,xyz,normal));
 unsigned char unaligned[19];memcpy(unaligned+1,before,8);memcpy(unaligned+10,after,8);
 assert(Q3IHV_ModelVertex(before,after,.13f,move,sines,oldXYZ,oldNormal));
 assert(Q3IHV_ModelVertex(unaligned+1,unaligned+10,.13f,move,sines,xyz,normal));
 assert(!memcmp(xyz,oldXYZ,12)&&!memcmp(normal,oldNormal,12));
 float bounds[2][3]={{-1,-2,-3},{2,3,4}},position[3]={1,2,3},oldPosition[3]={-2,3,1},out[2][3],saved[2][3];
 assert(Q3IHV_ModelBounds(0x40,bounds,bounds,position,oldPosition,out));assert(out[1][0]==5&&out[0][1]==-3&&out[1][2]==6);
 memcpy(saved,out,sizeof saved);position[0]=FLT_MAX;oldPosition[0]=-FLT_MAX;
 assert(!Q3IHV_ModelBounds(0x40,bounds,bounds,position,oldPosition,out)&&!memcmp(saved,out,sizeof saved));
 bounds[1][0]=-2;assert(!Q3IHV_ModelBounds(0,bounds,bounds,nullptr,nullptr,out)&&!memcmp(saved,out,sizeof saved));
 std::puts("MD3 native digest, malformed/unaligned input and staging checks passed");
}
