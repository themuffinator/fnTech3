/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_CLIENT_CL_MOHAA_H
#define FNTECH3_CLIENT_CL_MOHAA_H

/* Medal of Honor: Allied Assault 1.00's client: the protocol-6 records it is
 * sent (cl_mohaa.cpp) and the client game that reads them
 * (cl_mohaa_cgame.cpp). The engine's own snapshot keeps only the timing the
 * rest of the client runs on; the game's records stay the game's own, as
 * code/compat/mohaa_game_abi.h lays them out. */

/* Include after client.h, which defines msg_t, usercmd_t and vm_t, and
 * outside its extern "C" block: these are C++ functions. */
#include "../compat/mohaa_cgame_abi.h"

/* ---- cl_mohaa.cpp: the wire ------------------------------------------- */

/* Forget every snapshot, entity and baseline (a new gamestate). */
void CL_MOHAAClearState( void );
/* One svc_baseline inside a gamestate: an entity number and its delta from
 * the null entity. */
void CL_MOHAAParseBaseline( msg_t *msg );
/* The same baselines written again, into a demo's gamestate. */
void CL_MOHAAWriteBaselines( msg_t *msg );
/* svc_snapshot (7), and the three opcodes past it: a centre print (8), a
 * located print (9) and a client-game message (10). */
void CL_MOHAAParseSnapshot( msg_t *msg );
void CL_MOHAAParseCenterprint( msg_t *msg );
void CL_MOHAAParseLocationprint( msg_t *msg );
void CL_MOHAAParseCGameMessage( msg_t *msg );
/* Where the client game's MSG_Read imports read from while it parses a
 * client-game message; null otherwise. */
msg_t *CL_MOHAACurrentMessage( void );

/* The client game's snapshot, its entities ordered parents first as retail
 * hands them over. False when the frame or its entities are gone. */
bool CL_MOHAAGetSnapshot( int snapshotNumber, fntech3::mohaa::Snapshot *snapshot );
void CL_MOHAACurrentSnapshotNumber( int *snapshotNumber, int *serverTime );
int CL_MOHAAServerStartTime( void );
/* The current snapshot's player state, or null before the first. */
const fntech3::mohaa::PlayerState *CL_MOHAAPlayerState( void );
/* cl_cgame.cpp's liquid census: the current frame's entities, in 1.00's own
 * entity types, and the local player. */
void CL_MOHAALiquidCensus( clLiquidCensusVisit_t visit );

/* The eye record every move carries, written after the command count, and
 * each command after it. */
void CL_MOHAAWriteEyeInfo( msg_t *msg );
void CL_MOHAAWriteDeltaUsercmdKey( msg_t *msg, int key, const usercmd_t &from, const usercmd_t &to );
/* An engine command as 1.00's client sends it, and back. */
void CL_MOHAACommandFromEngine( const usercmd_t &in, fntech3::mohaa::UserCmd &out );

/* The prints the server sends, drawn over the view until they fade. */
void CL_MOHAADrawPrints( void );
/* The screen's fade and a cinematic's letterbox, over the view and under the
 * client game's 2D; and whether a letterbox is up, which hides the HUD. */
void CL_MOHAADrawFades( void );
bool CL_MOHAALetterboxActive( void );

/* `localservers`: beside the game ports' getinfo, GameSpy's LAN search,
 * whose answers join the LAN list for three seconds. */
void CL_MOHAAGameSpyLanSearch( void );
/* True, once, when local server `index` was listed from a GameSpy answer:
 * the server's own getinfo answer then lists it instead. */
bool CL_MOHAAGameSpyTakeOver( int index );

/* cl_cgame.cpp's reading of a server command (cs, bcs, disconnect and the
 * rest), which the client game's slot 118 is served by. */
qboolean CL_GetServerCommandForProfile( int serverCommandNumber );

/* ---- cl_mohaa_sound.cpp: the music ------------------------------------ */

