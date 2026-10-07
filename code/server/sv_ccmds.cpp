/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/

#include "server.h"
#include "../qcommon/netchan_safety.hpp"

#include <array>
#include "server_rtcw_save.hpp"
#include "../compat/rtcw_match_state.hpp"

/*
===============================================================================

OPERATOR CONSOLE ONLY COMMANDS

These commands can only be entered from stdin or by a remote operator datagram
===============================================================================
*/


/*
==================
SV_GetPlayerByHandle

Returns the player with player id or name from Cmd_Argv(1)
==================
*/
client_t *SV_GetPlayerByHandle( void ) {
	client_t	*cl;
	const char		*s;
	std::array<char, MAX_NAME_LENGTH> cleanName{};

	// make sure server is running
	if ( !com_sv_running->integer ) {
		return nullptr;
	}

	if ( Cmd_Argc() < 2 ) {
		Com_Printf( "No player specified.\n" );
		return nullptr;
	}

	s = Cmd_Argv(1);

	// Check whether this is a numeric player handle
	const char *end = s;
	while ( *end >= '0' && *end <= '9' ) {
		++end;
	}

	if(!*end)
	{
		int plid = SV_ParseInt(s);

		// Check for numeric playerid match
		if( SV_IsClientIndex( plid ) )
		{
			cl = &SV_ClientForIndex( plid );
			
			if (cl->state >= CS_CONNECTED)
				return cl;
		}
	}

	// check for a name match
	for ( client_t &client : SV_Clients() ) {
		if ( client.state < CS_CONNECTED ) {
			continue;
		}
		if ( !Q_stricmp( client.name, s ) ) {
			return &client;
		}

		Q_strncpyz( cleanName.data(), client.name, SV_ArraySize(cleanName) );
		Q_CleanStr( cleanName.data() );
		if ( !Q_stricmp( cleanName.data(), s ) ) {
			return &client;
		}
	}

	Com_Printf( "Player %s is not on the server\n", s );

	return nullptr;
}


/*
==================
SV_GetPlayerByNum

Returns the player with idnum from Cmd_Argv(1)
==================
*/
static client_t *SV_GetPlayerByNum( void ) {
	client_t	*cl;
	int			i;
	int			idnum;
	const char		*s;

	// make sure server is running
	if ( !com_sv_running->integer ) {
		return nullptr;
	}

	if ( Cmd_Argc() < 2 ) {
		Com_Printf( "No player specified.\n" );
		return nullptr;
	}

	s = Cmd_Argv(1);

	for (i = 0; s[i]; i++) {
		if (s[i] < '0' || s[i] > '9') {
			Com_Printf( "Bad slot number: %s\n", s);
			return nullptr;
		}
	}
	idnum = SV_ParseInt( s );
	if ( !SV_IsClientIndex( idnum ) ) {
		Com_Printf( "Bad client slot: %i\n", idnum );
		return nullptr;
	}

	cl = &SV_ClientForIndex( idnum );
	if ( cl->state < CS_CONNECTED ) {
		Com_Printf( "Client %i is not active\n", idnum );
		return nullptr;
	}
	return cl;
}

//=========================================================


/*
==================
SV_Map_f

Restart the server on a different map
==================
*/
static void SV_Map_f( void ) {
	const char		*cmd;
	const char		*map;
	bool		killBots, cheat, singlePlayerCommand;
	const bool		quakeLive = Com_GameIsQuakeLive() != 0;
	std::array<char, MAX_QPATH> expanded{};
	std::array<char, MAX_QPATH> mapname{};
	int			len;
	char saveFilename[fntech3::rtcw::savegame::PathBytes]{};
	fntech3::rtcw::savegame::Header saveHeader;

	cmd = Cmd_Argv(0);
	singlePlayerCommand = Q_stricmpn( cmd, "sp", 2 ) == 0;
	map = Cmd_Argv(1);
	if ( !map || !*map ) {
		if ( !quakeLive ) {
			return;
		} else if ( singlePlayerCommand ) {
			Com_Printf( "%s <mapname>\n", cmd );
		} else {
			SV_FactoryPrintMapUsage( cmd );
		}
		return;
	}
	// Enemy Territory enables its single-player map commands only when its
	// gameinfo.dat does, and retail's does not.
	if ( singlePlayerCommand && Com_GameIsWolfET() && !SV_WolfETSinglePlayerEnabled() ) {
		Com_Printf( "Single Player is not enabled.\n" );
		return;
	}
	if ( Com_GameIsRTCWSP() && Q_stricmp(COM_GetExtension(map), "svg") == 0 ) {
		if ( !fntech3::rtcw::savegame::Filename(map, saveFilename) ||
			!SV_RTCWReadSaveHeader(saveFilename, saveHeader) ) return;
		map = saveHeader.map;
	}
	// Retail enforces the factory argument threshold before touching the BSP.
	// An explicitly present empty argv[2] still passes this preflight and is
	// subsequently treated as an invalid exact factory when no selection exists.
	if ( quakeLive && !singlePlayerCommand && Cmd_Argc() < 3
		&& !SV_FactoryHasActive() ) {
		SV_FactoryPrintMapUsage( cmd );
		return;
	}

	// make sure the level exists before trying to change, so that
	// a typo at the server console won't end the game
	Com_sprintf( expanded.data(), SV_ArraySize(expanded), "maps/%s.bsp", map );
	// bypass pure check so we can open downloaded map
	FS_BypassPure();
	const auto restorePure = SV_MakeScopeExit( []() { FS_RestorePure(); } );
	len = FS_FOpenFileRead( expanded.data(), nullptr, qfalse );
	if ( len == -1 ) {
		Com_Printf( "Can't find map %s\n", expanded.data() );
		return;
	}
	if ( quakeLive && com_sv_running->integer
		&& SV_HandleQuitOnExitLevel( map ) ) {
		return;
	}

	// force latched values to get set
	const auto &serverPolicy = Com_GameProfile()->server;
	Cvar_Get ("g_gametype", serverPolicy.gameType, CVAR_SERVERINFO | CVAR_USERINFO | CVAR_LATCH );
	if ( serverPolicy.mapSkill ) {
		Cvar_Get( "g_gameskill", serverPolicy.mapSkill, CVAR_SERVERINFO | CVAR_LATCH );
	}
	if ( Com_GameIsRTCWSP() ) Cvar_Get( "g_episode", "0", CVAR_SERVERINFO | CVAR_LATCH );
	if ( quakeLive ) {
		(void)Cvar_Get( "g_ammoPack", "0", CVAR_LATCH );
	}

	if ( singlePlayerCommand ) {
		// The inherited single-player aliases do not select a retail factory.
		// Drop any previous factory snapshots before applying their SP cvars so
		// overrides cannot leak into this path or be reused by a later map call.
		if ( quakeLive ) {
			SV_FactoryDeactivate();
		}
		if ( Com_GameIsWolfET() ) {
			Cvar_SetIntegerValue( "g_gametype", SV_WolfETDefaultSinglePlayerGameType() );
		} else if ( Com_GameIsMOHAA() ) {
			// Medal of Honor's single player is game type 0 (its briefing
			// maps start the campaign with `spmap`); Quake III's 2 is its
			// team match.
			Cvar_Set( "g_gametype", serverPolicy.gameType );
		} else if ( Com_GameIsJKAMP() ) {
			// Raven's Jedi games number single player after their own modes,
			// where Quake III's 2 is Jedi Master (SV_GameTypeIsSinglePlayer).
			Cvar_SetIntegerValue( "g_gametype", SV_JKA_GT_SINGLE_PLAYER );
		} else if ( Com_GameIsJK2MP() ) {
			Cvar_SetIntegerValue( "g_gametype", SV_JK2_GT_SINGLE_PLAYER );
		} else {
			Cvar_SetIntegerValue( "g_gametype", GT_SINGLE_PLAYER );
		}
		Cvar_Set( "g_doWarmup", "0" );
		// Holomatch's single-player ladder runs with no Expansion Pack mode.
		SV_STVEFClearModes();
		// may not set sv_maxclients directly, always set latched
		Cvar_SetLatched( "sv_maxclients", serverPolicy.singlePlayerCommandClients );
		cmd += 2;
		if (!Q_stricmp( cmd, "devmap" ) ) {
			cheat = true;
		} else {
			cheat = false;
		}
		killBots = true;
	}
	else {
		if ( !Q_stricmp( cmd, "devmap" ) ) {
			cheat = true;
		} else {
			cheat = false;
		}
		// Quake 3 turns its single-player game type into a free-for-all
		// for an ordinary map command. Enemy Territory numbers its game
		// types differently and changes none of them here: its game reloads
		// a map in Objective, type 2, by this very command.
		// Medal of Honor's 2 is team match and its `map` keeps whatever
		// game type is set, single player included. Jedi Outcast turns its
		// own single player, 4, into a free-for-all (jk2mp.exe 1.02a
		// 0x447a2c); Jedi Academy's `map` keeps every game type (jamp.exe
		// 1.01 SV_Map_f, 0x4560b0), where this had made Jedi Master, 2,
		// into a free-for-all.
		if( !Com_GameIsWolfET() && !Com_GameIsMOHAA() && !Com_GameIsJKAMP()
			&& SV_GameTypeIsSinglePlayer( sv_gametype->integer ) ) {
			Cvar_SetIntegerValue( "g_gametype", GT_FFA );
			killBots = true;
		} else {
			killBots = false;
		}
	}

	// Retail QL's map command owns factory selection.  Keep the inherited
	// single-player aliases intact, but require/reuse a factory for map and
	// devmap before any level state is torn down.
	if ( quakeLive && !singlePlayerCommand &&
		!SV_FactoryPrepareMap( map, Cmd_Argv( 2 ), SV_QBool( Cmd_Argc() >= 3 ),
			SV_QBool( cheat ) ) ) {
		return;
	}

	// save the map name here cause on a map restart we reload the q3config.cfg
	// and thus nuke the arguments of the map command
	Q_strncpyz( mapname.data(), map, SV_ArraySize(mapname) );

	// start up the map
	if ( Com_GameIsRTCWSP() ) {
		// The native cgame publishes this value back into CS_FOGVARS at its
		// first snapshot. Empty fog causes the original game to write a zero
		// length followed by "none", misaligning the rest of its save stream.
		Cvar_Set("r_mapFogColor", "0");
		Cvar_Set("r_waterFogColor", "0");
		Cvar_Set("r_savegameFogColor", "0");
		if ( saveFilename[0] ) {
			if ( !SV_RTCWStageSave(saveFilename, saveHeader) ) {
				Com_Printf("Cannot stage RTCW save %s; previous save retained.\n", saveFilename); return;
			}
			Cvar_Set("savegame_filename", saveFilename);
			Cvar_Set("savegame_loading", "1");
		} else SV_RTCWClearSaveRequest();
	}
	if ( Com_GameIsRTCWMP() ) {
		Cvar_SetIntegerValue("gamestate", FNTECH3_RTCW_MP_GS_INITIALIZE);
		Cvar_Set("sv_serverRestarting", "0");
	}
	// Enemy Territory begins every map, including the one its game reloads
	// by this command, from a fresh match state, round and time limit.
	if ( Com_GameIsWolfET() ) {
		Cvar_SetIntegerValue("gamestate", FNTECH3_RTCW_MP_GS_INITIALIZE);
		Cvar_Set("g_currentRound", "0");
		Cvar_Set("g_nextTimeLimit", "0");
	}
	// Medal of Honor's `cheats` is set before the game starts: on for a
	// one-player server or a devmap, off for any other map, and left as it is
	// in developer mode (OpenMoHAA's SV_Map_f [O]). The 1.00 game refuses
	// every cheat while `thereisnomonkey` is 0 all the same, and clears
	// `cheats` then (gamex86.dll 0x310cfe61..0x310cfeb0).
	if ( Com_GameIsMOHAA() && !( com_developer && com_developer->integer ) ) {
		Cvar_Set( "cheats", ( cheat || sv_maxclients->integer == 1 ) ? "1" : "0" );
	}
	SV_SpawnServer( mapname.data(), SV_QBool( killBots ) );

	// set the cheat value
	// if the level was started with "map <levelname>", then
	// cheats will not be allowed.  If started with "devmap <levelname>"
	// then cheats will be allowed
	if ( cheat ) {
		Cvar_Set( "sv_cheats", "1" );
	} else {
		Cvar_Set( "sv_cheats", "0" );
	}

#ifndef DEDICATED
	// Quake Live's local match runs as single player (the FnQL baseline). In
	// the other profiles a client's map is a listen server that answers info
	// and challenge queries, as retail's SV_Map_f leaves it: setting this
	// flag would turn away every remote client.
	if ( quakeLive && !com_dedicated->integer && !singlePlayerCommand ) {
		Cvar_Set( "ui_singlePlayerActive", "1" );
		Cvar_Set( "ui_priv", "3" );
	}
#endif
}


