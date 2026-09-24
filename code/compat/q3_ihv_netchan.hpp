/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_Q3_IHV_NETCHAN_HPP
#define FNTECH3_Q3_IHV_NETCHAN_HPP

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

namespace fntech3::q3ihv {

/* The February 1999 netchan is neither Quake III's nor Quake II's alone. It
 * keeps Quake II's two sequence words with their reliable bits and its
 * single-slot reliable retransmission, adds Quake III's fragmenting for a
 * reliable message that does not fit one packet, and scrambles everything
 * after the header with a sequence-seeded key stream. Observed addresses are
 * in docs/fntech3/Q3_IHV.md (protocol-33 transport); no other profile shares
 * any part of this contract. */
enum {
	/* Netchan_Transmit 0x4128bb splits at this, and 0x412d7f treats a shorter
	 * reliable block as complete. */
	FNTECH3_Q3_IHV_FRAGMENT_BYTES = 1300,
	/* Netchan_Transmit 0x41283d builds each packet in this much space. */
	FNTECH3_Q3_IHV_PACKET_BYTES = 1400,
	/* Each netchan buffer holds this much (0x3ff0). */
	FNTECH3_Q3_IHV_NETCHAN_BYTES = 16368,
	/* Netchan_Encode 0x4126f5 leaves the first ten bytes in the clear. */
	FNTECH3_Q3_IHV_SCRAMBLE_OFFSET = 10
};

/* 0x412690 seeds a key stream from the packet's first word and exclusive-ors
 * every byte past the header with it. The same pass encodes and decodes. */
inline void NetchanScramble(unsigned char *data, std::size_t size) {
	if (!data || size <= FNTECH3_Q3_IHV_SCRAMBLE_OFFSET) return;
	std::uint32_t seed = 0;
	std::memcpy(&seed, data, sizeof seed);
	for (std::size_t i = FNTECH3_Q3_IHV_SCRAMBLE_OFFSET; i < size; ++i) {
		const std::size_t position = i - FNTECH3_Q3_IHV_SCRAMBLE_OFFSET;
		// Each key word covers four payload bytes, low byte first.
		if (!(position & 3)) seed = seed * 69069u + 1u;
		data[i] ^= static_cast<unsigned char>(seed >> ((position & 3) * 8));
	}
}

/* Where a packet is going. Only a client writes its qport, and only a server
 * reads one (0x41288d, 0x412b82). */
enum class NetchanSource { Client, Server };

/* One produced packet. Transmit can emit several for a fragmented reliable
 * message, so the host receives them one at a time. */
struct NetchanSink {
	void *context = nullptr;
	bool (*Send)(void *, const unsigned char *, std::size_t) = nullptr;
};

/* The sequence state a peer's packets advance. A host reads it for
 * diagnostics and restores it when it recreates a channel; the reference probe
 * drives it directly to reach acknowledgement combinations that would
 * otherwise need a live peer. */
struct NetchanSequences {
	int incomingSequence = 0;
	int incomingAcknowledged = 0;
	int incomingReliableAcknowledged = 0;
	int incomingReliableSequence = 0;
	int outgoingSequence = 1;
	int reliableSequence = 0;
	int lastReliableSequence = 0;
};

enum class NetchanResult {
	Nothing,       // Nothing to read from this packet yet.
	Message,       // msg now holds a complete message.
	OutOfOrder,    // Sequence at or behind the last one accepted.
	LostFragment   // A fragment arrived out of order; the message is gone.
};

class Netchan {
public:
	bool Setup(NetchanSource source, int qport) {
		if (qport < 0 || qport > 0xffff) return false;
		*this = Netchan{};
		source_ = source;
		qport_ = qport;
		try {
			fragment_.resize(FNTECH3_Q3_IHV_NETCHAN_BYTES);
			message_.resize(FNTECH3_Q3_IHV_NETCHAN_BYTES);
			reliable_.resize(FNTECH3_Q3_IHV_NETCHAN_BYTES);
		} catch (...) { *this = Netchan{}; return false; }
		ready_ = true;
		return true;
	}
	bool Ready() const { return ready_; }
	bool FatalError() const { return fatal_; }
	int Dropped() const { return dropped_; }
	int IncomingSequence() const { return incomingSequence_; }
	int OutgoingSequence() const { return outgoingSequence_; }
	int IncomingAcknowledged() const { return incomingAcknowledged_; }
	NetchanSequences Sequences() const {
		return {incomingSequence_, incomingAcknowledged_, incomingReliableAcknowledged_,
			incomingReliableSequence_, outgoingSequence_, reliableSequence_, lastReliableSequence_};
	}
	bool SetSequences(const NetchanSequences &state) {
		if (!ready_ || state.incomingSequence < 0 || state.outgoingSequence < 0
			|| state.incomingAcknowledged < 0 || state.lastReliableSequence < 0
			|| (state.reliableSequence & ~1) || (state.incomingReliableSequence & ~1)
			|| (state.incomingReliableAcknowledged & ~1)) return false;
		incomingSequence_ = state.incomingSequence;
		incomingAcknowledged_ = state.incomingAcknowledged;
		incomingReliableAcknowledged_ = state.incomingReliableAcknowledged;
		incomingReliableSequence_ = state.incomingReliableSequence;
		outgoingSequence_ = state.outgoingSequence;
		reliableSequence_ = state.reliableSequence;
		lastReliableSequence_ = state.lastReliableSequence;
		return true;
	}
	std::size_t ReliableLength() const { return reliableLength_; }
	std::size_t PendingLength() const { return messageLength_; }

