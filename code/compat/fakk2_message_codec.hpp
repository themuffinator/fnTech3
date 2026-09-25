/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_FAKK2_MESSAGE_CODEC_HPP
#define FNTECH3_FAKK2_MESSAGE_CODEC_HPP

/* Heavy Metal: F.A.K.K.2's message bodies.
 *
 * Retail fakk2.exe 1.02, the official patch's plain executable, reads them
 * with Alice's algorithm -- plain LSB-first bits, an entity delta that sends
 * a five-bit index into a thirty-one entry dictionary of changed-field
 * vectors (thirty-one: the vector follows raw), the changed values, then a
 * thirty-two byte block under its own mask; a player delta that flags each
 * field, then six thirty-two entry arrays of shorts under a mask each; floats
 * as a biased thirteen-bit integer or thirty-two raw bits. What it runs them
 * over is its own:
 *
 *   MSG_ReadDeltaEntity       0x00439e70  seventy-three fields of its
 *                                         488-byte record, its own dictionary
 *                                         (0x004e6220), the block at +0x178
 *   MSG_ReadDeltaPlayerstate  0x0043a680  Alice's forty-nine fields, but
 *                                         pm_flags and gravity in sixteen bits
 *   null entity               0x0043a220  Alice's values on F.A.K.K.2's record
 *
 * Alice's engine grew the entity record (bone controllers as records with a
 * scale, a second render-flags word) and with it the table and dictionary, so
 * a F.A.K.K.2 reader cannot decode Alice's bodies: retail fakk2.exe 1.02
 * refused an earlier fnTech3 demo that sent them ("CL_ParseGamestate: bad
 * command byte"). The tables here are generated from the executable by
 * scripts/generate_fakk2_wire_tables.py. Framing, snapshot, sound block and
 * gamestate are Alice's and stay in alice_message_codec.hpp.
 *
 * See docs/fntech3/2026-09-24-fakk2-contracts.md. */

#include "alice_message_codec.hpp"
#include "fakk2_game_abi.h"

#include <array>
#include <cstddef>

namespace fntech3::fakk2::message {

using alice::message::ChangeVector;
using alice::message::Field;

using Entity = fntech3_fakk2_entityState_t;
using Player = fntech3_fakk2_playerState_t;

inline constexpr std::size_t kEntityFields = 73;
inline constexpr std::size_t kPlayerFields = 49;

#define FNTECH3_FAKK2_ENTITY_FIELD( name, offset, bits ) Field{ offset, bits },
inline constexpr std::array<Field, kEntityFields> EntityFields{ {
#include "fakk2_wire_fields.def"
} };
#undef FNTECH3_FAKK2_ENTITY_FIELD

#define FNTECH3_FAKK2_PLAYER_FIELD( name, offset, bits ) Field{ offset, bits },
inline constexpr std::array<Field, kPlayerFields> PlayerFields{ {
#include "fakk2_wire_fields.def"
} };
#undef FNTECH3_FAKK2_PLAYER_FIELD

#define FNTECH3_FAKK2_DELTA_PATTERN( ... ) ChangeVector{ { __VA_ARGS__ } },
inline constexpr std::array<ChangeVector, 31> ChangeVectors{ {
#include "fakk2_delta_patterns.def"
} };
#undef FNTECH3_FAKK2_DELTA_PATTERN

static_assert( sizeof( Entity ) == 488, "fakk2.exe clears and copies 122 dwords" );
static_assert( offsetof( Entity, surfaces ) == 0x178, "the block MSG_ReadDeltaEntity sends by mask" );
static_assert( sizeof( Entity::surfaces ) == alice::message::kTaggedBytes, "thirty-two bytes" );

struct EntitySpec {
	using Record = Entity;
	static constexpr std::size_t FieldCount = kEntityFields;
	static constexpr const std::array<Field, FieldCount> &Fields = EntityFields;
	static constexpr const std::array<ChangeVector, 31> &Vectors = ChangeVectors;
	static const std::uint8_t *Block( const Record &record ) noexcept { return record.surfaces; }
	static std::uint8_t *Block( Record &record ) noexcept { return record.surfaces; }
};

struct PlayerSpec {
	using Record = Player;
	static constexpr std::size_t FieldCount = kPlayerFields;
	static constexpr const std::array<Field, FieldCount> &Fields = PlayerFields;
};

template<class Writer>
bool WriteEntity( Writer &io, const Entity *from, const Entity *to, bool force ) {
	return alice::message::WriteEntityAs<EntitySpec>( io, from, to, force );
}

template<class Reader>
bool ReadEntity( Reader &io, const Entity *from, Entity *to, int number ) {
	return alice::message::ReadEntityAs<EntitySpec>( io, from, to, number );
}

template<class Writer>
bool WritePlayer( Writer &io, const Player *from, const Player *to ) {
	return alice::message::WritePlayerAs<PlayerSpec>( io, from, to );
}

template<class Reader>
bool ReadPlayer( Reader &io, const Player *from, Player *to ) {
	return alice::message::ReadPlayerAs<PlayerSpec>( io, from, to );
}

/* fakk2.exe 0x0043a220: clear the 488 bytes, then constantLight 0x00ffffff
 * (+0x90), parent 1023 (+0xa0), scale and alpha 1.0 (+0x1a4, +0x1a8),
 * renderfx 0x10 (+0x1ac) and the five bone tags -1 (+0xd8..+0xe8). These are
 * Alice's values, so either title's null entity converts to the other's. */
inline void InitNullEntity( Entity &entity ) noexcept {
	entity = Entity{};
	entity.constantLight = 0x00ffffff;
	entity.parent = alice::message::kMaxGentities - 1;
	entity.scale = 1.0f;
	entity.alpha = 1.0f;
	entity.renderfx = 0x10;
	for ( auto &tag : entity.bone_tag ) tag = -1;
}

} // namespace fntech3::fakk2::message

#endif
