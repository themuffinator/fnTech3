/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_CLIENT_RENDER_COMPAT_HPP
#define FNTECH3_CLIENT_RENDER_COMPAT_HPP

#include "../compat/render_record_adapter.hpp"

// The VM owns only its frozen record. Renderer-facing objects are local copies;
// neither a QVM nor a native module may supply an engine-internal structure.
inline refEntity_t CL_ModuleRefEntity( vm_t *vm, intptr_t address ) {
	const auto *profile = Com_GameProfile();
	const auto id = profile ? profile->id : COM_GAME_AUTO;
	const auto bytes = fntech3::render::EntityBytes( id );
	if ( !vm || !address || !bytes ) Com_Error( ERR_DROP, "Invalid module render entity contract" );
	VM_CHECKBOUNDS( vm, address, bytes );
	refEntity_t result{};
	if ( !fntech3::render::ReadEntity( id, VM_ExplicitArgPtr( vm, address ), result ) )
		Com_Error( ERR_DROP, "Invalid module render entity" );
	return result;
}

inline refEntity_t CL_ModuleRTCWTagEntity( vm_t *vm, intptr_t address ) {
	const auto id = Com_GameProfile()->id;
	const auto bytes = fntech3::render::EntityBytes( id );
	if ( !vm || !address || !bytes ) Com_Error( ERR_DROP, "Invalid RTCW tag entity contract" );
	VM_CHECKBOUNDS( vm, address, bytes );
	refEntity_t result{};
	if ( !fntech3::render::ReadRTCWTagEntity( id, VM_ExplicitArgPtr( vm, address ), result ) )
		Com_Error( ERR_DROP, "Invalid RTCW tag entity" );
	return result;
}

inline refdef_t CL_ModuleRefdef( vm_t *vm, intptr_t address ) {
	const auto *profile = Com_GameProfile();
	const auto id = profile ? profile->id : COM_GAME_AUTO;
	const auto bytes = fntech3::render::RefdefBytes( id );
	if ( !vm || !address || !bytes ) Com_Error( ERR_DROP, "Invalid module render view contract" );
	VM_CHECKBOUNDS( vm, address, bytes );
	refdef_t result{};
	if ( !fntech3::render::ReadRefdef( id, VM_ExplicitArgPtr( vm, address ), result ) )
		Com_Error( ERR_DROP, "Invalid module render view" );
	return result;
}

#endif
