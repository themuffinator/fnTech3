/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
/* Optional protocol packet -> client session -> original cgame data callback
 * probe. Cgame Init is not called; the packet-graph helper is stubbed, registration imports
 * are synthetic, and server command text is recorded only. No game session. */
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

#include "q3_ihv_client_cases.hpp"
#include "../code/client/client_ihv_session.hpp"
namespace client=fntech3::q3ihv::client;
namespace wire=fntech3::q3ihv::wire;
namespace cases=ihv_client_cases;
static const client::ClientData *nativeData;
static unsigned char *moduleBase;
static unsigned models,sounds,music;
static bool failModel;
static void __cdecl Unexpected(){Check(false,"Unexpected cgame import");}
static void __cdecl GetState(void *out){Check(out==moduleBase+0xa1104&&nativeData&&nativeData->CopyGameState(out,20100),"Native gamestate import");}
static int __cdecl GetCommand(int number,void *out){Check(nativeData!=nullptr,"Command context");return nativeData->CopyCommand(static_cast<unsigned>(number),out,14)==client::ReadResult::Ok;}
static unsigned __cdecl CurrentCommand(){Check(nativeData!=nullptr,"Command counter context");return nativeData->CurrentCommandNumber();}
static int __cdecl GetEntity(int number,void *out){
 Check(nativeData!=nullptr,"Entity context");const auto result=nativeData->CopyEntity(static_cast<unsigned>(number),out,164);
 Check(result!=client::ReadResult::FutureEntity&&result!=client::ReadResult::InvalidOutput,"Invalid native entity request");return result==client::ReadResult::Ok;
}
static void Text(const char *value){
 const auto address=reinterpret_cast<std::uintptr_t>(value),first=reinterpret_cast<std::uintptr_t>(moduleBase+0xa2104);
 Check(address>=first&&address-first<16000,"Registered string pointer");
 Check(std::memchr(value,0,16000-(address-first))!=nullptr,"Registered string extent");
}
static int __cdecl Model(const char *value){Text(value);if(failModel)throw std::runtime_error("model callback failure");++models;return 17;}
static int __cdecl Sound(const char *value){Text(value);++sounds;return 23;}
static void __cdecl Music(const char *value){Text(value);++music;}
struct Events final:client::ClientEvents {
 void **exports{};unsigned games=0,configs=0,commands=0,snapshots=0;cases::Digest digest;
 bool checkOrder=true;
 void GameState(const client::ClientData &data) override {
  nativeData=&data;++games;
  *reinterpret_cast<unsigned*>(moduleBase+0xa8b0c)=0;*reinterpret_cast<unsigned*>(moduleBase+0xa8b10)=0;
  *reinterpret_cast<unsigned*>(moduleBase+0xa1100)=1;std::memset(moduleBase+0xa5f8c,0xa5,32*348);
  // Prime only the data copy via a configstring with no registration side effect.
  reinterpret_cast<void(__cdecl*)(int)>(exports[2])(1);
 }
 void Configstring(const client::ClientData &data,std::uint32_t index) override {
  Check(nativeData==&data,"Stable native import context");++configs;
  reinterpret_cast<void(__cdecl*)(int)>(exports[2])(static_cast<int>(index));
  Check(!std::memcmp(moduleBase+0xa1104,&data.Configstrings().Record(),20100),"Cgame configstring record");
 }
 void ServerCommand(const client::ClientData &data,std::string_view text) override {
  Check(nativeData==&data&&!text.empty(),"Recorded server command");++commands;
 }
 void Snapshot(const client::ClientData &data,const fntech3_q3_ihv_snapshot_t &snapshot) override {
  Check(nativeData==&data&&(!checkOrder||data.Configstrings().String(9).substr(0,6)=="before"),"Snapshot event order");
  const unsigned sequence=*reinterpret_cast<unsigned*>(moduleBase+0xa8b0c);
  std::array<unsigned char,32*348> expected;std::memcpy(expected.data(),moduleBase+0xa5f8c,expected.size());
  std::memcpy(expected.data()+(sequence&31)*348,&snapshot,348);
  reinterpret_cast<void(__cdecl*)(const void*)>(exports[1])(&snapshot);
  Check(!std::memcmp(expected.data(),moduleBase+0xa5f8c,expected.size()),"Cgame snapshot ring");
  Check(*reinterpret_cast<unsigned*>(moduleBase+0xa8b0c)==sequence+1&&*reinterpret_cast<unsigned*>(moduleBase+0xa8b10)==1,"Cgame snapshot counters");
  Check(!*reinterpret_cast<unsigned*>(moduleBase+0xa1100),"Cgame first-snapshot flag");
  digest.Word(snapshots++);digest.Bytes(moduleBase+0xa5f8c,expected.size());
 }
};
static void Supplied(const char *path,bool valid,void **exports) {
 const auto bytes=Verified(path,valid?"fbdc209d1c7fc1075976c9120c79d5e579ed36f4e38062b01248d6b2d807c57b":"d2ee63df0221c380f0233fac458445217016c3afa7986f300fed2405de61ec78");
 wire::DemoReader reader(bytes.data(),bytes.size());wire::DemoRecord record;wire::DemoRecordResult status;
 auto session=std::make_unique<client::ClientSession>();Events events;events.exports=exports;events.checkOrder=false;
 unsigned messages=0,entities=0;const auto modelCount=models,soundCount=sounds,musicCount=music;
 while((status=reader.Next(record))==wire::DemoRecordResult::Message) {
  const auto snapshots=events.snapshots;
  const auto result=session->Feed(record.data,record.size,0,events);
  if(!valid) {
   Check(result.code==client::PacketResult::Rejected&&result.message==wire::MessageResult::MissingGamestate,"Supplied incomplete rejection");
   Check(!events.games&&!events.snapshots&&!events.configs&&!events.commands&&!session->Data().Ready(),"Supplied incomplete no callbacks");
   std::puts("{\"scope\":\"hash-pinned berj.dm3 through ClientSession\",\"missing_gamestate\":true,\"callbacks\":0}");nativeData=nullptr;return;
  }
  Check(result.code==client::PacketResult::Delivered,"Supplied native session delivery");++messages;
  if(events.snapshots!=snapshots) {
   const auto *snapshot=session->Data().CurrentSnapshot();Check(snapshot!=nullptr,"Supplied current snapshot");
   const auto first=wire::Load(snapshot->bytes+344,4),count=wire::Load(snapshot->bytes+340,4);
   for(std::uint32_t n=0;n<count;++n) {
    std::array<unsigned char,180> original,modern;original.fill(0xa5);modern=original;
    Check(GetEntity(static_cast<int>(first+n),original.data()+8)==1,"Supplied native entity import");
    Check(session->Data().CopyEntity(first+n,modern.data()+8,164)==client::ReadResult::Ok&&original==modern,"Supplied native entity guards");++entities;
   }
  }
 }
 Check(valid&&status==wire::DemoRecordResult::End&&messages==57&&events.games==1&&events.snapshots==56&&entities==680,"Supplied native replay complete");
 Check(models==modelCount&&sounds==soundCount&&music==musicCount,"Supplied data-only callbacks");
 std::printf("{\"scope\":\"hash-pinned sli.dm3 through ClientSession and original cgame data callbacks; no Init, prediction, asset registration or rendered playback\",\"messages\":%u,\"gamestates\":%u,\"snapshots\":%u,\"entity_reads\":%u,\"fnv1a64\":\"%016llx\"}\n",messages,events.games,events.snapshots,entities,static_cast<unsigned long long>(events.digest.value));
 nativeData=nullptr;
}
int main(int argc,char **argv){
 if(argc<2||argc>4)return 77;
 SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);
 try {
  Verified(argv[1],"175f94756f994b06235690fdd2d9fe798503f7a797f2e070277a672847593614");Library library;library.value=LoadLibraryExA(argv[1],nullptr,DONT_RESOLVE_DLL_REFERENCES);Check(library.value!=nullptr,"Map cgame without entry point");
  moduleBase=reinterpret_cast<unsigned char*>(library.value);
  const auto getter=reinterpret_cast<void**(__cdecl*)()>(GetProcAddress(library.value,"GetCGameAPI"));Check(getter!=nullptr,"Getter");
  Events events;events.exports=getter();Check(events.exports==reinterpret_cast<void**>(moduleBase+0x2b020),"Export table");
  Check(events.exports[1]==moduleBase+0xaf30&&events.exports[2]==moduleBase+0xa820,"Data callback identities");
  PacketGraphStub stub(moduleBase+0x4b90);
  auto **imports=reinterpret_cast<void**>(moduleBase+0xa0f80);for(unsigned i=0;i<62;++i)imports[i]=reinterpret_cast<void*>(&Unexpected);
  imports[56]=reinterpret_cast<void*>(&GetState);imports[58]=reinterpret_cast<void*>(&GetEntity);
  imports[59]=reinterpret_cast<void*>(&CurrentCommand);imports[60]=reinterpret_cast<void*>(&GetCommand);
  imports[40]=reinterpret_cast<void*>(&Model);imports[33]=reinterpret_cast<void*>(&Sound);imports[36]=reinterpret_cast<void*>(&Music);
  auto session=std::make_unique<client::ClientSession>();wire::MessageDecoder observer;wire::DecodedMessage decoded;
  wire::MessageWriter initial;Check(initial.AddGameState(cases::Game()),"Initial encode");
  Check(observer.Decode(initial.Data(),initial.Bytes(),decoded)==wire::MessageResult::Ok,"Initial observer");
  Check(session->Feed(initial.Data(),initial.Bytes(),0,events).code==client::PacketResult::Delivered,"Initial packet");
  const auto game=observer.CurrentGameState();
  fntech3_q3_ihv_usercmd_t command{};for(unsigned i=0;i<14;++i)command.bytes[i]=static_cast<unsigned char>(i*31);
  Check(session->AppendCommand(command)&&CurrentCommand()==1,"Command publication");
  fntech3_q3_ihv_usercmd_t commandCopy{};Check(GetCommand(1,&commandCopy)==1&&!std::memcmp(&command,&commandCopy,14),"Command import");
  wire::MessageWriter registration;Check(registration.AddConfigstring(32,"synthetic/model")&&registration.AddConfigstring(288,"synthetic/sound")&&registration.AddConfigstring(2,"synthetic/music"),"Registration packet");
  Check(session->Feed(registration.Data(),registration.Bytes(),0,events).code==client::PacketResult::Delivered,"Registration callbacks");
  Check(models==1&&sounds==1&&music==1,"Registration routing");
  std::shared_ptr<const wire::Snapshot> previous;unsigned entityReads=0;
  for(unsigned index=0;index<cases::Frames;++index){
   wire::MessageWriter writer;auto frame=cases::Frame(index,previous.get());const auto base=frame.deltaNumber&&!(frame.deltaNumber&0x80000000u)?previous.get():nullptr;
   Check(writer.AddConfigstring(9,"before"+std::to_string(index))&&writer.AddServerCommand("record only"),"Before snapshot events");
   Check(writer.AddSnapshot(*game,base,frame)&&writer.AddConfigstring(9,"after"+std::to_string(index)),"Snapshot events");
   Check(observer.Decode(writer.Data(),writer.Bytes(),decoded)==wire::MessageResult::Ok,"Observer snapshot");
   Check(session->Feed(writer.Data(),writer.Bytes(),cases::Ping(index),events).code==client::PacketResult::Delivered,"Staged native packet");
   Check(session->Data().Configstrings().String(9)=="after"+std::to_string(index),"Later update delivery");
   for(std::size_t n=0;n<decoded.snapshot->entities.size();++n){
    wire::Entity entity{};Check(GetEntity(static_cast<int>(decoded.snapshot->firstEntity+n),entity.data())==1&&entity==decoded.snapshot->entities[n],"Native entity import");++entityReads;
   }
   previous=decoded.snapshot;
  }
  std::array<unsigned char,32*348> ringBefore;std::memcpy(ringBefore.data(),moduleBase+0xa5f8c,ringBefore.size());
  const auto stateBefore=session->Data().Configstrings().Record();const auto configsBefore=events.configs;
  wire::MessageWriter bad;Check(bad.AddConfigstring(32,"must not register")&&bad.AddServerCommand("must not deliver"),"Malformed prefix");
  std::vector<unsigned char> packet(bad.Data(),bad.Data()+bad.Bytes());packet.push_back(0);
  Check(session->Feed(packet.data(),packet.size(),0,events).code==client::PacketResult::Rejected,"Malformed packet rejection");
  Check(models==1&&events.configs==configsBefore&&!std::memcmp(&stateBefore,moduleBase+0xa1104,20100)&&!std::memcmp(ringBefore.data(),moduleBase+0xa5f8c,ringBefore.size()),"Rejected packet leaves cgame untouched");
  failModel=true;bool failed=false;try{session->Feed(bad.Data(),bad.Bytes(),0,events);}catch(const std::runtime_error &e){Check(!std::strcmp(e.what(),"model callback failure"),"Expected module failure");failed=true;}
  Check(failed&&!session->Data().Ready()&&!session->Data().CurrentSnapshot(),"Native callback failure invalidates session");
  std::printf("{\"scope\":\"protocol packets through ClientSession to original cgame data callbacks; no Init, prediction, real asset/audio registration or game session\",\"gamestates\":%u,\"snapshots\":%u,\"entity_reads\":%u,\"recorded_commands\":%u,\"models\":%u,\"sounds\":%u,\"music\":%u,\"malformed_packet_no_callbacks\":true,\"native_callback_failure_cleanup\":true,\"fnv1a64\":\"%016llx\"}\n",events.games,events.snapshots,entityReads,events.commands,models,sounds,music,static_cast<unsigned long long>(events.digest.value));
  nativeData=nullptr;failModel=false;
  if(argc>2)Supplied(argv[2],true,events.exports);if(argc>3)Supplied(argv[3],false,events.exports);return 0;
 }catch(const std::exception &e){nativeData=nullptr;std::fprintf(stderr,"%s\n",e.what());return 1;}
}
