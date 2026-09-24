/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
/* Optional original cgame Init prefix probe. Stop at the collision-map import
 * or, using a synthetic collision reply, the renderer map import. No completed
 * Init, renderer/audio/input/network startup, or game session is established. */
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
static std::vector<char> Verified(const char *path,const char *expected){
 std::ifstream input(path,std::ios::binary);Check(bool(input),"Open supplied cgame");
 std::vector<char> data{std::istreambuf_iterator<char>(input),std::istreambuf_iterator<char>()};
 HCRYPTPROV provider{};HCRYPTHASH hash{};
 Check(CryptAcquireContextA(&provider,nullptr,nullptr,PROV_RSA_AES,CRYPT_VERIFYCONTEXT),"Crypto context");
 if(!CryptCreateHash(provider,CALG_SHA_256,0,0,&hash)){CryptReleaseContext(provider,0);Check(false,"Hash create");}
 BYTE digest[32]{};DWORD length=32;
 const bool ok=CryptHashData(hash,reinterpret_cast<const BYTE*>(data.data()),static_cast<DWORD>(data.size()),0)&&CryptGetHashParam(hash,HP_HASHVAL,digest,&length,0);
 CryptDestroyHash(hash);CryptReleaseContext(provider,0);Check(ok&&length==32,"Hash result");
 char text[65]{};for(unsigned i=0;i<32;++i)std::snprintf(text+i*2,3,"%02x",digest[i]);
 Check(!std::strcmp(text,expected),"Unexpected input hash");return data;
}
struct Library {HMODULE value{};~Library(){if(value)FreeLibrary(value);}};