	/* The reliable slot the host writes into between transmissions. */
	bool QueueReliable(const unsigned char *data, std::size_t size) {
		if (!ready_ || (!data && size)) return false;
		if (size > message_.size() - messageLength_) { overflowed_ = true; return false; }
		if (size) std::memcpy(message_.data() + messageLength_, data, size);
		messageLength_ += size;
		return true;
	}

	/* Netchan_NeedReliable 0x412730. An unacknowledged block is resent as it
	 * stands and the pending message waits: unlike Quake II's version, this one
	 * returns at 0x41274f instead of also promoting. An empty reliable slot
	 * then takes the pending message and flips the reliable bit. */
	bool NeedReliable() {
		if (incomingAcknowledged_ >= lastReliableSequence_
			&& incomingReliableAcknowledged_ != reliableSequence_) return true;
		if (!reliableLength_ && messageLength_) {
			std::memcpy(reliable_.data(), message_.data(), messageLength_);
			reliableLength_ = messageLength_;
			messageLength_ = 0;
			reliableSequence_ ^= 1;
			return true;
		}
		return false;
	}

	/* Netchan_Transmit 0x4127b0. Returns false once the channel has recorded a
	 * fatal outgoing overflow, as the original refuses to send afterwards. */
	bool Transmit(const NetchanSink &sink, const unsigned char *data, std::size_t length) {
		if (!ready_ || !sink.Send || (!data && length)) return false;
		if (overflowed_) { fatal_ = true; return false; }
		const bool reliable = NeedReliable();
		if (reliable && reliableLength_ >= FNTECH3_Q3_IHV_FRAGMENT_BYTES) return Fragments(sink);
		unsigned char packet[FNTECH3_Q3_IHV_PACKET_BYTES];
		std::size_t size = 0;
		WriteHeader(packet, size, reliable);
		if (reliable) {
			WriteShort(packet, size, 0);
			WriteShort(packet, size, static_cast<int>(reliableLength_));
			std::memcpy(packet + size, reliable_.data(), reliableLength_);
			size += reliableLength_;
			lastReliableSequence_ = outgoingSequence_;
		}
		// The original drops an unreliable payload that no longer fits.
		if (sizeof packet - size >= length) {
			if (length) std::memcpy(packet + size, data, length);
			size += length;
			dumped_ = false;
		} else dumped_ = true;
		NetchanScramble(packet, size);
		return sink.Send(sink.context, packet, size);
	}
	bool DumpedUnreliable() const { return dumped_; }

