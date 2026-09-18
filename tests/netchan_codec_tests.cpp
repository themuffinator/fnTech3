#include "netchan_codec.hpp"
#include "stvef_netchan.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace {

// These bodies are the deterministic vectors tied to the retail-mapped
// CL/SV netchan routines in the QLSRP evidence corpus.  FnQL exercises them
// through its own bounded codec helper rather than importing the source loop.

template<std::size_t Size>
bool CheckVector( const std::array<std::uint8_t, Size> &clear,
	const std::array<std::uint8_t, Size> &expected, std::size_t begin,
	std::uint8_t key, std::string_view command ) {
	auto encoded = clear;

	fnql::ApplyNetchanXor( encoded.data(), encoded.size(), begin, key, command );
	if ( encoded != expected ) {
		return false;
	}

	fnql::ApplyNetchanXor( encoded.data(), encoded.size(), begin, key, command );
	return encoded == clear;
}

// Elite Force Holomatch packet scramble. The vectors follow the retail
// routine at stvoyHM.exe 0x4246c0: bytes before six are never touched.
int CheckSTVEFScramble() {
	using namespace fntech3::stvef::netchan;
	std::array<std::uint8_t, 22> packet = {
		0x2a, 0x00, 0x00, 0x00, 0x34, 0x12, 0x10, 0x11, 0x12, 0x13, 0x14,
		0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f
	};
	const auto clear = packet;
	constexpr std::array<std::uint8_t, 22> scrambled = {
		0x2a, 0x00, 0x00, 0x00, 0x34, 0x12, 0xba, 0x32, 0x1a, 0xc2, 0x02,
		0x52, 0x42, 0xca, 0x5a, 0xfa, 0x7a, 0x7a, 0x32, 0x8a, 0x32, 0x22
	};
	if ( !Scramble( packet.data(), packet.size() ) || packet != scrambled ) return 1;
	if ( !Unscramble( packet.data(), packet.size() ) || packet != clear ) return 2;

	std::array<std::uint8_t, 17> text = {
		0xff, 0xfe, 0x01, 0x80, 0x00, 0x00, 'E', 'l', 'i', 't', 'e', ' ',
		'F', 'o', 'r', 'c', 'e'
	};
	constexpr std::array<std::uint8_t, 17> scrambledText = {
		0xff, 0xfe, 0x01, 0x80, 0x00, 0x00, 0x32, 0x22, 0x9f, 0x8b, 0x51,
		0x85, 0x6b, 0x54, 0xad, 0xa5, 0x64
	};
	if ( !Scramble( text.data(), text.size() ) || text != scrambledText ) return 3;

	// Every length a datagram can have round-trips; short packets are left
	// alone and oversized ones are refused unchanged.
	std::array<std::uint8_t, kMaxPacketBytes + 1> large{};
	std::uint32_t state = 12345;
	for ( auto &value : large ) {
		state = state * 1103515245u + 12345u;
		value = static_cast<std::uint8_t>( state >> 16 );
	}
	const auto original = large;
	for ( std::size_t size = 0; size <= kMaxPacketBytes; ++size ) {
		if ( !Scramble( large.data(), size ) ) return 4;
		if ( size <= kScrambleStart && large != original ) return 5;
		if ( !Unscramble( large.data(), size ) || large != original ) return 6;
	}
	if ( Scramble( large.data(), large.size() ) || large != original ) return 7;
	return 0;
}

} // namespace

int main() {
	constexpr std::array<std::uint8_t, 16> clientClear = {
		0x04, 0x03, 0x02, 0x01, 0x0d, 0x0c, 0x0b, 0x0a,
		0x02, 0x00, 0x00, 0x00, 0x77, 0x02, 0x01, 0x05
	};
	constexpr std::array<std::uint8_t, 16> clientEncoded = {
		0x04, 0x03, 0x02, 0x01, 0x0d, 0x0c, 0x0b, 0x0a,
		0x02, 0x00, 0x00, 0x00, 0x45, 0xf4, 0x94, 0x52
	};
	constexpr std::array<std::uint8_t, 17> clientSanitizedClear = {
		0x04, 0x03, 0x02, 0x01, 0x0d, 0x0c, 0x0b, 0x0a,
		0x02, 0x00, 0x00, 0x00, 0x10, 0x20, 0x30, 0x40, 0x05
	};
	constexpr std::array<std::uint8_t, 17> clientSanitizedEncoded = {
		0x04, 0x03, 0x02, 0x01, 0x0d, 0x0c, 0x0b, 0x0a,
		0x02, 0x00, 0x00, 0x00, 0x6d, 0x01, 0x6b, 0x47, 0x2c
	};
	constexpr std::array<std::uint8_t, 8> serverClear = {
		0x03, 0x00, 0x00, 0x00, 0x55, 0xaa, 0x99, 0x06
	};
	constexpr std::array<std::uint8_t, 8> serverEncoded = {
		0x03, 0x00, 0x00, 0x00, 0x0e, 0x33, 0x79, 0x00
	};
	constexpr std::array<std::uint8_t, 9> serverSanitizedClear = {
		0x03, 0x00, 0x00, 0x00, 0x10, 0x20, 0x30, 0x40, 0x06
	};
	constexpr std::array<std::uint8_t, 9> serverSanitizedEncoded = {
		0x03, 0x00, 0x00, 0x00, 0x16, 0x7a, 0x1b, 0x37, 0x5f
	};
	constexpr char clientCommand[] = "abc";
	constexpr char serverCommand[] = "say";
	constexpr char clientSanitizedCommand[] = {
		'%', static_cast<char>( 0x80 ), 'z', '\0'
	};
	constexpr char serverSanitizedCommand[] = {
		'%', static_cast<char>( 0x80 ), 'q', '\0'
	};

	if ( !CheckVector( clientClear, clientEncoded, 12, 0x53,
		fnql::NetchanCommandView( clientCommand, sizeof( clientCommand ) ) ) ) {
		return 1;
	}
	if ( !CheckVector( clientSanitizedClear, clientSanitizedEncoded, 12, 0x53,
		fnql::NetchanCommandView( clientSanitizedCommand,
			sizeof( clientSanitizedCommand ) ) ) ) {
		return 2;
	}
	if ( !CheckVector( serverClear, serverEncoded, 4, 0x28,
		fnql::NetchanCommandView( serverCommand, sizeof( serverCommand ) ) ) ) {
		return 3;
	}
	if ( !CheckVector( serverSanitizedClear, serverSanitizedEncoded, 4, 0x28,
		fnql::NetchanCommandView( serverSanitizedCommand,
			sizeof( serverSanitizedCommand ) ) ) ) {
		return 4;
	}

	// Empty and unterminated fixed buffers must remain bounded and reversible.
	std::array<std::uint8_t, 4096> emptyCommandPayload{};
	const auto emptyClear = emptyCommandPayload;
	fnql::ApplyNetchanXor( emptyCommandPayload.data(), emptyCommandPayload.size(),
		4, 0xa5, {} );
	fnql::ApplyNetchanXor( emptyCommandPayload.data(), emptyCommandPayload.size(),
		4, 0xa5, {} );
	if ( emptyCommandPayload != emptyClear ) {
		return 5;
	}

	constexpr std::array<char, 3> unterminated = { 'a', 'b', 'c' };
	if ( fnql::NetchanCommandView( unterminated.data(), unterminated.size() ).size() !=
		unterminated.size() ) {
		return 6;
	}

	if ( const int result = CheckSTVEFScramble() ) {
		return 100 + result;
	}

	return 0;
}
