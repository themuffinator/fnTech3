/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_STVEF_NETCHAN_HPP
#define FNTECH3_STVEF_NETCHAN_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <utility>

/* Elite Force Holomatch's per-packet scramble. Every sequenced datagram is
 * obscured from byte six onward after its header and payload are written;
 * receivers restore it before reading anything past the sequence number.
 * The first four bytes and the length seed a linear congruential sequence;
 * each byte is XORed with its sequence value, then swapped with a byte chosen
 * by the same value. Retail stvoyHM.exe 1.20 scrambles at 0x4246c0 and
 * restores inline at the top of Netchan_Process (0x4249c0). */
namespace fntech3::stvef::netchan {

inline constexpr std::size_t kScrambleStart = 6;
inline constexpr std::size_t kMaxPacketBytes = 1400;

namespace detail {

inline void Sequence( const std::uint8_t *data, std::size_t size,
	std::uint32_t *sequence ) noexcept {
	const std::uint32_t first = static_cast<std::uint32_t>( data[0] ) |
		static_cast<std::uint32_t>( data[1] ) << 8 |
		static_cast<std::uint32_t>( data[2] ) << 16 |
		static_cast<std::uint32_t>( data[3] ) << 24;
	std::uint32_t seed = ( first * 3u ) ^
		( static_cast<std::uint32_t>( size ) * 123u ) ^ 0x87243987u;
	for ( std::size_t i = 0; i < size; ++i ) {
		seed = seed * 69069u + 1u;
		sequence[i] = seed;
	}
}

// Half of the smallest all-ones mask covering the scrambled span, so every
// swap partner stays inside it.
inline std::uint32_t SwapMask( std::size_t size ) noexcept {
	std::uint32_t mask = 1;
	while ( mask < size - kScrambleStart ) {
		mask = mask * 2u + 1u;
	}
	return mask >> 1;
}

} // namespace detail

/* Both return false, leaving the packet unchanged, when it is larger than a
 * datagram may be. Packets of six bytes or fewer have nothing to scramble. */
inline bool Scramble( std::uint8_t *data, std::size_t size ) noexcept {
	if ( size <= kScrambleStart ) return true;
	if ( !data || size > kMaxPacketBytes ) return false;
	std::array<std::uint32_t, kMaxPacketBytes> sequence;
	detail::Sequence( data, size, sequence.data() );
	for ( std::size_t i = kScrambleStart; i < size; ++i ) {
		data[i] ^= static_cast<std::uint8_t>( sequence[i] );
	}
	const std::uint32_t mask = detail::SwapMask( size );
	for ( std::size_t i = kScrambleStart; i < size; ++i ) {
		std::swap( data[i], data[kScrambleStart + ( sequence[i] & mask )] );
	}
	return true;
}

inline bool Unscramble( std::uint8_t *data, std::size_t size ) noexcept {
	if ( size <= kScrambleStart ) return true;
	if ( !data || size > kMaxPacketBytes ) return false;
	std::array<std::uint32_t, kMaxPacketBytes> sequence;
	detail::Sequence( data, size, sequence.data() );
	const std::uint32_t mask = detail::SwapMask( size );
	for ( std::size_t i = size; i-- > kScrambleStart; ) {
		std::swap( data[i], data[kScrambleStart + ( sequence[i] & mask )] );
	}
	for ( std::size_t i = kScrambleStart; i < size; ++i ) {
		data[i] ^= static_cast<std::uint8_t>( sequence[i] );
	}
	return true;
}

} // namespace fntech3::stvef::netchan

#endif
