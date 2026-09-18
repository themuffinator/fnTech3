/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_TR_WOLFET_VIEW_PARMS_H
#define FNTECH3_TR_WOLFET_VIEW_PARMS_H

/* Enemy Territory renders its portal sky as a second scene and asks the engine
 * to keep the first scene's view parameters across it, so the detail-level
 * computation is not thrown away by the second render. One saved set is all ET
 * ever asks for; its own renderer keeps exactly one too.
 *
 * Restoring before anything was saved would publish an all-zero view, so it is
 * refused rather than done. */
static viewParms_t wolfetSavedViewParms;
static qboolean wolfetViewParmsSaved;

void RE_SaveWolfETViewParms( void ) {
	if ( ri.gameId != COM_GAME_WOLFET ) {
		ri.Error( ERR_DROP, "ET view parameters saved outside their game profile" );
		return;
	}
	wolfetSavedViewParms = tr.viewParms;
	wolfetViewParmsSaved = qtrue;
}

void RE_RestoreWolfETViewParms( void ) {
	if ( ri.gameId != COM_GAME_WOLFET ) {
		ri.Error( ERR_DROP, "ET view parameters restored outside their game profile" );
		return;
	}
	if ( !wolfetViewParmsSaved ) return;
	tr.viewParms = wolfetSavedViewParms;
}

#endif
