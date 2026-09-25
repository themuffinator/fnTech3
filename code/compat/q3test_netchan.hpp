/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_Q3TEST_NETCHAN_HPP
#define FNTECH3_Q3TEST_NETCHAN_HPP

#include "q3_ihv_netchan.hpp"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

/* Q3Test's two netchans. 1.02 to 1.05 (protocols 35 and 36) keep the
 * February 1999 IHV channel unchanged in behaviour -- two sequence words
 * with reliable bits, one reliable block in flight, 1300-byte slices and a
 * word-wise key stream from byte 10 -- so they use fntech3::q3ihv::Netchan.
 * 1.06 to 1.08 (38 to 40) rewrote it into Quake III's shape, which this file
 * implements: no reliable bits and no reliable slot, any message of 1300
 * bytes or more sent as slices that all carry one sequence number, and a
 * per-byte scrambler from byte 10. The facts and addresses are in
 * docs/fntech3/Q3TEST.md; 1.07 and 1.08 are 1.06 modulo addresses. */
namespace fntech3::q3test {

using q3ihv::NetchanResult;
using q3ihv::NetchanSink;
using q3ihv::NetchanSource;

enum {
	/* Netchan_Transmit 1.06 0x4157b0 sends a message this long or longer as
	 * slices of this size (0x41582d, 0x415835). */
	FNTECH3_Q3TEST_FRAGMENT_BYTES = 1300,
	/* Each packet is built in 1400 bytes (0x41590a). */
	FNTECH3_Q3TEST_PACKET_BYTES = 1400,
	/* Reassembly holds 16384 bytes (Netchan_Process 0x415c51). */
	FNTECH3_Q3TEST_MESSAGE_BYTES = 16384,
	/* Both families leave the first ten bytes in the clear. */
	FNTECH3_Q3TEST_SCRAMBLE_OFFSET = 10
};

/* 1.06 0x4159e0: one LCG step per byte, taken before use, low byte only. The
 * same pass encodes and decodes; it is not family A's word-wise stream. */
inline void NetchanScrambleB(unsigned char *data, std::size_t size) {
	if (!data || size <= FNTECH3_Q3TEST_SCRAMBLE_OFFSET) return;
	std::uint32_t seed = 0;
	std::memcpy(&seed, data, sizeof seed);
	for (std::size_t i = FNTECH3_Q3TEST_SCRAMBLE_OFFSET; i < size; ++i) {
		seed = seed * 69069u + 1u;
		data[i] ^= static_cast<unsigned char>(seed);
	}
}

class NetchanB {
public:
	bool Setup(NetchanSource source, int qport) {
		if (qport < 0 || qport > 0xffff) return false;
		*this = NetchanB{};
		source_ = source;
		qport_ = qport;
		try { fragment_.resize(FNTECH3_Q3TEST_MESSAGE_BYTES); } catch (...) { *this = NetchanB{}; return false; }
		ready_ = true;
		return true;
	}
	bool Ready() const { return ready_; }
	int Dropped() const { return dropped_; }
	int IncomingSequence() const { return incomingSequence_; }
	int IncomingAcknowledged() const { return incomingAcknowledged_; }
	int OutgoingSequence() const { return outgoingSequence_; }
	int IncomingQport() const { return incomingQport_; }

