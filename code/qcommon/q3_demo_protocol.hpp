/*
 * Copyright (C) 2026 fnTech3 contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef FNTECH3_Q3_DEMO_PROTOCOL_HPP
#define FNTECH3_Q3_DEMO_PROTOCOL_HPP

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace fntech3::q3 {

// These are message formats, not a claim that the corresponding game-module
// or live-server contract is implemented. See the point-release gate ledger.
constexpr bool RawDemoProtocol( int protocol ) noexcept {
	return protocol == 43 || protocol == 45 || protocol == 46 || protocol == 48;
}

constexpr bool DemoProtocol( int protocol ) noexcept {
	return RawDemoProtocol( protocol ) || protocol == 66 || protocol == 67 ||
		protocol == 68 || protocol == 71;
}

// id's 1.27 pak4 contains protocol-46 recordings renamed to .dm_48. The
// structured gamestate is authoritative within the raw Q3 demo family.
constexpr bool RawDemoContainer( int extensionProtocol, int messageProtocol ) noexcept {
	return RawDemoProtocol( messageProtocol ) &&
		( extensionProtocol == 0 || extensionProtocol == messageProtocol ||
		  ( extensionProtocol == 48 && messageProtocol == 46 ) );
}

// Read only structured configstrings from the opening raw gamestate. Looking
// for a substring anywhere in a packet lets a hostname, chat message or binary
// baseline choose the decoder. Zero means absent, malformed or unsupported.
inline int DetectRawDemoProtocol( const std::uint8_t *data, std::size_t size ) noexcept {
	if ( !data ) return 0;
	const auto detect = [&]( std::size_t prefix ) -> int {
		if ( size <= prefix + 5 || data[prefix] != 2 ) return 0; // svc_gamestate
		std::size_t at = prefix + 5; // serverCommandSequence
		while ( at < size && data[at++] == 3 ) { // svc_configstring
			if ( size - at < 2 ) break;
			const unsigned index = data[at] | ( unsigned( data[at + 1] ) << 8 );
			at += 2;
			const std::size_t start = at;
			while ( at < size && data[at] ) ++at;
			if ( at == size ) break;
			const std::string_view info( reinterpret_cast<const char *>( data + start ), at - start );
			++at;
			if ( index != 0 ) continue; // CS_SERVERINFO
			int found = 0;
			std::size_t keyStart = !info.empty() && info[0] == '\\' ? 1 : 0;
			while ( keyStart < info.size() ) {
				const auto keyEnd = info.find( '\\', keyStart );
				if ( keyEnd == std::string_view::npos ) return 0;
				const auto valueEnd = info.find( '\\', keyEnd + 1 );
				const auto end = valueEnd == std::string_view::npos ? info.size() : valueEnd;
				if ( info.substr( keyStart, keyEnd - keyStart ) == "protocol" ) {
					const auto value = info.substr( keyEnd + 1, end - keyEnd - 1 );
					if ( found || value.size() != 2 || value[0] < '0' || value[0] > '9' ||
						value[1] < '0' || value[1] > '9' ) return 0;
					found = ( value[0] - '0' ) * 10 + value[1] - '0';
					if ( !RawDemoProtocol( found ) || ( found >= 46 ) != ( prefix == 4 ) ) return 0;
				}
				keyStart = end + 1;
			}
			return found;
		}
		return 0;
	};
	const int early = detect( 0 );
	const int acknowledged = detect( 4 );
	return early && acknowledged ? 0 : early ? early : acknowledged;
}

} // namespace fntech3::q3
#endif
