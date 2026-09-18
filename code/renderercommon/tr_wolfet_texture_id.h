/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_TR_WOLFET_TEXTURE_ID_H
#define FNTECH3_TR_WOLFET_TEXTURE_ID_H

/* Enemy Territory can ask for the index of an already-loaded image by the exact
 * name it was loaded under, and answers -1 when it does not hold one. The index
 * is a position in the renderer's own image list, valid only until the list is
 * rebuilt, which is what ET's own answer is too.
 *
 * Included by each renderer's image file, where the list is in scope. */
int RE_GetWolfETTextureId( const char *name ) {
	int i;
	if ( ri.gameId != COM_GAME_WOLFET ) {
		ri.Error( ERR_DROP, "ET texture lookup outside its game profile" );
		return -1;
	}
	if ( !name || !name[0] ) {
		return -1;
	}
	/* ET compares the stored name exactly, without case folding, so a name
	 * that differs only in case is a different image to it and to this. */
	for ( i = 0; i < tr.numImages; ++i ) {
		if ( tr.images[i] && !strcmp( name, tr.images[i]->imgName ) ) {
			return i;
		}
	}
	return -1;
}

#endif