	/* Netchan_Transmit 1.06 0x4157a0. A short message is one packet; a longer
	 * one goes out at once as slices with one sequence number and the fragment
	 * bit, a final full slice followed by an empty one, and the sequence moves
	 * on once after the last (0x4158f8). */
	bool Transmit(const NetchanSink &sink, const unsigned char *data, std::size_t length) {
		if (!ready_ || !sink.Send || (!data && length) || length > FNTECH3_Q3TEST_MESSAGE_BYTES) return false;
		if (length < FNTECH3_Q3TEST_FRAGMENT_BYTES) {
			unsigned char packet[FNTECH3_Q3TEST_PACKET_BYTES];
			std::size_t size = 0;
			WriteHeader(packet, size, false);
			if (length) std::memcpy(packet + size, data, length);
			size += length;
			++outgoingSequence_;
			NetchanScrambleB(packet, size);
			return sink.Send(sink.context, packet, size);
		}
		std::size_t offset = 0;
		for (;;) {
			std::size_t fragment = FNTECH3_Q3TEST_FRAGMENT_BYTES;
			if (offset + fragment > length) fragment = length - offset;
			unsigned char packet[FNTECH3_Q3TEST_PACKET_BYTES];
			std::size_t size = 0;
			WriteHeader(packet, size, true);
			WriteShort(packet, size, static_cast<int>(offset));
			WriteShort(packet, size, static_cast<int>(fragment));
			if (fragment) std::memcpy(packet + size, data + offset, fragment);
			size += fragment;
			NetchanScrambleB(packet, size);
			if (!sink.Send(sink.context, packet, size)) return false;
			offset += fragment;
			if (offset == length && fragment != FNTECH3_Q3TEST_FRAGMENT_BYTES) break;
		}
		++outgoingSequence_;
		return true;
	}

	/* Netchan_Process 1.06 0x415a30. The packet is decoded in place and a
	 * complete message is left in the same buffer with its new length.
	 *
	 * fnTech3 boundary: retail's completion of a fragmented message updates
	 * neither the incoming sequence nor the acknowledgement (they are stored
	 * only on the unfragmented path, 0x415d42/0x415d47), so it never
	 * acknowledges one, still takes an older packet afterwards, and delivers
	 * a duplicated copy twice. This channel records the sequence as any
	 * other; a retail sender relies on none of that. */
	NetchanResult Process(unsigned char *packet, std::size_t &size, std::size_t capacity) {
		if (!ready_ || !packet || size < 8 || capacity < size) return NetchanResult::Nothing;
		NetchanScrambleB(packet, size);
		std::size_t read = 0;
		const std::uint32_t first = ReadLong(packet, read), second = ReadLong(packet, read);
		const bool fragmented = (first & 0x80000000u) != 0;
		const int sequence = static_cast<int>(first & 0x7fffffffu);
		int fragmentStart = 0, fragmentLength = 0;
		if (source_ == NetchanSource::Server && !ReadShort(packet, size, read, incomingQport_))
			return NetchanResult::Nothing;
		if (fragmented && (!ReadShort(packet, size, read, fragmentStart) || !ReadShort(packet, size, read, fragmentLength)))
			return NetchanResult::Nothing;
		if (sequence <= incomingSequence_) return NetchanResult::OutOfOrder;
		dropped_ = sequence - incomingSequence_ - 1;
		if (!fragmented) {
			incomingSequence_ = sequence;
			incomingAcknowledged_ = static_cast<int>(second);
			Consume(packet, size, read);
			return NetchanResult::Message;
		}
		// A new sequence restarts reassembly, so a lost slice costs one message.
		if (sequence != fragmentSequence_) {
			fragmentSequence_ = sequence;
			fragmentLength_ = 0;
		}
		if (fragmentStart != static_cast<int>(fragmentLength_)) return NetchanResult::LostFragment;
		if (fragmentLength < 0 || read + static_cast<std::size_t>(fragmentLength) > size
			|| fragmentLength_ + static_cast<std::size_t>(fragmentLength) > fragment_.size())
			return NetchanResult::Nothing;
		std::memcpy(fragment_.data() + fragmentLength_, packet + read, static_cast<std::size_t>(fragmentLength));
		fragmentLength_ += static_cast<std::size_t>(fragmentLength);
		if (fragmentLength == FNTECH3_Q3TEST_FRAGMENT_BYTES) return NetchanResult::Nothing;
		if (fragmentLength_ > capacity) { fragmentLength_ = 0; return NetchanResult::Nothing; }
		std::memcpy(packet, fragment_.data(), fragmentLength_);
		size = fragmentLength_;
		fragmentLength_ = 0;
		incomingSequence_ = sequence;
		incomingAcknowledged_ = static_cast<int>(second);
		return NetchanResult::Message;
	}

private:
	void WriteHeader(unsigned char *packet, std::size_t &size, bool fragment) {
		const std::uint32_t first = (static_cast<std::uint32_t>(outgoingSequence_) & 0x7fffffffu) | (fragment ? 0x80000000u : 0u);
		const std::uint32_t second = static_cast<std::uint32_t>(incomingSequence_);
		std::memcpy(packet + size, &first, sizeof first); size += sizeof first;
		std::memcpy(packet + size, &second, sizeof second); size += sizeof second;
		if (source_ == NetchanSource::Client) WriteShort(packet, size, qport_);
	}
	static void WriteShort(unsigned char *packet, std::size_t &size, int value) {
		const auto word = static_cast<std::uint16_t>(value);
		std::memcpy(packet + size, &word, sizeof word);
		size += sizeof word;
	}
	static std::uint32_t ReadLong(const unsigned char *packet, std::size_t &read) {
		std::uint32_t value = 0;
		std::memcpy(&value, packet + read, sizeof value);
		read += sizeof value;
		return value;
	}
	static bool ReadShort(const unsigned char *packet, std::size_t size, std::size_t &read, int &value) {
		if (read + sizeof(std::int16_t) > size) return false;
		std::int16_t word = 0;
		std::memcpy(&word, packet + read, sizeof word);
		read += sizeof word;
		value = word;
		return true;
	}
	static void Consume(unsigned char *packet, std::size_t &size, std::size_t read) {
		size -= read;
		if (size) std::memmove(packet, packet + read, size);
	}

