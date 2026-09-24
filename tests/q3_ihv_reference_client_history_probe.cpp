/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
/* Optional original native client history/getter observation. Synthetic
 * histories only; no original entry point or engine session is initialized.
 * The host base is 0x3f0000 to reserve the original 0x405000 code page. */
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
	Check(reinterpret_cast<unsigned>(GetModuleHandleA(nullptr))==0x3f0000,"Harness image base");
	const auto *dos=reinterpret_cast<const IMAGE_DOS_HEADER *>(data.data());
	const auto *nt=reinterpret_cast<const IMAGE_NT_HEADERS *>(data.data()+dos->e_lfanew);
	const auto *sections=IMAGE_FIRST_SECTION(nt);
	for (auto range : {std::array<unsigned,2>{0x405000,0x1000},{0x40c000,0x1000}}) {
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
static void Hook(unsigned address,const void *target){
 DWORD old{};Check(VirtualProtect(reinterpret_cast<void*>(address),5,PAGE_EXECUTE_READWRITE,&old),"Hook writable");
 auto *p=reinterpret_cast<unsigned char*>(address);p[0]=0xe9;const unsigned displacement=reinterpret_cast<unsigned>(target)-address-5;std::memcpy(p+1,&displacement,4);
 DWORD ignored{};Check(VirtualProtect(p,5,old,&ignored),"Hook protection");Check(FlushInstructionCache(GetCurrentProcess(),p,5),"Hook flush");
}


#include "q3_ihv_configstring_cases.hpp"
#include "../code/compat/q3_ihv_client_data.hpp"
static void __cdecl Error(int,const char*,...) { throw std::runtime_error("future entity"); }
static int Signed(std::uint32_t value){int result;std::memcpy(&result,&value,4);return result;}
int main(int argc,char **argv) {
 if(argc!=2)return 77;
 SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);
 try {
  LoadReference(argv[1]);Hook(0x40cca0,&Error);
  const auto command=reinterpret_cast<int(__cdecl*)(int,void*)>(0x405d60);
  const auto entity=reinterpret_cast<int(__cdecl*)(int,void*)>(0x405dd0);
  const auto getState=reinterpret_cast<void(__cdecl*)(void*)>(0x405d40);
  auto *commands=reinterpret_cast<unsigned char*>(0x43697d8);
  auto *entities=reinterpret_cast<unsigned char*>(0x43407cc);
  for(unsigned i=0;i<64*14;++i)commands[i]=static_cast<unsigned char>(i*73+17);
  for(unsigned i=0;i<1024*164;++i)entities[i]=static_cast<unsigned char>(i*113+61);
  ihv_config_cases::Digest digest;unsigned cases=0,acceptedCommands=0,acceptedEntities=0,errors=0;
  for(std::uint32_t current:{0u,1u,31u,32u,63u,64u,65u,1023u,1024u,1025u,0x7fffffffu,0x80000000u,0x800003ffu,0xfffffc00u,0xfffffffeu,0xffffffffu}) {
   *reinterpret_cast<std::uint32_t*>(0x4369b58)=current;
   *reinterpret_cast<std::uint32_t*>(0x43697cc)=current;
   fntech3::q3ihv::client::CommandHistory modernCommands(current-64);
   fntech3::q3ihv::client::EntityHistory modernEntities(current-1024);
   for(unsigned i=0;i<64;++i) {
    fntech3_q3_ihv_usercmd_t record;const auto sequence=current-63+i;
    std::memcpy(record.bytes,commands+(sequence&63)*14,14);modernCommands.Append(record);
   }
   for(unsigned i=0;i<1024;++i) {
    fntech3_q3_ihv_entityState_t record;const auto sequence=current-1024+i;
    std::memcpy(record.bytes,entities+(sequence&1023)*164,164);modernEntities.Append(record);
   }
   Check(modernCommands.Counter()==current&&modernEntities.Counter()==current,"History publication counters");
   std::vector<std::uint32_t> queries;
   for(int delta=-1025;delta<=1;++delta)queries.push_back(current+static_cast<std::uint32_t>(delta));
   for(std::uint32_t value:{0u,1u,63u,64u,1023u,1024u,0x7fffffffu,0x80000000u,0xffffffffu})queries.push_back(value);
   for(std::uint32_t query:queries)for(unsigned kind=0;kind<2;++kind) {
    std::array<unsigned char,180> output,expected;output.fill(0xa5);expected=output;
    int result=0;try { result=(kind?entity:command)(Signed(query),output.data()+8); }
    catch(const std::exception &e){Check(kind&&!std::strcmp(e.what(),"future entity"),"Unexpected native error");result=2;}
    const int expectedResult=kind&&Signed(query)>=Signed(current)?2:
     (!kind&&Signed(query)>Signed(current))||Signed(query)<=Signed(current-(kind?1024:64))?0:1;
    Check(result==expectedResult,"History acceptance");
    if(result==1) {
     const unsigned size=kind?164:14,index=query&(kind?1023:63);
     std::memcpy(expected.data()+8,(kind?entities:commands)+index*size,size);
     if(kind)++acceptedEntities;else++acceptedCommands;
    }else if(result==2)++errors;
    Check(output==expected,"Exact copy/output guards");
    std::array<unsigned char,180> modern;modern.fill(0xa5);
    const auto status=kind?modernEntities.Copy(query,modern.data()+8,164):modernCommands.Copy(query,modern.data()+8,14);
    Check(static_cast<int>(status)==result&&modern==output,"Native history adapter differs");
    digest.Word(current);digest.Word(query);digest.Word(kind);digest.Word(result);digest.Bytes(output.data(),output.size());++cases;
   }
  }
  const auto historyDigest=digest.value;
  std::vector<unsigned char> output(20116,0xa5),expected=output;
  auto *state=reinterpret_cast<unsigned char*>(0x4312948);
  for(unsigned i=0;i<20100;++i)state[i]=static_cast<unsigned char>(i*97+29);
  std::memcpy(expected.data()+8,state,20100);getState(output.data()+8);Check(output==expected,"Gamestate copy and guards");digest.Bytes(output.data(),output.size());
  std::printf("{\"scope\":\"original client getters compared with the native history adapter on synthetic records; no engine session\",\"cases\":%u,\"accepted_commands\":%u,\"accepted_entities\":%u,\"future_entity_errors\":%u,\"gamestate_bytes\":20100,\"history_fnv1a64\":\"%016llx\",\"fnv1a64\":\"%016llx\"}\n",cases,acceptedCommands,acceptedEntities,errors,static_cast<unsigned long long>(historyDigest),static_cast<unsigned long long>(digest.value));return 0;
 }catch(const std::exception &e){std::fprintf(stderr,"%s\n",e.what());return 1;}
}
