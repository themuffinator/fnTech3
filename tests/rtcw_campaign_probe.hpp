// SPDX-License-Identifier: GPL-2.0-or-later
// Included only by a private probe build, never by the shipped client.
// Only input: one SP briefing Ready click under AGENTS.md's standing rule.
static unsigned rtcwProbeStart;
static unsigned rtcwProbeSavedAt;
static int rtcwProbePhase;
static bool rtcwProbeClicked;
static bool rtcwProbeGuardChecked;
static void CL_RTCWCampaignObserve( const char *phase ) {
	Com_Printf("CAMPAIGN %s map=%s state=%d valid=%d time=%d camera=%d health=%d origin=%.3f,%.3f,%.3f weapon=%d loading=%d\n",
		phase, Cvar_VariableString("mapname"), cls.state, cl.snap.valid, cl.snap.serverTime,
		cl.rtcwCameraMode, cl.snap.ps.stats[0], cl.snap.ps.origin[0], cl.snap.ps.origin[1],
		cl.snap.ps.origin[2], cl.snap.ps.weapon, Cvar_VariableIntegerValue("savegame_loading"));
	if ( Com_GameIsRTCWMP() ) Com_Printf("CAMPAIGN mp-match phase=%s gamestate=%d time=%d commandTime=%d\n",
		phase, Cvar_VariableIntegerValue("gamestate"), cl.snap.serverTime, cl.snap.ps.commandTime);
}
static void CL_RTCWCampaignStart() {
	if ( rtcwProbePhase || !Com_GameIsRTCW() || cls.state != CA_ACTIVE ) return;
	rtcwProbeStart = static_cast<unsigned>(Sys_Milliseconds());
	rtcwProbePhase = 1; CL_RTCWCampaignObserve("start");
}
static void CL_RTCWCampaignFrame() {
	if ( !rtcwProbePhase ) return;
	unsigned elapsed = static_cast<unsigned>(Sys_Milliseconds()) - rtcwProbeStart;
	if ( Com_GameIsRTCWMP() ) {
		// Leave the command buffer free between observations so game-issued
		// warmup restarts execute immediately, without a long probe wait ahead.
		if ( rtcwProbePhase == 1 && elapsed >= 1500 ) {
			Cbuf_AddText("team red 0 3\n"); rtcwProbePhase = 2;
		}
		if ( rtcwProbePhase == 2 && elapsed >= 3000 ) {
			Cbuf_AddText("CloseLimboMenu; -scores\n"); rtcwProbePhase = 3;
		}
		if ( rtcwProbePhase == 3 && elapsed >= 14000 ) {
			CL_RTCWCampaignObserve("match-running"); rtcwProbePhase = 4;
		}
		if ( rtcwProbePhase == 4 && elapsed >= 17000 ) {
			CL_RTCWCampaignObserve("match-complete");
			Cbuf_AddText("screenshotPNG campaign-before; status; wait 2; quit\n"); rtcwProbePhase = 0;
		}
		return;
	}
	if ( !rtcwProbeGuardChecked && elapsed >= 5000 && Com_GameIsRTCWSP() &&
		cl.rtcwCameraMode && Cvar_VariableIntegerValue("rtcw_probe_guard") ) {
		rtcwProbeGuardChecked = true;
		Com_Printf("CAMPAIGN checking cinematic save refusal\n");
		Cbuf_AddText("savegame campaign_cinematic\n");
	}
	if ( rtcwProbePhase <= 2 && elapsed >= 1500 ) {
		if ( Com_GameIsRTCWSP() && !rtcwProbeClicked && uivm &&
			Key_GetCatcher() == KEYCATCH_UI && Cvar_VariableIntegerValue("cl_paused") &&
			Cvar_VariableIntegerValue("cg_norender") ) {
			rtcwProbeClicked = true;
			Com_Printf("CAMPAIGN intro Ready click at 620,470\n");
			VM_Call(uivm, 2, UI_MOUSE_EVENT, -10000, -10000);
			VM_Call(uivm, 2, UI_MOUSE_EVENT, 620, 470);
			CL_KeyEvent(K_MOUSE1, qtrue, cls.realtime);
			CL_KeyEvent(K_MOUSE1, qfalse, cls.realtime);
			rtcwProbeStart = static_cast<unsigned>(Sys_Milliseconds());
			elapsed = 0;
		}
		rtcwProbePhase = 2;
	}
	const unsigned dwell = static_cast<unsigned>(Cvar_VariableIntegerValue("rtcw_probe_dwell")) * 1000;
	if ( rtcwProbePhase == 2 && elapsed >= dwell ) {
		// Console saves during a camera sequence can persist the original
		// game's raw cameraPortal pointer. Exercise the playable save path.
		if ( Cvar_VariableIntegerValue("rtcw_probe_saveload") &&
			( cl.rtcwCameraMode || Cvar_VariableIntegerValue("g_reloading") ) ) return;
		rtcwProbeSavedAt = elapsed;
		CL_RTCWCampaignObserve("before-save");
		Cbuf_AddText("screenshotPNG campaign-before; status; g_missionStats\n");
		if ( Cvar_VariableIntegerValue("rtcw_probe_saveload") ) {
			Cbuf_AddText("savegame campaign\n"); rtcwProbePhase = 3;
		} else { Cbuf_AddText("wait 5; quit\n"); rtcwProbePhase = 0; }
	}
	if ( rtcwProbePhase == 3 && elapsed >= rtcwProbeSavedAt + 2000 ) {
		CL_RTCWCampaignObserve("load-request"); Cbuf_AddText("loadgame campaign\n"); rtcwProbePhase = 4;
	}
	if ( rtcwProbePhase == 4 && elapsed >= rtcwProbeSavedAt + 6000 ) {
		CL_RTCWCampaignObserve("after-load");
		Cbuf_AddText("screenshotPNG campaign-after; savegame campaign_after\n"); rtcwProbePhase = 5;
	}
	if ( rtcwProbePhase == 5 && elapsed >= rtcwProbeSavedAt + 8000 ) {
		CL_RTCWCampaignObserve("complete"); Cbuf_AddText("wait 5; quit\n"); rtcwProbePhase = 0;
	}
}
