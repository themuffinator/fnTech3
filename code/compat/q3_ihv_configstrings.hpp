/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_Q3_IHV_CONFIGSTRINGS_HPP
#define FNTECH3_Q3_IHV_CONFIGSTRINGS_HPP

#include "q3_ihv_client_abi.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string_view>

namespace fntech3::q3ihv::client {

/* Original initial gamestate parsing appends every string, including empty
 * and repeated indices. A changed configstring rebuilds all nonempty strings
 * in index order; an unchanged update preserves the entire previous record.
 * Native GetGameState copies all 20100 bytes, not only the live string data.
 * Failed validation leaves both the previous storage and changed flag intact. */
class ConfigStrings {
public:
	ConfigStrings() { Put(FNTECH3_Q3_IHV_CONFIG_COUNT_OFFSET, 1); }
	void Clear() { std::memset(storage_.bytes, 0, sizeof storage_.bytes); Put(FNTECH3_Q3_IHV_CONFIG_COUNT_OFFSET, 1); }
	const fntech3_q3_ihv_gameState_t &Record() const { return storage_; }
	std::uint32_t BytesUsed() const { return Get(FNTECH3_Q3_IHV_CONFIG_COUNT_OFFSET); }
	std::string_view String(std::uint32_t index) const {
		if (index >= FNTECH3_Q3_IHV_MAX_CONFIGSTRINGS) return {};
		const auto offset = Get(index * 4);
		const char *text = reinterpret_cast<const char *>(storage_.bytes + FNTECH3_Q3_IHV_CONFIG_DATA_OFFSET + offset);
		return {text, std::strlen(text)};
	}
	bool Append(std::uint32_t index, std::string_view text) {
		if (!Valid(index, text) || text.size() + 1 > FNTECH3_Q3_IHV_CONFIG_DATA_BYTES - BytesUsed()) return false;
		// The caller may pass a view into this record, including a previous string.
		std::array<char, 1024> copy{};
		if (!text.empty()) std::memcpy(copy.data(), text.data(), text.size());
		const auto offset = BytesUsed();
		std::memcpy(storage_.bytes + FNTECH3_Q3_IHV_CONFIG_DATA_OFFSET + offset, copy.data(), text.size() + 1);
		Put(index * 4, offset);
		Put(FNTECH3_Q3_IHV_CONFIG_COUNT_OFFSET, offset + static_cast<std::uint32_t>(text.size()) + 1);
		return true;
	}
	bool Set(std::uint32_t index, std::string_view text, bool &changed) {
		if (!Valid(index, text)) return false;
		if (String(index) == text) { changed = false; return true; }
		ConfigStrings next;
		for (std::uint32_t i = 0; i < FNTECH3_Q3_IHV_MAX_CONFIGSTRINGS; ++i) {
			const auto value = i == index ? text : String(i);
			if (!value.empty() && !next.Append(i, value)) return false;
		}
		storage_ = next.storage_;
		changed = true;
		return true;
	}
private:
	static bool Valid(std::uint32_t index, std::string_view text) {
		if (index >= FNTECH3_Q3_IHV_MAX_CONFIGSTRINGS || text.size() > 1023) return false;
		for (unsigned char c : text) if (!c || c == 255) return false;
		return true;
	}
	std::uint32_t Get(std::size_t offset) const {
		std::uint32_t value = 0;
		for (unsigned i = 0; i < 4; ++i) value |= std::uint32_t(storage_.bytes[offset + i]) << (i * 8);
		return value;
	}
	void Put(std::size_t offset, std::uint32_t value) {
		for (unsigned i = 0; i < 4; ++i) storage_.bytes[offset + i] = static_cast<std::uint8_t>(value >> (i * 8));
	}
	fntech3_q3_ihv_gameState_t storage_{};
};
}
#endif
