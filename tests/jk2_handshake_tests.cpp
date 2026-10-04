/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#include "../code/qcommon/jk2_handshake.hpp"
#include <cassert>
#include <limits>
#include <initializer_list>

int main() {
	using namespace fntech3::jk2::handshake;
	int challenge = 43;
	const char *reply[] = { "challengeResponse", "-2147483648" };
	assert( ParseChallengeReply( 2, reply, true, challenge )
		&& challenge == std::numeric_limits<int>::min() );
	reply[1] = "2147483647";
	assert( ParseChallengeReply( 2, reply, true, challenge )
		&& challenge == std::numeric_limits<int>::max() );
	for ( const char *bad : { "", "2147483648", "-2147483649", "23tail", " 12", "+2" } ) {
		reply[1] = bad;
		assert( !ParseChallengeReply( 2, reply, true, challenge )
			&& challenge == std::numeric_limits<int>::max() );
	}
	reply[1] = "23";
	assert( !ParseChallengeReply( 2, reply, false, challenge ) );
	assert( !ParseChallengeReply( 0, nullptr, true, challenge ) );
	assert( !ParseChallengeReply( 1, reply, true, challenge ) );
	assert( !ParseChallengeReply( 3, reply, true, challenge ) );
	const char *connect[] = { "connectResponse", "23" };
	assert( AcceptConnectReply( 1, connect ) );
	assert( !AcceptConnectReply( 2, connect ) );
	assert( !AcceptConnectReply( 0, nullptr ) );
	connect[0] = "challengeResponse";
	assert( !AcceptConnectReply( 1, connect ) );
}
