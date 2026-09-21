/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_STVEF_CVARS_H
#define FNTECH3_STVEF_CVARS_H

static cvar_t *R_RegisterTextureCompressionCvar( void ) {
	/* Retail SP 1.20 at 0x4404c9 and HM 1.20 at 0x453729 register
	 * this spelling with default 1 and ARCHIVE | LATCH. Both menus use
	 * it directly. Keep one authoritative cvar for the selected profile;
	 * a Q3-style setting must not override an Elite Force configuration.
	 */
	if ( ri.gameId == COM_GAME_STVEF_SP || ri.gameId == COM_GAME_STVEF_MP )
		return ri.Cvar_Get( "r_ext_compress_textures", "1", CVAR_ARCHIVE | CVAR_LATCH );
	return ri.Cvar_Get( "r_ext_compressed_textures", "0", CVAR_ARCHIVE_ND | CVAR_LATCH | CVAR_DEVELOPER );
}

#endif
