/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
/* The protocol-33 netchan's own boundaries: the packet scrambler, the sequence
 * words, reliable retransmission, fragmenting and what a receiver accepts.
 * Byte-level agreement with the original is a separate x86 probe. */
#include "../code/compat/q3_ihv_netchan.hpp"
#include "../code/qcommon/netchan_profile.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

static void Check(bool value, int line, const char *text) {
	if (!value) { std::fprintf(stderr, "%d: %s\n", line, text); std::exit(1); }
}
#define CHECK(x) Check((x) ? true : false, __LINE__, #x)

using namespace fntech3::q3ihv;

namespace {
std::vector<std::vector<unsigned char>> captured;
bool Capture(void *, const unsigned char *data, std::size_t size) {
	captured.emplace_back(data, data + size);
	return true;
}
bool Refuse(void *, const unsigned char *, std::size_t) { return false; }
const NetchanSink sink{nullptr, Capture};
const NetchanSink refusing{nullptr, Refuse};

std::uint32_t Word(const std::vector<unsigned char> &packet, std::size_t offset) {
	std::uint32_t value = 0;
	std::memcpy(&value, packet.data() + offset, sizeof value);
	return value;
}
int Short(const std::vector<unsigned char> &packet, std::size_t offset) {
	std::int16_t value = 0;
	std::memcpy(&value, packet.data() + offset, sizeof value);
	return value;
}
std::vector<unsigned char> Plain(std::vector<unsigned char> packet) {
	NetchanScramble(packet.data(), packet.size());
	return packet;
}
} // namespace

void Profile() {
	// Protocol 33 selects its own wire contract, whatever the compat flag says.
	CHECK(Netchan_SelectWireProfile(33, 0) == NETCHAN_WIRE_Q3_IHV);
	CHECK(Netchan_SelectWireProfile(33, 1) == NETCHAN_WIRE_Q3_IHV);
	CHECK(Netchan_SelectWireProfile(68, 0) == NETCHAN_WIRE_LEGACY_Q3);
	const unsigned features = Netchan_WireFeatures(NETCHAN_WIRE_Q3_IHV);
	CHECK(features & NETCHAN_FEATURE_SEQUENCE_PAIR);
	CHECK(features & NETCHAN_FEATURE_RELIABLE_SLOT);
	CHECK(features & NETCHAN_FEATURE_PAYLOAD_KEYSTREAM);
	CHECK(features & NETCHAN_FEATURE_CLIENT_QPORT);
	// It shares none of the later profiles' reliable command ring or codecs.
	CHECK(!(features & NETCHAN_FEATURE_RELIABLE_XOR));
	CHECK(!(features & NETCHAN_FEATURE_SEQUENCE_CHECKSUM));
	CHECK(!(features & NETCHAN_FEATURE_PACKET_SCRAMBLE));
	CHECK(!Netchan_ReliableCommandLimit(NETCHAN_WIRE_Q3_IHV));
	CHECK(Netchan_MessageLimit(NETCHAN_WIRE_Q3_IHV) == FNTECH3_Q3_IHV_NETCHAN_BYTES);
	for (int protocol : {66, 67, 68, 71, 91, 49, 60, 84, 24, 40})
		CHECK(Netchan_SelectWireProfile(protocol, 0) != NETCHAN_WIRE_Q3_IHV);
}

void Scrambler() {
	// The first ten bytes stay in the clear, and the pass is its own inverse.
	std::vector<unsigned char> packet(64);
	for (std::size_t i = 0; i < packet.size(); ++i) packet[i] = static_cast<unsigned char>(i);
	const auto original = packet;
	NetchanScramble(packet.data(), packet.size());
	CHECK(!std::memcmp(packet.data(), original.data(), FNTECH3_Q3_IHV_SCRAMBLE_OFFSET));
	CHECK(std::memcmp(packet.data(), original.data(), packet.size()));
	NetchanScramble(packet.data(), packet.size());
	CHECK(packet == original);
	// The key stream follows the sequence word, so two words differ everywhere.
	std::vector<unsigned char> other = original;
	other[0] ^= 1;
	NetchanScramble(other.data(), other.size());
	std::vector<unsigned char> same = original;
	NetchanScramble(same.data(), same.size());
	CHECK(std::memcmp(other.data() + FNTECH3_Q3_IHV_SCRAMBLE_OFFSET,
		same.data() + FNTECH3_Q3_IHV_SCRAMBLE_OFFSET, 4));
	// A packet with nothing past the header is left alone.
	std::vector<unsigned char> header(FNTECH3_Q3_IHV_SCRAMBLE_OFFSET, 0xa5);
	const auto untouched = header;
	NetchanScramble(header.data(), header.size());
	CHECK(header == untouched);
	NetchanScramble(nullptr, 100);
}

