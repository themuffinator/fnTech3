/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
/* Optional Win32 comparison of fnTech3's native IHV client records against the
 * original bot imports, bot scheduler, userinfo update and snapshot getters.
 * Only the observed code pages are mapped: no original entry point, filesystem,
 * display, input, audio or network service is initialized, and no original
 * module or asset is incorporated. The host base is 0x400000 so the harness
 * owns the original's address range as zeroed storage. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wincrypt.h>
#include <float.h>
#include "../code/compat/q3_ihv_native_boundary.hpp"
#include "../code/compat/q3_ihv_server_clients.hpp"
#include "q3_ihv_collision_cases.hpp"
#include <array>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

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
	char text[65]{}; for (int i=0;i<32;++i) std::snprintf(text+i*2,3,"%02x",digest[i]);
	Check(!std::strcmp(text,"e9fcaa0a5682e7f07d540aacee08cdb77f3ea40190719f5cecf31109a5135d37"),"Unexpected original executable");
	Check(reinterpret_cast<unsigned>(GetModuleHandleA(nullptr))==0x400000,"Harness image base");
	const auto *dos=reinterpret_cast<const IMAGE_DOS_HEADER *>(data.data());
	const auto *nt=reinterpret_cast<const IMAGE_NT_HEADERS *>(data.data()+dos->e_lfanew);
	const auto *sections=IMAGE_FIRST_SECTION(nt);
	/* Code: message buffers, info parsing, the bot imports and scheduler, the
	 * snapshot getters and the shared string/number helpers. Read-only data
	 * holds the float constants; initialized data holds the userinfo key names
	 * and the runtime tables the engine's own update reads, and must stay
	 * writable because its section also covers the original's variables. */
	for (auto range : {std::array<unsigned,3>{0x410000,0x1000,1},{0x415000,0x1000,1},{0x417000,0x3000,1},
		{0x41b000,0x1000,1},{0x448000,0x2000,1},{0x45b000,0x3000,0},{0x45e000,0x12000,0}}) {
		const unsigned address=range[0],length=range[1],first=reinterpret_cast<unsigned>(observation);
		Check(address>=first && address-first<=sizeof(observation)-length,"Original range overlaps harness");
		for (unsigned i=0;i<nt->FileHeader.NumberOfSections;++i) {
			const unsigned start=0x400000+sections[i].VirtualAddress,end=start+sections[i].SizeOfRawData;
			const unsigned from=address>start?address:start,to=address+length<end?address+length:end;
			if (from>=to) continue;
			const std::size_t offset=sections[i].PointerToRawData+from-start;
			Check(offset<=data.size() && to-from<=data.size()-offset,"Section bounds");
			std::memcpy(reinterpret_cast<void *>(from),data.data()+offset,to-from);
		}
		if (!range[2]) continue;
		DWORD old{}; Check(VirtualProtect(reinterpret_cast<void *>(address),length,PAGE_EXECUTE_READ,&old),"Code protection");
		Check(FlushInstructionCache(GetCurrentProcess(),reinterpret_cast<void *>(address),length),"Instruction cache");
	}
}

namespace {
constexpr unsigned ClientStride=0xee18,EntityStride=556,StateBytes=FNTECH3_Q3_IHV_ENTITY_STATE_BYTES;
constexpr unsigned StateOffset=0,UserinfoOffset=4,EntityOffset=0x410,NameOffset=0x414;
constexpr unsigned PacketTimeOffset=0x444,SequenceOffset=0x2e18,RateOffset=0x2ddc,SnapsOffset=0x2de0;
constexpr unsigned FrameCountOffset=0x590,FrameFirstOffset=0x594,FrameStride=0x14c;

/* Observed callbacks, recorded in call order for both implementations. */
struct Callback { int kind,entity,msec; std::string text; };
std::vector<Callback> reference,replica;
fntech3_q3_ihv_entity_t *entityBase;
int CallbackEntity(const void *entity) {
	const auto offset=reinterpret_cast<const unsigned char *>(entity)-reinterpret_cast<unsigned char *>(entityBase);
	Check(offset>=0 && offset%EntityStride==0,"Callback entity outside the array");
	return static_cast<int>(offset/EntityStride);
}
void __cdecl UserinfoChanged(fntech3_q3_ihv_entity_t *entity,char *text) {
	reference.push_back({0,CallbackEntity(entity),0,text?text:""});
}
void __cdecl BotBegin(fntech3_q3_ihv_entity_t *entity) { reference.push_back({1,CallbackEntity(entity),0,{}}); }
void __cdecl BotThink(fntech3_q3_ihv_entity_t *entity,int msec) { reference.push_back({2,CallbackEntity(entity),msec,{}}); }

template<typename T> void Put(void *base,unsigned offset,T value) {
	std::memcpy(static_cast<char *>(base)+offset,&value,sizeof value);
}
template<typename T> T Get(const void *base,unsigned offset) {
	T value; std::memcpy(&value,static_cast<const char *>(base)+offset,sizeof value); return value;
}
} // namespace

