/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_Q3TEST_WIRE_HPP
#define FNTECH3_Q3TEST_WIRE_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

/* Q3Test message bodies and .dm3 demos: protocol 35 (1.02, 1.03), 36 (1.05;
 * its shipped demos were recorded by 1.04), 38 (1.06), 39 (1.07, identical to
 * 38 in every message body) and 40 (1.08). Records keep the native layouts
 * the retail modules exchange with the engine; no engine entityState_t or
 * playerState_t is read or written here. Rules cite the retail quake3.exe
 * that implements them as release@address (image base 0x400000); the
 * profile's evidence ledger is docs/fntech3/Q3TEST.md. The netchan header,
 * fragments and connectionless packets lie outside these bodies.
 *
 * Readers keep retail's readcount/bit model but never read past the buffer:
 * where retail would read stale memory the reader fails instead, and a failed
 * call leaves its outputs and the reader's position untouched. Writers keep
 * retail's cursize/bit model and overflow rule; a failed call writes nothing,
 * not even into the buffer's spare bytes, and leaves the writer failed, as
 * retail's overflowed flag does. Every entry point refuses protocols other
 * than 35, 36, 38, 39 and 40. */
namespace fntech3::q3test::wire {
static_assert(std::numeric_limits<float>::is_iec559 && sizeof(float) == 4,
	"Q3Test wire floats are IEEE-754 binary32");

inline constexpr bool IsProtocol(int protocol) {
	return protocol == 35 || protocol == 36 || protocol == 38 || protocol == 39 || protocol == 40;
}
// 35 and 36 form the 1.02-1.05 message family, 38 to 40 the 1.06-1.08 one.
inline constexpr bool IsLegacy(int protocol) { return protocol == 35 || protocol == 36; }

inline constexpr std::size_t MaxMessageBytes = 16384;
// A 1023-character string leaves its terminator unread (1.05@0x413cea).
inline constexpr std::size_t MaxStringChars = 1022;
// 1 plus (length + 1) for every gamestate configstring (1.05@0x40770b).
inline constexpr std::size_t MaxGamestateChars = 16000;
inline constexpr std::size_t MaxAreaBytes = 32;
inline constexpr std::size_t MaxMoveCommands = 32;
// clc_move counts below 2 (35/36, 1.05@0x41b620) or 1 (38-40, 1.06@0x41c0c0) are refused.
inline constexpr std::size_t MinMoveCommands(int protocol) { return IsLegacy(protocol) ? 2 : 1; }

/* entityState_t is 51 dwords in every release, dword 0 being the number
 * (ReadDeltaEntity `rep movsd`, ecx 0x33: 1.02@0x413715, 1.06@0x414e7a). */
inline constexpr std::size_t EntityBytes = 204;
inline constexpr std::size_t MaxPlayerBytes = 440;
inline constexpr std::size_t MaxUsercmdBytes = 24;
// playerState_t: 0x45, 0x68, 0x6c, 0x6e dwords (1.02@0x4141ba, 1.05@0x414916, 1.06@0x4153b6, 1.08@0x415de6).
inline constexpr std::size_t PlayerBytes(int protocol) {
	return protocol == 35 ? 276 : protocol == 36 ? 416 : protocol == 38 || protocol == 39 ? 432 : protocol == 40 ? 440 : 0;
}
/* usercmd_t: 14 bytes with 16-bit angles (1.02@0x413a8a), 20 with 32-bit
 * angles (1.05@0x41400c), 24 with serverTime in place of msec (1.08@0x40484b). */
inline constexpr std::size_t UsercmdBytes(int protocol) {
	return protocol == 35 ? 14 : protocol == 36 || protocol == 38 || protocol == 39 ? 20 : protocol == 40 ? 24 : 0;
}
/* Packet entities end with, and removed entities decode to, number 0 in 35/36
 * (1.05@0x40719e, 0x407104) and 1023 in 38-40 (1.06@0x407eaa, 0x407dc0). */
inline constexpr std::int32_t EntityEnd(int protocol) { return IsLegacy(protocol) ? 0 : 1023; }
/* Retail checks 1..1023 in 35/36 (1.05@0x41415f writer, 0x41434b reader) and
 * 0..1023 in 38-40 (1.06@0x414bdf, 0x414dcd); 1023, 38-40's end marker, is
 * refused here as well. */
inline constexpr bool ValidEntityNumber(int protocol, std::int32_t number) {
	return IsLegacy(protocol) ? number >= 1 && number <= 1023 : IsProtocol(protocol) && number >= 0 && number <= 1022;
}

using Entity = std::array<std::uint8_t, EntityBytes>;
// A player or usercmd record is the first PlayerBytes(protocol) or
// UsercmdBytes(protocol) bytes; readers zero the rest and writers ignore it.
using Player = std::array<std::uint8_t, MaxPlayerBytes>;
using Usercmd = std::array<std::uint8_t, MaxUsercmdBytes>;
using Baselines = std::array<Entity, 1024>;
using Configstrings = std::array<std::string, 1024>;

inline std::uint32_t Load(const std::uint8_t *data, unsigned bytes) {
	std::uint32_t value = 0;
	for (unsigned i = 0; i < bytes; ++i) value |= static_cast<std::uint32_t>(data[i]) << (8 * i);
	return value;
}
inline void Store(std::uint8_t *data, unsigned bytes, std::uint32_t value) {
	for (unsigned i = 0; i < bytes; ++i) data[i] = static_cast<std::uint8_t>(value >> (8 * i));
}
inline std::int32_t EntityNumber(const Entity &entity) { return static_cast<std::int32_t>(Load(entity.data(), 4)); }
inline void SetEntityNumber(Entity &entity, std::int32_t number) { Store(entity.data(), 4, static_cast<std::uint32_t>(number)); }

// ---- Bit and byte primitives ----

class Reader {
public:
	Reader() = default;
	Reader(const void *data, std::size_t bytes) : data_(static_cast<const std::uint8_t *>(data)) {
		good_ = (data || !bytes) && bytes <= (std::numeric_limits<std::size_t>::max)() / 8;
		size_ = good_ ? bytes : 0;
	}
	bool Good() const { return good_; }
	bool Fail() { good_ = false; return false; }
	std::size_t Size() const { return size_; }
	// Bytes touched, a partly read one included, and the bit inside it.
	std::size_t ReadCount() const { return readcount_; }
	unsigned Bit() const { return bit_; }
	std::size_t BitPosition() const { return readcount_ * 8 - (bit_ ? 8 - bit_ : 0); }
	std::size_t BitsLeft() const { return (size_ - readcount_) * 8 + (bit_ ? 8 - bit_ : 0); }
	std::size_t Gaps() const { return gaps_; }
	// Retail's end test, `cursize >= readcount + bytes`, which counts a partly
	// read byte; it holds exactly when 8 * bytes more bits remain.
	bool Has(std::size_t bytes) const { return good_ && size_ - readcount_ >= bytes; }

	/* MSG_ReadBits (1.05@0x4139d0): least significant bit first; a negative
	 * width reads |bits| and sign-extends (1.05 and later; no protocol-35
	 * field has one). Reading past the end fails and returns 0. */
	std::int32_t ReadBits(int bits) {
		if (!good_ || bits == 0 || bits > 32 || bits < -31) { Fail(); return 0; }
		const unsigned count = static_cast<unsigned>(bits < 0 ? -bits : bits);
		if (count > BitsLeft()) { Fail(); return 0; }
		std::uint32_t value = 0;
		for (unsigned got = 0; got < count;) {
			if (!bit_) ++readcount_;
			unsigned take = 8 - bit_;
			if (take > count - got) take = count - got;
			value |= ((static_cast<std::uint32_t>(data_[readcount_ - 1]) >> bit_) & ((1u << take) - 1)) << got;
			got += take;
			bit_ = (bit_ + take) & 7;
		}
		if (bits < 0 && ((value >> (count - 1)) & 1)) value |= ~0u << count;
		return static_cast<std::int32_t>(value);
	}
	// Retail value readers (1.05@0x413bd0..0x413c60): -1, nothing consumed,
	// when too few bytes remain; the reader stays good.
	int ReadChar() {
		if (!Has(1)) return -1;
		const int value = ReadBits(8) & 0xff;
		return value < 128 ? value : value - 256;
	}
	int ReadByte() { return Has(1) ? ReadBits(8) & 0xff : -1; }
	int ReadShort() {
		if (!Has(2)) return -1;
		const int value = ReadBits(16) & 0xffff;
		return value < 32768 ? value : value - 65536;
	}
	std::int32_t ReadLong() { return Has(4) ? ReadBits(32) : -1; }

	/* The read-side aligner (1.05@0x413800) steps past a partly read byte and,
	 * that byte being counted already, one more: the writer's alignment gap. */
	bool Align() {
		if (!good_) return false;
		if (!bit_) return true;
		if (readcount_ >= size_) return Fail();
		bit_ = 0;
		++readcount_;
		++gaps_;
		return true;
	}
	// MSG_ReadData (1.05@0x413d90): align, then whole bytes.
	bool ReadData(void *output, std::size_t bytes) {
		Reader next = *this;
		if (!next.Align() || (!output && bytes) || next.size_ - next.readcount_ < bytes) return Fail();
		if (bytes) std::memcpy(output, data_ + next.readcount_, bytes);
		next.readcount_ += bytes;
		*this = next;
		return true;
	}
	/* MSG_ReadString, after aligning. 35/36 (1.05@0x413cc0) read signed chars,
	 * so 0xFF arrives as -1 and ends the string, consumed like 0. 38-40
	 * (1.06@0x414850) read unsigned bytes and store '%' as '.'. Both stop
	 * after 1023 characters without consuming the next byte (1.05@0x413cea,
	 * 1.06@0x414884). A string cut off by the end of data, which retail
	 * accepts, fails here. */
	bool ReadString(int protocol, std::string &text) {
		if (!IsProtocol(protocol)) return Fail();
		Reader next = *this;
		if (!next.Align()) return Fail();
		std::string value;
		for (;;) {
			if (!next.Has(1)) return Fail();
			int c = IsLegacy(protocol) ? next.ReadChar() : next.ReadByte();
			if (c == 0 || c == -1) break;
			if (!IsLegacy(protocol) && c == '%') c = '.';
			value.push_back(static_cast<char>(c));
			if (value.size() >= 1023) break;
		}
		text = std::move(value);
		*this = next;
		return true;
	}

private:
	const std::uint8_t *data_ = nullptr;
	std::size_t size_ = 0, readcount_ = 0, gaps_ = 0;
	unsigned bit_ = 0;
	bool good_ = true;
};

class Writer {
public:
	Writer() = default;
	Writer(void *data, std::size_t bytes) : data_(static_cast<std::uint8_t *>(data)) {
		good_ = (data || !bytes) && bytes <= (std::numeric_limits<std::size_t>::max)() / 8;
		maxsize_ = good_ ? bytes : 0;
	}
	bool Good() const { return good_; }
	bool Fail() { good_ = false; return false; }
	const std::uint8_t *Data() const { return data_; }
	// Bytes written, a partly written one included, and the bit inside it.
	std::size_t Size() const { return cursize_; }
	std::size_t Capacity() const { return maxsize_; }
	unsigned Bit() const { return bit_; }
	std::size_t BitPosition() const { return cursize_ * 8 - (bit_ ? 8 - bit_ : 0); }
	std::size_t Gaps() const { return gaps_; }

