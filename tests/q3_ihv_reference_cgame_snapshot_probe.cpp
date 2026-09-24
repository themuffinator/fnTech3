/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
/* Optional original IHV cgame snapshot-copy observation. The packet-graph helper
 * called after the copy is replaced by a return. No cgame Init, renderer,
 * filesystem, input, sound, level, or live engine session runs. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wincrypt.h>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <vector>

static_assert(sizeof(void*)==4,"Original cgame requires x86");
static void Check(bool yes,const char *why){if(!yes)throw std::runtime_error(why);}
static void Verify(const char *path){
 std::ifstream input(path,std::ios::binary);Check(bool(input),"Open supplied cgame");
 std::vector<char> data{std::istreambuf_iterator<char>(input),std::istreambuf_iterator<char>()};
 HCRYPTPROV provider{};HCRYPTHASH hash{};
 Check(CryptAcquireContextA(&provider,nullptr,nullptr,PROV_RSA_AES,CRYPT_VERIFYCONTEXT),"Crypto context");
 if(!CryptCreateHash(provider,CALG_SHA_256,0,0,&hash)){CryptReleaseContext(provider,0);Check(false,"Hash create");}
 BYTE digest[32]{};DWORD length=32;
 const bool ok=CryptHashData(hash,reinterpret_cast<const BYTE*>(data.data()),static_cast<DWORD>(data.size()),0)&&CryptGetHashParam(hash,HP_HASHVAL,digest,&length,0);
 CryptDestroyHash(hash);CryptReleaseContext(provider,0);Check(ok&&length==32,"Hash result");
 char text[65]{};for(unsigned i=0;i<32;++i)std::snprintf(text+i*2,3,"%02x",digest[i]);
 Check(!std::strcmp(text,"175f94756f994b06235690fdd2d9fe798503f7a797f2e070277a672847593614"),"Unexpected cgame hash");
}
struct Library {HMODULE value{};~Library(){if(value)FreeLibrary(value);}};
struct Memory {void *value{};~Memory(){if(value)VirtualFree(value,0,MEM_RELEASE);}};
struct PacketGraphStub {
 unsigned char *address;unsigned char original;DWORD protection{};
 explicit PacketGraphStub(unsigned char *at):address(at),original(*at){
  Check(VirtualProtect(address,1,PAGE_EXECUTE_READWRITE,&protection),"Patch protection");
  *address=0xc3;DWORD old{};Check(VirtualProtect(address,1,protection,&old),"Restore protection");
  Check(FlushInstructionCache(GetCurrentProcess(),address,1),"Patch flush");
 }
 ~PacketGraphStub(){DWORD old{};if(VirtualProtect(address,1,PAGE_EXECUTE_READWRITE,&old)){*address=original;VirtualProtect(address,1,protection,&old);FlushInstructionCache(GetCurrentProcess(),address,1);}}
};
int main(int argc,char **argv){
 if(argc!=2)return 77;
 SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);
 try{
  Verify(argv[1]);Library library;library.value=LoadLibraryExA(argv[1],nullptr,DONT_RESOLVE_DLL_REFERENCES);Check(library.value!=nullptr,"Map cgame without entry point");
  auto *base=reinterpret_cast<unsigned char*>(library.value);
  const auto getter=reinterpret_cast<void**(__cdecl*)()>(GetProcAddress(library.value,"GetCGameAPI"));Check(getter!=nullptr,"Getter export");
  void **exports=getter();Check(exports==reinterpret_cast<void**>(base+0x2b020),"Export table");
  Check(exports[1]==base+0xaf30,"Snapshot callback");
  PacketGraphStub stub(base+0x4b90);
  SYSTEM_INFO system{};GetSystemInfo(&system);Memory input;
  input.value=VirtualAlloc(nullptr,system.dwPageSize*2,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);Check(input.value!=nullptr,"Guarded input");
  DWORD old{};Check(VirtualProtect(static_cast<unsigned char*>(input.value)+system.dwPageSize,system.dwPageSize,PAGE_NOACCESS,&old),"Input guard");
  auto *record=static_cast<unsigned char*>(input.value)+system.dwPageSize-348;
  const auto snapshot=reinterpret_cast<void(__cdecl*)(const void*)>(exports[1]);
  constexpr std::size_t ringBytes=32*348;
  auto *ring=base+0xa5f8c;auto *counter=reinterpret_cast<std::uint32_t*>(base+0xa8b0c);
  auto *pending=reinterpret_cast<std::uint32_t*>(base+0xa8b10);auto *initial=reinterpret_cast<std::uint32_t*>(base+0xa1100);
  std::array<unsigned char,ringBytes> expected;
  std::uint64_t digest=14695981039346656037ull;unsigned cases=0;
  const auto add=[&](const void *pointer,std::size_t count){auto *bytes=static_cast<const unsigned char*>(pointer);for(std::size_t i=0;i<count;++i){digest^=bytes[i];digest*=1099511628211ull;}};
  for(std::uint32_t sequence:{0u,1u,30u,31u,32u,63u,64u,0x7ffffffeu,0x7fffffffu,0x80000000u,0xfffffffeu,0xffffffffu})
  for(std::uint32_t initialized:{0u,1u,7u}){
   for(unsigned i=0;i<348;++i)record[i]=static_cast<unsigned char>(i*73+cases*37);
   std::memset(ring,0xa5,ringBytes);*counter=sequence;*pending=0;*initial=initialized;
   std::memcpy(expected.data(),ring,ringBytes);std::memcpy(expected.data()+(sequence&31)*348,record,348);
   snapshot(record);
   Check(!std::memcmp(ring,expected.data(),ringBytes),"Exact ring copy/neighbor preservation");
   Check(*counter==sequence+1&&*pending==1&&*initial==0,"Counter, pending and initial flags");
   add(&sequence,4);add(&initialized,4);add(counter,4);add(pending,4);add(initial,4);add(ring,ringBytes);++cases;
  }
  std::printf("{\"scope\":\"original cgame snapshot copy only; post-copy packet-graph helper stubbed, no cgame Init or engine session\",\"callback_rva\":\"0xaf30\",\"stubbed_helper_rva\":\"0x4b90\",\"snapshot_bytes\":348,\"ring_records\":32,\"cases\":%u,\"guarded_input\":true,\"fnv1a64\":\"%016llx\"}\n",cases,static_cast<unsigned long long>(digest));
  return 0;
 }catch(const std::exception &e){std::fprintf(stderr,"%s\n",e.what());return 1;}
}
