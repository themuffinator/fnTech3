/*
===========================================================================
Copyright (C) 2026 fnTech3 contributors

This file is part of fnTech3 and is distributed under the terms of the GNU
General Public License, version 2 or (at your option) any later version.
===========================================================================
*/

#ifndef FNTECH3_RTCW_HANDSHAKE_HPP
#define FNTECH3_RTCW_HANDSHAKE_HPP

#include <cstddef>
#include <charconv>
#include <cstdio>
#include <cstring>

/*
 * RTCW's and ET's connectionless handshake does not use Quake III's token layout.
 *
 * Original RTCW MP answers getchallenge with "challengeResponse <challenge>",
 * adding sv_onlyVisibleClients as a second token whenever it is set, and its
 * client reads that token into clc.onlyVisibleClients. iortcw MP keeps that
 * second token and appends the echoed client nonce and its protocol:
 * "challengeResponse <challenge> <onlyVisibleClients> <nonce> <protocol>".
 * Original SP answers with the challenge alone; iortcw SP uses ioquake3's
 * "<challenge> <nonce> <protocol>". Original servers of both modes answer a
 * connect with a bare "connectResponse"; ioquake3-lineage servers echo the
 * challenge. Observed in RTCW-MP/RTCW-SP src/server/sv_client.c
 * (SV_GetChallenge, SV_DirectConnect) and src/client/cl_main.c
 * (CL_ConnectionlessPacket), and iortcw MP/SP code/server/sv_client.c.
 * Enemy Territory's original server and client use the same MP token layout,
 * including the optional onlyVisibleClients value, with protocol 84. The
 * shared helper is given that profile's protocol; it never selects Q3's 68.
 */
namespace fntech3::rtcw::handshake {

/* The game name a client puts after its nonce in getchallenge. iortcw servers
 * refuse any other name ("Game mismatch"); original servers ignore the token.
 * iortcw MP/SP code/qcommon/q_shared.h, GAMENAME_FOR_MASTER. */
inline constexpr const char *MultiplayerGameName = "wolfmp";
inline constexpr const char *SinglePlayerGameName = "wolfsp";

struct ChallengeReply {
	bool accepted = false;
	int challenge = 0;
	// MP only: the server asks its clients to hide players they cannot see.
	int onlyVisibleClients = 0;
	// The server's advertised protocol, or the profile's own when it names none.
	int protocol = 0;
	bool advertisedProtocol = false;
	bool echoedNonce = false;
};

[[nodiscard]] inline bool ParseInteger( const char *text, int &value ) noexcept {
	if ( !text || !*text ) return false;
	const char *end = text + std::strlen( text );
	const auto parsed = std::from_chars( text, end, value );
	return parsed.ec == std::errc{} && parsed.ptr == end;
}

/* argv[0] is "challengeResponse". A reply that echoes the client's nonce must
 * echo the one sent. One that echoes nothing cannot prove where it came from,
 * so it is only taken from the address the request went to, which is all an
 * original client can check. */
[[nodiscard]] inline ChallengeReply ParseChallengeReply( bool multiplayer,
	int argc, const char *const *argv, int clientNonce, int profileProtocol,
	bool fromExpectedAddress ) noexcept {
	ChallengeReply reply;
	if ( argc < 2 || !argv ) {
		return reply;
	}
	const int nonceIndex = multiplayer ? 3 : 2;
	const int protocolIndex = nonceIndex + 1;
	if ( !ParseInteger( argv[1], reply.challenge ) ) return reply;
	if ( multiplayer && argc > 2 && !ParseInteger( argv[2], reply.onlyVisibleClients ) ) return reply;
	reply.echoedNonce = argc > nonceIndex;
	int nonce = 0;
	if ( reply.echoedNonce ? !ParseInteger( argv[nonceIndex], nonce ) || nonce != clientNonce
		: !fromExpectedAddress ) {
		return reply;
	}
	reply.advertisedProtocol = argc > protocolIndex;
	reply.protocol = profileProtocol;
	if ( reply.advertisedProtocol && !ParseInteger( argv[protocolIndex], reply.protocol ) ) return reply;
	reply.accepted = true;
	return reply;
}

/* argv[0] is "connectResponse". Original servers send it bare; others echo the
 * challenge, which must then match. A protocol, when named, must be the
 * profile's own. */
[[nodiscard]] inline bool AcceptConnectReply( int argc, const char *const *argv,
	int challenge, int profileProtocol ) noexcept {
	if ( argc < 1 || !argv ) {
		return false;
	}
	int echoed = 0;
	if ( argc > 1 && ( !ParseInteger( argv[1], echoed ) || echoed != challenge ) ) {
		return false;
	}
	return argc <= 2 || ( ParseInteger( argv[2], echoed ) && echoed == profileProtocol );
}

/* The server's MP reply, in the layout of the client that asked: nothing but
 * the challenge (and a set onlyVisibleClients) for a client that sent no nonce,
 * as original servers answer, and iortcw's four tokens for one that did. A
 * trailing marker, when given, follows them. Returns snprintf's result. */
inline int FormatMultiplayerChallengeReply( char *buffer, std::size_t size,
	int challenge, int onlyVisibleClients, bool clientSentNonce, int nonce,
	int protocol, const char *marker ) noexcept {
	if ( clientSentNonce ) {
		return marker && *marker
			? std::snprintf( buffer, size, "challengeResponse %i %i %i %i %s",
				challenge, onlyVisibleClients, nonce, protocol, marker )
			: std::snprintf( buffer, size, "challengeResponse %i %i %i %i",
				challenge, onlyVisibleClients, nonce, protocol );
	}
	return onlyVisibleClients
		? std::snprintf( buffer, size, "challengeResponse %i %i", challenge,
			onlyVisibleClients )
		: std::snprintf( buffer, size, "challengeResponse %i", challenge );
}

} // namespace fntech3::rtcw::handshake

#endif
