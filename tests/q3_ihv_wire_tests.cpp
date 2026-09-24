/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#include "../code/qcommon/q3_ihv_wire.hpp"
#include "q3_ihv_wire_cases.hpp"
#include "fixtures/q3_ihv_wire_vectors.h"
#include <cstdio>
#include <cstdlib>

#define CHECK(x) do { if (!(x)) { std::fprintf(stderr,"%d: %s\n",__LINE__,#x); std::exit(1); } } while(0)
namespace codec = fntech3::q3ihv::wire;
static_assert(ihv_cases::Count<164> == ihv_vectors::EntityCases);
static_assert(ihv_cases::Count<14> == ihv_vectors::CommandCases);
static_assert(ihv_cases::Count<276> == ihv_vectors::PlayerCases);

template<std::size_t N> bool Encode(codec::Writer &out,const ihv_cases::Case<N> &c) {
	if constexpr(N==164) return codec::WriteEntity(out,c.from,c.remove?nullptr:&c.to,c.force);
	else if constexpr(N==14) return codec::WriteCommand(out,c.from,c.to);
	else return codec::WritePlayer(out,c.from,c.to);
}
template<std::size_t N> bool Decode(codec::Reader &in,const ihv_cases::Case<N> &c,std::array<std::uint8_t,N> &decoded) {
	if constexpr(N==164) { std::uint32_t number; return in.Read(10,number) && codec::ReadEntityDelta(in,c.from,decoded,number); }
	else if constexpr(N==14) return codec::ReadCommand(in,c.from,decoded);
	else return codec::ReadPlayer(in,c.from,decoded);
}

void Bits() {
	for(unsigned prefix=0;prefix<8;++prefix) for(unsigned width=1;width<=32;++width) for(auto value : {0u,1u,0xffffffffu,0x80000000u,0xa5c36987u}) {
		std::array<std::uint8_t,10> bytes{}; bytes.fill(0xff);
		codec::Writer out(bytes.data()+1,8); CHECK(out.Write(0x5d,prefix)); CHECK(out.Write(value,width));
		CHECK(bytes.front()==255 && bytes.back()==255);
		codec::Reader in(bytes.data()+1,out.Bytes()); std::uint32_t got;
		CHECK(in.Read(prefix,got) && got==(0x5du & ((1u<<prefix)-1)));
		CHECK(in.Read(width,got) && got==(width==32?value:value&((1u<<width)-1)));
		CHECK(in.Position()==out.Position());
	}
	std::uint32_t value=123;
	codec::Reader empty(nullptr,0); CHECK(empty.Read(0,value) && !value); CHECK(!empty.Read(1,value) && !empty.Position());
	codec::Writer noSpace(nullptr,0); CHECK(noSpace.Write(0,0)); CHECK(!noSpace.Write(0,1) && !noSpace.Position());
	std::uint8_t data[8]{}; codec::Writer tooWide(data,8); CHECK(!tooWide.Write(0,33) && !tooWide.Position());
	codec::Reader tooMany(data,8); CHECK(!tooMany.Read(33,value) && !tooMany.Position());
	const auto overflow=(std::numeric_limits<std::size_t>::max)()/8+1;
	codec::Reader invalid(data,overflow); codec::Writer invalidOut(data,overflow); CHECK(!invalid.Good() && !invalidOut.Good());
	codec::Reader missing(nullptr,8); codec::Writer missingOut(nullptr,8); CHECK(!missing.Good() && !missingOut.Good());
}

template<std::size_t N> void Vectors(std::uint64_t expected) {
	ihv_cases::Digest digest;
	for(std::size_t index=0;index<ihv_cases::Count<N>;++index) for(unsigned prefix=0;prefix<8;++prefix) {
		const auto c=ihv_cases::Make<N>(index); std::array<std::uint8_t,2048> bytes{};
		codec::Writer out(bytes.data(),bytes.size()); CHECK(out.Write(0x5d,prefix)); CHECK(Encode(out,c));
		auto decoded=c.from; codec::Reader in(bytes.data(),out.Bytes()); std::uint32_t ignored; CHECK(in.Read(prefix,ignored));
		if(out.Position()>prefix) CHECK(Decode(in,c,decoded));
		CHECK(in.Position()==out.Position());
		digest.Word(static_cast<std::uint32_t>(index)); digest.Word(prefix); digest.Word(static_cast<std::uint32_t>(out.Position()));
		digest.Bytes(bytes.data(),out.Bytes()); digest.Bytes(decoded.data(),decoded.size());
	}
	if(digest.hash!=expected) std::fprintf(stderr,"%zu-byte state: expected %016llx, got %016llx\n",N,
		static_cast<unsigned long long>(expected),static_cast<unsigned long long>(digest.hash));
	CHECK(digest.hash==expected);
}

