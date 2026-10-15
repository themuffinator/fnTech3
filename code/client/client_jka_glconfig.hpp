/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_CLIENT_JKA_GLCONFIG_HPP
#define FNTECH3_CLIENT_JKA_GLCONFIG_HPP
#include "client_jka_records.hpp"

/* hudFrame: the client game projects world points onto its virtual 640x480
 * screen with the view it computed from this screen. Told it is the HUD's 4:3
 * frame (CL_HudModuleScreen), it computes a 4:3 view, which the engine widens
 * to the real screen while placing its 2D in that frame (cl_hud.cpp). */
static void CL_JKAGetGlconfig( vm_t *vm, intptr_t address, bool hudFrame = false ) {
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
	if ( hudFrame ) {
		int width = result.vidWidth, height = result.vidHeight;
		CL_HudModuleScreen( &width, &height );
		result.vidWidth = width;
		result.vidHeight = height;
	}
	std::memcpy( VM_ExplicitArgPtr( vm, address ), &result, sizeof( result ) );
}
#endif
