/* Numeric observations of the hash-pinned IHV codecs, 2026-09-21.
 * No executable code or game data is incorporated. Generate with
 * tests/q3_ihv_reference_wire_probe.cpp. Each digest covers the synthetic case
 * index, bit alignment, encoded bit count, encoded bytes and decoded state.
 * Executable SHA256: e9fcaa0a5682e7f07d540aacee08cdb77f3ea40190719f5cecf31109a5135d37
 */
#ifndef FNTECH3_Q3_IHV_WIRE_VECTORS_H
#define FNTECH3_Q3_IHV_WIRE_VECTORS_H
#include <cstdint>
namespace ihv_vectors {
constexpr unsigned EntityCases = 1690, CommandCases = 241, PlayerCases = 2593;
constexpr unsigned Alignments = 8;
constexpr std::uint64_t EntityHash = 0xebfa8e3a99a505deull;
constexpr std::uint64_t CommandHash = 0xb70fc23d1e6092e0ull;
constexpr std::uint64_t PlayerHash = 0x7c77f151d62c6a03ull;
}
#endif
