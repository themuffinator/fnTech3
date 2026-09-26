/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_RTCW_STATE_ADAPTER_TESTS_HPP
#define FNTECH3_RTCW_STATE_ADAPTER_TESTS_HPP

#include "../code/compat/rtcw_state_adapter.hpp"
#include <cstdio>
#include <initializer_list>

namespace rtcwStateTests {
namespace state = fntech3::rtcw::state;

#define RTCW_STATE_CHECK(expression) do { if (!(expression)) { \
	std::fprintf(stderr, "RTCW state adapter line %d: %s\n", __LINE__, #expression); \
	return __LINE__; } } while (false)

template<class T> void Fill(T &value) {
	static_assert(sizeof(T) % 4 == 0);
	for (std::size_t i = 0; i < sizeof(T) / 4; ++i) {
		const std::uint32_t word = 0x3f010000u + static_cast<std::uint32_t>(i);
		std::memcpy(reinterpret_cast<unsigned char *>(&value) + i * 4, &word, 4);
	}
}

template<class T> bool IsZero(const T &value) {
	const T zero{};
	return std::memcmp(&value, &zero, sizeof(T)) == 0;
}

template<class T> struct Guarded {
	std::uint32_t before = 0x5aa55aa5u;
	T value{};
	std::uint32_t after = 0xa55aa55au;

	bool Intact() const { return before == 0x5aa55aa5u && after == 0xa55aa55au; }
};

int Entity() {
	fntech3_rtcw_entityState_t input;
	Fill(input);
	const trType_t expected[]{TR_STATIONARY, TR_INTERPOLATE, TR_LINEAR, TR_LINEAR_STOP,
		TR_RTCW_LINEAR_STOP_BACK, TR_SINE, TR_GRAVITY, TR_RTCW_GRAVITY_LOW,
		TR_RTCW_GRAVITY_FLOAT, TR_RTCW_GRAVITY_PAUSED, TR_RTCW_ACCELERATE, TR_RTCW_DECELERATE};
	for (int type = 0; type < 12; ++type) {
		input.pos.trType = type;
		input.apos.trType = 11 - type;
		Guarded<entityState_t> internal;
		Fill(internal.value);
		RTCW_STATE_CHECK(state::ToInternal(input, internal.value) && internal.Intact());
		RTCW_STATE_CHECK(internal.value.pos.trType == expected[type]);
		RTCW_STATE_CHECK(internal.value.apos.trType == expected[11 - type]);
		RTCW_STATE_CHECK(internal.value.pos.gravity == 0 && internal.value.apos.gravity == 0);
		RTCW_STATE_CHECK(internal.value.generic1 == 0 && internal.value.health == 0 &&
			internal.value.armor == 0 && internal.value.location == 0 &&
			internal.value.jumpTime == 0 && internal.value.doubleJumped == 0);
		Guarded<fntech3_rtcw_entityState_t> output;
		RTCW_STATE_CHECK(state::FromInternal(internal.value, output.value) && output.Intact());
		RTCW_STATE_CHECK(std::memcmp(&input, &output.value, sizeof(input)) == 0);
	}
	Guarded<entityState_t> internal;
	Fill(internal.value);
	const auto saved = internal;
	for (int invalid : {-1, 12, 0x7fffffff}) {
		input.pos.trType = invalid;
		RTCW_STATE_CHECK(!state::ToInternal(input, internal.value));
		RTCW_STATE_CHECK(std::memcmp(&saved, &internal, sizeof(internal)) == 0);
	}
	Guarded<fntech3_rtcw_entityState_t> output;
	Fill(output.value);
	const auto savedOutput = output;
	internal.value = {};
	for (std::uint32_t invalid : {static_cast<std::uint32_t>(TR_QL_ACCEL), 0xffffffffu}) {
		std::memcpy(&internal.value.apos.trType, &invalid, 4);
		RTCW_STATE_CHECK(!state::FromInternal(internal.value, output.value));
		RTCW_STATE_CHECK(std::memcmp(&output, &savedOutput, sizeof(output)) == 0);
	}
	return 0;
}

