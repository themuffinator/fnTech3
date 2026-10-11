/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#include "../code/qcommon/q_shared.h"
#include "../code/qcommon/qcommon.h"
#include "../code/cgame/cg_public.h"
#include "../code/client/client_jka_cgame.hpp"
#include "../code/compat/jka_cgame_exports.h"
#include <array>
#include <cstdio>
#include <cstring>
#include <limits>

#define CHECK(test) do { if (!(test)) { std::fprintf(stderr, "%d: %s\n", __LINE__, #test); return __LINE__; } } while (false)
namespace client = fntech3::jka::client;
namespace state = fntech3::jka::state;
namespace cgame = fntech3::jka::cgame;
using Call = fntech3::jka::module::Call;

static int Mapping() {
    CHECK(cgame::SharedImport(0) == CG_PRINT && cgame::SharedImport(7) == CG_CVAR_SET);
    CHECK(cgame::SharedImport(100) == TRAP_MEMSET && cgame::SharedImport(103) == TRAP_SIN);
    CHECK(cgame::SharedImport(110) == CG_FLOOR && cgame::SharedImport(114) == CG_ACOS);
    CHECK(cgame::SharedImport(229) == CG_GETCURRENTSNAPSHOTNUMBER);
    CHECK(cgame::SharedImport(232) == CG_GETSERVERCOMMAND);
    CHECK(cgame::SharedImport(54) == CG_R_REGISTERMODEL);
    CHECK(cgame::SharedImport(202) == CG_R_ADDREFENTITYTOSCENE && cgame::SharedImport(209) == CG_R_RENDERSCENE);
    for (int ordinal : {5, 6, 30, 39, 46, 47, 58, 227, 228, 230, 231, 234, 235, 297, 344, 350, -1, 999})
        CHECK(cgame::SharedImport(ordinal) == -1);
    CHECK(cgame::CvarFlags(0x7ff) == 0x7ff);
    CHECK(cgame::CvarFlags(0x1800) == 0 && cgame::CvarFlags(0xffffffff) == 0x7ff);
    const int counts[]{3, 0, 0, 3, 0, 0, 2, 2, 1};
    for (int operation = CG_INIT; operation <= CG_EVENT_HANDLING; ++operation) {
        CHECK(FNTech3_JKACgameExportOrdinal(operation) == operation);
        CHECK(FNTech3_JKACgameExportArgumentCount(operation) == counts[operation]);
    }
    const int unserved[]{-1, CG_CHAT_DOWN, CG_CHAT_UP, CG_COPY_CLIENT_IDENTITY,
        CG_GET_PHYSICS_TIME, CG_GET_TAG, CG_CHECKEXECKEY, CG_EXPORT_LAST, 27, 31};
    for (int operation : unserved) {
        CHECK(FNTech3_JKACgameExportOrdinal(operation) == -1);
        CHECK(FNTech3_JKACgameExportArgumentCount(operation) == -1);
    }
    CHECK(FNTech3_JKACgameExportOrdinal(CG_RAVEN_MAP_CHANGE) == 27);
    CHECK(FNTech3_JKACgameExportArgumentCount(CG_RAVEN_MAP_CHANGE) == 0);
    CHECK(FNTech3_JKACgameExportOrdinal(CG_RAVEN_TRACE) == 15);
    CHECK(FNTech3_JKACgameExportArgumentCount(CG_RAVEN_TRACE) == 0);
    for (int event = 0; event <= 3; ++event) CHECK(FNTech3_JKACgameEventToExternal(event) == event);
    CHECK(FNTech3_JKACgameEventToExternal(CGAME_EVENT_REFRESH_DISPLAY_CONTEXT) == -1);
    CHECK(FNTech3_JKACgameEventToExternal(CGAME_EVENT_CLOSECOMMANDOVERLAY) == -1);
    float mins[]{-1, -2, -3}, maxs[]{1, 2, 3};
    CHECK(client::Bounds(mins, maxs) && !client::Bounds(maxs, mins));
    CHECK(!client::FiniteVector(nullptr));
    for (uint32_t encoding : {0x7f800000u, 0xff800000u, 0x7fc00001u}) {
        std::memcpy(&mins[1], &encoding, sizeof(encoding));
        CHECK(!client::FiniteVector(mins) && !client::Bounds(mins, maxs));
    }
    return 0;
}

static int GameState() {
    gameState_t source{}; source.dataCount = 7;
    std::memcpy(source.stringData + 1, "hello", 6);
    source.stringOffsets[1699] = 1;
    fntech3_jka_mp_gameState_t output{};
    CHECK(client::GameState(source, output));
    CHECK(output.dataCount == 7 && output.stringOffsets[1699] == 1);
    CHECK(!std::strcmp(output.stringData + output.stringOffsets[1699], "hello"));
    CHECK(output.stringData[7] == 0 && output.stringOffsets[0] == 0);
    auto before = output;
    for (int bad : {-1, 16001, std::numeric_limits<int>::max()}) {
        source.dataCount = bad;
        CHECK(!client::GameState(source, output) && !std::memcmp(&output, &before, sizeof(output)));
    }
    source.dataCount = 7;
    for (int bad : {-1, 7, std::numeric_limits<int>::max()}) {
        source.stringOffsets[1699] = bad;
        CHECK(!client::GameState(source, output) && !std::memcmp(&output, &before, sizeof(output)));
    }
    source.stringOffsets[1699] = 1; source.stringData[6] = 'x';
    CHECK(!client::GameState(source, output) && !std::memcmp(&output, &before, sizeof(output)));
    source.stringData[6] = 0; source.stringOffsets[1700] = 1;
    CHECK(!client::GameState(source, output) && !std::memcmp(&output, &before, sizeof(output)));
    source.stringOffsets[1700] = 0; source.stringData[0] = 'x';
    CHECK(!client::GameState(source, output) && !std::memcmp(&output, &before, sizeof(output)));
    source = {}; source.dataCount = 16000; source.stringOffsets[1699] = 15998;
    source.stringData[15998] = 'z'; CHECK(client::GameState(source, output));
    CHECK(output.stringData[15998] == 'z' && output.dataCount == 16000);
    return 0;
}

