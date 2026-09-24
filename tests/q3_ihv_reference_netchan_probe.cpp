/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
/* Optional Win32 comparison of fnTech3's protocol-33 netchan against the
 * original Netchan_Transmit, Netchan_Process and their packet scrambler. Both
 * channels are driven with the same sequences, reliable blocks and payloads,
 * and the produced packets are compared byte for byte. Only the sending
 * routine is replaced, so no socket is opened; no original entry point,
 * filesystem, display, input, audio or collision service starts, and no
 * original module or asset is incorporated. The host base is 0x400000 so the
 * harness owns the original's address range as zeroed storage. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wincrypt.h>
#include <float.h>
#include "../code/compat/q3_ihv_game_abi.h"
#include "../code/compat/q3_ihv_netchan.hpp"
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
using fntech3::q3ihv::Netchan;
using fntech3::q3ihv::NetchanResult;
using fntech3::q3ihv::NetchanSink;
using fntech3::q3ihv::NetchanSource;

/* The original's netchan_t, as its own code addresses it. */
constexpr unsigned ChanBytes=0xc034;
constexpr unsigned FatalError=0,Sock=4,Dropped=8,RemoteAddress=0xc,Qport=0x20;
constexpr unsigned IncomingSequence=0x24,IncomingAcknowledged=0x28;
constexpr unsigned IncomingReliableAcknowledged=0x2c,IncomingReliableSequence=0x30;
constexpr unsigned OutgoingSequence=0x34,ReliableSequence=0x38,LastReliableSequence=0x3c;
constexpr unsigned FragmentLength=0x40,FragmentBuffer=0x44;
constexpr unsigned Message=0x4034,MessageBuffer=0x4050;
constexpr unsigned ReliableLength=0x8040,ReliableBuffer=0x8044;
constexpr unsigned MessageData=8,MessageMaxsize=0xc,MessageCursize=0x10,MessageReadcount=0x14;
constexpr std::size_t Buffer=16368,Packet=1400;

std::vector<std::vector<unsigned char>> sent;
int prints;
void __cdecl StubPrintf(const char *,...){++prints;}
void __cdecl StubError(int,const char *,...){throw std::runtime_error("original engine error");}
/* NET_SendPacket(sock, length, data, address) keeps the bytes instead. */
void __cdecl StubSendPacket(int,int length,const void *data,...) {
	Check(length>=0 && static_cast<std::size_t>(length)<=Packet,"Original packet length");
	sent.emplace_back(static_cast<const unsigned char *>(data),
		static_cast<const unsigned char *>(data)+length);
}
const char * __cdecl StubAdrToString(...){return "probe";}
/* LittleLong is a function pointer on this build; x86 keeps it identity. */
int __cdecl StubIdentity(int value){return value;}

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
unsigned seed=0x0f1e2d3cu;
unsigned Next(){seed=seed*1664525u+1013904223u;return seed>>8;}

bool Capture(void *context,const unsigned char *data,std::size_t size) {
	auto *packets=static_cast<std::vector<std::vector<unsigned char>> *>(context);
	packets->emplace_back(data,data+size);
	return true;
}
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
	/* The message readers and writers, the netchan itself, the byte-order
	 * thunks, the shared runtime helpers and the original data sections. */
	constexpr std::array<std::array<unsigned,3>,4> ranges{{{0x410000,0x5000,1},{0x448000,0x2000,1},
		{0x45b000,0x3000,0},{0x45e000,0x12000,0}}};
	for (auto range : ranges) {
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
	}
	/* Diagnostics, address formatting and the sending routine answer from this
	 * harness. The byte-order helper calls through a pointer the original
	 * installs at startup, so replace the helper itself with the identity x86
	 * startup would have chosen. Patch before the pages become read-only. */
	PatchJump(0x4148d0,reinterpret_cast<const void *>(&StubIdentity));
	PatchJump(0x40cc40,reinterpret_cast<const void *>(&StubPrintf));
	PatchJump(0x40cae0,reinterpret_cast<const void *>(&StubPrintf));
	PatchJump(0x40cca0,reinterpret_cast<const void *>(&StubError));
	PatchJump(0x412f40,reinterpret_cast<const void *>(&StubAdrToString));
	PatchJump(0x413210,reinterpret_cast<const void *>(&StubSendPacket));
	for (auto range : ranges) {
		if (!range[2]) continue;
		DWORD old{};
		Check(VirtualProtect(reinterpret_cast<void *>(range[0]),range[1],PAGE_EXECUTE_READ,&old),"Code protection");
		Check(FlushInstructionCache(GetCurrentProcess(),reinterpret_cast<void *>(range[0]),range[1]),"Instruction cache");
	}
}

