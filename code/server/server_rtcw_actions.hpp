/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_SERVER_RTCW_ACTIONS_HPP
#define FNTECH3_SERVER_RTCW_ACTIONS_HPP
#include "server_rtcw_args.hpp"
#include "../compat/rtcw_bot_actions.hpp"
#include "../botlib/be_ea.h"

static bool SV_RTCWActionCall(const vm_t *vm, fntech3::rtcw::module::Call call,
	const intptr_t *args, intptr_t &result) {
	using fntech3::rtcw::module::Call;
	if (!Com_GameIsRTCW()) return false;
	switch (call) {
	case Call::BOTLIB_EA_SAY: case Call::BOTLIB_EA_SAY_TEAM:
	case Call::BOTLIB_EA_USE_ITEM: case Call::BOTLIB_EA_DROP_ITEM:
	case Call::BOTLIB_EA_USE_INV: case Call::BOTLIB_EA_DROP_INV:
	case Call::BOTLIB_EA_GESTURE: case Call::BOTLIB_EA_COMMAND:
	case Call::BOTLIB_EA_SELECT_WEAPON: case Call::BOTLIB_EA_TALK:
	case Call::BOTLIB_EA_ATTACK: case Call::BOTLIB_EA_RELOAD:
	case Call::BOTLIB_EA_USE: case Call::BOTLIB_EA_RESPAWN:
	case Call::BOTLIB_EA_JUMP: case Call::BOTLIB_EA_DELAYED_JUMP:
	case Call::BOTLIB_EA_CROUCH: case Call::BOTLIB_EA_MOVE_UP:
	case Call::BOTLIB_EA_MOVE_DOWN: case Call::BOTLIB_EA_MOVE_FORWARD:
	case Call::BOTLIB_EA_MOVE_BACK: case Call::BOTLIB_EA_MOVE_LEFT:
	case Call::BOTLIB_EA_MOVE_RIGHT: case Call::BOTLIB_EA_MOVE:
	case Call::BOTLIB_EA_VIEW: case Call::BOTLIB_EA_END_REGULAR:
	case Call::BOTLIB_EA_GET_INPUT: case Call::BOTLIB_EA_RESET_INPUT: break;
	default: return false;
	}
	if (!botlib_export) Com_Error(ERR_DROP, "RTCW bot library interface is unavailable");
	const int client = SV_RTCWInteger(args[1]);
	if (client < 0 || client >= Com_GameProfile()->maxClients || !EA_ClientInputAvailable(client))
		Com_Error(ERR_DROP, "RTCW action client has no input state");
	auto &ea = botlib_export->ea;
	switch (call) {
	case Call::BOTLIB_EA_SAY: case Call::BOTLIB_EA_SAY_TEAM:
	case Call::BOTLIB_EA_USE_ITEM: case Call::BOTLIB_EA_DROP_ITEM:
	case Call::BOTLIB_EA_USE_INV: case Call::BOTLIB_EA_DROP_INV:
	case Call::BOTLIB_EA_COMMAND: {
		const auto text = SV_RTCWString<MAX_STRING_CHARS>(vm, args[2]);
		if (call == Call::BOTLIB_EA_SAY) ea.EA_Say(client, text.data());
		else if (call == Call::BOTLIB_EA_SAY_TEAM) ea.EA_SayTeam(client, text.data());
		else if (call == Call::BOTLIB_EA_COMMAND) ea.EA_Command(client, text.data());
		else {
			const char *prefix = call == Call::BOTLIB_EA_USE_ITEM ? "use " :
				call == Call::BOTLIB_EA_DROP_ITEM ? "drop " :
				call == Call::BOTLIB_EA_USE_INV ? "invuse " : "invdrop ";
			std::array<char, MAX_STRING_CHARS + 8> command{};
			std::strcpy(command.data(), prefix);
			std::strcat(command.data(), text.data());
			ea.EA_Command(client, command.data());
		}
		break;
	}
	case Call::BOTLIB_EA_GESTURE: ea.EA_Gesture(client); break;
	case Call::BOTLIB_EA_SELECT_WEAPON: ea.EA_SelectWeapon(client, SV_RTCWInteger(args[2])); break;
	case Call::BOTLIB_EA_TALK: ea.EA_Talk(client); break;
	case Call::BOTLIB_EA_ATTACK: ea.EA_Attack(client); break;
	case Call::BOTLIB_EA_RELOAD: ea.EA_Action(client, ACTION_RTCW_RELOAD); break;
	case Call::BOTLIB_EA_USE: ea.EA_Use(client); break;
	case Call::BOTLIB_EA_RESPAWN: ea.EA_Respawn(client); break;
	case Call::BOTLIB_EA_JUMP: ea.EA_Jump(client); break;
	case Call::BOTLIB_EA_DELAYED_JUMP: ea.EA_DelayedJump(client); break;
	case Call::BOTLIB_EA_CROUCH: ea.EA_Crouch(client); break;
	case Call::BOTLIB_EA_MOVE_UP: ea.EA_MoveUp(client); break;
	case Call::BOTLIB_EA_MOVE_DOWN: ea.EA_MoveDown(client); break;
	case Call::BOTLIB_EA_MOVE_FORWARD: ea.EA_MoveForward(client); break;
	case Call::BOTLIB_EA_MOVE_BACK: ea.EA_MoveBack(client); break;
	case Call::BOTLIB_EA_MOVE_LEFT: ea.EA_MoveLeft(client); break;
	case Call::BOTLIB_EA_MOVE_RIGHT: ea.EA_MoveRight(client); break;
	case Call::BOTLIB_EA_MOVE: {
		auto dir = SV_RTCWVector(vm, args[2]);
		ea.EA_Move(client, dir.data(), SV_RTCWFloat(args[3])); break;
	}
	case Call::BOTLIB_EA_VIEW: {
		auto angles = SV_RTCWVector(vm, args[2]); ea.EA_View(client, angles.data()); break;
	}
	case Call::BOTLIB_EA_END_REGULAR: ea.EA_EndRegular(client, SV_RTCWFloat(args[2])); break;
	case Call::BOTLIB_EA_GET_INPUT: {
		const float time = SV_RTCWFloat(args[2]);
		void *destination = SV_RTCWPointer(vm, args[3], sizeof(fntech3_rtcw_bot_input_t));
		bot_input_t input; ea.EA_GetInput(client, time, &input);
		const auto output = fntech3::rtcw::bot::ActionOutput(input, EA_RTCWExtraInputFlags(client));
		std::memcpy(destination, &output, sizeof(output)); break;
	}
	case Call::BOTLIB_EA_RESET_INPUT: {
		if (!args[2]) { EA_RestoreRTCWInput(client, nullptr, 0); break; }
		fntech3_rtcw_bot_input_t input;
		std::memcpy(&input, SV_RTCWPointer(vm, args[2], sizeof(input)), sizeof(input));
		bot_input_t converted;
		if (!fntech3::rtcw::bot::ActionInput(input, converted))
			Com_Error(ERR_DROP, "Invalid RTCW action input");
		EA_RestoreRTCWInput(client, &converted,
			static_cast<unsigned>(input.actionflags) & ~fntech3::rtcw::bot::KnownActionBits); break;
	}
	default: return false;
	}
	result = 0; return true;
}
#endif