/*
================
SV_MapRestart_f

Completely restarts a level, but doesn't send a new gamestate to the clients.
This allows fair starts with variable load times.
================
*/
static void SV_MapRestart_f( void ) {
	int			delay;

	// make sure we aren't restarting twice in the same frame
	if ( com_frameTime == sv.restartedServerId ) {
		return;
	}

	// make sure server is running
	if ( !com_sv_running->integer ) {
		Com_Printf( "Server is not running.\n" );
		return;
	}

	// Enemy Territory lets a restart through while a delayed one is pending:
	// et.exe parses the delay (0x43fa53) straight after its running check,
	// where WolfMP.exe refuses (0x432eea). A new delay replaces the pending
	// time and CS_WARMUP; an immediate restart leaves it to fire later.
	if ( sv.restartTime != 0 && !Com_GameIsWolfET() ) {
		return;
	}

	if ( Com_GameIsQuakeLive()
		&& SV_HandleQuitOnExitLevel( "map_restart" ) ) {
		return;
	}

	// RTCW MP keeps g_gametype to a type the map allows, before any delay, as
	// WolfMP.exe does at 0x432ef6..0x432f7c. Getting the cvar takes a type the
	// console latched; retail gets it again after forcing the replacement.
	if ( Com_GameIsRTCWMP() ) {
		const char *defaultType = Com_GameProfile()->server.gameType;
		sv_gametype = Cvar_Get( "g_gametype", defaultType, CVAR_SERVERINFO | CVAR_LATCH );
		int allowed;
		if ( fntech3::rtcw::MapForbidsGameType( sv_gametype->integer, SV_RTCWMPWorldflags(), allowed ) ) {
			Cvar_SetIntegerValue( "g_gametype", allowed );
			sv_gametype = Cvar_Get( "g_gametype", defaultType, CVAR_SERVERINFO | CVAR_LATCH );
		}
	}

	if ( Cmd_Argc() > 1 ) {
		delay = SV_ParseInt( Cmd_Argv(1) );
	} else {
		delay = Com_GameIsRTCW() || Com_GameIsWolfET() || Com_GameIsQuake3IHV() ? 0 : 5;
	}
	if ( Com_GameIsRTCWSP() && Cvar_VariableIntegerValue("savegame_loading") ) delay = 0;

	if ( delay != 0 && ( Com_GameIsRTCWMP() || Com_GameIsWolfET() || Cvar_VariableIntegerValue( "g_doWarmup" ) == 0 ) ) {
		sv.restartTime = sv.time + delay * 1000;
		if ( sv.restartTime == 0 ) {
			sv.restartTime = 1;
		}
		if ( !Com_GameIsQuake3IHV() ) SV_SetConfigstring( CS_WARMUP, va( "%i", sv.restartTime ) );
		return;
	}

	if ( Com_GameIsQuake3IHV() ) {
		// Native IHV has one level-spawn path. Reset its configstrings and
		// world links while retaining the game DLL and its game allocation.
		std::array<char, MAX_QPATH> mapname{};
		Q_strncpyz( mapname.data(), Cvar_VariableString( "mapname" ), SV_ArraySize(mapname) );
		SV_SpawnServer( mapname.data(), qfalse );
		return;
	}

	if ( !SV_RTCWRestoreSaveTime() ) return;
	if ( Com_GameIsRTCWMP() ) {
		int nextState;
		const int requested = Cmd_Argc() > 2 ? SV_ParseInt(Cmd_Argv(2)) : FNTECH3_RTCW_MP_GS_PLAYING;
		if ( !fntech3::rtcw::RestartMatchState(Cvar_VariableIntegerValue("gamestate"), requested,
			Cvar_VariableIntegerValue("g_noTeamSwitching") != 0, nextState) ) return;
		Cvar_SetIntegerValue("gamestate", nextState);
	}
	if ( Com_GameIsWolfET() ) {
		const int gameType = sv_gametype->integer;
		const bool solo = SV_GameTypeIsSinglePlayer( gameType ) || SV_WolfETGameTypeIsCoop( gameType );
		const int requested = !solo && Cmd_Argc() > 2 ? SV_ParseInt(Cmd_Argv(2)) : FNTECH3_RTCW_MP_GS_PLAYING;
		int nextState;
		if ( !fntech3::rtcw::RestartMatchState(Cvar_VariableIntegerValue("gamestate"), requested,
			false, nextState, !solo) ) return;
		Cvar_SetIntegerValue("gamestate", nextState);
	}

	// Holomatch also reloads the map when an Expansion Pack mode changed.
	// RTCW MP and Enemy Territory reload it only for a new client count, as
	// WolfMP.exe (0x433016) and et.exe (0x43faf6) do. Their game restarts in
	// place in a new game type or pure mode.
	const bool clientCountOnly = Com_GameIsRTCWMP() || Com_GameIsWolfET();
	if ( !Com_GameIsQuakeLive()
		&& ( sv_maxclients->modified || ( !clientCountOnly && ( sv_gametype->modified
			|| sv_pure->modified || SV_STVEFModesModified() ) ) ) ) {
		std::array<char, MAX_QPATH> mapname{};

		Com_Printf( clientCountOnly ? "sv_maxclients variable change -- restarting.\n"
			: "variable change -- restarting.\n" );
		Q_strncpyz( mapname.data(), Cvar_VariableString( "mapname" ),
			SV_ArraySize(mapname) );
		SV_SpawnServer( mapname.data(), qfalse );
		return;
	}

	// Retail QL skips these transition checks once qagame marks g_restarted.
	// On a normal restart, ammo-pack or gametype changes require a full spawn,
	// while a max-client change resizes the retained client array in place.
	if ( Com_GameIsQuakeLive()
		&& Cvar_VariableValue( "g_restarted" ) == 0.0f ) {
		if ( ( sv_ammoPack && sv_ammoPack->modified ) || sv_gametype->modified ) {
			std::array<char, MAX_QPATH> mapname{};

			Com_Printf( "variable change -- restarting.\n" );
			Q_strncpyz( mapname.data(), Cvar_VariableString( "mapname" ),
				SV_ArraySize(mapname) );
			SV_SpawnServer( mapname.data(), qfalse );
			return;
		}
		if ( sv_maxclients->modified ) {
			SV_ChangeMaxClients();
		}
	}

	// toggle the server bit so clients can detect that a
	// map_restart has happened
	svs.snapFlagServerBit ^= SNAPFLAG_SERVERCOUNT;

	// generate a new restartedServerid
	sv.restartedServerId = com_frameTime;

	// if a map_restart occurs while a client is changing maps, we need
	// to give them the correct time so that when they finish loading
	// they don't violate the backwards time check in cl_cgame.c
	// Q3 stored sv.restartTime, which its refusal above kept zero. ET's can be
	// a pending restart's time, and snapshots add this offset to sv.time.
	for ( client_t &client : SV_Clients() ) {
		if ( client.state == CS_PRIMED ) {
			client.oldServerTime = 0;
		}
	}

	// reset all the vm data in place without changing memory allocation
	// note that we do NOT set sv.state = SS_LOADING, so configstrings that
	// had been changed from their default values will generate broadcast updates
	sv.state = SS_LOADING;
	sv.restarting = qtrue;

	// make sure that level time is not zero
	//sv.time = sv.time ? sv.time : 8;

	if ( Com_GameIsRTCWMP() || Com_GameIsWolfET() ) Cvar_Set("sv_serverRestarting", "1");
	SV_RestartGameProgs();
	// A saved SP level can rewind game time. Rebase the retained navigation
	// clock and initialize the reloaded game's bot scheduler before cast
	// entities are spawned by the settling frames.
	if ( Com_GameIsRTCWSP() ) SV_BotFrame(sv.time);

	// run a few frames to allow everything to settle
	// Each runs at the current time before the time advances, as in every
	// retail engine, not after advancing it as in Quake3e: the game's first
	// frame runs at the time GAME_INIT was given. Quake III 1.32 (quake3.exe
	// 0x432ab1..0x432add), Quake Live (quakelive_steam.exe 0x4de8ae..0x4de8e7),
	// RTCW (WolfSP.exe 0x430993..0x4309bf, WolfMP.exe 0x4330ad..0x4330da),
	// Holomatch (stvoyHM.exe 0x42e796..0x42e7c2) and the Jedi multiplayer
	// engines (jamp.exe 0x456396..0x4563c6, jk2mp.exe 0x447c03..0x447c2f) run
	// three and begin the clients below 200 ms after the level's start. Enemy
	// Territory runs six (et.exe 0x43fc3b..0x43fc63, etded.exe
	// 0x4260bd..0x4260e5) and begins them 500 ms after it. The single-player
	// engines have no map_restart; their map loads settle the same way.
	const bool wolfETFrames = Com_GameIsWolfET();
	for ( int ignored : SV_Indices( wolfETFrames ? 6 : 3 ) )
	{
		(void)ignored;
		Cbuf_Wait();
		VM_Call( gvm, 1, GAME_RUN_FRAME, sv.time );
		sv.time += 100;
	}

	sv.state = SS_GAME;
	sv.restarting = qfalse;

	// connect and begin all the clients
	for ( SV_ClientSlot slot : SV_ClientSlots() ) {
		// send the new gamestate to all connected clients
		if ( slot.client.state < CS_CONNECTED ) {
			continue;
		}

		const bool isBot = slot.client.netchan.remoteAddress.type == NA_BOT;

		// Enemy Territory carries no bot across a restart in a single-player
		// or co-op game type (et.exe 0x43fca1..0x43fcbe, etded.exe
		// 0x426121..0x42613e): no command, no reconnection, no entry.
		if ( isBot && Com_GameIsWolfET() && ( SV_GameTypeIsSinglePlayer( sv_gametype->integer )
			|| SV_WolfETGameTypeIsCoop( sv_gametype->integer ) ) ) {
			continue;
		}

		// add the map_restart command
		SV_AddServerCommand( &slot.client, "map_restart\n" );

		// connect the client again, without the firstTime flag
		const char *denied = static_cast<const char *>( GVM_ArgPtr( VM_Call( gvm, 3, GAME_CLIENT_CONNECT, slot.index, qfalse, isBot ) ) );
		if ( denied ) {
			// this generally shouldn't happen, because the client
			// was connected before the level change
			SV_DropClient( &slot.client, denied );
			Com_Printf( "SV_MapRestart_f(%d): dropped client %i - denied!\n", delay, slot.index );
			continue;
		}

		// et.exe also makes a connected or loading client active and enters
		// it here (0x43fd1f..0x43fd2d), relying on its serverId check to send
		// a gamestate to any client still on an older one. This server sends
		// or resends a gamestate only before a client enters, and entering
		// marks it acknowledged, so such a client still enters on its first
		// command.
		if ( slot.client.state == CS_ACTIVE ) {
			// A restart brings the client back with no new command of its own,
			// so the last command it sent stays the spawn's view-angle anchor.
			SV_ClientEnterWorld( &slot.client, nullptr );
		}
	}

	// run another frame to allow things to look at all the players, at the
	// current time too (quake3.exe 0x432b9e..0x432bbf, quakelive_steam.exe
	// 0x4de9f2..0x4dea0e, WolfSP.exe 0x430a80..0x430aa1, WolfMP.exe
	// 0x433192..0x4331b9, stvoyHM.exe 0x42e85f..0x42e880, jamp.exe
	// 0x4564ae..0x4564cf, jk2mp.exe 0x447cf2..0x447d14, et.exe
	// 0x43fd57..0x43fd65)
	Cbuf_Wait();
	VM_Call( gvm, 1, GAME_RUN_FRAME, sv.time );
	sv.time += 100;
	svs.time += 100;
	if ( Com_GameIsRTCWMP() || Com_GameIsWolfET() ) Cvar_Set("sv_serverRestarting", "0");

	// cycle per-match demo recordings: finalize the old demo and start a new one
	if ( sv_autoRecordDemos && sv_autoRecordDemos->integer >= 3 ) {
		for ( client_t &client : SV_Clients() ) {
			if ( client.state < CS_CONNECTED ) {
				continue;
			}
			SV_StopDemoRecord( &client, qfalse );
			SV_StartDemoRecord( &client );
		}
	}

	for ( client_t &client : SV_Clients() ) {
		if ( client.state >= CS_PRIMED ) {
			// accept usercmds starting from current server time only
			// to emulate original behavior which dropped pre-restart commands via serverid check
			// Only the gate moves. A client's commands are dropped until it
			// catches up to this time, which for any non-zero ping spans the
			// frames in which the game respawns everyone, so the angles left
			// here are the anchor trap_GetUsercmd() hands those spawns.
			if ( Com_GameIsQuakeLive() ) {
				client.lastUsercmd.serverTime =
					fnql::net::CounterSubtract( sv.time, 1u );
			} else {
				client.lastUsercmd = {};
				client.lastUsercmd.serverTime = sv.time - 1;
			}
		}
	}

	if ( Com_GameIsQuakeLive() ) {
		SV_MapPoolRefreshCvars();
	}
}


