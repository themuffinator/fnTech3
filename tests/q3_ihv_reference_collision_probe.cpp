/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
/* Optional Win32 numerical observation of original SV_LinkEntity. The original
 * entry point, filesystem, display and input are never initialized. BSP leaf
 * lookup is replaced by an empty result so only link geometry is observed. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wincrypt.h>
#include <float.h>
#include "../code/compat/q3_ihv_game_abi.h"
#include "q3_ihv_collision_cases.hpp"
#include <cstdio>
#include <fstream>
#include <iterator>
#include <stdexcept>

static_assert(sizeof(void *) == 4,"Windows x86 reference probe");
#pragma bss_seg(".ihv")
static unsigned char observation[0x4b46000];
#pragma bss_seg()
static int leafCalls;
static void Check(bool value,const char *why) { if (!value) throw std::runtime_error(why); }
static int __cdecl NoLeaves(const float *,const float *,int *,int maximum,int *topnode) {
	Check(maximum == 128,"Original leaf capacity"); ++leafCalls; *topnode = -1; return 0;
}
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
	for (auto range : {std::array<unsigned,2>{0x408000,0x3000},{0x414000,0x1000},
		{0x418000,0x1000},{0x41b000,0x1000},{0x448000,0x1000},{0x45b000,0x1000}}) {
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
		if (address==0x408000) {
			// Redirect only the external leaf query. The original geometry,
			// _ftol, vector length and bounds arithmetic remain unchanged.
			auto *jump=reinterpret_cast<unsigned char *>(0x40aa00); jump[0]=0xe9;
			const unsigned displacement=reinterpret_cast<unsigned>(&NoLeaves)-0x40aa05;
			std::memcpy(jump+1,&displacement,4);
		}
		DWORD old{}; Check(VirtualProtect(reinterpret_cast<void *>(address),length,PAGE_EXECUTE_READ,&old),"Code protection");
		Check(FlushInstructionCache(GetCurrentProcess(),reinterpret_cast<void *>(address),length),"Instruction cache");
	}
}
int main(int argc,char **argv) {
	if(argc!=2) return 77;
	try {
		LoadReference(argv[1]);
		unsigned previous{}; Check(!_controlfp_s(&previous,_PC_53|_RC_NEAR,_MCW_PC|_MCW_RC),"FPU control");
		const auto cases=ihv_collision_cases::Make(); ihv_collision_cases::Digest digest;
		for (std::size_t index=0;index<cases.size();++index) {
			const auto &c=cases[index]; fntech3_q3_ihv_entity_t entity{};
			entity.state[0]=1; entity.inuse=1; entity.bmodel=c.brush;
			std::memcpy(&entity.contents,&c.contents,4);
			std::memcpy(entity.mins,c.mins.data(),12); std::memcpy(entity.maxs,c.maxs.data(),12);
			// Poison the unused transform with different, finite values so the
			// origin-selection branch must agree, not accidentally read zeros.
			const float unused[3]={1234,5678,-4321};
			std::memcpy(entity.state+88,unused,12); std::memcpy(entity.state+112,unused,12);
			std::memcpy(entity.currentOrigin,unused,12); std::memcpy(entity.currentAngles,unused,12);
			entity.svFlags=c.current?0x80:0;
			std::memcpy(c.current?entity.currentOrigin:reinterpret_cast<void *>(entity.state+88),c.origin.data(),12);
			std::memcpy(c.current?entity.currentAngles:reinterpret_cast<void *>(entity.state+112),c.angles.data(),12);
			fntech3::q3ihv::LinkBounds expected,actual;
			Check(ihv_collision_cases::Calculate(c,actual),"Valid synthetic bounds");
			reinterpret_cast<void (__cdecl *)(fntech3_q3_ihv_entity_t *)>(0x41b970)(&entity);
			std::memcpy(&expected.solid,entity.state+154,2);
			std::memcpy(expected.mins,entity.absmin,12); std::memcpy(expected.maxs,entity.absmax,12);
			if(expected.solid!=actual.solid || std::memcmp(expected.mins,actual.mins,12) || std::memcmp(expected.maxs,actual.maxs,12)) {
				std::fprintf(stderr,"Link case %zu: solid %u/%u\n",index,expected.solid,actual.solid);
				for(int i=0;i<3;++i) std::fprintf(stderr,"axis %d min %.9g/%.9g max %.9g/%.9g\n",i,expected.mins[i],actual.mins[i],expected.maxs[i],actual.maxs[i]);
				Check(false,"Original link geometry differs");
			}
			Check(!entity.linked && !entity.linkcount,"Empty leaf query remains unlinked");
			digest.Word(static_cast<std::uint32_t>(index)); digest.Add(expected);
		}
		Check(leafCalls==static_cast<int>(cases.size()),"One leaf query per original link");
		std::printf("{\"scope\":\"link geometry only; BSP query stubbed empty\",\"cases\":%zu,\"x87_precision\":53,\"fnv1a64\":\"%016llx\"}\n",cases.size(),static_cast<unsigned long long>(digest.value));
		return 0;
	} catch(const std::exception &e) { std::fprintf(stderr,"%s\n",e.what()); return 1; }
}
