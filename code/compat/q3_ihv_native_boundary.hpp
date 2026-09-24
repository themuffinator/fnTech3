/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_Q3_IHV_NATIVE_BOUNDARY_HPP
#define FNTECH3_Q3_IHV_NATIVE_BOUNDARY_HPP

#include "q3_ihv_game_abi.h"
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>

namespace fntech3::q3ihv {

static_assert(sizeof(fntech3_q3_ihv_cvar_t) == 32);
static_assert(offsetof(fntech3_q3_ihv_cvar_t, value) == 24);
static_assert(sizeof(fntech3_q3_ihv_entity_t) == 268);
static_assert(offsetof(fntech3_q3_ihv_entity_t, client) == 0xa4);
static_assert(offsetof(fntech3_q3_ihv_entity_t, linked) == 0xac);
static_assert(offsetof(fntech3_q3_ihv_entity_t, linkcount) == 0xb0);
static_assert(offsetof(fntech3_q3_ihv_entity_t, mins) == 0xbc);
static_assert(offsetof(fntech3_q3_ihv_entity_t, contents) == 0xd4);
static_assert(offsetof(fntech3_q3_ihv_entity_t, absmin) == 0xd8);
static_assert(offsetof(fntech3_q3_ihv_entity_t, currentOrigin) == 0xf0);
static_assert(offsetof(fntech3_q3_ihv_entity_t, owner) == 0x108);
static_assert(sizeof(fntech3_q3_ihv_trace_t) == 56);
static_assert(offsetof(fntech3_q3_ihv_trace_t, fraction) == 8);
static_assert(offsetof(fntech3_q3_ihv_trace_t, entityNum) == 52);
static_assert(sizeof(fntech3_q3_ihv_usercmd_t) == 14);

/* Engine 0x418fc0/0x4190d0 converts a native entity pointer by the exported
 * stride. Validate the entire allocation first, then reject interior and
 * one-past pointers. World is index zero; native clients are indices 1..N. */
class EntityArray {
public:
	bool Bind(void *base, std::size_t span, int stride, int count, int capacity) {
		Clear();
		if (!base || stride < static_cast<int>(sizeof(fntech3_q3_ihv_entity_t))
			|| stride % alignof(fntech3_q3_ihv_entity_t) || capacity < 1 || count < 1
			|| count > capacity || reinterpret_cast<std::uintptr_t>(base) % alignof(fntech3_q3_ihv_entity_t)
			|| static_cast<std::size_t>(capacity) > span / static_cast<std::size_t>(stride)) return false;
		const auto bytes = static_cast<std::size_t>(capacity) * static_cast<std::size_t>(stride);
		if (reinterpret_cast<std::uintptr_t>(base) > (std::numeric_limits<std::uintptr_t>::max)() - bytes) return false;
		base_ = static_cast<unsigned char *>(base); stride_ = stride; count_ = count; capacity_ = capacity;
		return true;
	}
	void Clear() { base_ = nullptr; stride_ = count_ = capacity_ = 0; }
	bool SetCount(int count) {
		if (!base_ || count < 1 || count > capacity_) return false;
		count_ = count; return true;
	}
	int Index(const void *entity, bool allocated = false) const {
		if (!base_ || !entity) return -1;
		const auto pointer = reinterpret_cast<std::uintptr_t>(entity);
		const auto base = reinterpret_cast<std::uintptr_t>(base_);
		if (pointer < base) return -1;
		const auto offset = pointer - base;
		const auto limit = static_cast<std::size_t>(allocated ? capacity_ : count_);
		if (offset % stride_ || offset / stride_ >= limit) return -1;
		return static_cast<int>(offset / stride_);
	}
	fntech3_q3_ihv_entity_t *At(int index, bool allocated = false) const {
		if (!base_ || index < 0 || index >= (allocated ? capacity_ : count_)) return nullptr;
		return reinterpret_cast<fntech3_q3_ihv_entity_t *>(base_ + static_cast<std::size_t>(index) * stride_);
	}
	int ClientIndex(const void *entity, int clients) const {
		if (clients < 1 || clients >= count_) return -1;
		const int index = Index(entity);
		return index > 0 && index <= clients ? index - 1 : -1;
	}
private:
	unsigned char *base_ = nullptr;
	int stride_ = 0, count_ = 0, capacity_ = 0;
};

inline int EntityNumber(const fntech3_q3_ihv_entity_t &entity) {
	std::int16_t number; std::memcpy(&number, entity.state, sizeof(number)); return number;
}

/* BotAllocateClient 0x4176a4 republishes the slot's entity number as a signed
 * short before the module sees the entity. Only that word is written; the rest
 * of the native state belongs to the game. */
inline bool SetEntityNumber(fntech3_q3_ihv_entity_t &entity, int number) {
	if (number < (std::numeric_limits<std::int16_t>::min)()
		|| number > (std::numeric_limits<std::int16_t>::max)()) return false;
	const auto word = static_cast<std::int16_t>(number);
	std::memcpy(entity.state, &word, sizeof(word));
	return true;
}

/* Cvar mirrors own their strings, so a module never sees the engine's expanded
 * cvar_t. Records retain their addresses across updates. The engine-side host
 * supplies already-resolved values/flags and synchronizes modified separately;
 * this object does not reimplement cvar permissions, latching or persistence. */
class CvarMirror {
public:
	static constexpr std::size_t NameSize = 256, ValueSize = 1024;
	CvarMirror() = default;
	CvarMirror(const CvarMirror &) = delete;
	CvarMirror &operator=(const CvarMirror &) = delete;
	bool Assign(const char *name, const char *value, const char *reset, const char *latched,
		int flags, int modified, float numericValue, std::uint32_t next = 0) {
		if (sizeof(void *) != 4 || !Fits(name, NameSize) || !*name || !Fits(value, ValueSize)
			|| !Fits(reset, ValueSize) || (latched && !Fits(latched, ValueSize))) return false;
		// Inputs may refer to this mirror's old strings, including another field.
		char newName[NameSize]{}, newValue[ValueSize]{}, newReset[ValueSize]{}, newLatched[ValueSize]{};
		std::memcpy(newName, name, std::strlen(name) + 1);
		std::memcpy(newValue, value, std::strlen(value) + 1);
		std::memcpy(newReset, reset, std::strlen(reset) + 1);
		if (latched) std::memcpy(newLatched, latched, std::strlen(latched) + 1);
		std::memcpy(name_, newName, NameSize); std::memcpy(value_, newValue, ValueSize);
		std::memcpy(reset_, newReset, ValueSize); std::memcpy(latched_, newLatched, ValueSize);
		record_ = {Address(name_), Address(value_), Address(reset_), latched ? Address(latched_) : 0,
			flags, modified, numericValue, next};
		return true;
	}
	fntech3_q3_ihv_cvar_t *Record() { return &record_; }
	const fntech3_q3_ihv_cvar_t *Record() const { return &record_; }
private:
	static bool Fits(const char *text, std::size_t size) {
		if (!text) return false;
		for (std::size_t i = 0; i < size; ++i) if (!text[i]) return true;
		return false;
	}
	static std::uint32_t Address(const void *pointer) {
		return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(pointer));
	}
	fntech3_q3_ihv_cvar_t record_{};
	char name_[NameSize]{}, value_[ValueSize]{}, reset_[ValueSize]{}, latched_[ValueSize]{};
};