static void SV_LoadGame_f() {
	if ( !Com_GameIsRTCWSP() || Cvar_VariableIntegerValue("savegame_loading") ||
		Cvar_VariableIntegerValue("g_reloading") ) return;
	char filename[fntech3::rtcw::savegame::PathBytes]{};
	fntech3::rtcw::savegame::Header header;
	if ( Cmd_Argc() != 2 || !fntech3::rtcw::savegame::Filename(Cmd_Argv(1), filename) ) {
		Com_Printf("loadgame <save name>: use a name within the save directory.\n"); return;
	}
	if ( !SV_RTCWReadSaveHeader(filename, header) ) return;
	if ( com_sv_running->integer && com_frameTime != sv.restartedServerId &&
		!sv.restartTime && Q_stricmp(header.map, Cvar_VariableString("mapname")) == 0 ) {
		if ( !SV_RTCWStageSave(filename, header) ) {
			Com_Printf("Cannot stage RTCW save %s; previous save retained.\n", filename); return;
		}
		Cvar_Set("savegame_filename", filename);
		Cvar_Set("savegame_loading", "2");
		SV_MapRestart_f();
	} else {
		Cbuf_AddText(va("%s \"%s\"\n", Cvar_VariableIntegerValue("sv_cheats") ? "spdevmap" : "spmap", filename));
	}
}

