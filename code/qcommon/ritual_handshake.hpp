/*
===========================================================================
Copyright (C) 2026 fnTech3 contributors

This file is part of fnTech3 and is distributed under the terms of the GNU
General Public License, version 2 or (at your option) any later version.
===========================================================================
*/

#ifndef FNTECH3_RITUAL_HANDSHAKE_HPP
#define FNTECH3_RITUAL_HANDSHAKE_HPP

#include <cstdlib>

/*
 * The connectionless handshake of Ritual's engine: Heavy Metal: F.A.K.K.2 and
 * American McGee's Alice.
 *
 * The retail client asks with a bare "getchallenge" (fakk2.exe 1.02
 * 0x004216d3) and its server answers "challengeResponse <challenge>"
 * (0x00443236). The client reads the first argument alone and takes the
 * address it came from (0x00421a6d), then prints its connect as plain text,
 * `connect "<userinfo>"`, with protocol 1, its qport and the challenge in the
 * userinfo (0x004215ef). The server reads that line with no Huffman stage
 * (0x00447100) and answers a bare "connectResponse" (0x004439fd), which the
 * client takes only from the server's address (0x00421ab5..0x00421b2c).
 * fnTech3's server echoes a client's nonce and names its protocol when the
 * request carried a nonce, as ioquake3's does.
 */
namespace fntech3::ritual::handshake {

struct ChallengeReply {
	bool accepted = false;
	int challenge = 0;
	// The protocol the server named, or 0 when it named none.
	int advertisedProtocol = 0;
};

/* argv[0] is "challengeResponse". A reply that echoes a nonce must echo ours;
 * a bare one proves nothing, so it is taken only from the address the request
 * went to. */
[[nodiscard]] inline ChallengeReply ParseChallengeReply( int argc,
	const char *const *argv, int clientNonce, bool fromExpectedAddress ) noexcept {
	ChallengeReply reply;
	if ( argc < 2 || !argv || !argv[1] ) {
		return reply;
	}
	reply.challenge = std::atoi( argv[1] );
	const bool echoed = argc > 2 && argv[2];
	if ( echoed ? std::atoi( argv[2] ) != clientNonce : !fromExpectedAddress ) {
		return reply;
	}
	if ( argc > 3 && argv[3] ) {
		reply.advertisedProtocol = std::atoi( argv[3] );
	}
	reply.accepted = true;
	return reply;
}

/* argv[0] is "connectResponse": the retail server's bare keyword, or one that
 * echoes the challenge, which must then be ours. */
[[nodiscard]] inline bool AcceptConnectReply( int argc, const char *const *argv,
	int challenge ) noexcept {
	if ( argc < 1 || !argv ) {
		return false;
	}
	return argc < 2 || ( argv[1] && std::atoi( argv[1] ) == challenge );
}

} // namespace fntech3::ritual::handshake

#endif