/* The triggered-music and soundtrack commands, with the engine's menus. */
void CL_MOHAAMusicInit( void );
void CL_MOHAAMusicShutdown( void );
/* The client game's music calls (its slots 65..67), and the level's end. */
void CL_MOHAAMusicNewSoundtrack( const char *name );
void CL_MOHAAMusicUpdateMood( int current, int fallback );
void CL_MOHAAMusicUpdateVolume( float volume, float fadeTime );
void CL_MOHAAMusicFreeAll( void );
/* Once a frame. */
void CL_MOHAAMusicUpdate( void );
/* The triggered music a save keeps, and a loaded save's back from where it
 * was (server_mohaa_save.hpp declares these for itself too). */
bool CL_MOHAAMusicTriggeredState( char *file, int fileSize, bool *loops, int *offset );
void CL_MOHAAMusicRestoreTriggered( const char *file, bool loops, int offset );
/* The main menu's theme, as triggered music; and the triggered music stopped. */
void CL_MOHAAMusicMainTheme( void );
void CL_MOHAAMusicStopTriggered( void );
/* A movie's own sound file (cl_cin.cpp): from a system cinematic's first
 * frame, fed each frame it plays, stopped as it ends. */
void CL_MOHAAMovieSoundStart( const char *movie );
void CL_MOHAAMovieSoundUpdate( void );
void CL_MOHAAMovieSoundStop( void );
/* The sounds a level's script waits on: each sound the client game starts,
 * which gives a waited one the channel the local game asks after (-1 for
 * any other); an entity channel's stop; whether a channel still plays a
 * sound (server_mohaa_game.hpp declares it for itself too); and none, for a
 * new client game. */
int CL_MOHAASoundStarted( int entity, int channel, const char *name, float pitch, bool waited );
void CL_MOHAASoundStopped( int entity, int channel );
bool CL_MOHAASoundPlaying( int id, const char *name );
void CL_MOHAASoundsClear( void );
/* The local server's game told that a waited sound started (server_mohaa_game.hpp). */
extern "C" void SV_MOHAASoundCallback( int entity, int channel, const char *name );

/* ---- cl_mohaa_ui.cpp: the engine's menus ------------------------------- */

vm_t *CL_MOHAALoadUI( void );
/* The engine's own white picture for fills: the game's "white" is an
 * additive sprite. */
qhandle_t CL_MOHAAWhite( void );
/* The in-game HUD menus, drawn after the client game's 2D and the prints. */
void CL_MOHAAUIDrawHud( void );
/* True while `saveshot` draws its frame of the view alone: nothing 2D --
 * menu, HUD, client-game drawing, print or console -- goes over it. */
bool CL_MOHAAViewOnlyFrame( void );
/* A system cinematic's end (cl_cin.cpp CL_RunCinematicNextmap): true when it
 * was one of the startup intro's movies, and the intro goes on from there. */
bool CL_MOHAAIntroMovieEnded( void );
/* A model for the menus' labels (cl_mohaa_cgame.cpp): its renderer handle
 * and its TIKI definition, or false when it cannot be drawn. */
bool CL_MOHAAUIModel( const char *name, qhandle_t *renderer, void **tiki );
/* The client game's menu and scoreboard calls (its slots 128, 129 and 155 to
 * 159). */
void CL_MOHAAUIShowMenu( const char *name, bool force );
void CL_MOHAAUIHideMenu( const char *name, bool force );
void CL_MOHAAUIShowScoreboard( const char *menuName );
void CL_MOHAAUIHideScoreboard( void );
void CL_MOHAAUISetScoreboardItem( int item, const char *const data[8], const float *textColor,
	const float *backColor, bool header );
void CL_MOHAAUIDeleteScoreboardItems( int maxIndex );
/* A printed line that starts with one of the game's message codes (1..5),
 * for the HUD's message boxes. */
void CL_MOHAAUIPrint( const char *text );

/* The fonts (cl_mohaa_cgame.cpp), for the menus. */
void *CL_MOHAAFontLoad( const char *name );
void CL_MOHAAFontDraw( void *font, const char *text, float x, float y, int maxLength, const float *scale );
int CL_MOHAAFontWidth( void *font, const char *text, int maxLength );
int CL_MOHAAFontHeight( void *font );

/* ---- cl_mohaa_cgame.cpp: the client game ------------------------------ */

