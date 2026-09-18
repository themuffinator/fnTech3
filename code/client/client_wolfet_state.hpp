/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_CLIENT_WOLFET_STATE_HPP
#define FNTECH3_CLIENT_WOLFET_STATE_HPP
/* Include after client.h; neither it nor g_public.h carries an include guard. */
#include "../compat/wolfet_game_abi.h"
#include <array>
#include <cstring>

/* Enemy Territory's client-side binary message channel. ET's cgame hands the
 * engine an opaque payload, the engine appends it after the end of its next
 * outgoing packet, and the cgame asks whether it has gone yet. The buffer is
 * ET's own 32 KiB, so it is kept here rather than widening every profile's
 * connection state by that much. */
namespace fntech3 {
namespace wolfet {
namespace client {

constexpr int MaxBinaryMessage = 32768;

/* ET's cg_public.h message states. */
enum { MessageEmpty = 0, MessageWaiting = 1, MessageWaitingOverflow = 2 };

struct BinaryMessage {
	int length = 0;
	bool overflowed = false;
	std::array<char, MaxBinaryMessage> data{};
};

inline BinaryMessage &Message() {
	static BinaryMessage message;
	return message;
}

/* Cleared in place: the buffer is far too large to pass or return by value. */
inline void Reset() {
	auto &message = Message();
	message.length = 0;
	message.overflowed = false;
	std::memset( message.data.data(), 0, message.data.size() );
}

} // namespace client
} // namespace wolfet
} // namespace fntech3

#endif
