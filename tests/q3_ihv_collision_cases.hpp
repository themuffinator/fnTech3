/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_Q3_IHV_COLLISION_CASES_HPP
#define FNTECH3_Q3_IHV_COLLISION_CASES_HPP
#include "../code/compat/q3_ihv_collision.hpp"
#include <array>
#include <vector>

namespace ihv_collision_cases {
struct Case {
	std::array<float,3> mins{{-16,-16,-24}}, maxs{{16,16,32}};
	std::array<float,3> origin{{11,22,33}}, angles{};
	bool brush = false, current = false;
	std::uint32_t contents = 1;
};
inline std::vector<Case> Make() {
	std::vector<Case> cases;
	Case base;
	for (auto contents : {0u,1u,0x04000000u,0x40000000u,0x44000000u,0xffffffffu}) {
		base.contents = contents;
		for (bool brush : {false,true}) for (bool current : {false,true}) {
			base.brush = brush; base.current = current; cases.push_back(base);
		}
	}
	base = {};
	for (unsigned bit = 0; bit < 32; ++bit) { base.contents = 1u << bit; cases.push_back(base); }
	base = {};
	for (int units = -3; units <= 66; ++units) for (float epsilon : {-0.01f,0.0f,0.01f}) {
		const float value = units * 8.0f + epsilon;
		Case c;
		c.maxs[0] = value; c.mins[0] = value - 16; cases.push_back(c);
		c = {}; c.mins[2] = -value; c.maxs[2] = 128; cases.push_back(c);
		c = {}; c.maxs[2] = value - 32; c.mins[2] = -128; cases.push_back(c);
	}
	// _ftol returns int64, while the original uses only its low signed dword.
	for (float value : {17179869184.0f,34359738368.0f,34359742464.0f,68719476736.0f,
		73786976294838206464.0f,3.4028234663852886e38f}) {
		Case c; c.maxs[0] = c.maxs[2] = value; c.mins[2] = -value; cases.push_back(c);
	}
	std::uint32_t random = 0x19990212;
	const auto next = [&random]() { random ^= random << 13; random ^= random >> 17; random ^= random << 5; return random; };
	for (unsigned i = 0; i < 2048; ++i) {
		Case c; c.brush = i & 1; c.current = i & 2; c.contents = next();
		for (unsigned axis = 0; axis < 3; ++axis) {
			c.mins[axis] = -static_cast<float>(next() % 4096) / 8.0f;
			c.maxs[axis] = static_cast<float>(next() % 4096) / 8.0f;
			c.origin[axis] = static_cast<float>(static_cast<int>(next() % 32768) - 16384) / 4.0f;
			c.angles[axis] = i % 4 ? static_cast<float>(next() % 360) : 0.0f;
		}
		cases.push_back(c);
	}
	return cases;
}
inline bool Calculate(const Case &c,fntech3::q3ihv::LinkBounds &out) {
	return fntech3::q3ihv::CalculateLinkBounds(c.mins.data(),c.maxs.data(),c.origin.data(),c.angles.data(),c.brush,c.contents,out);
}
struct Digest {
	std::uint64_t value = 14695981039346656037ull;
	void Byte(unsigned byte) { value = (value ^ (byte & 255u)) * 1099511628211ull; }
	void Word(std::uint32_t word) { for (unsigned i = 0; i < 4; ++i) Byte(word >> (i*8)); }
	void Add(const fntech3::q3ihv::LinkBounds &bounds) {
		Word(bounds.solid);
		for (const float *vec : {bounds.mins,bounds.maxs}) for (int i = 0; i < 3; ++i) {
			std::uint32_t bits; std::memcpy(&bits,vec+i,4); Word(bits);
		}
	}
};
}
#endif