#include "q3_ihv_client_cases.hpp"
#include "../code/client/client_ihv_session.hpp"
#include "../code/compat/q3_ihv_native_boundary.hpp"
#include <cstdlib>
namespace client=fntech3::q3ihv::client;
namespace wire=fntech3::q3ihv::wire;
namespace cases=ihv_client_cases;
static const client::ClientData *nativeData;
static unsigned char *moduleBase;
static void __cdecl Unexpected(){Check(false,"Unexpected cgame import");}
static void __cdecl GetState(void *out){Check(out==moduleBase+0xa1104&&nativeData&&nativeData->CopyGameState(out,20100),"Native gamestate import");}
struct PrefixStop {};
static std::vector<std::unique_ptr<fntech3::q3ihv::CvarMirror>> mirrors;
static cases::Digest cvarDigest;
static unsigned cvarCalls,screenCalls,mapCalls,soundBeginCalls,worldCalls;
static int stopCvar=-1,mutableClientState=7;
static fntech3_q3_ihv_cvar_t *__cdecl PrefixCvar(const char *name,const char *value,int flags) {
 Check(name&&value&&std::strlen(name)<256&&std::strlen(value)<1024,"Cvar arguments");
 if(static_cast<int>(cvarCalls)==stopCvar)throw PrefixStop{};
 auto mirror=std::make_unique<fntech3::q3ihv::CvarMirror>();
 Check(mirror->Assign(name,value,value,nullptr,flags,1,std::strtof(value,nullptr)),"Cvar mirror");
 cvarDigest.Bytes(name,std::strlen(name)+1);cvarDigest.Bytes(value,std::strlen(value)+1);cvarDigest.Word(static_cast<unsigned>(flags));
 ++cvarCalls;auto *record=mirror->Record();mirrors.push_back(std::move(mirror));return record;
}
static void __cdecl PrefixScreen() {
 Check(mutableClientState==2,"Mutable loading state");
 Check(!std::strcmp(reinterpret_cast<const char*>(moduleBase+0xaae4c),"collision map"),"Loading status text");
 ++screenCalls;
}
static void __cdecl PrefixMap(const char *name,int clientLoad,void *checksum) {
 Check(!std::strcmp(name,"maps/synthetic.bsp")&&clientLoad==1&&checksum,"Collision map import arguments");
 Check(nativeData&&nativeData->Ready()&&mutableClientState==2,"State available during Init");
 Check(!std::memcmp(moduleBase+0xa1104,&nativeData->Configstrings().Record(),20100),"Init native gamestate copy");
 for(unsigned i=0;i<5;++i)Check(*reinterpret_cast<const int*>(moduleBase+0xaccdc+i*4)==int(i+11),"Native serverinfo parsing");
 ++mapCalls;
 if(stopCvar==-2){*static_cast<unsigned*>(checksum)=0x12345678;return;}
 throw PrefixStop{};
}
static void __cdecl PrefixSoundBegin(){Check(stopCvar==-2&&mapCalls==1,"Sound registration order");++soundBeginCalls;}
static void __cdecl PrefixWorld(const char *name){
 Check(stopCvar==-2&&mapCalls==1&&soundBeginCalls==1&&!std::strcmp(name,"maps/synthetic.bsp"),"Renderer map import and order");
 ++worldCalls;throw PrefixStop{};
}
struct PrefixEvents final:client::ClientEvents {
 void **exports{};std::array<void*,62> imports{};
 void GameState(const client::ClientData &data) override {
  nativeData=&data;reinterpret_cast<void(__cdecl*)(void*)>(exports[0])(imports.data());
  Check(false,"Init should stop at the selected registration import");
 }
 void Configstring(const client::ClientData&,std::uint32_t)override{Check(false,"Unexpected config event");}
 void ServerCommand(const client::ClientData&,std::string_view)override{Check(false,"Unexpected command event");}
 void Snapshot(const client::ClientData&,const fntech3_q3_ihv_snapshot_t&)override{Check(false,"Unexpected snapshot event");}
};
int main(int argc,char **argv) {
 if(argc<2||argc>3)return 77;
 SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);
 try {
  if(argc==3)stopCvar=std::atoi(argv[2]);
  Verified(argv[1],"175f94756f994b06235690fdd2d9fe798503f7a797f2e070277a672847593614");
  Library library;library.value=LoadLibraryExA(argv[1],nullptr,DONT_RESOLVE_DLL_REFERENCES);Check(library.value!=nullptr,"Map cgame without DLL entry point");
  moduleBase=reinterpret_cast<unsigned char*>(library.value);
  const auto getter=reinterpret_cast<void**(__cdecl*)()>(GetProcAddress(library.value,"GetCGameAPI"));Check(getter!=nullptr,"Cgame getter");
  PrefixEvents events;events.exports=getter();Check(events.exports[0]==moduleBase+0xa9a0,"Init identity");
  events.imports.fill(reinterpret_cast<void*>(&Unexpected));events.imports[0]=events.imports[57]=nullptr;
  events.imports[4]=reinterpret_cast<void*>(&PrefixCvar);events.imports[18]=reinterpret_cast<void*>(&PrefixMap);
  events.imports[37]=reinterpret_cast<void*>(640);events.imports[38]=reinterpret_cast<void*>(480);
  events.imports[32]=reinterpret_cast<void*>(&PrefixSoundBegin);events.imports[39]=reinterpret_cast<void*>(&PrefixWorld);
  events.imports[48]=reinterpret_cast<void*>(&PrefixScreen);events.imports[56]=reinterpret_cast<void*>(&GetState);events.imports[61]=&mutableClientState;
  auto session=std::make_unique<client::ClientSession>();wire::MessageWriter writer;auto game=cases::Game();
  game.configstrings[0]="\\protocol\\33\\mapname\\synthetic\\dmflags\\11\\teamflags\\12\\fraglimit\\13\\timelimit\\14\\maxclients\\15";
  Check(writer.AddGameState(game),"Prefix gamestate");bool stopped=false;
  try{session->Feed(writer.Data(),writer.Bytes(),0,events);}catch(const PrefixStop&){stopped=true;}
  Check(stopped&&!session->Data().Ready()&&!session->Data().CurrentSnapshot(),"Partial Init clears session");
  Check(stopCvar>=0?cvarCalls==static_cast<unsigned>(stopCvar)&&!mapCalls&&!screenCalls:mapCalls==1&&screenCalls==1,"Expected stop boundary");
  if(stopCvar<0)Check(cvarCalls==59&&cvarDigest.value==0x4995ca0b04c29cc7ull,"Complete observed cvar requests");
  Check(stopCvar==-2?soundBeginCalls==1&&worldCalls==1:!soundBeginCalls&&!worldCalls,"Expected registration boundary");
  std::printf("{\"scope\":\"original cgame Init prefix through ClientSession and native cvar/data adapters; stops at collision or renderer map import, with synthetic collision reply for the latter; no completed Init or game session\",\"stop_cvar\":%d,\"cvars\":%u,\"loading_callbacks\":%u,\"map_callbacks\":%u,\"sound_begin_callbacks\":%u,\"world_callbacks\":%u,\"mutable_client_state\":%d,\"failure_cleanup\":true,\"cvar_fnv1a64\":\"%016llx\"}\n",stopCvar,cvarCalls,screenCalls,mapCalls,soundBeginCalls,worldCalls,mutableClientState,static_cast<unsigned long long>(cvarDigest.value));
  nativeData=nullptr;return 0;
 }catch(const std::exception &e){nativeData=nullptr;std::fprintf(stderr,"%s\n",e.what());return 1;}
}
