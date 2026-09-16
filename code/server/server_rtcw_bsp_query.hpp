/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_SERVER_RTCW_BSP_QUERY_HPP
#define FNTECH3_SERVER_RTCW_BSP_QUERY_HPP
#include "server_rtcw_args.hpp"

static bool SV_RTCWBSPQueryCall(const vm_t *vm, fntech3::rtcw::module::Call call,
	const intptr_t *args, intptr_t &result) {
	using fntech3::rtcw::module::Call;
	if (!Com_GameIsRTCW()) return false;
	switch (call) {
	case Call::BOTLIB_AAS_NEXT_BSP_ENTITY:
	case Call::BOTLIB_AAS_VALUE_FOR_BSP_EPAIR_KEY:
	case Call::BOTLIB_AAS_VECTOR_FOR_BSP_EPAIR_KEY:
	case Call::BOTLIB_AAS_FLOAT_FOR_BSP_EPAIR_KEY:
	case Call::BOTLIB_AAS_INT_FOR_BSP_EPAIR_KEY: break;
	default: return false;
	}
	if (!botlib_export) Com_Error(ERR_DROP, "RTCW bot library interface is unavailable");
	const int entity = SV_RTCWInteger(args[1]);
	if (call == Call::BOTLIB_AAS_NEXT_BSP_ENTITY) {
		result = botlib_export->aas.AAS_NextBSPEntity(entity);
		return true;
	}
	// Preserve overlapping key/output arguments by owning the key first.
	const auto key = SV_RTCWString<MAX_STRING_CHARS>(vm, args[2]);
	if (call == Call::BOTLIB_AAS_VALUE_FOR_BSP_EPAIR_KEY) {
		const int capacity = SV_RTCWInteger(args[4]);
		if (capacity < 0) Com_Error(ERR_DROP, "Invalid RTCW BSP property capacity");
		if (!capacity) { result = 0; return true; }
		auto *out = static_cast<char *>(SV_RTCWPointer(vm, args[3], capacity));
		result = botlib_export->aas.AAS_ValueForBSPEpairKey(entity, key.data(), out, capacity);
	} else if (call == Call::BOTLIB_AAS_VECTOR_FOR_BSP_EPAIR_KEY) {
		void *out = SV_RTCWPointer(vm, args[3], sizeof(vec3_t));
		vec3_t value{};
		result = botlib_export->aas.AAS_VectorForBSPEpairKey(entity, key.data(), value);
		std::memcpy(out, value, sizeof(value));
	} else if (call == Call::BOTLIB_AAS_FLOAT_FOR_BSP_EPAIR_KEY) {
		void *out = SV_RTCWPointer(vm, args[3], sizeof(float));
		float value = 0;
		result = botlib_export->aas.AAS_FloatForBSPEpairKey(entity, key.data(), &value);
		std::memcpy(out, &value, sizeof(value));
	} else {
		void *out = SV_RTCWPointer(vm, args[3], sizeof(int));
		int value = 0;
		result = botlib_export->aas.AAS_IntForBSPEpairKey(entity, key.data(), &value);
		std::memcpy(out, &value, sizeof(value));
	}
	return true;
}
#endif
