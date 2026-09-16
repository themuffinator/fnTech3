/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_RTCW_LIMBO_CHAT_HPP
#define FNTECH3_RTCW_LIMBO_CHAT_HPP
#include "../qcommon/q_shared.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace fntech3::rtcw::limbo {

// The reader checks VM memory a byte at a time. Build the new line before
// moving history so a malformed input cannot partially replace old messages.
template<class Reader>
void Append( rtcwLimboChat_t &history, Reader read ) {
	std::array<char, FNTECH3_RTCW_LIMBO_STORAGE> line{};
	std::size_t input = 0, output = 0, visible = 0;
	while ( visible < FNTECH3_RTCW_LIMBO_WIDTH && output + 1 < line.size() ) {
		const char first = read( input );
		if ( !first ) break;
		if ( first == '^' ) {
			const char next = read( input + 1 );
			if ( next && next != '^' ) {
				if ( output + 2 >= line.size() ) break;
				line[output++] = first;
				line[output++] = next;
				input += 2;
				continue;
			}
		}
		line[output++] = first;
		++input;
		++visible;
	}
	std::memmove( history.lines[1], history.lines[0],
		( FNTECH3_RTCW_LIMBO_LINES - 1 ) * FNTECH3_RTCW_LIMBO_STORAGE );
	std::memcpy( history.lines[0], line.data(), line.size() );
}

// The original UI contract writes exactly 140 bytes, padding short strings
// with zero. A full-width record need not have a trailing zero in this range.
inline bool Get( const rtcwLimboChat_t &history, std::intptr_t index, char *destination ) {
	if ( index < 0 || index >= FNTECH3_RTCW_LIMBO_LINES ) return false;
	const char *line = history.lines[index];
	std::size_t size = 0;
	while ( size < FNTECH3_RTCW_LIMBO_WIDTH && line[size] ) ++size;
	std::memcpy( destination, line, size );
	std::memset( destination + size, 0, FNTECH3_RTCW_LIMBO_WIDTH - size );
	return true;
}

} // namespace fntech3::rtcw::limbo
#endif