void Sequences() {
	Netchan chan;
	CHECK(!chan.Ready() && !chan.Setup(NetchanSource::Client, -1));
	CHECK(!chan.Setup(NetchanSource::Client, 0x10000));
	CHECK(chan.Setup(NetchanSource::Client, 0x1234) && chan.Ready());
	CHECK(chan.OutgoingSequence() == 1 && !chan.IncomingSequence());
	captured.clear();
	const unsigned char payload[] = {1, 2, 3, 4};
	CHECK(chan.Transmit(sink, payload, sizeof payload));
	CHECK(captured.size() == 1);
	const auto packet = Plain(captured[0]);
	// Two sequence words, then a client's own qport, then the payload.
	CHECK(packet.size() == 8 + 2 + sizeof payload);
	CHECK(Word(packet, 0) == 1u && !Word(packet, 4));
	CHECK(Short(packet, 8) == 0x1234);
	CHECK(!std::memcmp(packet.data() + 10, payload, sizeof payload));
	CHECK(chan.OutgoingSequence() == 2);
	// A server writes no qport.
	Netchan server;
	CHECK(server.Setup(NetchanSource::Server, 0));
	captured.clear();
	CHECK(server.Transmit(sink, payload, sizeof payload));
	CHECK(Plain(captured[0]).size() == 8 + sizeof payload);
	// A refusing sink reports failure without losing the sequence.
	captured.clear();
	CHECK(!server.Transmit(refusing, payload, sizeof payload));
}

void Reliable() {
	Netchan chan;
	CHECK(chan.Setup(NetchanSource::Server, 0));
	const std::vector<unsigned char> block(40, 0x5a);
	CHECK(chan.QueueReliable(block.data(), block.size()));
	CHECK(chan.PendingLength() == block.size() && !chan.ReliableLength());
	captured.clear();
	CHECK(chan.Transmit(sink, nullptr, 0));
	CHECK(captured.size() == 1 && chan.ReliableLength() == block.size() && !chan.PendingLength());
	auto packet = Plain(captured[0]);
	CHECK(Word(packet, 0) & 0x80000000u);
	CHECK(!Short(packet, 8) && Short(packet, 10) == static_cast<int>(block.size()));
	CHECK(!std::memcmp(packet.data() + 12, block.data(), block.size()));
	// Nothing is resent until the peer has acknowledged a packet at least as
	// new as the one that carried the block, so a newly queued message waits.
	const std::vector<unsigned char> second(12, 0x3c);
	CHECK(chan.QueueReliable(second.data(), second.size()));
	captured.clear();
	CHECK(chan.Transmit(sink, nullptr, 0));
	CHECK(Plain(captured[0]).size() == 8);
	CHECK(chan.PendingLength() == second.size() && chan.ReliableLength() == block.size());
	// An acknowledgement that does not name the reliable bit resends the block
	// as it stands; the waiting message still does not take its place.
	auto state = chan.Sequences();
	state.incomingAcknowledged = state.lastReliableSequence;
	state.incomingReliableAcknowledged = state.reliableSequence ^ 1;
	CHECK(chan.SetSequences(state));
	captured.clear();
	CHECK(chan.Transmit(sink, nullptr, 0));
	packet = Plain(captured[0]);
	CHECK(Short(packet, 10) == static_cast<int>(block.size()));
	CHECK(chan.PendingLength() == second.size());
	// A peer packet that does name it frees the slot, and the next transmission
	// promotes the waiting message and flips the reliable bit.
	Netchan peer;
	CHECK(peer.Setup(NetchanSource::Client, 7));
	auto peerState = peer.Sequences();
	peerState.incomingReliableSequence = chan.Sequences().reliableSequence;
	CHECK(peer.SetSequences(peerState));
	captured.clear();
	CHECK(peer.Transmit(sink, nullptr, 0));
	auto ack = captured[0];
	std::size_t size = ack.size();
	ack.resize(FNTECH3_Q3_IHV_NETCHAN_BYTES);
	CHECK(chan.Process(ack.data(), size, ack.size()) == NetchanResult::Message);
	CHECK(!chan.ReliableLength() && chan.IncomingQport() == 7);
	const int before = chan.Sequences().reliableSequence;
	captured.clear();
	CHECK(chan.Transmit(sink, nullptr, 0));
	packet = Plain(captured[0]);
	CHECK(Short(packet, 10) == static_cast<int>(second.size()) && !chan.PendingLength());
	CHECK(chan.Sequences().reliableSequence != before);
	// The pending buffer refuses more than it can hold, and says so once.
	Netchan full;
	CHECK(full.Setup(NetchanSource::Server, 0));
	const std::vector<unsigned char> huge(FNTECH3_Q3_IHV_NETCHAN_BYTES + 1, 0);
	CHECK(!full.QueueReliable(huge.data(), huge.size()));
	CHECK(!full.Transmit(sink, nullptr, 0) && full.FatalError());
	CHECK(!full.QueueReliable(nullptr, 4));
}