/*
==================
SV_Kick_f

Kick a user off of the server  FIXME: move to game
==================
*/
static void SV_Kick_f( void ) {
	client_t	*cl;

	// make sure server is running
	if ( !com_sv_running->integer ) {
		Com_Printf( "Server is not running.\n" );
		return;
	}

	if ( Cmd_Argc() != 2 ) {
		Com_Printf ("Usage: kick <player name>\nkick all = kick everyone\nkick allbots = kick all bots\n");
		return;
	}

	cl = SV_GetPlayerByHandle();
	if ( !cl ) {
		if ( !Q_stricmp( Cmd_Argv( 1 ), "all" ) ) {
			for ( client_t &client : SV_Clients() ) {
				if ( client.state < CS_CONNECTED ) {
					continue;
				}
				if ( client.netchan.remoteAddress.type == NA_LOOPBACK ) {
					continue;
				}
				SV_DropClient( &client, "was kicked" );
				client.lastPacketTime = svs.time;	// in case there is a funny zombie
			}
		}
		else if ( !Q_stricmp( Cmd_Argv( 1 ), "allbots" ) ) {
			for ( client_t &client : SV_Clients() ) {
				if ( client.state < CS_CONNECTED ) {
					continue;
				}
				if ( client.netchan.remoteAddress.type != NA_BOT ) {
					continue;
				}
				SV_DropClient( &client, "was kicked" );
				client.lastPacketTime = svs.time;	// in case there is a funny zombie
			}
		}
		return;
	}
	if ( cl->netchan.remoteAddress.type == NA_LOOPBACK ) {
		Com_Printf( "Cannot kick host player\n" );
		return;
	}

	SV_DropClient( cl, "was kicked" );
	cl->lastPacketTime = svs.time;	// in case there is a funny zombie
}

/*
==================
SV_KickBots_f

Kick all bots off of the server
==================
*/
static void SV_KickBots_f( void ) {
	client_t	*cl;
	int			i;

	// make sure server is running
	if ( !com_sv_running->integer ) {
		Com_Printf("Server is not running.\n");
		return;
	}

	for( i = 0, cl = svs.clients; i < sv.maxclients; i++, cl++ ) {
		if ( cl->state < CS_CONNECTED ) {
			continue;
		}

		if ( cl->netchan.remoteAddress.type != NA_BOT ) {
			continue;
		}

		SV_DropClient( cl, "was kicked" );
		cl->lastPacketTime = svs.time; // in case there is a funny zombie
	}
}
/*
==================
SV_KickAll_f

Kick all users off of the server
==================
*/
static void SV_KickAll_f( void ) {
	client_t *cl;
	int i;

	// make sure server is running
	if ( !com_sv_running->integer ) {
		Com_Printf( "Server is not running.\n" );
		return;
	}

	for( i = 0, cl = svs.clients; i < sv.maxclients; i++, cl++ ) {
		if ( cl->state < CS_CONNECTED ) {
			continue;
		}

		if ( cl->netchan.remoteAddress.type == NA_LOOPBACK ) {
			continue;
		}

		SV_DropClient( cl, "was kicked" );
		cl->lastPacketTime = svs.time; // in case there is a funny zombie
	}
}

/*
==================
SV_KickNum_f

Kick a user off of the server
==================
*/
static void SV_KickNum_f( void ) {
	client_t	*cl;

	// make sure server is running
	if ( !com_sv_running->integer ) {
		Com_Printf( "Server is not running.\n" );
		return;
	}

	if ( Cmd_Argc() != 2 ) {
		Com_Printf ("Usage: %s <client number>\n", Cmd_Argv(0));
		return;
	}

	cl = SV_GetPlayerByNum();
	if ( !cl ) {
		return;
	}
	if ( cl->netchan.remoteAddress.type == NA_LOOPBACK ) {
		Com_Printf("Cannot kick host player\n");
		return;
	}

	SV_DropClient( cl, "was kicked" );
	cl->lastPacketTime = svs.time;	// in case there is a funny zombie
}

#ifndef STANDALONE
// these functions require the auth server which of course is not available anymore for stand-alone games.

#ifdef USE_BANS
static bool SV_ResolveAuthorizeAddress( void ) {
	// look up the authorize server's IP
	if ( !svs.authorizeAddress.ipv._4[0] && svs.authorizeAddress.type != NA_BAD ) {
		Com_Printf( "Resolving %s\n", AUTHORIZE_SERVER_NAME );
		if ( !NET_StringToAdr( AUTHORIZE_SERVER_NAME, &svs.authorizeAddress, NA_IP ) ) {
			Com_Printf( "Couldn't resolve address\n" );
			return false;
		}
		svs.authorizeAddress.port = BigShort( PORT_AUTHORIZE );
		Com_Printf( "%s resolved to %i.%i.%i.%i:%i\n", AUTHORIZE_SERVER_NAME,
			svs.authorizeAddress.ipv._4[0], svs.authorizeAddress.ipv._4[1],
			svs.authorizeAddress.ipv._4[2], svs.authorizeAddress.ipv._4[3],
			BigShort( svs.authorizeAddress.port ) );
	}

	return svs.authorizeAddress.type != NA_BAD;
}

static void SV_BanClientThroughAuthorize( const client_t *cl ) {
	if ( !SV_ResolveAuthorizeAddress() ) {
		return;
	}

	NET_OutOfBandPrint( NS_SERVER, &svs.authorizeAddress,
		"banUser %i.%i.%i.%i", cl->netchan.remoteAddress.ipv._4[0], cl->netchan.remoteAddress.ipv._4[1],
							   cl->netchan.remoteAddress.ipv._4[2], cl->netchan.remoteAddress.ipv._4[3] );
	Com_Printf("%s was banned from coming back\n", cl->name);
}

/*
==================
SV_Ban_f

Ban a user from being able to play on this server through the auth
server
==================
*/
static void SV_Ban_f( void ) {
	client_t	*cl;

	// make sure server is running
	if ( !com_sv_running->integer ) {
		Com_Printf( "Server is not running.\n" );
		return;
	}

	if ( Cmd_Argc() != 2 ) {
		Com_Printf ("Usage: banUser <player name>\n");
		return;
	}

	cl = SV_GetPlayerByHandle();

	if (!cl) {
		return;
	}

	if( cl->netchan.remoteAddress.type == NA_LOOPBACK ) {
		Com_Printf("Cannot kick host player\n");
		return;
	}

	SV_BanClientThroughAuthorize( cl );
}

/*
==================
SV_BanNum_f

Ban a user from being able to play on this server through the auth
server
==================
*/
static void SV_BanNum_f( void ) {
	client_t	*cl;

	// make sure server is running
	if ( !com_sv_running->integer ) {
		Com_Printf( "Server is not running.\n" );
		return;
	}

	if ( Cmd_Argc() != 2 ) {
		Com_Printf ("Usage: banClient <client number>\n");
		return;
	}

	cl = SV_GetPlayerByNum();
	if ( !cl ) {
		return;
	}
	if( cl->netchan.remoteAddress.type == NA_LOOPBACK ) {
		Com_Printf("Cannot kick host player\n");
		return;
	}

	SV_BanClientThroughAuthorize( cl );
}

#endif // USE_BANS
#endif // !COM_STANDALONE

#ifdef USE_BANS
static int SV_DefaultSubnetForAddress( const netadr_t &address );
static int SV_NormalizeSubnetForAddress( const netadr_t &address, int subnet );

/*
==================
SV_RehashBans_f

Load saved bans from file.
==================
*/
static void SV_RehashBans_f(void)
{
	int index, filelen;
	fileHandle_t readfrom;
	char *textbuf, *curpos, *maskpos, *newlinepos;
	const char *endpos;
	std::array<char, MAX_QPATH> filepath{};
	
	// make sure server is running
	if ( !com_sv_running->integer ) {
		return;
	}
	
	serverBansCount = 0;
	
	if(!sv_banFile->string || !*sv_banFile->string)
		return;

	Com_sprintf( filepath.data(), SV_ArraySize(filepath), "%s/%s", FS_GetCurrentGameDir(), sv_banFile->string );

	if((filelen = FS_SV_FOpenFileRead(filepath.data(), &readfrom)) >= 0)
	{
		const auto closeRead = SV_MakeScopeExit( [&]() { SV_CloseFileHandle( readfrom ); } );

		if(filelen < 2)
		{
			// Don't bother if file is too short.
			return;
		}

		curpos = textbuf = SV_ZMallocArray<char>( filelen );
		const auto freeText = SV_MakeScopeExit( [&]() { SV_ZFree( textbuf ); } );
		
		filelen = FS_Read(textbuf, filelen, readfrom);
		
		endpos = textbuf + filelen;
		
		index = 0;
		while(index < SERVER_MAXBANS && curpos + 2 < endpos)
		{
			// find the end of the address string
			for(maskpos = curpos + 2; maskpos < endpos && *maskpos != ' '; maskpos++);
			
			if(maskpos + 1 >= endpos)
				break;

			*maskpos = '\0';
			maskpos++;
			
			// find the end of the subnet specifier
			for(newlinepos = maskpos; newlinepos < endpos && *newlinepos != '\n'; newlinepos++);
			
			if(newlinepos >= endpos)
				break;
			
			*newlinepos = '\0';
			
			serverBan_t &ban = SV_BanForIndex( index );

			if(NET_StringToAdr(curpos + 2, &ban.ip, NA_UNSPEC))
			{
				ban.isexception = SV_QBool( curpos[0] != '0' );
				ban.subnet = SV_ParseInt(maskpos);
				
				ban.subnet = SV_NormalizeSubnetForAddress( ban.ip, ban.subnet );
			}
			
			curpos = newlinepos + 1;
			index++;
		}
			
		serverBansCount = index;
	}
}