	NetchanSource source_ = NetchanSource::Server;
	bool ready_ = false;
	int qport_ = 0, incomingQport_ = 0, dropped_ = 0;
	int incomingSequence_ = 0, incomingAcknowledged_ = 0, outgoingSequence_ = 1;
	int fragmentSequence_ = 0;
	std::size_t fragmentLength_ = 0;
	std::vector<unsigned char> fragment_;
};

/* Either family behind one face: reliable data exists only in family A. */
class Netchan {
public:
	bool Setup(bool legacy, NetchanSource source, int qport) {
		legacy_ = legacy;
		return legacy ? a_.Setup(source, qport) : b_.Setup(source, qport);
	}
	bool Legacy() const { return legacy_; }
	bool Ready() const { return legacy_ ? a_.Ready() : b_.Ready(); }
	int Dropped() const { return legacy_ ? a_.Dropped() : b_.Dropped(); }
	int IncomingSequence() const { return legacy_ ? a_.IncomingSequence() : b_.IncomingSequence(); }
	int IncomingAcknowledged() const { return legacy_ ? a_.IncomingAcknowledged() : b_.IncomingAcknowledged(); }
	int OutgoingSequence() const { return legacy_ ? a_.OutgoingSequence() : b_.OutgoingSequence(); }
	bool FatalError() const { return legacy_ && a_.FatalError(); }
	std::size_t PendingReliable() const { return legacy_ ? a_.PendingLength() : 0; }
	std::size_t ReliableInFlight() const { return legacy_ ? a_.ReliableLength() : 0; }
	bool QueueReliable(const unsigned char *data, std::size_t size) { return legacy_ && a_.QueueReliable(data, size); }
	bool Transmit(const NetchanSink &sink, const unsigned char *data, std::size_t length) {
		return legacy_ ? a_.Transmit(sink, data, length) : b_.Transmit(sink, data, length);
	}
	NetchanResult Process(unsigned char *packet, std::size_t &size, std::size_t capacity) {
		return legacy_ ? a_.Process(packet, size, capacity) : b_.Process(packet, size, capacity);
	}

private:
	bool legacy_ = false;
	q3ihv::Netchan a_;
	NetchanB b_;
};

} // namespace fntech3::q3test
#endif
