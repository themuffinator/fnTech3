/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_RTCW_IMAGE_MIP_H
#define FNTECH3_RTCW_IMAGE_MIP_H

static int R_ImagePicMipLevel( imgFlags_t flags ) {
	const cvar_t *setting = r_picmip;
	if ( ri.gameId == COM_GAME_RTCW_SP && ( flags & IMGFLAG_RTCW_CHARACTER_MIP ) && r_picmip2 )
		setting = r_picmip2;
	/* Range checks normally enforce this at registration. Keep shifts bounded
	 * if an internal caller supplies a malformed value during initialization.
	 */
	return setting->integer < 0 ? 0 : setting->integer > 16 ? 16 : setting->integer;
}
#endif