int main(int argc,char **argv) {
	if (argc!=2) return 77;
	try {
		LoadReference(argv[1]);
		unsigned previous{}; Check(!_controlfp_s(&previous,_PC_53|_RC_NEAR,_MCW_PC|_MCW_RC),"FPU control");
		std::vector<unsigned char> chan(ChanBytes);
		fntech3_q3_ihv_cvar_t showpackets{},showdrop{},qport{};
		*reinterpret_cast<void **>(0x42e0f34)=&qport;      // qport cvar
		*reinterpret_cast<void **>(0x42e0f38)=&showpackets;
		*reinterpret_cast<void **>(0x42e0f30)=&showdrop;
		const auto transmit=reinterpret_cast<void (__cdecl *)(void *,int,const void *)>(0x4127b0);
		const auto process=reinterpret_cast<int (__cdecl *)(void *,void *)>(0x412b30);
		const auto scramble=reinterpret_cast<void (__cdecl *)(void *)>(0x412690);

		unsigned scenarios=0,packets=0,fragments=0,received=0,scrambles=0,bytes=0;
		ihv_collision_cases::Digest digest;

		/* The scrambler first: its key stream must match byte for byte before
		 * any packet comparison means anything. */
		for (int trial=0;trial<512;++trial) {
			const std::size_t size=8+static_cast<std::size_t>(Next()%(Packet-8));
			std::vector<unsigned char> original(size),replica(size);
			for (auto &byte : original) byte=static_cast<unsigned char>(Next()&0xff);
			replica=original;
			std::vector<unsigned char> message(0x20);
			Put<void *>(message.data(),MessageData,original.data());
			Put<int>(message.data(),MessageMaxsize,static_cast<int>(size));
			Put<int>(message.data(),MessageCursize,static_cast<int>(size));
			scramble(message.data());
			fntech3::q3ihv::NetchanScramble(replica.data(),replica.size());
			Check(original==replica,"Packet key stream");
			for (unsigned char byte : replica) digest.Byte(byte);
			++scrambles;
		}

		for (int scenario=0;scenario<1024;++scenario) {
			const bool client=(Next()&1)!=0;
			const int port=static_cast<int>(Next()%0xffff);
			std::memset(chan.data(),0,chan.size());
			Put<int>(chan.data(),Sock,client?0:1);
			Put<int>(chan.data(),Qport,port);
			Put<int>(chan.data(),OutgoingSequence,1);
			Put<void *>(chan.data()+Message,MessageData,chan.data()+MessageBuffer);
			Put<int>(chan.data()+Message,MessageMaxsize,static_cast<int>(Buffer));
			qport.value=static_cast<float>(port);
			Netchan replica;
			Check(replica.Setup(client?NetchanSource::Client:NetchanSource::Server,port),"Replica setup");

			for (int round=0;round<4;++round) {
				// Acknowledgements the peer would have sent back.
				const int ack=static_cast<int>(Next()%8),reliableAck=static_cast<int>(Next()&1);
				const int incoming=static_cast<int>(Next()%8);
				Put<int>(chan.data(),IncomingAcknowledged,ack);
				Put<int>(chan.data(),IncomingReliableAcknowledged,reliableAck);
				Put<int>(chan.data(),IncomingSequence,incoming);
				Put<int>(chan.data(),IncomingReliableSequence,static_cast<int>(Next()&1));
				auto state=replica.Sequences();
				state.incomingAcknowledged=ack;
				state.incomingReliableAcknowledged=reliableAck;
				state.incomingSequence=incoming;
				state.incomingReliableSequence=Get<int>(chan.data(),IncomingReliableSequence);
				Check(replica.SetSequences(state),"Replica sequences");

				// A reliable block, sometimes large enough to fragment.
				const unsigned kind=Next()%4;
				const std::size_t reliable=kind==0?0:kind==3
					?1300+static_cast<std::size_t>(Next()%2600)
					:1+static_cast<std::size_t>(Next()%600);
				std::vector<unsigned char> block(reliable);
				for (auto &byte : block) byte=static_cast<unsigned char>(Next()&0xff);
				// The host appends to the pending message; the channel promotes it
				// when its reliable slot is free, so both sides must append alike.
				const auto pending=static_cast<std::size_t>(Get<int>(chan.data()+Message,MessageCursize));
				if (reliable && pending+reliable<=Buffer) {
					std::memcpy(chan.data()+MessageBuffer+pending,block.data(),reliable);
					Put<int>(chan.data()+Message,MessageCursize,static_cast<int>(pending+reliable));
					Check(replica.QueueReliable(block.data(),reliable),"Replica reliable queue");
					Check(replica.PendingLength()==pending+reliable,"Replica pending length");
				}
				const std::size_t payload=static_cast<std::size_t>(Next()%700);
				std::vector<unsigned char> unreliable(payload);
				for (auto &byte : unreliable) byte=static_cast<unsigned char>(Next()&0xff);

				sent.clear();
				prints=0;
				transmit(chan.data(),static_cast<int>(payload),unreliable.data());
				std::vector<std::vector<unsigned char>> mine;
				NetchanSink sink{&mine,Capture};
				Check(replica.Transmit(sink,unreliable.data(),payload),"Replica transmit");

				Check(sent.size()==mine.size(),"Packet count");
				for (std::size_t i=0;i<sent.size();++i) {
					Check(sent[i]==mine[i],"Packet bytes");
					for (unsigned char byte : mine[i]) digest.Byte(byte);
					bytes+=static_cast<unsigned>(mine[i].size());
					++packets;
				}
				if (sent.size()>1) ++fragments;
				Check(Get<int>(chan.data(),OutgoingSequence)==replica.OutgoingSequence(),"Outgoing sequence");
				const auto after=replica.Sequences();
				Check(Get<int>(chan.data(),ReliableSequence)==after.reliableSequence,"Reliable sequence");
				Check(Get<int>(chan.data(),LastReliableSequence)==after.lastReliableSequence,
					"Last reliable sequence");
				Check(static_cast<std::size_t>(Get<int>(chan.data(),ReliableLength))==replica.ReliableLength(),
					"Reliable length");
			}
			++scenarios;
		}

		/* The receive side: feed the packets one channel produced into another
		 * and compare what each side reports and delivers. */
		for (int scenario=0;scenario<512;++scenario) {
			std::vector<unsigned char> sender(ChanBytes),receiver(ChanBytes);
			std::memset(sender.data(),0,sender.size());
			std::memset(receiver.data(),0,receiver.size());
			Put<int>(sender.data(),Sock,0);
			Put<int>(sender.data(),Qport,1234);
			Put<int>(sender.data(),OutgoingSequence,1);
			Put<void *>(sender.data()+Message,MessageData,sender.data()+MessageBuffer);
			Put<int>(sender.data()+Message,MessageMaxsize,static_cast<int>(Buffer));
			Put<int>(receiver.data(),Sock,1);
			Put<void *>(receiver.data()+Message,MessageData,receiver.data()+MessageBuffer);
			Put<int>(receiver.data()+Message,MessageMaxsize,static_cast<int>(Buffer));
			qport.value=1234;
			Netchan replicaReceiver;
			Check(replicaReceiver.Setup(NetchanSource::Server,0),"Replica receiver");

			const unsigned kind=Next()%3;
			const std::size_t reliable=kind==0?0:kind==2
				?1300+static_cast<std::size_t>(Next()%2600)
				:1+static_cast<std::size_t>(Next()%600);
			std::vector<unsigned char> block(reliable);
			for (auto &byte : block) byte=static_cast<unsigned char>(Next()&0xff);
			if (reliable) {
				std::memcpy(sender.data()+MessageBuffer,block.data(),reliable);
				Put<int>(sender.data()+Message,MessageCursize,static_cast<int>(reliable));
			}
			const std::size_t payload=reliable>=1300?0:static_cast<std::size_t>(Next()%400);
			std::vector<unsigned char> unreliable(payload);
			for (auto &byte : unreliable) byte=static_cast<unsigned char>(Next()&0xff);
			sent.clear();
			transmit(sender.data(),static_cast<int>(payload),unreliable.data());

			for (const auto &packet : sent) {
				std::vector<unsigned char> originalCopy=packet,replicaCopy=packet;
				originalCopy.resize(Buffer);
				std::vector<unsigned char> message(0x20);
				Put<void *>(message.data(),MessageData,originalCopy.data());
				Put<int>(message.data(),MessageMaxsize,static_cast<int>(Buffer));
				Put<int>(message.data(),MessageCursize,static_cast<int>(packet.size()));
				prints=0;
				const int accepted=process(receiver.data(),message.data());
				std::size_t size=replicaCopy.size();
				replicaCopy.resize(Buffer);
				const auto result=replicaReceiver.Process(replicaCopy.data(),size,Buffer);
				Check((result==NetchanResult::Message)==(accepted!=0),"Accepted packet");
				if (!accepted) continue;
				const auto read=static_cast<std::size_t>(Get<int>(message.data(),MessageReadcount));
				const auto cursize=static_cast<std::size_t>(Get<int>(message.data(),MessageCursize));
				Check(cursize>=read,"Original message bounds");
				Check(size==cursize-read,"Delivered length");
				Check(!std::memcmp(replicaCopy.data(),originalCopy.data()+read,size),"Delivered bytes");
				for (std::size_t i=0;i<size;++i) digest.Byte(replicaCopy[i]);
				++received;
			}
			++scenarios;
		}
		std::printf("{\"scope\":\"original Netchan_Transmit, Netchan_Process and packet scrambler against"
			" fntech3's protocol-33 netchan; only the sending routine is replaced\","
			"\"scenarios\":%u,\"scrambled\":%u,\"packets\":%u,\"fragmented_transmissions\":%u,"
			"\"delivered\":%u,\"packet_bytes\":%u,\"digest\":\"%016llx\"}\n",
			scenarios,scrambles,packets,fragments,received,bytes,
			static_cast<unsigned long long>(digest.value));
		return 0;
	} catch (const std::exception &e) {
		std::fprintf(stderr,"%s\n",e.what());
		return 1;
	}
}
