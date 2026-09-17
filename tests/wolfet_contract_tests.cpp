/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#include "../code/compat/wolfet_state_adapter.hpp"
#include "../code/compat/wolfet_module_abi.h"
#include "../code/compat/wolfet_message_codec.hpp"
#include "../code/compat/wolfet_tags.hpp"
#include "../code/compat/q3_game_abi.h"
#include "../code/server/server_game_data.h"
#include "../code/qcommon/protocol_contract.hpp"
#include <cassert>
#include <cstring>
#include <random>
#include <vector>

struct Bits {
    std::vector<unsigned char> values;
    size_t position = 0, limit = 0;
    bool good = true;
    bool Good() const { return good; }
    bool Fail() { good = false; return false; }
    void Write(uint32_t value, int width) {
        for (int i = 0; i < (width < 0 ? -width : width); ++i) values.push_back((value >> i) & 1);
        limit = values.size();
    }
    uint32_t Read(int width) {
        const unsigned count = width < 0 ? -width : width;
        if (!good || count > limit-position) { Fail(); return 0; }
        uint32_t word = 0;
        for (unsigned i = 0; i < count; ++i) word |= uint32_t(values[position++]) << i;
        if (width < 0 && count < 32 && (word & (1u << (count-1)))) word |= ~((1u << count)-1);
        return word;
    }
};

