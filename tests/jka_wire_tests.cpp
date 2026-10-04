/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#include "../code/compat/jka_message_codec.hpp"
#include "../code/compat/jk2_message_codec.hpp"
#include "../code/compat/wolfet_message_codec.hpp"
#include <cstdio>
#include <limits>
#include <string>
#include <vector>

namespace codec = fntech3::jka::message;
#define CHECK(test) do { if (!(test)) { std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #test); return __LINE__; } } while (false)

// Exact bit limits let malformed/truncated bodies be checked at every bit,
// independently of Huffman coding and the engine's msg_t storage policy.
struct Bits {
    std::vector<std::uint8_t> data;
    std::size_t position = 0, limit = 0;
    bool good = true;
    void Write(std::uint32_t value, int bits) {
        const auto width = static_cast<std::size_t>(bits < 0 ? -bits : bits);
        if (!good || !width || width > 32) { good = false; return; }
        for (std::size_t n = 0; n < width; ++n) {
            if (position / 8 >= data.size()) data.push_back(0);
            data[position / 8] |= static_cast<std::uint8_t>(((value >> n) & 1u) << (position % 8));
            ++position;
        }
        limit = position;
    }
    std::uint32_t Read(int bits) {
        const auto width = static_cast<std::size_t>(bits < 0 ? -bits : bits);
        if (!good || !width || width > 32 || position > limit || width > limit-position) { good = false; return 0; }
        std::uint32_t value = 0;
        for (std::size_t n = 0; n < width; ++n, ++position)
            value |= static_cast<std::uint32_t>((data[position / 8] >> (position % 8)) & 1u) << n;
        if (bits < 0 && width < 32 && (value & (1u << (width-1)))) value |= ~0u << width;
        return value;
    }
    bool Good() const { return good; }
    bool Fail() { good = false; return false; }
    void Rewind(std::size_t size) { position = 0; limit = size; good = true; }
};

int Overrides() {
    codec::Tables tables;
    CHECK(codec::Configure(tables, ";comment without final newline", "\r\n;comment\r\r\n"));
    CHECK(tables.entity[0].bits == 32 && tables.player[0].bits == 32);
    CHECK(codec::Configure(tables, "pos.trTime, 15\r\nowner, GENTITYNUM_BITS", "commandTime, -16\nfd.forcePowerLevel[FP_SEE], 3"));
    CHECK(tables.entity[0].bits == 15 && tables.player[0].bits == -16);
    bool found = false;
    for (const auto field : tables.player) if (field.name == "fd.forcePowerLevel[FP_SEE]") { CHECK(field.bits == 3); found = true; }
    CHECK(found && codec::pilotFields[0].bits == 32 && codec::vehicleFields[0].bits == 32);
    const auto expected = tables;
    for (const auto text : {"owner", "owner,", "owner, 33", "owner, -32", "owner, 2147483647999", "owner, 4junk", "owner, --1", "owner, 1,2"}) {
        CHECK(!codec::Configure(tables, "pos.trTime, 2", text));
        CHECK(tables.entity[0].bits == expected.entity[0].bits && tables.player[0].bits == expected.player[0].bits);
    }
    const char nul[]{'x', '\0', 'y'};
    CHECK(!codec::Configure(tables, std::string_view(nul, sizeof(nul)), ""));
    const std::string tooLong(4096, ';');
    CHECK(!codec::Configure(tables, tooLong, ""));
    CHECK(codec::Configure(tables, "obsoleteRetailExample, 10", ""));
    CHECK(tables.entity[0].bits == 32); // fresh defaults at each configuration
    return 0;
}

