/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
// Elite Force Holomatch client handshake: retail, Lilium Voyager and fnTech3
// server replies, and the checks each must pass.
#include "stvef_handshake.hpp"

#include <cstdio>
#include <initializer_list>

namespace {

int failures = 0;

#define CHECK( condition ) do { \
	if ( !( condition ) ) { \
		std::fprintf( stderr, "%s:%d: CHECK( %s ) failed\n", __FILE__, __LINE__, #condition ); \
		++failures; \
	} \
} while ( 0 )

using namespace fntech3::stvef::handshake;

void TestChallengeReplies() {
	constexpr int nonce = 4242;
	// Retail: the challenge alone, taken only from the address asked.
	const char *retail[] = { "challengeResponse", "-1795323906" };
	auto reply = ParseChallengeReply( 2, retail, nonce, true );
	CHECK( reply.accepted && reply.challenge == -1795323906 && !reply.echoedNonce );
	CHECK( reply.advertisedProtocol == 0 );
	CHECK( SelectProtocol( reply ) == 24 );
	CHECK( !ParseChallengeReply( 2, retail, nonce, false ).accepted );

	// Lilium Voyager echoes the nonce and names its own protocol 26.
	const char *lilium[] = { "challengeResponse", "77", "4242", "26" };
	reply = ParseChallengeReply( 4, lilium, nonce, false );
	CHECK( reply.accepted && reply.challenge == 77 && reply.echoedNonce );
	CHECK( reply.advertisedProtocol == 26 );
	CHECK( SelectProtocol( reply ) == 26 );
	const char *wrongNonce[] = { "challengeResponse", "77", "4243", "26" };
	CHECK( !ParseChallengeReply( 4, wrongNonce, nonce, true ).accepted );

	// fnTech3 names the retail protocol and appends a marker.
	const char *ours[] = { "challengeResponse", "99", "4242", "24", "fnql" };
	reply = ParseChallengeReply( 5, ours, nonce, false );
	CHECK( reply.accepted && reply.challenge == 99 && reply.advertisedProtocol == 24 );
	CHECK( SelectProtocol( reply ) == 24 );
	CHECK( SelectProtocol( {} ) == 0 );
	reply.advertisedProtocol = 68;
	CHECK( SelectProtocol( reply ) == 0 );
	reply.advertisedProtocol = 26; reply.echoedNonce = false;
	CHECK( SelectProtocol( reply ) == 0 );

	const char *empty[] = { "challengeResponse" };
	CHECK( !ParseChallengeReply( 1, empty, nonce, true ).accepted );
	CHECK( !ParseChallengeReply( 2, nullptr, nonce, true ).accepted );
	for ( const char *bad : { "", "wrong", "7junk", "2147483648", "-2147483649", "9999999999999999999999" } ) {
		const char *badChallenge[] = { "challengeResponse", bad };
		const char *badEcho[] = { "challengeResponse", "77", bad, "26" };
		const char *badProtocol[] = { "challengeResponse", "77", "4242", bad };
		CHECK( !ParseChallengeReply( 2, badChallenge, nonce, true ).accepted );
		CHECK( !ParseChallengeReply( 4, badEcho, nonce, true ).accepted );
		CHECK( !ParseChallengeReply( 4, badProtocol, nonce, true ).accepted );
	}
	const char *boundary[] = { "challengeResponse", "-2147483648", "2147483647", "26" };
	CHECK( SelectProtocol( ParseChallengeReply( 4, boundary, 2147483647, false ) ) == 26 );
}

void TestConnectReplies() {
	const char *retail[] = { "connectResponse" };
	CHECK( AcceptConnectReply( 1, retail, 5, 24 ) );
	const char *echoed[] = { "connectResponse", "5" };
	CHECK( AcceptConnectReply( 2, echoed, 5, 24 ) );
	CHECK( !AcceptConnectReply( 2, echoed, 6, 24 ) );
	const char *withProtocol[] = { "connectResponse", "5", "24" };
	CHECK( AcceptConnectReply( 3, withProtocol, 5, 24 ) );
	const char *otherProtocol[] = { "connectResponse", "5", "26" };
	CHECK( !AcceptConnectReply( 3, otherProtocol, 5, 24 ) );
	CHECK( AcceptConnectReply( 3, otherProtocol, 5, 26 ) );
	CHECK( AcceptConnectReply( 2, echoed, 5, 26 ) );
	CHECK( !AcceptConnectReply( 1, retail, 5, 26 ) );
	CHECK( !AcceptConnectReply( 3, withProtocol, 5, 26 ) );
	CHECK( !AcceptConnectReply( 2, echoed, 5, 68 ) );
	CHECK( !AcceptConnectReply( 0, retail, 5, 24 ) );
	const char *badChallenge[] = { "connectResponse", "5junk", "26" };
	const char *badProtocol[] = { "connectResponse", "5", "26junk" };
	const char *nullChallenge[] = { "connectResponse", nullptr, "26" };
	CHECK( !AcceptConnectReply( 3, badChallenge, 5, 26 ) );
	CHECK( !AcceptConnectReply( 3, badProtocol, 5, 26 ) );
	CHECK( !AcceptConnectReply( 3, nullChallenge, 5, 26 ) );
}

} // namespace

int main() {
	TestChallengeReplies();
	TestConnectReplies();
	if ( failures ) {
		std::fprintf( stderr, "%d Elite Force client wire check(s) failed\n", failures );
		return 1;
	}
	return 0;
}