/*
==================
SV_WriteBans

Save bans to file.
==================
*/
static void SV_WriteBans(void)
{
	fileHandle_t writeto;
	std::array<char, MAX_QPATH> filepath{};
	
	if(!sv_banFile->string || !*sv_banFile->string)
		return;
	
	Com_sprintf( filepath.data(), SV_ArraySize(filepath), "%s/%s", FS_GetCurrentGameDir(), sv_banFile->string );

	if((writeto = FS_SV_FOpenFileWrite(filepath.data())))
	{
		const auto closeWrite = SV_MakeScopeExit( [&]() { SV_CloseFileHandle( writeto ); } );
		std::array<char, 128> writebuf{};
		
		for( const serverBan_t &curban : SV_Bans() )
		{
			Com_sprintf( writebuf.data(), SV_ArraySize(writebuf), "%d %s %d\n",
				    curban.isexception, NET_AdrToString(&curban.ip), curban.subnet);
			FS_Write( writebuf.data(), strlen( writebuf.data() ), writeto );
		}
	}
}

/*
==================
SV_DelBanEntryFromList

Remove a ban or an exception from the list.
==================
*/

static bool SV_DelBanEntryFromList(int index)
{
	if(index == serverBansCount - 1)
		serverBansCount--;
	else if(index < SV_ArraySize(serverBans) - 1)
	{
		for ( int next = index + 1; next < serverBansCount; ++next ) {
			SV_BanForIndex( next - 1 ) = SV_BanForIndex( next );
		}
		serverBansCount--;
	}
	else
		return true;

	return false;
}

static int SV_DefaultSubnetForAddress( const netadr_t &address )
{
#ifdef USE_IPV6
	return ( address.type == NA_IP6 ) ? 128 : 32;
#else
	return 32;
#endif
}

static int SV_NormalizeSubnetForAddress( const netadr_t &address, int subnet )
{
	const int defaultSubnet = SV_DefaultSubnetForAddress( address );
	return ( subnet < 1 || subnet > defaultSubnet ) ? defaultSubnet : subnet;
}

/*
==================
SV_ParseCIDRNotation

Parse a CIDR notation type string and return a netadr_t and suffix by reference
==================
*/

static bool SV_ParseCIDRNotation(netadr_t *dest, int *mask, const char *adrstr)
{
	std::array<char, NET_ADDRSTRMAXLEN> address{};
	char *suffix;

	Q_strncpyz( address.data(), adrstr, SV_ArraySize(address) );
	suffix = strchr(address.data(), '/');
	if(suffix)
	{
		*suffix = '\0';
		suffix++;
	}

	if(!NET_StringToAdr(address.data(), dest, NA_UNSPEC))
		return true;

	if(suffix)
	{
		*mask = SV_NormalizeSubnetForAddress( *dest, SV_ParseInt(suffix) );
	}
	else
		*mask = SV_DefaultSubnetForAddress( *dest );
	
	return false;
}

/*
==================
SV_AddBanToList

Ban a user from being able to play on this server based on his ip address.
==================
*/

static void SV_AddBanToList( bool isexception )
{
	const char *banstring;
	std::array<char, NET_ADDRSTRMAXLEN> addy2{};
	netadr_t ip;
	int index, argc, mask;

	// make sure server is running
	if ( !com_sv_running->integer ) {
		Com_Printf( "Server is not running.\n" );
		return;
	}

	argc = Cmd_Argc();
	
	if(argc < 2 || argc > 3)
	{
		Com_Printf ("Usage: %s (ip[/subnet] | clientnum [subnet])\n", Cmd_Argv(0));
		return;
	}

	if(serverBansCount >= SV_ArraySize(serverBans))
	{
		Com_Printf ("Error: Maximum number of bans/exceptions exceeded.\n");
		return;
	}

	banstring = Cmd_Argv(1);
	
	if(strchr(banstring, '.') || strchr(banstring, ':'))
	{
		// This is an ip address, not a client num.
		
		if(SV_ParseCIDRNotation(&ip, &mask, banstring))
		{
			Com_Printf("Error: Invalid address %s\n", banstring);
			return;
		}
	}
	else
	{
		client_t *cl;
		
		// client num.
		
		cl = SV_GetPlayerByNum();

		if(!cl)
		{
			Com_Printf("Error: Playernum %s does not exist.\n", Cmd_Argv(1));
			return;
		}
		
		ip = cl->netchan.remoteAddress;
		
		if(argc == 3)
		{
			mask = SV_NormalizeSubnetForAddress( ip, SV_ParseInt(Cmd_Argv(2)) );
		}
		else
			mask = SV_DefaultSubnetForAddress( ip );
	}

	if(ip.type != NA_IP && ip.type != NA_IP6)
	{
		Com_Printf("Error: Can ban players connected via the internet only.\n");
		return;
	}

	// first check whether a conflicting ban exists that would supersede the new one.
	for( const serverBan_t &curban : SV_Bans() )
	{
		if(curban.subnet <= mask)
		{
			if((curban.isexception || !isexception) && NET_CompareBaseAdrMask(&curban.ip, &ip, curban.subnet))
			{
				Q_strncpyz(addy2.data(), NET_AdrToString(&ip), SV_ArraySize(addy2));
				
				Com_Printf("Error: %s %s/%d supersedes %s %s/%d\n", curban.isexception ? "Exception" : "Ban",
					   NET_AdrToString(&curban.ip), curban.subnet,
					   isexception ? "exception" : "ban", addy2.data(), mask);
				return;
			}
		}
		if(curban.subnet >= mask)
		{
			if(!curban.isexception && isexception && NET_CompareBaseAdrMask(&curban.ip, &ip, mask))
			{
				Q_strncpyz(addy2.data(), NET_AdrToString(&curban.ip), SV_ArraySize(addy2));
			
				Com_Printf("Error: %s %s/%d supersedes already existing %s %s/%d\n", isexception ? "Exception" : "Ban",
					   NET_AdrToString(&ip), mask,
					   curban.isexception ? "exception" : "ban", addy2.data(), curban.subnet);
				return;
			}
		}
	}

	// now delete bans that are superseded by the new one
	index = 0;
	while(index < serverBansCount)
	{
		const serverBan_t &curban = SV_BanForIndex( index );
		
		if(curban.subnet > mask && (!curban.isexception || isexception) && NET_CompareBaseAdrMask(&curban.ip, &ip, mask))
			SV_DelBanEntryFromList(index);
		else
			index++;
	}

	serverBan_t &newBan = SV_BanForIndex( serverBansCount );
	newBan.ip = ip;
	newBan.subnet = mask;
	newBan.isexception = SV_QBool( isexception );
	
	serverBansCount++;
	
	SV_WriteBans();

	Com_Printf("Added %s: %s/%d\n", isexception ? "ban exception" : "ban",
		   NET_AdrToString(&ip), mask);
}

/*
==================
SV_DelBanFromList

Remove a ban or an exception from the list.
==================
*/

static void SV_DelBanFromList( bool isexception )
{
	int index, count = 0, todel, mask;
	netadr_t ip;
	const char *banstring;
	
	// make sure server is running
	if ( !com_sv_running->integer ) {
		Com_Printf( "Server is not running.\n" );
		return;
	}
	
	if(Cmd_Argc() != 2)
	{
		Com_Printf ("Usage: %s (ip[/subnet] | num)\n", Cmd_Argv(0));
		return;
	}

	banstring = Cmd_Argv(1);
	
	if(strchr(banstring, '.') || strchr(banstring, ':'))
	{
		if(SV_ParseCIDRNotation(&ip, &mask, banstring))
		{
			Com_Printf("Error: Invalid address %s\n", banstring);
			return;
		}
		
		index = 0;
		
		while(index < serverBansCount)
		{
			serverBan_t &curban = SV_BanForIndex( index );
			
			if( SV_AsBool( curban.isexception ) == isexception		&&
			   curban.subnet >= mask 			&&
			   NET_CompareBaseAdrMask(&curban.ip, &ip, mask))
			{
				Com_Printf("Deleting %s %s/%d\n",
					   isexception ? "exception" : "ban",
					   NET_AdrToString(&curban.ip), curban.subnet);
					   
				SV_DelBanEntryFromList(index);
			}
			else
				index++;
		}
	}
	else
	{
		todel = SV_ParseInt(Cmd_Argv(1));

		if(todel < 1 || todel > serverBansCount)
		{
			Com_Printf("Error: Invalid ban number given\n");
			return;
		}
	
		for( SV_BanSlot slot : SV_BanSlots() )
		{
			if( SV_AsBool( slot.ban.isexception ) == isexception)
			{
				count++;
			
				if(count == todel)
				{
					Com_Printf("Deleting %s %s/%d\n",
					   isexception ? "exception" : "ban",
					   NET_AdrToString(&slot.ban.ip), slot.ban.subnet);

					SV_DelBanEntryFromList(slot.index);

					break;
				}
			}
		}
	}
	
	SV_WriteBans();
}