	/* Netchan_Process 0x412b30. The packet is decoded in place; a complete
	 * message is left in the same buffer with its new length. */
	NetchanResult Process(unsigned char *packet, std::size_t &size, std::size_t capacity) {
		if (!ready_ || !packet || size < 8 || capacity < size) return NetchanResult::Nothing;
		NetchanScramble(packet, size);
		std::size_t read = 0;
		const std::uint32_t first = ReadLong(packet, read), second = ReadLong(packet, read);
		const int sequence = static_cast<int>(first & 0x7fffffffu);
		const int reliableMessage = static_cast<int>(first >> 31);
		const int sequenceAck = static_cast<int>(second & 0x7fffffffu);
		const int reliableAck = static_cast<int>(second >> 31);
		int fragmentStart = 0, fragmentLength = 0;
		if (source_ == NetchanSource::Server && !ReadShort(packet, size, read, incomingQport_))
			return NetchanResult::Nothing;
		if (reliableMessage && (!ReadShort(packet, size, read, fragmentStart)
			|| !ReadShort(packet, size, read, fragmentLength))) return NetchanResult::Nothing;
		if (sequence <= incomingSequence_) return NetchanResult::OutOfOrder;
		dropped_ = sequence - incomingSequence_ - 1;
		if (reliableAck == reliableSequence_) reliableLength_ = 0;
		incomingSequence_ = sequence;
		incomingAcknowledged_ = sequenceAck;
		incomingReliableAcknowledged_ = reliableAck;
		if (!reliableMessage) { Consume(packet, size, read); return NetchanResult::Message; }
		if (fragmentStart != static_cast<int>(fragmentLength_)) {
			/* The original drops the fragment and keeps its expected offset
			 * (0x412d15), so a single loss stops every later reassembly on
			 * that channel. fnTech3 forgets the partial message instead, which
			 * lets the peer's retransmission start over. */
			fragmentLength_ = 0;
			return NetchanResult::LostFragment;
		}
		if (!fragmentStart && fragmentLength < FNTECH3_Q3_IHV_FRAGMENT_BYTES) {
			incomingReliableSequence_ ^= 1;
			Consume(packet, size, read);
			return NetchanResult::Message;
		}
		if (fragmentLength < 0 || read + static_cast<std::size_t>(fragmentLength) > size
			|| fragmentLength_ + static_cast<std::size_t>(fragmentLength) > fragment_.size())
			return NetchanResult::Nothing;
		std::memcpy(fragment_.data() + fragmentLength_, packet + read, static_cast<std::size_t>(fragmentLength));
		fragmentLength_ += static_cast<std::size_t>(fragmentLength);
		if (fragmentLength == FNTECH3_Q3_IHV_FRAGMENT_BYTES) return NetchanResult::Nothing;
		if (fragmentLength_ > capacity) { fragmentLength_ = 0; return NetchanResult::Nothing; }
		incomingReliableSequence_ ^= 1;
		std::memcpy(packet, fragment_.data(), fragmentLength_);
		size = fragmentLength_;
		fragmentLength_ = 0;
		return NetchanResult::Message;
	}
	int IncomingQport() const { return incomingQport_; }

private:
	void WriteHeader(unsigned char *packet, std::size_t &size, bool reliable) {
		const std::uint32_t first = (static_cast<std::uint32_t>(outgoingSequence_) & 0x7fffffffu)
			| (reliable ? 0x80000000u : 0u);
		const std::uint32_t second = (static_cast<std::uint32_t>(incomingSequence_) & 0x7fffffffu)
			| (static_cast<std::uint32_t>(incomingReliableSequence_) << 31);
		++outgoingSequence_;
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
	bool ready_ = false, fatal_ = false, overflowed_ = false, dumped_ = false;
	int qport_ = 0, incomingQport_ = 0, dropped_ = 0;
	int incomingSequence_ = 0, incomingAcknowledged_ = 0;
	int incomingReliableAcknowledged_ = 0, incomingReliableSequence_ = 0;
	int outgoingSequence_ = 1, reliableSequence_ = 0, lastReliableSequence_ = 0;
	std::size_t fragmentLength_ = 0, messageLength_ = 0, reliableLength_ = 0;
	std::vector<unsigned char> fragment_, message_, reliable_;

	bool Fragments(const NetchanSink &sink) {
		std::size_t offset = 0;
		for (;;) {
			std::size_t fragment = FNTECH3_Q3_IHV_FRAGMENT_BYTES;
			if (offset + fragment > reliableLength_) fragment = reliableLength_ - offset;
			unsigned char packet[FNTECH3_Q3_IHV_PACKET_BYTES];
			std::size_t size = 0;
			WriteHeader(packet, size, true);
			WriteShort(packet, size, static_cast<int>(offset));
			WriteShort(packet, size, static_cast<int>(fragment));
			if (fragment) std::memcpy(packet + size, reliable_.data() + offset, fragment);
			size += fragment;
			NetchanScramble(packet, size);
			if (!sink.Send(sink.context, packet, size)) return false;
			offset += fragment;
			// A final fragment of exactly the split size needs an empty one.
			if (offset == reliableLength_ && fragment != FNTECH3_Q3_IHV_FRAGMENT_BYTES) break;
		}
		lastReliableSequence_ = outgoingSequence_;
		return true;
	}
};

} // namespace fntech3::q3ihv
#endif
