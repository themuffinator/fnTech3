/*
===========================================================================
Copyright (C) 2026 FnQL contributors

This file is part of FnQL.

FnQL is free software; you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation; either version 2 of the License, or (at your option) any later
version.
===========================================================================
*/

// steam_identity.hpp -- retail Quake Live Steam persona normalization

// Retail Quake Live owns the local player identity: the client registers
// "name" with the "UnnamedPlayer" default and then republishes the signed-in
// Steam persona over it, and it publishes the Steam country code the same way.
// The retail web UI never writes either key -- its only identity reference is
// the read-only qz_instance.playerName rendered through a color stripper -- so
// a rename is a Steam profile action, not an in-game setting.
//
// A persona is arbitrary profile text while every engine consumer publishes it
// through an infostring: the client through "name" and a listen server through
// the retail "<persona>'s Match" hostname. Keeping the normalization rules here
// means that boundary is shared and testable without a Steam session.

#ifndef FNQL_QCOMMON_STEAM_IDENTITY_HPP
#define FNQL_QCOMMON_STEAM_IDENTITY_HPP

#include <cstddef>
#include <cstdint>

namespace fnql::identity {

// The default retail registers for "name" and the value FnQL falls back to
// when a persona normalizes away to nothing.
inline constexpr char kUnnamedPlayer[] = "UnnamedPlayer";

// Info_ValidateKeyValue() rejects these three bytes. A persona carrying one
// would drop the entire "name" key out of the userinfo string rather than
// rename the player, so they can never reach a cvar that feeds userinfo.
[[nodiscard]] constexpr bool IsInfostringHostile( char value ) noexcept {
	return value == '\\' || value == '"' || value == ';';
}

// Control bytes and DEL do not survive the console, the config writer, or an
// infostring round trip. Everything else is retained byte for byte, including
// UTF-8 continuation bytes and retail color escapes: the retail UI strips
// color codes at render time instead of at the identity source.
[[nodiscard]] constexpr bool IsRetainedNameByte( char value ) noexcept {
	const unsigned char byte = static_cast<unsigned char>( value );

	return byte >= 0x20u && byte != 0x7fu && !IsInfostringHostile( value );
}

// Length of the UTF-8 sequence introduced by lead, or zero when lead is a
// continuation byte or an invalid lead byte.
[[nodiscard]] constexpr std::size_t Utf8SequenceLength(
	unsigned char lead ) noexcept {
	if ( lead < 0x80u ) {
		return 1u;
	}
	if ( ( lead & 0xe0u ) == 0xc0u ) {
		return 2u;
	}
	if ( ( lead & 0xf0u ) == 0xe0u ) {
		return 3u;
	}
	if ( ( lead & 0xf8u ) == 0xf0u ) {
		return 4u;
	}
	return 0u;
}

// Truncating a persona at a byte boundary can leave a partial UTF-8 sequence
// behind. Drop it so the published name is never a lone continuation run.
[[nodiscard]] inline std::size_t TrimPartialUtf8(
	const char *text, std::size_t length ) noexcept {
	std::size_t scan = length;

	if ( !text ) {
		return 0u;
	}
	while ( scan > 0u ) {
		const unsigned char byte = static_cast<unsigned char>( text[scan - 1u] );
		if ( ( byte & 0xc0u ) != 0x80u ) {
			// scan - 1 is a lead byte (or plain ASCII): keep the sequence only
			// when every one of its bytes survived the truncation.
			const std::size_t sequence = Utf8SequenceLength( byte );
			if ( sequence == 0u ) {
				return scan - 1u; // invalid lead byte, never publish it
			}
			return sequence <= length - ( scan - 1u ) ? length : scan - 1u;
		}
		--scan;
	}
	return 0u;
}

// A name may not end on a color escape. Q_IsColorString() ignores a trailing
// caret in isolation, but every rendered name is concatenated with surrounding
// text, and the caret then swallows the following character. Retail discards
// bare carets in stripColors() for the same reason, so a persona that ends in
// carets publishes and renders identically without them.
[[nodiscard]] inline std::size_t TrimTrailingColorEscape(
	const char *text, std::size_t length ) noexcept {
	if ( !text ) {
		return 0u;
	}
	while ( length > 0u && text[length - 1u] == '^' ) {
		--length;
	}
	return length;
}

[[nodiscard]] constexpr bool IsTrimmedSpace( char value ) noexcept {
	return value == ' ';
}

struct NameNormalization {
	// A persona-derived name survived normalization. When false the caller
	// publishes kUnnamedPlayer, matching the retail registration default.
	bool usable = false;
	// Normalization altered the persona text, so the published identity is not
	// a byte-for-byte copy of the Steam profile name.
	bool adjusted = false;
	std::size_t length = 0;
};

/*
Normalize a Steam persona name into a value that can be published through
"name" without corrupting userinfo, the config writer, or name rendering.

`capacity` is the full buffer size including the terminator, so callers pass
MAX_NAME_LENGTH and get the same bound the server applies to cl->name.
*/
inline NameNormalization NormalizePersonaName( const char *persona, char *out,
	std::size_t capacity ) noexcept {
	NameNormalization result;
	std::size_t length = 0;
	std::size_t leading = 0;
	bool truncated = false;

	if ( !out || capacity == 0u ) {
		return result;
	}
	out[0] = '\0';
	if ( !persona ) {
		return result;
	}

	// Leading spaces would make the published name look empty in every retail
	// name column, so they never enter the buffer.
	while ( persona[leading] != '\0' && IsTrimmedSpace( persona[leading] ) ) {
		++leading;
	}
	if ( leading != 0u ) {
		result.adjusted = true;
	}

	for ( const char *scan = persona + leading; *scan != '\0'; ++scan ) {
		if ( !IsRetainedNameByte( *scan ) ) {
			result.adjusted = true;
			continue;
		}
		if ( length + 1u >= capacity ) {
			truncated = true;
			break;
		}
		out[length++] = *scan;
	}
	if ( truncated ) {
		result.adjusted = true;
	}

	// Trailing space, partial-sequence, and half-escape trimming interact, so
	// settle them together instead of assuming an order.
	for ( ;; ) {
		const std::size_t before = length;

		while ( length > 0u && IsTrimmedSpace( out[length - 1u] ) ) {
			--length;
		}
		length = TrimPartialUtf8( out, length );
		length = TrimTrailingColorEscape( out, length );
		if ( length == before ) {
			break;
		}
		result.adjusted = true;
	}

	out[length] = '\0';
	result.length = length;
	result.usable = length != 0u;
	return result;
}

/*
Steam reports the account country as an ISO 3166-1 alpha-2 code, which retail
publishes verbatim through the "country" userinfo key. Anything that is not
exactly two letters is not a country code and is published as empty rather than
guessed at.
*/
inline bool NormalizeCountryCode( const char *country, char *out,
	std::size_t capacity ) noexcept {
	if ( !out || capacity < 3u ) {
		if ( out && capacity != 0u ) {
			out[0] = '\0';
		}
		return false;
	}
	out[0] = '\0';
	if ( !country ) {
		return false;
	}

	char code[2] = { '\0', '\0' };
	for ( std::size_t index = 0; index < 2u; ++index ) {
		char letter = country[index];
		if ( letter >= 'a' && letter <= 'z' ) {
			letter = static_cast<char>( letter - ( 'a' - 'A' ) );
		}
		if ( letter < 'A' || letter > 'Z' ) {
			return false;
		}
		code[index] = letter;
	}
	if ( country[2] != '\0' ) {
		return false;
	}

	out[0] = code[0];
	out[1] = code[1];
	out[2] = '\0';
	return true;
}

} // namespace fnql::identity

#endif