	/* MSG_WriteBits (1.05@0x4138d0): least significant bit first. With fewer
	 * than 4 bytes free it overflows and writes nothing (0x4138d6), so bit
	 * writes never use a buffer's last three bytes; widths outside 1..32 and
	 * -1..-31 are fatal (0x4138ed). Values are truncated to the width. */
	bool WriteBits(std::int32_t value, int bits) {
		if (!good_ || bits == 0 || bits > 32 || bits < -31 || maxsize_ - cursize_ < 4) return Fail();
		unsigned count = static_cast<unsigned>(bits < 0 ? -bits : bits);
		std::uint32_t remaining = static_cast<std::uint32_t>(value);
		while (count) {
			if (!bit_) {
				if (!dry_) data_[cursize_] = 0;
				++cursize_;
			}
			unsigned take = 8 - bit_;
			if (take > count) take = count;
			if (!dry_) data_[cursize_ - 1] |= static_cast<std::uint8_t>((remaining & ((1u << take) - 1)) << bit_);
			remaining >>= take;
			count -= take;
			bit_ = (bit_ + take) & 7;
		}
		return true;
	}
	bool WriteByte(std::int32_t value) { return WriteBits(value, 8); }    // also MSG_WriteChar
	bool WriteShort(std::int32_t value) { return WriteBits(value, 16); }
	bool WriteLong(std::int32_t value) { return WriteBits(value, 32); }
	/* MSG_WriteData via MSG_GetSpace (1.05@0x413820): a partly written byte is
	 * counted already, so aligned data starts one byte further on, even for a
	 * zero-length write. Retail leaves that gap byte stale; it is zeroed here. */
	bool WriteData(const void *bytes, std::size_t count) {
		if (!good_ || (!bytes && count)) return Fail();
		const std::size_t start = cursize_ + (bit_ ? 1 : 0);
		if (start > maxsize_ || count > maxsize_ - start) return Fail();
		if (bit_) {
			if (!dry_) data_[cursize_] = 0;
			++gaps_;
		}
		if (count && !dry_) std::memmove(data_ + start, bytes, count);
		cursize_ = start + count;
		bit_ = 0;
		return true;
	}
	/* MSG_WriteString: 35/36 (1.05@0x413b30) send the bytes as they are,
	 * 38-40 (1.06@0x4146d0) send bytes above 0x7f as '.'. Strings a retail
	 * reader cannot take back whole fail instead: more than 1022 characters
	 * (retail sends 1023 and empties longer ones), an embedded NUL, and in
	 * 35/36 a 0xFF byte, which ends the string on read. */
	bool WriteString(int protocol, std::string_view text) {
		if (!good_ || !IsProtocol(protocol) || text.size() > MaxStringChars) return Fail();
		char buffer[MaxStringChars + 1];
		for (std::size_t i = 0; i < text.size(); ++i) {
			const auto c = static_cast<unsigned char>(text[i]);
			if (!c || (IsLegacy(protocol) && c == 0xff)) return Fail();
			buffer[i] = !IsLegacy(protocol) && c > 0x7f ? '.' : text[i];
		}
		buffer[text.size()] = 0;
		return WriteData(buffer, text.size() + 1);
	}
	// A copy that follows positions and failures without touching the buffer.
	Writer Dry() const {
		Writer copy = *this;
		copy.dry_ = true;
		return copy;
	}

private:
	std::uint8_t *data_ = nullptr;
	std::size_t maxsize_ = 0, cursize_ = 0, gaps_ = 0;
	unsigned bit_ = 0;
	bool good_ = true, dry_ = false;
};

namespace detail {
// Runs an encoder on a dry copy first, so a failure changes no byte.
template<class Encode> bool Atomic(Writer &out, Encode &&encode) {
	if (!out.Good()) return false;
	Writer dry = out.Dry();
	if (!encode(dry)) return out.Fail();
	return encode(out) || out.Fail();
}
inline bool GetByte(Reader &in, std::int32_t &value) {
	if (!in.Has(1)) return in.Fail();
	value = in.ReadByte();
	return true;
}
inline bool GetShort(Reader &in, std::int32_t &value) {
	if (!in.Has(2)) return in.Fail();
	value = in.ReadShort();
	return true;
}
inline bool GetLong(Reader &in, std::int32_t &value) {
	if (!in.Has(4)) return in.Fail();
	value = in.ReadLong();
	return true;
}
inline bool Error(std::string *error, const char *text) {
	if (error) *error = text;
	return false;
}
} // namespace detail

// ---- Floats ----

inline bool IsNaN(std::uint32_t bits) { return (bits & 0x7fffffffu) > 0x7f800000u; }
inline std::uint32_t FloatBits(float value) {
	std::uint32_t bits;
	std::memcpy(&bits, &value, 4);
	return bits;
}
inline float BitsFloat(std::uint32_t bits) {
	float value;
	std::memcpy(&value, &bits, 4);
	return value;
}
/* Delta-coded floats (table width 0) are a form bit, then 13 bits of
 * value + 4096 or 32 raw bits. The writers' choice (1.05@0x414662..0x414691,
 * the same in every release): i = __ftol(f), truncating; `fild i; fcomp f`
 * with only C3 tested; then 0 <= i + 4096 < 8192. NaN makes __ftol return 0
 * and compares unordered (C3 set), and -0.0 equals 0, so both go as code 4096
 * and arrive as +0.0. Readers accept either form for any value. */
inline bool SmallFloat(std::uint32_t bits, std::uint32_t &code) {
	if (IsNaN(bits)) {
		code = 4096;
		return true;
	}
	const float value = BitsFloat(bits);
	if (!(value >= -4096.0f && value <= 4095.0f)) return false;
	const int integer = static_cast<int>(value);
	if (static_cast<float>(integer) != value) return false;
	code = static_cast<std::uint32_t>(integer + 4096);
	return true;
}

namespace detail {
inline bool WriteField(Writer &out, int bits, std::uint32_t value, bool full) {
	if (bits) return out.WriteBits(static_cast<std::int32_t>(value), bits);
	if (full) return out.WriteBits(1, 1) && out.WriteBits(static_cast<std::int32_t>(value), 32);
	std::uint32_t code = 0;
	if (!SmallFloat(value, code)) return out.Fail();
	return out.WriteBits(0, 1) && out.WriteBits(static_cast<std::int32_t>(code), 13);
}
// The 32-bit form is stored as raw bits (1.05@0x41453f), the 13-bit one
// through `fild; fstp dword` (1.05@0x414525).
inline void ReadField(Reader &in, int bits, std::uint32_t &value, bool &full) {
	full = false;
	if (bits) {
		value = static_cast<std::uint32_t>(in.ReadBits(bits));
		return;
	}
	full = in.ReadBits(1) != 0;
	if (full) value = static_cast<std::uint32_t>(in.ReadBits(32));
	else value = FloatBits(static_cast<float>(in.ReadBits(13) - 4096));
}
} // namespace detail

// ---- Entity states ----

/* The 50-entry field table {name, offset, bits} (1.02@0x468350,
 * 1.05@0x4695e8, 1.06@0x460148, 1.08@0x4611a8), identical in every release.
 * Mask bit i is entry i; width 0 is a delta-coded float. The order swaps
 * loopSound/constantLight and frame/clientNum against their offsets. */
struct EntityField {
	std::uint8_t offset, bits;
};
inline constexpr EntityField EntityFields[50] = {
	{4, 8}, {8, 16}, {12, 8}, {16, 32}, {20, 32}, {24, 0}, {28, 0}, {32, 0}, {36, 0}, {40, 0}, {44, 0},
	{48, 8}, {52, 32}, {56, 32}, {60, 0}, {64, 0}, {68, 0}, {72, 0}, {76, 0}, {80, 0},
	{84, 32}, {88, 32}, {92, 0}, {96, 0}, {100, 0}, {104, 0}, {108, 0}, {112, 0},
	{116, 0}, {120, 0}, {124, 0}, {128, 0}, {132, 0}, {136, 0},
	{140, 10}, {144, 10}, {148, 10}, {156, 8}, {152, 32}, {160, 8}, {164, 8}, {172, 16}, {168, 8}, {176, 24},
	{180, 10}, {184, 8}, {188, 16}, {192, 8}, {196, 8}, {200, 8},
};
inline constexpr std::uint64_t EntityFloatFields() {
	std::uint64_t mask = 0;
	for (unsigned i = 0; i < 50; ++i)
		if (!EntityFields[i].bits) mask |= std::uint64_t(1) << i;
	return mask;
}
/* Common change masks, the lo/hi words of {useCount, maskLo, maskHi}.
 * 1.02@0x465350, 1.03@0x4664e0 and 1.05@0x4665e8 hold these 27 and five zero
 * entries under a count of 32. 1.06@0x45d148, 1.07@0x45d178 and
 * 1.08@0x45e1a8 hold the second table under a count nothing writes
 * (1.06@0x4a65d4 stays 0), so those writers always send index 31 and an
 * explicit mask while their readers still honour 0..30. */
inline constexpr std::uint64_t CommonMasks35[32] = {
	0x20040e0000e1, 0x8000000007ed, 0x100000000000, 0x100000000002, 0x3000000007ed, 0x2, 0x20000e0000e1,
	0x8001c000e1, 0xe000060, 0x6000060, 0xb000000000e0, 0x202001c000e1, 0x8000000006ed, 0x2000000000e1,
	0x108001c000e1, 0x3000000006cd, 0x88000000000, 0x80000000e1, 0x40e0000e1, 0x8800c000001, 0x8001c000e3,
	0x20000e0000e0, 0xe0000e0, 0x8800a000001, 0x8800e000001, 0x260001c000e1, 0x200001c000e1, 0, 0, 0, 0, 0,
};
inline constexpr std::uint64_t CommonMasks38[32] = {
	0xc0e0, 0xc060, 0x60, 0x8060, 0xe0, 0x80, 0x20040e0000e1, 0x80e0, 0x10000000c060, 0x100000000000, 0xa0,
	0x40e0, 0x100000008060, 0x100000000060, 0x8000000007ed, 0x4060, 0x210000000c060, 0xc0a0, 0x3000000007ed,
	0xc0c0, 0x2100000008060, 0x10000c060, 0x10000c0e0, 0x2100000000060, 0xc0, 0x210000000c0e0, 0x8000,
	0x100008060, 0x20000e0000e1, 0xc000, 0x20, 0x10000000c0e0,
};
inline const std::uint64_t *CommonMasks(int protocol) { return IsLegacy(protocol) ? CommonMasks35 : CommonMasks38; }
// The writers' lookup (1.05@0x4140c0): the first equal entry, else 31.
inline unsigned CommonMaskIndex(int protocol, std::uint64_t mask) {
	if (IsLegacy(protocol) && mask)
		for (unsigned i = 0; i < 32; ++i)
			if (CommonMasks35[i] == mask) return i;
	return 31;
}
// Fields compare as raw dwords (1.05@0x414189), so -0.0 differs from 0.0.
inline std::uint64_t EntityChangeMask(const Entity &from, const Entity &to) {
	std::uint64_t mask = 0;
	for (unsigned i = 0; i < 50; ++i) {
		const unsigned offset = EntityFields[i].offset;
		if (Load(from.data() + offset, 4) != Load(to.data() + offset, 4)) mask |= std::uint64_t(1) << i;
	}
	return mask;
}

enum class DeltaKind : std::uint8_t { Removed, Unchanged, Changed };
/* One entity record as it crosses the wire:
 *   10 bits number, bit removed, bit changed, 5 bits common-mask index,
 *   index 31: six unaligned ReadBytes and 2 bits of explicit 50-bit mask,
 *   then the masked fields in table order (1.05@0x414120 / 0x414340). */
struct EntityDelta {
	std::int32_t number = 0;
	DeltaKind kind = DeltaKind::Changed;
	std::uint8_t index = 31;
	std::uint64_t mask = 0;
	std::uint64_t fullFloats = 0;    // float fields sent in the 32-bit form
};

namespace detail {
inline bool EncodeEntityRecord(Writer &out, int protocol, const EntityDelta &delta, const Entity &to) {
	if (!IsProtocol(protocol) || !ValidEntityNumber(protocol, delta.number)) return out.Fail();
	if (delta.kind == DeltaKind::Removed) return out.WriteBits(delta.number, 10) && out.WriteBits(1, 1);
	if (delta.kind == DeltaKind::Unchanged)
		return out.WriteBits(delta.number, 10) && out.WriteBits(0, 1) && out.WriteBits(0, 1);
	if (delta.kind != DeltaKind::Changed || delta.index > 31 || (delta.mask >> 50)
		|| (delta.fullFloats & ~(delta.mask & EntityFloatFields()))
		|| (delta.index != 31 && CommonMasks(protocol)[delta.index] != delta.mask)) return out.Fail();
	if (!out.WriteBits(delta.number, 10) || !out.WriteBits(0, 1) || !out.WriteBits(1, 1) || !out.WriteBits(delta.index, 5))
		return false;
	if (delta.index == 31) {
		for (unsigned i = 0; i < 6; ++i)
			if (!out.WriteByte(static_cast<std::int32_t>((delta.mask >> (8 * i)) & 0xff))) return false;
		if (!out.WriteBits(static_cast<std::int32_t>(delta.mask >> 48), 2)) return false;
	}
	for (unsigned i = 0; i < 50; ++i) {
		if (!((delta.mask >> i) & 1)) continue;
		const EntityField &field = EntityFields[i];
		if (!WriteField(out, field.bits, Load(to.data() + field.offset, 4), ((delta.fullFloats >> i) & 1) != 0)) return false;
	}
	return true;
}
inline bool EncodeDeltaEntity(Writer &out, int protocol, const Entity *from, const Entity *to, bool force) {
	if (!IsProtocol(protocol)) return out.Fail();
	EntityDelta delta;
	if (!to) {
		if (!from) return true;
		// Removal writes from->number unchecked in retail (1.05@0x41413a).
		delta.number = EntityNumber(*from);
		delta.kind = DeltaKind::Removed;
		return EncodeEntityRecord(out, protocol, delta, *from);
	}
	const Entity zero{};
	const Entity &base = from ? *from : zero;
	delta.number = EntityNumber(*to);
	if (!ValidEntityNumber(protocol, delta.number)) return out.Fail();
	delta.mask = EntityChangeMask(base, *to);
	if (!delta.mask) {
		if (!force) return true;
		delta.kind = DeltaKind::Unchanged;
	} else {
		delta.index = static_cast<std::uint8_t>(CommonMaskIndex(protocol, delta.mask));
		for (unsigned i = 0; i < 50; ++i) {
			std::uint32_t code = 0;
			if (((delta.mask >> i) & 1) && !EntityFields[i].bits
				&& !SmallFloat(Load(to->data() + EntityFields[i].offset, 4), code)) delta.fullFloats |= std::uint64_t(1) << i;
		}
	}
	return EncodeEntityRecord(out, protocol, delta, *to);
}
} // namespace detail

/* MSG_WriteDeltaEntity: a null `to` removes `from` (nothing when both are
 * null); a null `from` is the all-zero state. An unchanged entity is written
 * as number, 0, 0 only when forced. 35/36 use the common-mask table, 38-40
 * always send index 31. */
inline bool WriteDeltaEntity(Writer &out, int protocol, const Entity *from, const Entity *to, bool force) {
	return detail::Atomic(out, [&](Writer &w) { return detail::EncodeDeltaEntity(w, protocol, from, to, force); });
}
// Writes a decoded record exactly as it was received, `to` supplying the fields.
inline bool WriteEntityRecord(Writer &out, int protocol, const EntityDelta &delta, const Entity &to) {
	return detail::Atomic(out, [&](Writer &w) { return detail::EncodeEntityRecord(w, protocol, delta, to); });
}
/* MSG_ReadDeltaEntity after its caller read the 10-bit number. Removed: all
 * zero, then number 1023 in 38-40 (1.06@0x414e2c). Unchanged: a copy of
 * `from`, keeping from's own number in 35/36 (1.05@0x4143ca), so an
 * unchanged never-sent baseline decodes as number 0 and is dropped, and
 * taking the wire number in 38-40. Changed: a copy with the wire number and
 * the masked fields replaced. */
inline bool ReadDeltaEntity(Reader &in, int protocol, const Entity &from, Entity &to, int number, EntityDelta *delta = nullptr) {
	if (!IsProtocol(protocol) || !ValidEntityNumber(protocol, number)) return in.Fail();
	Reader next = in;
	Entity decoded = from;
	EntityDelta record;
	record.number = number;
	if (next.ReadBits(1)) {
		decoded.fill(0);
		if (!IsLegacy(protocol)) SetEntityNumber(decoded, 1023);
		record.kind = DeltaKind::Removed;
	} else if (!next.ReadBits(1)) {
		if (!IsLegacy(protocol)) SetEntityNumber(decoded, number);
		record.kind = DeltaKind::Unchanged;
	} else {
		record.index = static_cast<std::uint8_t>(next.ReadBits(5));
		if (record.index == 31) {
			for (unsigned i = 0; i < 6; ++i) {
				const int byte = next.ReadByte();
				if (byte < 0) return in.Fail();
				record.mask |= static_cast<std::uint64_t>(byte) << (8 * i);
			}
			record.mask |= static_cast<std::uint64_t>(next.ReadBits(2) & 3) << 48;
		} else {
			record.mask = CommonMasks(protocol)[record.index];
		}
		SetEntityNumber(decoded, number);
		for (unsigned i = 0; i < 50; ++i) {
			if (!((record.mask >> i) & 1)) continue;
			std::uint32_t value = 0;
			bool full = false;
			detail::ReadField(next, EntityFields[i].bits, value, full);
			Store(decoded.data() + EntityFields[i].offset, 4, value);
			if (full) record.fullFloats |= std::uint64_t(1) << i;
		}
	}
	if (!next.Good()) return in.Fail();
	to = decoded;
	in = next;
	if (delta) *delta = record;
	return true;
}

// ---- Player states ----

/* Protocols 36-40: a change bit per net-field table entry {name, offset,
 * bits} (1.05@0x469840, 35 entries; 1.06@0x4603a0, 42; 1.08@0x461400, 44),
 * compared as raw dwords (1.05@0x414644); width 0 is a delta-coded float,
 * a negative width is signed. */
struct NetField {
	std::uint16_t offset;
	std::int8_t bits;
};
inline constexpr NetField PlayerFields36[35] = {
	{0, 8}, {4, 8}, {8, 16}, {12, -16}, {16, 0}, {20, 0}, {24, 0}, {28, 0}, {32, 0}, {36, 0}, {40, -16}, {44, 16},
	{48, 16}, {52, 16}, {56, 16}, {60, 16}, {64, 10}, {68, 16}, {72, 8}, {76, 8}, {80, 4}, {84, 16}, {88, 10},
	{92, 8}, {96, 8}, {100, 5}, {104, 4}, {108, 0}, {112, 0}, {116, 0}, {120, -8}, {124, 8}, {128, 8}, {132, 8},
	{136, 8},
};
inline constexpr NetField PlayerFields38[42] = {
	{0, 8}, {4, 8}, {8, 16}, {12, -16}, {16, 0}, {20, 0}, {24, 0}, {28, 0}, {32, 0}, {36, 0}, {40, -16}, {44, 16},
	{48, 16}, {52, 16}, {56, 16}, {60, 16}, {64, 10}, {68, 16}, {72, 8}, {76, 8}, {80, 4}, {96, 16}, {100, 16},
	{104, 8}, {108, 8}, {112, 8}, {116, 8}, {120, 8}, {128, 8}, {132, 5}, {136, 4}, {140, 0}, {144, 0}, {148, 0},
	{152, -8}, {156, 8}, {160, 8}, {164, 8}, {168, 8}, {84, 0}, {88, 0}, {92, 0},
};
inline constexpr NetField PlayerFields40[44] = {
	{0, 32}, {4, 8}, {8, 8}, {12, 16}, {16, -16}, {20, 0}, {24, 0}, {28, 0}, {32, 0}, {36, 0}, {40, 0}, {44, -16},
	{48, 16}, {52, 16}, {56, 16}, {60, 16}, {64, 16}, {68, 10}, {72, 16}, {76, 8}, {80, 8}, {84, 4}, {100, 16},
	{104, 16}, {108, 8}, {112, 8}, {116, 8}, {120, 8}, {124, 8}, {128, 8}, {136, 8}, {140, 5}, {144, 4}, {148, 0},
	{152, 0}, {156, 0}, {160, -8}, {164, 8}, {168, 8}, {172, 8}, {176, 8}, {88, 0}, {92, 0}, {96, 0},
};
/* The four int[16] arrays follow the fields in the order stats, persistant,
 * ammo, powerups: a presence bit, a short mask, then a short per element
 * (sign-extended on read) or, for powerups, a long (1.05@0x4146f4). */
struct NetFieldTable {
	const NetField *fields;
	unsigned count;
	std::array<std::uint16_t, 4> arrays;
};
inline constexpr NetFieldTable PlayerNetFields(int protocol) {
	if (protocol == 36) return {PlayerFields36, 35, {{0x8c, 0xcc, 0x14c, 0x10c}}};
	if (protocol == 38 || protocol == 39) return {PlayerFields38, 42, {{0xac, 0xec, 0x16c, 0x12c}}};
	return {PlayerFields40, 44, {{0xb4, 0xf4, 0x174, 0x134}}};
}

/* Protocol 35: a 16-bit flag word, its high half behind PS_MOREBITS 0x8000,
 * then each flagged group in this order (writer 1.02@0x413b50, reader
 * 1.02@0x4141b0; names from the reader's PS_* strings at 1.02@0x468aa8). */
enum class Value35 : std::uint8_t { Byte, Char, Short, Long, Float, Angle };
struct Group35 {
	std::uint32_t flag;
	std::uint8_t offset, storage, count;
	Value35 value;
};
inline constexpr Group35 PlayerGroups35[23] = {
	{0x20000, 0x00, 1, 1, Value35::Byte},    // PS_TYPE pm_type
	{0x1, 0x08, 4, 3, Value35::Float},       // PS_ORIGIN
	{0x2, 0x14, 4, 3, Value35::Float},       // PS_VELOCITY
	{0x4, 0x04, 2, 1, Value35::Short},       // PS_TIME pm_time
	{0x8, 0x02, 2, 1, Value35::Short},       // PS_FLAGS pm_flags
	{0x10000, 0x26, 2, 3, Value35::Short},   // PS_DELTA_ANGLES
	{0x10, 0x20, 2, 1, Value35::Short},      // PS_WEAPON_TIME
	{0x40, 0x50, 4, 3, Value35::Angle},      // PS_VIEWANGLES
	{0x800, 0x48, 4, 1, Value35::Byte},      // PS_WEAPON
	{0x80, 0x4c, 4, 1, Value35::Byte},       // PS_WEAPON_STATE
	{0x40000, 0x60, 1, 4, Value35::Byte},    // PS_DAMAGE_EVENT event, yaw, pitch, count
	{0x100000, 0x22, 2, 1, Value35::Short},  // PS_GRAVITY
	{0x200000, 0x24, 2, 1, Value35::Short},  // PS_SPEED
	{0x100, 0x01, 1, 1, Value35::Byte},      // PS_BOBCYCLE
	{0x1000000, 0x5c, 4, 1, Value35::Char},  // PS_VIEWHEIGHT
	{0x2000000, 0x44, 4, 1, Value35::Byte},  // PS_PLAYERNUM
	{0x4000000, 0x34, 4, 1, Value35::Long},  // PS_EFLAGS
	{0x20, 0x32, 1, 1, Value35::Byte},       // PS_MOVEMENTDIR
	{0x2000, 0x30, 1, 1, Value35::Byte},     // PS_LEGSANIM
	{0x1000, 0x31, 1, 1, Value35::Byte},     // PS_TORSOANIM
	{0x4000, 0x2e, 2, 1, Value35::Short},    // PS_ANIMTIMER
	{0x8000000, 0x38, 4, 2, Value35::Byte},  // PS_EVENT event, eventParm
	{0x10000000, 0x2c, 2, 1, Value35::Short}, // PS_GROUNDENTITY
};
// PS_STATS, PS_PERSISTANT, PS_AMMO (short[16]) and PS_POWERUPS (int[16]):
// a short mask, then a short or long per element.
struct Array35 {
	std::uint32_t flag;
	std::uint8_t offset, storage;
};
inline constexpr Array35 PlayerArrays35[4] = {{0x200, 0x64, 2}, {0x400000, 0x84, 2}, {0x400, 0xe4, 2}, {0x800000, 0xa4, 4}};
inline constexpr std::uint32_t MoreBits35 = 0x8000;
inline constexpr std::uint32_t KnownFlags35() {
	std::uint32_t flags = MoreBits35;
	for (const Group35 &group : PlayerGroups35) flags |= group.flag;
	for (const Array35 &list : PlayerArrays35) flags |= list.flag;
	return flags;
}
// x87 fcomp with only C3 tested (1.02@0x413b8c): -0.0 equals 0.0 and a NaN
// equals anything, so neither alone sets a group flag.
inline bool FloatChanged35(std::uint32_t from, std::uint32_t to) {
	return !IsNaN(from) && !IsNaN(to) && from != to && ((from | to) & 0x7fffffffu) != 0;
}
// MSG_WriteAngle16 (1.02@0x4130e0): __ftol(a * 182.044449f) & 0xffff; the
// float product is exact in double, and x87's integer indefinite gives 0.
inline std::uint32_t EncodeAngle(std::uint32_t bits) {
	if ((bits & 0x7fffffffu) >= 0x7f800000u) return 0;
	const double scaled = static_cast<double>(BitsFloat(bits)) * static_cast<double>(182.0444488525390625f);
	if (scaled < -9223372036854775808.0 || scaled >= 9223372036854775808.0) return 0;
	return static_cast<std::uint32_t>(static_cast<std::uint64_t>(static_cast<std::int64_t>(scaled)) & 0xffffu);
}
// MSG_ReadAngle16 (1.02@0x4132b0): ReadShort * 0.0054931640625, stored as a float.
inline std::uint32_t DecodeAngle(int value) { return FloatBits(static_cast<float>(value * 0.0054931640625)); }
// ReadFloat loads the raw bits with fld and its caller stores them with
// fstp (1.02@0x4131f7, 0x41427f), which quiets a signaling NaN.
inline std::uint32_t QuietFloat(std::uint32_t bits) { return IsNaN(bits) ? bits | 0x00400000u : bits; }

/* One player delta as it crosses the wire. 35: `changed` is the flag word as
 * sent, PS_MOREBITS included, and `masks` hold the flagged arrays' element
 * masks. 36-40: `changed` has a bit per table field, `fullFloats` marks
 * floats sent in the 32-bit form, `arrays` has the presence bits. Arrays and
 * masks are in wire order: stats, persistant, ammo, powerups. */
struct PlayerDelta {
	std::uint64_t changed = 0;
	std::uint64_t fullFloats = 0;
	std::uint8_t arrays = 0;
	std::array<std::uint16_t, 4> masks{};
};

namespace detail {
inline std::uint64_t NetFloatFields(const NetFieldTable &table) {
	std::uint64_t mask = 0;
	for (unsigned i = 0; i < table.count; ++i)
		if (!table.fields[i].bits) mask |= std::uint64_t(1) << i;
	return mask;
}
inline bool WriteValue35(Writer &out, Value35 value, std::uint32_t stored) {
	switch (value) {
	case Value35::Byte:
	case Value35::Char: return out.WriteBits(static_cast<std::int32_t>(stored), 8);
	case Value35::Short: return out.WriteBits(static_cast<std::int32_t>(stored), 16);
	case Value35::Long:
	case Value35::Float: return out.WriteBits(static_cast<std::int32_t>(stored), 32);
	case Value35::Angle: return out.WriteBits(static_cast<std::int32_t>(EncodeAngle(stored)), 16);
	}
	return out.Fail();
}
inline bool ReadValue35(Reader &in, Value35 value, std::uint32_t &stored) {
	std::int32_t read = 0;
	switch (value) {
	case Value35::Byte:
		if (!GetByte(in, read)) return false;
		stored = static_cast<std::uint32_t>(read);
		return true;
	case Value35::Char:
		if (!in.Has(1)) return in.Fail();
		stored = static_cast<std::uint32_t>(in.ReadChar());
		return true;
	case Value35::Short:
		if (!GetShort(in, read)) return false;
		stored = static_cast<std::uint32_t>(read);
		return true;
	case Value35::Long:
		if (!GetLong(in, read)) return false;
		stored = static_cast<std::uint32_t>(read);
		return true;
	case Value35::Float:
		if (!GetLong(in, read)) return false;
		stored = QuietFloat(static_cast<std::uint32_t>(read));
		return true;
	case Value35::Angle:
		if (!GetShort(in, read)) return false;
		stored = DecodeAngle(read);
		return true;
	}
	return in.Fail();
}
inline bool EncodePlayerRecord(Writer &out, int protocol, const PlayerDelta &delta, const Player &to) {
	if (!IsProtocol(protocol)) return out.Fail();
	if (protocol == 35) {
		const auto flags = static_cast<std::uint32_t>(delta.changed);
		if ((delta.changed >> 32) || (flags & ~KnownFlags35()) || ((flags >> 16) && !(flags & MoreBits35))) return out.Fail();
		if (!out.WriteBits(static_cast<std::int32_t>(flags & 0xffff), 16)) return false;
		if ((flags & MoreBits35) && !out.WriteBits(static_cast<std::int32_t>(flags >> 16), 16)) return false;
		for (const Group35 &group : PlayerGroups35) {
			if (!(flags & group.flag)) continue;
			for (unsigned i = 0; i < group.count; ++i)
				if (!WriteValue35(out, group.value, Load(to.data() + group.offset + i * group.storage, group.storage))) return false;
		}
		for (unsigned j = 0; j < 4; ++j) {
			const Array35 &list = PlayerArrays35[j];
			if (!(flags & list.flag)) continue;
			if (!out.WriteBits(delta.masks[j], 16)) return false;
			for (unsigned i = 0; i < 16; ++i) {
				if (!((delta.masks[j] >> i) & 1)) continue;
				const auto element = Load(to.data() + list.offset + i * list.storage, list.storage);
				if (!out.WriteBits(static_cast<std::int32_t>(element), list.storage * 8)) return false;
			}
		}
		return true;
	}
	const NetFieldTable table = PlayerNetFields(protocol);
	if ((delta.changed >> table.count) || (delta.fullFloats & ~(delta.changed & NetFloatFields(table))) || (delta.arrays >> 4))
		return out.Fail();
	for (unsigned i = 0; i < table.count; ++i) {
		const bool changed = ((delta.changed >> i) & 1) != 0;
		if (!out.WriteBits(changed, 1)) return false;
		if (changed && !WriteField(out, table.fields[i].bits, Load(to.data() + table.fields[i].offset, 4),
			((delta.fullFloats >> i) & 1) != 0)) return false;
	}
	for (unsigned j = 0; j < 4; ++j) {
		const bool present = ((delta.arrays >> j) & 1) != 0;
		if (!out.WriteBits(present, 1)) return false;
		if (!present) continue;
		if (!out.WriteBits(delta.masks[j], 16)) return false;
		for (unsigned i = 0; i < 16; ++i) {
			if (!((delta.masks[j] >> i) & 1)) continue;
			if (!out.WriteBits(static_cast<std::int32_t>(Load(to.data() + table.arrays[j] + 4 * i, 4)), j == 3 ? 32 : 16)) return false;
		}
	}
	return true;
}
inline bool EncodeDeltaPlayer(Writer &out, int protocol, const Player *from, const Player &to) {
	if (!IsProtocol(protocol)) return out.Fail();
	const Player zero{};
	const Player &base = from ? *from : zero;
	PlayerDelta delta;
	if (protocol == 35) {
		std::uint32_t flags = 0;
		for (const Group35 &group : PlayerGroups35) {
			// PS_DAMAGE_EVENT compares damageEvent alone yet sends all four bytes.
			const unsigned count = group.flag == 0x40000 ? 1u : group.count;
			for (unsigned i = 0; i < count; ++i) {
				const unsigned offset = group.offset + i * group.storage;
				const auto a = Load(base.data() + offset, group.storage), b = Load(to.data() + offset, group.storage);
				const bool floating = group.value == Value35::Float || group.value == Value35::Angle;
				if (floating ? FloatChanged35(a, b) : a != b) {
					flags |= group.flag;
					break;
				}
			}
		}
		for (unsigned j = 0; j < 4; ++j) {
			const Array35 &list = PlayerArrays35[j];
			for (unsigned i = 0; i < 16; ++i) {
				const unsigned offset = list.offset + i * list.storage;
				if (Load(base.data() + offset, list.storage) != Load(to.data() + offset, list.storage))
					delta.masks[j] = static_cast<std::uint16_t>(delta.masks[j] | (1u << i));
			}
			if (delta.masks[j]) flags |= list.flag;
		}
		if (flags & 0xffff0000u) flags |= MoreBits35;    // 1.02@0x413e1a
		delta.changed = flags;
		return EncodePlayerRecord(out, protocol, delta, to);
	}
	const NetFieldTable table = PlayerNetFields(protocol);
	for (unsigned i = 0; i < table.count; ++i) {
		const unsigned offset = table.fields[i].offset;
		const auto value = Load(to.data() + offset, 4);
		if (Load(base.data() + offset, 4) == value) continue;
		delta.changed |= std::uint64_t(1) << i;
		std::uint32_t code = 0;
		if (!table.fields[i].bits && !SmallFloat(value, code)) delta.fullFloats |= std::uint64_t(1) << i;
	}
	for (unsigned j = 0; j < 4; ++j) {
		for (unsigned i = 0; i < 16; ++i) {
			const unsigned offset = table.arrays[j] + 4 * i;
			if (Load(base.data() + offset, 4) != Load(to.data() + offset, 4))
				delta.masks[j] = static_cast<std::uint16_t>(delta.masks[j] | (1u << i));
		}
		if (delta.masks[j]) delta.arrays = static_cast<std::uint8_t>(delta.arrays | (1u << j));
	}
	return EncodePlayerRecord(out, protocol, delta, to);
}
} // namespace detail

/* MSG_WriteDeltaPlayerstate; a null `from` is the all-zero state on both
 * sides. Protocol 35 compares origin, velocity and viewangles as floats and
 * the other groups at their stored width; 36-40 compare raw dwords. */
inline bool WriteDeltaPlayer(Writer &out, int protocol, const Player *from, const Player &to) {
	return detail::Atomic(out, [&](Writer &w) { return detail::EncodeDeltaPlayer(w, protocol, from, to); });
}
inline bool WritePlayerRecord(Writer &out, int protocol, const PlayerDelta &delta, const Player &to) {
	return detail::Atomic(out, [&](Writer &w) { return detail::EncodePlayerRecord(w, protocol, delta, to); });
}
/* MSG_ReadDeltaPlayerstate. Unknown protocol-35 flag bits (0x80000 and
 * 0x20000000 upward), which retail ignores, fail here. */
inline bool ReadDeltaPlayer(Reader &in, int protocol, const Player *from, Player &to, PlayerDelta *delta = nullptr) {
	if (!IsProtocol(protocol)) return in.Fail();
	Reader next = in;
	Player decoded{};
	if (from) std::memcpy(decoded.data(), from->data(), PlayerBytes(protocol));
	PlayerDelta record;
	std::int32_t read = 0;
	if (protocol == 35) {
		if (!detail::GetShort(next, read)) return in.Fail();
		std::uint32_t flags = static_cast<std::uint32_t>(read) & 0xffff;
		if (flags & MoreBits35) {
			// ReadShort << 16 (1.02@0x41421a) keeps the short's own 16 bits.
			if (!detail::GetShort(next, read)) return in.Fail();
			flags |= (static_cast<std::uint32_t>(read) & 0xffff) << 16;
		}
		if (flags & ~KnownFlags35()) return in.Fail();
		record.changed = flags;
		for (const Group35 &group : PlayerGroups35) {
			if (!(flags & group.flag)) continue;
			for (unsigned i = 0; i < group.count; ++i) {
				std::uint32_t stored = 0;
				if (!detail::ReadValue35(next, group.value, stored)) return in.Fail();
				Store(decoded.data() + group.offset + i * group.storage, group.storage, stored);
			}
		}
		for (unsigned j = 0; j < 4; ++j) {
			const Array35 &list = PlayerArrays35[j];
			if (!(flags & list.flag)) continue;
			if (!detail::GetShort(next, read)) return in.Fail();
			record.masks[j] = static_cast<std::uint16_t>(read & 0xffff);
			for (unsigned i = 0; i < 16; ++i) {
				if (!((record.masks[j] >> i) & 1)) continue;
				if (!(list.storage == 4 ? detail::GetLong(next, read) : detail::GetShort(next, read))) return in.Fail();
				Store(decoded.data() + list.offset + i * list.storage, list.storage, static_cast<std::uint32_t>(read));
			}
		}
	} else {
		const NetFieldTable table = PlayerNetFields(protocol);
		for (unsigned i = 0; i < table.count; ++i) {
			if (!next.ReadBits(1)) continue;
			record.changed |= std::uint64_t(1) << i;
			std::uint32_t value = 0;
			bool full = false;
			detail::ReadField(next, table.fields[i].bits, value, full);
			Store(decoded.data() + table.fields[i].offset, 4, value);
			if (full) record.fullFloats |= std::uint64_t(1) << i;
		}
		for (unsigned j = 0; j < 4; ++j) {
			if (!next.ReadBits(1)) continue;
			record.arrays = static_cast<std::uint8_t>(record.arrays | (1u << j));
			if (!detail::GetShort(next, read)) return in.Fail();
			record.masks[j] = static_cast<std::uint16_t>(read & 0xffff);
			for (unsigned i = 0; i < 16; ++i) {
				if (!((record.masks[j] >> i) & 1)) continue;
				if (!(j == 3 ? detail::GetLong(next, read) : detail::GetShort(next, read))) return in.Fail();
				Store(decoded.data() + table.arrays[j] + 4 * i, 4, static_cast<std::uint32_t>(read));
			}
		}
	}
	if (!next.Good()) return in.Fail();
	to = decoded;
	in = next;
	if (delta) *delta = record;
	return true;
}

// ---- Usercmds ----

/* 35-39 (writer 1.05@0x413ed0, reader 1.05@0x414000): a flag byte for
 * angles[0..2] (short), forward/right/up (char), buttons, weapon (byte), then
 * msec (+0) always. 35 compares 16-bit angles, 36-39 32-bit ones, of which
 * the low 16 bits are sent and read back sign-extended. 40 (1.08@0x415400,
 * 0x4154c0): a change bit per field, then its low bits; serverTime replaces
 * msec and nothing is sign-extended on read. */
struct CommandField {
	std::uint8_t offset, storage, bits;
};
inline constexpr CommandField Commands35[8] = {{4, 2, 16}, {6, 2, 16}, {8, 2, 16}, {10, 1, 8}, {11, 1, 8}, {12, 1, 8}, {1, 1, 8}, {2, 1, 8}};
inline constexpr CommandField Commands36[8] = {{4, 4, 16}, {8, 4, 16}, {12, 4, 16}, {16, 1, 8}, {17, 1, 8}, {18, 1, 8}, {1, 1, 8}, {2, 1, 8}};
inline constexpr CommandField Commands40[9] = {
	{0, 4, 32}, {8, 4, 16}, {12, 4, 16}, {16, 4, 16}, {20, 1, 8}, {21, 1, 8}, {22, 1, 8}, {4, 1, 8}, {5, 1, 8},
};

namespace detail {
inline bool EncodeUsercmd(Writer &out, int protocol, const Usercmd *from, const Usercmd &to) {
	if (!IsProtocol(protocol)) return out.Fail();
	const Usercmd zero{};
	const Usercmd &base = from ? *from : zero;
	if (protocol == 40) {
		for (const CommandField &field : Commands40) {
			const auto value = Load(to.data() + field.offset, field.storage);
			const bool changed = Load(base.data() + field.offset, field.storage) != value;
			if (!out.WriteBits(changed, 1) || (changed && !out.WriteBits(static_cast<std::int32_t>(value), field.bits))) return false;
		}
		return true;
	}
	const CommandField *fields = protocol == 35 ? Commands35 : Commands36;
	unsigned flags = 0;
	for (unsigned i = 0; i < 8; ++i)
		if (Load(base.data() + fields[i].offset, fields[i].storage) != Load(to.data() + fields[i].offset, fields[i].storage)) flags |= 1u << i;
	if (!out.WriteByte(static_cast<std::int32_t>(flags))) return false;
	for (unsigned i = 0; i < 8; ++i)
		if (((flags >> i) & 1) && !out.WriteBits(static_cast<std::int32_t>(Load(to.data() + fields[i].offset, fields[i].storage)), fields[i].bits))
			return false;
	return out.WriteByte(to[0]);
}
} // namespace detail

// MSG_WriteDeltaUsercmd; a null `from` is the all-zero command.
inline bool WriteDeltaUsercmd(Writer &out, int protocol, const Usercmd *from, const Usercmd &to) {
	return detail::Atomic(out, [&](Writer &w) { return detail::EncodeUsercmd(w, protocol, from, to); });
}
inline bool ReadDeltaUsercmd(Reader &in, int protocol, const Usercmd *from, Usercmd &to) {
	if (!IsProtocol(protocol)) return in.Fail();
	Reader next = in;
	Usercmd decoded{};
	if (from) std::memcpy(decoded.data(), from->data(), UsercmdBytes(protocol));
	if (protocol == 40) {
		for (const CommandField &field : Commands40)
			if (next.ReadBits(1)) Store(decoded.data() + field.offset, field.storage, static_cast<std::uint32_t>(next.ReadBits(field.bits)));
	} else {
		const CommandField *fields = protocol == 35 ? Commands35 : Commands36;
		std::int32_t flags = 0, value = 0;
		if (!detail::GetByte(next, flags)) return in.Fail();
		for (unsigned i = 0; i < 8; ++i) {
			if (!((flags >> i) & 1)) continue;
			if (i < 3) {
				if (!detail::GetShort(next, value)) return in.Fail();
			} else if (i < 6) {
				if (!next.Has(1)) return in.Fail();
				value = next.ReadChar();
			} else if (!detail::GetByte(next, value)) {
				return in.Fail();
			}
			Store(decoded.data() + fields[i].offset, fields[i].storage, static_cast<std::uint32_t>(value));
		}
		if (!detail::GetByte(next, value)) return in.Fail();
		decoded[0] = static_cast<std::uint8_t>(value);
	}
	if (!next.Good()) return in.Fail();
	to = decoded;
	in = next;
	return true;
}

// ---- Server to client ----

struct Nop {};
// One gamestate entry: 3, short index, string, or 4, 10-bit number and an
// entity delta from the zero state that does not repeat the number.
struct GamestateEntry {
	std::uint8_t command = 3;
	std::int32_t index = 0;          // configstring index, or the baseline's number
	std::string text;
	EntityDelta delta{};             // baseline record as sent; delta.number == index
	Entity state{};                  // baseline as decoded
};
/* svc_gamestate (2): a long (none in 35; 36 csSequence, -1 in demos,
 * 1.05@0x41b09f; 38-40 serverCommandSequence, 1.06@0x41bc1b), then entries
 * until a 0 byte. The entry loop also ends at the end of data
 * (1.05@0x4076b6, `ReadByte() < 1`); `terminated` records which. */
struct Gamestate {
	std::int32_t sequence = 0;
	std::vector<GamestateEntry> entries;
	bool terminated = true;
};
// svc_configstring (3), 35/36 only: short index, string (1.05@0x407800).
struct ConfigstringUpdate {
	std::int32_t index = 0;
	std::string text;
};
/* svc_serverCommand (5): 35/36 a string, 38-40 a long sequence and a string
 * (1.06@0x408530). Text is carried verbatim: 38-40 servers store commands
 * with a literal 0x05 first (1.06@0x41d431) and callers keep it. `fresh` is
 * a 38-40 sequence above the client's; 35/36 commands are always fresh. */
struct ServerCommand {
	std::int32_t sequence = 0;
	std::string text;
	bool fresh = true;
};
struct EntityRecord {
	EntityDelta delta{};
	Entity state{};                  // as decoded, removed or dropped states included
	bool fromBaseline = false;       // decoded against baseline[number], not the old frame
};
// The client's delta-frame check (1.05@0x407405..0x4074a0).
enum class FrameStatus : std::uint8_t { Full, Delta, InvalidFrame, FrameTooOld, EntitiesTooOld };
/* svc_snapshot (7). Header, 35/36 (1.05@0x4073b0): serverTime, messageNum,
 * byte deltaOffset, csSequence, lastUsercmd, byte snapFlags; 38-40
 * (1.06@0x4080a0): reliableAcknowledge, serverTime, messageNum, byte
 * deltaOffset, lastUsercmd, byte snapFlags; then a byte count and that many
 * areamask bytes (aligned), the player delta and packet entities. 38-40
 * servers send the areamask inverted (1.06@0x41e7a0); it is carried as sent. */
struct Snapshot {
	std::int32_t reliableAcknowledge = 0;
	std::int32_t serverTime = 0;
	std::int32_t messageNum = 0;
	std::int32_t deltaOffset = 0;
	std::int32_t csSequence = 0;
	std::int32_t lastUsercmd = 0;
	std::int32_t snapFlags = 0;
	std::vector<std::uint8_t> areamask;
	Player player{};
	std::vector<Entity> entities;    // the frame, numbers ascending
	// Parse results; the retail-equivalent writer ignores them.
	PlayerDelta playerDelta{};
	std::vector<EntityRecord> records;
	FrameStatus status = FrameStatus::Full;
	std::int32_t commandSequence = 0;    // 38-40: the client's serverCommandSequence (1.06@0x4080cc)
	std::uint64_t parseEntitiesNum = 0;  // first parse-ring index of `entities`
	// 1.05@0x4073e4; retail wraps.
	std::int32_t DeltaNum() const {
		return deltaOffset ? static_cast<std::int32_t>(static_cast<std::uint32_t>(messageNum) - static_cast<std::uint32_t>(deltaOffset)) : -1;
	}
	bool Valid() const { return status == FrameStatus::Full || status == FrameStatus::Delta; }
};
using ServerOp = std::variant<Nop, Gamestate, ConfigstringUpdate, ServerCommand, Snapshot>;

/* What CL_ParseServerMessage keeps between messages: baselines, the 32-slot
 * frame ring and the counters that decide delta validity. A gamestate resets
 * it as CL_ClearState does. Cleared ring slots keep their frames, since
 * retail clears only the valid flag and parses against a stale slot. */
struct ClientState {
	explicit ClientState(int selected) : protocol(selected) {}
	int protocol = 0;
	std::shared_ptr<const Baselines> baselines;                  // null: all zero
	std::array<std::shared_ptr<const Snapshot>, 32> frames{};    // slot messageNum & 31
	std::array<bool, 32> valid{};
	std::shared_ptr<const Snapshot> snap;                        // cl.snap, the latest valid frame
	std::uint64_t parseEntitiesNum = 0;
	std::int32_t serverCommandSequence = 0;   // 38-40: from the gamestate and fresh commands
	std::int32_t csSequence = 0;              // 35/36: acknowledged as clc_move's csAck
	const Entity &Baseline(std::int32_t number) const {
		static const Entity zero{};
		return baselines && number >= 0 && number < 1024 ? (*baselines)[static_cast<std::size_t>(number)] : zero;
	}
	std::shared_ptr<const Snapshot> Frame(std::int32_t messageNum) const {
		const std::size_t slot = static_cast<std::uint32_t>(messageNum) & 31;
		return frames[slot] && valid[slot] && frames[slot]->messageNum == messageNum ? frames[slot] : nullptr;
	}
};

// A parsed message. Retail writers leave the last byte's spare bits zero;
// retail readers ignore them.
struct ServerMessage {
	std::vector<ServerOp> ops;
	std::size_t gaps = 0;            // alignment-gap bytes skipped
	unsigned paddingBits = 0;
	std::uint32_t padding = 0;
};

namespace detail {
inline bool ParseGamestate(Reader &in, int protocol, ClientState &state, Gamestate &game, std::string *error) {
	if (protocol != 35 && !GetLong(in, game.sequence)) return Error(error, "truncated gamestate");
	auto baselines = std::make_shared<Baselines>();
	std::size_t chars = 1;
	for (;;) {
		const int command = in.ReadByte();
		if (command < 1) {
			game.terminated = command == 0;
			break;
		}
		GamestateEntry entry;
		entry.command = static_cast<std::uint8_t>(command);
		if (command == 3) {
			if (!GetShort(in, entry.index) || entry.index < 0 || entry.index > 1023) return Error(error, "bad gamestate configstring index");
			if (!in.ReadString(protocol, entry.text)) return Error(error, "truncated gamestate configstring");
			if (chars + entry.text.size() + 1 > MaxGamestateChars) return Error(error, "MAX_GAMESTATE_CHARS exceeded");
			chars += entry.text.size() + 1;
		} else if (command == 4) {
			entry.index = in.ReadBits(10);
			const Entity zero{};
			if (!in.Good() || !ReadDeltaEntity(in, protocol, zero, entry.state, entry.index, &entry.delta))
				return Error(error, "bad gamestate baseline");
			(*baselines)[static_cast<std::size_t>(entry.index)] = entry.state;
		} else {
			return Error(error, "bad gamestate command byte");
		}
		game.entries.push_back(std::move(entry));
	}
	state.baselines = std::move(baselines);
	state.frames = {};
	state.valid = {};
	state.snap.reset();
	state.parseEntitiesNum = 0;
	if (IsLegacy(protocol)) state.csSequence = protocol == 36 ? game.sequence : 0;
	else state.serverCommandSequence = game.sequence;
	return true;
}
inline bool ParseSnapshot(Reader &in, int protocol, ClientState &state, Snapshot &snap, std::string *error) {
	const bool legacy = IsLegacy(protocol);
	std::int32_t areaBytes = 0;
	if ((!legacy && !GetLong(in, snap.reliableAcknowledge)) || !GetLong(in, snap.serverTime) || !GetLong(in, snap.messageNum)
		|| !GetByte(in, snap.deltaOffset) || (legacy && !GetLong(in, snap.csSequence)) || !GetLong(in, snap.lastUsercmd)
		|| !GetByte(in, snap.snapFlags) || !GetByte(in, areaBytes)) return Error(error, "truncated snapshot header");
	// The client copies the areamask into 32 bytes unchecked.
	if (areaBytes > static_cast<std::int32_t>(MaxAreaBytes)) return Error(error, "snapshot areamask over 32 bytes");
	snap.areamask.resize(static_cast<std::size_t>(areaBytes));
	if (!in.ReadData(snap.areamask.data(), snap.areamask.size())) return Error(error, "truncated snapshot areamask");
	snap.commandSequence = legacy ? 0 : state.serverCommandSequence;
	const std::int32_t deltaNum = snap.DeltaNum();
	const Snapshot *old = nullptr;
	if (deltaNum < 1) {
		snap.status = FrameStatus::Full;
	} else {
		const std::size_t slot = static_cast<std::uint32_t>(deltaNum) & 31;
		old = state.frames[slot].get();
		if (!old || !state.valid[slot]) snap.status = FrameStatus::InvalidFrame;
		else if (old->messageNum != deltaNum) snap.status = FrameStatus::FrameTooOld;
		else if (state.parseEntitiesNum - old->parseEntitiesNum > 2048 - 128) snap.status = FrameStatus::EntitiesTooOld;
		else snap.status = FrameStatus::Delta;
	}
	// Retail parses against the slot whatever its state, then discards.
	if (!ReadDeltaPlayer(in, protocol, old ? &old->player : nullptr, snap.player, &snap.playerDelta))
		return Error(error, "bad snapshot playerState");
	/* CL_ParsePacketEntities (1.05@0x407130, 1.06@0x407e40): merge the old
	 * frame's list with the records; numbers outside the old list decode
	 * against their baseline; old entities not named are copied unchanged.
	 * Retail does not require ascending numbers; this parser does. */
	static const std::vector<Entity> none;
	const std::vector<Entity> &oldList = old ? old->entities : none;
	std::size_t oldIndex = 0;
	std::int32_t last = -1;
	const auto oldNumber = [&]() -> std::int32_t { return oldIndex < oldList.size() ? EntityNumber(oldList[oldIndex]) : 99999; };
	for (;;) {
		const std::int32_t number = in.ReadBits(10);
		if (!in.Good()) return Error(error, "truncated packet entities");
		if (number == EntityEnd(protocol)) break;
		if (number <= last) return Error(error, "packet entity numbers out of order");
		last = number;
		while (oldNumber() < number) snap.entities.push_back(oldList[oldIndex++]);
		EntityRecord record;
		const Entity *source = nullptr;
		if (oldNumber() == number) {
			source = &oldList[oldIndex++];
		} else {
			source = &state.Baseline(number);
			record.fromBaseline = true;
		}
		if (!ReadDeltaEntity(in, protocol, *source, record.state, number, &record.delta)) return Error(error, "bad entity delta");
		if (EntityNumber(record.state) != EntityEnd(protocol)) snap.entities.push_back(record.state);
		snap.records.push_back(record);
	}
	while (oldIndex < oldList.size()) snap.entities.push_back(oldList[oldIndex++]);
	snap.parseEntitiesNum = state.parseEntitiesNum;
	state.parseEntitiesNum += snap.entities.size();
	if (snap.Valid()) {
		// Invalidate the slots skipped since cl.snap, then store the frame.
		std::int64_t clear = static_cast<std::int64_t>(state.snap ? state.snap->messageNum : 0) + 1;
		const std::int64_t current = snap.messageNum;
		if (current - clear >= 32) clear = current - 31;
		for (; clear < current; ++clear) state.valid[static_cast<std::uint64_t>(clear) & 31] = false;
		const auto stored = std::make_shared<const Snapshot>(snap);
		const std::size_t slot = static_cast<std::uint32_t>(snap.messageNum) & 31;
		state.frames[slot] = stored;
		state.valid[slot] = true;
		state.snap = stored;
		if (legacy) state.csSequence = snap.csSequence;
	}
	return true;
}
} // namespace detail

/* CL_ParseServerMessage (1.05@0x407a80, 1.06@0x408590, 1.08@0x408ec0):
 * opcodes are unaligned bytes and the message ends where ReadByte returns
 * -1; there is no svc_EOF. 35/36 clients take 1, 2, 3, 5 and 7, 38-40
 * clients 1, 2, 5 and 7; anything else is illegible. On failure neither
 * `state` nor `out` changes. */
inline bool ParseServerMessage(int protocol, const void *data, std::size_t size, ClientState &state, ServerMessage &out,
	std::string *error = nullptr) {
	if (!IsProtocol(protocol) || state.protocol != protocol) return detail::Error(error, "protocol mismatch");
	if ((!data && size) || size > MaxMessageBytes) return detail::Error(error, "bad message size");
	Reader in(data, size);
	ClientState next = state;
	ServerMessage result;
	for (;;) {
		const int command = in.ReadByte();
		if (command == -1) break;
		if (command == 1) {
			result.ops.emplace_back(Nop{});
		} else if (command == 2) {
			Gamestate game;
			if (!detail::ParseGamestate(in, protocol, next, game, error)) return false;
			result.ops.emplace_back(std::move(game));
		} else if (command == 3 && IsLegacy(protocol)) {
			ConfigstringUpdate update;
			if (!detail::GetShort(in, update.index) || update.index < 0 || update.index > 1023)
				return detail::Error(error, "bad configstring index");
			if (!in.ReadString(protocol, update.text)) return detail::Error(error, "truncated configstring");
			result.ops.emplace_back(std::move(update));
		} else if (command == 5) {
			ServerCommand received;
			if ((!IsLegacy(protocol) && !detail::GetLong(in, received.sequence)) || !in.ReadString(protocol, received.text))
				return detail::Error(error, "truncated server command");
			if (!IsLegacy(protocol)) {
				received.fresh = received.sequence > next.serverCommandSequence;
				if (received.fresh) next.serverCommandSequence = received.sequence;
			}
			result.ops.emplace_back(std::move(received));
		} else if (command == 7) {
			Snapshot snap;
			if (!detail::ParseSnapshot(in, protocol, next, snap, error)) return false;
			result.ops.emplace_back(std::move(snap));
		} else {
			return detail::Error(error, "illegible server message");
		}
	}
	if (!in.Good()) return detail::Error(error, "bad server message");
	result.gaps = in.Gaps();
	result.paddingBits = in.Bit() ? 8 - in.Bit() : 0;
	result.padding = result.paddingBits ? static_cast<std::uint32_t>(static_cast<const std::uint8_t *>(data)[size - 1] >> in.Bit()) : 0;
	state = std::move(next);
	out = std::move(result);
	return true;
}

namespace detail {
inline bool EncodeConfigEntry(Writer &out, int protocol, std::int32_t index, std::string_view text, std::size_t &chars) {
	if (index < 0 || index > 1023 || chars + text.size() + 1 > MaxGamestateChars) return out.Fail();
	chars += text.size() + 1;
	return out.WriteByte(3) && out.WriteShort(index) && out.WriteString(protocol, text);
}
/* Baselines a gamestate carries: 35/36 servers those with eType, eFlags,
 * loopSound or modelindex set (1.05@0x41b0ee), 35/36 demo recorders those
 * with modelindex set (1.02@0x40484d, 1.05@0x405125), 38-40 both those with
 * a nonzero number (1.06@0x41bc76, 1.06@0x40567a). */
inline bool BaselineSent(int protocol, const Entity &baseline, bool demo) {
	if (!IsLegacy(protocol)) return EntityNumber(baseline) != 0;
	const auto at = [&](unsigned offset) { return Load(baseline.data() + offset, 4) != 0; };
	return demo ? at(160) : at(4) || at(8) || at(156) || at(160);
}
inline bool EncodeStateGamestate(Writer &out, int protocol, std::int32_t sequence, const Configstrings &configstrings,
	const Baselines &baselines, bool demo) {
	if (!IsProtocol(protocol)) return out.Fail();
	if (demo && protocol == 36) sequence = -1;    // 1.05@0x4050bf
	if (!out.WriteByte(2) || (protocol != 35 && !out.WriteLong(sequence))) return false;
	std::size_t chars = 1;
	for (std::size_t i = 0; i < configstrings.size(); ++i)
		if (!configstrings[i].empty() && !EncodeConfigEntry(out, protocol, static_cast<std::int32_t>(i), configstrings[i], chars))
			return false;
	const Entity zero{};
	for (std::size_t i = 0; i < baselines.size(); ++i) {
		if (!BaselineSent(protocol, baselines[i], demo)) continue;
		if (EntityNumber(baselines[i]) != static_cast<std::int32_t>(i)) return out.Fail();
		if (!out.WriteByte(4) || !EncodeDeltaEntity(out, protocol, &zero, &baselines[i], true)) return false;
	}
	return out.WriteByte(0);
}
inline bool EncodeSnapshotHeader(Writer &out, int protocol, const Snapshot &snap) {
	if (snap.deltaOffset < 0 || snap.deltaOffset > 255 || snap.snapFlags < 0 || snap.snapFlags > 255
		|| snap.areamask.size() > MaxAreaBytes) return out.Fail();
	const bool legacy = IsLegacy(protocol);
	return out.WriteByte(7) && (legacy || out.WriteLong(snap.reliableAcknowledge)) && out.WriteLong(snap.serverTime)
		&& out.WriteLong(snap.messageNum) && out.WriteByte(snap.deltaOffset) && (!legacy || out.WriteLong(snap.csSequence))
		&& out.WriteLong(snap.lastUsercmd) && out.WriteByte(snap.snapFlags)
		&& out.WriteByte(static_cast<std::int32_t>(snap.areamask.size())) && out.WriteData(snap.areamask.data(), snap.areamask.size());
}
inline bool AscendingEntities(int protocol, const std::vector<Entity> &entities) {
	std::int32_t last = -1;
	for (const Entity &entity : entities) {
		const std::int32_t number = EntityNumber(entity);
		if (number <= last || !ValidEntityNumber(protocol, number)) return false;
		last = number;
	}
	return true;
}
/* SV_EmitPacketEntities (1.05@0x41d5c0, 1.06@0x41e270): merge by number;
 * the same entity is a delta from the old frame, a new one a forced delta
 * from its baseline, a missing one a removal; then the end marker. 35/36
 * stop when the message passes 1350 bytes (1.05@0x41d601) and keep only the
 * entities already emitted in the server's frame (0x41d6d6). */
inline bool EncodeSnapshot(Writer &out, int protocol, const Snapshot *from, const Snapshot &to, const Baselines &baselines,
	bool sizeCap, std::size_t &written) {
	// A deltaNum below 1 reads as a full snapshot (1.05@0x4073e4), so it cannot name `from`.
	if (!IsProtocol(protocol) || (sizeCap && !IsLegacy(protocol)) || (from != nullptr) != (to.deltaOffset != 0)
		|| (from && (to.DeltaNum() < 1 || from->messageNum != to.DeltaNum())) || !AscendingEntities(protocol, to.entities)
		|| (from && !AscendingEntities(protocol, from->entities))) return out.Fail();
	if (!EncodeSnapshotHeader(out, protocol, to) || !EncodeDeltaPlayer(out, protocol, from ? &from->player : nullptr, to.player))
		return false;
	static const std::vector<Entity> none;
	const std::vector<Entity> &oldList = from ? from->entities : none;
	std::size_t oldIndex = 0, newIndex = 0;
	while (newIndex < to.entities.size() || oldIndex < oldList.size()) {
		if (sizeCap && out.Size() > 1350) break;
		const std::int32_t newNumber = newIndex < to.entities.size() ? EntityNumber(to.entities[newIndex]) : 9999;
		const std::int32_t oldNumber = oldIndex < oldList.size() ? EntityNumber(oldList[oldIndex]) : 9999;
		if (newNumber == oldNumber) {
			if (!EncodeDeltaEntity(out, protocol, &oldList[oldIndex++], &to.entities[newIndex++], false)) return false;
		} else if (newNumber < oldNumber) {
			if (!EncodeDeltaEntity(out, protocol, &baselines[static_cast<std::size_t>(newNumber)], &to.entities[newIndex++], true))
				return false;
		} else if (!EncodeDeltaEntity(out, protocol, &oldList[oldIndex++], nullptr, true)) {
			return false;
		}
	}
	written = newIndex;
	return out.WriteBits(EntityEnd(protocol), 10);
}
inline bool EncodeServerOp(Writer &out, int protocol, const ServerOp &op) {
	if (!IsProtocol(protocol)) return out.Fail();
	if (std::holds_alternative<Nop>(op)) return out.WriteByte(1);
	if (const auto *game = std::get_if<Gamestate>(&op)) {
		if (!out.WriteByte(2) || (protocol != 35 && !out.WriteLong(game->sequence))) return false;
		std::size_t chars = 1;
		for (const GamestateEntry &entry : game->entries) {
			if (entry.command == 3) {
				if (!EncodeConfigEntry(out, protocol, entry.index, entry.text, chars)) return false;
			} else if (entry.command != 4 || entry.delta.number != entry.index || !out.WriteByte(4)
				|| !EncodeEntityRecord(out, protocol, entry.delta, entry.state)) {
				return out.Fail();
			}
		}
		return !game->terminated || out.WriteByte(0);
	}
	if (const auto *update = std::get_if<ConfigstringUpdate>(&op)) {
		if (!IsLegacy(protocol) || update->index < 0 || update->index > 1023) return out.Fail();
		return out.WriteByte(3) && out.WriteShort(update->index) && out.WriteString(protocol, update->text);
	}
	if (const auto *command = std::get_if<ServerCommand>(&op))
		return out.WriteByte(5) && (IsLegacy(protocol) || out.WriteLong(command->sequence)) && out.WriteString(protocol, command->text);
	const auto &snap = std::get<Snapshot>(op);
	if (!EncodeSnapshotHeader(out, protocol, snap) || !EncodePlayerRecord(out, protocol, snap.playerDelta, snap.player)) return false;
	std::int32_t last = -1;
	for (const EntityRecord &record : snap.records) {
		if (record.delta.number <= last) return out.Fail();
		last = record.delta.number;
		if (!EncodeEntityRecord(out, protocol, record.delta, record.state)) return false;
	}
	return out.WriteBits(EntityEnd(protocol), 10);
}
} // namespace detail

inline bool WriteNop(Writer &out) { return out.WriteByte(1); }
inline bool WriteConfigstring(Writer &out, int protocol, std::int32_t index, std::string_view text) {
	return detail::Atomic(out, [&](Writer &w) {
		return IsLegacy(protocol) && index >= 0 && index <= 1023 ? w.WriteByte(3) && w.WriteShort(index) && w.WriteString(protocol, text)
			: w.Fail();
	});
}
/* The opcode is an unaligned byte and the string aligns, as the client reads
 * them. 35/36 servers append "\x05" + text + "\0" in one aligned MSG_WriteData
 * (1.05@0x41c777): the same bytes whenever the message is byte-aligned, which
 * is the only case a retail client parses correctly. `sequence` is ignored
 * in 35/36. */
inline bool WriteServerCommand(Writer &out, int protocol, std::int32_t sequence, std::string_view text) {
	return detail::Atomic(out, [&](Writer &w) {
		return IsProtocol(protocol) && w.WriteByte(5) && (IsLegacy(protocol) || w.WriteLong(sequence)) && w.WriteString(protocol, text);
	});
}
/* SV_SendGameState's body (1.05@0x41b092, 1.06@0x41bc0a): every non-empty
 * configstring in index order, the baselines retail selects, then 0. Each
 * selected baseline must carry its own index as number. */
inline bool WriteServerGamestate(Writer &out, int protocol, std::int32_t sequence, const Configstrings &configstrings,
	const Baselines &baselines) {
	return detail::Atomic(out, [&](Writer &w) {
		return detail::EncodeStateGamestate(w, protocol, sequence, configstrings, baselines, false);
	});
}
/* The first record of a recording (CL_Record_f, 1.02@0x4047ea, 1.05@0x4050b6,
 * 1.06@0x405606, 1.08@0x405f16): a gamestate synthesized from the client's
 * own configstrings and baselines; 35 writes no long, 36 always -1, 38-40
 * clc.serverCommandSequence (`sequence`). */
inline bool WriteDemoGamestate(Writer &out, int protocol, std::int32_t sequence, const Configstrings &configstrings,
	const Baselines &baselines) {
	return detail::Atomic(out, [&](Writer &w) {
		return detail::EncodeStateGamestate(w, protocol, sequence, configstrings, baselines, true);
	});
}
/* SV_WriteSnapshotToClient (1.05@0x41d7a1, 1.06@0x41e425): `from` is the
 * frame to.deltaOffset names, or null when deltaOffset is 0. `baselines` are
 * the server's own: 35/36 servers delta new entities from baselines their
 * gamestate did not carry (BaselineSent), which the client takes as zero.
 * `sizeCap` applies the 35/36 1350-byte cap, measured over everything
 * already in `out`; `entitiesWritten` receives how many of to.entities were
 * emitted, which the server's frame must then keep. 35/36 build snapshot
 * messages in 1400 bytes (1.05@0x41de6f), 38-40 in 16384. */
inline bool WriteSnapshot(Writer &out, int protocol, const Snapshot *from, const Snapshot &to, const Baselines &baselines,
	bool sizeCap = false, std::size_t *entitiesWritten = nullptr) {
	std::size_t written = 0;
	if (!detail::Atomic(out, [&](Writer &w) { return detail::EncodeSnapshot(w, protocol, from, to, baselines, sizeCap, written); }))
		return false;
	if (entitiesWritten) *entitiesWritten = written;
	return true;
}
/* A server message puts its unacknowledged records -- 35/36's configstrings,
 * 38-40's commands -- before its snapshot. `item(w, i)` writes record i and
 * `finish(w)` the snapshot. MessageFits says whether every record and then the
 * snapshot fit after what `out` holds; WriteFittingPrefix writes records in
 * order while each still leaves room for the snapshot and returns how many
 * went in, or -1 when one that fit on a dry copy did not. Neither writes the
 * snapshot, and neither leaves `out` failed when a record does not fit. */
template<class Item, class Finish> bool MessageFits(const Writer &out, int count, Item &&item, Finish &&finish) {
	Writer dry = out.Dry();
	for (int i = 0; i < count; ++i) {
		if (!item(dry, i)) return false;
	}
	return finish(dry);
}
template<class Item, class Finish> int WriteFittingPrefix(Writer &out, int count, Item &&item, Finish &&finish) {
	int written = 0;
	for (; written < count; ++written) {
		Writer dry = out.Dry();
		if (!item(dry, written) || !finish(dry)) break;
		if (!item(out, written)) return -1;
	}
	return written;
}
// Writes a parsed operation exactly as received: gamestate entries in their
// order, records with their masks, indexes and float forms.
inline bool WriteServerOp(Writer &out, int protocol, const ServerOp &op) {
	return detail::Atomic(out, [&](Writer &w) { return detail::EncodeServerOp(w, protocol, op); });
}

// ---- Client to server ----

/* clc_move (2). 35/36 (1.05@0x4038e0 / 0x41b620): serverId, clientTime,
 * deltaMessage, csAck, lastCmd = cmdNumber - 1; 38-40 (1.06@0x403d80 /
 * 0x41c0c0): serverCommandSequence, serverId, clientTime, deltaMessage,
 * lastCmd = cmdNumber. Then a count byte and the commands, the first delta
 * coded from zero. Which command numbers they are, and 35/36's trailing
 * provisional command, are the caller's business. */
struct Move {
	std::int32_t serverCommandSequence = 0;
	std::int32_t serverId = 0;
	std::int32_t clientTime = 0;
	std::int32_t deltaMessage = -1;
	std::int32_t csAck = 0;
	std::int32_t lastCmd = 0;
	std::vector<Usercmd> cmds;
};
// clc_clientCommand (3): 35/36 a string, 38-40 a long sequence and a string (1.06@0x403dbf).
struct ClientCommand {
	std::int32_t sequence = 0;
	std::string text;
};
using ClientOp = std::variant<Nop, Move, ClientCommand>;
struct ClientMessage {
	std::vector<ClientOp> ops;
	std::size_t gaps = 0;
	unsigned paddingBits = 0;
	std::uint32_t padding = 0;
};

namespace detail {
inline bool EncodeMove(Writer &out, int protocol, const Move &move) {
	if (!IsProtocol(protocol) || move.cmds.size() < MinMoveCommands(protocol) || move.cmds.size() > MaxMoveCommands) return out.Fail();
	const bool legacy = IsLegacy(protocol);
	if (!out.WriteByte(2) || (!legacy && !out.WriteLong(move.serverCommandSequence)) || !out.WriteLong(move.serverId)
		|| !out.WriteLong(move.clientTime) || !out.WriteLong(move.deltaMessage) || (legacy && !out.WriteLong(move.csAck))
		|| !out.WriteLong(move.lastCmd) || !out.WriteByte(static_cast<std::int32_t>(move.cmds.size()))) return false;
	const Usercmd *from = nullptr;
	for (const Usercmd &cmd : move.cmds) {
		if (!EncodeUsercmd(out, protocol, from, cmd)) return false;
		from = &cmd;
	}
	return true;
}
inline bool EncodeClientOp(Writer &out, int protocol, const ClientOp &op) {
	if (!IsProtocol(protocol)) return out.Fail();
	if (std::holds_alternative<Nop>(op)) return out.WriteByte(1);
	if (const auto *move = std::get_if<Move>(&op)) return EncodeMove(out, protocol, *move);
	const auto &command = std::get<ClientCommand>(op);
	return out.WriteByte(3) && (IsLegacy(protocol) || out.WriteLong(command.sequence)) && out.WriteString(protocol, command.text);
}
} // namespace detail

inline bool WriteMove(Writer &out, int protocol, const Move &move) {
	return detail::Atomic(out, [&](Writer &w) { return detail::EncodeMove(w, protocol, move); });
}
inline bool WriteClientCommand(Writer &out, int protocol, std::int32_t sequence, std::string_view text) {
	return detail::Atomic(out, [&](Writer &w) {
		return IsProtocol(protocol) && w.WriteByte(3) && (IsLegacy(protocol) || w.WriteLong(sequence)) && w.WriteString(protocol, text);
	});
}
inline bool WriteClientOp(Writer &out, int protocol, const ClientOp &op) {
	return detail::Atomic(out, [&](Writer &w) { return detail::EncodeClientOp(w, protocol, op); });
}
/* SV_ExecuteClientMessage (1.05@0x41b830, 1.06@0x41c2c0): unaligned opcode
 * bytes until ReadByte returns -1; 1 nop, 2 move, 3 client command, anything
 * else drops the client. There is no clc_moveNoDelta and no usercmd key. */
inline bool ParseClientMessage(int protocol, const void *data, std::size_t size, ClientMessage &out, std::string *error = nullptr) {
	if (!IsProtocol(protocol)) return detail::Error(error, "protocol mismatch");
	if ((!data && size) || size > MaxMessageBytes) return detail::Error(error, "bad message size");
	const bool legacy = IsLegacy(protocol);
	Reader in(data, size);
	ClientMessage result;
	for (;;) {
		const int command = in.ReadByte();
		if (command == -1) break;
		if (command == 1) {
			result.ops.emplace_back(Nop{});
		} else if (command == 2) {
			Move move;
			std::int32_t count = 0;
			if ((!legacy && !detail::GetLong(in, move.serverCommandSequence)) || !detail::GetLong(in, move.serverId)
				|| !detail::GetLong(in, move.clientTime) || !detail::GetLong(in, move.deltaMessage)
				|| (legacy && !detail::GetLong(in, move.csAck)) || !detail::GetLong(in, move.lastCmd) || !detail::GetByte(in, count))
				return detail::Error(error, "truncated move");
			if (count < static_cast<std::int32_t>(MinMoveCommands(protocol)) || count > static_cast<std::int32_t>(MaxMoveCommands))
				return detail::Error(error, "bad usercmd count");
			move.cmds.resize(static_cast<std::size_t>(count));
			for (std::size_t i = 0; i < move.cmds.size(); ++i)
				if (!ReadDeltaUsercmd(in, protocol, i ? &move.cmds[i - 1] : nullptr, move.cmds[i])) return detail::Error(error, "truncated usercmd");
			result.ops.emplace_back(std::move(move));
		} else if (command == 3) {
			ClientCommand received;
			if ((!legacy && !detail::GetLong(in, received.sequence)) || !in.ReadString(protocol, received.text))
				return detail::Error(error, "truncated client command");
			result.ops.emplace_back(std::move(received));
		} else {
			return detail::Error(error, "unknown client command");
		}
	}
	if (!in.Good()) return detail::Error(error, "bad client message");
	result.gaps = in.Gaps();
	result.paddingBits = in.Bit() ? 8 - in.Bit() : 0;
	result.padding = result.paddingBits ? static_cast<std::uint32_t>(static_cast<const std::uint8_t *>(data)[size - 1] >> in.Bit()) : 0;
	out = std::move(result);
	return true;
}

// ---- Demo files ----

/* .dm3: records of int32 length (LittleLong) and that many message bytes,
 * after the netchan header, ended by length -1; no header, sequence or time
 * (CL_WriteDemoMessage 1.05@0x404db0, CL_ReadDemoMessage 1.05@0x405230).
 * Lengths over 16384 are fatal in retail. A zero length stops retail
 * playback as "Demo file was truncated." (fread of 0 bytes reports
 * nothing read) and is reported as Truncated; a missing terminator is
 * Truncated too, and bytes after the terminator are Malformed. */
enum class DemoResult { Message, End, Truncated, Malformed };
struct DemoRecord {
	const std::uint8_t *data = nullptr;
	std::size_t size = 0;
};
class DemoReader {
public:
	DemoReader(const void *data, std::size_t size) : data_(static_cast<const std::uint8_t *>(data)), size_(data ? size : 0) {}
	DemoResult Next(DemoRecord &record) {
		if (ended_) return DemoResult::End;
		if (size_ - position_ < 4) return DemoResult::Truncated;
		const auto length = static_cast<std::int32_t>(Load(data_ + position_, 4));
		if (length == -1) {
			if (size_ - position_ != 4) return DemoResult::Malformed;
			position_ += 4;
			ended_ = true;
			return DemoResult::End;
		}
		if (length < 0 || static_cast<std::size_t>(length) > MaxMessageBytes) return DemoResult::Malformed;
		if (!length || static_cast<std::size_t>(length) > size_ - position_ - 4) return DemoResult::Truncated;
		record = {data_ + position_ + 4, static_cast<std::size_t>(length)};
		position_ += 4 + static_cast<std::size_t>(length);
		return DemoResult::Message;
	}
	std::size_t Position() const { return position_; }

private:
	const std::uint8_t *data_;
	std::size_t size_, position_ = 0;
	bool ended_ = false;
};
/* Caller-owned, bounded .dm3 output. A failed Append or Finish changes no
 * byte and no position. Empty messages are refused, since retail playback
 * would stop at them. */
class DemoWriter {
public:
	DemoWriter(void *data, std::size_t size) : data_(static_cast<std::uint8_t *>(data)), size_(data ? size : 0) {}
	bool Append(const void *message, std::size_t bytes) {
		if (!data_ || !message || ended_ || !bytes || bytes > MaxMessageBytes || size_ - position_ < 4 || bytes > size_ - position_ - 4)
			return false;
		std::memmove(data_ + position_ + 4, message, bytes);
		Store(data_ + position_, 4, static_cast<std::uint32_t>(bytes));
		position_ += 4 + bytes;
		return true;
	}
	bool Finish() {
		if (ended_) return true;
		if (!data_ || size_ - position_ < 4) return false;
		Store(data_ + position_, 4, 0xffffffffu);
		position_ += 4;
		ended_ = true;
		return true;
	}
	std::size_t Position() const { return position_; }

private:
	std::uint8_t *data_;
	std::size_t size_, position_ = 0;
	bool ended_ = false;
};
} // namespace fntech3::q3test::wire
#endif
