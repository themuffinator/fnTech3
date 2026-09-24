/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
/* Optional original CM_RegisterModel observation with supplied read-only MD3
 * files and replaced file, allocation and endian services. Compares native
 * descriptors and complete frame/tag records. No original engine entry point,
 * display, input or network session. Numeric host base: 0x3f0000. */
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
	for (auto range : {std::array<unsigned,2>{0x40a000,0x1000},{0x40c000,0x3000},{0x414000,0x1000},{0x448000,0x1000},{0x45b000,0x1000},{0x460000,0x1000}}) {
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

#include "../code/compat/q3_ihv_model_tags.hpp"
#include <filesystem>
#include <algorithm>
#include <cstdlib>
static std::filesystem::path assetRoot;
static std::vector<void*> allocations;
static FILE *activeFile;
static unsigned reads,opens,seeks;
static LONG CALLBACK Fault(EXCEPTION_POINTERS *fault){if(fault->ExceptionRecord->ExceptionCode==EXCEPTION_ACCESS_VIOLATION){std::fprintf(stderr,"fault eip=%08lx address=%08lx opens=%u reads=%u seeks=%u\n",fault->ContextRecord->Eip,static_cast<unsigned long>(fault->ExceptionRecord->ExceptionInformation[1]),opens,reads,seeks);std::fflush(stderr);}return EXCEPTION_CONTINUE_SEARCH;}
static void __cdecl Error(int,const char*,...){throw std::runtime_error("Original model error");}
static void __cdecl Print(const char*,...){throw std::runtime_error("Unexpected original diagnostic");}
static void *__cdecl Allocate(int bytes){Check(bytes>=0,"Original allocation size");void *p=std::calloc(bytes?bytes:1,1);Check(p!=nullptr,"Allocation");allocations.push_back(p);return p;}
static int __cdecl Open(const char *name,FILE **file,int unique){Check(name&&file&&unique==1&&!activeFile,"Open model arguments");const auto path=assetRoot/name;Check(fopen_s(&activeFile,path.string().c_str(),"rb")==0&&activeFile,"Original model file open");*file=activeFile;++opens;return static_cast<int>(std::filesystem::file_size(path));}
static std::size_t __cdecl Read(void *out,std::size_t bytes,std::size_t count,FILE *file){Check(file==activeFile&&out,"Read arguments");++reads;return std::fread(out,bytes,count,file);}
static int __cdecl Seek(FILE *file,long offset,int origin){Check(file==activeFile&&origin==SEEK_CUR,"Seek arguments");++seeks;return std::fseek(file,offset,origin);}
static int __cdecl Close(FILE *file){Check(file==activeFile,"Close file");activeFile=nullptr;return std::fclose(file);}
static char *__cdecl Name(char *out,const char *input,std::size_t count){std::size_t i=0;for(;i<count&&input[i];++i)out[i]=input[i];for(;i<count;++i)out[i]=0;return out;}
static int __cdecl LittleWord(int value){return value;}
static float __cdecl LittleFloat(float value){return value;}
static void Hook(unsigned address,const void *target){DWORD old{},ignored{};Check(VirtualProtect(reinterpret_cast<void*>(address),5,PAGE_EXECUTE_READWRITE,&old),"Hook writable");auto *p=reinterpret_cast<unsigned char*>(address);p[0]=0xe9;const unsigned delta=reinterpret_cast<unsigned>(target)-address-5;std::memcpy(p+1,&delta,4);Check(VirtualProtect(p,5,PAGE_EXECUTE_READ,&ignored)&&FlushInstructionCache(GetCurrentProcess(),p,5),"Hook executable");}
int main(int argc,char **argv){
 if(argc!=3)return 77;SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);AddVectoredExceptionHandler(1,&Fault);
 try{
  LoadReference(argv[1]);assetRoot=std::filesystem::canonical(argv[2]);
  Hook(0x40cca0,&Error);Hook(0x40cae0,&Print);Hook(0x40d1b0,&Allocate);Hook(0x40ec40,&Open);Hook(0x448a1e,&Read);Hook(0x448ec5,&Seek);Hook(0x44889e,&Close);Hook(0x448920,&Name);
  Hook(0x4148d0,&LittleWord);Hook(0x4148f0,&LittleFloat);
  const auto load=reinterpret_cast<fntech3_q3_ihv_collision_model_t*(__cdecl*)(const char*)>(0x40a140);
  std::vector<std::filesystem::path> paths;for(const auto &entry:std::filesystem::recursive_directory_iterator(assetRoot))if(entry.is_regular_file()&&entry.path().extension()==".md3")paths.push_back(entry.path());std::sort(paths.begin(),paths.end());
  std::uint64_t frameBytes=0,tagBytes=0,digest=14695981039346656037ull;
  auto hash=[&](const void *data,std::size_t length){const auto *p=static_cast<const unsigned char*>(data);for(std::size_t i=0;i<length;++i){digest^=p[i];digest*=1099511628211ull;}};
  for(const auto &path:paths){const auto name=path.lexically_relative(assetRoot).generic_string();std::ifstream input(path,std::ios::binary);std::vector<char> bytes{std::istreambuf_iterator<char>(input),std::istreambuf_iterator<char>()};
   fntech3::q3ihv::NativeModelMetadata modern;Check(modern.Load(name,bytes.data(),bytes.size()),"Modern model metadata");const auto *original=load(name.c_str());Check(original&&!activeFile,"Original registration");const auto *record=modern.Record();
   Check(!std::memcmp(original,record,104),"Native descriptor prefix differs");const std::size_t frames=record->frameCount*56u,tags=record->frameCount*record->tagCount*112u;
   Check(!std::memcmp(reinterpret_cast<const void*>(original->frames),reinterpret_cast<const void*>(record->frames),frames),"Frame records differ");
   if(tags)Check(!std::memcmp(reinterpret_cast<const void*>(original->tags),reinterpret_cast<const void*>(record->tags),tags),"Tag records differ");
   Check(load(name.c_str())==original,"Registration cache identity");frameBytes+=frames;tagBytes+=tags;hash(original,104);hash(reinterpret_cast<const void*>(original->frames),frames);if(tags)hash(reinterpret_cast<const void*>(original->tags),tags);
  }
  for(void *p:allocations)std::free(p);allocations.clear();std::printf("{\"models\":%zu,\"opens\":%u,\"reads\":%u,\"seeks\":%u,\"frame_bytes\":%llu,\"tag_bytes\":%llu,\"fnv1a64\":\"%016llx\"}\n",paths.size(),opens,reads,seeks,static_cast<unsigned long long>(frameBytes),static_cast<unsigned long long>(tagBytes),static_cast<unsigned long long>(digest));return 0;
 }catch(const std::exception &e){if(activeFile)std::fclose(activeFile);for(void *p:allocations)std::free(p);std::fprintf(stderr,"%s\n",e.what());return 1;}
}
