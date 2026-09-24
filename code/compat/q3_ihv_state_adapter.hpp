/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_Q3_IHV_STATE_ADAPTER_HPP
#define FNTECH3_Q3_IHV_STATE_ADAPTER_HPP

#include "../qcommon/q_shared.h"
#include "q3_ihv_game_abi.h"
#include <cstdint>
#include <cstring>

/* The shared server needs a view of the native entity for spatial operations.
 * Native snapshots retain the exact records through the raw overloads in
 * ServerGameDataView; these transfers never substitute a retail wire layout.
 * Original field offsets are pinned by the IHV codec/reference observations.
 * Trajectory words remain opaque here: IHV game/cgame own their evaluation. */
namespace fntech3::q3ihv::state {
struct IntegerField { unsigned offset, bytes; int entityState_t::*member; bool signedValue=false; };
inline constexpr IntegerField EntityIntegers[] = {
	{0,2,&entityState_t::number,true}, {2,1,&entityState_t::eType}, {4,2,&entityState_t::eFlags},
	{80,4,&entityState_t::time,true}, {84,4,&entityState_t::time2,true},
	{136,2,&entityState_t::otherEntityNum,true}, {138,2,&entityState_t::otherEntityNum2,true},
	{140,2,&entityState_t::groundEntityNum,true}, {144,4,&entityState_t::constantLight,true},
	{148,1,&entityState_t::loopSound}, {149,1,&entityState_t::modelindex}, {150,1,&entityState_t::modelindex2},
	{151,1,&entityState_t::clientNum}, {152,2,&entityState_t::frame}, {154,2,&entityState_t::solid},
	{156,1,&entityState_t::event}, {157,1,&entityState_t::eventParm}, {158,2,&entityState_t::powerups},
	{160,1,&entityState_t::weapon}, {161,1,&entityState_t::legsAnim}, {162,1,&entityState_t::torsoAnim}
};
inline int ReadInteger(const std::uint8_t *data,unsigned bytes,bool signedValue) {
	std::uint32_t bits=0;for(unsigned i=0;i<bytes;++i)bits|=std::uint32_t(data[i])<<(8*i);
	if(bytes==2&&signedValue&&(bits&0x8000u))bits|=0xffff0000u;
	std::int32_t value;std::memcpy(&value,&bits,4);return value;
}
inline void ReadWords(const std::uint8_t *bytes,void *destination,unsigned count) {
	auto *out=static_cast<std::uint8_t *>(destination);
	for(unsigned i=0;i<count;++i) { const auto value=ReadInteger(bytes+i*4,4,true);std::memcpy(out+i*4,&value,4); }
}
inline void WriteWords(const void *source,std::uint8_t *bytes,unsigned count) {
	const auto *in=static_cast<const std::uint8_t *>(source);
	for(unsigned i=0;i<count;++i) {
		std::uint32_t bits;std::memcpy(&bits,in+i*4,4);
		for(unsigned j=0;j<4;++j)bytes[i*4+j]=static_cast<std::uint8_t>(bits>>(8*j));
	}
}
inline void ReadTrajectory(const std::uint8_t *bytes,trajectory_t &out) {
	ReadWords(bytes,&out.trType,1);ReadWords(bytes+4,&out.trTime,1);
	ReadWords(bytes+8,&out.trDuration,1);ReadWords(bytes+12,out.trBase,3);
	ReadWords(bytes+24,out.trDelta,3);
}
inline void WriteTrajectory(const trajectory_t &in,std::uint8_t *bytes) {
	WriteWords(&in.trType,bytes,1);WriteWords(&in.trTime,bytes+4,1);
	WriteWords(&in.trDuration,bytes+8,1);WriteWords(in.trBase,bytes+12,3);
	WriteWords(in.trDelta,bytes+24,3);
}
inline bool ToInternal(const fntech3_q3_ihv_entityState_t &in,entityState_t &out) {
	entityState_t result{};
	for(const auto &field:EntityIntegers) result.*field.member=ReadInteger(in.bytes+field.offset,field.bytes,field.signedValue);
	ReadTrajectory(in.bytes+8,result.pos);ReadTrajectory(in.bytes+44,result.apos);
	ReadWords(in.bytes+88,result.origin,3);ReadWords(in.bytes+100,result.origin2,3);
	ReadWords(in.bytes+112,result.angles,3);ReadWords(in.bytes+124,result.angles2,3);
	out=result;return true;
}
/* Merge into the previous exact native state: padding is module-owned too.
 * Validate every narrowing conversion before publishing a single byte. */
inline bool FromInternal(const entityState_t &in,fntech3_q3_ihv_entityState_t &out) {
	auto result=out;
	for(const auto &field:EntityIntegers) {
		const int value=in.*field.member;
		if(field.bytes<4) {
			const int maximum=field.signedValue?32767:field.bytes==1?255:65535;
			const int minimum=field.signedValue?-32768:0;
			if(value<minimum||value>maximum)return false;
		}
		const auto bits=static_cast<std::uint32_t>(value);
		for(unsigned i=0;i<field.bytes;++i)result.bytes[field.offset+i]=static_cast<std::uint8_t>(bits>>(8*i));
	}
	WriteTrajectory(in.pos,result.bytes+8);WriteTrajectory(in.apos,result.bytes+44);
	WriteWords(in.origin,result.bytes+88,3);WriteWords(in.origin2,result.bytes+100,3);
	WriteWords(in.angles,result.bytes+112,3);WriteWords(in.angles2,result.bytes+124,3);
	out=result;return true;
}
} // namespace fntech3::q3ihv::state
#endif