int main() {
    using namespace fntech3::wolfet;
    static_assert(sizeof(fntech3_wolfet_pc_token_t) == 1048);
    std::vector<uint8_t> tagBytes(128);
    const auto tagWord = [&tagBytes](size_t at, uint32_t value) {
        for (int i = 0; i < 4; ++i) tagBytes[at+i] = uint8_t(value >> (8*i));
    };
    tagWord(0,0x31474154); tagWord(4,1); tagWord(8,1); tagWord(12,128);
    std::memcpy(tagBytes.data()+16,"tag_turret",11);
    tagWord(80,0x3f800000);
    std::vector<tags::Tag> tags;
    assert(tags::Parse(tagBytes.data(),tagBytes.size(),tags));
    assert(tags.size() == 1 && tags[0].transform[0] == 1.0f);
    tagWord(0,0x33474154); assert(tags::Parse(tagBytes.data(),tagBytes.size(),tags));
    for (size_t length = 0; length < tagBytes.size(); ++length) {
        assert(!tags::Parse(tagBytes.data(),length,tags));
        assert(tags.size() == 1 && tags[0].transform[0] == 1.0f);
    }
    for (auto offset : {0,4,8,12}) {
        auto invalid = tagBytes; invalid[offset+3] = 0xff;
        assert(!tags::Parse(invalid.data(),invalid.size(),tags));
    }
    tagWord(80,0x7f800000); assert(!tags::Parse(tagBytes.data(),tagBytes.size(),tags));
    tagWord(80,0x3f800000); std::memset(tagBytes.data()+16,'x',64);
    assert(!tags::Parse(tagBytes.data(),tagBytes.size(),tags));
    static_assert(sizeof(fntech3_wolfet_usercmd_t) == 28);
    static_assert(sizeof(fntech3_wolfet_entityState_t) == 288);
    static_assert(sizeof(fntech3_wolfet_playerState_t) == 1452);
    static_assert(offsetof(fntech3_wolfet_sharedEntity_t,r.linked) == 288);
    static_assert(offsetof(fntech3_rtcw_mp_sharedEntity_t,r.linked) == 576);
    static_assert(fnql::protocol::Find(84) == &fnql::protocol::WolfET);
    static_assert(Netchan_SelectWireProfile(84,0) == NETCHAN_WIRE_WOLFET);
    static_assert(Netchan_MessageLimit(NETCHAN_WIRE_WOLFET) == 32768);
    static_assert(Netchan_ReliableCommandLimit(NETCHAN_WIRE_WOLFET) == 256);
    for (auto descriptor : module::descriptors) {
        assert(module::Lookup(descriptor.module, descriptor.direction, descriptor.ordinal) == descriptor.call);
        assert(module::Describe(descriptor.call)->ordinal == descriptor.ordinal);
    }
    assert(module::Lookup(module::Module::GAME,module::Direction::IMPORT,-1) == module::Call::Invalid);
    assert(module::Lookup(module::Module::GAME,module::Direction::IMPORT,1024) == module::Call::Invalid);
    assert(module::Lookup(static_cast<module::Module>(999),module::Direction::IMPORT,0) == module::Call::Invalid);
    assert(FNTECH3_WOLFET_BOTAI_START_FRAME == 11);
    std::mt19937 rng(1873030);
    for (int iteration = 0; iteration < 64; ++iteration) {
        fntech3_wolfet_playerState_t player{}, returned{};
        auto *bytes = reinterpret_cast<unsigned char *>(&player);
        for (size_t i = 0; i < sizeof(player); ++i) bytes[i] = static_cast<unsigned char>(rng());
        player.serverCursorHintTrace.allsolid = 0; player.serverCursorHintTrace.startsolid = 1;
        playerState_t internal{};
        assert(state::ToInternal(player,internal)); assert(state::FromInternal(internal,returned));
        assert(!std::memcmp(&player,&returned,sizeof(player)));
        fntech3_wolfet_entityState_t entity{}, entityOut{};
        bytes = reinterpret_cast<unsigned char *>(&entity);
        for (size_t i = 0; i < sizeof(entity); ++i) bytes[i] = static_cast<unsigned char>(rng());
        entity.pos.trType = iteration % 14; entity.apos.trType = (iteration+3) % 14;
        entityState_t canonical{};
        assert(state::ToInternal(entity,canonical)); assert(state::FromInternal(canonical,entityOut));
        assert(!std::memcmp(&entity,&entityOut,sizeof(entity)));
        auto before = canonical;
        entity.pos.trType = 14;
        assert(!state::ToInternal(entity,canonical)); assert(!std::memcmp(&before,&canonical,sizeof(before)));
    }
    fntech3_wolfet_usercmd_t command{}, commandOut{};
    command.serverTime = 123456; command.angles[0] = 65535; command.forwardmove = -127;
    command.wbuttons = 128; command.flags = 255; command.doubleTap = 7; command.identClient = 255;
    usercmd_t internalCommand{};
    assert(state::ToInternal(command,internalCommand)); assert(state::FromInternal(internalCommand,commandOut));
    assert(!std::memcmp(&command,&commandOut,sizeof(command)));
    command.doubleTap = 8; const auto saved = internalCommand;
    assert(!state::ToInternal(command,internalCommand)); assert(!std::memcmp(&saved,&internalCommand,sizeof(saved)));

    fntech3_wolfet_playerState_t player{};
    player.commandTime = 700; player.identifyClientHealth = 97; player.nextWeapon = 41;
    player.stats[15] = -100; player.ammo[63] = 32000; player.ammoclip[63] = -1;
    player.viewangles[2] = 1.25f; player.weapons[1] = static_cast<int32_t>(0x80000000u);
    Bits encoded; assert(message::WritePlayer(encoded,nullptr,&player));
    fntech3_wolfet_playerState_t decoded{};
    assert(message::ReadPlayer(encoded,nullptr,&decoded)); assert(!std::memcmp(&player,&decoded,sizeof(player)));
    for (size_t length = 0; length < encoded.values.size(); ++length) {
        auto shortMessage = encoded; shortMessage.position = 0; shortMessage.limit = length;
        std::memset(&decoded,0x5a,sizeof(decoded)); const auto previous = decoded;
        assert(!message::ReadPlayer(shortMessage,nullptr,&decoded));
        assert(!std::memcmp(&previous,&decoded,sizeof(decoded)));
    }
    Bits malformed; malformed.Write(255,8);
    assert(!message::ReadPlayer(malformed,nullptr,&decoded));
    fntech3_wolfet_entityState_t invalid{}; invalid.number = 1023;
    Bits badEntity; assert(!message::WriteEntity(badEntity,nullptr,&invalid,true));

    using namespace fntech3::server;
    struct EntitySlot { fntech3_wolfet_sharedEntity_t prefix; uint32_t tail[8]; } entities[2]{};
    struct ClientSlot { fntech3_wolfet_playerState_t prefix; uint32_t tail[8]; } clients[2]{};
    for (auto &slot : entities) for (auto &word : slot.tail) word = 0x12345678;
    for (auto &slot : clients) for (auto &word : slot.tail) word = 0x87654321;
    entities[1].prefix.s.number = 1; entities[1].prefix.s.nextWeapon = 42;
    entities[1].prefix.r.linked = 1; entities[1].prefix.r.svFlags = 0x200;
    entities[1].prefix.r.snapshotCallback = 1;
    clients[1].prefix = player;
    NativeGameDataBinding binding{entities,sizeof(entities),2,sizeof(EntitySlot),clients,sizeof(clients),2,sizeof(ClientSlot)};
    ServerGameDataView view; assert(view.BindNative(COM_GAME_WOLFET,binding) == GameDataBindError::None);
    GameEntityRef reference; assert(view.EntityRefAt(1,reference));
    entityState_t entity{}; assert(view.ReadEntityState(reference,entity)); assert(entity.wolfet.nextWeapon == 42);
    int32_t linked; assert(view.GetLinked(reference,linked) && linked == 1);
    int32_t callback = -1;
    assert(view.GetWolfETSnapshotCallback(reference,callback) && callback == 1);
    assert(view.SetLinked(reference,0)); assert(entities[1].prefix.r.linked == 0);
    assert(view.SetPlayerPing(1,70)); assert(clients[1].prefix.ping == 70);
    playerState_t client{}; assert(view.ReadPlayerState(1,client)); assert(client.wolfet.identifyClientHealth == 97);
    fntech3_rtcw_entityState_t wrong{}; assert(!view.ReadEntityState(reference,wrong));
    assert(!view.EntityRefAt(2,reference)); assert(!view.ReadPlayerState(-1,client));
    binding.entityStride = sizeof(fntech3_wolfet_sharedEntity_t)-4;
    assert(view.BindNative(COM_GAME_WOLFET,binding) == GameDataBindError::EntityStrideTooSmall);
    assert(!view.IsBound() && !view.IsCurrent(reference));
    callback = 42;
    assert(!view.GetWolfETSnapshotCallback(reference,callback) && callback == 42);
    fntech3_q3_sharedEntity_t q3Entities[2]{};
    fntech3_q3_playerState_t q3Clients[2]{};
    NativeGameDataBinding q3Binding{q3Entities,sizeof(q3Entities),2,sizeof(q3Entities[0]),
        q3Clients,sizeof(q3Clients),2,sizeof(q3Clients[0])};
    assert(view.BindNative(COM_GAME_QUAKE3,q3Binding) == GameDataBindError::None);
    assert(view.EntityRefAt(1,reference));
    assert(!view.GetWolfETSnapshotCallback(reference,callback) && callback == 42);
    for (auto &slot : entities) for (auto word : slot.tail) assert(word == 0x12345678);
    for (auto &slot : clients) for (auto word : slot.tail) assert(word == 0x87654321);
}
