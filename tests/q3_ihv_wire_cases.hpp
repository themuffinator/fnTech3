/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
/* Synthetic inputs shared by the optional original-binary oracle and CI.
 * No codec field table is consulted: every storage bit is varied, including
 * padding. None of these inputs comes from game assets or network traffic. */
#ifndef FNTECH3_Q3_IHV_WIRE_CASES_HPP
#define FNTECH3_Q3_IHV_WIRE_CASES_HPP
#include <array>
#include <cstddef>
#include <cstdint>

namespace ihv_cases {
inline std::uint32_t Next(std::uint32_t &seed) { seed = seed * 1664525u + 1013904223u; return seed; }
template<std::size_t N> struct Case { std::array<std::uint8_t,N> from{}, to{}; bool force = true, remove = false; };
template<std::size_t N> constexpr std::size_t BitCases = (N - (N == 164 ? 2 : 0)) * 8;
inline constexpr std::uint32_t FloatValues[] = {0,0x80000000u,0x3f800000u,0xbf800000u,0x7f800000u,0xff800000u,
	0x7fc12345u,0x7f812345u,1,0x80000001u,0x7f7fffffu,0xff7fffffu,0x43340000u,0xc3340000u,0x43b40000u,0xc3b40000u};
template<std::size_t N> constexpr std::size_t Count = 1 + BitCases<N> + 128 + (N == 14 ? 0 : 256) + (N == 164 ? 9 : 0);
template<std::size_t N> Case<N> Make(std::size_t index) {
	Case<N> c;
	if (index > 0 && index <= BitCases<N>) {
		const std::size_t bit = index - 1; c.to[bit/8 + (N == 164 ? 2 : 0)] = static_cast<std::uint8_t>(1u << (bit & 7));
	} else if (index > BitCases<N> && index <= BitCases<N> + 128) {
		std::uint32_t seed = static_cast<std::uint32_t>(index * 65537 + N);
		for (std::size_t i = 0; i < N; ++i) { c.from[i] = static_cast<std::uint8_t>(Next(seed) >> 24); c.to[i] = c.from[i]; }
		for (std::size_t i = 0; i < N; i += 3) c.to[i] = static_cast<std::uint8_t>(Next(seed) >> 24);
	} else if constexpr (N != 14) {
		if (index > BitCases<N> + 128 && index <= BitCases<N> + 128 + 256) {
			const auto pair = index - BitCases<N> - 129;
			const unsigned offset = N == 164 ? 20 : 80;
			for(unsigned byte=0;byte<4;++byte) {
				c.from[offset+byte]=static_cast<std::uint8_t>(FloatValues[pair/16]>>(8*byte));
				c.to[offset+byte]=static_cast<std::uint8_t>(FloatValues[pair%16]>>(8*byte));
			}
		}
	}
	if constexpr (N == 164) {
		c.from[0] = c.to[0] = 7; c.from[1] = c.to[1] = 0;
		if (index == Count<N>-9) c.force = false;
		if (index == Count<N>-8) c.remove = true;
		if (index == Count<N>-7) { c.from[144] = 23; c.from[148] = 7; c.to = c.from; c.to[144] = 7; }
		if (index == Count<N>-6) { c.from[144] = 7; c.from[148] = 23; c.to = c.from; c.to[3] = 1; }
		if (index == Count<N>-5) { c.from[0]=0; c.to[0]=1; }
		if (index == Count<N>-4) { c.from[0]=0; c.to[0]=255; c.to[1]=3; }
		if (index == Count<N>-3) c.from[0]=8;
		if (index == Count<N>-2 || index == Count<N>-1) { c.from[0]=c.to[0]=255; c.from[1]=c.to[1]=3; c.force=false; }
		if (index == Count<N>-1) c.remove=true;
	}
	return c;
}
struct Digest {
	std::uint64_t hash = 14695981039346656037ull;
	void Byte(std::uint8_t value) { hash = (hash ^ value) * 1099511628211ull; }
	void Word(std::uint32_t value) { for (int i=0; i<4; ++i) Byte(static_cast<std::uint8_t>(value >> (8*i))); }
	void Bytes(const void *data, std::size_t count) { const auto *p = static_cast<const std::uint8_t *>(data); for(std::size_t i=0;i<count;++i) Byte(p[i]); }
};
} // namespace ihv_cases
#endif
