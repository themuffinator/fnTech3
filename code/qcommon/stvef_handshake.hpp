/*
===========================================================================
Copyright (C) 2026 fnTech3 contributors

This file is part of fnTech3 and is distributed under the terms of the GNU
General Public License, version 2 or (at your option) any later version.
===========================================================================
*/

#ifndef FNTECH3_STVEF_HANDSHAKE_HPP
#define FNTECH3_STVEF_HANDSHAKE_HPP

#include <charconv>
#include <cstring>
#include "netchan_profile.h"

/*
 * Elite Force Holomatch's connectionless handshake.
 *
 * The retail 1.20 client asks with a bare "getchallenge" and its server
 * answers "challengeResponse <challenge>" (stvoyHM.exe SV_GetChallenge
 * 0x42ee80), after up to 20 seconds when it waits for the authorize server.
 * The retail server takes no arguments, so a client may add its own nonce.
 * Lilium Voyager's server echoes that nonce and its protocol,
 * "challengeResponse <challenge> <nonce> <protocol>", and accepts a missing
 * game name only in its legacy mode, so the name it expects is sent with it.
 * fnTech3's own server adds a trailing marker. The retail server answers the
 * plain-text connect with a bare "connectResponse"; Lilium's and fnTech3's
 * echo the challenge. The retail client matches the keyword alone (0x40edfe).
 */
namespace fntech3::stvef::handshake {

// Lilium Voyager's com_gamename, which its server compares with the second
// getchallenge token.
inline constexpr const char *GameName = "EliteForce";
// ioEF and Lilium Voyager speak protocol 26 among themselves and accept the
// retail protocol from a client that asks for it.
inline constexpr int IoEFProtocol = NETCHAN_IOEF_PROTOCOL_VERSION;

struct ChallengeReply {
	bool accepted = false;
	int challenge = 0;
	// The protocol the server named, or 0 when it named none.
	int advertisedProtocol = 0;
	bool echoedNonce = false;
};

[[nodiscard]] inline bool ParseInteger( const char *text, int &value ) noexcept {
	if ( !text || !*text ) return false;
	const char *end = text + std::strlen( text );
	const auto parsed = std::from_chars( text, end, value );
	return parsed.ec == std::errc{} && parsed.ptr == end;
}

/* argv[0] is "challengeResponse". A reply that echoes a nonce must echo ours;
 * a bare one proves nothing, so it is taken only from the address the request
 * went to. The retail client accepts it from anywhere (0x40ed89). */
[[nodiscard]] inline ChallengeReply ParseChallengeReply( int argc,
	const char *const *argv, int clientNonce, bool fromExpectedAddress ) noexcept {
	ChallengeReply reply;
	if ( argc < 2 || !argv || !argv[1] ) {
		return reply;
	}
	if ( !ParseInteger( argv[1], reply.challenge ) ) return reply;
	reply.echoedNonce = argc > 2;
	int echoed = 0;
	if ( reply.echoedNonce ? !ParseInteger( argv[2], echoed ) || echoed != clientNonce : !fromExpectedAddress ) {
		return reply;
	}
	if ( argc > 3 && !ParseInteger( argv[3], reply.advertisedProtocol ) ) return reply;
	reply.accepted = true;
	return reply;
}

/* A bare retail reply selects 24. Protocol 26 is selected only after the
 * server proves the nonce and advertises that exact contract. Unknown
 * protocols must not accidentally select Q3 framing through their number. */
[[nodiscard]] inline int SelectProtocol( const ChallengeReply &reply ) noexcept {
	if ( !reply.accepted ) return 0;
	if ( reply.advertisedProtocol == 0 || reply.advertisedProtocol == NETCHAN_STVEF_HM_PROTOCOL_VERSION )
		return NETCHAN_STVEF_HM_PROTOCOL_VERSION;
	return reply.echoedNonce && reply.advertisedProtocol == IoEFProtocol ? IoEFProtocol : 0;
}

/* argv[0] is "connectResponse". A bare reply is the retail server's; one that
 * echoes a challenge must echo ours, and a protocol, when named, must be the
 * one asked for. */
[[nodiscard]] inline bool AcceptConnectReply( int argc, const char *const *argv,
	int challenge, int requestedProtocol ) noexcept {
	if ( argc < 1 || !argv ) {
		return false;
	}
	if ( requestedProtocol != NETCHAN_STVEF_HM_PROTOCOL_VERSION && requestedProtocol != IoEFProtocol ) return false;
	if ( requestedProtocol == IoEFProtocol && argc < 2 ) return false;
	int echoed = 0;
	if ( argc > 1 && ( !ParseInteger( argv[1], echoed ) || echoed != challenge ) ) {
		return false;
	}
	return argc <= 2 || ( ParseInteger( argv[2], echoed ) && echoed == requestedProtocol );
}

} // namespace fntech3::stvef::handshake

#endif