/*
==================
SV_ListBans_f

List all bans and exceptions on console
==================
*/

static void SV_ListBans_f(void)
{
	int count;

	// make sure server is running
	if ( !com_sv_running->integer ) {
		Com_Printf( "Server is not running.\n" );
		return;
	}
	
	// List all bans
	count = 0;
	for( const serverBan_t &ban : SV_Bans() )
	{
		if(!ban.isexception)
		{
			count++;

			Com_Printf("Ban #%d: %s/%d\n", count,
				    NET_AdrToString(&ban.ip), ban.subnet);
		}
	}
	// List all exceptions
	count = 0;
	for( const serverBan_t &ban : SV_Bans() )
	{
		if(ban.isexception)
		{
			count++;

			Com_Printf("Except #%d: %s/%d\n", count,
				    NET_AdrToString(&ban.ip), ban.subnet);
		}
	}
}

/*
==================
SV_FlushBans_f

Delete all bans and exceptions.
==================
*/

static void SV_FlushBans_f(void)
{
	// make sure server is running
	if ( !com_sv_running->integer ) {
		Com_Printf( "Server is not running.\n" );
		return;
	}

	serverBansCount = 0;
	
	// empty the ban file.
	SV_WriteBans();
	
	Com_Printf("All bans and exceptions have been deleted.\n");
}

static void SV_BanAddr_f(void)
{
	SV_AddBanToList( false );
}

static void SV_ExceptAddr_f(void)
{
	SV_AddBanToList( true );
}

static void SV_BanDel_f(void)
{
	SV_DelBanFromList( false );
}

static void SV_ExceptDel_f(void)
{
	SV_DelBanFromList( true );
}

#endif // USE_BANS

/*
** SV_Strlen -- skips color escape codes
*/
int SV_Strlen( const char *str ) {
	const char *s = str;
	int count = 0;

	while ( *s ) {
		if ( Q_IsColorString( s ) ) {
			s += 2;
		} else {
			count++;
			s++;
		}
	}

	return count;
}

static void SV_PrintRepeatedChar( char value, int count ) {
	for ( int ignored : SV_Indices( count ) ) {
		(void)ignored;
		Com_Printf( "%c", value );
	}
}

static unsigned long long SV_StatusClientSteamId( int clientNum ) {
	unsigned int low = 0;
	unsigned int high = 0;

	if ( !SV_ClientSteamId( clientNum, &low, &high ) ) {
		return 0ULL;
	}

	return ( static_cast<unsigned long long>( high ) << 32 ) | static_cast<unsigned long long>( low );
}


/*
================
SV_Status_f
================
*/
static void SV_Status_f( void ) {
	int l;
	const client_t *cl;
	playerState_t ps;
	const char *s;
	int max_namelength;
	int max_addrlength;
	unsigned long long steamIdValue;
	std::array<char, MAX_GAME_CLIENTS * MAX_NAME_LENGTH> names{};
	std::array<char, MAX_GAME_CLIENTS * 48> addrs{};
	std::array<const char *, MAX_GAME_CLIENTS> np{};
	std::array<const char *, MAX_GAME_CLIENTS> ap{};
	std::array<int, MAX_GAME_CLIENTS> nl{};
	std::array<int, MAX_GAME_CLIENTS> al{};
	char *nc;
	char *ac;

	// make sure server is running
	if ( !com_sv_running->integer ) {
		Com_Printf( "Server is not running.\n" );
		return;
	}

	max_namelength = 4; // strlen( "name" )
	max_addrlength = 7; // strlen( "address" )

	nc = names.data(); *nc = '\0';
	ac = addrs.data(); *ac = '\0';

	// first pass: save and determine max.lengths of name/address fields
	for ( SV_ClientSlot slot : SV_ClientSlots() )
	{
		client_t *slotClient = &slot.client;

		if ( slotClient->state == CS_FREE )
			continue;

		l = strlen( slotClient->name ) + 1;
		Q_strncpyz( nc, slotClient->name, SV_ArraySize(names) - static_cast<int>( nc - names.data() ) );
		np[ slot.index ] = nc; nc += l;			// name pointer in name buffer
		nl[ slot.index ] = SV_Strlen( slotClient->name );// name length without color sequences
		if ( nl[ slot.index ] > max_namelength )
			max_namelength = nl[ slot.index ];

		s = NET_AdrToString( &slotClient->netchan.remoteAddress );
		l = strlen( s ) + 1;
		Q_strncpyz( ac, s, SV_ArraySize(addrs) - static_cast<int>( ac - addrs.data() ) );
		ap[ slot.index ] = ac; ac += l;			// address pointer in address buffer
		al[ slot.index ] = l - 1;				// address length
		if ( al[ slot.index ] > max_addrlength )
			max_addrlength = al[ slot.index ];
	}

	Com_Printf( "map: %s\n", sv_mapname->string );
	if ( Cmd_Argc() > 1 && !Q_stricmp( Cmd_Argv( 1 ), "debug" ) ) {
		Com_Printf( "runtime: profile=%s serverTime=%d hostTime=%d residual=%d clients=%d entities=%d\n",
			Com_GameProfile()->key, sv.time, svs.time, sv.timeResidual,
			sv.maxclients, sv.num_entities );
	}

#if 0
	Com_Printf( "cl score ping name                        address                     rate\n" );
	Com_Printf( "-- ----- ---- --------------------------- --------------------------- -----\n" );
#else // variable-length fields
	Com_Printf( "cl score ping name" );
	SV_PrintRepeatedChar( ' ', max_namelength - 4 );
	Com_Printf( " address" );
	SV_PrintRepeatedChar( ' ', max_addrlength - 7 );
	Com_Printf( " rate steamid\n" );

	Com_Printf( "-- ----- ---- " );
	SV_PrintRepeatedChar( '-', max_namelength );
	Com_Printf( " " );
	SV_PrintRepeatedChar( '-', max_addrlength );
	Com_Printf( " ----- -----------------\n" );
#endif

	for ( SV_ClientSlot slot : SV_ClientSlots() )
	{
		cl = &slot.client;
		if ( cl->state == CS_FREE )
			continue;

		Com_Printf( "%2i ", slot.index ); // id
		if ( !SV_ReadGameClient( slot.index, &ps ) ) {
			Com_Error( ERR_DROP, "%s: invalid game client %i", __func__,
				slot.index );
			return;
		}
		Com_Printf( "%5i ", SV_GameGetClientScore( slot.index,
			ps.persistant[PERS_SCORE] ) );

		// ping/status
		if ( cl->state == CS_PRIMED )
			Com_Printf( " PRM " );
		else if ( cl->state == CS_CONNECTED )
			Com_Printf( " CON " );
		else if ( cl->state == CS_ZOMBIE )
			Com_Printf( " ZMB " );
		else
			Com_Printf( "%4i ", cl->ping < 999 ? cl->ping : 999 );
	
		// variable-length name field
		s = np[ slot.index ];
		Com_Printf( "%s", s );
		l = max_namelength - nl[ slot.index ];
		SV_PrintRepeatedChar( ' ', l );

		// variable-length address field
		s = ap[ slot.index ];
		Com_Printf( S_COLOR_WHITE " %s", s );
		l = max_addrlength - al[ slot.index ];
		SV_PrintRepeatedChar( ' ', l );

		// rate
		steamIdValue = SV_StatusClientSteamId( slot.index );
		Com_Printf( " %5i %llu\n", cl->rate, steamIdValue );
	}

	Com_Printf( "\n" );
}


/*
==================
SV_ConSay_f
==================
*/
static void SV_ConSay_f( void ) {
	char	*p;
	std::array<char, MAX_STRING_CHARS> text{};
	int		len;

	// make sure server is running
	if ( !com_sv_running->integer ) {
		Com_Printf( "Server is not running.\n" );
		return;
	}

	if ( Cmd_Argc () < 2 ) {
		return;
	}

	p = Cmd_ArgsFrom( 1 );
	len = static_cast<int>( strlen( p ) );

	if ( len > 1000 ) {
		return;
	}

	if ( *p == '"' ) {
		p[len-1] = '\0';
		p++;
	}

	Com_sprintf( text.data(), SV_ArraySize(text), "console: %s", p );

	SV_SendServerCommand( nullptr, "chat \"%s\"", text.data() );
}


