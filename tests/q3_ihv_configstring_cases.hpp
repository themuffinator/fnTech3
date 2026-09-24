/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_Q3_IHV_CONFIGSTRING_CASES_HPP
#define FNTECH3_Q3_IHV_CONFIGSTRING_CASES_HPP
#include "../code/compat/q3_ihv_configstrings.hpp"
#include <string>
#include <vector>

namespace ihv_config_cases {
using Strings = fntech3::q3ihv::client::ConfigStrings;
struct Update { std::uint32_t index; std::string value; };
inline std::vector<Update> Seed(unsigned variant) {
	std::vector<Update> values{{1023,"tail"},{7,"discarded"},{0,"\\protocol\\33"},
		{7,"kept"},{512,""},{32,"models/synthetic"},{288,"sound/synthetic"}};
	for (unsigned i=0; i<variant; ++i) values.push_back({600+i,std::string(80+i*13,static_cast<char>('a'+i))});
	return values;
}
inline std::vector<Update> Updates(unsigned variant) {
	std::vector<Update> result{{7,"kept"},{512,""},{7,""},{7,"restored"},
		{1023,"last"},{0,"\\protocol\\33\\mode\\synthetic"},{32,""},{32,"models/other"}};
	for (unsigned i=8; i<64; ++i) {
		const unsigned index=(i*37+variant*83)%1024;
		std::string value;
		if(i%4) for(unsigned n=0;n<(i*29+variant*7)%1024;++n)
			value.push_back(static_cast<char>(1+(n*53+i*71+variant*97)%254));
		// Restrict live total size; long strings are overwritten on the next pass.
		result.push_back({index%8+700,std::move(value)});
	}
	return result;
}
struct Digest {
	std::uint64_t value=14695981039346656037ull;
	void Bytes(const void *pointer,std::size_t size) {
		const auto *p=static_cast<const std::uint8_t *>(pointer);
		for(std::size_t i=0;i<size;++i) { value^=p[i]; value*=1099511628211ull; }
	}
	void Word(std::uint32_t word) {
		for(unsigned i=0;i<4;++i) { const auto b=static_cast<std::uint8_t>(word>>(i*8)); Bytes(&b,1); }
	}
};
}
#endif
