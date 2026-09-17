/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_SERVER_WOLFET_CLIENT_HPP
#define FNTECH3_SERVER_WOLFET_CLIENT_HPP

/* Engine-internal ET server state. These records are never handed to a module
 * and never widen another profile's client_t. Contract observations:
 * docs/fntech3/2026-09-17-wolfet-contracts.md.
 * Reference: id-Software/Enemy-Territory 40342a9e3690cb5b627a433d4d5cbf30e3c57698
 * (sv_ccmds.c temporary bans, sv_game.c/sv_net_chan.c binary messages). */

/* Include after server.h: the shared server/game headers have no guards. */
#include "../compat/wolfet_game_abi.h"

#include <array>
#include <cstring>

namespace fntech3 {
namespace wolfet {
namespace server {

// ET sizes MAX_TEMPBAN_ADDRESSES with its own MAX_CLIENTS.
constexpr int MaxTempBans = FNTECH3_WOLFET_MAX_CLIENTS;
// ET's q_shared.h MAX_BINARY_MESSAGE.
constexpr int MaxBinaryMessage = 32768;

struct TempBan {
	netadr_t address{};
	int endTime = 0;
};

struct BinaryMessage {
	int length = 0;
	bool overflowed = false;
	std::array<char, MaxBinaryMessage> data{};
};

/* ET's redirected-download state. The original keeps these on its client_t;
 * they stay engine-internal here so no other profile's client widens. */
struct WebDownload {
	bool capable = false;      // client advertised cl_wwwDownload
	bool redirected = false;   // a redirect message was sent for this file
	bool transferring = false; // client acknowledged and is fetching
	bool fallback = false;     // last redirect failed, use the block transfer
	unsigned notify = 0;       // one-shot console notices, see NotifyAll
};

enum { NotifyRedirect = 0x1, NotifyBegin = 0x2, NotifyAll = 0x3 };

struct State {
	std::array<TempBan, MaxTempBans> bans{};
	std::array<BinaryMessage, FNTECH3_WOLFET_MAX_CLIENTS> messages{};
	std::array<WebDownload, FNTECH3_WOLFET_MAX_CLIENTS> downloads{};
};

inline State &Storage() {
	static State state;
	return state;
}

// The message buffers are large; clear them in place rather than through a
// temporary that would not fit on a server thread's stack.
inline void Reset() {
	auto &state = Storage();
	state.bans.fill( TempBan{} );
	for ( auto &message : state.messages ) {
		message.length = 0;
		message.overflowed = false;
		message.data.fill( '\0' );
	}
	state.downloads.fill( WebDownload{} );
}

inline WebDownload *Download( int client ) {
	if ( !Com_GameIsWolfET() || !svs.clients || client < 0 || client >= sv_maxclients->integer
		|| client >= FNTECH3_WOLFET_MAX_CLIENTS ) {
		return nullptr;
	}
	return &Storage().downloads[client];
}

// Original ET stores svs.time + length * 1000 without checking the product.
// fnTech3 keeps the same durations while rejecting unrepresentable requests.
inline bool BanDuration( int seconds, int &milliseconds ) {
	constexpr int limit = 0x7fffffff / 1000;
	if ( seconds <= 0 || seconds > limit ) {
		return false;
	}
	milliseconds = seconds * 1000;
	return true;
}

inline void TempBanAddress( const netadr_t &address, int seconds ) {
	int milliseconds = 0;
	if ( !BanDuration( seconds, milliseconds ) ) {
		return;
	}
	auto &bans = Storage().bans;
	int oldest = -1;
	int oldestTime = 0;
	for ( int index = 0; index < MaxTempBans; ++index ) {
		if ( !bans[index].endTime || bans[index].endTime < svs.time ) {
			bans[index].address = address;
			bans[index].endTime = svs.time + milliseconds;
			return;
		}
		if ( oldest == -1 || oldestTime > bans[index].endTime ) {
			oldestTime = bans[index].endTime;
			oldest = index;
		}
	}
	// Every slot still holds an unexpired ban. The original engine writes the
	// unscaled length here; fnTech3 keeps the requested duration instead so an
	// evicted slot and a free slot expire at the same time.
	bans[oldest].address = address;
	bans[oldest].endTime = svs.time + milliseconds;
}

inline bool TempBanIsBanned( const netadr_t &address ) {
	const auto &bans = Storage().bans;
	for ( int index = 0; index < MaxTempBans; ++index ) {
		if ( !bans[index].endTime || bans[index].endTime <= svs.time ) {
			continue;
		}
		if ( NET_CompareAdr( &address, &bans[index].address ) ) {
			return true;
		}
	}
	return false;
}


// ET reports a module message as empty, queued, or dropped for size.
enum MessageStatus { MessageEmpty = 0, MessageWaiting = 1, MessageWaitingOverflow = 2 };

inline bool ValidClient( int client ) {
	return svs.clients && client >= 0 && client < sv_maxclients->integer
		&& client < FNTECH3_WOLFET_MAX_CLIENTS;
}

// The module owns one pending message per client; a new one replaces the old.
inline void SendBinaryMessage( int client, const char *data, int length ) {
	if ( !ValidClient( client ) ) {
		Com_Error( ERR_DROP, "WolfET binary message for client %i outside server", client );
		return;
	}
	auto &message = Storage().messages[client];
	if ( length < 0 || length > MaxBinaryMessage ) {
		message.length = 0;
		Com_Error( ERR_DROP, "WolfET binary message length %i out of range", length );
		return;
	}
	message.length = length;
	message.overflowed = false;
	if ( length ) {
		if ( !data ) {
			message.length = 0;
			Com_Error( ERR_DROP, "WolfET binary message without payload" );
			return;
		}
		std::memcpy( message.data.data(), data, static_cast<size_t>( length ) );
	}
}

inline int BinaryMessageStatus( int client ) {
	if ( !ValidClient( client ) ) {
		return qfalse;
	}
	const auto &message = Storage().messages[client];
	if ( message.length == 0 ) {
		return MessageEmpty;
	}
	return message.overflowed ? MessageWaitingOverflow : MessageWaiting;
}

inline void ClearBinaryMessage( int client ) {
	if ( !ValidClient( client ) ) {
		return;
	}
	Storage().messages[client].length = 0;
	Storage().messages[client].overflowed = false;
}

} // namespace server
} // namespace wolfet
} // namespace fntech3

#endif
