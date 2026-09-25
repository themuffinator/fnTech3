/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_Q3TEST_STATE_ADAPTER_HPP
#define FNTECH3_Q3TEST_STATE_ADAPTER_HPP

#include "../qcommon/q_shared.h"
#include "q3test_game_abi.h"
#include <cstddef>
#include <cstdint>
#include <cstring>

/* The shared server links, clips and selects snapshot entities through the
 * engine's own records, so it needs a view of each Q3Test entity. The wire
 * and the client game never see these projections: snapshots carry the
 * module's exact 204-byte states. The state's memory order is Quake III's
 * own (1.02 SV_CreateBaseline copies 51 dwords), so every member has a home
 * in the engine record and a round trip loses nothing. */
namespace fntech3::q3test::state {

inline void ToInternal(const fntech3_q3test_trajectory_t &in, trajectory_t &out) {
	out.trType = static_cast<trType_t>(in.trType);
	out.trTime = in.trTime; out.trDuration = in.trDuration;
	for (int i = 0; i < 3; ++i) { out.trBase[i] = in.trBase[i]; out.trDelta[i] = in.trDelta[i]; }
}
inline void FromInternal(const trajectory_t &in, fntech3_q3test_trajectory_t &out) {
	out.trType = static_cast<int32_t>(in.trType);
	out.trTime = in.trTime; out.trDuration = in.trDuration;
	for (int i = 0; i < 3; ++i) { out.trBase[i] = in.trBase[i]; out.trDelta[i] = in.trDelta[i]; }
}

inline bool ToInternal(const fntech3_q3test_entityState_t &in, entityState_t &out) {
	entityState_t result{};
	result.number = in.number; result.eType = in.eType; result.eFlags = in.eFlags;
	ToInternal(in.pos, result.pos); ToInternal(in.apos, result.apos);
	result.time = in.time; result.time2 = in.time2;
	for (int i = 0; i < 3; ++i) {
		result.origin[i] = in.origin[i]; result.origin2[i] = in.origin2[i];
		result.angles[i] = in.angles[i]; result.angles2[i] = in.angles2[i];
	}
	result.otherEntityNum = in.otherEntityNum; result.otherEntityNum2 = in.otherEntityNum2;
	result.groundEntityNum = in.groundEntityNum; result.constantLight = in.constantLight;
	result.loopSound = in.loopSound; result.modelindex = in.modelindex; result.modelindex2 = in.modelindex2;
	result.clientNum = in.clientNum; result.frame = in.frame; result.solid = in.solid;
	result.event = in.event; result.eventParm = in.eventParm; result.powerups = in.powerups;
	result.weapon = in.weapon; result.legsAnim = in.legsAnim; result.torsoAnim = in.torsoAnim;
	out = result;
	return true;
}

inline bool FromInternal(const entityState_t &in, fntech3_q3test_entityState_t &out) {
	fntech3_q3test_entityState_t result{};
	result.number = in.number; result.eType = in.eType; result.eFlags = in.eFlags;
	FromInternal(in.pos, result.pos); FromInternal(in.apos, result.apos);
	result.time = in.time; result.time2 = in.time2;
	for (int i = 0; i < 3; ++i) {
		result.origin[i] = in.origin[i]; result.origin2[i] = in.origin2[i];
		result.angles[i] = in.angles[i]; result.angles2[i] = in.angles2[i];
	}
	result.otherEntityNum = in.otherEntityNum; result.otherEntityNum2 = in.otherEntityNum2;
	result.groundEntityNum = in.groundEntityNum; result.constantLight = in.constantLight;
	result.loopSound = in.loopSound; result.modelindex = in.modelindex; result.modelindex2 = in.modelindex2;
	result.clientNum = in.clientNum; result.frame = in.frame; result.solid = in.solid;
	result.event = in.event; result.eventParm = in.eventParm; result.powerups = in.powerups;
	result.weapon = in.weapon; result.legsAnim = in.legsAnim; result.torsoAnim = in.torsoAnim;
	out = result;
	return true;
}

/* The engine reads only a few members of a player state: the view point
 * (origin plus the integer view height), the player's own entity, the score
 * status prints and the ping it writes. Their places differ by release
 * (1.02 SV_BuildClientSnapshot 0x41d763.., 1.05 0x41dba9.., 1.06 0x41e80e..,
 * 1.08 0x41f16d.., SV_CalcPings and the status commands). */
struct PlayerLayout {
	std::size_t bytes;
	std::size_t origin, velocity, viewangles, viewheight;
	std::size_t clientNum;   /* the player's own number; kNone when absent */
	std::size_t score;
	bool shortScore;         /* 1.02/1.03 read it as a signed short */
	std::size_t ping;
};
inline constexpr std::size_t kNone = ~std::size_t{0};

/* Offsets from the delta tables (net_messages 4.2) and the engine reads. */
inline constexpr PlayerLayout kPlayer35{ 276, 0x08, 0x14, 0x50, 0x5c, 0x44, 0x84, true, 0x110 };
inline constexpr PlayerLayout kPlayer36{ 416, 16, 28, 108, 120, 96, 0xcc, false, 0x19c };
inline constexpr PlayerLayout kPlayer38{ 432, 16, 28, 140, 152, 128, 0xec, false, 0x1ac };
inline constexpr PlayerLayout kPlayer40{ 440, 20, 32, 148, 160, 136, 0xf4, false, 0x1b4 };

inline const PlayerLayout *PlayerLayoutForProtocol(int protocol) {
	switch (protocol) {
	case 35: return &kPlayer35;
	case 36: return &kPlayer36;
	case 38: case 39: return &kPlayer38;
	case 40: return &kPlayer40;
	default: return nullptr;
	}
}

inline int32_t LoadInt(const std::uint8_t *data) { int32_t v; std::memcpy(&v, data, 4); return v; }
inline float LoadFloat(const std::uint8_t *data) { float v; std::memcpy(&v, data, 4); return v; }

/* A read-only projection of the members above; the rest stays zero. */
inline bool PlayerToInternal(const PlayerLayout &layout, const std::uint8_t *in, playerState_t &out) {
	if (!in) return false;
	playerState_t result{};
	for (int i = 0; i < 3; ++i) {
		result.origin[i] = LoadFloat(in + layout.origin + 4 * i);
		result.velocity[i] = LoadFloat(in + layout.velocity + 4 * i);
		result.viewangles[i] = LoadFloat(in + layout.viewangles + 4 * i);
	}
	result.viewheight = LoadInt(in + layout.viewheight);
	if (layout.clientNum != kNone) result.clientNum = LoadInt(in + layout.clientNum);
	if (layout.shortScore) {
		int16_t score; std::memcpy(&score, in + layout.score, 2); result.persistant[0] = score;
	} else {
		result.persistant[0] = LoadInt(in + layout.score);
	}
	result.ping = LoadInt(in + layout.ping);
	out = result;
	return true;
}

} // namespace fntech3::q3test::state
#endif