int KnownBodies() {
    codec::Tables tables;
    codec::Entity entity{}, received{}; entity.number = 7;
    Bits out;
    CHECK(codec::WriteEntity(out, nullptr, &entity, true, tables));
    CHECK(out.position == 12 && out.data == std::vector<std::uint8_t>({7, 0}));
    out.Rewind(out.limit);
    CHECK(out.Read(10) == 7 && codec::ReadEntity(out, nullptr, &received, 7, tables));
    CHECK(received.number == 7);
    out = {};
    CHECK(codec::WriteEntity(out, &entity, nullptr, false, tables));
    CHECK(out.position == 11 && out.data == std::vector<std::uint8_t>({7, 4}));
    out = {}; entity.number = 1023;
    CHECK(!codec::WriteEntity(out, nullptr, &entity, true, tables) && out.position == 0);
    codec::Player player{}, decoded{};
    out = {}; CHECK(codec::WritePlayer(out, nullptr, &player, false, tables));
    CHECK(out.position == 10 && out.data == std::vector<std::uint8_t>({0, 0}));
    out = {}; CHECK(codec::WritePlayer(out, nullptr, &player, true, tables));
    CHECK(out.position == 9);
    player.stats[4] = 1 << 18; // final JKA weapon, wider than a Q3 short
    out = {}; CHECK(codec::WritePlayer(out, nullptr, &player, false, tables));
    CHECK(out.position == 49 && out.data == std::vector<std::uint8_t>({0, 0x86, 0, 0, 0, 0x20, 0}));
    out.Rewind(out.limit); CHECK(codec::ReadPlayer(out, nullptr, &decoded, false, tables));
    CHECK(decoded.stats[4] == 1 << 18);
    codec::Command cmd{}, command{}; cmd.serverTime = 123;
    out = {}; CHECK(codec::WriteCommand(out, 0, nullptr, &cmd));
    CHECK(out.position == 10 && out.data == std::vector<std::uint8_t>({0xf7, 0}));
    out.Rewind(out.limit); CHECK(codec::ReadCommand(out, 0, nullptr, &command));
    CHECK(command.serverTime == 123);
    return 0;
}

template<class State, class Read> int Truncated(Bits &out, const State &baseline, Read read) {
    for (std::size_t size = 0; size <= out.limit; ++size) for (const bool alias : {false, true}) {
        State destination{};
        std::memset(&destination, 0xa5, sizeof(destination));
        if (alias) destination = baseline;
        const auto unchanged = destination;
        auto input = out; input.Rewind(size);
        const bool complete = size == out.limit;
        CHECK(read(input, alias ? &destination : &baseline, &destination) == complete);
        CHECK(complete || !std::memcmp(&destination, &unchanged, sizeof(destination)));
    }
    return 0;
}

int BoundsAndFloats() {
    codec::Tables tables;
    for (const auto word : {0x7f800000u, 0xff800000u, 0x7fc12345u, 0x7f7fffffu, 0x00800000u, 0x80000000u}) {
        codec::Entity base{}, sent{}, received{}; sent.number = 1;
        codec::SetWord(sent, offsetof(codec::Entity, origin), word);
        Bits out; CHECK(codec::WriteEntity(out, &base, &sent, true, tables));
        out.Rewind(out.limit); CHECK(out.Read(10) == 1);
        CHECK(codec::ReadEntity(out, &base, &received, 1, tables));
        CHECK(codec::Word(received, offsetof(codec::Entity, origin)) == (word == 0x80000000u ? 0 : word));
    }
    codec::Entity entityBase{}, entity{}; entity.number = 3; entity.userInt3 = 1;
    Bits out; CHECK(codec::WriteEntity(out, &entityBase, &entity, false, tables));
    auto entityRead = [&tables](auto &io, const auto *base, auto *next) { const auto id = io.Read(10); return codec::ReadEntity(io, base, next, id, tables); };
    if (const auto result = Truncated(out, entityBase, entityRead)) return result;
    for (const bool vehicle : {false, true}) for (const bool pilot : {false, true}) {
        codec::Player base{}, sent{};
        sent.userVec2[2] = 1.0f; sent.stats[4] = 1 << 18; sent.ammo[15] = -1;
        if (pilot) { sent.m_iVehicleNum = 5; sent.eFlags = 256; }
        out = {}; CHECK(codec::WritePlayer(out, &base, &sent, vehicle, tables));
        auto read = [&tables, vehicle](auto &io, const auto *old, auto *next) { return codec::ReadPlayer(io, old, next, vehicle, tables); };
        if (const auto result = Truncated(out, base, read)) return result;
        // Counts are bounded before any table indexing or record publication.
        Bits bad; if (!vehicle) bad.Write(pilot, 1); bad.Write(255, 8);
        bad.Rewind(bad.limit); codec::Player unchanged{}, next{};
        std::memset(&next, 0xa5, sizeof(next)); unchanged = next;
        CHECK(!codec::ReadPlayer(bad, &base, &next, vehicle, tables));
        CHECK(!std::memcmp(&next, &unchanged, sizeof(next)));
    }
    codec::Command base{}, cmd{}; cmd.serverTime = 1500; cmd.generic_cmd = 127; cmd.forwardmove = -128;
    out = {}; CHECK(codec::WriteCommand(out, 0x12345678, &base, &cmd));
    auto commandRead = [](auto &io, const auto *old, auto *next) { return codec::ReadCommand(io, 0x12345678, old, next); };
    return Truncated(out, base, commandRead);
}

