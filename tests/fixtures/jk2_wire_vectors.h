/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
/* Synthetic protocol-15 records passed through the retail 1.02a x86 body
 * instructions by tests/jk2_retail_wire_oracle.py. This contains only synthetic
 * inputs and observed outputs, no proprietary binaries, code or assets.
 * Engine SHA-256: 2f08dcf9a089cb66062766f4922fda63a9e553de893c174b497dfefeb7a1577a.
 * MSG_WriteBits/ReadBits were bounded raw IO hooks, so Huffman is not included. */
#ifndef FNTECH3_JK2_WIRE_VECTORS_H
#define FNTECH3_JK2_WIRE_VECTORS_H

namespace jk2_fixture {
struct WordPatch { unsigned offset; std::uint32_t value; };
enum class Kind { Entity, Player, Command };
struct Vector {
 const char *name; Kind kind; std::uint32_t key;
 const WordPatch *baseline; unsigned baselineCount;
 const WordPatch *sent; unsigned sentCount;
 const WordPatch *decoded; unsigned decodedCount;
 const std::uint8_t *wire; unsigned wireBytes, wireBits;
};
inline constexpr WordPatch v0_baseline[] = { {0u, 0u} };
inline constexpr WordPatch v0_sent[] = { {0u, 0x00000007u} };
inline constexpr WordPatch v0_decoded[] = { {0u, 0x00000007u} };
inline constexpr std::uint8_t v0_wire[] = { 0x07, 0x00 };
inline constexpr WordPatch v1_baseline[] = { {0u, 0u} };
inline constexpr WordPatch v1_sent[] = { {0u, 0x0000002au}, {16u, 0x000003e8u}, {24u, 0xc5800000u}, {28u, 0x457ff000u}, {212u, 0xfffffffdu}, {236u, 0x00000013u}, {244u, 0x00000001u} };
inline constexpr WordPatch v1_decoded[] = { {0u, 0x0000002au}, {16u, 0x000003e8u}, {24u, 0xc5800000u}, {28u, 0x457ff000u}, {212u, 0xfffffffdu}, {236u, 0x00000013u}, {244u, 0x00000001u} };
inline constexpr std::uint8_t v1_wire[] = { 0x2a, 0x98, 0x34, 0xfa, 0x00, 0x00, 0xc0, 0x00, 0xc0, 0xfe, 0x3f, 0x00, 0x00, 0x00, 0x00, 0x00, 0xee, 0x07, 0x00, 0x00, 0x3c, 0xe1 };
inline constexpr WordPatch v2_baseline[] = { {0u, 0u} };
inline constexpr WordPatch v2_sent[] = { {0u, 0x000003feu}, {24u, 0x3fa00000u}, {164u, 0x7fc12345u}, {196u, 0xaabbccddu} };
inline constexpr WordPatch v2_decoded[] = { {0u, 0x000003feu}, {24u, 0x3fa00000u}, {164u, 0x7fc12345u}, {196u, 0xaabbccddu} };
inline constexpr std::uint8_t v2_wire[] = { 0xfe, 0x3b, 0xe4, 0x00, 0x00, 0xa0, 0x3f, 0x00, 0x00, 0x80, 0x17, 0x8d, 0x04, 0xff, 0x01, 0x00, 0x00, 0x00, 0x00, 0xdc, 0xcd, 0xbc, 0xab, 0x0a };
inline constexpr WordPatch v3_baseline[] = { {0u, 0u} };
inline constexpr WordPatch v3_sent[] = { {0u, 0x00003039u}, {44u, 0xffffffefu}, {168u, 0xfffffff4u}, {216u, 0xffff8000u}, {340u, 0x00007fffu}, {372u, 0x87654321u}, {420u, 0xffffffffu} };
inline constexpr WordPatch v3_decoded[] = { {0u, 0x00003039u}, {44u, 0xffffffefu}, {168u, 0xfffffff4u}, {216u, 0xffff8000u}, {340u, 0x00007fffu}, {372u, 0x87654321u}, {420u, 0xffffffffu} };
inline constexpr std::uint8_t v3_wire[] = { 0x21, 0x73, 0x60, 0x00, 0x00, 0x00, 0xdf, 0xff, 0x01, 0x00, 0x00, 0xe9, 0x0f, 0x00, 0x00, 0x00, 0x0c, 0x00, 0xf8, 0xff, 0x17, 0x01, 0xe0, 0xff, 0x3f, 0x20, 0x40, 0xc8, 0x50, 0xd9, 0x21 };
inline constexpr WordPatch v4_baseline[] = { {0u, 0u} };
inline constexpr WordPatch v4_sent[] = { {20u, 0x3fa00000u}, {832u, 0xabcd1234u}, {988u, 0x00000003u}, {1080u, 0x45800000u}, {1356u, 0xc5800000u}, {1364u, 0x7f800000u} };
inline constexpr WordPatch v4_decoded[] = { {20u, 0x3fa00000u}, {832u, 0xabcd1234u}, {988u, 0x00000003u}, {1080u, 0x45800000u}, {1356u, 0xc5800000u}, {1364u, 0x7f800000u} };
inline constexpr std::uint8_t v4_wire[] = { 0x68, 0x06, 0x00, 0x00, 0xfd, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x00, 0x00, 0xb0, 0x48, 0x1a, 0x89, 0xe6, 0x55, 0x38, 0x00, 0x00, 0x00, 0x00, 0x0c, 0x00, 0x00, 0xf8, 0x17, 0x00, 0x00 };
inline constexpr WordPatch v5_baseline[] = { {0u, 0x000003e8u} };
inline constexpr WordPatch v5_sent[] = { {0u, 0x000003f2u}, {4u, 0x00009c40u}, {8u, 0x0000000cu}, {16u, 0x00001234u}, {20u, 0x0b090704u}, {24u, 0x00ff7f80u} };
inline constexpr WordPatch v5_decoded[] = { {0u, 0x000003f2u}, {4u, 0x00019c40u}, {8u, 0x0001000cu}, {16u, 0x00011234u}, {20u, 0x0b090704u}, {24u, 0x00ff7f80u} };
inline constexpr std::uint8_t v5_wire[] = { 0x15, 0x56, 0x4e, 0x6e, 0x58, 0xa5, 0xc2, 0xfa, 0x75, 0x7d, 0x8f, 0x3a, 0x6e, 0x3c, 0x38, 0x10 };
inline constexpr WordPatch v6_baseline[] = { {0u, 0x000003e8u} };
inline constexpr WordPatch v6_sent[] = { {0u, 0x000003e8u} };
inline constexpr WordPatch v6_decoded[] = { {0u, 0x000003e8u} };
inline constexpr std::uint8_t v6_wire[] = { 0x01, 0x00 };
inline constexpr WordPatch v7_baseline[] = { {0u, 0u} };
inline constexpr WordPatch v7_sent[] = { {0u, 0x000186a0u}, {4u, 0x00008000u} };
inline constexpr WordPatch v7_decoded[] = { {0u, 0x000186a0u}, {4u, 0x00008000u} };
inline constexpr std::uint8_t v7_wire[] = { 0x40, 0x0d, 0x03, 0x00, 0x06, 0x35, 0x00, 0x00 };
inline constexpr WordPatch v8_baseline[] = { {0u, 0x000003e8u} };
inline constexpr WordPatch v8_sent[] = { {0u, 0x000003e7u} };
inline constexpr WordPatch v8_decoded[] = { {0u, 0x000004e7u} };
inline constexpr std::uint8_t v8_wire[] = { 0xff, 0x01 };

inline constexpr Vector Vectors[] = {
 { "entity-zero", Kind::Entity, 0x00000000u, v0_baseline, 0, v0_sent, 1, v0_decoded, 1, v0_wire, 2, 12 },
 { "entity-integral-floats", Kind::Entity, 0x00000000u, v1_baseline, 0, v1_sent, 7, v1_decoded, 7, v1_wire, 22, 176 },
 { "entity-binary32", Kind::Entity, 0x00000000u, v2_baseline, 0, v2_sent, 4, v2_decoded, 4, v2_wire, 24, 188 },
 { "player-arrays", Kind::Player, 0x00000000u, v3_baseline, 0, v3_sent, 7, v3_decoded, 7, v3_wire, 31, 246 },
 { "player-force", Kind::Player, 0x00000000u, v4_baseline, 0, v4_sent, 6, v4_decoded, 6, v4_wire, 33, 260 },
 { "command-raven", Kind::Command, 0x12355678u, v5_baseline, 1, v5_sent, 6, v5_decoded, 6, v5_wire, 16, 125 },
 { "command-no-change", Kind::Command, 0x12355678u, v6_baseline, 1, v6_sent, 1, v6_decoded, 1, v6_wire, 2, 10 },
 { "command-long-time", Kind::Command, 0x00010000u, v7_baseline, 0, v7_sent, 2, v7_decoded, 2, v7_wire, 8, 61 },
 { "command-backwards-time", Kind::Command, 0x00000000u, v8_baseline, 1, v8_sent, 1, v8_decoded, 1, v8_wire, 2, 10 }
};
} // namespace jk2_fixture
#endif
