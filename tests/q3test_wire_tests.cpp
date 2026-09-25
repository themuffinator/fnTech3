/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#include "../code/qcommon/q3test_wire.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

/* Synthetic tests always run. Retail recordings are optional: pass the .dm3
 * files extracted from the demos directory of the 1.05, 1.06 and 1.08
 * demoq3/pak0.pk3 as arguments. Every record must parse to its last bit and
 * re-encode byte for byte, and the six shipped demos must match the counts
 * measured for them. */

#define CHECK(x) do { if (!(x)) { std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #x); std::exit(1); } } while (0)

namespace {
using namespace fntech3::q3test::wire;
using Bytes = std::vector<std::uint8_t>;
constexpr int Protocols[] = {35, 36, 38, 39, 40};

// ---- Tables ----

static_assert(PlayerBytes(35) == 276 && PlayerBytes(36) == 416 && PlayerBytes(38) == 432 && PlayerBytes(39) == 432
	&& PlayerBytes(40) == 440 && PlayerBytes(37) == 0 && PlayerBytes(33) == 0);
static_assert(UsercmdBytes(35) == 14 && UsercmdBytes(36) == 20 && UsercmdBytes(38) == 20 && UsercmdBytes(39) == 20
	&& UsercmdBytes(40) == 24 && UsercmdBytes(41) == 0);
static_assert(EntityEnd(35) == 0 && EntityEnd(36) == 0 && EntityEnd(38) == 1023 && EntityEnd(40) == 1023);
static_assert(ValidEntityNumber(35, 1) && ValidEntityNumber(36, 1023) && !ValidEntityNumber(36, 0) && !ValidEntityNumber(35, 1024)
	&& ValidEntityNumber(38, 0) && ValidEntityNumber(39, 1022) && !ValidEntityNumber(40, 1023) && !ValidEntityNumber(40, -1)
	&& !ValidEntityNumber(37, 5) && !ValidEntityNumber(0, 5));
static_assert(MinMoveCommands(35) == 2 && MinMoveCommands(36) == 2 && MinMoveCommands(38) == 1 && MinMoveCommands(40) == 1);
static_assert(IsProtocol(35) && IsProtocol(36) && IsProtocol(38) && IsProtocol(39) && IsProtocol(40) && !IsProtocol(37)
	&& !IsProtocol(33) && !IsProtocol(34) && !IsProtocol(41) && !IsProtocol(68) && !IsProtocol(0));

constexpr unsigned PopCount(std::uint64_t value) {
	unsigned count = 0;
	for (; value; value &= value - 1) ++count;
	return count;
}
// The 50 fields cover dwords 1..50, each once, with the table's widths.
constexpr bool EntityTableCoversRecord() {
	std::uint64_t seen = 0;
	for (const EntityField &field : EntityFields) {
		if (field.offset < 4 || field.offset > 200 || field.offset % 4) return false;
		const std::uint64_t bit = std::uint64_t(1) << (field.offset / 4);
		if (seen & bit) return false;
		seen |= bit;
		if (field.bits != 0 && field.bits != 8 && field.bits != 10 && field.bits != 16 && field.bits != 24 && field.bits != 32) return false;
	}
	return seen == (std::uint64_t(1) << 51) - 2;
}
static_assert(EntityTableCoversRecord() && PopCount(EntityFloatFields()) == 24);
constexpr bool MasksValid(const std::uint64_t *masks, unsigned used) {
	for (unsigned i = 0; i < 32; ++i) {
		if ((masks[i] >> 50) || (i < used) != (masks[i] != 0)) return false;
		for (unsigned j = 0; j < i; ++j)
			if (masks[i] && masks[i] == masks[j]) return false;
	}
	return true;
}
static_assert(MasksValid(CommonMasks35, 27) && MasksValid(CommonMasks38, 32));
constexpr bool NetTableValid(int protocol, unsigned count) {
	const NetFieldTable table = PlayerNetFields(protocol);
	if (table.count != count) return false;
	std::uint64_t seen = 0;
	for (unsigned i = 0; i < table.count; ++i) {
		const NetField &field = table.fields[i];
		if (field.offset % 4 || field.offset + 4u > table.arrays[0] || field.bits < -31 || field.bits > 32) return false;
		const std::uint64_t bit = std::uint64_t(1) << (field.offset / 4);
		if (seen & bit) return false;
		seen |= bit;
	}
	// stats, persistant, powerups and ammo lie back to back after the fields.
	return table.arrays[1] == table.arrays[0] + 64 && table.arrays[3] == table.arrays[1] + 64
		&& table.arrays[2] == table.arrays[3] + 64 && table.arrays[2] + 64u <= PlayerBytes(protocol);
}
static_assert(NetTableValid(36, 35) && NetTableValid(38, 42) && NetTableValid(39, 42) && NetTableValid(40, 44));
constexpr bool Groups35Valid() {
	for (const Group35 &group : PlayerGroups35)
		if (group.offset + group.storage * group.count > 0x64) return false;
	for (const Array35 &list : PlayerArrays35)
		if (list.offset < 0x64 || list.offset + list.storage * 16 > 276) return false;
	return KnownFlags35() == ~(0x80000u | 0x20000000u | 0x40000000u | 0x80000000u);
}
static_assert(Groups35Valid());
constexpr bool CommandsValid(const CommandField *fields, unsigned count, unsigned bytes) {
	for (unsigned i = 0; i < count; ++i)
		if (fields[i].offset + fields[i].storage > static_cast<int>(bytes)) return false;
	return true;
}
static_assert(CommandsValid(Commands35, 8, 14) && CommandsValid(Commands36, 8, 20) && CommandsValid(Commands40, 9, 24));

// ---- Helpers ----

struct Random {
	std::uint64_t state;
	explicit Random(std::uint64_t seed) : state(seed * 0x9e3779b97f4a7c15ull + 0x2545f4914f6cdd1dull) {}
	std::uint32_t Next() {
		state ^= state << 13;
		state ^= state >> 7;
		state ^= state << 17;
		return static_cast<std::uint32_t>(state >> 32);
	}
	std::uint32_t Below(std::uint32_t bound) { return bound ? Next() % bound : 0; }
	bool Chance(unsigned percent) { return Below(100) < percent; }
};

bool Bit(std::uint64_t mask, unsigned index) { return ((mask >> index) & 1) != 0; }
std::uint32_t LowMask(unsigned bits) { return bits >= 32 ? 0xffffffffu : (1u << bits) - 1; }
std::uint32_t SignExtend(std::uint32_t value, unsigned bits) {
	value &= LowMask(bits);
	return bits < 32 && ((value >> (bits - 1)) & 1) ? value | ~LowMask(bits) : value;
}
// Floats that cross the wire unchanged, and (not clean) -0.0 and NaNs too.
std::uint32_t RandomFloat(Random &random, bool clean) {
	switch (random.Below(clean ? 5 : 9)) {
	case 0: return FloatBits(static_cast<float>(static_cast<int>(random.Below(8192)) - 4096));
	case 1: return FloatBits(static_cast<float>(static_cast<int>(random.Below(2000001)) - 1000000) / 7.0f);
	case 2: return FloatBits(static_cast<float>(static_cast<int>(random.Below(200001)) - 100000));
	case 3: return (random.Next() & 0x7f7fffffu) | (random.Next() & 0x80000000u);
	case 4: return random.Chance(50) ? 0x7f800000u : 0xff800000u;
	case 5: return 0x80000000u;
	case 6: return 0x7fc00000u | (random.Next() & 0x3fffffu);
	case 7: return 0x7f800001u + random.Below(0x3ffffe);
	default: return 0xff800001u + random.Below(0x7ffffe);
	}
}
// What a delta-coded float arrives as: integral values in -4096..4095 (and
// -0.0) through the 13-bit form, NaN as 0, anything else as sent.
std::uint32_t WireFloat(std::uint32_t bits) {
	const float value = BitsFloat(bits);
	if (std::isnan(value)) return 0;
	if (value >= -4096.0f && value <= 4095.0f && std::trunc(value) == value) return FloatBits(std::trunc(value) + 0.0f);
	return bits;
}
Entity RandomEntity(Random &random, std::int32_t number, unsigned density, bool clean) {
	Entity entity{};
	SetEntityNumber(entity, number);
	for (const EntityField &field : EntityFields) {
		if (!random.Chance(density)) continue;
		const std::uint32_t value = field.bits ? (clean ? random.Next() & LowMask(field.bits) : random.Next()) : RandomFloat(random, clean);
		Store(entity.data() + field.offset, 4, value);
	}
	return entity;
}
// A nonzero modelindex keeps an entity unlike any unsent (zero) baseline.
Entity CleanEntity(Random &random, std::int32_t number, unsigned density) {
	Entity entity = RandomEntity(random, number, density, true);
	Store(entity.data() + 160, 4, 1 + random.Below(255));
	return entity;
}
Bytes Contents(const Writer &out) { return Bytes(out.Data(), out.Data() + out.Size()); }

// ---- Expected decodes, written from the specification ----

Entity ExpectEntity(int protocol, const Entity &from, const Entity &to) {
	Entity out = from;
	bool changed = false;
	for (const EntityField &field : EntityFields) {
		const std::uint32_t value = Load(to.data() + field.offset, 4);
		if (Load(from.data() + field.offset, 4) == value) continue;
		changed = true;
		Store(out.data() + field.offset, 4, field.bits ? value & LowMask(field.bits) : WireFloat(value));
	}
	if (changed || !IsLegacy(protocol)) SetEntityNumber(out, EntityNumber(to));
	return out;
}
std::uint32_t Angle16(std::uint32_t bits) {
	const float value = BitsFloat(bits);
	if (!std::isfinite(value)) return 0;
	const double scaled = std::trunc(static_cast<double>(value) * 182.0444488525390625);
	if (std::fabs(scaled) >= 9223372036854775808.0) return 0;
	return static_cast<std::uint32_t>(static_cast<std::int64_t>(scaled)) & 0xffff;
}
std::uint32_t AngleFloat(std::uint32_t angle) {
	const int value = static_cast<int>(SignExtend(angle, 16));
	return FloatBits(static_cast<float>(value * (360.0 / 65536.0)));
}
Player ExpectPlayer(int protocol, const Player *from, const Player &to) {
	Player out{};
	if (from) std::memcpy(out.data(), from->data(), PlayerBytes(protocol));
	const Player zero{};
	const Player &base = from ? *from : zero;
	if (protocol == 35) {
		for (const Group35 &group : PlayerGroups35) {
			bool changed = false;
			const unsigned compared = group.flag == 0x40000 ? 1u : group.count;
			for (unsigned i = 0; i < compared; ++i) {
				const unsigned offset = group.offset + i * group.storage;
				const std::uint32_t a = Load(base.data() + offset, group.storage), b = Load(to.data() + offset, group.storage);
				if (group.value == Value35::Float || group.value == Value35::Angle) {
					const float fa = BitsFloat(a), fb = BitsFloat(b);
					changed = changed || !(std::isnan(fa) || std::isnan(fb) || fa == fb);
				} else {
					changed = changed || a != b;
				}
			}
			if (!changed) continue;
			for (unsigned i = 0; i < group.count; ++i) {
				const unsigned offset = group.offset + i * group.storage;
				std::uint32_t value = Load(to.data() + offset, group.storage);
				switch (group.value) {
				case Value35::Byte: value &= 0xff; break;
				case Value35::Char: value = SignExtend(value, 8); break;
				case Value35::Short: value &= 0xffff; break;
				case Value35::Long: break;
				case Value35::Float:
					if (std::isnan(BitsFloat(value))) value |= 0x400000;
					break;
				case Value35::Angle: value = AngleFloat(Angle16(value)); break;
				}
				Store(out.data() + offset, group.storage, value);
			}
		}
		for (const Array35 &list : PlayerArrays35)
			for (unsigned i = 0; i < 16; ++i) {
				const unsigned offset = list.offset + i * list.storage;
				if (Load(base.data() + offset, list.storage) != Load(to.data() + offset, list.storage))
					Store(out.data() + offset, list.storage, Load(to.data() + offset, list.storage));
			}
		return out;
	}
	const NetFieldTable table = PlayerNetFields(protocol);
	for (unsigned i = 0; i < table.count; ++i) {
		const NetField &field = table.fields[i];
		const std::uint32_t value = Load(to.data() + field.offset, 4);
		if (Load(base.data() + field.offset, 4) == value) continue;
		const std::uint32_t stored = field.bits > 0 ? value & LowMask(static_cast<unsigned>(field.bits))
			: field.bits < 0 ? SignExtend(value, static_cast<unsigned>(-field.bits)) : WireFloat(value);
		Store(out.data() + field.offset, 4, stored);
	}
	for (unsigned j = 0; j < 4; ++j)
		for (unsigned i = 0; i < 16; ++i) {
			const unsigned offset = table.arrays[j] + 4 * i;
			const std::uint32_t value = Load(to.data() + offset, 4);
			if (Load(base.data() + offset, 4) != value) Store(out.data() + offset, 4, j == 3 ? value : SignExtend(value, 16));
		}
	return out;
}
Usercmd ExpectUsercmd(int protocol, const Usercmd *from, const Usercmd &to) {
	Usercmd out{};
	if (from) std::memcpy(out.data(), from->data(), UsercmdBytes(protocol));
	const Usercmd zero{};
	const Usercmd &base = from ? *from : zero;
	if (protocol == 40) {
		for (const CommandField &field : Commands40) {
			const std::uint32_t value = Load(to.data() + field.offset, field.storage);
			if (Load(base.data() + field.offset, field.storage) != value) Store(out.data() + field.offset, field.storage, value & LowMask(field.bits));
		}
		return out;
	}
	const CommandField *fields = protocol == 35 ? Commands35 : Commands36;
	for (unsigned i = 0; i < 8; ++i) {
		const CommandField &field = fields[i];
		const std::uint32_t value = Load(to.data() + field.offset, field.storage);
		if (Load(base.data() + field.offset, field.storage) == value) continue;
		Store(out.data() + field.offset, field.storage, i < 3 ? SignExtend(value, 16) : value & 0xff);
	}
	out[0] = to[0];
	return out;
}

// Players whose every change survives the wire, for message sessions.
Player CleanPlayer(Random &random, int protocol, const Player &previous) {
	Player player = previous;
	if (protocol == 35) {
		for (const Group35 &group : PlayerGroups35) {
			if (!random.Chance(40)) continue;
			for (unsigned i = 0; i < group.count; ++i) {
				std::uint32_t value = random.Next();
				switch (group.value) {
				case Value35::Byte: value &= 0xff; break;
				case Value35::Char: value = SignExtend(value, 8); break;
				case Value35::Short:
				case Value35::Long: break;
				case Value35::Float: value = RandomFloat(random, true); break;
				case Value35::Angle: value = AngleFloat(value & 0xffff); break;
				}
				Store(player.data() + group.offset + i * group.storage, group.storage, value);
			}
			// The damage bytes travel only with a new damageEvent.
			if (group.flag == 0x40000 && player[0x60] == previous[0x60]) player[0x60] = static_cast<std::uint8_t>(previous[0x60] + 1);
		}
		for (const Array35 &list : PlayerArrays35)
			for (unsigned i = 0; i < 16; ++i)
				if (random.Chance(10)) Store(player.data() + list.offset + i * list.storage, list.storage, random.Next());
		return player;
	}
	const NetFieldTable table = PlayerNetFields(protocol);
	for (unsigned i = 0; i < table.count; ++i) {
		if (!random.Chance(40)) continue;
		const NetField &field = table.fields[i];
		const std::uint32_t value = field.bits > 0 ? random.Next() & LowMask(static_cast<unsigned>(field.bits))
			: field.bits < 0 ? SignExtend(random.Next(), static_cast<unsigned>(-field.bits)) : RandomFloat(random, true);
		Store(player.data() + field.offset, 4, value);
	}
	for (unsigned j = 0; j < 4; ++j)
		for (unsigned i = 0; i < 16; ++i)
			if (random.Chance(10)) Store(player.data() + table.arrays[j] + 4 * i, 4, j == 3 ? random.Next() : SignExtend(random.Next(), 16));
	return player;
}
bool SamePlayer(int protocol, const Player &a, const Player &b) { return !std::memcmp(a.data(), b.data(), PlayerBytes(protocol)); }

bool SameState(const ClientState &a, const ClientState &b) {
	return a.protocol == b.protocol && a.baselines == b.baselines && a.frames == b.frames && a.valid == b.valid && a.snap == b.snap
		&& a.parseEntitiesNum == b.parseEntitiesNum && a.serverCommandSequence == b.serverCommandSequence && a.csSequence == b.csSequence;
}
// A parse that fails must leave the state and the previous output alone.
bool Parse(int protocol, const Bytes &bytes, ClientState &state, ServerMessage &out, std::string *error = nullptr) {
	const ClientState before = state;
	ServerMessage sentinel;
	ServerCommand marker;
	marker.text = "sentinel";
	sentinel.ops.emplace_back(marker);
	out = sentinel;
	if (ParseServerMessage(protocol, bytes.data(), bytes.size(), state, out, error)) return true;
	CHECK(SameState(before, state) && out.ops.size() == 1 && std::get<ServerCommand>(out.ops[0]).text == "sentinel");
	return false;
}
Bytes Reencode(int protocol, const ServerMessage &message) {
	Bytes buffer(MaxMessageBytes);
	Writer out(buffer.data(), buffer.size());
	for (const ServerOp &op : message.ops) CHECK(WriteServerOp(out, protocol, op));
	buffer.resize(out.Size());
	return buffer;
}
bool SameEntry(const GamestateEntry &a, const GamestateEntry &b) {
	return a.command == b.command && a.index == b.index && a.text == b.text && a.state == b.state && a.delta.number == b.delta.number
		&& a.delta.kind == b.delta.kind && a.delta.mask == b.delta.mask && a.delta.index == b.delta.index;
}

// ---- Bit and byte primitives ----

void Bits() {
	for (unsigned prefix = 0; prefix < 8; ++prefix)
		for (int bits = -31; bits <= 32; ++bits) {
			if (!bits) continue;
			const unsigned count = static_cast<unsigned>(bits < 0 ? -bits : bits);
			for (std::uint32_t value : {0u, 1u, 0x5au, 0x80000000u, 0xffffffffu, 0xa5c36987u, 0x7fffffffu}) {
				Bytes bytes(16, 0xee);
				Writer out(bytes.data(), bytes.size());
				if (prefix) CHECK(out.WriteBits(0x55, static_cast<int>(prefix)));
				CHECK(out.WriteBits(static_cast<std::int32_t>(value), bits));
				CHECK(out.BitPosition() == prefix + count && out.Size() == (prefix + count + 7) / 8 && out.Bit() == (prefix + count) % 8);
				for (std::size_t i = out.Size(); i < bytes.size(); ++i) CHECK(bytes[i] == 0xee);
				if (out.Bit()) CHECK(!(bytes[out.Size() - 1] >> out.Bit()));
				Reader in(bytes.data(), out.Size());
				if (prefix) CHECK(static_cast<std::uint32_t>(in.ReadBits(static_cast<int>(prefix))) == (0x55u & LowMask(prefix)));
				const std::uint32_t expect = bits < 0 ? SignExtend(value, count) : value & LowMask(count);
				CHECK(static_cast<std::uint32_t>(in.ReadBits(bits)) == expect);
				CHECK(in.Good() && in.BitPosition() == out.BitPosition() && in.ReadCount() == out.Size() && in.Bit() == out.Bit());
			}
		}
	// Least significant bits first, each byte from its bit 0.
	{
		std::uint8_t bytes[8] = {};
		Writer out(bytes, sizeof bytes);
		CHECK(out.WriteBits(1, 1) && out.WriteBits(0, 1) && out.WriteBits(0x3f, 6) && out.WriteBits(0x1ff, 9) && out.WriteShort(0x1234));
		CHECK(bytes[0] == 0xfd && bytes[1] == 0xff && bytes[2] == 0x69 && bytes[3] == 0x24 && bytes[4] == 0 && out.Size() == 5 && out.Bit() == 1);
	}
	// Retail value readers: -1 while fewer than 1, 2 or 4 bytes remain counting
	// a partly read byte, which equals too few bits; nothing is consumed.
	for (std::size_t size = 0; size <= 6; ++size)
		for (unsigned prefix = 0; prefix < 8; ++prefix) {
			if (prefix > size * 8) continue;
			Bytes bytes(size);
			for (std::size_t i = 0; i < size; ++i) bytes[i] = static_cast<std::uint8_t>(0x81 + 7 * i);
			Reader base(bytes.data(), size);
			if (prefix) base.ReadBits(static_cast<int>(prefix));
			CHECK(base.Good());
			for (std::size_t need : {std::size_t(1), std::size_t(2), std::size_t(4)}) {
				CHECK(base.Has(need) == (base.BitsLeft() >= 8 * need));
				CHECK(base.Has(need) == (base.Size() - base.ReadCount() >= need));
			}
			Reader in = base;
			const int byte = in.ReadByte();
			if (base.BitsLeft() < 8) CHECK(byte == -1 && in.Good() && in.BitPosition() == base.BitPosition());
			else CHECK(byte >= 0 && in.BitPosition() == base.BitPosition() + 8);
			in = base;
			const int word = in.ReadShort();
			if (base.BitsLeft() < 16) CHECK(word == -1 && in.Good() && in.BitPosition() == base.BitPosition());
			in = base;
			const std::int32_t dword = in.ReadLong();
			if (base.BitsLeft() < 32) CHECK(dword == -1 && in.Good() && in.BitPosition() == base.BitPosition());
			else CHECK(in.BitPosition() == base.BitPosition() + 32);
			in = base;
			const int character = in.ReadChar();
			CHECK(base.BitsLeft() >= 8 ? character >= -128 && character < 128 : character == -1);
			// Bit reads past the end fail, return 0 and consume nothing.
			if (base.BitsLeft() < 32) {
				in = base;
				CHECK(!in.ReadBits(static_cast<int>(base.BitsLeft() + 1)) && !in.Good() && in.BitPosition() == base.BitPosition());
				CHECK(in.ReadByte() == -1 && in.ReadLong() == -1);
			}
		}
	{
		const std::uint8_t data[4] = {0xff, 0x7f, 0x80, 0x01};
		Reader in(data, 4);
		CHECK(in.ReadChar() == -1 && in.Good() && in.ReadCount() == 1);    // 0xFF as a char reads -1 too
		CHECK(in.ReadChar() == 127 && in.ReadChar() == -128 && in.ReadByte() == 1 && in.ReadByte() == -1 && in.Good());
		Reader shorts(data, 4);
		CHECK(shorts.ReadShort() == 0x7fff && shorts.ReadShort() == 0x180 && shorts.ReadShort() == -1);
		Reader longs(data, 4);
		CHECK(longs.ReadLong() == 0x01807fff && longs.ReadLong() == -1 && longs.Good());
	}
	// Bit writes overflow while fewer than 4 bytes are free, writing nothing.
	{
		Bytes bytes(8, 0xee);
		Writer out(bytes.data(), bytes.size());
		CHECK(out.WriteLong(0x04030201) && out.WriteBits(1, 1) && out.Size() == 5);
		const Bytes before = bytes;
		CHECK(!out.WriteBits(1, 1) && !out.Good() && out.Size() == 5 && out.Bit() == 1 && bytes == before);
		CHECK(!out.WriteData("x", 1) && !out.WriteByte(0) && out.Size() == 5 && bytes == before);    // failure is sticky
		Writer exact(bytes.data(), 8);
		CHECK(exact.WriteLong(1) && exact.WriteLong(2) && exact.Size() == 8 && !exact.WriteBits(0, 1));
		Writer last(bytes.data(), 7);
		CHECK(last.WriteBits(0, 3) && last.WriteLong(-1) && last.Size() == 5 && !last.WriteBits(0, 1));
		// Aligned data may use the last bytes.
		Writer data(bytes.data(), 8);
		CHECK(data.WriteLong(0) && data.WriteBits(0, 1) && data.WriteData("ab", 2) && data.Size() == 8 && !data.WriteData("c", 1));
	}
	// Bad widths, null and oversized buffers.
	{
		std::uint8_t bytes[16] = {};
		for (int bits : {0, 33, -32, -100, 1000}) {
			Writer out(bytes, 16);
			CHECK(!out.WriteBits(1, bits) && !out.Good() && !out.Size());
			Reader in(bytes, 16);
			CHECK(!in.ReadBits(bits) && !in.Good() && !in.BitPosition());
		}
		Writer none(nullptr, 0);
		CHECK(none.Good() && !none.WriteBits(0, 1));
		Writer missing(nullptr, 8);
		CHECK(!missing.Good() && !missing.Capacity());
		Reader empty(nullptr, 0);
		CHECK(empty.Good() && empty.ReadByte() == -1 && empty.Good() && !empty.ReadBits(1) && !empty.Good());
		Reader absent(nullptr, 8);
		CHECK(!absent.Good() && !absent.Size() && absent.ReadLong() == -1);
		const auto huge = (std::numeric_limits<std::size_t>::max)() / 8 + 1;
		Reader tooLong(bytes, huge);
		Writer tooLarge(bytes, huge);
		CHECK(!tooLong.Good() && !tooLarge.Good());
	}
}

void Alignment() {
	// After bits in data[0], aligned data lands at data[2]; data[1] is the gap.
	Bytes bytes(16, 0xee);
	Writer out(bytes.data(), bytes.size());
	CHECK(out.WriteBits(5, 3) && out.WriteData("ab", 2));
	CHECK(out.Size() == 4 && out.Bit() == 0 && out.Gaps() == 1 && bytes[0] == 5 && bytes[1] == 0 && bytes[2] == 'a' && bytes[3] == 'b');
	// A zero-length write still steps past the gap; aligned writes add none.
	CHECK(out.WriteBits(1, 1) && out.WriteData(nullptr, 0) && out.Size() == 6 && out.Gaps() == 2 && bytes[5] == 0);
	CHECK(out.WriteData("c", 1) && out.Size() == 7 && out.Gaps() == 2);
	Reader in(bytes.data(), out.Size());
	char text[2] = {};
	CHECK(in.ReadBits(3) == 5 && in.ReadData(text, 2) && text[0] == 'a' && text[1] == 'b' && in.ReadCount() == 4 && in.Gaps() == 1);
	CHECK(in.ReadBits(1) == 1 && in.ReadData(nullptr, 0) && in.ReadCount() == 6 && in.ReadData(text, 1) && text[0] == 'c' && in.Gaps() == 2);
	CHECK(in.ReadByte() == -1 && in.Good());
	// Aligning past the end fails; so does data past the end, untouched.
	const std::uint8_t one[1] = {7};
	Reader end(one, 1);
	CHECK(end.ReadBits(3) == 7 && !end.Align() && !end.Good() && end.BitPosition() == 3);
	Reader data(one, 1);
	char pair[2] = {'x', 'y'};
	CHECK(!data.ReadData(pair, 2) && pair[0] == 'x' && pair[1] == 'y' && !data.Good() && !data.BitPosition());
	// Opcodes stay unaligned: a byte written after bits straddles two bytes.
	Bytes mixed(8);
	Writer w(mixed.data(), mixed.size());
	CHECK(w.WriteBits(3, 2) && w.WriteByte(0xab) && w.WriteString(38, "q"));
	CHECK(mixed[0] == (3 | ((0xab << 2) & 0xff)) && mixed[1] == (0xab >> 6) && mixed[2] == 0 && mixed[3] == 'q' && mixed[4] == 0);
	Reader r(mixed.data(), w.Size());
	std::string q;
	CHECK(r.ReadBits(2) == 3 && r.ReadByte() == 0xab && r.ReadString(38, q) && q == "q" && r.ReadCount() == w.Size());
}

void Strings() {
	for (int protocol : Protocols) {
		const bool legacy = IsLegacy(protocol);
		Bytes bytes(4096);
		// Plain text, bytes above 0x7f and '%'.
		const std::string high = "a\x80\xfe%b";
		Writer out(bytes.data(), bytes.size());
		CHECK(out.WriteString(protocol, "hello") && out.WriteString(protocol, "") && out.WriteString(protocol, high));
		Reader in(bytes.data(), out.Size());
		std::string text;
		CHECK(in.ReadString(protocol, text) && text == "hello" && in.ReadString(protocol, text) && text.empty());
		CHECK(in.ReadString(protocol, text) && text == (legacy ? high : std::string("a...b")) && in.ReadCount() == out.Size());
		// 0xFF: refused by 35/36 writers, it ends 35/36 strings (consumed); 38-40 keep it.
		Writer ff(bytes.data(), bytes.size());
		CHECK(ff.WriteString(protocol, "x\xffy") == !legacy && ff.Good() == !legacy);
		const Bytes raw = {'x', 0xff, 'y', 0, 'z', 0};
		Reader rff(raw.data(), raw.size());
		CHECK(rff.ReadString(protocol, text));
		if (legacy) CHECK(text == "x" && rff.ReadCount() == 2 && rff.ReadString(protocol, text) && text == "y");
		else CHECK(text == "x\xffy" && rff.ReadCount() == 4);
		// '%' travels as sent; 38-40 readers store '.'.
		const Bytes percent = {'5', '0', '%', 0};
		Reader rp(percent.data(), percent.size());
		CHECK(rp.ReadString(protocol, text) && text == (legacy ? "50%" : "50."));
		// 1022 characters round-trip; 1023 are refused; a received 1023 stops before its terminator.
		Writer longest(bytes.data(), bytes.size());
		CHECK(longest.WriteString(protocol, std::string(1022, 'k')));
		Reader rl(bytes.data(), longest.Size());
		CHECK(rl.ReadString(protocol, text) && text.size() == 1022 && rl.ReadCount() == 1023);
		Writer tooLong(bytes.data(), bytes.size());
		CHECK(!tooLong.WriteString(protocol, std::string(1023, 'k')) && !tooLong.Size());
		Bytes cap(1026, 'c');
		cap[1023] = 0;
		cap[1024] = 5;
		cap[1025] = 0;
		Reader rc(cap.data(), cap.size());
		CHECK(rc.ReadString(protocol, text) && text.size() == 1023 && rc.ReadCount() == 1023 && rc.ReadByte() == 0);
		// Embedded NULs are refused; a string cut by the end fails untouched.
		Writer nul(bytes.data(), bytes.size());
		CHECK(!nul.WriteString(protocol, std::string("a\0b", 3)) && !nul.Size());
		const Bytes cut = {'a', 'b'};
		Reader rcut(cut.data(), cut.size());
		text = "kept";
		CHECK(!rcut.ReadString(protocol, text) && text == "kept" && !rcut.Good() && !rcut.ReadCount());
	}
	std::string text = "kept";
	const Bytes hello = {'h', 'i', 0};
	for (int bad : {0, 33, 34, 37, 41, 43, 68, -1}) {
		Reader in(hello.data(), hello.size());
		CHECK(!in.ReadString(bad, text) && text == "kept" && !in.Good());
		Bytes bytes(16);
		Writer out(bytes.data(), bytes.size());
		CHECK(!out.WriteString(bad, "hi") && !out.Size() && !out.Good());
	}
}

void Floats() {
	std::uint32_t code = 0;
	for (int i = -4096; i <= 4095; ++i) CHECK(SmallFloat(FloatBits(static_cast<float>(i)), code) && code == static_cast<std::uint32_t>(i + 4096));
	for (float value : {-4097.0f, 4096.0f, 0.5f, -0.5f, 4095.5f, -4096.5f, 1e10f, -2147483648.0f, 2147483648.0f, 4294967296.0f, 1e-40f, 1e30f})
		CHECK(!SmallFloat(FloatBits(value), code));
	CHECK(SmallFloat(0x80000000u, code) && code == 4096);    // -0.0
	for (std::uint32_t nan : {0x7fc00000u, 0x7f800001u, 0xffffffffu, 0xff800001u, 0x7fbfffffu}) CHECK(SmallFloat(nan, code) && code == 4096);
	CHECK(!SmallFloat(0x7f800000u, code) && !SmallFloat(0xff800000u, code));
	// The 13-bit code arrives through fild/fstp, the 32-bit form as raw bits.
	for (std::uint32_t bits : {FloatBits(-4096.0f), FloatBits(4095.0f), 0x80000000u, 0x7fc12345u, 0x7f812345u, FloatBits(0.25f), 0x7f800000u}) {
		std::uint8_t bytes[16] = {};
		Writer out(bytes, sizeof bytes);
		Entity from{}, to{};
		SetEntityNumber(to, 9);
		Store(to.data() + 24, 4, bits);
		CHECK(WriteDeltaEntity(out, 38, &from, &to, false));
		Reader in(bytes, out.Size());
		Entity got{};
		EntityDelta delta;
		CHECK(ReadDeltaEntity(in, 38, from, got, in.ReadBits(10), &delta));
		CHECK(Load(got.data() + 24, 4) == WireFloat(bits) && Bit(delta.fullFloats, 5) == !SmallFloat(bits, code));
	}
	// Protocol-35 angles: every short survives float and back.
	for (int value = -32768; value < 32768; ++value) {
		CHECK(DecodeAngle(value) == AngleFloat(static_cast<std::uint32_t>(value)));
		CHECK(EncodeAngle(DecodeAngle(value)) == (static_cast<std::uint32_t>(value) & 0xffff));
	}
	CHECK(EncodeAngle(FloatBits(90.0f)) == 16384 && EncodeAngle(FloatBits(-90.0f)) == 49152 && EncodeAngle(FloatBits(360.0f)) == 0);
	CHECK(EncodeAngle(FloatBits(1.0f)) == 182 && EncodeAngle(FloatBits(-1.0f)) == 65536 - 182 && EncodeAngle(FloatBits(1e30f)) == 0);
	CHECK(EncodeAngle(0x7fc00000u) == 0 && EncodeAngle(0x7f800000u) == 0 && EncodeAngle(0xff800000u) == 0);
	Random random(5);
	for (int i = 0; i < 200000; ++i) {
		const std::uint32_t bits = random.Next();
		CHECK(EncodeAngle(bits) == Angle16(bits));
	}
	CHECK(QuietFloat(0x7f812345u) == 0x7fc12345u && QuietFloat(0x7fc00001u) == 0x7fc00001u && QuietFloat(0x3f800000u) == 0x3f800000u);
}

// ---- Deltas ----

// Every shorter input fails without output or movement, and every smaller
// buffer fails without writing a byte.
template<class Encode, class Decode>
void Truncations(const Bytes &encoded, std::size_t bits, unsigned prefix, Encode &&encode, Decode &&decode) {
	for (std::size_t size = 0; size < (bits + 7) / 8; ++size) {
		Reader in(encoded.data(), size);
		if (prefix && size * 8 >= prefix) in.ReadBits(static_cast<int>(prefix));
		const std::size_t position = in.BitPosition();
		CHECK(!decode(in) && !in.Good() && in.BitPosition() == position);
	}
	for (std::size_t capacity = 0; capacity < (bits + 7) / 8 + 4; ++capacity) {
		Bytes bytes(capacity + 8, 0xa5);
		Writer out(bytes.data(), capacity);
		if (prefix && !out.WriteBits(0x55, static_cast<int>(prefix))) continue;
		const Bytes afterPrefix = bytes;
		const std::size_t position = out.BitPosition();
		if (encode(out)) CHECK(out.BitPosition() == bits && !std::memcmp(bytes.data(), encoded.data(), out.Size()));
		else CHECK(!out.Good() && out.BitPosition() == position && bytes == afterPrefix);
	}
}

void EntityDeltas() {
	for (int protocol : Protocols) {
		const bool legacy = IsLegacy(protocol);
		Random random(1000 + static_cast<std::uint64_t>(protocol));
		unsigned indexed = 0, explicitMasks = 0, forced = 0;
		for (unsigned round = 0; round < 4000; ++round) {
			const auto number = static_cast<std::int32_t>(legacy ? 1 + random.Below(1023) : random.Below(1023));
			const bool clean = random.Chance(30);
			const Entity from = RandomEntity(random, static_cast<std::int32_t>(random.Below(4096)), 40, clean);
			Entity to = from;
			SetEntityNumber(to, number);
			const unsigned shape = random.Below(10);
			if (shape == 0) {
				// unchanged
			} else if (shape < 4) {
				// Exactly the fields of one common mask, from either table.
				const std::uint64_t mask = (random.Chance(50) ? CommonMasks35 : CommonMasks38)[random.Below(27)];
				for (unsigned i = 0; i < 50; ++i) {
					if (!Bit(mask, i)) continue;
					const EntityField &field = EntityFields[i];
					const std::uint32_t old = Load(from.data() + field.offset, 4);
					std::uint32_t value = field.bits ? random.Next() : RandomFloat(random, clean);
					if (value == old) value ^= 1;
					Store(to.data() + field.offset, 4, value);
				}
			} else {
				const Entity other = RandomEntity(random, number, 15 + random.Below(60), clean);
				for (unsigned i = 0; i < 50; ++i)
					if (random.Chance(30)) std::memcpy(to.data() + EntityFields[i].offset, other.data() + EntityFields[i].offset, 4);
			}
			const bool force = random.Chance(50);
			const unsigned prefix = random.Below(8);
			const std::uint64_t mask = EntityChangeMask(from, to);
			Bytes bytes(512, 0xa5);
			Writer out(bytes.data(), bytes.size());
			if (prefix) CHECK(out.WriteBits(0x55, static_cast<int>(prefix)));
			CHECK(WriteDeltaEntity(out, protocol, &from, &to, force));
			if (!mask && !force) {
				CHECK(out.BitPosition() == prefix);
				continue;
			}
			bytes.resize(out.Size());
			// The caller reads the number, then the delta.
			const auto decode = [&](Reader &in, Entity &decoded, EntityDelta &delta) {
				Reader next = in;
				const std::int32_t wire = next.ReadBits(10);
				if (!next.Good() || !ReadDeltaEntity(next, protocol, from, decoded, wire, &delta)) return in.Fail();
				in = next;
				return true;
			};
			Reader in(bytes.data(), bytes.size());
			if (prefix) in.ReadBits(static_cast<int>(prefix));
			Entity decoded;
			decoded.fill(0x5a);
			EntityDelta delta;
			CHECK(decode(in, decoded, delta) && in.BitPosition() == out.BitPosition() && delta.number == number);
			CHECK(decoded == ExpectEntity(protocol, from, to));
			if (!mask) {
				CHECK(delta.kind == DeltaKind::Unchanged && out.BitPosition() == prefix + 12);
				++forced;
			} else {
				CHECK(delta.kind == DeltaKind::Changed && delta.mask == mask && delta.index == CommonMaskIndex(protocol, mask));
				CHECK(legacy || delta.index == 31);
				++(delta.index == 31 ? explicitMasks : indexed);
				for (unsigned i = 0; i < 50; ++i) {
					std::uint32_t code = 0;
					if (Bit(mask, i) && !EntityFields[i].bits)
						CHECK(Bit(delta.fullFloats, i) == !SmallFloat(Load(to.data() + EntityFields[i].offset, 4), code));
				}
			}
			// The decoded record re-encodes to the same bits from the decoded state.
			Bytes again(512, 0x3c);
			Writer re(again.data(), again.size());
			if (prefix) CHECK(re.WriteBits(0x55, static_cast<int>(prefix)));
			CHECK(WriteEntityRecord(re, protocol, delta, decoded) && re.BitPosition() == out.BitPosition());
			CHECK(!std::memcmp(again.data(), bytes.data(), bytes.size()));
			if (round % 16 == 0) {
				Entity sentinel;
				sentinel.fill(0x77);
				Truncations(bytes, out.BitPosition(), prefix,
					[&](Writer &w) { return WriteDeltaEntity(w, protocol, &from, &to, force); },
					[&](Reader &r) {
						Entity target = sentinel;
						EntityDelta record;
						record.number = -5;
						const bool ok = decode(r, target, record);
						if (!ok) CHECK(target == sentinel && record.number == -5);
						return ok;
					});
			}
		}
		CHECK(legacy ? indexed > 100 : indexed == 0);
		CHECK(explicitMasks > 1000 && forced > 50);

		// Removal: number, 1. It decodes to zero, numbered 1023 in 38-40.
		const Entity entity = RandomEntity(random, legacy ? 17 : 0, 60, false);
		std::uint8_t bytes[512] = {};
		Writer out(bytes, sizeof bytes);
		CHECK(WriteDeltaEntity(out, protocol, &entity, nullptr, false) && out.BitPosition() == 11);
		CHECK(WriteDeltaEntity(out, protocol, nullptr, nullptr, true) && out.BitPosition() == 11);
		Reader in(bytes, out.Size());
		Entity removed;
		removed.fill(0x33);
		EntityDelta delta;
		CHECK(ReadDeltaEntity(in, protocol, entity, removed, in.ReadBits(10), &delta) && delta.kind == DeltaKind::Removed);
		Entity zero{};
		if (!legacy) SetEntityNumber(zero, 1023);
		CHECK(removed == zero && delta.number == (legacy ? 17 : 0));
		// A null `from` is the zero state.
		Writer fromNull(bytes, sizeof bytes);
		const Entity blank{};
		Entity target{};
		SetEntityNumber(target, 5);
		Store(target.data() + 160, 4, 9);
		CHECK(WriteDeltaEntity(fromNull, protocol, nullptr, &target, false));
		Reader rn(bytes, fromNull.Size());
		Entity got{};
		CHECK(ReadDeltaEntity(rn, protocol, blank, got, rn.ReadBits(10)) && got == target);
		// Unchanged records: 35/36 keep the source's number, so a never-sent
		// baseline decodes as number 0; 38-40 take the wire number.
		Writer same(bytes, sizeof bytes);
		CHECK(WriteDeltaEntity(same, protocol, &blank, &blank, true) == !legacy);
		SetEntityNumber(target, 44);
		Entity baseline = target;
		SetEntityNumber(baseline, 0);
		Writer forcedOut(bytes, sizeof bytes);
		CHECK(WriteEntityRecord(forcedOut, protocol, EntityDelta{44, DeltaKind::Unchanged, 31, 0, 0}, target));
		Reader rf(bytes, forcedOut.Size());
		CHECK(ReadDeltaEntity(rf, protocol, baseline, got, rf.ReadBits(10), &delta) && delta.kind == DeltaKind::Unchanged);
		CHECK(EntityNumber(got) == (legacy ? 0 : 44) && Load(got.data() + 160, 4) == 9);
		// Numbers outside the protocol's range are refused on both sides, untouched.
		for (std::int32_t bad : {-1, 1024, 65536, legacy ? 0 : 1023}) {
			Entity numbered = target;
			SetEntityNumber(numbered, bad);
			std::uint8_t buffer[64];
			std::memset(buffer, 0xcd, sizeof buffer);
			Writer w(buffer, sizeof buffer);
			CHECK(!WriteDeltaEntity(w, protocol, &blank, &numbered, true) && !w.Size());
			for (std::uint8_t byte : buffer) CHECK(byte == 0xcd);
			Writer removal(buffer, sizeof buffer);
			CHECK(!WriteDeltaEntity(removal, protocol, &numbered, nullptr, true) && !removal.Size());
			Writer quiet(buffer, sizeof buffer);
			CHECK(!WriteDeltaEntity(quiet, protocol, &numbered, &numbered, false));    // checked even when nothing is sent
			Reader r(bytes, sizeof bytes);
			Entity untouched = numbered;
			CHECK(!ReadDeltaEntity(r, protocol, blank, untouched, bad) && untouched == numbered && !r.BitPosition());
		}
		// Readers honour every table index 0..30 of their protocol.
		for (unsigned index = 0; index < 31; ++index) {
			const std::uint64_t mask = CommonMasks(protocol)[index];
			const Entity changed = RandomEntity(random, 12, 100, true);
			EntityDelta record{12, DeltaKind::Changed, static_cast<std::uint8_t>(index), mask, 0};
			for (unsigned i = 0; i < 50; ++i) {
				std::uint32_t code = 0;
				if (Bit(mask, i) && !EntityFields[i].bits && !SmallFloat(Load(changed.data() + EntityFields[i].offset, 4), code))
					record.fullFloats |= std::uint64_t(1) << i;
			}
			Writer w(bytes, sizeof bytes);
			CHECK(WriteEntityRecord(w, protocol, record, changed));
			Reader r(bytes, w.Size());
			CHECK(ReadDeltaEntity(r, protocol, blank, got, r.ReadBits(10), &delta) && delta.index == index && delta.mask == mask);
			CHECK(r.BitPosition() == w.BitPosition() && EntityNumber(got) == 12);
			for (unsigned i = 0; i < 50; ++i) {
				const std::uint32_t value = Load(changed.data() + EntityFields[i].offset, 4);
				CHECK(Load(got.data() + EntityFields[i].offset, 4) == (Bit(mask, i) ? value : 0u));
			}
			// A mask that is not the entry's is refused.
			record.mask ^= 1;
			record.fullFloats &= record.mask;
			Writer wrong(bytes, sizeof bytes);
			CHECK(!WriteEntityRecord(wrong, protocol, record, changed) && !wrong.Size());
		}
		// Bad records: mask bits past 50, float forms on other fields, unknown kinds, index 32.
		for (const EntityDelta &bad : {EntityDelta{3, DeltaKind::Changed, 31, std::uint64_t(1) << 50, 0},
				EntityDelta{3, DeltaKind::Changed, 31, 1, 1}, EntityDelta{3, DeltaKind::Changed, 31, std::uint64_t(1) << 5, std::uint64_t(1) << 6},
				EntityDelta{3, static_cast<DeltaKind>(9), 31, 0, 0}, EntityDelta{3, DeltaKind::Changed, 32, 1, 0}}) {
			Writer w(bytes, sizeof bytes);
			CHECK(!WriteEntityRecord(w, protocol, bad, target) && !w.Size());
		}
		// Integer fields send their low bits: the table's widths apply.
		Entity wide{};
		SetEntityNumber(wide, 7);
		for (const EntityField &field : EntityFields)
			if (field.bits) Store(wide.data() + field.offset, 4, 0xfffffffeu);
		Writer w(bytes, sizeof bytes);
		CHECK(WriteDeltaEntity(w, protocol, &blank, &wide, false));
		Reader r(bytes, w.Size());
		CHECK(ReadDeltaEntity(r, protocol, blank, got, r.ReadBits(10)));
		for (const EntityField &field : EntityFields)
			if (field.bits) CHECK(Load(got.data() + field.offset, 4) == (0xfffffffeu & LowMask(field.bits)));
	}
	Entity a{}, b{};
	SetEntityNumber(b, 3);
	b[4] = 1;
	std::uint8_t bytes[64] = {};
	for (int bad : {0, 33, 34, 37, 41}) {
		Writer w(bytes, sizeof bytes);
		CHECK(!WriteDeltaEntity(w, bad, &a, &b, true) && !w.Size());
		Reader r(bytes, sizeof bytes);
		CHECK(!ReadDeltaEntity(r, bad, a, b, 3) && !r.Good());
	}
}

void PlayerDeltas() {
	for (int protocol : Protocols) {
		Random random(2000 + static_cast<std::uint64_t>(protocol));
		const std::size_t used = PlayerBytes(protocol);
		for (unsigned round = 0; round < 3000; ++round) {
			Player from{};
			for (std::size_t i = 0; i < used; ++i) from[i] = static_cast<std::uint8_t>(random.Next());
			Player to = from;
			// Change random dwords, floats among them.
			for (unsigned k = random.Below(40); k; --k) {
				const std::size_t offset = 4 * random.Below(static_cast<std::uint32_t>(used / 4));
				Store(to.data() + offset, 4, random.Chance(50) ? RandomFloat(random, random.Chance(50)) : random.Next());
			}
			for (std::size_t i = used; i < MaxPlayerBytes; ++i) to[i] = static_cast<std::uint8_t>(random.Next());    // ignored
			const bool nullFrom = random.Chance(10);
			const Player *source = nullFrom ? nullptr : &from;
			const unsigned prefix = random.Below(8);
			Bytes bytes(2048, 0xa5);
			Writer out(bytes.data(), bytes.size());
			if (prefix) CHECK(out.WriteBits(0x55, static_cast<int>(prefix)));
			CHECK(WriteDeltaPlayer(out, protocol, source, to));
			bytes.resize(out.Size());
			Reader in(bytes.data(), bytes.size());
			if (prefix) in.ReadBits(static_cast<int>(prefix));
			Player decoded;
			decoded.fill(0x5a);
			PlayerDelta delta;
			CHECK(ReadDeltaPlayer(in, protocol, source, decoded, &delta) && in.BitPosition() == out.BitPosition());
			CHECK(decoded == ExpectPlayer(protocol, source, to));
			if (protocol == 35) {
				CHECK(Bit(delta.changed, 15) == ((delta.changed >> 16) != 0));
			} else {
				const NetFieldTable table = PlayerNetFields(protocol);
				const Player zero{};
				const Player &base = nullFrom ? zero : from;
				for (unsigned i = 0; i < table.count; ++i) {
					const std::uint32_t value = Load(to.data() + table.fields[i].offset, 4);
					const bool changed = Load(base.data() + table.fields[i].offset, 4) != value;
					std::uint32_t code = 0;
					CHECK(Bit(delta.changed, i) == changed);
					CHECK(Bit(delta.fullFloats, i) == (changed && !table.fields[i].bits && !SmallFloat(value, code)));
				}
			}
			// The record re-encodes to the same bits; from the decoded state in
			// 36-40, from the sent one in 35, whose reader quiets signaling NaNs.
			Bytes again(2048, 0x3c);
			Writer re(again.data(), again.size());
			if (prefix) CHECK(re.WriteBits(0x55, static_cast<int>(prefix)));
			CHECK(WritePlayerRecord(re, protocol, delta, protocol == 35 ? to : decoded) && re.BitPosition() == out.BitPosition());
			CHECK(!std::memcmp(again.data(), bytes.data(), bytes.size()));
			if (round % 32 == 0) {
				Player sentinel;
				sentinel.fill(0x77);
				Truncations(bytes, out.BitPosition(), prefix, [&](Writer &w) { return WriteDeltaPlayer(w, protocol, source, to); },
					[&](Reader &r) {
						Player target = sentinel;
						const bool ok = ReadDeltaPlayer(r, protocol, source, target);
						if (!ok) CHECK(target == sentinel);
						return ok;
					});
			}
		}
	}
	// Protocol 35 specifics.
	{
		std::uint8_t bytes[1024] = {};
		Player from{}, to{};
		// -0.0 against 0.0, and a NaN against anything, raise no flag (C3 only).
		Store(to.data() + 0x08, 4, 0x80000000u);
		Store(to.data() + 0x14, 4, 0x7fc00000u);
		Store(from.data() + 0x50, 4, 0x7f812345u);
		Store(to.data() + 0x50, 4, FloatBits(45.0f));
		Writer quiet(bytes, sizeof bytes);
		CHECK(WriteDeltaPlayer(quiet, 35, &from, to) && quiet.BitPosition() == 16 && !bytes[0] && !bytes[1]);
		// damageEvent alone decides PS_DAMAGE_EVENT, which then sends all four bytes.
		to = from;
		to[0x61] = 9;
		Writer damage(bytes, sizeof bytes);
		CHECK(WriteDeltaPlayer(damage, 35, &from, to) && damage.BitPosition() == 16);
		to[0x60] = 1;
		Writer both(bytes, sizeof bytes);
		CHECK(WriteDeltaPlayer(both, 35, &from, to) && both.BitPosition() == 64);
		CHECK(bytes[0] == 0 && bytes[1] == 0x80 && bytes[2] == 4 && bytes[3] == 0 && bytes[4] == 1 && bytes[5] == 9 && !bytes[6] && !bytes[7]);
		// PS_MOREBITS only with a high flag; a low flag alone is one short.
		to = from;
		to[0x31] = 3;    // PS_TORSOANIM 0x1000
		Writer low(bytes, sizeof bytes);
		CHECK(WriteDeltaPlayer(low, 35, &from, to) && low.BitPosition() == 24 && bytes[0] == 0 && bytes[1] == 0x10 && bytes[2] == 3);
		// A group moved by one member carries a signaling NaN, which the reader quiets.
		to = from;
		Store(to.data() + 0x08, 4, 0x7f800123u);
		Store(to.data() + 0x0c, 4, FloatBits(5.0f));
		Store(to.data() + 0xa4 + 4 * 15, 4, 0xdeadbeefu);
		Writer mixed(bytes, sizeof bytes);
		CHECK(WriteDeltaPlayer(mixed, 35, &from, to));
		Reader in(bytes, mixed.Size());
		Player got{};
		PlayerDelta delta;
		CHECK(ReadDeltaPlayer(in, 35, &from, got, &delta) && delta.changed == (0x8000u | 0x800000u | 1u) && delta.masks[3] == 0x8000);
		CHECK(Load(got.data() + 0x08, 4) == 0x7fc00123u && Load(got.data() + 0x0c, 4) == FloatBits(5.0f));
		CHECK(Load(got.data() + 0xa4 + 60, 4) == 0xdeadbeefu);
		// Unknown flag bits are refused, on read and in records.
		for (std::uint32_t unknown : {0x80000u, 0x20000000u, 0x40000000u, 0x80000000u}) {
			const std::uint8_t raw[4] = {0x00, 0x80, static_cast<std::uint8_t>(unknown >> 16), static_cast<std::uint8_t>(unknown >> 24)};
			Reader r(raw, sizeof raw);
			Player target;
			target.fill(0x44);
			const Player before = target;
			CHECK(!ReadDeltaPlayer(r, 35, nullptr, target) && target == before && !r.BitPosition());
			Writer w(bytes, sizeof bytes);
			CHECK(!WritePlayerRecord(w, 35, PlayerDelta{0x8000u | unknown, 0, 0, {}}, from) && !w.Size());
		}
		// A high flag without PS_MOREBITS cannot be written.
		Writer w(bytes, sizeof bytes);
		CHECK(!WritePlayerRecord(w, 35, PlayerDelta{0x20000u, 0, 0, {}}, from) && !w.Size());
		// PS_MOREBITS with a zero high half is read, as retail does, and re-encoded as received.
		const std::uint8_t full[16] = {0x01, 0x80, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3f, 0, 0, 0, 0, 0, 0, 0, 0};
		Reader r(full, sizeof full);
		CHECK(ReadDeltaPlayer(r, 35, nullptr, got, &delta) && delta.changed == 0x8001u && BitsFloat(Load(got.data() + 8, 4)) == 1.0f);
		Writer again(bytes, sizeof bytes);
		CHECK(WritePlayerRecord(again, 35, delta, got) && again.Size() == 16 && !std::memcmp(bytes, full, 16));
	}
	// 36-40: signed widths and short arrays sign-extend; powerups are longs.
	for (int protocol : {36, 38, 39, 40}) {
		const NetFieldTable table = PlayerNetFields(protocol);
		Player from{}, to{};
		for (unsigned i = 0; i < table.count; ++i)
			if (table.fields[i].bits) Store(to.data() + table.fields[i].offset, 4, 0xffffffffu);
		for (unsigned j = 0; j < 4; ++j) Store(to.data() + table.arrays[j] + 8, 4, 0xffff8001u);
		std::uint8_t bytes[1024] = {};
		Writer out(bytes, sizeof bytes);
		CHECK(WriteDeltaPlayer(out, protocol, &from, to));
		Reader in(bytes, out.Size());
		Player got{};
		CHECK(ReadDeltaPlayer(in, protocol, &from, got));
		for (unsigned i = 0; i < table.count; ++i) {
			const int bits = table.fields[i].bits;
			if (bits) CHECK(Load(got.data() + table.fields[i].offset, 4) == (bits < 0 ? 0xffffffffu : LowMask(static_cast<unsigned>(bits))));
		}
		for (unsigned j = 0; j < 4; ++j) CHECK(Load(got.data() + table.arrays[j] + 8, 4) == 0xffff8001u);
		Store(to.data() + table.arrays[0] + 8, 4, 0x00018001u);
		Writer wide(bytes, sizeof bytes);
		CHECK(WriteDeltaPlayer(wide, protocol, &from, to));
		Reader r(bytes, wide.Size());
		CHECK(ReadDeltaPlayer(r, protocol, &from, got) && Load(got.data() + table.arrays[0] + 8, 4) == 0xffff8001u);
		for (const PlayerDelta &bad : {PlayerDelta{std::uint64_t(1) << table.count, 0, 0, {}}, PlayerDelta{1, 1, 0, {}}, PlayerDelta{0, 0, 16, {}}}) {
			Writer w(bytes, sizeof bytes);
			CHECK(!WritePlayerRecord(w, protocol, bad, to) && !w.Size());
		}
	}
	std::uint8_t bytes[64] = {};
	Player player{};
	for (int bad : {0, 34, 37, 41}) {
		Writer w(bytes, sizeof bytes);
		CHECK(!WriteDeltaPlayer(w, bad, nullptr, player) && !w.Size());
		Reader r(bytes, sizeof bytes);
		CHECK(!ReadDeltaPlayer(r, bad, nullptr, player) && !r.Good());
	}
}

void UsercmdDeltas() {
	for (int protocol : Protocols) {
		Random random(3000 + static_cast<std::uint64_t>(protocol));
		const std::size_t size = UsercmdBytes(protocol);
		Usercmd previous{};
		for (unsigned round = 0; round < 5000; ++round) {
			Usercmd cmd = previous;
			for (std::size_t i = 0; i < size; ++i)
				if (random.Chance(15)) cmd[i] = static_cast<std::uint8_t>(random.Next());
			for (std::size_t i = size; i < MaxUsercmdBytes; ++i) cmd[i] = static_cast<std::uint8_t>(random.Next());
			const Usercmd *source = random.Chance(10) ? nullptr : &previous;
			const unsigned prefix = random.Below(8);
			Bytes bytes(128, 0xa5);
			Writer out(bytes.data(), bytes.size());
			if (prefix) CHECK(out.WriteBits(0x55, static_cast<int>(prefix)));
			CHECK(WriteDeltaUsercmd(out, protocol, source, cmd));
			bytes.resize(out.Size());
			Reader in(bytes.data(), bytes.size());
			if (prefix) in.ReadBits(static_cast<int>(prefix));
			Usercmd decoded;
			decoded.fill(0x5a);
			CHECK(ReadDeltaUsercmd(in, protocol, source, decoded) && in.BitPosition() == out.BitPosition());
			CHECK(decoded == ExpectUsercmd(protocol, source, cmd));
			// 35-39 stay byte-sized: a flag byte, the flagged fields, msec.
			if (protocol != 40) CHECK((out.BitPosition() - prefix) % 8 == 0);
			if (round % 64 == 0) {
				Usercmd sentinel;
				sentinel.fill(0x77);
				Truncations(bytes, out.BitPosition(), prefix, [&](Writer &w) { return WriteDeltaUsercmd(w, protocol, source, cmd); },
					[&](Reader &r) {
						Usercmd target = sentinel;
						const bool ok = ReadDeltaUsercmd(r, protocol, source, target);
						if (!ok) CHECK(target == sentinel);
						return ok;
					});
			}
			previous = decoded;
		}
	}
	// Angles: 35 keeps 16 bits, 36-39 sign-extend the 16 sent bits, 40 does not.
	for (int protocol : Protocols) {
		Usercmd cmd{};
		const unsigned angle = protocol == 40 ? 8 : 4, storage = protocol == 35 ? 2 : 4;
		Store(cmd.data() + angle, storage, 40000);
		Store(cmd.data() + angle + storage, storage, 0xfffffffbu);
		if (protocol == 40) Store(cmd.data(), 4, 0x12345678u);
		else cmd[0] = 99;
		std::uint8_t bytes[64] = {};
		Writer out(bytes, sizeof bytes);
		CHECK(WriteDeltaUsercmd(out, protocol, nullptr, cmd));
		Reader in(bytes, out.Size());
		Usercmd got{};
		CHECK(ReadDeltaUsercmd(in, protocol, nullptr, got));
		if (protocol == 35) CHECK(Load(got.data() + 4, 2) == 40000 && Load(got.data() + 6, 2) == 0xfffb && got[0] == 99);
		else if (protocol == 40) CHECK(Load(got.data() + 8, 4) == 40000 && Load(got.data() + 12, 4) == 0xfffb && Load(got.data(), 4) == 0x12345678u);
		else CHECK(Load(got.data() + 4, 4) == static_cast<std::uint32_t>(40000 - 65536) && Load(got.data() + 8, 4) == 0xfffffffbu && got[0] == 99);
	}
}

// ---- Server messages ----

void ServerSession(int protocol) {
	const bool legacy = IsLegacy(protocol);
	Random random(4000 + static_cast<std::uint64_t>(protocol));
	Configstrings configstrings;
	configstrings[0] = "\\protocol\\" + std::to_string(protocol) + "\\mapname\\q3test1";
	configstrings[1] = "\\sv_serverid\\1234";
	configstrings[5] = std::string(200, 'x');
	configstrings[1023] = "last";
	const auto baselines = std::make_unique<Baselines>();
	for (unsigned k = 0; k < 60; ++k) {
		const auto number = static_cast<std::int32_t>(1 + random.Below(1000));
		(*baselines)[static_cast<std::size_t>(number)] = CleanEntity(random, number, 50);
	}
	Bytes buffer(MaxMessageBytes);
	Writer out(buffer.data(), buffer.size());
	CHECK(WriteServerGamestate(out, protocol, 77, configstrings, *baselines));
	const Bytes gamestate = Contents(out);
	ClientState client(protocol);
	ServerMessage message;
	std::string error;
	CHECK(Parse(protocol, gamestate, client, message, &error));
	CHECK(message.ops.size() == 1 && !message.gaps && !message.padding);
	const Gamestate &game = std::get<Gamestate>(message.ops[0]);
	CHECK(game.sequence == (protocol == 35 ? 0 : 77) && game.terminated);
	// Every non-empty configstring in index order, then every selected baseline.
	std::vector<std::int32_t> indexes, numbers, expected;
	for (const GamestateEntry &entry : game.entries) {
		if (entry.command == 3) {
			CHECK(numbers.empty() && entry.text == configstrings[static_cast<std::size_t>(entry.index)]);
			indexes.push_back(entry.index);
		} else {
			CHECK(entry.command == 4 && entry.delta.number == entry.index && entry.state == (*baselines)[static_cast<std::size_t>(entry.index)]);
			numbers.push_back(entry.index);
		}
	}
	for (std::int32_t n = 0; n < 1024; ++n)
		if ((*baselines)[static_cast<std::size_t>(n)] != Entity{}) expected.push_back(n);
	CHECK(indexes == (std::vector<std::int32_t>{0, 1, 5, 1023}) && numbers == expected);
	for (std::int32_t n = 0; n < 1024; ++n) CHECK(client.Baseline(n) == (*baselines)[static_cast<std::size_t>(n)]);
	CHECK(Reencode(protocol, message) == gamestate);
	CHECK(client.serverCommandSequence == (legacy ? 0 : 77) && client.csSequence == (protocol == 36 ? 77 : 0));

	std::vector<Snapshot> history;
	std::vector<Entity> live;
	Player player{};
	unsigned deltas = 0, commands = 0;
	std::int32_t sequence = 77;
	for (std::int32_t round = 1; round <= 90; ++round) {
		std::vector<Entity> next;
		for (const Entity &entity : live) {
			if (random.Chance(8)) continue;
			Entity changed = entity;
			if (random.Chance(50)) {
				const Entity other = CleanEntity(random, EntityNumber(entity), 100);
				for (unsigned i = 0; i < 50; ++i)
					if (random.Chance(8)) std::memcpy(changed.data() + EntityFields[i].offset, other.data() + EntityFields[i].offset, 4);
			}
			next.push_back(changed);
		}
		for (unsigned k = random.Below(5); k; --k) {
			const auto number = static_cast<std::int32_t>(legacy ? 1 + random.Below(1000) : random.Below(1001));
			if (std::any_of(next.begin(), next.end(), [&](const Entity &e) { return EntityNumber(e) == number; })) continue;
			const Entity &base = (*baselines)[static_cast<std::size_t>(number)];
			next.push_back(base != Entity{} && random.Chance(50) ? base : CleanEntity(random, number, 40));
		}
		std::sort(next.begin(), next.end(), [](const Entity &a, const Entity &b) { return EntityNumber(a) < EntityNumber(b); });
		live = next;
		player = CleanPlayer(random, protocol, player);
		Snapshot frame;
		frame.reliableAcknowledge = legacy ? 0 : round;
		frame.serverTime = 1000 + 50 * round;
		frame.messageNum = 5000 + round;
		frame.csSequence = legacy ? round : 0;
		frame.lastUsercmd = 3000 + round;
		frame.snapFlags = static_cast<std::int32_t>(random.Below(protocol == 35 ? 2 : 4));
		frame.areamask.resize(random.Chance(10) ? 32 : random.Below(5));
		for (auto &byte : frame.areamask) byte = static_cast<std::uint8_t>(random.Next());
		frame.player = player;
		frame.entities = live;
		const Snapshot *from = nullptr;
		if (round > 1 && random.Chance(80)) {
			const auto back = static_cast<std::int32_t>(1 + random.Below(static_cast<std::uint32_t>(std::min(round - 1, 3))));
			from = &history[static_cast<std::size_t>(round - 1 - back)];
			frame.deltaOffset = back;
		}
		Writer w(buffer.data(), buffer.size());
		std::vector<std::size_t> kinds;
		if (random.Chance(70)) {
			const std::string text = legacy ? "print \"round " + std::to_string(round) + "\"\n" : "\x05" "cs 7 \"" + std::to_string(round) + "\"\n";
			CHECK(WriteServerCommand(w, protocol, ++sequence, text));
			kinds.push_back(3);
		}
		if (legacy && random.Chance(30)) {
			CHECK(WriteConfigstring(w, protocol, 20, "value " + std::to_string(round)));
			kinds.push_back(2);
		}
		std::size_t written = 0;
		CHECK(WriteSnapshot(w, protocol, from, frame, *baselines, false, &written) && written == live.size());
		kinds.push_back(4);
		if (random.Chance(25)) {
			CHECK(WriteNop(w));    // after bit data: an unaligned byte
			kinds.push_back(0);
		}
		const Bytes bytes = Contents(w);
		CHECK(Parse(protocol, bytes, client, message, &error));
		CHECK(message.ops.size() == kinds.size() && !message.padding && !message.gaps);
		for (std::size_t i = 0; i < kinds.size(); ++i) CHECK(message.ops[i].index() == kinds[i]);
		const Snapshot &snap = std::get<Snapshot>(message.ops[kinds.size() - (kinds.back() == 0 ? 2 : 1)]);
		CHECK(snap.status == (from ? FrameStatus::Delta : FrameStatus::Full) && snap.Valid());
		CHECK(snap.entities == live && SamePlayer(protocol, snap.player, player));
		CHECK(snap.messageNum == frame.messageNum && snap.serverTime == frame.serverTime && snap.deltaOffset == frame.deltaOffset);
		CHECK(snap.reliableAcknowledge == frame.reliableAcknowledge && snap.csSequence == frame.csSequence);
		CHECK(snap.lastUsercmd == frame.lastUsercmd && snap.snapFlags == frame.snapFlags && snap.areamask == frame.areamask);
		CHECK(snap.commandSequence == (legacy ? 0 : sequence));
		CHECK(client.snap && client.snap->messageNum == frame.messageNum && client.Frame(frame.messageNum));
		CHECK(client.csSequence == (legacy ? round : 0) && client.serverCommandSequence == (legacy ? 0 : sequence));
		CHECK(Reencode(protocol, message) == bytes);
		for (const ServerOp &op : message.ops)
			if (const auto *command = std::get_if<ServerCommand>(&op)) {
				CHECK(command->fresh && (legacy || command->sequence == sequence));
				++commands;
			}
		deltas += from != nullptr;
		history.push_back(frame);
	}
	CHECK(deltas > 50 && commands > 40);

	// A repeated 38-40 command is not fresh and leaves the sequence alone.
	if (!legacy) {
		Writer w(buffer.data(), buffer.size());
		CHECK(WriteServerCommand(w, protocol, sequence, "\x05" "old") && WriteServerCommand(w, protocol, sequence + 1, "\x05" "new"));
		CHECK(Parse(protocol, Contents(w), client, message));
		CHECK(!std::get<ServerCommand>(message.ops[0]).fresh && std::get<ServerCommand>(message.ops[1]).fresh);
		CHECK(client.serverCommandSequence == sequence + 1 && std::get<ServerCommand>(message.ops[0]).text == "\x05" "old");
	}
	// A delta from a frame the client never stored parses, but is invalid and not kept.
	{
		Snapshot lost = history.back();
		lost.messageNum += 40;
		lost.deltaOffset = 5;
		Snapshot phantom = history.back();
		phantom.messageNum = lost.messageNum - 5;
		Writer w(buffer.data(), buffer.size());
		CHECK(WriteSnapshot(w, protocol, &phantom, lost, *baselines));
		const auto latest = client.snap;
		CHECK(Parse(protocol, Contents(w), client, message));
		const Snapshot &snap = std::get<Snapshot>(message.ops[0]);
		CHECK(!snap.Valid() && (snap.status == FrameStatus::FrameTooOld || snap.status == FrameStatus::InvalidFrame));
		CHECK(client.snap == latest && !client.Frame(lost.messageNum));
	}
	// The ring: a valid frame invalidates the slots it skipped.
	{
		const Snapshot &last = history.back();
		CHECK(client.Frame(last.messageNum) && client.Frame(last.messageNum - 1));
		Snapshot jump = last;
		jump.messageNum = last.messageNum + 3;
		jump.deltaOffset = 3;
		Writer w(buffer.data(), buffer.size());
		CHECK(WriteSnapshot(w, protocol, &last, jump, *baselines));
		CHECK(Parse(protocol, Contents(w), client, message) && std::get<Snapshot>(message.ops[0]).status == FrameStatus::Delta);
		CHECK(client.Frame(jump.messageNum) && client.Frame(last.messageNum) && client.Frame(last.messageNum - 5));
		CHECK(!client.valid[static_cast<std::size_t>(last.messageNum + 1) & 31] && !client.valid[static_cast<std::size_t>(last.messageNum + 2) & 31]);
		// A jump of 40 leaves only the new frame valid.
		Snapshot far = jump;
		far.messageNum = jump.messageNum + 40;
		far.deltaOffset = 0;
		Writer f(buffer.data(), buffer.size());
		CHECK(WriteSnapshot(f, protocol, nullptr, far, *baselines));
		CHECK(Parse(protocol, Contents(f), client, message) && std::get<Snapshot>(message.ops[0]).status == FrameStatus::Full);
		unsigned valid = 0;
		for (bool flag : client.valid) valid += flag;
		CHECK(valid == 1 && client.Frame(far.messageNum) && !client.Frame(jump.messageNum));
		// A cleared slot keeps its frame; a delta from it is invalid.
		Snapshot stale = far;
		stale.messageNum = far.messageNum + 1;
		stale.deltaOffset = 41;
		Writer s(buffer.data(), buffer.size());
		CHECK(WriteSnapshot(s, protocol, &jump, stale, *baselines));
		CHECK(Parse(protocol, Contents(s), client, message) && std::get<Snapshot>(message.ops[0]).status == FrameStatus::InvalidFrame);
	}
	// A new gamestate resets the frames, the counters and the baselines.
	{
		Configstrings other;
		other[0] = configstrings[0];
		const auto empty = std::make_unique<Baselines>();
		Writer w(buffer.data(), buffer.size());
		CHECK(WriteServerGamestate(w, protocol, 5, other, *empty));
		CHECK(Parse(protocol, Contents(w), client, message));
		CHECK(!client.snap && !client.parseEntitiesNum);
		for (std::int32_t n = 0; n < 1024; ++n) CHECK(client.Baseline(n) == Entity{});
		for (const auto &slot : client.frames) CHECK(!slot);
	}
}

// 35/36: an entity equal to an unsent baseline goes as "unchanged" and the
// client drops it; 38-40 send that baseline and keep the entity.
void UnsentBaselines() {
	for (int protocol : Protocols) {
		const auto baselines = std::make_unique<Baselines>();
		Entity invisible{};
		SetEntityNumber(invisible, 300);
		Store(invisible.data() + 12, 4, 2);    // pos.trType, not among the fields 35/36 servers select by
		(*baselines)[300] = invisible;
		Bytes buffer(4096);
		Writer w(buffer.data(), buffer.size());
		CHECK(WriteServerGamestate(w, protocol, 0, Configstrings{}, *baselines));
		ClientState client(protocol);
		ServerMessage message;
		CHECK(Parse(protocol, Contents(w), client, message));
		CHECK(std::get<Gamestate>(message.ops[0]).entries.size() == (IsLegacy(protocol) ? 0u : 1u));
		Snapshot frame;
		frame.messageNum = 10;
		frame.entities = {invisible};
		Writer s(buffer.data(), buffer.size());
		CHECK(WriteSnapshot(s, protocol, nullptr, frame, *baselines));
		CHECK(Parse(protocol, Contents(s), client, message));
		const Snapshot &snap = std::get<Snapshot>(message.ops[0]);
		CHECK(snap.records.size() == 1 && snap.records[0].delta.kind == DeltaKind::Unchanged && snap.records[0].fromBaseline);
		CHECK(snap.entities.size() == (IsLegacy(protocol) ? 0u : 1u));
		CHECK(Reencode(protocol, message) == Contents(s));
	}
}

// The 35/36 1350-byte cap: the server keeps the emitted entities, while the
// client copies the old frame's remaining ones unchanged.
void SizeCap() {
	for (int protocol : {35, 36}) {
		Random random(5000 + static_cast<std::uint64_t>(protocol));
		const auto baselines = std::make_unique<Baselines>();
		Snapshot first;
		first.messageNum = 100;
		for (std::int32_t n = 1; n <= 400; ++n) first.entities.push_back(CleanEntity(random, n, 10));
		Bytes buffer(MaxMessageBytes);
		Writer w(buffer.data(), 1400);
		std::size_t written = 0;
		CHECK(WriteSnapshot(w, protocol, nullptr, first, *baselines, true, &written));
		CHECK(written > 0 && written < first.entities.size() && w.Size() > 1350 && w.Size() <= 1400);
		ClientState client(protocol);
		ServerMessage message;
		CHECK(Parse(protocol, Contents(w), client, message));
		const Snapshot &parsed = std::get<Snapshot>(message.ops[0]);
		CHECK(parsed.entities.size() == written && std::equal(parsed.entities.begin(), parsed.entities.end(), first.entities.begin()));
		// Without the cap everything goes; 38-40 have no cap to ask for.
		Writer all(buffer.data(), buffer.size());
		CHECK(WriteSnapshot(all, protocol, nullptr, first, *baselines, false, &written) && written == first.entities.size());
		Writer refused(buffer.data(), buffer.size());
		CHECK(!WriteSnapshot(refused, 38, nullptr, first, *baselines, true) && !refused.Size());
		// A capped delta: the client keeps the old frame's unsent tail.
		ClientState full(protocol);
		CHECK(Parse(protocol, Contents(all), full, message) && full.Frame(100)->entities.size() == 400);
		Snapshot next = first;
		next.messageNum = 101;
		next.deltaOffset = 1;
		for (Entity &entity : next.entities) Store(entity.data() + 84, 4, random.Next() | 1);    // time
		Writer capped(buffer.data(), 1400);
		CHECK(WriteSnapshot(capped, protocol, &first, next, *baselines, true, &written) && written < 400);
		CHECK(Parse(protocol, Contents(capped), full, message));
		const Snapshot &diverged = std::get<Snapshot>(message.ops[0]);
		const auto split = static_cast<std::ptrdiff_t>(written);
		CHECK(diverged.entities.size() == 400 && std::equal(diverged.entities.begin(), diverged.entities.begin() + split, next.entities.begin()));
		CHECK(std::equal(diverged.entities.begin() + split, diverged.entities.end(), first.entities.begin() + split));
	}
}

// Records that overflow a server message go in as far as they leave room for
// its snapshot; a message that fits is written byte for byte as before.
void FittingPrefix() {
	for (int protocol : Protocols) {
		const bool legacy = IsLegacy(protocol);
		Random random(7000 + static_cast<std::uint64_t>(protocol));
		const auto baselines = std::make_unique<Baselines>();
		Snapshot snap;
		snap.messageNum = 10;
		for (std::int32_t n = 1; n <= 20; ++n) snap.entities.push_back(CleanEntity(random, n, 10));
		const std::size_t capacity = legacy ? 1400 : MaxMessageBytes;
		std::vector<std::string> texts;
		for (int i = 0; i < 40; ++i) texts.push_back(std::string(legacy ? 300 : 900, static_cast<char>('a' + i % 26)));
		const auto item = [&](Writer &w, int i) {
			return legacy ? WriteConfigstring(w, protocol, 100 + i, texts[static_cast<std::size_t>(i)])
				: WriteServerCommand(w, protocol, 1 + i, texts[static_cast<std::size_t>(i)]);
		};
		std::size_t written = 0;
		const auto finish = [&](Writer &w) { return WriteSnapshot(w, protocol, nullptr, snap, *baselines, legacy, &written); };
		Bytes buffer(MaxMessageBytes, 0xee);

		// Forty records do not fit; asking changes nothing.
		Writer probe(buffer.data(), capacity);
		CHECK(!MessageFits(probe, 40, item, finish) && probe.Good() && !probe.Size());
		CHECK(std::all_of(buffer.begin(), buffer.end(), [](std::uint8_t b) { return b == 0xee; }));

		// A prefix does, with its snapshot, and one more record would not.
		Writer out(buffer.data(), capacity);
		const int sent = WriteFittingPrefix(out, 40, item, finish);
		CHECK(sent > 0 && sent < 40 && out.Good());
		Writer more = out.Dry();
		CHECK(!(item(more, sent) && finish(more)));
		CHECK(finish(out) && out.Size() <= capacity);
		ClientState client(protocol);
		ServerMessage message;
		CHECK(Parse(protocol, Contents(out), client, message));
		CHECK(message.ops.size() == static_cast<std::size_t>(sent) + 1 && std::holds_alternative<Snapshot>(message.ops.back()));
		for (int i = 0; i < sent; ++i) {
			const ServerOp &op = message.ops[static_cast<std::size_t>(i)];
			if (legacy) {
				const auto &update = std::get<ConfigstringUpdate>(op);
				CHECK(update.index == 100 + i && update.text == texts[static_cast<std::size_t>(i)]);
			} else {
				const auto &command = std::get<ServerCommand>(op);
				CHECK(command.sequence == 1 + i && command.text == texts[static_cast<std::size_t>(i)]);
			}
		}
		CHECK(std::get<Snapshot>(message.ops.back()).Valid());

		// Three records fit whole: the same bytes as writing each, then the snapshot.
		Bytes fitted(MaxMessageBytes), plain(MaxMessageBytes);
		Writer whole(fitted.data(), capacity), each(plain.data(), capacity);
		CHECK(MessageFits(whole, 3, item, finish) && !whole.Size());
		CHECK(WriteFittingPrefix(whole, 3, item, finish) == 3 && finish(whole));
		for (int i = 0; i < 3; ++i) CHECK(item(each, i));
		CHECK(finish(each) && Contents(whole) == Contents(each));

		// No record fits beside a snapshot that fills the message alone.
		Writer tight(buffer.data(), 24);
		CHECK(WriteFittingPrefix(tight, 40, item, finish) == 0 && tight.Good() && !tight.Size());
	}
}

// A delta is refused once the frame's entities lie more than 1920 parse-ring
// entries back (2048 - 128), although the frame is still in the ring.
void EntityWindow() {
	for (int protocol : Protocols) {
		const auto baselines = std::make_unique<Baselines>();
		const auto frameOf = [](std::int32_t messageNum, std::int32_t count) {
			Snapshot frame;
			frame.messageNum = messageNum;
			for (std::int32_t n = 1; n <= count; ++n) {
				Entity entity{};
				SetEntityNumber(entity, n);
				Store(entity.data() + 160, 4, static_cast<std::uint32_t>(messageNum));
				frame.entities.push_back(entity);
			}
			return frame;
		};
		const Snapshot a = frameOf(50, 1000), b = frameOf(51, 920), c = frameOf(52, 1);
		Snapshot d = frameOf(53, 5);
		d.deltaOffset = 3;
		ClientState client(protocol);
		ServerMessage message;
		Bytes buffer(MaxMessageBytes);
		const auto send = [&](const Snapshot *from, const Snapshot &to, ClientState &state) {
			Writer w(buffer.data(), buffer.size());
			CHECK(WriteSnapshot(w, protocol, from, to, *baselines));
			CHECK(Parse(protocol, Contents(w), state, message));
			return std::get<Snapshot>(message.ops[0]).status;
		};
		CHECK(send(nullptr, a, client) == FrameStatus::Full && send(nullptr, b, client) == FrameStatus::Full);
		CHECK(client.parseEntitiesNum == 1920);
		ClientState trial = client;
		CHECK(send(&a, d, trial) == FrameStatus::Delta);
		CHECK(send(nullptr, c, client) == FrameStatus::Full && client.parseEntitiesNum == 1921);
		trial = client;
		CHECK(send(&a, d, trial) == FrameStatus::EntitiesTooOld && !trial.Frame(53) && trial.parseEntitiesNum == 1921 + 5);
	}
}

void ServerErrors() {
	for (int protocol : Protocols) {
		const bool legacy = IsLegacy(protocol);
		ClientState client(protocol);
		ServerMessage message;
		const auto baselines = std::make_unique<Baselines>();
		Configstrings configstrings;
		configstrings[0] = "\\protocol\\" + std::to_string(protocol);
		Bytes buffer(MaxMessageBytes);
		Writer game(buffer.data(), buffer.size());
		CHECK(WriteServerGamestate(game, protocol, 1, configstrings, *baselines));
		CHECK(Parse(protocol, Contents(game), client, message));
		// Opcodes: 1, 2, 5 and 7, and 3 in 35/36 only; 0, 4, 6 and 8 up are illegible.
		for (int opcode = 0; opcode < 256; ++opcode) {
			const bool legal = opcode == 1 || opcode == 2 || opcode == 5 || opcode == 7 || (legacy && opcode == 3);
			if (!legal) CHECK(!Parse(protocol, Bytes{static_cast<std::uint8_t>(opcode)}, client, message));
		}
		CHECK(Parse(protocol, Bytes{1, 1}, client, message) && message.ops.size() == 2);
		CHECK(Parse(protocol, Bytes{}, client, message) && message.ops.empty());
		const auto header = [&](Writer &w, int areas) {
			w.WriteByte(7);
			if (!legacy) w.WriteLong(0);
			w.WriteLong(10);
			w.WriteLong(20);
			w.WriteByte(0);
			if (legacy) w.WriteLong(0);
			w.WriteLong(0);
			w.WriteByte(0);
			w.WriteByte(areas);
			const Bytes area(static_cast<std::size_t>(areas), 0);
			w.WriteData(area.data(), area.size());
			CHECK(WriteDeltaPlayer(w, protocol, nullptr, Player{}));
		};
		// Areamasks over 32 bytes.
		for (int areas : {0, 32, 33, 255}) {
			Writer w(buffer.data(), buffer.size());
			header(w, areas);
			CHECK(w.WriteBits(EntityEnd(protocol), 10));
			CHECK(Parse(protocol, Contents(w), client, message) == (areas <= 32));
		}
		Snapshot frame;
		frame.messageNum = 9;
		frame.areamask.resize(33);
		Writer toolong(buffer.data(), buffer.size());
		CHECK(!WriteSnapshot(toolong, protocol, nullptr, frame, *baselines) && !toolong.Size());
		// Configstring indexes.
		for (int index : {-1, 1024, 32767}) {
			Writer w(buffer.data(), buffer.size());
			w.WriteByte(2);
			if (protocol != 35) w.WriteLong(0);
			w.WriteByte(3);
			w.WriteShort(index);
			w.WriteString(protocol, "x");
			w.WriteByte(0);
			CHECK(!Parse(protocol, Contents(w), client, message));
			if (legacy) {
				Writer u(buffer.data(), buffer.size());
				u.WriteByte(3);
				u.WriteShort(index);
				u.WriteString(protocol, "x");
				CHECK(!Parse(protocol, Contents(u), client, message));
				Writer v(buffer.data(), buffer.size());
				CHECK(!WriteConfigstring(v, protocol, index, "x") && !v.Size());
			}
		}
		Writer update(buffer.data(), buffer.size());
		CHECK(WriteConfigstring(update, protocol, 5, "x") == legacy);
		// The gamestate limit: 1 + sum(length + 1) <= 16000, duplicates counted.
		for (std::size_t extra : {std::size_t(0), std::size_t(1)}) {
			Writer w(buffer.data(), buffer.size());
			w.WriteByte(2);
			if (protocol != 35) w.WriteLong(0);
			std::size_t chars = 1;
			for (int k = 0; k < 15; ++k) {
				w.WriteByte(3);
				w.WriteShort(k % 3);
				w.WriteString(protocol, std::string(1000, 'c'));
				chars += 1001;
			}
			w.WriteByte(3);
			w.WriteShort(7);
			w.WriteString(protocol, std::string(MaxGamestateChars - chars - 1 + extra, 'd'));
			w.WriteByte(0);
			CHECK(w.Good() && Parse(protocol, Contents(w), client, message) == !extra);
			Configstrings big;
			for (std::size_t k = 0; k < 15; ++k) big[k] = std::string(1000, 'c');
			big[20] = std::string(MaxGamestateChars - chars - 1 + extra, 'd');
			Writer s(buffer.data(), buffer.size());
			CHECK(WriteServerGamestate(s, protocol, 0, big, *baselines) == !extra);
		}
		// Gamestate command bytes other than 0, 3 and 4.
		for (int command : {1, 2, 5, 7, 255}) {
			Writer w(buffer.data(), buffer.size());
			w.WriteByte(2);
			if (protocol != 35) w.WriteLong(0);
			w.WriteByte(command);
			CHECK(!Parse(protocol, Contents(w), client, message));
		}
		// Baseline numbers: 0 in 35/36 and 1023 in 38-40 are refused.
		{
			Writer w(buffer.data(), buffer.size());
			w.WriteByte(2);
			if (protocol != 35) w.WriteLong(0);
			w.WriteByte(4);
			w.WriteBits(legacy ? 0 : 1023, 10);
			w.WriteBits(0, 1);
			w.WriteBits(0, 1);
			w.WriteByte(0);
			CHECK(!Parse(protocol, Contents(w), client, message));
		}
		// An unterminated gamestate ends with the data, as in retail, and re-encodes as received.
		{
			Writer w(buffer.data(), buffer.size());
			w.WriteByte(2);
			if (protocol != 35) w.WriteLong(3);
			w.WriteByte(3);
			w.WriteShort(0);
			w.WriteString(protocol, "cs");
			ClientState trial(protocol);
			CHECK(Parse(protocol, Contents(w), trial, message) && !std::get<Gamestate>(message.ops[0]).terminated);
			CHECK(Reencode(protocol, message) == Contents(w));
		}
		// Packet entity numbers must ascend.
		{
			Writer w(buffer.data(), buffer.size());
			header(w, 0);
			Entity a{}, b{};
			SetEntityNumber(a, 20);
			SetEntityNumber(b, 10);
			a[160] = b[160] = 1;
			CHECK(WriteDeltaEntity(w, protocol, nullptr, &a, true) && WriteDeltaEntity(w, protocol, nullptr, &b, true));
			CHECK(w.WriteBits(EntityEnd(protocol), 10));
			CHECK(!Parse(protocol, Contents(w), client, message));
		}
		// Writers refuse unordered or invalid frames and mismatched deltas.
		{
			const auto refused = [&](const Snapshot *from, const Snapshot &to) {
				Writer w(buffer.data(), buffer.size());
				return !WriteSnapshot(w, protocol, from, to, *baselines) && !w.Size() && !w.Good();
			};
			Snapshot bad;
			bad.messageNum = 5;
			Entity a{};
			SetEntityNumber(a, 20);
			bad.entities = {a, a};
			CHECK(refused(nullptr, bad));
			SetEntityNumber(a, legacy ? 0 : 1023);
			bad.entities = {a};
			CHECK(refused(nullptr, bad));
			Snapshot old;
			old.messageNum = 4;
			Snapshot next;
			next.messageNum = 5;
			CHECK(refused(&old, next));
			next.deltaOffset = 2;
			CHECK(refused(&old, next));
			next.deltaOffset = 1;
			CHECK(refused(nullptr, next) && !refused(&old, next));
			next.messageNum = 1;
			old.messageNum = 0;
			CHECK(refused(&old, next));    // deltaNum 0 reads as a full snapshot
			next.deltaOffset = 256;
			old.messageNum = -255;
			CHECK(refused(&old, next));
			next.deltaOffset = 0;
			next.snapFlags = 256;
			CHECK(refused(nullptr, next));
		}
		// Oversized strings, bad sizes and protocol mismatches.
		{
			Writer w(buffer.data(), buffer.size());
			CHECK(!WriteServerCommand(w, protocol, 1, std::string(1023, 'x')) && !w.Size());
			CHECK(!WriteServerCommand(w, protocol, 1, "ok") && !w.Size());    // failure is sticky
			CHECK(!Parse(protocol, Bytes(MaxMessageBytes + 1, 1), client, message));
			ClientState other(protocol == 40 ? 38 : 40);
			std::string error;
			CHECK(!ParseServerMessage(protocol, nullptr, 0, other, message, &error) && error == "protocol mismatch");
			CHECK(!ParseServerMessage(protocol, nullptr, 1, client, message, &error) && error == "bad message size");
			ClientState wrong(37);
			CHECK(!ParseServerMessage(37, nullptr, 0, wrong, message, &error));
		}
		// Truncations fail untouched unless they end on an operation boundary:
		// accepted prefixes of a gamestate are its unterminated prefixes, and no
		// prefix of a snapshot message yields the snapshot.
		{
			Random random(7000 + static_cast<std::uint64_t>(protocol));
			const auto bases = std::make_unique<Baselines>();
			Configstrings strings;
			strings[0] = "\\protocol\\" + std::to_string(protocol);
			strings[3] = "abc";
			for (std::int32_t n = 1; n < 30; ++n) (*bases)[static_cast<std::size_t>(n)] = CleanEntity(random, n, 40);
			Writer g(buffer.data(), buffer.size());
			CHECK(WriteServerGamestate(g, protocol, 9, strings, *bases));
			const Bytes gameBytes = Contents(g);
			ClientState base(protocol);
			CHECK(Parse(protocol, gameBytes, base, message));
			const Gamestate whole = std::get<Gamestate>(message.ops[0]);
			for (std::size_t size = 0; size < gameBytes.size(); ++size) {
				ClientState trial = base;
				if (!Parse(protocol, Bytes(gameBytes.begin(), gameBytes.begin() + static_cast<std::ptrdiff_t>(size)), trial, message)) continue;
				CHECK(message.ops.size() <= 1);
				if (message.ops.empty()) continue;
				const Gamestate &part = std::get<Gamestate>(message.ops[0]);
				CHECK(!part.terminated && part.entries.size() <= whole.entries.size());
				for (std::size_t i = 0; i < part.entries.size(); ++i) CHECK(SameEntry(part.entries[i], whole.entries[i]));
			}
			Snapshot snapshot;
			snapshot.messageNum = 70;
			snapshot.areamask = {1, 2, 3};
			snapshot.player = CleanPlayer(random, protocol, Player{});
			for (std::int32_t n = 1; n < 40; n += 3) snapshot.entities.push_back(CleanEntity(random, n, 30));
			Writer s(buffer.data(), buffer.size());
			CHECK(WriteServerCommand(s, protocol, 12, legacy ? "print x" : "\x05" "print x") && WriteSnapshot(s, protocol, nullptr, snapshot, *bases));
			const Bytes snapBytes = Contents(s);
			for (std::size_t size = 0; size < snapBytes.size(); ++size) {
				ClientState trial = base;
				if (!Parse(protocol, Bytes(snapBytes.begin(), snapBytes.begin() + static_cast<std::ptrdiff_t>(size)), trial, message)) continue;
				for (const ServerOp &op : message.ops) CHECK(std::holds_alternative<ServerCommand>(op));
			}
		}
	}
}

// Flipped bits and random bytes either fail untouched or give sane frames,
// and every accepted operation can be handed back to the writers.
void Fuzz() {
	for (int protocol : Protocols) {
		Random random(8000 + static_cast<std::uint64_t>(protocol));
		const auto baselines = std::make_unique<Baselines>();
		for (std::int32_t n = 1; n < 50; ++n) (*baselines)[static_cast<std::size_t>(n)] = CleanEntity(random, n, 30);
		Configstrings strings;
		strings[0] = "\\protocol\\" + std::to_string(protocol);
		Bytes buffer(MaxMessageBytes);
		Writer g(buffer.data(), buffer.size());
		CHECK(WriteServerGamestate(g, protocol, 2, strings, *baselines));
		ClientState base(protocol);
		ServerMessage message;
		CHECK(Parse(protocol, Contents(g), base, message));
		// Every parsed gamestate zeroes a new 1024-entry baseline table, so a
		// smaller one is flipped, at every third bit.
		const auto few = std::make_unique<Baselines>();
		for (std::size_t n = 1; n < 8; ++n) (*few)[n] = (*baselines)[n];
		Writer small(buffer.data(), buffer.size());
		CHECK(WriteServerGamestate(small, protocol, 2, strings, *few));
		const Bytes gameBytes = Contents(small);
		Snapshot first;
		first.messageNum = 7;
		first.player = CleanPlayer(random, protocol, Player{});
		for (std::int32_t n = 2; n < 50; n += 2) first.entities.push_back(CleanEntity(random, n, 30));
		Writer f(buffer.data(), buffer.size());
		CHECK(WriteSnapshot(f, protocol, nullptr, first, *baselines));
		CHECK(Parse(protocol, Contents(f), base, message));
		Snapshot second = first;
		second.messageNum = 8;
		second.deltaOffset = 1;
		second.entities.erase(second.entities.begin() + 3);
		for (Entity &entity : second.entities) Store(entity.data() + 92, 4, FloatBits(static_cast<float>(random.Below(9000))));
		Writer s(buffer.data(), buffer.size());
		CHECK(WriteServerCommand(s, protocol, 3, "\x05" "text") && WriteSnapshot(s, protocol, &first, second, *baselines) && WriteNop(s));
		const Bytes original = Contents(s);
		Bytes scratch(MaxMessageBytes);
		unsigned accepted = 0;
		const auto check = [&](const Bytes &bytes) {
			ClientState trial = base;
			if (!Parse(protocol, bytes, trial, message)) return;
			++accepted;
			for (const ServerOp &op : message.ops) {
				if (const auto *snap = std::get_if<Snapshot>(&op)) {
					CHECK(snap->areamask.size() <= MaxAreaBytes);
					std::int32_t last = -1;
					for (const Entity &entity : snap->entities) {
						CHECK(ValidEntityNumber(protocol, EntityNumber(entity)) && EntityNumber(entity) > last);
						last = EntityNumber(entity);
					}
				}
				Writer w(scratch.data(), scratch.size());
				WriteServerOp(w, protocol, op);
			}
		};
		for (const Bytes *whole : {&gameBytes, &original})
			for (std::size_t bit = 0; bit < whole->size() * 8; bit += whole == &gameBytes ? 3 : 1) {
				Bytes changed = *whole;
				changed[bit / 8] = static_cast<std::uint8_t>(changed[bit / 8] ^ (1u << (bit & 7)));
				check(changed);
			}
		for (unsigned k = 0; k < 3000; ++k) {
			Bytes bytes(random.Below(64));
			for (auto &byte : bytes) byte = static_cast<std::uint8_t>(random.Next());
			if (!bytes.empty() && random.Chance(50)) bytes[0] = static_cast<std::uint8_t>(random.Chance(50) ? 7 : 5);
			check(bytes);
		}
		CHECK(accepted > 100);
	}
}

// ---- Client messages ----

void ClientMessages() {
	for (int protocol : Protocols) {
		const bool legacy = IsLegacy(protocol);
		Random random(9000 + static_cast<std::uint64_t>(protocol));
		for (unsigned round = 0; round < 300; ++round) {
			std::vector<ClientOp> ops;
			for (unsigned k = 1 + random.Below(4); k; --k) {
				const unsigned kind = random.Below(3);
				if (kind == 0) {
					ops.emplace_back(Nop{});
				} else if (kind == 1) {
					ClientCommand command;
					command.sequence = legacy ? 0 : static_cast<std::int32_t>(random.Next());
					command.text = "userinfo \"\\name\\player" + std::to_string(random.Below(100)) + "\"";
					ops.emplace_back(command);
				} else {
					Move move;
					move.serverCommandSequence = legacy ? 0 : static_cast<std::int32_t>(random.Next());
					move.serverId = static_cast<std::int32_t>(random.Next());
					move.clientTime = static_cast<std::int32_t>(random.Next());
					move.deltaMessage = random.Chance(20) ? -1 : static_cast<std::int32_t>(random.Below(100000));
					move.csAck = legacy ? static_cast<std::int32_t>(random.Next()) : 0;
					move.lastCmd = static_cast<std::int32_t>(random.Next());
					const std::size_t count = MinMoveCommands(protocol)
						+ random.Below(static_cast<std::uint32_t>(MaxMoveCommands - MinMoveCommands(protocol) + 1));
					// Commands as they arrive, so the parse must give them back exactly.
					Usercmd raw{};
					for (std::size_t i = 0; i < count; ++i) {
						for (std::size_t b = 0; b < UsercmdBytes(protocol); ++b)
							if (random.Chance(20)) raw[b] = static_cast<std::uint8_t>(random.Next());
						move.cmds.push_back(ExpectUsercmd(protocol, i ? &move.cmds[i - 1] : nullptr, raw));
					}
					ops.emplace_back(move);
				}
			}
			Bytes buffer(MaxMessageBytes);
			Writer out(buffer.data(), buffer.size());
			for (const ClientOp &op : ops) CHECK(WriteClientOp(out, protocol, op));
			const Bytes bytes = Contents(out);
			ClientMessage message;
			std::string error;
			CHECK(ParseClientMessage(protocol, bytes.data(), bytes.size(), message, &error));
			// 35-39 moves are whole bytes; a 40 move ends anywhere, so a string after it aligns.
			CHECK(message.ops.size() == ops.size() && !message.padding && (protocol == 40 || !message.gaps));
			for (std::size_t i = 0; i < ops.size(); ++i) {
				CHECK(message.ops[i].index() == ops[i].index());
				if (const auto *move = std::get_if<Move>(&ops[i])) {
					const Move &got = std::get<Move>(message.ops[i]);
					CHECK(got.serverCommandSequence == move->serverCommandSequence && got.serverId == move->serverId);
					CHECK(got.clientTime == move->clientTime && got.deltaMessage == move->deltaMessage && got.csAck == move->csAck);
					CHECK(got.lastCmd == move->lastCmd && got.cmds == move->cmds);
				} else if (const auto *command = std::get_if<ClientCommand>(&ops[i])) {
					const ClientCommand &got = std::get<ClientCommand>(message.ops[i]);
					CHECK(got.sequence == command->sequence && got.text == command->text);
				}
			}
			Bytes again(MaxMessageBytes);
			Writer re(again.data(), again.size());
			for (const ClientOp &op : message.ops) CHECK(WriteClientOp(re, protocol, op));
			CHECK(Contents(re) == bytes);
			if (round % 25 == 0)
				for (std::size_t size = 0; size < bytes.size(); ++size) {
					ClientMessage cut;
					cut.ops.emplace_back(Nop{});
					if (!ParseClientMessage(protocol, bytes.data(), size, cut)) CHECK(cut.ops.size() == 1 && cut.ops[0].index() == 0);
				}
		}
		// Command counts: 35/36 take 2..32, 38-40 1..32.
		for (std::size_t count : {std::size_t(0), std::size_t(1), std::size_t(2), std::size_t(32), std::size_t(33)}) {
			Move move;
			move.cmds.resize(count);
			Bytes buffer(4096);
			Writer out(buffer.data(), buffer.size());
			const bool legal = count >= MinMoveCommands(protocol) && count <= MaxMoveCommands;
			CHECK(WriteMove(out, protocol, move) == legal);
			Writer raw(buffer.data(), buffer.size());
			raw.WriteByte(2);
			for (int k = 0; k < 5; ++k) raw.WriteLong(0);
			raw.WriteByte(static_cast<std::int32_t>(count));
			for (std::size_t i = 0; i < count; ++i) CHECK(WriteDeltaUsercmd(raw, protocol, nullptr, Usercmd{}));
			ClientMessage message;
			CHECK(ParseClientMessage(protocol, buffer.data(), raw.Size(), message) == legal);
		}
		// Unknown opcodes drop the client; 38-40 client commands carry a sequence.
		for (int opcode : {0, 4, 5, 7, 255}) {
			const auto byte = static_cast<std::uint8_t>(opcode);
			ClientMessage message;
			CHECK(!ParseClientMessage(protocol, &byte, 1, message));
		}
		Bytes buffer(64);
		Writer out(buffer.data(), buffer.size());
		CHECK(WriteClientCommand(out, protocol, 0x01020304, "say hi"));
		CHECK(buffer[0] == 3 && (legacy ? buffer[1] == 's' : buffer[1] == 4 && buffer[4] == 1 && buffer[5] == 's'));
		CHECK(!WriteClientCommand(out, protocol, 1, std::string(1023, 'x')));
	}
	const std::uint8_t byte = 1;
	ClientMessage message;
	CHECK(!ParseClientMessage(37, &byte, 1, message) && !ParseClientMessage(35, nullptr, 1, message));
	CHECK(ParseClientMessage(35, nullptr, 0, message) && message.ops.empty());
}

// ---- Demo files ----

void DemoFiles() {
	// Container: records, terminator, truncation and malformed lengths.
	const Bytes one = {1}, three = {1, 1, 1};
	Bytes file(64, 0xee);
	DemoWriter writer(file.data(), file.size());
	CHECK(writer.Append(one.data(), one.size()) && writer.Append(three.data(), three.size()) && writer.Finish() && writer.Finish());
	CHECK(writer.Position() == 4 + 1 + 4 + 3 + 4 && !writer.Append(one.data(), 1));
	file.resize(writer.Position());
	DemoReader reader(file.data(), file.size());
	DemoRecord record;
	CHECK(reader.Next(record) == DemoResult::Message && record.size == 1 && record.data[0] == 1);
	CHECK(reader.Next(record) == DemoResult::Message && record.size == 3);
	CHECK(reader.Next(record) == DemoResult::End && reader.Position() == file.size() && reader.Next(record) == DemoResult::End);
	for (std::size_t size = 0; size < file.size(); ++size) {
		DemoReader cut(file.data(), size);
		DemoResult result;
		while ((result = cut.Next(record)) == DemoResult::Message) {}
		CHECK(result == DemoResult::Truncated);
	}
	Bytes trailing = file;
	trailing.push_back(0);
	DemoReader extra(trailing.data(), trailing.size());
	CHECK(extra.Next(record) == DemoResult::Message && extra.Next(record) == DemoResult::Message && extra.Next(record) == DemoResult::Malformed);
	for (std::uint32_t length : {16385u, 0x7fffffffu, 0xfffffffeu, 0x80000000u}) {
		std::uint8_t bytes[8] = {};
		Store(bytes, 4, length);
		DemoReader bad(bytes, sizeof bytes);
		record = {nullptr, 123};
		CHECK(bad.Next(record) == DemoResult::Malformed && !bad.Position() && record.size == 123);
	}
	// A zero length stops retail playback as truncated; writers never produce one.
	const std::uint8_t zero[8] = {0, 0, 0, 0, 0xff, 0xff, 0xff, 0xff};
	DemoReader empty(zero, sizeof zero);
	CHECK(empty.Next(record) == DemoResult::Truncated && !empty.Position());
	Bytes largest(MaxMessageBytes + 8);
	const Bytes body(MaxMessageBytes, 7);
	DemoWriter big(largest.data(), largest.size());
	CHECK(!big.Append(body.data(), 0) && !big.Append(nullptr, 1) && !big.Append(body.data(), MaxMessageBytes + 1));
	CHECK(big.Append(body.data(), MaxMessageBytes) && big.Finish());
	DemoReader maximum(largest.data(), largest.size());
	CHECK(maximum.Next(record) == DemoResult::Message && record.size == MaxMessageBytes && maximum.Next(record) == DemoResult::End);
	for (std::size_t capacity = 0; capacity < 12; ++capacity) {
		Bytes bytes(16, 0xa5);
		const Bytes before = bytes;
		DemoWriter small(bytes.data(), capacity);
		const bool appended = small.Append(three.data(), three.size());
		CHECK(appended == (capacity >= 7));
		if (!appended) CHECK(bytes == before && !small.Position());
		const std::size_t position = small.Position();
		const Bytes middle = bytes;
		const bool finished = small.Finish();
		CHECK(finished == (capacity >= position + 4));
		if (!finished) CHECK(bytes == middle && small.Position() == position);
	}
	DemoWriter null(nullptr, 64);
	CHECK(!null.Append(one.data(), 1) && !null.Finish());

	// The synthesized first record of a recording, per release.
	for (int protocol : Protocols) {
		const bool legacy = IsLegacy(protocol);
		Configstrings configstrings;
		configstrings[0] = "\\protocol\\" + std::to_string(protocol);
		configstrings[9] = "nine";
		const auto baselines = std::make_unique<Baselines>();
		Entity modelled{}, silent{}, typed{};
		SetEntityNumber(modelled, 3);
		Store(modelled.data() + 160, 4, 4);
		SetEntityNumber(silent, 5);
		Store(silent.data() + 24, 4, FloatBits(1.5f));
		SetEntityNumber(typed, 6);
		Store(typed.data() + 4, 4, 2);
		(*baselines)[3] = modelled;
		(*baselines)[5] = silent;
		(*baselines)[6] = typed;
		Bytes buffer(4096);
		Writer demo(buffer.data(), buffer.size());
		CHECK(WriteDemoGamestate(demo, protocol, 1234, configstrings, *baselines));
		const Bytes first = Contents(demo);
		CHECK(first[0] == 2);
		if (protocol == 35) CHECK(first[1] == 3);
		else CHECK(static_cast<std::int32_t>(Load(first.data() + 1, 4)) == (protocol == 36 ? -1 : 1234));
		ClientState client(protocol);
		ServerMessage message;
		CHECK(Parse(protocol, first, client, message));
		std::vector<std::int32_t> numbers;
		for (const GamestateEntry &entry : std::get<Gamestate>(message.ops[0]).entries)
			if (entry.command == 4) numbers.push_back(entry.index);
		// 35/36 recorders keep modelindex baselines only, 38-40 every numbered one.
		CHECK(numbers == (legacy ? std::vector<std::int32_t>{3} : std::vector<std::int32_t>{3, 5, 6}));
		CHECK(Reencode(protocol, message) == first);
		// 35/36 servers select by eType, eFlags, loopSound or modelindex.
		Writer server(buffer.data(), buffer.size());
		CHECK(WriteServerGamestate(server, protocol, 1234, configstrings, *baselines));
		CHECK(Parse(protocol, Contents(server), client, message));
		numbers.clear();
		for (const GamestateEntry &entry : std::get<Gamestate>(message.ops[0]).entries)
			if (entry.command == 4) numbers.push_back(entry.index);
		CHECK(numbers == (legacy ? std::vector<std::int32_t>{3, 6} : std::vector<std::int32_t>{3, 5, 6}));
		// A selected baseline must carry its own index.
		SetEntityNumber((*baselines)[3], 4);
		Writer mismatch(buffer.data(), buffer.size());
		CHECK(!WriteDemoGamestate(mismatch, protocol, 0, configstrings, *baselines) && !mismatch.Size());
	}
	const auto baselines = std::make_unique<Baselines>();
	for (int bad : {0, 37, 41}) {
		Bytes buffer(64);
		Writer w(buffer.data(), buffer.size());
		CHECK(!WriteDemoGamestate(w, bad, 0, Configstrings{}, *baselines) && !WriteServerGamestate(w, bad, 0, Configstrings{}, *baselines));
	}
}

// ---- Retail recordings ----

struct Expected {
	int protocol;
	std::size_t fileBytes;
	const char *name;
	unsigned records, firstGamestate, firstSnapshot, firstCommand, snapshots, commands;
	unsigned entityDeltas, fromBaseline, removals, forcedUnchanged, copies, smallFloats, fullFloats, snapshotFloats;
	std::int32_t sequence;
	unsigned configstrings, baselines;
	std::int32_t firstTime, lastTime;
};
// net_messages.md section 6, and the reference decoder's writer check for the
// float fields of snapshot deltas (138,555 in all).
constexpr Expected Shipped[] = {
	{36, 230533, "1.05 q3test1", 2705, 1, 2696, 8, 2704, 8, 6292, 1537, 1468, 335, 31785, 21143, 186, 20989, -1, 17, 62, 340900, 476050},
	{36, 486418, "1.05 q3test2", 3840, 1, 3834, 5, 3839, 5, 19838, 1333, 1289, 69, 137265, 61945, 280, 62039, -1, 20, 45, 548650, 740550},
	{38, 171650, "1.06 q3testdemo1", 1655, 1, 1637, 17, 1654, 19, 4776, 1116, 1110, 208, 23997, 17133, 165, 16901, 113, 21, 71, 575500, 658150},
	{38, 118527, "1.06 q3testdemo2", 766, 1, 757, 8, 765, 9, 4343, 465, 406, 43, 37257, 15154, 113, 14991, 58, 22, 60, 591300, 667700},
	{40, 102842, "1.08 q3demo1", 1109, 1, 1101, 7, 1108, 10, 2388, 492, 478, 89, 16864, 8392, 107, 8102, 107, 20, 71, 3595300, 3650650},
	{40, 127733, "1.08 q3demo2", 821, 1, 815, 5, 820, 5, 4575, 395, 339, 45, 42004, 15665, 144, 15533, 24, 22, 60, 556700, 597650},
};

int DetectProtocol(const DemoRecord &record) {
	const std::string text(reinterpret_cast<const char *>(record.data), record.size);
	const std::size_t key = text.find("\\protocol\\");
	return key == std::string::npos ? 0 : std::atoi(text.c_str() + key + 10);
}

void Recording(const char *path) {
	std::ifstream stream(path, std::ios::binary);
	if (!stream) {
		std::fprintf(stderr, "cannot read %s\n", path);
		std::exit(1);
	}
	const Bytes file{std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
	DemoReader probe(file.data(), file.size());
	DemoRecord record;
	CHECK(probe.Next(record) == DemoResult::Message);
	const int protocol = DetectProtocol(record);
	CHECK(IsProtocol(protocol));
	DemoReader demo(file.data(), file.size());
	ClientState client(protocol);
	unsigned records = 0, identical = 0, retailIdentical = 0, gaps = 0, padding = 0;
	unsigned first[5] = {}, totals[5] = {};
	unsigned entityDeltas = 0, fromBaseline = 0, removals = 0, forcedUnchanged = 0, copies = 0, smallFloats = 0, fullFloats = 0;
	unsigned snapshotDeltas = 0, snapshotFloats = 0, indexMismatches = 0, formMismatches = 0, invalid = 0;
	std::int32_t sequence = 0, firstTime = 0, lastTime = 0;
	unsigned configs = 0, baselineCount = 0;
	bool increasing = true, firstRecordRetail = false, workedExample = false;
	Bytes buffer(MaxMessageBytes);
	const auto countDelta = [&](const EntityDelta &delta) {
		removals += delta.kind == DeltaKind::Removed;
		forcedUnchanged += delta.kind == DeltaKind::Unchanged;
		if (delta.kind != DeltaKind::Changed) return;
		++entityDeltas;
		fullFloats += PopCount(delta.fullFloats);
		smallFloats += PopCount(delta.mask & EntityFloatFields()) - PopCount(delta.fullFloats);
	};
	DemoResult result;
	while ((result = demo.Next(record)) == DemoResult::Message) {
		const ClientState before = client;
		ServerMessage message;
		std::string error;
		if (!ParseServerMessage(protocol, record.data, record.size, client, message, &error)) {
			std::fprintf(stderr, "%s record %u: %s\n", path, records, error.c_str());
			std::exit(1);
		}
		gaps += static_cast<unsigned>(message.gaps);
		padding += message.padding != 0;
		CHECK(!message.ops.empty());
		++first[message.ops[0].index()];
		// Exact re-encode from the decoded records.
		Writer exact(buffer.data(), buffer.size());
		for (const ServerOp &op : message.ops) CHECK(WriteServerOp(exact, protocol, op));
		if (exact.Size() == record.size && !std::memcmp(buffer.data(), record.data, record.size)) ++identical;
		else std::fprintf(stderr, "%s record %u: re-encode differs (%zu of %zu bytes)\n", path, records, exact.Size(), record.size);
		// The retail-equivalent writers, fed with the client's own states.
		Writer retail(buffer.data(), buffer.size());
		bool written = true;
		for (const ServerOp &op : message.ops) {
			++totals[op.index()];
			if (std::holds_alternative<Nop>(op)) {
				written = written && WriteNop(retail);
			} else if (const auto *game = std::get_if<Gamestate>(&op)) {
				Configstrings strings;
				const auto bases = std::make_unique<Baselines>();
				for (const GamestateEntry &entry : game->entries) {
					if (entry.command == 3) {
						strings[static_cast<std::size_t>(entry.index)] = entry.text;
						++configs;
					} else {
						(*bases)[static_cast<std::size_t>(entry.index)] = entry.state;
						++baselineCount;
						countDelta(entry.delta);
					}
				}
				sequence = game->sequence;
				written = written && WriteDemoGamestate(retail, protocol, game->sequence, strings, *bases);
			} else if (const auto *update = std::get_if<ConfigstringUpdate>(&op)) {
				written = written && WriteConfigstring(retail, protocol, update->index, update->text);
			} else if (const auto *command = std::get_if<ServerCommand>(&op)) {
				written = written && WriteServerCommand(retail, protocol, command->sequence, command->text);
			} else {
				const Snapshot &snap = std::get<Snapshot>(op);
				invalid += !snap.Valid();
				if (firstTime && snap.serverTime <= lastTime) increasing = false;
				if (!firstTime) firstTime = snap.serverTime;
				lastTime = snap.serverTime;
				std::size_t kept = 0;
				for (const EntityRecord &entity : snap.records) {
					kept += EntityNumber(entity.state) != EntityEnd(protocol);
					fromBaseline += entity.fromBaseline;
					countDelta(entity.delta);
					if (entity.delta.kind != DeltaKind::Changed) continue;
					// The writers' decisions, re-derived from the decoded values.
					++snapshotDeltas;
					indexMismatches += CommonMaskIndex(protocol, entity.delta.mask) != entity.delta.index;
					for (unsigned i = 0; i < 50; ++i) {
						if (!Bit(entity.delta.mask, i) || EntityFields[i].bits) continue;
						++snapshotFloats;
						std::uint32_t code = 0;
						formMismatches += SmallFloat(Load(entity.state.data() + EntityFields[i].offset, 4), code) == Bit(entity.delta.fullFloats, i);
					}
				}
				copies += static_cast<unsigned>(snap.entities.size() - kept);
				const auto from = snap.deltaOffset ? before.Frame(snap.DeltaNum()) : nullptr;
				static const Baselines none{};
				written = written && (!snap.deltaOffset || from)
					&& WriteSnapshot(retail, protocol, from.get(), snap, before.baselines ? *before.baselines : none);
				// net_messages.md section 6's worked example: q3demo1.dm3 record 3 at file offset 3065.
				if (protocol == 40 && file.size() == 102842 && records == 3) {
					CHECK(record.data - file.data() == 3065 + 4 && record.size == 46 && snap.reliableAcknowledge == 15);
					CHECK(snap.serverTime == 3595400 && snap.messageNum == 16994 && snap.deltaOffset == 1 && snap.lastUsercmd == 43308);
					CHECK(!snap.snapFlags && snap.areamask == Bytes{0xfe} && snap.playerDelta.changed == 1 && !snap.playerDelta.arrays);
					CHECK(Load(snap.player.data(), 4) == 0x36dc88 && snap.records.size() == 1 && snap.records[0].delta.number == 5);
					CHECK(!snap.records[0].fromBaseline);
					workedExample = true;
				}
			}
		}
		if (written && retail.Size() == record.size && !std::memcmp(buffer.data(), record.data, record.size)) {
			++retailIdentical;
			if (!records) firstRecordRetail = true;
		}
		++records;
	}
	CHECK(result == DemoResult::End && demo.Position() == file.size());
	const unsigned snapshots = totals[4], commands = totals[3];
	std::printf("%s: protocol %d, %u records (+ -1); first operation gamestate %u, snapshot %u, server command %u; "
		"%u snapshots, %u server commands\n", path, protocol, records, first[1], first[4], first[3], snapshots, commands);
	std::printf("  gamestate long %d, %u configstrings, %u baselines; serverTime %d..%d%s\n", sequence, configs, baselineCount,
		firstTime, lastTime, increasing ? ", increasing" : ", NOT increasing");
	std::printf("  entity deltas %u (from baseline %u, removals %u, forced no-change %u), unchanged copies %u, player deltas %u\n",
		entityDeltas, fromBaseline, removals, forcedUnchanged, copies, snapshots);
	std::printf("  float fields 13-bit %u, 32-bit %u; writer decisions over %u snapshot deltas and %u floats: %u index, %u form mismatches\n",
		smallFloats, fullFloats, snapshotDeltas, snapshotFloats, indexMismatches, formMismatches);
	std::printf("  exact re-encode %u/%u byte-identical (%u mismatches); alignment gaps %u; nonzero padding %u\n", identical, records,
		records - identical, gaps, padding);
	std::printf("  retail-equivalent writers from client-side states: %u/%u identical, record 0 %s\n", retailIdentical, records,
		firstRecordRetail ? "identical" : "different");
	CHECK(identical == records && !gaps && !padding && !invalid && increasing && !indexMismatches && !formMismatches && firstRecordRetail);
	CHECK(totals[1] == 1 && first[0] + first[1] + first[2] + first[3] + first[4] == records);
	for (const Expected &expect : Shipped) {
		if (expect.protocol != protocol || expect.fileBytes != file.size()) continue;
		CHECK(records == expect.records && first[1] == expect.firstGamestate && first[4] == expect.firstSnapshot);
		CHECK(first[3] == expect.firstCommand && snapshots == expect.snapshots && commands == expect.commands);
		CHECK(entityDeltas == expect.entityDeltas && fromBaseline == expect.fromBaseline && removals == expect.removals);
		CHECK(forcedUnchanged == expect.forcedUnchanged && copies == expect.copies && smallFloats == expect.smallFloats);
		CHECK(fullFloats == expect.fullFloats && snapshotFloats == expect.snapshotFloats && sequence == expect.sequence);
		CHECK(configs == expect.configstrings && baselineCount == expect.baselines && firstTime == expect.firstTime && lastTime == expect.lastTime);
		CHECK(expect.fileBytes != 102842 || workedExample);
		std::printf("  matches the measured counts of %s\n", expect.name);
	}
}
} // namespace

int main(int argc, char **argv) {
	Bits();
	Alignment();
	Strings();
	Floats();
	EntityDeltas();
	PlayerDeltas();
	UsercmdDeltas();
	for (int protocol : Protocols) ServerSession(protocol);
	UnsentBaselines();
	SizeCap();
	FittingPrefix();
	EntityWindow();
	ServerErrors();
	Fuzz();
	ClientMessages();
	DemoFiles();
	std::puts("Q3Test wire: primitives, strings, floats, entity/player/usercmd deltas, server and client messages, malformed input "
		"and demo files passed for protocols 35, 36, 38, 39 and 40");
	for (int i = 1; i < argc; ++i) Recording(argv[i]);
	return 0;
}