vm_t *CL_MOHAALoadCGame( void );
/* The local player's key bindings, for a listen server's game (its imports
 * 153..155; server_mohaa_game.hpp declares these for itself too). */
int CL_MOHAAKeyStringToKeynum( const char *name );
const char *CL_MOHAAKeyBindString( int keynum );
void CL_MOHAAKeysForCommand( const char *command, int *key1, int *key2 );
/* The server's side (server.h, C linkage): the local server's level may start,
 * its first snapshot being on the client. */
extern "C" void SV_MOHAAServerLoaded( void );
/* The client game's own calls the engine makes. */
void CL_MOHAACGameParseMessage( void );
void CL_MOHAACGameDraw2D( void );
void CL_MOHAACGameEyeOffset( float out[3] );
void CL_MOHAACGameEyeAngles( float out[3] );
float CL_MOHAACGameSensitivityScale( void );
int CL_MOHAACGameWeaponCommandButtonBits( void );
qboolean CL_MOHAACGameCheckCaptureKey( int key, qboolean down );
/* The scoreboard's place in 640x480, its colours and header row, and its
 * columns, as the client game lays them out; false without a client game. */
bool CL_MOHAACGameScoreboardLayout( float rect[4], float color[4], float fontColor[4], bool *drawHeader );
const char *CL_MOHAACGameScoreboardColumn( int column, int *width );
float CL_MOHAACGameObjectiveAlpha( void );
/* The game module's HUD elements on a local server. */
void CL_MOHAAHudDrawShader( int info, const char *name );
void CL_MOHAAHudDrawAlign( int info, int horizontal, int vertical );
void CL_MOHAAHudDrawRect( int info, int x, int y, int width, int height );
void CL_MOHAAHudDrawVirtualSize( int info, bool virtualScreen );
void CL_MOHAAHudDrawColor( int info, const float *color );
void CL_MOHAAHudDrawAlpha( int info, float alpha );
void CL_MOHAAHudDrawString( int info, const char *string );
void CL_MOHAAHudDrawFont( int info, const char *name );
void CL_MOHAACGameRendererRestarted( void );
/* A centre or located print from the server, drawn as 1.00's view draws it. */
void CL_MOHAADrawViewPrint( const char *text, bool located, int x, int y, float alpha );
/* Every model the client game registered, forgotten with the renderer's. */
void CL_MOHAACGameClearModels( void );
/* The client game's decal cutter (exports 16 to 18), for the decal editor.
 * The permanent mark traces from `origin` along `direction` and leaves the
 * hit point and its surface's normal in them; each cuts into arrays of
 * FNTECH3_MOHAA_MARK_FRAGMENTS fragments and FNTECH3_MOHAA_MARK_POINTS
 * points and returns the fragments, 0 without a client game. */
int CL_MOHAACGamePermanentMark( vec3_t origin, vec3_t direction, float orientation, float sScale, float tScale,
	const vec4_t color, bool doLighting, fntech3::mohaa::MarkFragment *fragments,
	fntech3::mohaa::PolyVert *points );
int CL_MOHAACGamePermanentTreadMarkDecal( fntech3::mohaa::TreadMark *tread, bool startSegment, bool doLighting,
	fntech3::mohaa::MarkFragment *fragments, fntech3::mohaa::PolyVert *points );
/* -1 when the path grew no segment, 0 when its last one grew, 1 when a new
 * one began. */
int CL_MOHAACGamePermanentUpdateTreadMark( fntech3::mohaa::TreadMark *tread, float alpha, float minSegment,
	float maxSegment, float maxOffset, float texScale );

/* ---- cl_mohaa_decals.cpp: the decal editor ----------------------------- */

/* The dcl_* cvars, when the client game loads. */
void CL_MOHAADecalsInit( void );
/* A level begins to load, before its world: with dcl_editmode, the editor
 * reads its decal file and its commands exist until the client game goes. */
void CL_MOHAADecalsBeginLevel( const char *mapName );
void CL_MOHAADecalsShutdown( void );
/* Each frame of the world view, before it is drawn: the decals, the path
 * being laid and the selected decal's outline join the scene. */
void CL_MOHAADecalsFrame( const refdef_t *view );

#endif
