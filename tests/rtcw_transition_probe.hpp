// SPDX-License-Identifier: GPL-2.0-or-later
// Included only by a private probe build, never by the shipped client.
// Only input: one SP briefing Ready click per level under AGENTS.md's standing
// rule. Everything else is console commands from the probe's own config files.
static int rtcwTransitionPhase;
static unsigned rtcwTransitionStart;
static unsigned rtcwTransitionBegun;
static unsigned rtcwTransitionObserved;
static int rtcwTransitionLevels;
static bool rtcwTransitionCommanded;
static char rtcwTransitionMap[MAX_QPATH];

static void CL_RTCWTransitionObserve( const char *phase ) {
	const playerState_t &ps = cl.snap.ps;
	char ammo[512] = "";
	for ( int i = 0; i < 64; ++i ) {
		const int count = i < MAX_WEAPONS ? ps.ammo[i] : ps.rtcw.ammo[i - MAX_WEAPONS];
		const int clip = ps.rtcw.ammoclip[i];
		if ( count || clip ) Q_strcat(ammo, sizeof(ammo), va("%d:%d/%d,", i, count, clip));
	}
	Com_Printf("TRANSITION %s level=%d map=%s state=%d valid=%d time=%d camera=%d paused=%d norender=%d "
		"reloading=%d persid=%s health=%d armor=%d weapon=%d weapons=%08x,%08x ammo=%s origin=%.0f,%.0f,%.0f\n",
		phase, rtcwTransitionLevels, Cvar_VariableString("mapname"), cls.state, cl.snap.valid,
		cl.snap.serverTime, cl.rtcwCameraMode, Cvar_VariableIntegerValue("cl_paused"),
		Cvar_VariableIntegerValue("cg_norender"), Cvar_VariableIntegerValue("g_reloading"),
		Cvar_VariableString("persid"), ps.stats[0], ps.stats[2], ps.weapon,
		ps.rtcw.weapons[0], ps.rtcw.weapons[1], ammo, ps.origin[0], ps.origin[1], ps.origin[2]);
}

static void CL_RTCWTransitionStart() {
	if ( rtcwTransitionPhase || !Com_GameIsRTCWSP() ) return;
	rtcwTransitionBegun = static_cast<unsigned>(Sys_Milliseconds());
	rtcwTransitionPhase = 1;
	Com_Printf("TRANSITION probe started\n");
}

static void CL_RTCWTransitionFinish( const char *why ) {
	CL_RTCWTransitionObserve(why);
	Cbuf_AddText(va("screenshotPNG transition-%02d-%s-final; wait 5; quit\n",
		rtcwTransitionLevels, Cvar_VariableString("mapname")));
	rtcwTransitionPhase = 0;
}

static void CL_RTCWTransitionFrame() {
	if ( !rtcwTransitionPhase ) return;
	const unsigned now = static_cast<unsigned>(Sys_Milliseconds());
	const unsigned timeout = static_cast<unsigned>(Cvar_VariableIntegerValue("rtcw_probe_timeout")) * 1000;
	if ( timeout && now - rtcwTransitionBegun >= timeout ) {
		CL_RTCWTransitionFinish("timeout");
		return;
	}
	if ( cls.state != CA_ACTIVE ) return;
	const char *map = Cvar_VariableString("mapname");
	if ( Q_stricmp(map, rtcwTransitionMap) ) {
		Q_strncpyz(rtcwTransitionMap, map, sizeof(rtcwTransitionMap));
		++rtcwTransitionLevels;
		rtcwTransitionPhase = 1;
		rtcwTransitionStart = now;
		rtcwTransitionObserved = now;
		CL_RTCWTransitionObserve("level-active");
	}
	unsigned elapsed = now - rtcwTransitionStart;
	if ( rtcwTransitionPhase == 1 ) {
		if ( elapsed >= 1500 && uivm && Key_GetCatcher() == KEYCATCH_UI &&
			Cvar_VariableIntegerValue("cl_paused") && Cvar_VariableIntegerValue("cg_norender") ) {
			// Optionally issue one console command while the first briefing
			// holds the game, before the Ready click.
			const char *command = Cvar_VariableString("rtcw_probe_briefing_command");
			if ( command[0] && !rtcwTransitionCommanded ) {
				rtcwTransitionCommanded = true;
				CL_RTCWTransitionObserve("briefing-command");
				Com_Printf("TRANSITION briefing command: %s\n", command);
				Cbuf_AddText(va("%s\n", command));
				rtcwTransitionStart = now;
				return;
			}
			CL_RTCWTransitionObserve("briefing");
			Com_Printf("TRANSITION briefing Ready click at 620,470\n");
			VM_Call(uivm, 2, UI_MOUSE_EVENT, -10000, -10000);
			VM_Call(uivm, 2, UI_MOUSE_EVENT, 620, 470);
			CL_KeyEvent(K_MOUSE1, qtrue, cls.realtime);
			CL_KeyEvent(K_MOUSE1, qfalse, cls.realtime);
			rtcwTransitionPhase = 2;
			rtcwTransitionStart = now;
		} else if ( elapsed >= 60000 ) {
			Com_Printf("TRANSITION no briefing within 60 s\n");
			rtcwTransitionPhase = 2;
			rtcwTransitionStart = now;
		}
		return;
	}
	if ( now - rtcwTransitionObserved >= 10000 ) {
		rtcwTransitionObserved = now;
		CL_RTCWTransitionObserve("tick");
	}
	// Let an opening camera finish before the level's steps run; the steps
	// teleport the player, which a camera sequence would fight.
	if ( rtcwTransitionPhase == 2 && elapsed >= 3000 &&
		( !cl.rtcwCameraMode || elapsed >= 150000 ) && !Cvar_VariableIntegerValue("cg_norender") ) {
		CL_RTCWTransitionObserve("playing");
		Cbuf_AddText(va("screenshotPNG transition-%02d-%s-start\n", rtcwTransitionLevels, map));
		if ( rtcwTransitionLevels >= Cvar_VariableIntegerValue("rtcw_probe_levels") ) {
			rtcwTransitionPhase = 4;
		} else {
			Cbuf_AddText(va("exec transition-%s.cfg\n", map));
			rtcwTransitionPhase = 3;
		}
		rtcwTransitionStart = now;
		return;
	}
	if ( rtcwTransitionPhase == 4 &&
		elapsed >= static_cast<unsigned>(Cvar_VariableIntegerValue("rtcw_probe_dwell")) * 1000 ) {
		CL_RTCWTransitionFinish("final");
	}
}