int main(int argc,char **argv) {
	if (argc!=2) return 77;
	try {
		LoadReference(argv[1]);
		unsigned previous{}; Check(!_controlfp_s(&previous,_PC_53|_RC_NEAR,_MCW_PC|_MCW_RC),"FPU control");
		constexpr int ServerTime=12345,RingCapacity=64,Entities=16;
		std::vector<unsigned> clients(8*ClientStride/4),entities(Entities*EntityStride/4),ring(RingCapacity*StateBytes/4);
		fntech3_q3_ihv_game_export_t game{}; fntech3_q3_ihv_cvar_t maxclients{};
		entityBase=reinterpret_cast<fntech3_q3_ihv_entity_t *>(entities.data());
		game.entities=entityBase; game.entityStride=EntityStride;
		game.entityCount=Entities; game.entityCapacity=Entities;
		game.ClientUserinfoChanged=UserinfoChanged; game.BotBegin=BotBegin; game.BotThink=BotThink;
		*reinterpret_cast<void **>(0x424de24)=&maxclients;
		*reinterpret_cast<void **>(0x424de44)=clients.data();
		*reinterpret_cast<int *>(0x424de48)=RingCapacity;
		*reinterpret_cast<void **>(0x424de50)=ring.data();
		*reinterpret_cast<void **>(0x4254e74)=&game;
		*reinterpret_cast<int *>(0x4254e8c)=ServerTime;
		const auto allocate=reinterpret_cast<fntech3_q3_ihv_entity_t *(__cdecl *)()>(0x417640);
		const auto release=reinterpret_cast<void (__cdecl *)(fntech3_q3_ihv_entity_t *)>(0x4176e0);
		const auto setUserinfo=reinterpret_cast<void (__cdecl *)(fntech3_q3_ihv_entity_t *,const char *)>(0x417720);
		const auto botFrame=reinterpret_cast<void (__cdecl *)(int)>(0x4177e0);
		const auto snapshotCount=reinterpret_cast<int (__cdecl *)(fntech3_q3_ihv_entity_t *)>(0x41b370);
		const auto snapshotNumber=reinterpret_cast<int (__cdecl *)(fntech3_q3_ihv_entity_t *,int)>(0x41b3b0);
		const auto valueForKey=reinterpret_cast<char *(__cdecl *)(const char *,const char *)>(0x415020);

		auto Client=[&](int slot) { return reinterpret_cast<unsigned char *>(clients.data())+slot*ClientStride; };
		auto Entity=[&](int index) {
			return reinterpret_cast<fntech3_q3_ihv_entity_t *>(reinterpret_cast<unsigned char *>(entities.data())+index*EntityStride);
		};
		/* fnTech3's adapter maps slot to entity slot+1 and republishes the
		 * number, exactly as BotAllocateClient does. */
		fntech3::q3ihv::NativeClients records;
		auto Allocate=[&](int &slot) {
			slot=records.FirstFreeSlot();
			if (slot<0) return static_cast<fntech3_q3_ihv_entity_t *>(nullptr);
			auto *entity=Entity(slot+1);
			Check(fntech3::q3ihv::SetEntityNumber(*entity,slot+1) && records.Connect(slot,ServerTime),"Replica allocation");
			return entity;
		};
		auto StepFrame=[&](int msec) {
			for (int slot=0;slot<records.Count();++slot) {
				if (!records.HasEntity(slot)) continue;
				auto *entity=Entity(slot+1);
				if (!(entity->svFlags&FNTECH3_Q3_IHV_SVF_BOT)) continue;
				const auto step=records.StepBot(slot,ServerTime);
				if (step==fntech3::q3ihv::NativeClients::BotStep::Begin) replica.push_back({1,slot+1,0,{}});
				else if (step==fntech3::q3ihv::NativeClients::BotStep::Think) replica.push_back({2,slot+1,msec,{}});
			}
		};

		unsigned allocations=0,frameSteps=0,userinfoCases=0,snapshotCases=0;
		ihv_collision_cases::Digest digest;
		for (int slots : {1,2,8}) {
			std::memset(clients.data(),0,clients.size()*4);
			std::memset(entities.data(),0,entities.size()*4);
			maxclients.value=static_cast<float>(slots);
			Check(records.Reset(slots,Entities),"Replica client array");
			reference.clear(); replica.clear();
			for (int slot=0;slot<slots;++slot) {
				auto *original=allocate();
				int mine=-1; auto *copy=Allocate(mine);
				Check(original==Entity(slot+1) && copy==original && mine==slot,"Allocated slot and entity");
				const auto *client=Client(slot);
				Check(Get<short>(original,StateOffset)==slot+1,"Published entity number");
				Check(Get<void *>(client,EntityOffset)==original,"Published entity pointer");
				Check(Get<int>(client,0)==records.State(mine),"Connected state");
				Check(Get<int>(client,PacketTimeOffset)==records.LastPacketTime(mine),"Connected packet time");
				Check(Get<void *>(client,0x6e20)==client+0x6e34 && Get<int>(client,0x6e24)==0x3ff0,"Native message buffer");
				Check(records.MessageCleared(mine) && records.HasEntity(mine),"Replica message and association");
				digest.Word(records.State(mine)); digest.Word(records.LastPacketTime(mine)); ++allocations;
			}
			int full=-1;
			Check(!allocate() && !Allocate(full) && full<0,"Full native client array");
			for (int slot=0;slot<slots;++slot) Entity(slot+1)->svFlags=FNTECH3_Q3_IHV_SVF_BOT;
			for (int msec : {50,25,33}) {
				botFrame(msec); StepFrame(msec); frameSteps+=static_cast<unsigned>(slots);
				for (int slot=0;slot<slots;++slot)
					Check(Get<int>(Client(slot),0)==records.State(slot)
						&& Get<int>(Client(slot),PacketTimeOffset)==records.LastPacketTime(slot),"Stepped client record");
			}
			for (int slot=0;slot<slots;++slot) {
				Put(Client(slot),NameOffset,'X');
				release(Entity(slot+1));
				Check(records.Release(slot),"Replica release");
				Check(Get<int>(Client(slot),0)==records.State(slot),"Released state");
				Check(!Get<char>(Client(slot),NameOffset) && !records.Name(slot)[0],"Released name");
				Check(Get<void *>(Client(slot),EntityOffset)==Entity(slot+1) && records.HasEntity(slot),"Released association");
				Entity(slot+1)->svFlags=0;
			}
			int zombie=-1;
			Check(!allocate() && !Allocate(zombie) && zombie<0,"Zombie is not a free slot");
			Check(reference.size()==replica.size(),"Callback count");
			for (std::size_t i=0;i<reference.size();++i) {
				Check(reference[i].kind==replica[i].kind && reference[i].entity==replica[i].entity
					&& reference[i].msec==replica[i].msec,"Callback order and arguments");
				digest.Word(replica[i].kind); digest.Word(replica[i].entity); digest.Word(replica[i].msec);
			}
		}

		/* Userinfo copy limits, the module callback's buffer and the engine's
		 * own name, rate and snaps update. */
		std::memset(clients.data(),0,clients.size()*4);
		std::memset(entities.data(),0,entities.size()*4);
		maxclients.value=8; Check(records.Reset(8,Entities),"Replica client array");
		for (int slot=0;slot<8;++slot) {
			auto *original=allocate();
			int mine=-1; auto *copy=Allocate(mine);
			Check(original==copy && mine==slot,"Userinfo host allocation");
		}
		std::vector<std::string> cases{
			R"(\name\Bot\rate\25000\snaps\20)",R"(\name\Bot\rate\50\snaps\0)",
			R"(\rate\\snaps\)",R"(\name\\rate\-9\snaps\-4)",R"(\snaps\31\rate\15001)",
			R"(\snaps\30\rate\100)",R"(\name\0123456789012345678901234567890123456789)",
			R"(\rate\abc\snaps\xyz)",R"(\name\Bot\team\red)",std::string{}};
		cases.push_back(R"(\name\)"+std::string(1200,'a'));
		cases.push_back(R"(\rate\)"+std::string(1100,'7'));
		for (std::size_t i=0;i<cases.size();++i) {
			const int slot=static_cast<int>(i%8);
			auto *entity=Entity(slot+1);
			auto *client=Client(slot);
			reference.clear();
			setUserinfo(entity,cases[i].c_str());
			Check(records.CopyUserinfo(slot,cases[i].c_str()),"Replica userinfo copy");
			char name[1024],rate[1024],snaps[1024];
			auto *stored=records.MutableUserinfo(slot);
			Check(stored && !std::strcmp(reinterpret_cast<const char *>(client)+UserinfoOffset,stored),"Stored userinfo copy");
			Check(reference.size()==1 && reference[0].kind==0 && reference[0].entity==slot+1
				&& reference[0].text==stored,"Userinfo callback");
			std::snprintf(name,sizeof name,"%s",valueForKey(stored,"name"));
			std::snprintf(rate,sizeof rate,"%s",valueForKey(stored,"rate"));
			std::snprintf(snaps,sizeof snaps,"%s",valueForKey(stored,"snaps"));
			Check(records.ApplyUserinfo(slot,name,rate,snaps),"Replica userinfo update");
			/* The original's name update is strncpy with a 31-byte limit, so it
			 * pads with NULs and leaves a full-length name unterminated. */
			char expected[31]{};
			for (int c=0;c<31 && records.Name(slot)[c];++c) expected[c]=records.Name(slot)[c];
			Check(!std::memcmp(reinterpret_cast<const char *>(client)+NameOffset,expected,sizeof expected),"Client name");
			Check(Get<int>(client,RateOffset)==records.Rate(slot),"Client rate");
			Check(Get<int>(client,SnapsOffset)==records.SnapshotMsec(slot),"Client snapshot interval");
			digest.Word(records.Rate(slot)); digest.Word(records.SnapshotMsec(slot));
			for (const char *c=records.Name(slot);*c;++c) digest.Word(static_cast<unsigned char>(*c));
			++userinfoCases;
		}

		/* Snapshot history and ring addressing. */
		Check(records.BindSnapshotRing(RingCapacity),"Replica snapshot ring");
		for (int i=0;i<RingCapacity;++i) {
			const short number=static_cast<short>(i*37-80);
			Put(ring.data(),i*StateBytes,number);
			fntech3_q3_ihv_entityState_t state{};
			std::memcpy(state.bytes,&number,sizeof number);
			Check(records.SetSnapshotState(i,state),"Replica ring state");
		}
		for (int slot=0;slot<8;++slot) {
			auto *entity=Entity(slot+1);
			auto *client=Client(slot);
			for (int sequence=0;sequence<96;++sequence) {
				const int frame=sequence&31,first=(sequence*3+slot)%RingCapacity,total=1+sequence%16;
				Put(client,SequenceOffset,sequence);
				Put(client,FrameCountOffset+frame*FrameStride,total);
				Put(client,FrameFirstOffset+frame*FrameStride,first);
				Check(records.SetOutgoingSequence(slot,sequence) && records.SetFrame(slot,sequence,total,first),"Replica frame");
				fntech3::q3ihv::NativeClients::SnapshotFrame mine{};
				Check(records.CurrentFrame(slot,mine),"Replica current frame");
				Check(snapshotCount(entity)==mine.count,"Snapshot entity count");
				digest.Word(mine.count);
				for (int i=0;i<total;++i) {
					int number=0;
					Check(records.SnapshotNumber(mine.first,i,number),"Replica snapshot number");
					Check(snapshotNumber(entity,i)==number,"Snapshot entity number");
					digest.Word(number); ++snapshotCases;
				}
			}
		}
		std::printf("{\"scope\":\"original bot imports, scheduler, userinfo update and snapshot getters against"
			" fntech3 native client records; no original engine startup\",\"allocations\":%u,\"frame_steps\":%u,"
			"\"userinfo_cases\":%u,\"snapshot_getters\":%u,\"digest\":\"%016llx\"}\n",
			allocations,frameSteps,userinfoCases,snapshotCases,static_cast<unsigned long long>(digest.value));
		return 0;
	} catch (const std::exception &e) {
		std::fprintf(stderr,"%s\n",e.what());
		return 1;
	}
}
