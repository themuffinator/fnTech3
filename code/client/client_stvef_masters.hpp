/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_CLIENT_STVEF_MASTERS_HPP
#define FNTECH3_CLIENT_STVEF_MASTERS_HPP
#include <cstdint>

/* Holomatch's master server list. stvoyHM.exe 1.20 reads a getserversResponse
 * as text, not as Quake III's binary entries (CL_ServersResponsePacket
 * 0x40e6e0): after each backslash eight hex digits give the address, a byte
 * to every two, and four the port, as written in network order; an entry
 * counts only when another backslash follows it, and the list ends at
 * "\EOT", at the end of the packet or after 256 entries (0x40eb4e..0x40ebb8).
 * Retail reads a character that is not a hex digit as 0 and never looks for
 * the packet's end inside an entry; this stops at either instead. */
struct STVEFMasterEntry {
	std::uint8_t ip[4];
	std::uint16_t port;	// the value the master wrote, network order once BigShort'd
};

inline int CL_STVEFHexDigit( std::uint8_t c ) {
	if ( c >= '0' && c <= '9' ) return c - '0';
	if ( c >= 'a' && c <= 'f' ) return c - 'a' + 10;
	if ( c >= 'A' && c <= 'F' ) return c - 'A' + 10;
	return -1;
}

/* Parses [data, end) into at most `max` entries and returns how many. */
inline int CL_STVEFParseMasterList( const std::uint8_t *data, const std::uint8_t *end, STVEFMasterEntry *out,
	int max ) {
	enum { DIGITS = 12 };
	int count = 0;
	if ( !data || !end || !out || max <= 0 ) {
		return 0;
	}
	while ( data < end && *data != '\\' ) {
		data++;
	}
	while ( count < max && end - data > DIGITS + 1 ) {
		const std::uint8_t *at = data + 1;
		int nibbles[DIGITS];
		for ( int i = 0; i < DIGITS; i++ ) {
			nibbles[i] = CL_STVEFHexDigit( at[i] );
			if ( nibbles[i] < 0 ) {
				return count;
			}
		}
		if ( at[DIGITS] != '\\' ) {
			return count;
		}
		STVEFMasterEntry &entry = out[count++];
		for ( int i = 0; i < 4; i++ ) {
			entry.ip[i] = static_cast<std::uint8_t>( ( nibbles[i * 2] << 4 ) | nibbles[i * 2 + 1] );
		}
		entry.port = static_cast<std::uint16_t>( ( nibbles[8] << 12 ) | ( nibbles[9] << 8 ) | ( nibbles[10] << 4 ) |
			nibbles[11] );
		data = at + DIGITS;
		if ( end - data >= 4 && data[1] == 'E' && data[2] == 'O' && data[3] == 'T' ) {
			break;
		}
	}
	return count;
}

#endif /* FNTECH3_CLIENT_STVEF_MASTERS_HPP */
