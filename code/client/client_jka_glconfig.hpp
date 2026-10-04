/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_CLIENT_JKA_GLCONFIG_HPP
#define FNTECH3_CLIENT_JKA_GLCONFIG_HPP
#include "client_jka_records.hpp"

static void CL_JKAGetGlconfig( vm_t *vm, intptr_t address ) {
	VM_CHECKBOUNDS( vm, address, sizeof( fntech3_jka_mp_glconfig_t ) );
	if ( !vm->entryPoint || vm->dllExports || !address )
		Com_Error( ERR_DROP, "Jedi Academy glconfig requires its legacy native module and a destination" );
	if ( !re.GetTextureCapabilities || !re.GetConfig )
		Com_Error( ERR_DROP, "The selected renderer does not expose Jedi Academy's texture capabilities" );
	float anisotropy = 0;
	qboolean clamp = qfalse;
	re.GetTextureCapabilities( &anisotropy, &clamp );
	fntech3_jka_mp_glconfig_t result{};
	if ( !fntech3::jka::records::Glconfig( *re.GetConfig(), anisotropy, clamp != qfalse, result ) )
		Com_Error( ERR_DROP, "The selected renderer cannot supply Jedi Academy's native glconfig" );
	std::memcpy( VM_ExplicitArgPtr( vm, address ), &result, sizeof( result ) );
}
#endif