void Fragments() {
	Netchan chan;
	CHECK(chan.Setup(NetchanSource::Server, 0));
	std::vector<unsigned char> block(FNTECH3_Q3_IHV_FRAGMENT_BYTES * 2 + 17);
	for (std::size_t i = 0; i < block.size(); ++i) block[i] = static_cast<unsigned char>(i * 7);
	CHECK(chan.QueueReliable(block.data(), block.size()));
	captured.clear();
	CHECK(chan.Transmit(sink, nullptr, 0));
	CHECK(captured.size() == 3);
	std::size_t offset = 0;
	for (const auto &raw : captured) {
		const auto packet = Plain(raw);
		CHECK(Word(packet, 0) & 0x80000000u);
		CHECK(Short(packet, 8) == static_cast<int>(offset));
		const auto length = static_cast<std::size_t>(Short(packet, 10));
		CHECK(!std::memcmp(packet.data() + 12, block.data() + offset, length));
		offset += length;
	}
	CHECK(offset == block.size());
	// A block that divides exactly needs one more, empty, fragment.
	Netchan exact;
	CHECK(exact.Setup(NetchanSource::Server, 0));
	const std::vector<unsigned char> even(FNTECH3_Q3_IHV_FRAGMENT_BYTES * 2, 0x11);
	CHECK(exact.QueueReliable(even.data(), even.size()));
	captured.clear();
	CHECK(exact.Transmit(sink, nullptr, 0));
	CHECK(captured.size() == 3 && Short(Plain(captured[2]), 10) == 0);
}

void Receiving() {
	Netchan sender, receiver;
	CHECK(sender.Setup(NetchanSource::Client, 0x4321));
	CHECK(receiver.Setup(NetchanSource::Server, 0));
	const std::vector<unsigned char> payload(64, 0x77);
	captured.clear();
	CHECK(sender.Transmit(sink, payload.data(), payload.size()));
	auto packet = captured[0];
	std::size_t size = packet.size();
	packet.resize(FNTECH3_Q3_IHV_NETCHAN_BYTES);
	CHECK(receiver.Process(packet.data(), size, packet.size()) == NetchanResult::Message);
	CHECK(size == payload.size() && !std::memcmp(packet.data(), payload.data(), size));
	CHECK(receiver.IncomingQport() == 0x4321 && receiver.IncomingSequence() == 1);
	// The same packet again is behind the sequence already accepted.
	auto again = captured[0];
	size = again.size();
	again.resize(FNTECH3_Q3_IHV_NETCHAN_BYTES);
	CHECK(receiver.Process(again.data(), size, again.size()) == NetchanResult::OutOfOrder);
	// A gap is counted, not rejected.
	captured.clear();
	for (int i = 0; i < 4; ++i) CHECK(sender.Transmit(sink, payload.data(), payload.size()));
	auto later = captured[3];
	size = later.size();
	later.resize(FNTECH3_Q3_IHV_NETCHAN_BYTES);
	CHECK(receiver.Process(later.data(), size, later.size()) == NetchanResult::Message);
	CHECK(receiver.Dropped() == 3);
	// A missing fragment drops the partial message rather than wedging.
	Netchan large, target;
	// Only a client writes its qport, so a receiving server needs a client peer.
	CHECK(large.Setup(NetchanSource::Client, 99) && target.Setup(NetchanSource::Server, 0));
	const std::vector<unsigned char> block(FNTECH3_Q3_IHV_FRAGMENT_BYTES * 2 + 5, 0x2b);
	CHECK(large.QueueReliable(block.data(), block.size()));
	captured.clear();
	CHECK(large.Transmit(sink, nullptr, 0));
	CHECK(captured.size() == 3);
	auto middle = captured[1];
	size = middle.size();
	middle.resize(FNTECH3_Q3_IHV_NETCHAN_BYTES);
	CHECK(target.Process(middle.data(), size, middle.size()) == NetchanResult::LostFragment);
	// A retransmission with fresh sequences reassembles from the start.
	auto resend = large.Sequences();
	resend.incomingAcknowledged = resend.lastReliableSequence;
	resend.incomingReliableAcknowledged = resend.reliableSequence ^ 1;
	CHECK(large.SetSequences(resend));
	captured.clear();
	CHECK(large.Transmit(sink, nullptr, 0));
	CHECK(captured.size() == 3);
	for (std::size_t i = 0; i < captured.size(); ++i) {
		auto copy = captured[i];
		size = copy.size();
		copy.resize(FNTECH3_Q3_IHV_NETCHAN_BYTES);
		const auto result = target.Process(copy.data(), size, copy.size());
		if (i + 1 < captured.size()) {
			CHECK(result == NetchanResult::Nothing);
			continue;
		}
		CHECK(result == NetchanResult::Message && size == block.size());
		CHECK(!std::memcmp(copy.data(), block.data(), size));
	}
	// Truncated and unusable packets are refused without reading past them.
	std::vector<unsigned char> stub(4, 0);
	size = stub.size();
	CHECK(target.Process(stub.data(), size, stub.size()) == NetchanResult::Nothing);
	CHECK(target.Process(nullptr, size, 16) == NetchanResult::Nothing);
	Netchan unready;
	size = 16;
	std::vector<unsigned char> room(16, 0);
	CHECK(unready.Process(room.data(), size, room.size()) == NetchanResult::Nothing);
}

int main() {
	Profile();
	Scrambler();
	Sequences();
	Reliable();
	Fragments();
	Receiving();
	std::puts("IHV netchan profile, scrambler, sequences, reliable slot, fragments and receiving passed");
	return 0;
}
