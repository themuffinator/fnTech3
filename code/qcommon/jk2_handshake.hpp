/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_JK2_HANDSHAKE_HPP
#define FNTECH3_JK2_HANDSHAKE_HPP

#include <charconv>
#include <cstring>

/* Retail jk2mp.exe 1.02a sends bare getchallenge (0x40b507), replies
 * challengeResponse <challenge> (0x448887), and sends a bare
 * connectResponse (0x4491b6). It never names another protocol in either
 * reply. Reject an unrelated endpoint rather than allowing redirection. */
namespace fntech3::jk2::handshake {

inline bool ParseChallengeReply( int argc, const char *const *argv,
	bool expectedAddress, int &challenge ) noexcept {
	if ( !expectedAddress || argc != 2 || !argv || !argv[0] || !argv[1]
		|| std::strcmp( argv[0], "challengeResponse" ) || !*argv[1] ) return false;
	int parsed = 0;
	const char *end = argv[1] + std::strlen( argv[1] );
	const auto result = std::from_chars( argv[1], end, parsed );
	if ( result.ec != std::errc{} || result.ptr != end ) return false;
	challenge = parsed;
	return true;
}

inline bool AcceptConnectReply( int argc, const char *const *argv ) noexcept {
	return argc == 1 && argv && argv[0] && !std::strcmp( argv[0], "connectResponse" );
}

} // namespace fntech3::jk2::handshake
#endif