/*
==================
SV_ConSvSay_f

Raven's engines name their console chat svsay and have no say:
stvoyHM.exe 1.20 and jk2mp.exe 1.02a send every client chat
"console: <text>\n" (0x42ecc0, 0x448580), jamp.exe 1.01 chat
"Server: <text>\n" (0x456c80). Each refuses on a server that is not
dedicated or not running.
==================
*/
static void SV_ConSvSay_f( void ) {
	char	*p;
	std::array<char, MAX_STRING_CHARS> text{};
	int		len;

	if ( !com_dedicated->integer ) {
		Com_Printf( "Server is not dedicated.\n" );
		return;
	}
	if ( !com_sv_running->integer ) {
		Com_Printf( "Server is not running.\n" );
		return;
	}

	if ( Cmd_Argc () < 2 ) {
		return;
	}

	p = Cmd_ArgsFrom( 1 );
	len = static_cast<int>( strlen( p ) );

	if ( len > 1000 ) {
		return;
	}

	if ( *p == '"' ) {
		p[len-1] = '\0';
		p++;
	}

	Com_sprintf( text.data(), SV_ArraySize(text), "%s%s", Com_GameIsJKAMP() ? "Server: " : "console: ", p );

	SV_SendServerCommand( nullptr, "chat \"%s\n\"", text.data() );
}


/*
==================
SV_ConTell_f
==================
*/
static void SV_ConTell_f( void ) {
	char	*p;
	std::array<char, MAX_STRING_CHARS> text{};
	client_t	*cl;
	int		len;

	// make sure server is running
	if ( !com_sv_running->integer ) {
		Com_Printf( "Server is not running.\n" );
		return;
	}

	if ( Cmd_Argc() < 3 ) {
		Com_Printf( "Usage: tell <client number> <text>\n" );
		return;
	}

	cl = SV_GetPlayerByNum();
	if ( !cl ) {
		return;
	}

	p = Cmd_ArgsFrom( 2 );
	len = static_cast<int>( strlen( p ) );

	if ( len > 1000 ) {
		return;
	}

	if ( *p == '"' ) {
		p[len-1] = '\0';
		p++;
	}

	Com_sprintf( text.data(), SV_ArraySize(text), S_COLOR_MAGENTA "console: %s", p );

	Com_Printf( "%s\n", text.data() );
	SV_SendServerCommand( cl, "chat \"%s\"", text.data() );
}


/*
==================
SV_Heartbeat_f

Also called by SV_DropClient, SV_DirectConnect, and SV_SpawnServer
==================
*/
void SV_Heartbeat_f( void ) {
	svs.nextHeartbeatTime = svs.time;
}


/*
===========
SV_Serverinfo_f

Examine the serverinfo string
===========
*/
static void SV_Serverinfo_f( void ) {
	const char *info;

	// make sure server is running
	if ( !com_sv_running->integer ) {
		Com_Printf( "Server is not running.\n" );
		return;
	}

	Com_Printf ("Server info settings:\n");
	info = sv.configstrings[ CS_SERVERINFO ];
	if ( info ) {
		Info_Print( info );
	}
}


/*
===========
SV_Systeminfo_f

Examine the systeminfo string
===========
*/
static void SV_Systeminfo_f( void ) {
	const char *info;
	// make sure server is running
	if ( !com_sv_running->integer ) {
		Com_Printf( "Server is not running.\n" );
		return;
	}
	Com_Printf( "System info settings:\n" );
	info = sv.configstrings[ CS_SYSTEMINFO ];
	if ( info ) {
		Info_Print( info );
	}
}


/*
===========
SV_DumpUser_f

Examine all a users info strings
===========
*/
static void SV_DumpUser_f( void ) {
	client_t	*cl;

	// make sure server is running
	if ( !com_sv_running->integer ) {
		Com_Printf( "Server is not running.\n" );
		return;
	}

	if ( Cmd_Argc() != 2 ) {
		Com_Printf ("Usage: dumpuser <userid>\n");
		return;
	}

	cl = SV_GetPlayerByHandle();
	if ( !cl ) {
		return;
	}

	Com_Printf( "userinfo\n" );
	Com_Printf( "--------\n" );
	Info_Print( cl->userinfo );
}


/*
=================
SV_KillServer
=================
*/
static void SV_KillServer_f( void ) {
	SV_Shutdown( "killserver" );
}


/*
=================
SV_SaveEntFile_f

The running level's entity text -- its .ent override's when one was
loaded -- written where sv_entityOverrides reads the map's, maps/<map>.ent
in the home directory: a start for an override, as DarkPlaces'
sv_saveentfile makes one.
=================
*/
static void SV_SaveEntFile_f( void ) {
	char path[MAX_QPATH];

	if ( !com_sv_running->integer || sv.state != SS_GAME ) {
		Com_Printf( "Server is not running.\n" );
		return;
	}
	if ( !SV_EntityOverrideFile( path, sizeof( path ) ) ) {
		Com_Printf( "Map %s makes no safe entity file path.\n", sv_mapname->string );
		return;
	}
	const char *text = SV_EntityText();
	const int length = static_cast<int>( strlen( text ) );
	// FS_WriteFile reports no failure, so the write is checked here
	const fileHandle_t file = FS_FOpenFileWrite( path );
	if ( file == FS_INVALID_HANDLE ) {
		Com_Printf( "Could not open %s for writing.\n", path );
		return;
	}
	const int written = length ? FS_Write( text, length, file ) : 0;
	FS_FCloseFile( file );
	if ( written != length ) {
		Com_Printf( "Could not write %s: %d of %d bytes written.\n", path, written, length );
		return;
	}
	Com_Printf( "Wrote %s (%d bytes).\n", path, length );
}


/*
=================
SV_ReloadMapCommand

The command that loads the running map again from its start, as it was
started: devmap while cheats are on, and in the games whose game type 2 is
single player (Quake III, its tests, RTCW and Elite Force's Holomatch) the
single-player alias while that type runs, since a plain map would turn it
into a free-for-all (SV_Map_f). Quake Live's map reuses its active factory;
Enemy Territory's and Medal of Honor's keep their game type themselves.
=================
*/
qboolean SV_ReloadMapCommand( char *command, int size ) {
	if ( !command || size <= 0 || !com_sv_running || !com_sv_running->integer || !sv_mapname
		|| !sv_mapname->string[0] ) {
		return qfalse;
	}
	const bool singlePlayerType = Com_GameIsQuake3() || Com_GameIsQ3Test() || Com_GameIsQuake3IHV()
		|| Com_GameIsRTCW() || Com_GameIsSTVEFMP();
	const bool singlePlayer = singlePlayerType && sv_gametype && sv_gametype->integer == GT_SINGLE_PLAYER;
	const bool cheats = Cvar_VariableIntegerValue( "sv_cheats" ) != 0;
	Com_sprintf( command, size, "%s%s %s\n", singlePlayer ? "sp" : "", cheats ? "devmap" : "map", sv_mapname->string );
	return qtrue;
}


/*
=================
SV_Locations
=================
*/
static void SV_Locations_f( void ) {

	// make sure server is running
	if ( !com_sv_running->integer ) {
		Com_Printf( "Server is not running.\n" );
		return;
	}

	if ( !sv_clientTLD->integer ) {
		Com_Printf( "Disabled on this server.\n" );
		return;
	}

	SV_PrintLocations_f( nullptr );
}

//===========================================================

/*
==================
SV_CompleteMapName
==================
*/
static void SV_CompleteMapName( const char *args, int argNum ) {
	if ( argNum == 2 ) 	{
		if ( sv.pure != 0 ) {
			Field_CompleteFilename( "maps", "bsp", qtrue, FS_MATCH_PK3s | FS_MATCH_STICK );
		} else {
			Field_CompleteFilename( "maps", "bsp", qtrue, FS_MATCH_ANY | FS_MATCH_STICK );
		}
	}
}

typedef qboolean ( *workshopAction_t )( uint64_t itemId );

static qboolean SV_ParseWorkshopItemId( const char *text, uint64_t *itemId ) {
	uint64_t value = 0;
	const uint64_t maxValue = ~static_cast<uint64_t>( 0 );

	if ( itemId ) {
		*itemId = 0;
	}
	if ( !text || !text[0] || !itemId ) {
		return qfalse;
	}

	for ( const char *cursor = text; *cursor; ++cursor ) {
		if ( *cursor < '0' || *cursor > '9' ) {
			return qfalse;
		}
		const uint64_t digit = static_cast<uint64_t>( *cursor - '0' );
		if ( value > ( maxValue - digit ) / 10u ) {
			return qfalse;
		}
		value = value * 10u + digit;
	}

	if ( !value ) {
		return qfalse;
	}
	*itemId = value;
	return qtrue;
}

