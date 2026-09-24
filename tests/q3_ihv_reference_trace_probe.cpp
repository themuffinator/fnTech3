/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
/* Optional Win32 observations of original server sweeps and point contents.
 * Contact enumeration and CM replies are synthetic. No original entry point,
 * filesystem, display, input, level or network session is initialized. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wincrypt.h>
#include <float.h>
#include "../code/compat/q3_ihv_game_abi.h"
#include <array>
#include <vector>
#include <cstring>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <stdexcept>

static_assert(sizeof(void *) == 4,"Windows x86 reference probe");
#pragma bss_seg(".ihv")
static unsigned char observation[0x4b46000];
#pragma bss_seg()
static void Check(bool value,const char *why) { if (!value) throw std::runtime_error(why); }
static void LoadReference(const char *path) {
	std::ifstream input(path,std::ios::binary); Check(bool(input),"Open original executable");
	std::vector<char> data{std::istreambuf_iterator<char>(input),std::istreambuf_iterator<char>()};
	HCRYPTPROV provider{}; HCRYPTHASH hash{};
	Check(CryptAcquireContextA(&provider,nullptr,nullptr,PROV_RSA_AES,CRYPT_VERIFYCONTEXT),"Crypto context");
	if (!CryptCreateHash(provider,CALG_SHA_256,0,0,&hash)) { CryptReleaseContext(provider,0); Check(false,"Hash create"); }
	BYTE digest[32]{}; DWORD count=32;
	const bool ok=CryptHashData(hash,reinterpret_cast<const BYTE *>(data.data()),static_cast<DWORD>(data.size()),0)
		&& CryptGetHashParam(hash,HP_HASHVAL,digest,&count,0);
	CryptDestroyHash(hash); CryptReleaseContext(provider,0); Check(ok && count==32,"SHA-256");
	char text[65]{}; for(int i=0;i<32;++i) std::snprintf(text+i*2,3,"%02x",digest[i]);
	Check(!std::strcmp(text,"e9fcaa0a5682e7f07d540aacee08cdb77f3ea40190719f5cecf31109a5135d37"),"Unexpected original executable");
	Check(reinterpret_cast<unsigned>(GetModuleHandleA(nullptr))==0x400000,"Harness image base");
	const auto *dos=reinterpret_cast<const IMAGE_DOS_HEADER *>(data.data());
	const auto *nt=reinterpret_cast<const IMAGE_NT_HEADERS *>(data.data()+dos->e_lfanew);
	const auto *sections=IMAGE_FIRST_SECTION(nt);
	for (auto range : {std::array<unsigned,2>{0x40a000,0x2000},
        {0x418000,0x1000},{0x41b000,0x2000},{0x448000,0x1000},{0x45b000,0x1000}}) {
		const unsigned address=range[0],length=range[1],first=reinterpret_cast<unsigned>(observation);
		Check(address>=first && address-first<=sizeof(observation)-length,"Original range overlaps harness");
		for(unsigned i=0;i<nt->FileHeader.NumberOfSections;++i) {
			const unsigned start=0x400000+sections[i].VirtualAddress,end=start+sections[i].SizeOfRawData;
			const unsigned from=address>start?address:start,to=address+length<end?address+length:end;
			if(from>=to) continue;
			const std::size_t offset=sections[i].PointerToRawData+from-start;
			Check(offset<=data.size() && to-from<=data.size()-offset,"Section bounds");
			std::memcpy(reinterpret_cast<void *>(from),data.data()+offset,to-from);
		}
		DWORD old{}; Check(VirtualProtect(reinterpret_cast<void *>(address),length,PAGE_EXECUTE_READ,&old),"Code protection");
		Check(FlushInstructionCache(GetCurrentProcess(),reinterpret_cast<void *>(address),length),"Instruction cache");
	}
}
#include "q3_ihv_trace_cases.hpp"
namespace tc=ihv_trace_cases;
static std::array<tc::Entity,8> entities;
static const tc::Case *currentCase;
static tc::Digest digest;
static int worldCalls,contactCalls,entityCalls,pointCalls;
static int EntityIndex(const tc::Entity *entity){for(int i=0;i<8;++i)if(entity==&entities[i])return i;Check(false,"Unexpected entity");return -1;}
static tc::Entity *__cdecl EntityAt(int index){Check(index>=0&&index<8,"Entity index");return &entities[index];}
static int __cdecl Contacts(const float *mins,const float *maxs,tc::Entity **out,int maximum){
 Check(maximum==1024,"Contact limit");++contactCalls;digest.Word(2);digest.Vector(mins);digest.Vector(maxs);
 for(int i=0;i<currentCase->count;++i)out[i]=&entities[currentCase->order[i]];
 return currentCase->count;
}
static int __cdecl Model(const tc::Entity *e){return 64+EntityIndex(e);}
static tc::Trace __cdecl World(const float *start,const float *end,const float *mins,const float *maxs,int model,int mask){
 Check(model==0&&mask==currentCase->mask,"World model/mask");++worldCalls;digest.Word(1);
 digest.Vector(start);digest.Vector(end);digest.Vector(mins);digest.Vector(maxs);return currentCase->world;
}
static tc::Trace __cdecl Transformed(const float *start,const float *end,const float *mins,const float *maxs,int model,int mask,const float *origin,const float *angles){
 Check(model>=64&&model<72&&mask==currentCase->mask,"Entity model/mask");++entityCalls;digest.Word(3);digest.Word(model-64);
 digest.Vector(start);digest.Vector(end);digest.Vector(mins);digest.Vector(maxs);digest.Vector(origin);digest.Vector(angles);
 return currentCase->traces[model-64];
}
static int __cdecl WorldContents(const float *point,int model){Check(!model,"World point model");digest.Word(4);digest.Vector(point);return 0x800;}
static int __cdecl TransformedContents(const float *point,int model,const float *origin,const float *angles){
 Check(model>=64&&model<72,"Entity point model");++pointCalls;digest.Word(5);digest.Word(model-64);
 digest.Vector(point);digest.Vector(origin);digest.Vector(angles);return 1<<(model-64);
}
static void Hook(unsigned address,const void *target){
 DWORD old{};Check(VirtualProtect(reinterpret_cast<void*>(address),5,PAGE_EXECUTE_READWRITE,&old),"Hook writable");
 auto *p=reinterpret_cast<unsigned char*>(address);p[0]=0xe9;const unsigned displacement=reinterpret_cast<unsigned>(target)-address-5;std::memcpy(p+1,&displacement,4);
 DWORD ignored{};Check(VirtualProtect(p,5,old,&ignored),"Hook protection");Check(FlushInstructionCache(GetCurrentProcess(),p,5),"Hook flush");
}
int main(int argc,char **argv){
 if(argc!=2)return 77;
 try{
  LoadReference(argv[1]);unsigned previous{};Check(!_controlfp_s(&previous,_PC_53|_RC_NEAR,_MCW_PC|_MCW_RC),"FPU control");
  Hook(0x418fa0,&EntityAt);Hook(0x41bee0,&Contacts);Hook(0x41bf30,&Model);
  Hook(0x40ba80,&World);Hook(0x40be20,&Transformed);Hook(0x40ab20,&WorldContents);Hook(0x40abe0,&TransformedContents);
  const auto trace=reinterpret_cast<tc::Trace (__cdecl *)(const float*,const float*,const float*,const float*,int,int)>(0x41c110);
  const auto point=reinterpret_cast<int (__cdecl *)(const float*,int)>(0x41c280);
  const auto cases=tc::Make();
  for(std::size_t index=0;index<cases.size();++index){
   const auto &c=cases[index];currentCase=&c;tc::Initialize(c,entities);const auto before=entities;
   digest.Word(static_cast<std::uint32_t>(index));
   const auto result=trace(c.start.data(),c.nullMins?nullptr:c.mins.data(),c.nullMaxs?nullptr:c.maxs.data(),c.end.data(),c.pass,c.mask);
   digest.Add(result);Check(!std::memcmp(&before,&entities,sizeof(entities)),"Trace mutated entities");
  }
  const auto traceDigest=digest.value;digest={};
  for(std::size_t index=4096;index<cases.size();++index){
   const auto &c=cases[index];currentCase=&c;tc::Initialize(c,entities);const auto before=entities;
   digest.Word(static_cast<std::uint32_t>(index));digest.Word(point(c.start.data(),c.pass));
   Check(!std::memcmp(&before,&entities,sizeof(entities)),"Point query mutated entities");
  }
  std::printf("{\"scope\":\"original server trace/point aggregation with synthetic contacts and CM replies; no BSP or engine session\",\"trace_cases\":%zu,\"point_cases\":%zu,\"world_calls\":%d,\"contact_calls\":%d,\"entity_trace_calls\":%d,\"entity_point_calls\":%d,\"trace_fnv1a64\":\"%016llx\",\"point_fnv1a64\":\"%016llx\"}\n",cases.size(),cases.size()-4096,worldCalls,contactCalls,entityCalls,pointCalls,static_cast<unsigned long long>(traceDigest),static_cast<unsigned long long>(digest.value));
  return 0;
 }catch(const std::exception &e){std::fprintf(stderr,"%s\n",e.what());return 1;}
}
