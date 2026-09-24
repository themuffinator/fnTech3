/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
/* Optional original cgame configstring notification probe. No Init, renderer,
 * sound, input or engine session; registration imports are test callbacks. */
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

#include "../code/compat/q3_ihv_configstrings.hpp"
#include <string>
static fntech3::q3ihv::client::ConfigStrings strings;
static unsigned char *moduleBase;
static unsigned copyCalls,modelCalls,soundCalls,musicCalls;
static unsigned currentCase;
static std::string currentText;
static void __cdecl Unexpected() { Check(false,"Unexpected cgame import"); }
static void __cdecl GetState(void *output) {
 Check(output==moduleBase+0xa1104,"GetGameState destination");
 std::memcpy(output,&strings.Record(),sizeof(strings.Record()));++copyCalls;
}
static void Text(const char *value) {
 const auto address=reinterpret_cast<std::uintptr_t>(value),first=reinterpret_cast<std::uintptr_t>(moduleBase+0xa2104);
 Check(address>=first&&address-first+currentText.size()+1<=16000,"Bounded native configstring pointer");
 Check(!std::memcmp(value,currentText.c_str(),currentText.size()+1),"Native configstring text");
}
static int __cdecl Model(const char *value) { Text(value);++modelCalls;return 0x1000+currentCase; }
static int __cdecl Sound(const char *value) { Text(value);++soundCalls;return 0x2000+currentCase; }
static void __cdecl Music(const char *value) { Text(value);++musicCalls; }
int main(int argc,char **argv) {
 if(argc!=2)return 77;
 SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);
 try {
  Verify(argv[1]);Library library;library.value=LoadLibraryExA(argv[1],nullptr,DONT_RESOLVE_DLL_REFERENCES);
  Check(library.value!=nullptr,"Map cgame without entry point");moduleBase=reinterpret_cast<unsigned char*>(library.value);
  const auto getter=reinterpret_cast<void**(__cdecl*)()>(GetProcAddress(library.value,"GetCGameAPI"));Check(getter!=nullptr,"Getter export");
  void **exports=getter();Check(exports==reinterpret_cast<void**>(moduleBase+0x2b020)&&exports[2]==moduleBase+0xa820,"Export identities");
  const auto callback=reinterpret_cast<void(__cdecl*)(int)>(exports[2]);
  auto **imports=reinterpret_cast<void**>(moduleBase+0xa0f80);
  for(unsigned i=0;i<62;++i)imports[i]=reinterpret_cast<void*>(&Unexpected);
  imports[56]=reinterpret_cast<void*>(&GetState);imports[40]=reinterpret_cast<void*>(&Model);
  imports[33]=reinterpret_cast<void*>(&Sound);imports[36]=reinterpret_cast<void*>(&Music);
  std::uint64_t digest=14695981039346656037ull;
  const auto add=[&](const void *p,std::size_t n){auto *b=static_cast<const unsigned char*>(p);for(std::size_t i=0;i<n;++i){digest^=b[i];digest*=1099511628211ull;}};
  unsigned models=0,sounds=0,music=0;
  for(unsigned index:{1u,2u,3u,31u,32u,33u,287u,288u,289u,543u,800u,1023u})
  for(const char *value:{"","synthetic/asset","*custom"}) {
   currentText=value;bool changed{};Check(strings.Set(index,value,changed),"String storage");
   copyCalls=modelCalls=soundCalls=musicCalls=0;
   auto *before=reinterpret_cast<std::uint32_t*>(moduleBase+0xa1100);
   auto *after=reinterpret_cast<std::uint32_t*>(moduleBase+0xa5f88);
   *before=0xdeadbeef;*after=0xfedcba98;
   callback(static_cast<int>(index));
   Check(copyCalls==1&&*before==0xdeadbeef&&*after==0xfedcba98,"Gamestate copy and guards");
   Check(!std::memcmp(moduleBase+0xa1104,&strings.Record(),sizeof(strings.Record())),"Whole native gamestate");
   Check(modelCalls==unsigned(index>=32&&index<288),"Model dispatch");
   Check(soundCalls==unsigned(index>=288&&index<544&&(currentText.empty()||currentText.front()!='*')),"Sound dispatch");
   Check(musicCalls==unsigned(index==2),"Music dispatch");
   if(modelCalls)Check(*reinterpret_cast<unsigned*>(moduleBase+0xaccb0+index*4)==0x1000+currentCase,"Native model handle");
   if(soundCalls)Check(*reinterpret_cast<unsigned*>(moduleBase+0xad0b0+index*4)==0x2000+currentCase,"Native sound handle");
   add(&index,4);add(&copyCalls,4);add(&modelCalls,4);add(&soundCalls,4);add(&musicCalls,4);add(moduleBase+0xa1104,sizeof(strings.Record()));
   models+=modelCalls;sounds+=soundCalls;music+=musicCalls;++currentCase;
  }
  std::printf("{\"scope\":\"original cgame configstring copy and model/sound/music routing with synthetic registration imports; no Init or engine session\",\"cases\":%u,\"models\":%u,\"sounds\":%u,\"music\":%u,\"fnv1a64\":\"%016llx\"}\n",currentCase,models,sounds,music,static_cast<unsigned long long>(digest));
  return 0;
 }catch(const std::exception &e){std::fprintf(stderr,"%s\n",e.what());return 1;}
}
