/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
/* Optional x86 original-codec observation. Only hash-pinned, self-contained
 * message routines are executed; original entry point/imports are not run.
 * Synthetic cases and numeric outputs are the only inputs/outputs retained. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wincrypt.h>
#include "../code/qcommon/q3_ihv_wire.hpp"
#include "q3_ihv_wire_cases.hpp"
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <vector>

static_assert(sizeof(void *) == 4, "Windows x86 reference probe");
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
	Check(reinterpret_cast<unsigned>(GetModuleHandleA(nullptr))==0x400000,"Harness image base");
	const auto *dos=reinterpret_cast<const IMAGE_DOS_HEADER *>(data.data());
	const auto *nt=reinterpret_cast<const IMAGE_NT_HEADERS *>(data.data()+dos->e_lfanew);
	const auto *sections=IMAGE_FIRST_SECTION(nt);
	for (auto range : {std::array<unsigned,2>{0x410000,0x3000},{0x448000,0x1000},{0x45b000,0x1000}}) {
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

struct Message { int allow,overflow; void *data; int capacity,size,read,bit; };
static_assert(sizeof(Message)==28);
using WriteBits=void (__cdecl *)(Message *,unsigned,int);
using ReadBits=unsigned (__cdecl *)(Message *,int);
using WriteEntity=void (__cdecl *)(Message *,const void *,const void *,int);
using ReadEntity=void (__cdecl *)(Message *,const void *,void *,int);
using StateCodec=void (__cdecl *)(Message *,const void *,void *);

template<std::size_t N> static void Run(const char *name,unsigned writeAddress,unsigned readAddress) {
	namespace codec=fntech3::q3ihv::wire;
	ihv_cases::Digest digest;
	for(std::size_t index=0;index<ihv_cases::Count<N>;++index) for(unsigned align=0;align<8;++align) {
		const auto c=ihv_cases::Make<N>(index);
		std::array<unsigned char,2048> original{},modern{};
		Message msg{0,0,original.data(),static_cast<int>(original.size()),0,0,0};
		auto writeBits=reinterpret_cast<WriteBits>(0x410b40);
		auto readBits=reinterpret_cast<ReadBits>(0x410be0);
		writeBits(&msg,0x5d,align);
		if constexpr (N==164) reinterpret_cast<WriteEntity>(writeAddress)(&msg,c.from.data(),c.remove?nullptr:c.to.data(),c.force);
		else reinterpret_cast<StateCodec>(writeAddress)(&msg,c.from.data(),const_cast<unsigned char *>(c.to.data()));
		const unsigned bitCount=msg.size*8-(msg.bit?8-msg.bit:0);
		codec::Writer out(modern.data(),modern.size()); out.Write(0x5d,align);
		bool encoded;
		if constexpr (N==164) encoded=codec::WriteEntity(out,c.from,c.remove?nullptr:&c.to,c.force);
		else if constexpr (N==14) encoded=codec::WriteCommand(out,c.from,c.to);
		else encoded=codec::WritePlayer(out,c.from,c.to);
		if (!encoded || out.Position()!=bitCount || std::memcmp(original.data(),modern.data(),msg.size)) {
			std::fprintf(stderr,"%s case %zu alignment %u: encode mismatch original %u bits, new %zu\n",name,index,align,bitCount,out.Position());
			for(int i=0;i<msg.size;++i) if(original[i]!=modern[i]) { std::fprintf(stderr,"byte %d: %02x != %02x\n",i,original[i],modern[i]); break; }
			Check(false,"Encoder differs");
		}
		auto decoded=c.from,expected=c.from;
		codec::Reader reader(original.data(),msg.size); std::uint32_t number=0; reader.Read(align,number);
		msg.read=msg.bit=0; readBits(&msg,align);
		bool parsed=true;
		if constexpr(N==164) {
			if (bitCount>align) { const unsigned oldNumber=readBits(&msg,10); reader.Read(10,number); Check(number==oldNumber,"Entity number");
				reinterpret_cast<ReadEntity>(readAddress)(&msg,c.from.data(),expected.data(),number);
				parsed=codec::ReadEntityDelta(reader,c.from,decoded,number); }
		} else {
			reinterpret_cast<StateCodec>(readAddress)(&msg,c.from.data(),expected.data());
			if constexpr(N==14) parsed=codec::ReadCommand(reader,c.from,decoded); else parsed=codec::ReadPlayer(reader,c.from,decoded);
		}
		const unsigned readCount=msg.read*8-(msg.bit?8-msg.bit:0);
		if (!parsed || reader.Position()!=readCount || decoded!=expected) {
			std::fprintf(stderr,"%s case %zu alignment %u: decode mismatch original %u bits, new %zu\n",name,index,align,readCount,reader.Position());
			for(unsigned i=0;i<N;++i) if(decoded[i]!=expected[i]) { std::fprintf(stderr,"state byte %u: %02x != %02x\n",i,expected[i],decoded[i]); break; }
			Check(false,"Decoder differs");
		}
		Check(!msg.overflow && readCount==bitCount,"Original message extent");
		digest.Word(static_cast<std::uint32_t>(index)); digest.Word(align); digest.Word(bitCount);
		digest.Bytes(original.data(),msg.size); digest.Bytes(expected.data(),expected.size());
	}
	std::cout<<"{\"kind\":\""<<name<<"\",\"cases\":"<<ihv_cases::Count<N><<",\"alignments\":8,\"fnv1a64\":\""<<std::hex<<std::setw(16)<<std::setfill('0')<<digest.hash<<std::dec<<"\"}";
}
int main(int argc,char **argv) {
	if(argc!=2) return 77;
	try { LoadReference(argv[1]); std::cout<<"{\"entity\":"; Run<164>("entity",0x411090,0x411530);
		std::cout<<",\"command\":"; Run<14>("command",0x411910,0x411a50);
		std::cout<<",\"player\":"; Run<276>("player",0x411b20,0x412180); std::cout<<"}\n"; return 0;
	} catch(const std::exception &e) { std::fprintf(stderr,"%s\n",e.what()); return 1; }
}
