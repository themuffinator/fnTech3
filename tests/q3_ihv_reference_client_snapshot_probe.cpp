/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
/* Optional original snapshot parser comparison. Baselines and packet timing
 * are synthetic, or read from the optional hash-pinned demo; the cgame callback
 * only captures records. No original entry
 * point, game, rendering, input or network session is initialized. */
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
static std::vector<char> Verified(const char *path,const char *expected) {
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
	Check(!std::strcmp(text,expected),"Unexpected input hash");
	return data;
}
static void LoadReference(const char *path) {
	const auto data=Verified(path,"e9fcaa0a5682e7f07d540aacee08cdb77f3ea40190719f5cecf31109a5135d37");
	Check(reinterpret_cast<unsigned>(GetModuleHandleA(nullptr))==0x300000,"Harness image base");
	const auto *dos=reinterpret_cast<const IMAGE_DOS_HEADER *>(data.data());
	const auto *nt=reinterpret_cast<const IMAGE_NT_HEADERS *>(data.data()+dos->e_lfanew);
	const auto *sections=IMAGE_FIRST_SECTION(nt);
	for (auto range : {std::array<unsigned,2>{0x405000,0x1000},{0x406000,0x2000},
        {0x40c000,0x1000},{0x410000,0x3000},{0x448000,0x1000},{0x45b000,0x1000}}) {
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



#include "q3_ihv_client_cases.hpp"
namespace cases=ihv_client_cases;
namespace wire=fntech3::q3ihv::wire;
using fntech3::q3ihv::client::ClientData;
struct Message { int allow,overflow;void *data;int capacity,size,read,bit; };
static_assert(sizeof(Message)==28);
static fntech3_q3_ihv_snapshot_t captured;
static unsigned calls;
static void __cdecl Error(int,const char*,...) { Check(false,"Original snapshot parser failure"); }
static void __cdecl Print(const char*,...) {}
static void __cdecl Show(Message*,const char*) {}
static void __cdecl Snapshot(const void *data) { std::memcpy(&captured,data,sizeof captured);++calls; }
static void Supplied(const char *path) {
 const auto bytes=Verified(path,"fbdc209d1c7fc1075976c9120c79d5e579ed36f4e38062b01248d6b2d807c57b");
 wire::DemoReader reader(bytes.data(),bytes.size());wire::DemoRecord record;wire::DemoRecordResult status;
 wire::MessageDecoder decoder;ClientData data;cases::Digest digest;unsigned messages=0,frames=0,entities=0;
 const auto parse=reinterpret_cast<void(__cdecl*)(Message*)>(0x406a30);
 const auto readBits=reinterpret_cast<unsigned(__cdecl*)(Message*,int)>(0x410be0);
 const auto getEntity=reinterpret_cast<int(__cdecl*)(int,void*)>(0x405dd0);
 while((status=reader.Next(record))==wire::DemoRecordResult::Message) {
  wire::DecodedMessage decoded;Check(decoder.Decode(record.data,record.size,decoded)==wire::MessageResult::Ok,"Supplied decode");++messages;
  Check(decoded.events.size()==1,"Supplied single event per packet");
  if(decoded.gamestate) {
   // Initialize only parser storage/baselines, without cgame Init or map loading.
   std::memset(reinterpret_cast<void*>(0x430fc60),0,0x16804*4);
   std::memcpy(reinterpret_cast<void*>(0x43177cc),decoded.gamestate->baselines.data(),sizeof(decoded.gamestate->baselines));
   Check(data.BeginGameState(decoded.gamestate->nativeConfigstrings),"Supplied gamestate");continue;
  }
  Check(bool(decoded.snapshot),"Supplied snapshot event");
  *reinterpret_cast<std::uint32_t*>(0x430fc10)=250000;
  *reinterpret_cast<std::uint32_t*>(0x43039fc)=1;
  *reinterpret_cast<std::uint32_t*>(0x4369bdc)=decoded.snapshot->commandSequence;
  *reinterpret_cast<std::uint32_t*>(0x4369b5c)=250000; // deterministic zero ping
  Message message{0,0,const_cast<std::uint8_t*>(record.data),static_cast<int>(record.size),static_cast<int>(record.size),0,0};
  Check(readBits(&message,8)==7,"Supplied snapshot opcode");calls=0;parse(&message);
  Check(calls==1&&!message.overflow,"Supplied original callback");
  const auto bits=message.read*8-(message.bit?8-message.bit:0);
  Check(bits>=0&&static_cast<std::size_t>((bits+7)/8)==record.size,"Supplied consumed extent");
  Check(data.PublishSnapshot(*decoded.snapshot,0),"Supplied publication");
  Check(!std::memcmp(&captured,data.CurrentSnapshot(),sizeof captured),"Supplied original native snapshot");
  digest.Bytes(&captured,sizeof captured);++frames;
  for(std::size_t n=0;n<decoded.snapshot->entities.size();++n) {
   std::array<std::uint8_t,180> original,modern;original.fill(0xa5);modern=original;
   const auto sequence=static_cast<std::uint32_t>(decoded.snapshot->firstEntity+n);
   Check(getEntity(static_cast<int>(sequence),original.data()+8)==1,"Supplied original entity");
   Check(data.CopyEntity(sequence,modern.data()+8,164)==fntech3::q3ihv::client::ReadResult::Ok&&modern==original,"Supplied entity and guards");
   digest.Bytes(original.data()+8,164);++entities;
  }
 }
 Check(status==wire::DemoRecordResult::End&&messages==57&&frames==56&&entities==680,"Supplied demo complete");
 std::printf("{\"scope\":\"hash-pinned sli.dm3 snapshot packets through original parser; decoded baselines, zero ping, capture callback, no Init or engine playback\",\"messages\":%u,\"snapshots\":%u,\"entities\":%u,\"fnv1a64\":\"%016llx\"}\n",messages,frames,entities,static_cast<unsigned long long>(digest.value));
}
int main(int argc,char **argv) {
 if(argc<2||argc>3)return 77;
 SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);
 try {
  LoadReference(argv[1]);unsigned precision{};Check(!_controlfp_s(&precision,_PC_53|_RC_NEAR,_MCW_PC|_MCW_RC),"FPU control");
  Hook(0x40cca0,&Error);Hook(0x40cae0,&Print);Hook(0x4066f0,&Show);
  const auto parse=reinterpret_cast<void(__cdecl*)(Message*)>(0x406a30);
  const auto readBits=reinterpret_cast<unsigned(__cdecl*)(Message*,int)>(0x410be0);
  const auto getEntity=reinterpret_cast<int(__cdecl*)(int,void*)>(0x405dd0);
  std::memset(reinterpret_cast<void*>(0x430fc60),0,0x16804*4);
  fntech3_q3_ihv_cvar_t quiet{};*reinterpret_cast<fntech3_q3_ihv_cvar_t**>(0x430fc20)=&quiet;
  std::array<void*,8> exports{};exports[1]=reinterpret_cast<void*>(&Snapshot);*reinterpret_cast<void***>(0x43039a0)=exports.data();
  ClientData data;wire::MessageDecoder decoder;wire::MessageWriter initial;
  Check(initial.AddGameState(cases::Game()),"Initial game encode");wire::DecodedMessage gameMessage;
  Check(decoder.Decode(initial.Data(),initial.Bytes(),gameMessage)==wire::MessageResult::Ok,"Initial game decode");
  const auto game=decoder.CurrentGameState();Check(data.BeginGameState(game->nativeConfigstrings),"Native gamestate");
  std::memcpy(reinterpret_cast<void*>(0x43177cc),game->baselines.data(),sizeof(game->baselines));
  cases::Digest digest;std::shared_ptr<const wire::Snapshot> previous;unsigned entities=0;
  for(unsigned index=0;index<cases::Frames;++index) {
   auto frame=cases::Frame(index,previous.get());wire::MessageWriter writer;std::size_t prefixBits=0;
   if(!index) { Check(writer.AddGameState(cases::Game()),"Embedded gamestate");prefixBits=writer.Bits(); }
   const auto base=frame.deltaNumber&&!(frame.deltaNumber&0x80000000u)?previous.get():nullptr;
   Check(writer.AddSnapshot(*game,base,frame),"Snapshot encode");wire::DecodedMessage decoded;
   Check(decoder.Decode(writer.Data(),writer.Bytes(),decoded)==wire::MessageResult::Ok,"Snapshot decode");
   const auto ping=cases::Ping(index);
   *reinterpret_cast<std::uint32_t*>(0x430fc10)=250000;
   *reinterpret_cast<std::uint32_t*>(0x43039fc)=1;
   *reinterpret_cast<std::uint32_t*>(0x4369bdc)=decoded.snapshot->commandSequence;
   *reinterpret_cast<std::uint32_t*>(0x4369b5c)=250000-ping;
   Message message{0,0,const_cast<std::uint8_t*>(writer.Data()),static_cast<int>(writer.Bytes()),static_cast<int>(writer.Bytes()),0,0};
   while(prefixBits) {const unsigned n=static_cast<unsigned>(prefixBits>32?32:prefixBits);readBits(&message,n);prefixBits-=n;}
   Check(readBits(&message,8)==7,"Snapshot opcode");calls=0;parse(&message);
   Check(calls==1&&!message.overflow,"Original snapshot callback");
   const auto bits=message.read*8-(message.bit?8-message.bit:0);
   Check(static_cast<std::size_t>(bits)==writer.Bits(),"Original complete bit extent");
   Check(data.PublishSnapshot(*decoded.snapshot,ping),"Native snapshot publication");
   if(std::memcmp(&captured,data.CurrentSnapshot(),sizeof captured)) {
    for(unsigned offset=0;offset<sizeof captured;++offset)if(captured.bytes[offset]!=data.CurrentSnapshot()->bytes[offset]) {
     std::fprintf(stderr,"Frame %u byte %u: original %02x, adapter %02x\n",index,offset,captured.bytes[offset],data.CurrentSnapshot()->bytes[offset]);break;
    }
    Check(false,"Native snapshot differs");
   }
   digest.Word(index);digest.Bytes(&captured,sizeof captured);
   for(std::size_t n=0;n<decoded.snapshot->entities.size();++n) {
    std::array<std::uint8_t,180> original,modern;original.fill(0xa5);modern=original;
    const auto number=static_cast<std::uint32_t>(decoded.snapshot->firstEntity+n);
    Check(getEntity(static_cast<int>(number),original.data()+8)==1,"Original entity history");
    Check(data.CopyEntity(number,modern.data()+8,164)==fntech3::q3ihv::client::ReadResult::Ok&&modern==original,"Native entity history and guards");
    digest.Bytes(original.data()+8,164);++entities;
   }
   previous=decoded.snapshot;
  }
  std::printf("{\"scope\":\"original snapshot parser, player/entity deltas and history getters compared with native client adapter; synthetic packets/baselines/timing and capture callback, no engine session\",\"frames\":%u,\"entities\":%u,\"snapshot_bytes\":348,\"fnv1a64\":\"%016llx\"}\n",cases::Frames,entities,static_cast<unsigned long long>(digest.value));
  if(argc>2)Supplied(argv[2]);return 0;
 }catch(const std::exception &e){std::fprintf(stderr,"%s\n",e.what());return 1;}
}
