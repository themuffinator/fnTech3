/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_Q3_IHV_MODEL_TAG_CASES_HPP
#define FNTECH3_Q3_IHV_MODEL_TAG_CASES_HPP
#include "../code/compat/q3_ihv_model_tags.hpp"
#include <array>
#include <cstdio>
#include <vector>

namespace ihv_tag_cases {
inline void Word(std::vector<std::uint8_t> &data, std::size_t at, std::uint32_t value) {
	for (unsigned i=0;i<4;++i) data[at+i]=static_cast<std::uint8_t>(value>>(i*8));
}
inline void Float(std::vector<std::uint8_t> &data, std::size_t at, float value) {
	std::uint32_t bits;std::memcpy(&bits,&value,4);Word(data,at,bits);
}
inline std::vector<std::uint8_t> Model(unsigned frames=4,unsigned tags=3) {
	std::vector<std::uint8_t> data(108+frames*56+frames*tags*112);
	Word(data,0,0x33504449);Word(data,4,15);Word(data,76,frames);Word(data,80,tags);
	Word(data,92,108);Word(data,96,108+frames*56);Word(data,100,static_cast<std::uint32_t>(data.size()));Word(data,104,static_cast<std::uint32_t>(data.size()));
	for(unsigned frame=0;frame<frames;++frame) {
		const auto at=108+frame*56;
		for(unsigned i=0;i<3;++i){Float(data,at+i*4,-float(frame+i+1));Float(data,at+12+i*4,float(frame+i+2));Float(data,at+24+i*4,float(frame+i)*0.25f);}
		Float(data,at+36,float(frame+4));std::memcpy(data.data()+at+40,"generated frame",15);
	}
	for(unsigned frame=0;frame<frames;++frame)for(unsigned tag=0;tag<tags;++tag) {
		const auto at=108+frames*56+(frame*tags+tag)*112;
		char name[64]{};std::snprintf(name,sizeof name,"tag_%u",tag);std::memcpy(data.data()+at,name,64);
		for(unsigned i=0;i<3;++i)Float(data,at+64+i*4,static_cast<float>(frame*23+tag*7+i)*0.125f-5.0f);
		for(unsigned i=0;i<9;++i)Float(data,at+76+i*4,
			tag==2?0.0f:(static_cast<float>((frame*13+tag*5+i*7)%23)-11.0f)*0.1f);
	}
	return data;
}
struct Digest {
	std::uint64_t value=14695981039346656037ull;
	void Add(const void *data,std::size_t length){const auto *p=static_cast<const unsigned char*>(data);for(std::size_t i=0;i<length;++i){value^=p[i];value*=1099511628211ull;}}
};
} // namespace ihv_tag_cases
#endif