template<std::size_t N> void Truncation() {
	for (auto index : {std::size_t(0), ihv_cases::BitCases<N>+1, ihv_cases::BitCases<N>+128}) for(unsigned prefix=0;prefix<8;++prefix) {
		const auto c=ihv_cases::Make<N>(index); std::array<std::uint8_t,2048> bytes{};
		codec::Writer out(bytes.data(),bytes.size()); out.Write(0x5d,prefix); CHECK(Encode(out,c));
		for(std::size_t size=0;size<out.Bytes();++size) {
			codec::Reader truncated(bytes.data(),size); std::uint32_t ignored;
			std::array<std::uint8_t,N> decoded; decoded.fill(0xa5); const auto before=decoded;
			if(truncated.Read(prefix,ignored)) CHECK(!Decode(truncated,c,decoded));
			CHECK(decoded==before && !truncated.Good());
			std::array<std::uint8_t,2048> shortOutput; shortOutput.fill(0xa5);
			codec::Writer shortWriter(shortOutput.data(),size);
			if(shortWriter.Write(0x5d,prefix)) {
				const auto previous=shortOutput; const auto position=shortWriter.Position();
				CHECK(!Encode(shortWriter,c) && shortWriter.Position()==position && shortOutput==previous);
			}
		}
	}
}

void MalformedAndQuirks() {
	std::array<std::uint8_t,128> bytes{}; codec::Entity a{},b{};
	for(unsigned number : {0u,1024u,65535u}) {
		codec::Store(a.data(),2,number); const auto original=bytes; codec::Writer writer(bytes.data(),bytes.size());
		CHECK(!codec::WriteEntity(writer,a,&a,true) && !writer.Position() && bytes==original);
		codec::Reader reader(bytes.data(),bytes.size()); b.fill(0xa5); const auto prior=b;
		CHECK(!codec::ReadEntityDelta(reader,a,b,number) && !reader.Position() && b==prior);
	}
	// Unknown player flags are rejected before publication.
	for(unsigned reserved : {0x80000u,0x20000000u,0x40000000u,0x80000000u}) {
		codec::Writer writer(bytes.data(),bytes.size()); writer.Write(0x8000,16); writer.Write(reserved>>16,16);
		codec::Player from{},to; to.fill(0xa5); const auto before=to; codec::Reader reader(bytes.data(),writer.Bytes());
		CHECK(!codec::ReadPlayer(reader,from,to) && !reader.Position() && to==before);
	}
	// A legacy light comparison can suppress a real light change. The reader
	// retains its previous light, exactly as the original client does.
	a={}; a[0]=7; a[144]=23; a[148]=7; b=a; b[144]=7;
	codec::Writer writer(bytes.data(),bytes.size()); CHECK(codec::WriteEntity(writer,a,&b,true));
	codec::Reader reader(bytes.data(),writer.Bytes()); std::uint32_t number; reader.Read(10,number);
	codec::Entity decoded{}; CHECK(codec::ReadEntityDelta(reader,a,decoded,number) && decoded[144]==23);
	// The original color group is triggered by its first byte only.
	codec::Player from{},to{}; to[97]=255; codec::Writer color(bytes.data(),bytes.size());
	CHECK(codec::WritePlayer(color,from,to) && color.Position()==16 && !bytes[0] && !bytes[1]);
	CHECK(!codec::FloatChanged(0,0x80000000u)); CHECK(!codec::FloatChanged(0,0x7fc12345u));
	CHECK(codec::QuietFloat(0x7f812345u)==0x7fc12345u);
}
int main() {
	Bits(); Vectors<164>(ihv_vectors::EntityHash); Vectors<14>(ihv_vectors::CommandHash); Vectors<276>(ihv_vectors::PlayerHash);
	Truncation<164>(); Truncation<14>(); Truncation<276>(); MalformedAndQuirks();
	std::printf("%zu IHV original-codec vectors and malformed/bounds checks passed\n",
		8*(ihv_cases::Count<164>+ihv_cases::Count<14>+ihv_cases::Count<276>));
}
