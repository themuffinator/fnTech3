/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_Q3_IHV_WIRE_HPP
#define FNTECH3_Q3_IHV_WIRE_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>

/* Protocol 33's packed external states. These codecs never read or write an
 * engine entityState_t/playerState_t. Offsets, field order and historical
 * encoder quirks are independently observed in the supplied IHV executable;
 * see docs/fntech3/Q3_IHV.md. Native module and live session integration must
 * select the entire IHV contract before using them. */
namespace fntech3::q3ihv::wire {
static_assert(sizeof(float)==4 && std::numeric_limits<float>::radix==2
	&& std::numeric_limits<float>::digits==24 && std::numeric_limits<float>::max_exponent==128,
	"IHV wire floats require IEEE-754 binary32");
using Entity = std::array<std::uint8_t, 164>;
using Player = std::array<std::uint8_t, 276>;
using Command = std::array<std::uint8_t, 14>;

class Reader {
public:
	Reader(const void *data, std::size_t bytes) : data_(static_cast<const std::uint8_t *>(data)) {
		good_ = (data || !bytes) && bytes <= (std::numeric_limits<std::size_t>::max)() / 8;
		limit_ = good_ ? bytes * 8 : 0;
	}
	bool Read(unsigned bits, std::uint32_t &value) {
		if (bits > 32 || !good_ || bits > limit_ - position_) return Fail();
		value = 0;
		for (unsigned written = 0; written < bits;) {
			unsigned take = 8 - static_cast<unsigned>(position_ & 7);
			if (take > bits - written) take = bits - written;
			value |= ((data_[position_ / 8] >> (position_ & 7)) & ((1u << take) - 1)) << written;
			written += take; position_ += take;
		}
		return true;
	}
	bool Fail() { good_ = false; return false; }
	bool Good() const { return good_; }
	std::size_t Position() const { return position_; }
	std::size_t Remaining() const { return limit_ - position_; }
private:
	const std::uint8_t *data_;
	std::size_t limit_ = 0, position_ = 0;
	bool good_ = true;
};

class Writer {
public:
	Writer(void *data, std::size_t bytes) : data_(static_cast<std::uint8_t *>(data)) {
		good_ = (data || !bytes) && bytes <= (std::numeric_limits<std::size_t>::max)() / 8;
		limit_ = good_ ? bytes * 8 : 0;
	}
	bool Reserve(std::size_t bits) { if (!good_ || bits > limit_ - position_) return Fail(); return true; }
	bool Write(std::uint32_t value, unsigned bits) {
		if (bits > 32 || !Reserve(bits)) return Fail();
		for (unsigned read = 0; read < bits;) {
			const unsigned shift = static_cast<unsigned>(position_ & 7);
			unsigned take = 8 - shift; if (take > bits - read) take = bits - read;
			if (!shift) data_[position_ / 8] = 0;
			data_[position_ / 8] |= static_cast<std::uint8_t>(((value >> read) & ((1u << take) - 1)) << shift);
			read += take; position_ += take;
		}
		return true;
	}
	bool Fail() { good_ = false; return false; }
	bool Good() const { return good_; }
	std::size_t Position() const { return position_; }
	std::size_t Bytes() const { return position_ / 8 + !!(position_ & 7); }
private:
	std::uint8_t *data_;
	std::size_t limit_ = 0, position_ = 0;
	bool good_ = true;
};

inline std::uint32_t Load(const std::uint8_t *data, unsigned bytes) {
	std::uint32_t value = 0;
	for (unsigned i = 0; i < bytes; ++i) value |= std::uint32_t(data[i]) << (8 * i);
	return value;
}
inline void Store(std::uint8_t *data, unsigned bytes, std::uint32_t value) {
	for (unsigned i = 0; i < bytes; ++i) data[i] = static_cast<std::uint8_t>(value >> (8 * i));
}
inline bool IsNaN(std::uint32_t value) { return (value & 0x7fffffffu) > 0x7f800000u; }
inline std::uint32_t QuietFloat(std::uint32_t value) { return IsNaN(value) ? value | 0x00400000u : value; }
inline bool FloatChanged(std::uint32_t from, std::uint32_t to) {
	// Original x87 comparisons treat unordered as unchanged, as well as +0/-0.
	return !IsNaN(from) && !IsNaN(to) && from != to && ((from | to) & 0x7fffffffu);
}

struct Field { unsigned offset, storage, bits; bool floating = false; };
inline constexpr Field EntityFields[] = {
	{2,1,8}, {4,2,16}, {8,4,8}, {12,4,32}, {16,4,32},
	{20,4,32,true}, {24,4,32,true}, {28,4,32,true}, {32,4,32,true}, {36,4,32,true}, {40,4,32,true},
	{44,4,8}, {48,4,32}, {52,4,32},
	{56,4,32,true}, {60,4,32,true}, {64,4,32,true}, {68,4,32,true}, {72,4,32,true}, {76,4,32,true},
	{80,4,32}, {84,4,32},
	{88,4,32,true}, {92,4,32,true}, {96,4,32,true}, {100,4,32,true}, {104,4,32,true}, {108,4,32,true},
	{112,4,32,true}, {116,4,32,true}, {120,4,32,true}, {124,4,32,true}, {128,4,32,true}, {132,4,32,true},
	{136,2,16}, {138,2,16}, {140,2,16}, {148,1,8}, {144,4,32},
	{149,1,8}, {150,1,8}, {152,2,16}, {151,1,8}, {154,2,16},
	{156,1,8}, {157,1,8}, {158,2,16}, {160,1,8}, {161,1,8}, {162,1,8}
};

inline bool EntityFieldChanged(const Entity &from, const Entity &to, const Field &f) {
	// Original writer 0x4113e6 compares new constantLight (+144) against the
	// OLD loopSound BYTE (+148). Its reader correctly uses old constantLight.
	const auto old = f.offset == 144 ? from[148] : Load(from.data() + f.offset, f.storage);
	const auto value = Load(to.data() + f.offset, f.storage);
	return f.floating ? FloatChanged(old, value) : old != value;
}

/* A successful write may emit no bits when an entity is unchanged and force
 * is false. Null to removes from. Failure never advances or changes output. */
inline bool WriteEntity(Writer &out, const Entity &from, const Entity *to, bool force) {
	const unsigned number = Load((to ? to->data() : from.data()), 2);
	if (!number || number >= 1024) return out.Fail();
	if (!to) { if (!out.Reserve(11)) return false; out.Write(number, 10); return out.Write(1, 1); }
	const bool changed = from != *to;
	if (!changed && !force) return out.Good();
	std::size_t bits = 12;
	if (changed) for (const auto &f : EntityFields) bits += 1 + (EntityFieldChanged(from, *to, f) ? f.bits : 0);
	if (!out.Reserve(bits)) return false;
	out.Write(number, 10); out.Write(0, 1); out.Write(changed, 1);
	if (changed) for (const auto &f : EntityFields) {
		const bool fieldChanged = EntityFieldChanged(from, *to, f); out.Write(fieldChanged, 1);
		if (fieldChanged) out.Write(Load(to->data() + f.offset, f.storage), f.bits);
	}
	return true;
}

/* The caller reads the 10-bit number first to select the proper baseline.
 * Number zero terminates the packet entity list and never enters this codec. */
inline bool ReadEntityDelta(Reader &input, const Entity &from, Entity &to, unsigned number) {
	if (!number || number >= 1024) return input.Fail();
	Reader next = input; Entity decoded = from; std::uint32_t value;
	if (!next.Read(1, value)) return input.Fail();
	if (value) decoded.fill(0);
	else {
		if (!next.Read(1, value)) return input.Fail();
		if (value) {
			Store(decoded.data(), 2, number);
			for (const auto &f : EntityFields) {
				if (!next.Read(1, value)) return input.Fail();
				if (value) { if (!next.Read(f.bits, value)) return input.Fail(); }
				else value = Load(from.data() + f.offset, f.storage);
				Store(decoded.data() + f.offset, f.storage, f.floating ? QuietFloat(value) : value);
			}
		}
	}
	to = decoded; input = next; return true;
}

inline constexpr Field CommandFields[] = {{4,2,16}, {6,2,16}, {8,2,16}, {10,1,8}, {11,1,8}, {12,1,8}, {1,1,8}, {2,1,8}};
inline bool WriteCommand(Writer &out, const Command &from, const Command &to) {
	unsigned mask = 0; std::size_t bits = 16;
	for (unsigned i = 0; i < 8; ++i) {
		const auto &f = CommandFields[i];
		if (Load(from.data()+f.offset, f.storage) != Load(to.data()+f.offset, f.storage)) { mask |= 1u << i; bits += f.bits; }
	}
	if (!out.Reserve(bits)) return false;
	out.Write(mask, 8);
	for (unsigned i = 0; i < 8; ++i) if (mask & (1u << i)) {
		const auto &f = CommandFields[i]; out.Write(Load(to.data()+f.offset, f.storage), f.bits);
	}
	return out.Write(to[0], 8); // Duration is unconditional; padding is not sent.
}
inline bool ReadCommand(Reader &input, const Command &from, Command &to) {
	Reader next = input; Command decoded = from; std::uint32_t mask, value;
	if (!next.Read(8, mask)) return input.Fail();
	for (unsigned i = 0; i < 8; ++i) if (mask & (1u << i)) {
		const auto &f = CommandFields[i]; if (!next.Read(f.bits, value)) return input.Fail();
		Store(decoded.data()+f.offset, f.storage, value);
	}
	if (!next.Read(8, value)) return input.Fail();
	decoded[0] = static_cast<std::uint8_t>(value); to = decoded; input = next; return true;
}

enum class Encoding { Integer, Float, Angle, SignedByte, Color };
struct PlayerGroup { std::uint32_t mask; unsigned offset, storage, count, bits; Encoding encoding = Encoding::Integer; };
inline constexpr PlayerGroup PlayerGroups[] = {
	{0x20000,0,1,1,8}, {1,8,4,3,32,Encoding::Float}, {2,20,4,3,32,Encoding::Float},
	{4,4,2,1,16}, {8,2,2,1,16}, {0x10000,38,2,3,16}, {16,32,2,1,16},
	{64,80,4,3,16,Encoding::Angle}, {0x800,72,4,1,8}, {0x80,76,4,1,8},
	{0x40000,96,1,4,8,Encoding::Color}, {0x100000,34,2,1,16}, {0x200000,36,2,1,16},
	{0x100,1,1,1,8}, {0x1000000,92,4,1,8,Encoding::SignedByte}, {0x2000000,68,4,1,8},
	{0x4000000,52,4,1,32}, {32,50,1,1,8}, {0x2000,48,1,1,8}, {0x1000,49,1,1,8},
	{0x4000,46,2,1,8}, {0x8000000,56,4,2,8}, {0x10000000,44,2,1,8}
};
inline constexpr PlayerGroup PlayerArrays[] = {{0x200,100,2,16,16}, {0x400000,132,2,16,16}, {0x400,228,2,16,16}, {0x800000,164,4,16,32}};
inline bool GroupChanged(const Player &from, const Player &to, const PlayerGroup &g) {
	// Original 0x411c22 compares only the first color byte, then sends all four.
	const unsigned count = g.encoding == Encoding::Color ? 1 : g.count;
	for (unsigned i = 0; i < count; ++i) {
		const auto offset = g.offset + i * g.storage;
		const auto a = Load(from.data()+offset, g.storage), b = Load(to.data()+offset, g.storage);
		if ((g.encoding == Encoding::Float || g.encoding == Encoding::Angle) ? FloatChanged(a,b) : a != b) return true;
	}
	return false;
}
inline std::uint32_t EncodeAngle(std::uint32_t bits) {
	if ((bits & 0x7fffffffu) >= 0x7f800000u) return 0;
	float angle; std::memcpy(&angle, &bits, 4);
	const double scaled = static_cast<double>(angle) * static_cast<double>(182.0444488525390625f);
	// The original _ftol (0x448050) truncates to a signed 64-bit integer, then
	// the caller keeps its low 16 bits. Out-of-range x87 results have low bits 0.
	if (scaled < -9223372036854775808.0 || scaled >= 9223372036854775808.0) return 0;
	return static_cast<std::uint32_t>(static_cast<std::uint64_t>(static_cast<std::int64_t>(scaled))) & 65535u;
}
inline std::uint32_t DecodeAngle(std::uint32_t value) {
	const int signedValue = value < 32768 ? static_cast<int>(value) : static_cast<int>(value) - 65536;
	const float angle = static_cast<float>(signedValue * 0.0054931640625);
	std::uint32_t bits; std::memcpy(&bits, &angle, 4); return bits;
}
inline bool WritePlayer(Writer &out, const Player &from, const Player &to) {
	std::uint32_t mask = 0; std::size_t bits = 16; std::array<std::uint32_t, 4> arrays{};
	for (const auto &g : PlayerGroups) if (GroupChanged(from,to,g)) { mask |= g.mask; bits += g.count * g.bits; }
	for (unsigned i = 0; i < 4; ++i) {
		const auto &g = PlayerArrays[i];
		for (unsigned j = 0; j < g.count; ++j) {
			const auto offset = g.offset + j * g.storage;
			if (Load(from.data()+offset,g.storage) != Load(to.data()+offset,g.storage)) { arrays[i] |= 1u << j; bits += g.bits; }
		}
		if (arrays[i]) { mask |= g.mask; bits += 16; }
	}
	if (mask >> 16) { mask |= 0x8000; bits += 16; }
	if (!out.Reserve(bits)) return false;
	out.Write(mask,16); if (mask & 0x8000) out.Write(mask >> 16,16);
	for (const auto &g : PlayerGroups) if (mask & g.mask) for (unsigned j = 0; j < g.count; ++j) {
		auto value = Load(to.data()+g.offset+j*g.storage,g.storage);
		if (g.encoding == Encoding::Angle) value = EncodeAngle(value);
		out.Write(value,g.bits);
	}
	for (unsigned i = 0; i < 4; ++i) if (arrays[i]) {
		const auto &g = PlayerArrays[i]; out.Write(arrays[i],16);
		for (unsigned j = 0; j < g.count; ++j) if (arrays[i] & (1u << j)) out.Write(Load(to.data()+g.offset+j*g.storage,g.storage),g.bits);
	}
	return true;
}
inline bool ReadPlayer(Reader &input, const Player &from, Player &to) {
	Reader next = input; Player decoded = from; std::uint32_t mask, value;
	if (!next.Read(16,mask)) return input.Fail();
	if (mask & 0x8000) { if (!next.Read(16,value)) return input.Fail(); mask |= value << 16; }
	std::uint32_t known = 0x8000;
	for (const auto &g : PlayerGroups) known |= g.mask;
	for (const auto &g : PlayerArrays) known |= g.mask;
	if (mask & ~known) return input.Fail();
	for (const auto &g : PlayerGroups) if (mask & g.mask) for (unsigned j = 0; j < g.count; ++j) {
		if (!next.Read(g.bits,value)) return input.Fail();
		if (g.encoding == Encoding::Angle) value = DecodeAngle(value);
		else if (g.encoding == Encoding::Float) value = QuietFloat(value);
		else if (g.encoding == Encoding::SignedByte && (value & 128)) value |= 0xffffff00u;
		Store(decoded.data()+g.offset+j*g.storage,g.storage,value);
	}
	for (const auto &g : PlayerArrays) if (mask & g.mask) {
		std::uint32_t changed; if (!next.Read(16,changed)) return input.Fail();
		for (unsigned j = 0; j < g.count; ++j) if (changed & (1u << j)) {
			if (!next.Read(g.bits,value)) return input.Fail();
			Store(decoded.data()+g.offset+j*g.storage,g.storage,value);
		}
	}
	to = decoded; input = next; return true;
}
} // namespace fntech3::q3ihv::wire
#endif
