/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#include "q3_ihv_configstring_cases.hpp"
#include <cstdio>
#include <cstdlib>
#include <type_traits>

#define CHECK(x) do { if(!(x)) { std::fprintf(stderr,"%d: %s\n",__LINE__,#x); std::exit(1); } } while(0)
using namespace ihv_config_cases;
static_assert(sizeof(fntech3_q3_ihv_gameState_t)==20100 && sizeof(fntech3_q3_ihv_snapshot_t)==348);
static_assert(std::is_trivially_copyable_v<fntech3_q3_ihv_gameState_t>);

static std::uint32_t Word(const fntech3_q3_ihv_gameState_t &r,unsigned at) {
	std::uint32_t value=0; for(unsigned i=0;i<4;++i) value|=std::uint32_t(r.bytes[at+i])<<(i*8); return value;
}
static void Storage() {
	Strings s; CHECK(s.BytesUsed()==1 && s.String(0).empty() && s.String(1024).empty());
	CHECK(s.Append(9,"abc") && s.Append(3,"") && s.Append(9,"xyz"));
	CHECK(s.BytesUsed()==10 && Word(s.Record(),9*4)==6 && Word(s.Record(),3*4)==5);
	CHECK(!std::memcmp(s.Record().bytes+4096,"\0abc\0\0xyz\0",10));
	const auto before=s.Record(); bool changed=true;
	CHECK(s.Set(9,"xyz",changed) && !changed && !std::memcmp(&before,&s.Record(),sizeof before));
	CHECK(s.Set(0,s.String(9),changed) && changed && s.String(0)=="xyz" && s.BytesUsed()==9);
	CHECK(Word(s.Record(),0)==1 && Word(s.Record(),9*4)==5 && Word(s.Record(),3*4)==0);
	CHECK(s.Append(3,s.String(9)) && s.String(3)=="xyz");
	CHECK(s.Set(9,"",changed) && changed && Word(s.Record(),9*4)==0 && s.BytesUsed()==9);
	CHECK(s.Set(0,"",changed) && s.Set(3,"",changed) && s.BytesUsed()==1);
	for(unsigned i=0;i<sizeof(s.Record().bytes);++i) CHECK(s.Record().bytes[i]==(i==20096?1:0));
}
static void Invalid() {
	Strings s; CHECK(s.Append(0,std::string(1023,'x')));
	for(unsigned i=1;i<15;++i) CHECK(s.Append(i,std::string(1023,static_cast<char>('a'+i))));
	CHECK(s.Append(15,std::string(638,'z')) && s.BytesUsed()==16000);
	const auto before=s.Record(); bool changed=true;
	CHECK(!s.Append(16,"")); // Even an empty initial record costs one byte.
	CHECK(!s.Set(16,"x",changed) && changed);
	CHECK(!s.Set(1024,"",changed) && changed);
	CHECK(!s.Append(0xffffffffu,"x"));
	CHECK(!s.Set(0,std::string(1024,'x'),changed));
	CHECK(!s.Set(0,std::string("x\0y",3),changed));
	CHECK(!s.Append(0,std::string(1,static_cast<char>(255))));
	CHECK(!std::memcmp(&before,&s.Record(),sizeof before));
	CHECK(s.Set(0,"",changed) && changed && s.BytesUsed()==14976);
	CHECK(s.Set(16,"recovered",changed) && changed);
}
static void OriginalCorpus() {
	Digest digest; unsigned cases=0,changes=0;
	for(unsigned variant=0;variant<8;++variant) {
		Strings s; for(const auto &seed:Seed(variant)) CHECK(s.Append(seed.index,seed.value));
		for(const auto &update:Updates(variant)) {
			bool changed=false; CHECK(s.Set(update.index,update.value,changed));
			digest.Word(variant); digest.Word(update.index); digest.Word(changed);
			digest.Bytes(&s.Record(),sizeof(s.Record())); ++cases; changes+=changed;
		}
	}
	std::printf("IHV configstring corpus: %u cases, %u changes, %016llx\n",cases,changes,static_cast<unsigned long long>(digest.value));
	// Optional hash-pinned original parser: 512 full-record comparisons.
	CHECK(cases==512 && changes==384 && digest.value==0x06594df1f7ec01c4ull);
}
int main() { Storage(); Invalid(); OriginalCorpus(); std::puts("IHV native configstring layout, aliases, capacity and recovery passed"); }