int ScalarEncodings() {
    struct Case { std::uint32_t word; int compact; };
    constexpr Case cases[]{
        {0x00000000u, 0}, {0x80000000u, 0},
        {0x3f800000u, 1}, {0xbf800000u, -1},
        {0x457ff000u, 4095}, {0xc5800000u, -4096},
        {0x45800000u, 4096}, {0xc5800800u, 4096},
        {0x46000000u, 4096}, {0xc6000000u, 4096},
        {0x3fc00000u, 4096}, {0xbfc00000u, 4096},
        {0x3f7fffffu, 4096}, {0x00000001u, 4096},
        {0x80000001u, 4096}, {0x00800000u, 4096},
        {0x7f7fffffu, 4096}, {0x7f800000u, 4096},
        {0xff800000u, 4096}, {0x7fc12345u, 4096},
        {0xffc54321u, 4096}, {0x7f800001u, 4096},
    };
    for (const auto sample : cases) for (const bool entity : {false, true}) {
        // RTCW, ET and JKA share one scalar writer; JK2 has its own wrapper.
        Bits shared, jk2;
        codec::WriteScalar(shared, sample.word, 0, entity);
        fntech3::jk2::message::WriteScalar(jk2, sample.word, 0, entity);
        CHECK(shared.data == jk2.data && shared.position == jk2.position);
        const bool zero = (sample.word & 0x7fffffffu) == 0;
        const bool compact = sample.compact != 4096;
        const auto expectedBits = static_cast<std::size_t>(entity && zero ? 1 :
            (entity ? 1 : 0) + (compact ? 14 : 33));
        CHECK(shared.position == expectedBits);
        shared.Rewind(shared.limit);
        if (entity) CHECK(shared.Read(1) == static_cast<std::uint32_t>(!zero));
        if (entity && zero) continue;
        CHECK(shared.Read(1) == static_cast<std::uint32_t>(!compact));
        CHECK(shared.Read(compact ? 13 : 32) == (compact ?
            static_cast<std::uint32_t>(sample.compact+4096) : sample.word));
    }
    // Every representable compact integer retains the exact legacy encoding.
    for (int integer = -4096; integer < 4096; ++integer) {
        const auto real = static_cast<float>(integer);
        std::uint32_t word; std::memcpy(&word, &real, 4);
        int decoded = 8192;
        CHECK(fntech3::delta::CompactFloatInteger(word, decoded) && decoded == integer);
    }
    return 0;
}

int main() {
    if (const auto result = Overrides()) return result;
    if (const auto result = KnownBodies()) return result;
    if (const auto result = BoundsAndFloats()) return result;
    return ScalarEncodings();
}
