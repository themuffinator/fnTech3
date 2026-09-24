/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
/* Optional Win32 comparison of fnTech3's protocol-33 snapshot message against
 * the original SV_WriteSnapshotToClient. Both sides receive the same client
 * frames, shared entity ring and baselines, and the produced messages are
 * compared byte for byte. No original entry point, filesystem, display, input,
 * audio or network service starts, no collision model is loaded, and no
 * original module or asset is incorporated. The host base is 0x400000 so the
 * harness owns the original's address range as zeroed storage. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wincrypt.h>
#include <float.h>
#include "../code/compat/q3_ihv_native_boundary.hpp"
#include "../code/compat/q3_ihv_server_clients.hpp"
#include "../code/qcommon/q3_ihv_message.hpp"
#include "q3_ihv_collision_cases.hpp"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <vector>

static_assert(sizeof(void *) == 4,"Windows x86 reference probe");
#pragma bss_seg(".ihv")
static unsigned char observation[0x4b46000];
#pragma bss_seg()
static void Check(bool value,const char *why) { if (!value) throw std::runtime_error(why); }

namespace {
using fntech3::q3ihv::wire::Entity;
using fntech3::q3ihv::wire::GameState;
using fntech3::q3ihv::wire::MessageWriter;
using fntech3::q3ihv::wire::Snapshot;

constexpr unsigned SvEntities=0x425729c,SvEntityStride=0x100,BaselineOffset=8;
constexpr unsigned ClientStride=0xee18,SequenceOffset=0x2e18,DeltaOffset=0x440;
constexpr unsigned NameOffset=0x414,CommandSequenceOffset=0x40c,FlagsOffset=0x450;
constexpr unsigned FramesOffset=0x458,FrameStride=0x14c;
constexpr unsigned FrameAreaBytes=0,FrameAreaBits=4,FramePlayerState=0x24;
constexpr unsigned FrameCount=0x138,FrameFirst=0x13c;
constexpr unsigned StateBytes=FNTECH3_Q3_IHV_ENTITY_STATE_BYTES;
constexpr unsigned PlayerBytes=FNTECH3_Q3_IHV_PLAYER_STATE_BYTES;
constexpr int AreaBits=FNTECH3_Q3_IHV_AREABITS_BYTES,Capacity=1024,MessageBytes=1400;
constexpr int Numbers=1024;

int warnings;
void __cdecl StubPrintf(const char *,...){++warnings;}
void __cdecl StubError(int,const char *,...){throw std::runtime_error("original engine error");}

void PatchJump(unsigned address,const void *target) {
	auto *code=reinterpret_cast<unsigned char *>(address);
	code[0]=0xe9;
	const unsigned displacement=reinterpret_cast<unsigned>(target)-(address+5);
	std::memcpy(code+1,&displacement,4);
}
template<typename T> void Put(void *base,unsigned offset,T value) {
	std::memcpy(static_cast<char *>(base)+offset,&value,sizeof value);
}
template<typename T> T Get(const void *base,unsigned offset) {
	T value; std::memcpy(&value,static_cast<const char *>(base)+offset,sizeof value); return value;
}
unsigned char *Baseline(int number) {
	return reinterpret_cast<unsigned char *>(SvEntities+BaselineOffset
		+static_cast<unsigned>(number)*SvEntityStride);
}
unsigned seed=0x2468ace0u;
unsigned Next(){seed=seed*1664525u+1013904223u;return seed>>8;}
} // namespace

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
	/* The message writers, the snapshot writer with its entity emitter, the
	 * shared runtime helpers, and the original's data sections. */
	for (auto range : {std::array<unsigned,3>{0x410000,0x3000,1},{0x41a000,0x1000,1},
		{0x448000,0x2000,1},{0x45b000,0x3000,0},{0x45e000,0x12000,0}}) {
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
	PatchJump(0x40cc40,reinterpret_cast<const void *>(&StubPrintf));
	PatchJump(0x40cca0,reinterpret_cast<const void *>(&StubError));
}

