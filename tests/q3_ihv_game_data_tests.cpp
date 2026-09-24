/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#include "../code/server/server_game_data.h"
#include "../code/compat/q3_ihv_state_adapter.hpp"
#include "../code/compat/q3_ihv_native_boundary.hpp"
#include "../code/compat/q3_game_abi.h"
#include "../code/qcommon/q3_ihv_wire.hpp"
#include <array>
#include <climits>
#include <cstdio>
#include <vector>

using namespace fntech3::server;
namespace native=fntech3::q3ihv;
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"%d: %s\n",__LINE__,#x);return false;}}while(0)

static bool States() {
	// All native bytes, including padding and float payloads, survive a shared
	// engine read/write. The native state, not that view, goes into protocol 33.
	for(unsigned seed=0;seed<257;++seed) {
		fntech3_q3_ihv_entityState_t original{};
		for(unsigned i=0;i<sizeof(original);++i)original.bytes[i]=static_cast<std::uint8_t>(i*73+seed*37);
		entityState_t shared;std::memset(&shared,0xa5,sizeof(shared));
		CHECK(native::state::ToInternal(original,shared));
		CHECK(!shared.health&&!shared.armor&&!shared.pos.gravity&&!shared.apos.gravity);
		auto written=original;CHECK(native::state::FromInternal(shared,written));
		CHECK(!std::memcmp(&written,&original,sizeof(original)));
		shared.number=1023;shared.modelindex=255;shared.solid=65535;
		CHECK(native::state::FromInternal(shared,written));
		CHECK(written.bytes[0]==255&&written.bytes[1]==3&&written.bytes[2]==original.bytes[2]);
		CHECK(written.bytes[149]==255&&written.bytes[154]==255&&written.bytes[155]==255);
		for(unsigned offset:{3,6,7,142,143,163})CHECK(written.bytes[offset]==original.bytes[offset]);
		for(const auto &field:native::state::EntityIntegers)if(field.bytes<4) {
			const auto before=written;const int previous=shared.*field.member;
			for(int invalid:{field.signedValue?-32769:-1,field.signedValue?32768:field.bytes==1?256:65536}) {
				shared.*field.member=invalid;CHECK(!native::state::FromInternal(shared,written));
				CHECK(!std::memcmp(&written,&before,sizeof(before)));
			}
			shared.*field.member=previous;
		}
	}
	return true;
}
struct Clients {
	native::TaggedMemory memory{1024*1024}; int count=8; bool enabled=true; int calls=0;
	static bool Validate(void *context,const void *record,std::int32_t index,std::size_t bytes) {
		auto &self=*static_cast<Clients *>(context);++self.calls;
		return self.enabled&&self.memory.OwnsRecord(record,index,self.count,FNTECH3_Q3_IHV_GAME_CLIENT_BYTES,bytes);
	}
};
static bool Ownership() {
	Clients clients;
	auto *base=static_cast<unsigned char *>(clients.memory.Allocate(clients.count*1464,765));
	CHECK(base);
	for(int index=0;index<clients.count;++index) {
		CHECK(Clients::Validate(&clients,base+index*1464,index,276));
		CHECK(!Clients::Validate(&clients,base+index*1464+1,index,276));
		CHECK(!Clients::Validate(&clients,base+index*1464,(index+1)%clients.count,276));
		CHECK(!Clients::Validate(&clients,base+index*1464,index,1465));
	}
	for(int index:{-1,clients.count,INT_MAX})CHECK(!Clients::Validate(&clients,base,index,276));
	CHECK(!clients.memory.OwnsRecord(base,0,INT_MAX,SIZE_MAX,276));
	CHECK(!clients.memory.OwnsRecord(base,0,8,0,0));
	CHECK(clients.memory.Free(base));CHECK(!Clients::Validate(&clients,base,0,276));
	return true;
}
static bool Binding() {
	constexpr int capacity=16,count=9,stride=556;
	std::vector<std::uint32_t> arena((capacity*stride+32)/4,0xa5a5a5a5u);
	auto *base=reinterpret_cast<unsigned char *>(arena.data())+16;
	std::memset(base,0,capacity*stride);
	Clients clients;ServerGameDataView view;NativeGameDataBinding binding;
	binding.entityBase=base;binding.entitySpanBytes=capacity*stride;binding.entityCount=count;
	binding.entityCapacity=capacity;binding.entityStride=stride;binding.clientCount=clients.count;
	binding.validateClientRecord=&Clients::Validate;binding.clientValidationContext=&clients;
	CHECK(view.EntityPrefixBytesForProfile(COM_GAME_QUAKE3_IHV)==268);
	CHECK(view.PlayerStateBytesForProfile(COM_GAME_QUAKE3_IHV)==276);
	if(sizeof(void*)!=4) {
		CHECK(view.BindNative(COM_GAME_QUAKE3_IHV,binding)==GameDataBindError::UnsupportedProfile);
		std::puts("Native IHV game data correctly restricted to a 32-bit host");return true;
	}
	CHECK(view.BindNative(COM_GAME_QUAKE3_IHV,binding)==GameDataBindError::None);
	CHECK(view.EntityCount()==9&&view.ClientCount()==8);
	playerState_t genericPlayer;std::memset(&genericPlayer,0xa5,sizeof(genericPlayer));const auto oldGeneric=genericPlayer;
	CHECK(!view.ReadPlayerState(0,genericPlayer)&&!std::memcmp(&genericPlayer,&oldGeneric,sizeof(genericPlayer)));
	const auto entity=[&](int index){return reinterpret_cast<fntech3_q3_ihv_entity_t *>(base+index*stride);};
	GameEntityRef world,first,last;CHECK(view.EntityRefAt(0,world)&&view.EntityRefAt(1,first)&&view.EntityRefAt(8,last));
	fntech3_q3_ihv_playerState_t player;std::memset(&player,0xa5,sizeof(player));const auto oldPlayer=player;
	CHECK(!view.ClientPointer(0)&&!view.ReadPlayerState(0,player)&&!std::memcmp(&player,&oldPlayer,sizeof(player)));
	CHECK(!clients.calls); // Unpublished/null pointers never reach validation.
	auto *records=static_cast<unsigned char *>(clients.memory.Allocate(8*1464,765));CHECK(records);
	for(int i=0;i<8;++i) {
		entity(i+1)->client=static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(records+i*1464));
		for(int j=0;j<1464;++j)records[i*1464+j]=static_cast<unsigned char>(i*37+j);
		CHECK(view.ClientPointer(i)==records+i*1464&&view.ReadPlayerState(i,player));
		CHECK(!std::memcmp(player.bytes,records+i*1464,276));
		std::array<unsigned char,1464> before;std::memcpy(before.data(),records+i*1464,1464);
		CHECK(view.SetPlayerPing(i,999));int ping=-1;CHECK(view.GetPlayerPing(i,ping)&&ping==999);
		std::int32_t expected=999;std::memcpy(before.data()+272,&expected,4);
		CHECK(!std::memcmp(before.data(),records+i*1464,1464));
	}
	CHECK(!view.ClientPointer(-1)&&!view.ClientPointer(8));
	// Wrong-slot, interior, unowned and one-past pointers fail before a read.
	const auto pointer=entity(1)->client;
	for(auto invalid:{pointer+1,pointer+4,entity(2)->client,pointer+8*1464,1u,0xfffffffcu}) {
		entity(1)->client=invalid;player=oldPlayer;
		CHECK(!view.ClientPointer(0)&&!view.ReadPlayerState(0,player)&&!std::memcmp(&player,&oldPlayer,sizeof(player)));
		CHECK(!view.SetPlayerPing(0,777));
	}
	entity(1)->client=pointer;
	// Number is a signed word; writes cannot overwrite eType or flags.
	entity(1)->state[2]=73;entity(1)->state[3]=91;
	CHECK(view.SetNumber(first,-1));int number=0;CHECK(view.GetNumber(first,number)&&number==-1);
	CHECK(entity(1)->state[2]==73&&entity(1)->state[3]==91);
	CHECK(!view.SetNumber(first,32768)&&!view.SetNumber(first,-32769));
	CHECK(view.SetNumber(first,1));entity(1)->inuse=1;CHECK(view.GetInuse(first,number)&&number==1);
	CHECK(view.SetOwnerNum(first,8)&&entity(1)->owner==static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(entity(8))));
	CHECK(view.GetOwnerNum(first,number)&&number==8);CHECK(view.SetOwnerNum(first,0));
	CHECK(view.GetOwnerNum(first,number)&&number==0);CHECK(view.SetOwnerNum(first,-1)&&!entity(1)->owner);
	entity(1)->owner=static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(base+1));number=97;
	CHECK(!view.GetOwnerNum(first,number)&&number==97);entity(1)->owner=0;
	fntech3_q3_ihv_entityState_t exact{};for(unsigned i=0;i<sizeof(exact);++i)exact.bytes[i]=static_cast<unsigned char>(i*73);
	CHECK(view.WriteEntityState(last,exact));entityState_t canonical{};CHECK(view.ReadEntityState(last,canonical));
	const auto beforeArena=arena;CHECK(view.WriteEntityState(last,canonical)&&arena==beforeArena);
	canonical.solid=65536;CHECK(!view.WriteEntityState(last,canonical)&&arena==beforeArena);
	fntech3_rtcw_entityState_t foreign{};CHECK(!view.ReadEntityState(last,foreign)&&!view.WriteEntityState(last,foreign));
	// Revoking ownership or freeing the array makes the pointer unreadable.
	clients.enabled=false;CHECK(!view.ReadPlayerState(0,player));clients.enabled=true;
	CHECK(clients.memory.Free(records));CHECK(!view.ReadPlayerState(0,player));
	auto invalid=binding;invalid.validateClientRecord=nullptr;
	CHECK(view.BindNative(COM_GAME_QUAKE3_IHV,invalid)==GameDataBindError::MissingClientValidation);
	CHECK(!view.IsBound()&&!view.IsCurrent(last)&&!view.ClientPointer(0));
	invalid=binding;invalid.clientCount=capacity;CHECK(view.BindNative(COM_GAME_QUAKE3_IHV,invalid)==GameDataBindError::InvalidClientCount);
	invalid=binding;invalid.entitySpanBytes--;CHECK(view.BindNative(COM_GAME_QUAKE3_IHV,invalid)==GameDataBindError::EntitySpanTooSmall);
	CHECK(view.BindNative(COM_GAME_QUAKE3_IHV,binding)==GameDataBindError::None);view.Invalidate();
	CHECK(!view.IsCurrent(last)&&!view.ReadPlayerState(0,player));
	for(int i=0;i<4;++i)CHECK(arena[i]==0xa5a5a5a5u&&arena[arena.size()-1-i]==0xa5a5a5a5u);
	fntech3_q3_sharedEntity_t q3Entity{};fntech3_q3_playerState_t q3Player{};
	NativeGameDataBinding q3Binding;
	q3Binding.entityBase=&q3Entity;q3Binding.entitySpanBytes=sizeof(q3Entity);q3Binding.entityCount=1;q3Binding.entityStride=sizeof(q3Entity);
	q3Binding.clientBase=&q3Player;q3Binding.clientSpanBytes=sizeof(q3Player);q3Binding.clientCount=1;q3Binding.clientStride=sizeof(q3Player);
	CHECK(view.BindNative(COM_GAME_QUAKE3,q3Binding)==GameDataBindError::None);
	CHECK(view.EntityRefAt(0,world));const auto beforeState=exact;
	CHECK(!view.ReadEntityState(world,exact)&&!view.WriteEntityState(world,exact)&&!std::memcmp(&exact,&beforeState,sizeof(exact)));
	CHECK(!view.ReadPlayerState(0,player)&&!view.GetInuse(world,number));
	return true;
}
static bool LastEntityOwner() {
	if(sizeof(void*)!=4)return true;
	constexpr int capacity=1024,stride=556;
	std::vector<std::uint32_t> arena(capacity*stride/4);
	ServerGameDataView view;NativeGameDataBinding binding;
	binding.entityBase=arena.data();binding.entitySpanBytes=capacity*stride;
	binding.entityCount=capacity;binding.entityStride=stride;binding.clientCount=1;
	binding.validateClientRecord=[](void*,const void*,std::int32_t,std::size_t){return false;};
	CHECK(view.BindNative(COM_GAME_QUAKE3_IHV,binding)==GameDataBindError::None);
	GameEntityRef first,last;CHECK(view.EntityRefAt(1,first)&&view.EntityRefAt(1023,last));
	int owner=17;CHECK(view.GetOwnerNum(first,owner)&&owner==-1);
	CHECK(view.SetOwnerNum(first,1023)&&view.GetOwnerNum(first,owner)&&owner==1023);
	auto expected=arena;std::uint32_t zero=0;
	std::memcpy(reinterpret_cast<unsigned char*>(expected.data())+stride+264,&zero,4);
	CHECK(view.SetOwnerNum(first,-1)&&view.GetOwnerNum(first,owner)&&owner==-1&&arena==expected);
	CHECK(view.SetOwnerNum(first,0)&&view.GetOwnerNum(first,owner)&&owner==0);
	CHECK(!view.SetOwnerNum(first,-2)&&!view.SetOwnerNum(first,1024));
	CHECK(view.SetOwnerNum(last,1023)&&view.GetOwnerNum(last,owner)&&owner==1023);
	CHECK(view.SetOwnerNum(last,-1)&&view.GetOwnerNum(last,owner)&&owner==-1);
	return true;
}
int main(){if(!States()||!Ownership()||!Binding()||!LastEntityOwner())return 1;
	std::puts("IHV state transfers, native pointer ownership and server game-data boundary passed");}
