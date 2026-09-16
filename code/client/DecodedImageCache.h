/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 fnTech3 contributors
 */
#pragma once

#include <cstddef>
#include <cstring>
#include <list>
#include <new>
#include <string>
#include <utility>
#include <vector>

namespace fntech3 {

// The caller still reads through the active filesystem on every request.
// Exact source-byte comparison preserves pure/override and live-edit behavior;
// this cache saves decoding work, never file admission or payload validation.
class DecodedImageCache {
public:
	struct Entry {
		std::string name;
		std::vector<unsigned char> source;
		std::vector<unsigned char> pixels;
		int width;
		int height;
	};

	explicit DecodedImageCache(std::size_t capacity) : capacity_(capacity) {}

	const Entry *Find(const char *name, const unsigned char *source, std::size_t length) {
		++misses_;
		for (auto it = entries_.begin(); it != entries_.end(); ++it) {
			if (it->name != name) continue;
			if (it->source.size() != length || std::memcmp(it->source.data(), source, length) != 0) {
				Erase(it);
				return nullptr;
			}
			entries_.splice(entries_.begin(), entries_, it);
			++hits_; --misses_;
			return &entries_.front();
		}
		return nullptr;
	}

	void Store(const char *name, const unsigned char *source, std::size_t length,
		const unsigned char *pixels, int width, int height) {
		if (!source || !pixels || width <= 0 || height <= 0 || length > capacity_) return;
		const auto row = static_cast<std::size_t>(width) * 4;
		if (row / 4 != static_cast<std::size_t>(width) ||
			static_cast<std::size_t>(height) > (capacity_ - length) / row) return;
		const auto pixelBytes = row * static_cast<std::size_t>(height);
		const auto bytes = length + pixelBytes;
		for (auto it = entries_.begin(); it != entries_.end(); ++it) {
			if (it->name == name) { Erase(it); break; }
		}
		while (!entries_.empty() && (used_ > capacity_ - bytes || entries_.size() >= 512)) {
			Erase(--entries_.end());
		}
		try {
			Entry entry{name, {source, source + length}, {pixels, pixels + pixelBytes}, width, height};
			entries_.push_front(std::move(entry));
			used_ += bytes;
		} catch (const std::bad_alloc &) {
			// Caching is optional. A successfully decoded image remains usable.
		}
	}

	std::size_t Bytes() const { return used_; }
	std::size_t Hits() const { return hits_; }
	std::size_t Misses() const { return misses_; }
	void SetCapacity(std::size_t capacity) {
		capacity_ = capacity;
		while (!entries_.empty() && used_ > capacity_) Erase(--entries_.end());
	}

private:
	void Erase(std::list<Entry>::iterator it) {
		used_ -= it->source.size() + it->pixels.size();
		entries_.erase(it);
	}
	std::list<Entry> entries_;
	std::size_t capacity_;
	std::size_t used_ = 0;
	std::size_t hits_ = 0;
	std::size_t misses_ = 0;
};

} // namespace fntech3