int main(int argc,char **argv) {
	if (argc!=2) return 77;
	try {
		LoadReference(argv[1]);
		unsigned previous{}; Check(!_controlfp_s(&previous,_PC_53|_RC_NEAR,_MCW_PC|_MCW_RC),"FPU control");
		std::vector<unsigned char> client(ClientStride),buffer(MessageBytes),message(0x20);
		std::vector<unsigned char> ring(static_cast<std::size_t>(Capacity)*StateBytes);
		fntech3_q3_ihv_cvar_t padding{};
		*reinterpret_cast<void **>(0x424de20)=&padding;
		*reinterpret_cast<int *>(0x424de48)=Capacity;
		*reinterpret_cast<void **>(0x424de50)=ring.data();
		const auto init=reinterpret_cast<void (__cdecl *)(void *,void *,int)>(0x410a00);
		const auto write=reinterpret_cast<void (__cdecl *)(void *,void *)>(0x41add0);
		std::memcpy(client.data()+NameOffset,"probe",6);

		/* One gamestate serves both sides: its systeminfo advertises protocol 33
		 * and its baselines are the ones the original emitter reads. */
		GameState game;
		game.configstrings[0]="\\protocol\\33\\mapname\\probe";
		fntech3::q3ihv::NativeClients records;
		Check(records.Reset(1,2),"Replica client record");
		Check(records.BindSnapshotRing(Capacity),"Replica snapshot ring");

		unsigned scenarios=0,deltas=0,truncations=0,messageBytes=0,staleRequests=0;
		ihv_collision_cases::Digest digest;
		for (int scenario=0;scenario<1536;++scenario) {
			std::memset(client.data(),0,client.size());
			std::memcpy(client.data()+NameOffset,"probe",6);
			std::memset(ring.data(),0,ring.size());
			for (int number=0;number<Numbers;++number) std::memset(Baseline(number),0,StateBytes);
			for (auto &baseline : game.baselines) baseline={};
			records.Clear();
			Check(records.Reset(1,2) && records.BindSnapshotRing(Capacity),"Replica reset");

			/* Baselines for a random subset of the entity numbers in play.
			 * Real states stay close to their baseline, so mutate a few bytes
			 * rather than randomizing all 164: an entirely random state would
			 * overflow the original's 1400-byte message on its own. */
			static std::array<Entity,Numbers> templates{};
			for (int number=1;number<Numbers;++number) {
				Entity baseline{};
				const auto word=static_cast<std::int16_t>(number);
				std::memcpy(baseline.data(),&word,sizeof word);
				for (int mutation=0;mutation<6;++mutation)
					baseline[2+Next()%(StateBytes-2)]=static_cast<std::uint8_t>(Next()&0xff);
				templates[static_cast<std::size_t>(number)]=baseline;
				if (Next()&1) continue;
				std::memcpy(Baseline(number),baseline.data(),StateBytes);
				game.baselines[static_cast<std::size_t>(number)]=baseline;
			}
			fntech3_q3_ihv_playerState_t playerTemplate{};
			for (int mutation=0;mutation<12;++mutation)
				playerTemplate.bytes[Next()%PlayerBytes]=static_cast<std::uint8_t>(Next()&0xff);

			/* Two frames: an older one a delta may refer to, and the one being
			 * written. Both sides receive identical bytes. */
			/* Four kinds: an ordinary pair, a request too many messages old, a
			 * request whose entities the ring has overwritten, and a crowded
			 * frame whose entity list the message limit truncates. */
			const unsigned kind=Next()%4;
			const int sequenceBase=kind==1?32:0;
			const int oldSequence=sequenceBase+1+static_cast<int>(Next()%16);
			const int newSequence=oldSequence+1+static_cast<int>(Next()%4);
			Check((oldSequence&31)!=(newSequence&31),"Probe frames stay distinct");
			std::uint64_t cursor=0;
			int counts[2]{},firsts[2]{};
			for (int which=0;which<2;++which) {
				const int sequence=which?newSequence:oldSequence;
				const int count=kind==3&&which?200+static_cast<int>(Next()%60):static_cast<int>(Next()%8);
				// Overwrite the older frame's entities before the newer one.
				if (kind==2 && which) {
					fntech3_q3_ihv_entityState_t filler{};
					for (int i=0;i<Capacity;++i) {
						std::memcpy(ring.data()+(cursor%Capacity)*StateBytes,filler.bytes,StateBytes);
						Check(records.PushSnapshotState(filler),"Replica ring filler");
						++cursor;
					}
				}
				auto *frame=client.data()+FramesOffset+static_cast<unsigned>(sequence&31)*FrameStride;
				Put<int>(frame,FrameAreaBytes,1+static_cast<int>(Next()%AreaBits));
				for (int i=0;i<AreaBits;++i)
					frame[FrameAreaBits+static_cast<unsigned>(i)]=static_cast<std::uint8_t>(Next()&0xff);
				fntech3_q3_ihv_playerState_t player=playerTemplate;
				for (int mutation=0;mutation<which*4;++mutation)
					player.bytes[Next()%PlayerBytes]=static_cast<std::uint8_t>(Next()&0xff);
				std::memcpy(frame+FramePlayerState,player.bytes,PlayerBytes);
				Put<int>(frame,FrameCount,count);
				Put<int>(frame,FrameFirst,static_cast<int>(cursor));
				counts[which]=count; firsts[which]=static_cast<int>(cursor);
				int number=0,stored=0;
				for (int i=0;i<count;++i) {
					number+=1+static_cast<int>(Next()%3);
					if (number>=Numbers) break;
					fntech3_q3_ihv_entityState_t state{};
					std::memcpy(state.bytes,templates[static_cast<std::size_t>(number)].data(),StateBytes);
					for (int mutation=0;mutation<2;++mutation)
						state.bytes[2+Next()%(StateBytes-2)]=static_cast<std::uint8_t>(Next()&0xff);
					std::memcpy(ring.data()+(cursor%Capacity)*StateBytes,state.bytes,StateBytes);
					Check(records.PushSnapshotState(state),"Replica ring push");
					++cursor; ++stored;
				}
				Put<int>(frame,FrameCount,stored);
				counts[which]=stored;
				fntech3::q3ihv::NativeClients::SnapshotPayload payload;
				std::memcpy(payload.areabits,frame+FrameAreaBits,sizeof payload.areabits);
				payload.areabytes=Get<int>(frame,FrameAreaBytes);
				std::memcpy(payload.playerState.bytes,frame+FramePlayerState,PlayerBytes);
				Check(records.SetFramePayload(0,sequence,payload.playerState,payload.areabits,payload.areabytes)
					&& records.SetFrame(0,sequence,stored,static_cast<std::uint64_t>(firsts[which])),"Replica frame");
			}
			*reinterpret_cast<int *>(0x424de4c)=static_cast<int>(cursor);

			const int serverTime=1000+static_cast<int>(Next()%100000);
			const int configSequence=static_cast<int>(Next()%1000);
			const int commandSequence=static_cast<int>(Next()%1000);
			const int flags=static_cast<int>(Next()%256);
			// A quarter of the requests are deliberately too old or overwritten.
			const int request=kind==0&&(Next()&1)?0:oldSequence;
			*reinterpret_cast<int *>(0x4254e8c)=serverTime;
			*reinterpret_cast<int *>(0x4255298)=configSequence;
			Put<int>(client.data(),SequenceOffset,newSequence);
			Put<int>(client.data(),DeltaOffset,request);
			Put<int>(client.data(),CommandSequenceOffset,commandSequence);
			Put<int>(client.data(),FlagsOffset,flags);
			Check(records.SetOutgoingSequence(0,newSequence) && records.SetDeltaMessage(0,request)
				&& records.SetSequences(0,configSequence,commandSequence,flags),"Replica header");

			warnings=0;
			std::memset(message.data(),0,message.size());
			std::memset(buffer.data(),0,buffer.size());
			init(message.data(),buffer.data(),MessageBytes);
			Put<int>(message.data(),0,1); // allowoverflow, as the original sets
			write(client.data(),message.data());
			const int originalWarnings=warnings;
			const int originalBytes=Get<int>(message.data(),0x10);
			Check(!Get<int>(message.data(),4),"Original message overflowed");
			const int originalCount=Get<int>(client.data()+FramesOffset
				+static_cast<unsigned>(newSequence&31)*FrameStride,FrameCount);

			// fnTech3's side: the same delta decision and the shared codec.
			warnings=0;
			Snapshot frame,base;
			const bool stale=fntech3::q3ihv::SnapshotDeltaStale(newSequence,request);
			bool useDelta=false;
			if (!stale) {
				fntech3::q3ihv::NativeClients::SnapshotFrame old{};
				Check(records.FrameAt(0,request,old),"Replica delta frame");
				useDelta=records.SnapshotEntitiesAvailable(old.first);
				if (!useDelta) ++warnings;
			} else if (request>0) ++warnings;
			for (int which=0;which<2;++which) {
				Snapshot &out=which?frame:base;
				const int sequence=which?newSequence:request;
				if (!which && !useDelta) continue;
				fntech3::q3ihv::NativeClients::SnapshotFrame stored{};
				fntech3::q3ihv::NativeClients::SnapshotPayload payload;
				Check(records.FrameAt(0,sequence,stored) && records.FramePayload(0,sequence,payload),"Replica read");
				out.messageNumber=static_cast<std::uint32_t>(sequence);
				out.areaBytes=static_cast<std::uint8_t>(payload.areabytes);
				std::memcpy(out.areaMask.data(),payload.areabits,sizeof payload.areabits);
				std::memcpy(out.player.data(),payload.playerState.bytes,out.player.size());
				out.firstEntity=stored.first;
				for (int i=0;i<stored.count;++i) {
					fntech3_q3_ihv_entityState_t state;
					Check(records.SnapshotState(stored.first,i,state),"Replica ring read");
					Entity entity; std::memcpy(entity.data(),state.bytes,entity.size());
					out.entities.push_back(entity);
				}
			}
			frame.serverTime=static_cast<std::uint32_t>(serverTime);
			frame.deltaNumber=static_cast<std::uint32_t>(useDelta?request:-1);
			frame.configSequence=static_cast<std::uint32_t>(configSequence);
			frame.commandSequence=static_cast<std::uint32_t>(commandSequence);
			frame.flags=static_cast<std::uint8_t>(flags);
			MessageWriter writer;
			std::size_t written=0;
			Check(writer.AddSnapshot(game,useDelta?&base:nullptr,frame,
				FNTECH3_Q3_IHV_SNAPSHOT_MESSAGE_LIMIT,&written),"Replica snapshot message");

			Check(warnings==originalWarnings,"Delta rejection reports");
			Check(static_cast<int>(writer.Bytes())==originalBytes,"Snapshot message length");
			Check(!std::memcmp(writer.Data(),buffer.data(),static_cast<std::size_t>(originalBytes)),"Snapshot message bytes");
			Check(static_cast<int>(written)==originalCount,"Emitted entity count");
			if (useDelta) ++deltas;
			if (warnings) ++staleRequests;
			if (static_cast<int>(written)<counts[1]) ++truncations;
			messageBytes+=static_cast<unsigned>(originalBytes);
			for (int i=0;i<originalBytes;++i) digest.Byte(buffer[static_cast<std::size_t>(i)]);
			++scenarios;
		}
		std::printf("{\"scope\":\"original SV_WriteSnapshotToClient against fntech3's protocol-33 snapshot"
			" message, with identical frames, ring and baselines; no original engine startup\","
			"\"scenarios\":%u,\"delta_messages\":%u,\"rejected_requests\":%u,\"truncations\":%u,"
			"\"message_bytes\":%u,\"digest\":\"%016llx\"}\n",
			scenarios,deltas,staleRequests,truncations,messageBytes,
			static_cast<unsigned long long>(digest.value));
		return 0;
	} catch (const std::exception &e) {
		std::fprintf(stderr,"%s\n",e.what());
		return 1;
	}
}
