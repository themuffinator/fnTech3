/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */

/* A dedicated server has no American McGee's Alice engine UI: its game
 * module's widget prints, loading stages and loading-screen refreshes (game
 * imports 106, 108 and 109) go nowhere, as retail skips them whenever
 * `dedicated` is set (WidgetPrintf 0x004389e0). The client build serves them
 * in code/client/cl_alice.cpp. */

#include "q_shared.h"
#include "qcommon.h"

#ifdef DEDICATED

void Com_AliceWidgetPrint( const char *widget, const char *text ) {
	(void)widget;
	(void)text;
}

void Com_AliceLoadingStage( int stage ) {
	(void)stage;
}

void Com_AliceUpdateLoadingScreen( void ) {
}

#endif
