/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#include "../code/compat/jka_sp_player_abi.h"
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>

using Player = fntech3_jka_sp_playerState_t;
static_assert(sizeof(Player) == 4916 && alignof(Player) == 4);
static_assert(std::is_standard_layout_v<Player> && std::is_trivially_copyable_v<Player>);
#define FNTECH3_JKA_SP_PLAYER_FIELD(member, offset, bits) \
	static_assert(offsetof(Player, member) == offset); \
	static_assert(sizeof(((Player *)nullptr)->member) == 4);
#include "../code/compat/jka_sp_player_fields.def"
#undef FNTECH3_JKA_SP_PLAYER_FIELD
static_assert(offsetof(Player, stats) == 196 && sizeof(Player::stats) == 16 * 4);
static_assert(offsetof(Player, persistant) == 260 && sizeof(Player::persistant) == 16 * 4);
static_assert(offsetof(Player, powerups) == 324 && sizeof(Player::powerups) == 16 * 4);
static_assert(offsetof(Player, ammo) == 388 && sizeof(Player::ammo) == 10 * 4);
static_assert(offsetof(Player, inventory) == 428 && sizeof(Player::inventory) == 15 * 4);
static_assert(offsetof(Player, private_488) == 488 && sizeof(Player::private_488) == 120);
static_assert(offsetof(Player, private_632) == 632 && sizeof(Player::private_632) == 8);
static_assert(offsetof(Player, ping) == 640);
static_assert(offsetof(Player, private_suffix) == 644 && sizeof(Player::private_suffix) == 4272);
struct Field { std::size_t offset; int bits; };
constexpr Field fields[]{
#define FNTECH3_JKA_SP_PLAYER_FIELD(member, offset, bits) {offset, bits},
#include "../code/compat/jka_sp_player_fields.def"
#undef FNTECH3_JKA_SP_PLAYER_FIELD
};
static_assert(sizeof(fields)/sizeof(fields[0]) == FNTECH3_JKA_SP_PLAYER_NETFIELD_COUNT);

int main() {
	/* These are independent binary fixtures: offsets come from the retail
	 * table/instructions, and their reads must land on the frozen declarations. */
	std::array<std::uint8_t, 4916 + 2> storage{};
	storage.front()=0xa5; storage.back()=0x5a;
	auto *bytes=storage.data()+1; // unaligned source; native reads must use memcpy.
	for (std::size_t index=0;index<4916;++index)
		bytes[index]=static_cast<std::uint8_t>(index*17+31);
	for (std::size_t index=0;index<50;++index) {
		assert(fields[index].offset<=4916-4);
		assert(fields[index].bits>=-32 && fields[index].bits<=32);
		for (std::size_t previous=0;previous<index;++previous)
			assert(fields[previous].offset!=fields[index].offset);
	}
	Player record{};
	std::memcpy(&record,bytes,sizeof(record));
#define FNTECH3_JKA_SP_PLAYER_FIELD(member, offset, bits) \
	{ std::uint32_t fixture=0, actual=0; \
	  std::memcpy(&fixture,bytes+offset,4); std::memcpy(&actual,&record.member,4); \
	  assert(actual==fixture); }
#include "../code/compat/jka_sp_player_fields.def"
#undef FNTECH3_JKA_SP_PLAYER_FIELD
	/* Ping is not in the network field table. Its independent engine write
	 * must alter only the four measured bytes, leaving native private data intact. */
	const std::int32_t ping=999;
	record.ping=ping;
	std::array<std::uint8_t,4916> output{};
	std::memcpy(output.data(),&record,sizeof(record));
	for (std::size_t index=0;index<4916;++index) {
		if (index<640 || index>=644) assert(output[index]==bytes[index]);
	}
	std::int32_t storedPing=0;std::memcpy(&storedPing,output.data()+640,4);
	assert(storedPing==999 && storage.front()==0xa5 && storage.back()==0x5a);
	/* The last inventory/ammo entries must not alias the following private
	 * storage or another array; these counts differ from Academy multiplayer. */
	record.inventory[14]=0x12345678;record.ammo[9]=0x34567812;
	std::memcpy(output.data(),&record,sizeof(record));
	std::int32_t inventoryLast=0,ammoLast=0;
	std::memcpy(&inventoryLast,output.data()+484,4);
	std::memcpy(&ammoLast,output.data()+424,4);
	assert(inventoryLast==0x12345678 && ammoLast==0x34567812);
	assert(!std::memcmp(output.data()+488,bytes+488,120));
	assert(!std::memcmp(output.data()+644,bytes+644,4272));
	return 0;
}
