/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_WOLFET_TAGS_HPP
#define FNTECH3_WOLFET_TAGS_HPP
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

namespace fntech3::wolfet::tags {
// Version 1 consists of a 16-byte header and 112-byte tag records.
// The published header names TAG1; installed ET assets actually carry TAG3.
// Observed format, independently bounded decoder; see the ET evidence record.
struct Tag { std::array<char,64> name{}; std::array<float,12> transform{}; };
inline uint32_t Word(const uint8_t *p) {
    return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
}
inline bool Parse(const void *bytes, size_t size, std::vector<Tag> &destination) {
    if (!bytes || size < 16) return false;
    const auto *p = static_cast<const uint8_t *>(bytes);
    const auto count = Word(p + 8), end = Word(p + 12);
    if ((Word(p) != 0x31474154 && Word(p) != 0x33474154) || Word(p + 4) != 1 || count >= 256 ||
        end != 16 + count * 112 || end > size) return false;
    std::vector<Tag> parsed(count);
    for (size_t n = 0; n < count; ++n) {
        const auto *record = p + 16 + n * 112;
        if (!std::memchr(record, 0, 64)) return false;
        std::memcpy(parsed[n].name.data(), record, 64);
        for (size_t c = 0; c < 12; ++c) {
            const auto value = Word(record + 64 + c * 4);
            std::memcpy(&parsed[n].transform[c], &value, 4);
            if (!std::isfinite(parsed[n].transform[c])) return false;
        }
    }
    destination.swap(parsed);
    return true;
}
}
#endif
