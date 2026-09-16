/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 fnTech3 contributors
 */
#pragma once

#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>

namespace fntech3::audio {

// Seekable view used while decoding a complete sound. The caller owns the
// immutable bytes and keeps them alive until the decoder closes its stream.
struct AudioMemoryStream {
	const unsigned char *data;
	int length;
	int position = 0;

	std::size_t Read(void *destination, std::size_t size, std::size_t count) {
		if (!size || !count) return 0;
		if (!destination || !data || position < 0 || position > length) {
			errno = EINVAL;
			return 0;
		}
		if (count > std::numeric_limits<std::size_t>::max() / size) {
			errno = EINVAL;
			return 0;
		}
		const auto requested = size * count;
		const auto remaining = static_cast<std::size_t>(length - position);
		const auto bytes = requested < remaining ? requested : remaining;
		std::memcpy(destination, data + position, bytes);
		position += static_cast<int>(bytes);
		return bytes / size;
	}

	int Seek(std::int64_t offset, int origin) {
		std::int64_t base;
		switch (origin) {
			case SEEK_SET: base = 0; break;
			case SEEK_CUR: base = position; break;
			case SEEK_END: base = length; break;
			default: errno = EINVAL; return -1;
		}
		if (base < 0 || base > length || offset < -base || offset > length - base) {
			errno = EINVAL;
			return -1;
		}
		position = static_cast<int>(base + offset);
		return 0;
	}
};

} // namespace fntech3::audio
