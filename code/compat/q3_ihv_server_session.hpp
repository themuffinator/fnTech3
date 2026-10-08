/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_Q3_IHV_SERVER_SESSION_HPP
#define FNTECH3_Q3_IHV_SERVER_SESSION_HPP

#include "../qcommon/q3_ihv_wire.hpp"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

/* The protocol-33 server's side of a client session, as the February 1999
 * quake3.exe reads and writes it. Only the rules that need no engine state are
 * here, so they can be tested alone; the session itself is
 * code/server/server_q3_ihv_net.hpp. Addresses are in docs/fntech3/Q3_IHV.md
 * (protocol-33 server sessions). */
namespace fntech3::q3ihv::server {

/* The original's msg_t writes. Bits fill the last byte from its low end
 * (MSG_WriteBits 0x410b40), and a bit write is dropped, with the message marked
 * overflowed, once fewer than four bytes remain. Strings and raw data are
 * copied at the byte count (MSG_WriteString 0x410d00, MSG_WriteData 0x410aa0)
 * without touching the bit position, and an overflowing copy first clears the
 * message (MSG_GetSpace 0x410a30). A gamestate can leave a partial last byte,
 * so the next server command's bytes can follow it unaligned, exactly as the
 * original queued them. */
class OriginalMessage {
public:
	explicit OriginalMessage(std::size_t capacity) : data_(capacity) {}
	void Clear() { size_ = 0; bit_ = 0; overflowed_ = false; }
	void WriteBits(std::uint32_t value, unsigned bits) {
		if (data_.size() - size_ < 4) { overflowed_ = true; return; }
		while (bits) {
			if (!bit_) data_[size_++] = 0;
			unsigned take = 8 - bit_;
			if (take > bits) take = bits;
			data_[size_ - 1] |= static_cast<std::uint8_t>((value & ((1u << take) - 1)) << bit_);
			value = take < 32 ? value >> take : 0;
			bit_ = (bit_ + take) & 7;
			bits -= take;
		}
	}
	void WriteByte(int value) { WriteBits(static_cast<std::uint32_t>(value) & 0xffu, 8); }
	void WriteShort(int value) { WriteBits(static_cast<std::uint32_t>(value) & 0xffffu, 16); }
	void WriteData(const void *data, std::size_t length) {
		if (size_ + length > data_.size()) {
			Clear();
			overflowed_ = true;
			if (length > data_.size()) return;
		}
		if (length) std::memcpy(data_.data() + size_, data, length);
		size_ += length;
	}
	void WriteString(const char *text) {
		static const char empty[1] = {0};
		if (!text) text = empty;
		WriteData(text, std::strlen(text) + 1);
	}
	/* A bit stream another writer produced with bit writes only. */
	void AppendBits(const std::uint8_t *data, std::size_t bits) {
		for (std::size_t offset = 0; offset < bits; offset += 8) {
			const unsigned count = bits - offset < 8 ? static_cast<unsigned>(bits - offset) : 8u;
			WriteBits(data[offset / 8] & ((1u << count) - 1), count);
		}
	}
	const std::uint8_t *Data() const { return data_.data(); }
	std::size_t Size() const { return size_; }
	bool Overflowed() const { return overflowed_; }

private:
	std::vector<std::uint8_t> data_;
	std::size_t size_ = 0;
	unsigned bit_ = 0;
	bool overflowed_ = false;
};

/* The original's msg_t reads over a client message. Every reader first checks
 * that the whole value is present and otherwise returns -1 without consuming
 * anything (MSG_ReadChar 0x410db0, ReadByte 0x410de0, ReadShort 0x410e10,
 * ReadLong 0x410e40). A truncated message therefore yields -1 fields rather
 * than a bad read: SV_ExecuteClientMessage's "had a badread" (0x418f58) cannot
 * be reached through these readers. */
class OriginalReader {
public:
	OriginalReader(const void *data, std::size_t bytes) : input_(data, bytes) {}
	int ReadChar() { return Read(8, true); }
	int ReadByte() { return Read(8, false); }
	int ReadShort() { return Read(16, true); }
	int ReadLong() { return Read(32, true); }
	/* MSG_ReadString 0x410ea0 stops at a zero, at -1 (a missing byte or an
	 * 0xff one, which ReadChar returns as -1) and after 1023 characters. */
	std::string ReadString() {
		std::string text;
		for (;;) {
			const int c = ReadChar();
			if (c == -1 || c == 0) break;
			text.push_back(static_cast<char>(c));
			if (text.size() >= 1023) break;
		}
		return text;
	}
	/* MSG_ReadDeltaUsercmd 0x411a50: a mask byte, the masked fields in the
	 * writer's order, then the unconditional duration. */
	void ReadCommand(const wire::Command &from, wire::Command &to) {
		to = from;
		const int mask = ReadByte();
		for (unsigned i = 0; i < 8; ++i) {
			if (!(static_cast<unsigned>(mask) & (1u << i))) continue;
			const auto &field = wire::CommandFields[i];
			const int value = field.bits == 16 ? ReadShort() : i >= 3 && i <= 5 ? ReadChar() : ReadByte();
			wire::Store(to.data() + field.offset, field.storage, static_cast<std::uint32_t>(value));
		}
		to[0] = static_cast<std::uint8_t>(ReadByte());
	}

private:
	int Read(unsigned bits, bool sign) {
		if (input_.Remaining() < bits) return -1;
		std::uint32_t value = 0;
		input_.Read(bits, value);
		if (!sign || bits == 32) return static_cast<int>(value);
		const std::uint32_t top = 1u << (bits - 1);
		return static_cast<int>(value ^ top) - static_cast<int>(top);
	}
	wire::Reader input_;
};

enum { MaxPacketCommands = 32, ClientMessageCommands = 7 };

/* SV_UserMove 0x418cc0 reads every field before it tests anything. The
 * delta message and acknowledged configstring sequence are stored before the
 * count is checked, and a count outside 2..32 ends the move with only a
 * console line, leaving the reader just after the count byte. */
enum class MoveStatus { Ok, CountLow, CountHigh };
struct Move {
	int serverId = 0, time = 0, deltaMessage = 0, configAck = 0, lastCommand = 0, count = 0;
	std::array<wire::Command, MaxPacketCommands> commands{};
};
inline MoveStatus ReadMove(OriginalReader &input, Move &move) {
	move.serverId = input.ReadLong();
	move.time = input.ReadLong();
	move.deltaMessage = input.ReadLong();
	move.configAck = input.ReadLong();
	move.lastCommand = input.ReadLong();
	move.count = input.ReadByte();
	if (move.count < 2) return MoveStatus::CountLow;
	if (move.count > MaxPacketCommands) return MoveStatus::CountHigh;
	wire::Command from{};
	for (int i = 0; i < move.count; ++i) {
		input.ReadCommand(from, move.commands[static_cast<std::size_t>(i)]);
		from = move.commands[static_cast<std::size_t>(i)];
	}
	return MoveStatus::Ok;
}

/* Which of a move's commands run (0x418ddb..0x418e97). The last command is the
 * client's provisional one and never runs here; the game gets it once through
 * its after-commands export. A gap since the last command run is reported and
 * filled with up to five runs of the move's first command. The original's
 * arithmetic is 32-bit and wraps; this keeps the same results for every value
 * a client can send without forming an out-of-range count. */
struct MovePlan {
	int lost = 0;     // the "Lost %i usercmds" count, 0 when nothing was lost
	int replays = 0;  // runs of the first command filling the gap
	int begin = 0;    // first command index to run after the replays
	int end = 0;      // one past the last, the provisional command excluded
};
inline MovePlan PlanMove(int lastRun, int lastCommand, int count) {
	MovePlan plan;
	const std::int64_t first = static_cast<std::int64_t>(lastCommand) - count + 2;
	std::int64_t last = lastRun;
	if (last < first - 1) {
		plan.lost = static_cast<int>(std::min<std::int64_t>(first - last - 1, 0x7fffffff));
		if (last < first - 6) last = first - 6;
		plan.replays = static_cast<int>(first - 1 - last);
		last = first - 1;
	}
	plan.end = count - 1;
	const std::int64_t begin = last - first + 1;
	plan.begin = begin > plan.end ? plan.end : static_cast<int>(begin);
	return plan;
}

/* The connectionless replies. SVC_Info 0x41a260 formats five numbers but
 * passes four: protocol, hostname, map, maxclients, and an uninitialised stack
 * word the original never wrote. fnTech3 fills that last field with the number
 * of connected clients. SVC_Status 0x41a0c0 sends the serverinfo, a newline,
 * then one "score ping name" line per connected client while the total stays
 * under 1400 bytes; the request's own argument goes first, as the original's
 * format places it. */
inline std::string InfoReply(const char *hostname, const char *mapname, int maxclients, int connected) {
	char line[1400];
	std::snprintf(line, sizeof line, "info\n%i \"%s\" \"%s\" %i %i\n", 33, hostname ? hostname : "",
		mapname ? mapname : "", maxclients, connected);
	return line;
}
struct StatusPlayer { int score = 0, ping = 0; std::string name; };
inline std::string StatusReply(const char *argument, const char *serverinfo, const std::vector<StatusPlayer> &players) {
	std::string body = serverinfo ? serverinfo : "";
	body += '\n';
	for (const auto &player : players) {
		char line[1024];
		std::snprintf(line, sizeof line, "%i %i \"%s\"\n", player.score, player.ping, player.name.c_str());
		if (body.size() + std::strlen(line) >= 1400) break;
		body += line;
	}
	return std::string("status\n") + (argument ? argument : "") + "\n" + body;
}

/* SV_SendClientDatagram 0x41b5e8..0x41b690: the next snapshot time after a
 * datagram of `bytes` bytes. Loopback clients get one every server frame; a
 * client silent for over a second gets one a second; otherwise the rate's
 * delay, (bytes + 48) * 1000 / rate, or the snapshot interval when that is
 * longer, with the rate-delayed flag set when the rate decided. */
struct NextSnapshot { int time = 0; bool rateDelayed = false; };
inline NextSnapshot ScheduleSnapshot(int now, bool loopback, int lastPacketTime, int bytes, int rate, int snapshotMsec) {
	if (loopback) return {now - 1, false};
	if (now - lastPacketTime > 1000) return {now + 1000, false};
	const std::int64_t rateMsec = (static_cast<std::int64_t>(bytes) + 48) * 1000 / (rate > 0 ? rate : 1);
	if (rateMsec < snapshotMsec) return {now + snapshotMsec, false};
	return {now + static_cast<int>(rateMsec), true};
}

/* SV_CalcPings 0x41a910: the mean of every acknowledgement time minus its send
 * time that is not negative, over all 32 frames, unused ones included; 999 when
 * none qualifies or the mean is larger. */
inline int Ping(const int *sent, const int *acked, int frames) {
	int total = 0, count = 0;
	for (int i = 0; i < frames; ++i) {
		const int delta = acked[i] - sent[i];
		if (delta < 0) continue;
		total += delta;
		++count;
	}
	if (!count) return 999;
	const int ping = total / count;
	return ping > 999 ? 999 : ping;
}

} // namespace fntech3::q3ihv::server
#endif
