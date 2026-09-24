/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
/* Optional original native configstring-parser observation. Message reads and
 * the cgame notification are synthetic. No original entry point, filesystem,
 * display, input, level or network session is initialized. */
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
	for (auto range : {std::array<unsigned,2>{0x406000,0x2000},
        {0x40c000,0x1000},{0x410000,0x1000},{0x448000,0x1000}}) {
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
namespace cc=ihv_config_cases;
static const cc::Update *current;
static unsigned callbacks;
static int __cdecl ReadIndex(void*) { return static_cast<int>(current->index); }
static const char *__cdecl ReadText(void*) { return current->value.c_str(); }
static void __cdecl Debug(const char*,...) {}
static void __cdecl Error(int,const char*,...) { Check(false,"Original configstring error"); }
static void __cdecl Changed(int index) { Check(index==static_cast<int>(current->index),"Callback index"); ++callbacks; }
int main(int argc,char **argv) {
 if(argc!=2) return 77;
 SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);
 try {
  LoadReference(argv[1]); Hook(0x410e10,&ReadIndex); Hook(0x410ea0,&ReadText);
  Hook(0x40cc40,&Debug); Hook(0x40cca0,&Error);
  std::array<void*,8> exports{}; exports[2]=reinterpret_cast<void*>(&Changed);
  *reinterpret_cast<void***>(0x43039a0)=exports.data();
  auto *record=reinterpret_cast<fntech3_q3_ihv_gameState_t*>(0x4312948);
  const auto update=reinterpret_cast<void(__cdecl*)(void*)>(0x406df0);
  cc::Digest digest; unsigned cases=0,changes=0;
  for(unsigned variant=0;variant<8;++variant) {
   cc::Strings expected; for(const auto &seed:cc::Seed(variant)) Check(expected.Append(seed.index,seed.value),"Seed capacity");
   *record=expected.Record();
   auto *guardBefore=reinterpret_cast<std::uint32_t*>(0x4312944);
   auto *guardAfter=reinterpret_cast<std::uint32_t*>(0x43177cc);
   *guardBefore=0xdeadbeef; *guardAfter=0xfedcba98;
   for(const auto &value:cc::Updates(variant)) {
    current=&value; callbacks=0; bool changed=false;
    Check(expected.Set(value.index,value.value,changed),"Update capacity");
    update(nullptr);
    Check(callbacks==unsigned(changed),"Changed callback count");
    Check(!std::memcmp(record,&expected.Record(),sizeof(*record)),"Original native record differs");
    Check(*guardBefore==0xdeadbeef && *guardAfter==0xfedcba98,"Native record guards");
    digest.Word(variant);digest.Word(value.index);digest.Word(callbacks);digest.Bytes(record,sizeof(*record));
    ++cases; changes+=callbacks;
   }
  }
  std::printf("{\"scope\":\"original configstring parser with synthetic message reads and cgame callback; no engine session\",\"cases\":%u,\"changes\":%u,\"gamestate_bytes\":20100,\"fnv1a64\":\"%016llx\"}\n",cases,changes,static_cast<unsigned long long>(digest.value));
  return 0;
 } catch(const std::exception &e) { std::fprintf(stderr,"%s\n",e.what());return 1; }
}
