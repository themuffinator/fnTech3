/*
===========================================================================
Copyright (C) 2026 fnTech3 contributors

This file is part of fnTech3 and is distributed under the terms of the GNU
General Public License, version 2 or (at your option) any later version.
===========================================================================
*/

#ifndef FNTECH3_STVEF_HANDSHAKE_HPP
#define FNTECH3_STVEF_HANDSHAKE_HPP

#include <cstdlib>

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
inline constexpr int IoEFProtocol = 26;

struct ChallengeReply {
	bool accepted = false;
	int challenge = 0;
	// The protocol the server named, or 0 when it named none.
	int advertisedProtocol = 0;
	bool echoedNonce = false;
};

/* argv[0] is "challengeResponse". A reply that echoes a nonce must echo ours;
 * a bare one proves nothing, so it is taken only from the address the request
 * went to. The retail client accepts it from anywhere (0x40ed89). */
[[nodiscard]] inline ChallengeReply ParseChallengeReply( int argc,
	const char *const *argv, int clientNonce, bool fromExpectedAddress ) noexcept {
	ChallengeReply reply;
	if ( argc < 2 || !argv || !argv[1] ) {
		return reply;
	}
	reply.challenge = std::atoi( argv[1] );
	reply.echoedNonce = argc > 2 && argv[2];
	if ( reply.echoedNonce ? std::atoi( argv[2] ) != clientNonce : !fromExpectedAddress ) {
		return reply;
	}
	if ( argc > 3 && argv[3] ) {
		reply.advertisedProtocol = std::atoi( argv[3] );
	}
	reply.accepted = true;
	return reply;
}

/* A server that names a protocol must name one that still lets this client
 * speak the retail wire: the retail protocol itself, or ioEF's, whose servers
 * fall back to it. */
[[nodiscard]] inline bool ServerAcceptsRetailProtocol( int advertisedProtocol,
	int retailProtocol ) noexcept {
	return advertisedProtocol == 0 || advertisedProtocol == retailProtocol ||
		advertisedProtocol == IoEFProtocol;
}

/* argv[0] is "connectResponse". A bare reply is the retail server's; one that
 * echoes a challenge must echo ours, and a protocol, when named, must be the
 * one asked for. */
[[nodiscard]] inline bool AcceptConnectReply( int argc, const char *const *argv,
	int challenge, int retailProtocol ) noexcept {
	if ( argc < 1 || !argv ) {
		return false;
	}
	if ( argc > 1 && ( !argv[1] || std::atoi( argv[1] ) != challenge ) ) {
		return false;
	}
	return argc <= 2 || ( argv[2] && std::atoi( argv[2] ) == retailProtocol );
}

} // namespace fntech3::stvef::handshake

#endif
