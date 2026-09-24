/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_Q3_IHV_CLIENT_CASES_HPP
#define FNTECH3_Q3_IHV_CLIENT_CASES_HPP
#include "../code/compat/q3_ihv_client_data.hpp"
#include "q3_ihv_configstring_cases.hpp"

namespace ihv_client_cases {
namespace wire = fntech3::q3ihv::wire;
using Digest = ihv_config_cases::Digest;
inline wire::GameState Game() {
	wire::GameState game; game.configstrings[0] = "\\protocol\\33\\mapname\\synthetic";
	game.configstrings[1] = "synthetic server";
	for (unsigned n=1;n<=12;++n) {
		wire::Store(game.baselines[n].data(),2,n); game.baselines[n][149]=2;
	}
	return game;
}
inline wire::Snapshot Frame(unsigned index,const wire::Snapshot *previous) {
	wire::Snapshot frame;
	constexpr unsigned counts[]={0,1,2,8,96,1023,1,0,12};
	constexpr std::uint32_t wrapped[]={0x7ffffffeu,0x7fffffffu,0x80000000u,0x80000001u,0xfffffffeu,0xffffffffu,0u,1u};
	frame.serverTime=0xfffff000u+index*257;
	frame.messageNumber=index<16?index+1:wrapped[(index-16)%8];
	frame.deltaNumber=previous && previous->messageNumber && !(previous->messageNumber&0x80000000u)
		&& previous->entities.size()<=896 ? previous->messageNumber : 0xffffffffu;
	frame.configSequence=0x12340000u+index; frame.commandSequence=0xfedc0000u+index;
	frame.flags=static_cast<std::uint8_t>(index*31);frame.areaBytes=static_cast<std::uint8_t>(index%33);
	for(unsigned i=0;i<frame.areaBytes;++i)frame.areaMask[i]=static_cast<std::uint8_t>(i*17+index);
	frame.player[100]=static_cast<std::uint8_t>(index*3); frame.player[132]=static_cast<std::uint8_t>(index*5);
	const auto count=counts[index%9];
	for(unsigned n=1;n<=count;++n) {
		wire::Entity entity{};wire::Store(entity.data(),2,n);entity[149]=1;
		if(count<128) {
			entity[148]=static_cast<std::uint8_t>(index*7+n);entity[149]=static_cast<std::uint8_t>(1+(index+n)%254);
			const float origin=float(int(index)*-19+int(n));std::uint32_t bits;std::memcpy(&bits,&origin,4);
			wire::Store(entity.data()+88,4,bits);wire::Store(entity.data()+144,4,index*923+n);
		}
		frame.entities.push_back(entity);
	}
	return frame;
}
inline std::uint32_t Ping(unsigned index) { return index%7==0?0xffffffffu:index*17; }
inline constexpr unsigned Frames=40;
}
#endif
