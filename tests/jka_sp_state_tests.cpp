/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#include "../code/compat/jka_sp_state_adapter.hpp"
#include "../code/server/server_game_data.h"
#include <cassert>
#include <cstring>
#include <vector>
#include <limits>

int main() {
	using fntech3::jka::sp::state::ToInternal;
	using fntech3::jka::sp::state::FromInternal;
	fntech3_jka_sp_entityState_t external{};
	for (std::size_t index=0; index<sizeof(external); ++index)
		reinterpret_cast<unsigned char *>(&external)[index]=static_cast<unsigned char>(index*7+3);
	for (int type=0; type<7; ++type) {
		external.pos.trType=type;external.apos.trType=6-type;
		entityState_t internal{};assert(ToInternal(external,internal));
		fntech3_jka_sp_entityState_t back{};assert(FromInternal(internal,back));
		assert(!std::memcmp(&external,&back,sizeof(external)));
		assert(internal.raven.sp.modelindex3==external.modelindex3);
		assert(internal.raven.sp.saberActive==external.saberActive);
	}
	entityState_t destination{};std::memset(&destination,0x51,sizeof(destination));
	const auto unchanged=destination;external.pos.trType=-1;
	assert(!ToInternal(external,destination)&&!std::memcmp(&destination,&unchanged,sizeof(destination)));
	external.pos.trType=6;external.apos.trType=0;assert(ToInternal(external,destination));
	assert(destination.pos.trType==TR_GRAVITY&&destination.apos.trType==TR_STATIONARY);
	fntech3_jka_sp_entityState_t output{};std::memset(&output,0x62,sizeof(output));const auto untouched=output;
	destination.pos.gravity=1;assert(!FromInternal(destination,output)&&!std::memcmp(&output,&untouched,sizeof(output)));
	destination.pos.gravity=0;const uint32_t invalid=0xffffffffu;std::memcpy(&destination.pos.trType,&invalid,4);
	assert(!FromInternal(destination,output)&&!std::memcmp(&output,&untouched,sizeof(output)));

	using namespace fntech3::server;
	constexpr std::size_t stride=1252, capacity=1024, guard=32;
	std::vector<uint32_t> storage((stride*capacity+guard*2)/4,0);
	auto *bytes=reinterpret_cast<unsigned char *>(storage.data());auto *base=bytes+guard;
	std::memset(bytes,0xa5,guard);std::memset(base+stride*capacity,0xa5,guard);
	auto *record=reinterpret_cast<fntech3_jka_sp_gentity_prefix_t *>(base+stride);
	external.pos.trType=6;external.apos.trType=0;record->s=external;record->s.number=1;
	record->inuse=1;record->svFlags=-1;record->owner_address=static_cast<uint32_t>(reinterpret_cast<uintptr_t>(base+stride*2));
	std::memset(base+stride+sizeof(*record),0x7b,stride-sizeof(*record));
	NativeGameDataBinding binding;
	binding.entityBase=base;binding.entityCount=4;binding.entityCapacity=capacity;
	binding.entityStride=stride;binding.entitySpanBytes=stride*capacity;binding.clientCount=1;
	ServerGameDataView view;assert(view.BindNative(COM_GAME_JKA_SP,binding)==GameDataBindError::None);
	assert(view.PlayerStateBytesForProfile(COM_GAME_JKA_SP)==0&&!view.ClientPointer(0));
	GameEntityRef ref;assert(view.EntityRefAt(1,ref));entityState_t canonical{};
	assert(view.ReadEntityState(ref,canonical)&&canonical.number==1);
	assert(view.WriteEntityState(ref,canonical));
	external.number=1;assert(!std::memcmp(&record->s,&external,sizeof(external)));
	int value;assert(view.GetInuse(ref,value)&&value==1);assert(view.GetOwnerNum(ref,value)&&value==2);
	record->owner_address=static_cast<uint32_t>(reinterpret_cast<uintptr_t>(base+1));assert(!view.GetOwnerNum(ref,value));
	record->owner_address=0;assert(view.GetOwnerNum(ref,value)&&value==ENTITYNUM_NONE);
	assert(view.SetOwnerNum(ref,2)&&record->owner_address==static_cast<uint32_t>(reinterpret_cast<uintptr_t>(base+stride*2)));
	assert(!view.SetOwnerNum(ref,1024));
	assert(view.GetSvFlags(ref,value)&&value==(1|8|32|64|128));
	assert(view.SetSvFlags(ref,0)&&record->svFlags==~(1|8|32|64|128));
	assert(view.SetLinked(ref,1)&&record->linked==1);assert(view.SetLinkcount(ref,123)&&view.GetLinkcount(ref,value)&&value==123);
	playerState_t player{};std::memset(&player,0x6a,sizeof(player));const auto playerBefore=player;
	assert(!view.ReadPlayerState(0,player)&&!std::memcmp(&player,&playerBefore,sizeof(player)));
	assert(!view.GetPlayerPing(0,value));
	GameEntityRef high;assert(view.EntityRefAt(1023,high));assert(!view.EntityRefAt(1024,high));
	assert(!view.EntityRefForPointer(base+1,high));assert(!view.EntityRefForPointer(base+stride*capacity,high));
	for(std::size_t i=sizeof(*record); i<stride; ++i)assert(base[stride+i]==0x7b);
	for(std::size_t i=0;i<guard;++i)assert(bytes[i]==0xa5&&base[stride*capacity+i]==0xa5);
	binding.entitySpanBytes--;assert(view.BindNative(COM_GAME_JKA_SP,binding)==GameDataBindError::EntitySpanTooSmall);
	assert(!view.IsBound()&&!view.IsCurrent(ref));
	return 0;
}