/* Per-module allocation ownership. Original TagMalloc zeroes every byte and
 * stores a signed 16-bit tag; FreeTags compares its sign-extended value with
 * the full argument. The ownership list is outside module-writable storage.
 * Bounds and invalid frees fail without dereferencing an unowned pointer. */
class TaggedMemory {
public:
	explicit TaggedMemory(std::size_t budget) : budget_(budget) {}
	~TaggedMemory() { Clear(); }
	TaggedMemory(const TaggedMemory &) = delete;
	TaggedMemory &operator=(const TaggedMemory &) = delete;
	void *Allocate(int bytes, int tag) {
		if (bytes < 0 || budget_ - charged_ < sizeof(Block)) return nullptr;
		const std::size_t storage = bytes ? static_cast<std::size_t>(bytes) : 1;
		if (storage > budget_ - charged_ - sizeof(Block)) return nullptr;
		auto *block = static_cast<Block *>(std::malloc(sizeof(Block)));
		if (!block) return nullptr;
		void *data = std::calloc(storage, 1);
		if (!data) { std::free(block); return nullptr; }
		const std::uint16_t bits = static_cast<std::uint16_t>(tag);
		std::int16_t signedTag; std::memcpy(&signedTag, &bits, sizeof(signedTag));
		*block = {head_, data, static_cast<std::size_t>(bytes), signedTag};
		head_ = block; used_ += block->bytes; charged_ += storage + sizeof(Block); ++count_; return data;
	}
	bool Free(void *pointer) {
		for (Block **link = &head_; *link; link = &(*link)->next) {
			if ((*link)->data == pointer) { Release(link); return true; }
		}
		return false;
	}
	void FreeTags(int tag) {
		for (Block **link = &head_; *link;) {
			if ((*link)->tag == tag) Release(link); else link = &(*link)->next;
		}
	}
	void Clear() { while (head_) Release(&head_); }
	std::size_t Bytes() const { return used_; }
	std::size_t Count() const { return count_; }
	std::size_t AllocationSize(const void *pointer) const {
		for (const Block *block = head_; block; block = block->next)
			if (block->data == pointer) return block->bytes;
		return 0;
	}
	/* Validate a published record without reading module-controlled memory.
	 * A client slot must name its exact position in one complete allocation,
	 * not an interior byte, another slot, a freed block or a neighboring arena. */
	bool OwnsRecord(const void *pointer, int index, int count, std::size_t stride,
		std::size_t requiredBytes) const {
		if (!pointer || index < 0 || count < 1 || index >= count || !stride
			|| !requiredBytes || requiredBytes > stride
			|| static_cast<std::size_t>(count) > (std::numeric_limits<std::size_t>::max)() / stride) return false;
		const auto bytes=static_cast<std::size_t>(count)*stride;
		const auto offset=static_cast<std::size_t>(index)*stride;
		const auto address=reinterpret_cast<std::uintptr_t>(pointer);
		for (const Block *block=head_;block;block=block->next) {
			const auto base=reinterpret_cast<std::uintptr_t>(block->data);
			if(block->bytes==bytes && address>=base && address-base==offset)return true;
		}
		return false;
	}
private:
	struct Block { Block *next; void *data; std::size_t bytes; int tag; };
	void Release(Block **link) {
		Block *block = *link; *link = block->next; used_ -= block->bytes; --count_;
		charged_ -= (block->bytes ? block->bytes : 1) + sizeof(Block);
		std::free(block->data); std::free(block);
	}
	Block *head_ = nullptr;
	std::size_t budget_, used_ = 0, count_ = 0, charged_ = 0;
};

} // namespace fntech3::q3ihv
#endif