static int Snapshot() {
    std::array<uint8_t, 32> mask; mask.fill(0xa5);
    fntech3_jka_playerState_t player{}, vehicle{};
    player.commandTime = 900; player.clientNum = 7; player.m_iVehicleNum = 99;
    vehicle.commandTime = 899; vehicle.clientNum = 9; vehicle.origin[2] = 17;
    playerState_t canonicalPlayer{}, canonicalVehicle{};
    CHECK(state::ToInternal(player, canonicalPlayer) && state::ToInternal(vehicle, canonicalVehicle));
    std::array<entityState_t, 512> entities{};
    for (unsigned index = 0; index < entities.size(); ++index) {
        entities[index].number = int(index);
        entities[index].raven.saberHolstered = 2;
        entities[index].pos.trType = TR_GRAVITY; entities[index].apos.trType = TR_SINE;
    }
    client::SnapshotView view{3, 24, 1000, 81, mask.data(), &canonicalPlayer, &canonicalVehicle,
        entities.data(), 0xfffffffeu, entities.size(), 3};
    fntech3_jka_mp_snapshot_t output{};
    CHECK(client::Snapshot(view, output));
    CHECK(output.ps.clientNum == 7 && output.ps.m_iVehicleNum == 99 && output.vps.clientNum == 9);
    CHECK(output.ps.commandTime == 900 && output.vps.commandTime == 899 && output.vps.origin[2] == 17);
    CHECK(output.snapFlags == 3 && output.ping == 24 && output.serverTime == 1000);
    CHECK(output.serverCommandSequence == 81 && output.numServerCommands == 0);
    CHECK(!std::memcmp(output.areamask, mask.data(), mask.size()));
    CHECK(output.numEntities == 3 && output.entities[0].number == 510);
    CHECK(output.entities[1].number == 511 && output.entities[2].number == 0);
    CHECK(output.entities[0].pos.trType == 6 && output.entities[0].apos.trType == 5);
    CHECK(output.entities[0].saberHolstered == 2 && output.entities[3].number == 0);
    auto before = output;
    auto reject = [&](client::SnapshotView bad) {
        return !client::Snapshot(bad, output) && !std::memcmp(&output, &before, sizeof(output));
    };
    auto bad = view; bad.areamask = nullptr; CHECK(reject(bad));
    bad = view; bad.player = nullptr; CHECK(reject(bad));
    bad = view; bad.vehicle = nullptr; CHECK(reject(bad));
    bad = view; bad.entities = nullptr; CHECK(reject(bad));
    bad = view; bad.entityCapacity = 0; CHECK(reject(bad));
    bad = view; bad.entityCapacity = 511; CHECK(reject(bad));
    bad = view; bad.entityCount = -1; CHECK(reject(bad));
    bad = view; bad.entityCount = 513; CHECK(reject(bad));
    entities[510].pos.gravity = 1; CHECK(reject(view)); entities[510].pos.gravity = 0;
    bad = view; bad.entityCount = 512; CHECK(client::Snapshot(bad, output));
    CHECK(output.numEntities == 256 && output.entities[255].number == 253);
    bad = view; bad.entityCount = 0; bad.entities = nullptr; CHECK(client::Snapshot(bad, output));
    CHECK(output.numEntities == 0 && output.entities[0].number == 0);
    return 0;
}

static int DefaultStateAndCommand() {
    entityState_t source{}; source.number = 17; source.raven.saberHolstered = 1;
    fntech3_jka_entityState_t output{}; output.number = -99; const auto before = output;
    CHECK(!client::DefaultState(source, output) && !std::memcmp(&output, &before, sizeof(output)));
    source.eFlags = 0x80; CHECK(client::DefaultState(source, output));
    CHECK(output.number == 17 && output.saberHolstered == 1 && output.eFlags == 0x80);
    fntech3_jka_usercmd_t external{}; external.serverTime = 234; external.weapon = 7;
    external.forcesel = 8; external.invensel = 9; external.generic_cmd = 10; external.rightmove = -127;
    usercmd_t command{}; CHECK(state::ToInternal(external, command));
    fntech3_jka_usercmd_t roundtrip{}; CHECK(state::FromInternal(command, roundtrip));
    CHECK(!std::memcmp(&external, &roundtrip, sizeof(external)));
    return 0;
}

int main() {
    CHECK(fntech3::jka::cgame::SharedImport(fntech3::jka::module::Call::CG_KEY_GETCATCHER) == CG_KEY_GETCATCHER);
    if (int result = Mapping()) return result;
    if (int result = GameState()) return result;
    if (int result = Snapshot()) return result;
    if (int result = DefaultStateAndCommand()) return result;
    std::puts("Jedi Academy client records and cgame import mapping checks passed");
    return 0;
}