static void SV_RunWorkshopCommand( const char *commandName,
	const char *actionName, workshopAction_t action ) {
	uint64_t itemId = 0;
	if ( Cmd_Argc() != 2 || !SV_ParseWorkshopItemId( Cmd_Argv( 1 ), &itemId ) ) {
		Com_Printf( "usage: %s <workshop item id>\n", commandName );
		return;
	}
	if ( !action( itemId ) ) {
		Com_Printf( S_COLOR_YELLOW "Unable to %s Workshop item %llu.\n",
			actionName, static_cast<unsigned long long>( itemId ) );
	}
}

static void SV_SteamDownloadUGC_f( void ) {
	SV_RunWorkshopCommand( "steam_downloadugc", "download",
		Com_WorkshopDownloadItem );
}

static void SV_SteamSubscribeUGC_f( void ) {
	SV_RunWorkshopCommand( "steam_subscribeugc", "subscribe to",
		Com_WorkshopSubscribeItem );
}

static void SV_SteamUnsubscribeUGC_f( void ) {
	SV_RunWorkshopCommand( "steam_unsubscribeugc", "unsubscribe from",
		Com_WorkshopUnsubscribeItem );
}

struct operatorCommand_t {
	const char *name;
	xcommand_t function;
};

template <std::size_t N>
static void SV_AddCommandSet( const std::array<operatorCommand_t, N> &commands )
{
	for ( const auto &command : commands ) {
		Cmd_AddCommand( command.name, command.function );
	}
}

template <std::size_t N>
static void SV_SetMapCompletionForCommands( const std::array<operatorCommand_t, N> &commands )
{
	for ( const auto &command : commands ) {
		Cmd_SetCommandCompletionFunc( command.name, SV_CompleteMapName );
	}
}


/*
==================
SV_AddOperatorCommands
==================
*/
static void SV_GameCompleteStatus_f( void ) {
	// The RTCW MP and ET game modules end a match with this console command;
	// their engines then report the final status to every master.
	SV_MasterGameCompleteStatus();
}

void SV_AddOperatorCommands( void ) {
	static bool	initialized;

	if ( initialized ) {
		return;
	}
	initialized = true;

	static constexpr std::array<operatorCommand_t, 13> commands = {{
		{ "heartbeat", SV_Heartbeat_f },
		{ "kick", SV_Kick_f },
		{ "kickbots", SV_KickBots_f },
		{ "kickall", SV_KickAll_f },
		{ "kicknum", SV_KickNum_f },
		{ "clientkick", SV_KickNum_f },
		{ "status", SV_Status_f },
		{ "dumpuser", SV_DumpUser_f },
		{ "sectorlist", SV_SectorList_f },
		{ "killserver", SV_KillServer_f },
		{ "filter", SV_AddFilter_f },
		{ "filtercmd", SV_AddFilterCmd_f },
		{ "sv_saveentfile", SV_SaveEntFile_f },
	}};
	static constexpr std::array<operatorCommand_t, 3> workshopCommands = {{
		{ "steam_downloadugc", SV_SteamDownloadUGC_f },
		{ "steam_subscribeugc", SV_SteamSubscribeUGC_f },
		{ "steam_unsubscribeugc", SV_SteamUnsubscribeUGC_f },
	}};
	static constexpr std::array<operatorCommand_t, 1> mapCommands = {{
		{ "map", SV_Map_f },
	}};
	static constexpr std::array<operatorCommand_t, 4> qlFactoryCommands = {{
		{ "reload_arenas", SV_ArenaReload_f },
		{ "reload_factories", SV_FactoryReload_f },
		{ "reload_mappool", SV_MapPoolReload_f },
		{ "startRandomMap", SV_StartRandomMap_f },
	}};
#ifndef PRE_RELEASE_DEMO
	static constexpr std::array<operatorCommand_t, 3> extraMapCommands = {{
		{ "devmap", SV_Map_f },
		{ "spmap", SV_Map_f },
		{ "spdevmap", SV_Map_f },
	}};
#endif
#ifdef USE_BANS
	static constexpr std::array<operatorCommand_t, 7> banCommands = {{
		{ "rehashbans", SV_RehashBans_f },
		{ "listbans", SV_ListBans_f },
		{ "banaddr", SV_BanAddr_f },
		{ "exceptaddr", SV_ExceptAddr_f },
		{ "bandel", SV_BanDel_f },
		{ "exceptdel", SV_ExceptDel_f },
		{ "flushbans", SV_FlushBans_f },
	}};
#ifndef STANDALONE
	static constexpr std::array<operatorCommand_t, 2> accountBanCommands = {{
		{ "banUser", SV_Ban_f },
		{ "banClient", SV_BanNum_f },
	}};
#endif
#endif

	SV_AddCommandSet( commands );
	if ( Com_GameIsRTCWSP() ) Cmd_AddCommand("loadgame", SV_LoadGame_f);
	if ( Com_GameIsRTCWMP() || Com_GameIsWolfET() ) Cmd_AddCommand("gameCompleteStatus", SV_GameCompleteStatus_f);
	if ( Com_GameIsSTVEFMP() || Com_GameIsJKAMP() || Com_GameIsJK2MP() ) Cmd_AddCommand("svsay", SV_ConSvSay_f);
	if ( Com_GameProfile()->supportsWorkshop ) {
		SV_AddCommandSet( workshopCommands );
	}
#ifndef STANDALONE
#ifdef USE_BANS
	if(!Cvar_VariableIntegerValue("com_standalone"))
	{
		SV_AddCommandSet( accountBanCommands );
	}
#endif
#endif
	if ( Com_GameIsSTVEFSP() ) {
		// Elite Force SP's map, devmap, transitions and savegames take the
		// place of the map commands. Retail has no map_restart or SP aliases
		// (stvoy.exe 1.20 SV_AddOperatorCommands, 0x4240e0); the shared fast
		// restart would detach its combined game/cgame without a client init.
		SV_STVEFSPAddCommands();
		Cmd_SetCommandCompletionFunc( "map", SV_CompleteMapName );
		Cmd_SetCommandCompletionFunc( "devmap", SV_CompleteMapName );
		Cmd_SetCommandCompletionFunc( "maptransition", SV_CompleteMapName );
		Cmd_SetCommandCompletionFunc( "loadtransition", SV_CompleteMapName );
		return;
	}
	if ( Com_GameIsRitual() ) {
		// American McGee's Alice's map, devmap, gamemap, restart, pause and
		// savegame commands take the place of the Quake III map set, which it
		// does not have (server_alice_save.hpp).
		SV_AliceAddCommands();
		Cmd_SetCommandCompletionFunc( "map", SV_CompleteMapName );
		Cmd_SetCommandCompletionFunc( "devmap", SV_CompleteMapName );
		Cmd_SetCommandCompletionFunc( "gamemap", SV_CompleteMapName );
		return;
	}
	Cmd_AddCommand( "map_restart", SV_MapRestart_f );
	SV_AddCommandSet( mapCommands );
	SV_SetMapCompletionForCommands( mapCommands );
	if ( Com_GameIsMOHAA() ) {
		// Medal of Honor's level changes, restarts and savegames, beside the
		// Quake III map set it keeps (server_mohaa_save.hpp).
		SV_MOHAAAddCommands();
		Cmd_SetCommandCompletionFunc( "gamemap", SV_CompleteMapName );
	}
	if ( Com_GameIsQuakeLive() ) {
		SV_AddCommandSet( qlFactoryCommands );
	}
#ifndef PRE_RELEASE_DEMO
	SV_AddCommandSet( extraMapCommands );
	SV_SetMapCompletionForCommands( extraMapCommands );
#endif
#ifdef USE_BANS	
	SV_AddCommandSet( banCommands );
#endif
}


/*
==================
SV_RemoveOperatorCommands
==================
*/
void SV_RemoveOperatorCommands( void ) {
#if 0
	// removing these won't let the server start again
	Cmd_RemoveCommand ("heartbeat");
	Cmd_RemoveCommand ("kick");
	Cmd_RemoveCommand ("kicknum");
	Cmd_RemoveCommand ("clientkick");
	Cmd_RemoveCommand ("kickall");
	Cmd_RemoveCommand ("kickbots");
	Cmd_RemoveCommand ("banUser");
	Cmd_RemoveCommand ("banClient");
	Cmd_RemoveCommand ("status");
	Cmd_RemoveCommand ("dumpuser");
	Cmd_RemoveCommand ("map_restart");
	Cmd_RemoveCommand ("sectorlist");
#endif
}


void SV_AddDedicatedCommands( void )
{
	static constexpr std::array<operatorCommand_t, 5> commands = {{
		{ "serverinfo", SV_Serverinfo_f },
		{ "systeminfo", SV_Systeminfo_f },
		{ "tell", SV_ConTell_f },
		{ "say", SV_ConSay_f },
		{ "locations", SV_Locations_f },
	}};

	SV_AddCommandSet( commands );
}


void SV_RemoveDedicatedCommands( void )
{
	Cmd_RemoveCommand( "serverinfo" );
	Cmd_RemoveCommand( "systeminfo" );
	Cmd_RemoveCommand( "tell" );
	Cmd_RemoveCommand( "say" );
	Cmd_RemoveCommand( "locations" );
}
