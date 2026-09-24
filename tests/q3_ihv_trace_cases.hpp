/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_IHV_TRACE_CASES_HPP
#define FNTECH3_IHV_TRACE_CASES_HPP
#include "../code/compat/q3_ihv_game_abi.h"
#include <array>
#include <cstdint>
#include <cstring>
#include <vector>
namespace ihv_trace_cases {
using Trace=fntech3_q3_ihv_trace_t;
using Entity=fntech3_q3_ihv_entity_t;
struct Case {
 std::array<float,3> start{1.125f,-5,8},end{-8,9,4.25f},mins{-2,-3,-4},maxs{2,3,6};
 bool nullMins=false,nullMaxs=false;
 int pass=0,mask=3,count=2;
 std::array<int,8> order{1,2,3,4,5,6,7,0};
 std::array<int,8> owner{-1,-1,-1,-1,-1,-1,-1,-1};
 std::array<int,8> contents{1,1,1,1,1,1,1,1};
 std::array<std::int16_t,8> numbers{0,1,2,3,4,5,6,7};
 unsigned currentBits=0,brushBits=0;
 Trace world{};
 std::array<Trace,8> traces{};
};
inline Trace Sample(unsigned value,unsigned salt) {
 Trace t{};t.allsolid=(value&1)!=0;t.startsolid=(value&2)!=0;
 const float fractions[]={0,.25f,.5f,1};t.fraction=fractions[(value>>2)&3];
 for(int i=0;i<3;++i){t.endpos[i]=float(salt*11+i)+.25f;t.planeNormal[i]=float(i-1)*.5f;}
 t.planeDist=float(salt)+.5f;t.planeType=2;t.planeSignbits=3;
 t.planePadding[0]=static_cast<std::uint8_t>(salt*13);t.planePadding[1]=0xab;
 t.surfaceFlags=int(salt*37+1);t.contents=int(salt*31+2);t.entityNum=713;
 return t;
}
inline std::vector<Case> Make() {
 std::vector<Case> result;
 for(unsigned world=0;world<16;++world)for(unsigned first=0;first<16;++first)for(unsigned second=0;second<16;++second){
  Case c;c.world=Sample(world,1);c.traces[1]=Sample(first,2);c.traces[2]=Sample(second,3);
  c.currentBits=(first&1)?0xaa:0x55;c.brushBits=(second&1)?0x55:0xaa;
  c.nullMins=(first&4)!=0;c.nullMaxs=(second&4)!=0;result.push_back(c);
 }
 // Same owner, an owner's own projectile, the owner itself, null/world owners,
 // negative/zero pass values, arbitrary contact order and contents filtering.
 for(int pass:{-2,-1,0,1,2,3,7})for(int owner:{-1,0,1,2,3})for(unsigned flags=0;flags<16;++flags){
  Case c;c.pass=pass;c.count=5;c.order={4,1,3,2,0,5,6,7};c.world=Sample(12,10);
  c.owner[1]=owner;c.owner[2]=owner;c.owner[3]=1;c.owner[4]=2;
  c.currentBits=flags;c.brushBits=flags^0xaa;c.mask=(flags&4)?1:3;
  c.contents[1]=(flags&1)?0:1;c.contents[2]=(flags&2)?2:3;
  for(unsigned i=0;i<8;++i)c.traces[i]=Sample(8,20+i);
  result.push_back(c);
 }
 // No contacts and an immediate world block must not request entity geometry.
 for(unsigned world=0;world<16;++world){Case c;c.count=0;c.world=Sample(world,40);result.push_back(c);}
 // Native filtering reads the state's number, but the pass entity's owner
 // exclusion compares its pointer. Keep those two notions distinct.
 for(int pass:{-1,0,1,2,3})for(int owner:{-1,0,1,2,3}){
  Case c;c.pass=pass;c.count=4;c.world=Sample(12,50);c.numbers={0,3,2,1,4,5,6,7};
  c.owner[1]=owner;c.owner[2]=3;c.owner[3]=2;
  for(unsigned i=0;i<8;++i)c.traces[i]=Sample(8,60+i);result.push_back(c);
 }
 // x87 keeps the sum and one-unit query pad in extended precision until
 // their final float store, which differs from binary32 reassociation.
 for(float position:{16777216.f,-16777216.f,33554432.f,-33554432.f,1073741824.f,-1073741824.f,1e20f,-1e20f})
 for(float extent:{.25f,1.f,3.f,15.f,31.f}){
  Case c;c.start={position,position+32,position-32};c.end={position+32,position-32,position};
  c.mins={-extent,-extent,-extent};c.maxs={extent,extent,extent};c.count=1;
  c.world=Sample(12,70);c.traces[1]=Sample(8,71);result.push_back(c);
 }
 return result;
}
inline void Initialize(const Case &c,std::array<Entity,8> &entities) {
 entities={};
 for(unsigned i=0;i<entities.size();++i){
  auto &e=entities[i];std::memcpy(e.state,&c.numbers[i],2);e.state[2]=73;e.inuse=1;
  e.svFlags=(c.currentBits&(1u<<i))?0x80:0;e.bmodel=(c.brushBits&(1u<<i))!=0;
  e.contents=c.contents[i];e.state[149]=static_cast<std::uint8_t>(i);
  for(int axis=0;axis<3;++axis){
   e.mins[axis]=-float(i+axis+1);e.maxs[axis]=float(i+axis+2);
   const float origin=float(i*100+axis*10)+.125f,angle=float(i*7+axis*11)+.5f;
   std::memcpy(e.state+88+axis*4,&origin,4);std::memcpy(e.state+112+axis*4,&angle,4);
   e.currentOrigin[axis]=origin+1000;e.currentAngles[axis]=angle+90;
  }
  if(c.owner[i]>=0)e.owner=static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&entities[c.owner[i]]));
 }
}
struct Digest {
 std::uint64_t value=14695981039346656037ull;
 void Bytes(const void *pointer,std::size_t count){const auto *p=static_cast<const std::uint8_t*>(pointer);for(std::size_t i=0;i<count;++i){value^=p[i];value*=1099511628211ull;}}
 void Word(std::uint32_t word){for(unsigned i=0;i<4;++i){const auto byte=static_cast<std::uint8_t>(word>>(i*8));Bytes(&byte,1);}}
 void Vector(const float *v){for(unsigned i=0;i<3;++i){std::uint32_t bits;std::memcpy(&bits,v+i,4);Word(bits);}}
 void Add(const Trace &t){Word(t.allsolid);Word(t.startsolid);std::uint32_t bits;std::memcpy(&bits,&t.fraction,4);Word(bits);Vector(t.endpos);Vector(t.planeNormal);std::memcpy(&bits,&t.planeDist,4);Word(bits);Bytes(&t.planeType,4);Word(t.surfaceFlags);Word(t.contents);Word(t.entityNum);}
};
}
#endif
