/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
/* Optional original native model-tag interpolation observation. Synthetic
 * model descriptors; no original entry point or engine session is initialized.
 * The host base is 0x3f0000 to reserve the original numeric code pages. */
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
	for (auto range : {std::array<unsigned,2>{0x40a000,0x1000},{0x414000,0x1000},{0x45b000,0x1000}}) {
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
#include "q3_ihv_model_tag_cases.hpp"
#include <filesystem>
#include <climits>
#include <algorithm>
namespace tc=ihv_tag_cases;
static unsigned cases,found,components,different;
static double maxAbsolute;
static tc::Digest digest;
static std::uint32_t Word(const unsigned char *data){std::uint32_t result;std::memcpy(&result,data,4);return result;}
static void Compare(const std::vector<std::uint8_t> &bytes) {
 fntech3::q3ihv::ModelTags model;Check(model.Load(bytes.data(),bytes.size()),"Read metadata");
 std::array<unsigned char,112> native{};auto put=[&](unsigned at,unsigned value){std::memcpy(native.data()+at,&value,4);};
 put(92,1);put(96,model.Frames());put(100,model.TagsPerFrame());put(108,reinterpret_cast<unsigned>(bytes.data()+Word(bytes.data()+96)));
 const auto lerp=reinterpret_cast<fntech3_q3_ihv_tag_t*(__cdecl*)(const void*,int,int,float,const char*)>(0x40a570);
 auto *scratch=reinterpret_cast<fntech3_q3_ihv_tag_t*>(0x2471b30);
 const int last=static_cast<int>(model.Frames()-1);
 std::vector<std::string> names{"missing","TAG_0",""};
 for(unsigned i=0;i<model.TagsPerFrame();++i)names.emplace_back(reinterpret_cast<const char*>(bytes.data()+Word(bytes.data()+96)+i*112));
 for(int start:{0,last/2,last,last+1,INT_MAX})for(int end:{0,last/2,last,last+1,INT_MAX})
 for(float fraction:{-0.5f,0.0f,0.125f,0.33333334f,0.5f,1.0f,1.5f})for(const auto &name:names) {
  fntech3_q3_ihv_tag_t output;std::memset(&output,0xa5,sizeof output);const auto before=output;*scratch=output;
  const auto *expected=lerp(native.data(),start,end,fraction,name.c_str());
  const bool ok=model.Lerp(start,end,fraction,name,output);Check(ok==(expected!=nullptr),"Tag presence differs");
  ++cases;const unsigned accepted=ok;digest.Add(&accepted,4);digest.Add(scratch,sizeof *scratch);
  if(!ok){Check(!std::memcmp(&output,&before,sizeof output)&&!std::memcmp(scratch,&before,sizeof *scratch),"Missing tag modified output");continue;}
  ++found;Check(expected==scratch&&!std::memcmp(output.name,expected->name,64),"Native name preservation/stable output");
  for(unsigned i=0;i<12;++i){float actual,reference;std::memcpy(&actual,reinterpret_cast<const char*>(&output)+64+i*4,4);std::memcpy(&reference,reinterpret_cast<const char*>(expected)+64+i*4,4);
   ++components;if(std::memcmp(&actual,&reference,4))++different;const double delta=std::abs(double(actual)-reference);maxAbsolute=(std::max)(maxAbsolute,delta);
   Check(delta<=2e-6*(1.0+std::abs(double(reference))),"Interpolated pose differs beyond numeric tolerance");
  }
 }
}
int main(int argc,char **argv) {
 if(argc<2||argc>3)return 77;SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);
 try {
  LoadReference(argv[1]);unsigned previous{};Check(!_controlfp_s(&previous,_PC_53|_RC_NEAR,_MCW_PC|_MCW_RC),"FPU control");
  Compare(tc::Model());Compare(tc::Model(2,0));unsigned files=0,taggedFiles=0;
  if(argc==3){std::vector<std::filesystem::path> paths;for(const auto &entry:std::filesystem::recursive_directory_iterator(argv[2]))if(entry.is_regular_file()&&entry.path().extension()==".md3")paths.push_back(entry.path());std::sort(paths.begin(),paths.end());
   for(const auto &path:paths){std::ifstream input(path,std::ios::binary);Check(bool(input),"Supplied model open");std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(input),std::istreambuf_iterator<char>()};
    fntech3::q3ihv::ModelTags model;Check(model.Load(bytes.data(),bytes.size()),"Supplied metadata parse");++files;if(model.TagsPerFrame())++taggedFiles;Compare(bytes);
   }
  }
  std::printf("{\"scope\":\"original CM_LerpTag with generated model descriptors, synthetic and optional read-only MD3 metadata; no engine session\",\"models\":%u,\"tagged_models\":%u,\"cases\":%u,\"found\":%u,\"components\":%u,\"different_float_bits\":%u,\"max_absolute_error\":%.17g,\"fnv1a64\":\"%016llx\"}\n",files,taggedFiles,cases,found,components,different,maxAbsolute,static_cast<unsigned long long>(digest.value));return 0;
 }catch(const std::exception &error){std::fprintf(stderr,"%s\n",error.what());return 1;}
}