template<class External> int Player() {
	External input;
	Fill(input);
	input.common.serverCursorHintTrace.allsolid = 0;
	input.common.serverCursorHintTrace.startsolid = 1;
	Guarded<playerState_t> internal;
	Fill(internal.value);
	RTCW_STATE_CHECK(state::ToInternal(input, internal.value) && internal.Intact());
	RTCW_STATE_CHECK(internal.value.ammo[15] == input.common.ammo[15]);
	RTCW_STATE_CHECK(internal.value.rtcw.ammo[0] == input.common.ammo[16]);
	RTCW_STATE_CHECK(internal.value.rtcw.ammo[47] == input.common.ammo[63]);
	RTCW_STATE_CHECK(internal.value.rtcw.ammoclip[63] == input.common.ammoclip[63]);
	RTCW_STATE_CHECK(internal.value.events[1] == input.common.events[1]);
	RTCW_STATE_CHECK(internal.value.rtcw.events[0] == input.common.events[2]);
	RTCW_STATE_CHECK(internal.value.rtcw.eventParms[1] == input.common.eventParms[3]);
	RTCW_STATE_CHECK(IsZero(internal.value.grapplePoint) && internal.value.weaponPrimary == 0 &&
		internal.value.fov == 0 && internal.value.location == 0 && internal.value.generic1 == 0 &&
		internal.value.armorTier == 0 && internal.value.crouchSlideTime == 0);
	if constexpr (std::is_same_v<External, fntech3_rtcw_sp_playerState_t>) {
		RTCW_STATE_CHECK(internal.value.rtcw.identifyClient == 0);
	} else {
		RTCW_STATE_CHECK(internal.value.rtcw.accShowBits == 0 &&
			internal.value.rtcw.accHideBits == 0 && internal.value.rtcw.footstepCount == 0);
	}
	Guarded<External> output;
	RTCW_STATE_CHECK(state::FromInternal(internal.value, output.value) && output.Intact());
	RTCW_STATE_CHECK(std::memcmp(&input, &output.value, sizeof(input)) == 0);

	const auto savedInternal = internal;
	const auto savedOutput = output;
	for (int invalid : {-1, 2, 0x7fffffff}) {
		input.common.serverCursorHintTrace.startsolid = invalid;
		RTCW_STATE_CHECK(!state::ToInternal(input, internal.value));
		RTCW_STATE_CHECK(std::memcmp(&internal, &savedInternal, sizeof(internal)) == 0);
		std::memcpy(&internal.value.rtcw.serverCursorHintTrace.allsolid, &invalid, 4);
		RTCW_STATE_CHECK(!state::FromInternal(internal.value, output.value));
		RTCW_STATE_CHECK(std::memcmp(&output, &savedOutput, sizeof(output)) == 0);
		internal = savedInternal;
	}
	return 0;
}

template<class External> int Command() {
	External input;
	Fill(input);
	// Native tail padding is not state. Source padding must never be exported.
	auto expected = input;
	std::memset(reinterpret_cast<unsigned char *>(&expected) + 26, 0, 2);
	input.common.buttons = 255;
	expected.common.buttons = 255;
	input.common.forwardmove = expected.common.forwardmove = -128;
	input.common.wolfkick = expected.common.wolfkick = -1;
	Guarded<usercmd_t> internal;
	Fill(internal.value);
	RTCW_STATE_CHECK(state::ToInternal(input, internal.value) && internal.Intact());
	RTCW_STATE_CHECK(internal.value.buttons == 255 && internal.value.forwardmove == -128 &&
		internal.value.rtcw.wolfkick == -1 && internal.value.weaponPrimary == 0 &&
		internal.value.fov == 0 && internal.value.reserved == 0);
	if constexpr (std::is_same_v<External, fntech3_rtcw_sp_usercmd_t>) {
		RTCW_STATE_CHECK(internal.value.rtcw.mpSetup == 0 && internal.value.rtcw.identClient == 0);
	} else RTCW_STATE_CHECK(internal.value.rtcw.cld == 0);
	Guarded<External> output;
	RTCW_STATE_CHECK(state::FromInternal(internal.value, output.value) && output.Intact());
	RTCW_STATE_CHECK(std::memcmp(&expected, &output.value, sizeof(expected)) == 0);
	return 0;
}

int Run() {
	gameState_t gameState{};
	// The engine's store holds the largest profile's gamestate; RTCW's record
	// holds its own 16,000 bytes, and a larger one is refused below.
	gameState.dataCount = FNTECH3_RTCW_MAX_GAMESTATE_CHARS;
	std::memset( gameState.stringData + 1, 'x', FNTECH3_RTCW_MAX_GAMESTATE_CHARS - 2 );
	for ( int index = 0; index < MAX_GAME_CONFIGSTRINGS; ++index ) gameState.stringOffsets[index] = index;
	Guarded<fntech3_rtcw_gameState_t> gameOutput;
	RTCW_STATE_CHECK(state::FromInternal(gameState, gameOutput.value) && gameOutput.Intact());
	RTCW_STATE_CHECK(gameOutput.value.stringOffsets[2047] == 2047 && gameOutput.value.dataCount == 16000);
	RTCW_STATE_CHECK(std::memcmp(gameOutput.value.stringData, gameState.stringData, FNTECH3_RTCW_MAX_GAMESTATE_CHARS) == 0);
	const auto savedGame = gameOutput;
	gameState.stringOffsets[2047] = 16000;
	RTCW_STATE_CHECK(!state::FromInternal(gameState, gameOutput.value));
	RTCW_STATE_CHECK(std::memcmp(&gameOutput, &savedGame, sizeof(gameOutput)) == 0);
	gameState.stringOffsets[2047] = 0;
	gameState.dataCount = 16001;
	RTCW_STATE_CHECK(!state::FromInternal(gameState, gameOutput.value));
	RTCW_STATE_CHECK(std::memcmp(&gameOutput, &savedGame, sizeof(gameOutput)) == 0);
	if (const int result = Entity()) return result;
	if (const int result = Player<fntech3_rtcw_sp_playerState_t>()) return result;
	if (const int result = Player<fntech3_rtcw_mp_playerState_t>()) return result;
	if (const int result = Command<fntech3_rtcw_sp_usercmd_t>()) return result;
	return Command<fntech3_rtcw_mp_usercmd_t>();
}

#undef RTCW_STATE_CHECK
} // namespace rtcwStateTests
#endif
