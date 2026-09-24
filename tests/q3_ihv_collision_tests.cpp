/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#include "q3_ihv_collision_cases.hpp"
#include <cstdio>
#include <cstdlib>

#define CHECK(x) do { if(!(x)) { std::fprintf(stderr,"%d: %s\n",__LINE__,#x); std::exit(1); } } while(0)
using namespace fntech3::q3ihv;
static bool Same(const LinkBounds &a,const LinkBounds &b) {
	return a.solid==b.solid && !std::memcmp(a.mins,b.mins,12) && !std::memcmp(a.maxs,b.maxs,12);
}
static void Malformed() {
	const float finite[3]={0,0,0};
	LinkBounds output{713,{1,2,3},{4,5,6}},before=output;
	for (unsigned pointer=0;pointer<4;++pointer) {
		const float *inputs[4]={finite,finite,finite,finite}; inputs[pointer]=nullptr;
		CHECK(!CalculateLinkBounds(inputs[0],inputs[1],inputs[2],inputs[3],false,1,output)); CHECK(Same(output,before));
	}
	for (std::uint32_t bits : {0x7f800000u,0xff800000u,0x7fc00000u,0x7f800001u}) {
		for (unsigned field=0;field<4;++field) for (unsigned axis=0;axis<3;++axis) {
			float inputs[4][3]{}; std::memcpy(inputs[field]+axis,&bits,4);
			CHECK(!CalculateLinkBounds(inputs[0],inputs[1],inputs[2],inputs[3],false,1,output)); CHECK(Same(output,before));
		}
	}
	float inverted[3]={0,1,0};
	CHECK(!CalculateLinkBounds(inverted,finite,finite,finite,false,1,output)); CHECK(Same(output,before));
	float huge[3]={(std::numeric_limits<float>::max)(),(std::numeric_limits<float>::max)(),0};
	CHECK(!CalculateLinkBounds(finite,huge,huge,finite,false,1,output)); CHECK(Same(output,before));
	float angles[3]={0,1,0};
	CHECK(!CalculateLinkBounds(finite,huge,finite,angles,true,1,output)); CHECK(Same(output,before));
	CHECK(CalculateLinkBounds(finite,finite,finite,finite,false,1,output));
	CHECK(output.solid==4129 && output.mins[0]==-1 && output.maxs[2]==1);
}
int main() {
	const auto cases=ihv_collision_cases::Make(); CHECK(cases.size()==2740);
	ihv_collision_cases::Digest digest;
	for (std::size_t index=0;index<cases.size();++index) {
		LinkBounds result; CHECK(ihv_collision_cases::Calculate(cases[index],result));
		digest.Word(static_cast<std::uint32_t>(index)); digest.Add(result);
	}
	// Original SV_LinkEntity, x87 precision 53, same synthetic cases; see the
	// optional reference probe. No executable or asset is needed by this test.
	if(digest.value!=0x7c40185466a69dbdull) std::fprintf(stderr,"digest %016llx\n",static_cast<unsigned long long>(digest.value));
	CHECK(digest.value==0x7c40185466a69dbdull);
	Malformed();
	std::puts("2740 original IHV link-geometry vectors and malformed/overflow checks passed");
}
