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
// client.h -- primary header for client

#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"
#include "../renderercommon/tr_public.h"
#include "../qcommon/vm_local.h"
#include "../ui/ui_public.h"
#include "../cgame/cg_public.h"
#include "../game/bg_public.h"
#include "audio/snd_public.h"
#include "keys.h"

#if defined(USE_CURL) && !defined(DEDICATED)
#define USE_CLIENT_CURL 1
#include "cl_curl.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

// file full of random crap that gets used to create cl_guid
#define QKEY_FILE "qkey"
#define QKEY_SIZE 2048

#define	RETRANSMIT_TIMEOUT	3000	// time between connection packet retransmits

// snapshots are a view of the server at a given time
struct clSnapshot_t {
	qboolean		valid;			// cleared if delta parsing was invalid
	int				snapFlags;		// rate delayed and dropped commands

	int				serverTime;		// server time the message is valid for (in msec)

	int				messageNum;		// copied from netchan->incoming_sequence
	int				deltaNum;		// messageNum the delta is from
	int				ping;			// time from when cmdNum-1 was sent to time packet was reeceived
	int				areabytes;
	byte			areamask[MAX_MAP_AREA_BYTES];		// portalarea visibility bits

	int				cmdNum;			// the next cmdNum the server is expecting
	playerState_t	ps;						// complete information about the current player at this time
	playerState_t	psRaw;					// raw protocol-specific playerstate for legacy demo delta bases
	playerState_t	jkaVehiclePs;			// separate protocol-26 vehicle delta base

	int				numEntities;			// all of the entities that need to be presented
	int				parseEntitiesNum;		// at the time of this snapshot

	int				serverCommandNum;		// execute all commands up to this before
											// making the snapshot current

	// American McGee's Alice: the sounds the server started or stopped with
	// this snapshot, read from the block that follows its packet entities.
	int				aliceNumSounds;
	aliceServerSound_t	aliceSounds[ALICE_MAX_SERVER_SOUNDS];
};
#ifndef __cplusplus
#define clSnapshot_t struct clSnapshot_t
#endif



/*
=============================================================================

the clientActive_t structure is wiped completely at every
new gamestate_t, potentially several times during an established connection

=============================================================================
*/

struct outPacket_t {
	int		p_cmdNumber;		// cl.cmdNumber when packet was sent
	int		p_serverTime;		// usercmd->serverTime when packet was sent
	int		p_realtime;			// cls.realtime when packet was sent
};
#ifndef __cplusplus
#define outPacket_t struct outPacket_t
#endif

// the parseEntities array must be large enough to hold PACKET_BACKUP frames of
// entities, so that when a delta compressed message arives from the server
// it can be un-deltad from the original 
#define	MAX_PARSE_ENTITIES	( PACKET_BACKUP * MAX_SNAPSHOT_ENTITIES )

extern int g_console_field_width;

struct clientActive_t {
	int			timeoutcount;		// it requres several frames in a timeout condition
									// to disconnect, preventing debugging breaks from
									// causing immediate disconnects on continue
	clSnapshot_t	snap;			// latest received from server

	int			serverTime;			// may be paused during play
	int			oldServerTime;		// to prevent time from flowing bakcwards
	int			oldFrameServerTime;	// to check tournament restarts
	int			serverTimeDelta;	// cl.serverTime = cls.gametime + cl.serverTimeDelta
									// this value changes as net lag varies
	qboolean	extrapolatedSnapshot;	// set if any cgame frame has been forced to extrapolate
									// cleared when CL_AdjustTimeDelta looks at it
	qboolean	newSnapshots;		// set on parse of any valid packet

	gameState_t	gameState;			// configstrings
	char		mapname[MAX_QPATH];	// extracted from CS_SERVERINFO

	int			parseEntitiesNum;	// index (not anded off) into cl_parse_entities[]

	int			mouseDx[2], mouseDy[2];	// added to by mouse events
	int			mouseIndex;
	int			joystickAxis[MAX_JOYSTICK_AXIS];	// set by joystick events

	// cgame communicates a few values to the client system
	int			cgameUserCmdValue;	// current weapon to add to usercmd_t
	int			cgameUserCmdPrimary;
	int			cgameUserCmdFov;
	float		cgameSensitivity;
	rtcwUsercmdExtension_t cgameRTCWCommand;	// mode-specific values supplied by RTCW cgame
	vec3_t		cgameRTCWLerpOrigin;	// MP's interpolated viewpoint; cleared with the map state
	// ET's cgame supplies its own command flags and spectated client; these
	// are a separate contract from RTCW's and never share its storage.
	struct { unsigned char flags, identClient; } cgameWolfETCommand;
	vec3_t		cgameWolfETLerpOrigin;	// ET's interpolated viewpoint; cleared with the map state
	qboolean	wolfetCameraMode;	// ET primary-camera input policy; reset with map state
	rtcwLimboChat_t rtcwLimboChat;
	qboolean	rtcwLimboMode;	// engine chat routing; cleared with connection/map state
	qboolean	rtcwCameraMode;	// SP primary-camera input policy; reset with map state

	// cmds[cmdNumber] is the predicted command, [cmdNumber-1] is the last
	// properly generated command
	usercmd_t	cmds[CMD_BACKUP];	// each message will send several old cmds
	int			cmdNumber;			// incremented each frame, because multiple
									// frames may need to be packed into a single packet

	outPacket_t	outPackets[PACKET_BACKUP];	// information about each packet we have sent out

	// the client maintains its own idea of view angles, which are
	// sent to the server each frame.  It is cleared to 0 upon entering each level.
	// the server sends a delta each frame which is added to the locally
	// tracked view angles to account for standing on rotating objects,
	// and teleport direction changes
	vec3_t		viewangles;

	int			serverId;			// included in each client message so the server
												// can tell if it is for a prior map_restart
	// big stuff at end of structure so most offsets are 15 bits or less
	clSnapshot_t	snapshots[PACKET_BACKUP];

	entityState_t	entityBaselines[MAX_GENTITIES];	// for delta compression when not in previous frame

	entityState_t	parseEntities[MAX_PARSE_ENTITIES];

	byte			baselineUsed[MAX_GENTITIES];
};
#ifndef __cplusplus
#define clientActive_t struct clientActive_t
#endif

extern	clientActive_t		cl;

#define EM_GAMESTATE 1
#define EM_SNAPSHOT  2
#define EM_COMMAND   4

/*
=============================================================================

the clientConnection_t structure is wiped when disconnecting from a server,
either to go to a full screen console, play a demo, or connect to a different server

A connection can be to either a server through the network layer or a
demo through a file.

=============================================================================
*/

struct clientConnection_t {

	int			clientNum;
	int			lastPacketSentTime;			// for retransmits during connection
	int			lastPacketTime;				// for timeouts

	netadr_t	serverAddress;
	int			connectTime;				// for connection retransmits
	int			connectPacketCount;			// for display on connection dialog
	char		serverMessage[MAX_STRING_CHARS]; // for display on connection dialog

	int			challenge;					// from the server to use for connecting
	int			handshakeProtocol;			// protocol selected by challengeResponse
	int			q3ProtocolChallenge;			// nonce for the Q3 version query
	int			q3ServerProtocol;			// exact-address, nonce-checked infoResponse
	int			rtcwOnlyVisibleClients;		// RTCW MP server asks to hide unseen players
	int			checksumFeed;				// from the server for checksum calculations

	// these are our reliable messages that go to the server
	int			reliableSequence;
	int			reliableAcknowledge;		// the last one the server has executed
	char		reliableCommands[MAX_GAME_RELIABLE_COMMANDS][MAX_STRING_CHARS];

	// server message (unreliable) and command (reliable) sequence
	// numbers are NOT cleared at level changes, but continue to
	// increase as long as the connection is valid

	// message sequence is used by both the network layer and the
	// delta compression layer
	int			serverMessageSequence;

	// reliable messages received from server
	int			serverCommandSequence;
	int			lastExecutedServerCommand;		// last server command grabbed or executed with CL_GetServerCommand
	char		serverCommands[MAX_GAME_RELIABLE_COMMANDS][MAX_STRING_CHARS];
	int			serverCommandHashes[MAX_GAME_RELIABLE_COMMANDS]; // protocol hash of the unmodified wire command
	qboolean	serverCommandsIgnore[MAX_GAME_RELIABLE_COMMANDS];

	// file transfer from server
	fileHandle_t download;
	char		downloadName[MAX_OSPATH];
	char		downloadTempName[MAX_OSPATH + 4]; // downloadName + ".tmp"
	int			sv_allowDownload;
	char		sv_dlURL[MAX_CVAR_VALUE_STRING];
	int			downloadNumber;
	int			downloadBlock;	// block we are waiting for
	int			downloadCount;	// how many bytes we got
	int			downloadSize;	// how many bytes we got
	char		downloadList[BIG_INFO_STRING]; // list of paks we need to download
	qboolean	downloadRestart;	// if true, we need to do another FS_Restart because we downloaded a pak
	// Enemy Territory's download redirect (svc_download block -1): 0 with
	// none in hand, 1 while its web transfer runs, 2 once it is declined or
	// has failed, until the server's next gamestate.
	int			wolfetWebDownload;

#ifdef USE_CLIENT_CURL
	qboolean	cURLEnabled;
	qboolean	cURLUsed;
	qboolean	cURLDisconnected;
	char		downloadURL[MAX_OSPATH];
	CURL		*downloadCURL;
	CURLM		*downloadCURLM;
	httpPakHeader_t downloadPakHeader;
#endif /* USE_CLIENT_CURL */

	// demo information
	char		demoName[MAX_OSPATH];
	char		recordName[MAX_OSPATH]; // without extension
	qboolean	explicitRecordName;
	char		recordNameShort[TRUNCATE_LENGTH]; // for recording message
	qboolean	dm68compat;
	qboolean	spDemoRecording;
	qboolean	demorecording;
	qboolean	demoplaying;
	qboolean	demoLegacyFormat;
	int			demoLegacyProtocol;
	netchanWireProfile_t demoWireProfile;
	qboolean	demowaiting;	// don't record until a non-delta message is received
	qboolean	firstDemoFrameSkipped;
	fileHandle_t	demofile;
	fileHandle_t	recordfile;

	int		timeDemoFrames;		// counter of rendered frames
	int		timeDemoStart;		// cls.realtime before first frame
	int		timeDemoBaseTime;	// each frame will be at this time + frameNum * 50

	float	aviVideoFrameRemainder;
	float	aviSoundFrameRemainder;
	int		aviFrameEndTime;
	char	videoName[MAX_QPATH];
	int		videoIndex;

	// big stuff at end of structure so most offsets are 15 bits or less
	netchan_t	netchan;

	qboolean compat;

	// simultaneous demo playback and recording
	int		eventMask;
	int		demoCommandSequence;
	int		demoDeltaNum;
	int		demoMessageSequence;

};
#ifndef __cplusplus
#define clientConnection_t struct clientConnection_t
#endif

extern	clientConnection_t clc;

/*
==================================================================

the clientStatic_t structure is never wiped, and is used even when
no client connection is active at all

==================================================================
*/

struct ping_t {
	netadr_t	adr;
	int			start;
	int			time;
	char		info[MAX_INFO_STRING];
};
#ifndef __cplusplus
#define ping_t struct ping_t
#endif

struct serverInfo_t {
	netadr_t	adr;
	char	  	hostName[MAX_NAME_LENGTH];
	char	  	mapName[MAX_NAME_LENGTH];
	char	  	game[MAX_NAME_LENGTH];
	int			netType;
	int			gameType;
	int		  	clients;
	int		  	maxClients;
	int			minPing;
	int			maxPing;
	int			ping;
	qboolean	visible;
	int			punkbuster;
	int			g_humanplayers;
	int			g_needpass;
};
#ifndef __cplusplus
#define serverInfo_t struct serverInfo_t
#endif

struct clientStatic_t {
	connstate_t	state;				// connection status
	qboolean	gameSwitch;

	qboolean	cddialog;			// bring up the cd needed dialog next frame
	qboolean	endgamemenu;		// RTCW SP: bring up the endgame menu next frame

	char		servername[MAX_OSPATH];		// name of server from original connect (used by reconnect)

	// when the server clears the hunk, all of these must be restarted
	qboolean	rendererStarted;
	qboolean	soundStarted;
	qboolean	soundRegistered;
	qboolean	uiStarted;
	qboolean	cgameStarted;

	int			framecount;
	int			frametime;			// real msec since last frame
	int			gameFrametime;		// scaled world msec since last frame

	int			realtime;			// ignores pause and timescale
	int			gametime;			// scaled world time
	int			realFrametime;		// ignoring pause, so console always works

	int			numlocalservers;
	serverInfo_t	localServers[MAX_OTHER_SERVERS];

	int			numglobalservers;
	serverInfo_t  globalServers[MAX_GLOBAL_SERVERS];
	// additional global servers
	int			numGlobalServerAddresses;
	netadr_t		globalServerAddresses[MAX_GLOBAL_SERVERS];

	int			numfavoriteservers;
	serverInfo_t	favoriteServers[MAX_OTHER_SERVERS];

	int pingUpdateSource;		// source currently pinging or updating

	// update server info
	netadr_t	updateServer;
	char		updateChallenge[MAX_TOKEN_CHARS];
	char		updateInfoString[MAX_INFO_STRING];

	netadr_t	authorizeServer;

	// rendering info
	glconfig_t	glconfig;
	qhandle_t	charSetShader;
	qhandle_t	recordShader;
	qhandle_t	whiteShader;
	qhandle_t	consoleShader;
	qhandle_t	consoleLogoShader;	// drawn over the background; 0 if none
	qhandle_t	cursorShader;

	int			lastVidRestart;
	int			soundMuted;

	qboolean	startCgame;

	int			captureWidth;
	int			captureHeight;

	float		con_factor;

	float		scale;
	float		biasX;
	float		biasY;

};
#ifndef __cplusplus
#define clientStatic_t struct clientStatic_t
#endif

extern int bigchar_width;
extern int bigchar_height;
extern int smallchar_width;
extern int smallchar_height;

extern	clientStatic_t		cls;

extern	char		cl_oldGame[MAX_QPATH];
extern	qboolean	cl_oldGameSet;

#ifdef USE_CLIENT_CURL

extern		download_t	download;
qboolean	Com_DL_Perform( download_t *dl );
void		Com_DL_Cleanup( download_t *dl );
qboolean	Com_DL_Begin( download_t *dl, const char *localName, const char *remoteURL, qboolean autoDownload );
qboolean	Com_DL_InProgress( const download_t *dl );
qboolean	Com_DL_ValidFileName( const char *fileName );
qboolean	CL_Download( const char *cmd, const char *pakname, qboolean autoDownload );

#endif

//=============================================================================

extern	vm_t			*cgvm;	// interface to cgame dll or vm
extern	vm_t			*uivm;	// interface to ui dll or vm
extern	refexport_t		re;		// interface to refresh .dll


//
// cvars
//
extern	cvar_t	*cl_noprint;
extern	cvar_t	*cl_debugMove;
extern	cvar_t	*cl_timegraph;
extern	cvar_t	*cl_shownet;
extern	cvar_t	*cl_debugViewAngles;
extern	cvar_t	*cl_autoNudge;
extern	cvar_t	*cl_autoTimeNudge;
extern	cvar_t	*cl_timeNudge;
extern	cvar_t	*cl_showTimeDelta;

extern	cvar_t	*com_timedemo;
extern	cvar_t	*cl_aviFrameRate;
extern	cvar_t	*cl_aviMotionJpeg;
extern	cvar_t	*cl_aviPipeFormat;

extern	cvar_t	*cl_activeAction;

extern	cvar_t	*cl_allowDownload;
#ifdef USE_CLIENT_CURL
extern	cvar_t	*cl_mapAutoDownload;
extern	cvar_t	*cl_dlDirectory;
#endif
extern	cvar_t	*cl_conXOffset;
extern	cvar_t	*cl_conColor;
extern	cvar_t	*cl_inGameVideo;

extern	cvar_t	*cl_lanForcePackets;
extern	cvar_t	*cl_autoRecordDemo;
extern	cvar_t	*cl_freezeDemo;
extern	cvar_t	*cl_allowConsoleChat;
extern	cvar_t	*cl_demoRecordMessage;
extern	cvar_t	*cl_drawRecording;
extern	cvar_t	*cl_demooffset;	// Enemy Territory's alone; null in other profiles
extern	cvar_t	*cl_menuAspect;
extern	cvar_t	*cl_menuBlur;
extern	cvar_t	*cl_viewShake;
extern	cvar_t	*cl_cinematicAspect;
extern	cvar_t	*cl_captureActive;
extern	cvar_t	*cl_playerHighlight;
extern	cvar_t	*cl_playerHighlightRimIntensity;
extern	cvar_t	*cl_playerHighlightOutlineIntensity;
extern	cvar_t	*cl_playerHighlightOutlineScale;
extern	cvar_t	*cl_playerHighlightRedColor;
extern	cvar_t	*cl_playerHighlightBlueColor;
extern	cvar_t	*cl_playerHighlightFreeColor;
extern	cvar_t	*cl_playerHighlightEnemyColor;
extern	cvar_t	*cl_playerHighlightTeammateColor;
extern	cvar_t	*r_levelshotHideHud;
extern	cvar_t	*r_levelshotHideViewWeapon;

extern	cvar_t	*com_maxfps;

extern	cvar_t	*vid_xpos;
extern	cvar_t	*vid_ypos;
extern	cvar_t	*r_noborder;

extern	cvar_t	*r_allowSoftwareGL;
extern	cvar_t	*r_swapInterval;
extern	cvar_t	*r_glDriver;

extern	cvar_t	*r_displayRefresh;
extern	cvar_t	*r_fullscreen;
extern	cvar_t	*r_mode;
extern	cvar_t	*r_modeFullscreen;
extern	cvar_t	*r_customwidth;
extern	cvar_t	*r_customheight;
extern	cvar_t	*r_customPixelAspect;
extern	cvar_t	*r_colorbits;
extern	cvar_t	*cl_stencilbits;
extern	cvar_t	*cl_depthbits;
extern	cvar_t	*cl_drawBuffer;

//=================================================

//
// cl_main
//
void CL_AddReliableCommand( const char *cmd, qboolean isDisconnectCmd );

// cl_alice.cpp: American McGee's Alice's engine-owned client state
void CL_AliceCenterPrint( const char *text, int x, int y, qboolean located );
void CL_AliceClearState( void );
void CL_JKAStringCacheReset( void );
void CL_AliceLoadingScreenRefresh( void );
void CL_AliceRegisterClientArt( void );
void CL_AliceMapLoading( void );
void CL_AliceFirstSnapshot( void );
// The loading screen a spawn that keeps the game module puts up, and the
// server-count test each frame's snapshot gets (0x00424b9f).
void CL_AliceMapLoadingKeep( void );
qboolean CL_AliceSnapshotServerCount( int snapFlags );
void CL_AliceWidgetPrint( const char *widget, const char *text );
// cl_alice_saveshot.cpp: the savegame picture. The `saveshot <path> <w> <h>`
// command's body, and the capture it makes: one frame drawn and kept as a
// width by height grey TGA at path.
void CL_AliceSaveshotCommand( void );
qboolean CL_AliceWriteSaveshot( const char *path, int width, int height );
// cl_jedi_sp_ui.cpp: the Jedi single-player engines' own menus, hosted as the
// UI module (neither game ships one), and cl_ui.cpp's placement of a point
// and size on Raven's virtual 640x480 menu screen.
vm_t *CL_JediSPLoadUI( void );
void CL_UIVirtualToScreen( float *x, float *y, float *w, float *h );
// cl_alice_screen.cpp and cl_alice_ui.cpp: Alice's engine-drawn frame and its
// engine-side menus, hosted as the UI module.
void CL_AliceUpdateScreenField( stereoFrame_t stereo );
vm_t *CL_AliceLoadUI( void );
void CL_AliceToggleMenu_f( void );
qboolean CL_AliceUIMenuUp( void );
// Escape when no menu holds the keys (retail CL_KeyEvent 0x00429d9d).
void CL_AliceEscape( void );
// The intro's state (retail 0x005315f8): the cinematic player's end of an
// intro logo sets the state its flag resumes at, and -1 means a map loaded.
void CL_AliceUISetIntroState( int state );
int CL_AliceUIGetIntroState( void );
// client_alice_cgame.hpp: the client game's 2D layer (its export 5), which the
// in-game view draws over the 3D view.
void CL_AliceCGameDraw2D( void );

void CL_StartHunkUsers( void );

void CL_Disconnect_f( void );
void CL_ReadDemoMessage( void );
void CL_StopRecord_f( void );
netchanWireProfile_t CL_ActiveWireProfile( void );
// Elite Force Holomatch's retail protocol-24 framing, live or from a demo.
qboolean CL_LegacyFraming( void );
// cl_viewShake as a finite 0..1 factor for view shakes the engine hands a
// game module; 1 when the cvar is not registered yet.
float CL_ViewShakeScale( void );
// Elite Force and both Jedi titles' charsgrid_med use the left half of a cell.
// The installed atlas is adapted into Fn's diagnostic and console text paths.
static inline float CL_CharsetCellWidth( void ) {
	const comGameProfile_t *profile = Com_GameProfile();
	const bool jedi = profile && ( profile->id == COM_GAME_JKA_SP || profile->id == COM_GAME_JKA_MP ||
		profile->id == COM_GAME_JK2_SP || profile->id == COM_GAME_JK2_MP );
	return Com_GameIsSTVEF() || jedi ? 0.03125f : 0.0625f;
}
// Ritual's protocol 1 is one wire for Alice and F.A.K.K.2, whose engines ring
// 128 and 64 commands (alice.exe 0x0042a220, fakk2.exe 1.02 0x0041ffaf); the
// profile holds its engine's count.
static inline int CL_ReliableCommandLimit( void ) {
	const netchanWireProfile_t wire = CL_ActiveWireProfile();
	return wire == NETCHAN_WIRE_ALICE ? Com_GameProfile()->bots.reliableCommands
		: Netchan_ReliableCommandLimit( wire );
}

void CL_InitDownloads( void );
void CL_NextDownload( void );
qboolean CL_GetWorkshopDownloadInfo( unsigned int itemIdLow, unsigned int itemIdHigh, unsigned long long *outDownloaded, unsigned long long *outTotal );
int CL_ParseRequiredWorkshopItems( const char *text, unsigned long long *items, int capacity );
void CL_Workshop_Init( void );
void CL_Workshop_Shutdown( void );
void CL_Workshop_Reset( void );
qboolean CL_WorkshopDownloadsActive( void );
qboolean CL_Workshop_BeginRequiredDownloads( const char *requiredItems );
void CL_Workshop_Frame( void );
void CL_ResumeDownloadsAfterWorkshop( void );

void CL_GetPing( int n, char *buf, int buflen, int *pingtime );
void CL_GetPingInfo( int n, char *buf, int buflen );
void CL_ClearPing( int n );
int CL_GetPingQueueCount( void );

void CL_ClearState( void );

int CL_ServerStatus( const char *serverAddress, char *serverStatusString, int maxLen );

qboolean CL_CheckPaused( void );
qboolean CL_NoDelay( void );
int CL_DemoRecordMessageMode( void );

int CL_GetRequestedMode( qboolean fullscreen );
qboolean CL_GetModeInfo( int *width, int *height, float *windowAspect, int mode, const char *modeFS, int dw, int dh, qboolean fullscreen );
qboolean CL_CopyClientIdentity( int clientNum, cgameClientIdentity_t *identity );
qboolean CL_GetClientSteamId( int clientNum, unsigned int *steamIdLow, unsigned int *steamIdHigh );
qboolean CL_IsSteamIdentityMuted( unsigned int identityLow, unsigned int identityHigh );
qboolean CL_ToggleSteamIdentityMute( unsigned int identityLow, unsigned int identityHigh );
qboolean CL_IsVoiceSenderMuted( int clientNum );
void CL_SetClientSpeakingState( int clientNum, qboolean speaking );
void CL_SetLocalSpeakingState( qboolean speaking );

void CL_RefreshSteamIdentity( void );
void CL_EnforceSteamIdentity( void );
const char *CL_SteamIdentityName( void );
qboolean CL_SteamIdentityNameLocked( void );


//
// cl_input.cpp
//
void CL_InitInput( void );
void CL_ClearInput( void );
void CL_SendCmd( void );
void CL_WritePacket( int repeat );
typedef enum {
	CL_INPUT_COMMAND_STALE = -2,
	CL_INPUT_COMMAND_LEGACY = -1
} clInputCommandSource_t;
int CL_ValidateInputCommandSource( void );
void CL_ClearKeyCommandInputState( void );
void CL_ClearGeneratedVoiceInputState( void );
void CL_ResetVoiceInputState( void );
void CL_RemoveVoiceInputSource( int sourceKey );
void CL_SetRetailClientMessageViewangleDeltaFlag( void );
void CL_SetRetailClientMessageCGameImportGuardFlag( void );
void CL_SetRetailClientMessageRendererNodeCount( int nodeCount );
// American McGee's Alice: client-game import 47, which reports +cameralook and
// a pending centerview reset and returns the engine's camera accumulators.
float *QDECL CL_AliceGetCameraOffset( qboolean *lookActive, qboolean *resetView );
// Alice's cursor mode, which its menus switch; while it is on, mouse motion
// moves a cursor and MOUSE1-MOUSE5 stop at a button mask.
void CL_AliceSetCursorMode( qboolean on );
qboolean CL_AliceCursorMode( void );
void CL_AliceGetMouseState( int *x, int *y, int *buttons );
void CL_AliceSetMousePosition( int x, int y );
qboolean CL_AliceMouseButtonEvent( int key, qboolean down );

//
// cl_keys.cpp
//
extern  field_t     chatField;
extern  field_t     g_consoleField;
// Medal of Honor's chat line target: 0 all, -1 the team, else a client
// (set by its client game's import 159, cl_mohaa_cgame.cpp).
extern  int         chat_dmTarget;

void Field_Draw( field_t *edit, int x, int y, int width, qboolean showCursor, qboolean noColorEscape );
void Field_BigDraw( field_t *edit, int x, int y, int width, qboolean showCursor, qboolean noColorEscape );
void CL_ToggleMenu_f( void );
// Alice's alt and ctrl binding tables and their commands.
void CL_AliceInitKeyCommands( void );
void CL_AliceShutdownKeyCommands( void );
// A probe's synthetic input: in_synthesize and its command-line switch.
void CL_InitSyntheticInput( void );
void Key_AliceWriteModifierBindings( fileHandle_t f, int keynum );

//
// cl_parse.cpp
//
extern int cl_connectedToPureServer;
extern int cl_connectedToCheatServer;

void CL_ParseServerMessage( msg_t *msg );

//====================================================================

qboolean CL_UpdateVisiblePings_f( int source );
qboolean CL_ValidPakSignature( const byte *data, int len );


//
// console
//

extern cvar_t *con_scale;

void Con_CheckResize( void );
void Con_Init( void );
void Con_Shutdown( void );
void Con_ToggleConsole_f( void );
void Con_ClearNotify( void );
void Con_RunConsole( void );
void Con_DrawConsole( void );
void Con_MouseEvent( int dx, int dy );
void Con_SetMousePos( int x, int y );
qboolean Con_KeyEvent( int key, qboolean down );
qboolean Con_InputKey( int key );
void Con_CharEvent( int key );
void Con_CharEventUtf8( const unsigned char *bytes, int byteCount );
qboolean Con_UseAutoSay( void );
qboolean Con_UseRawSay( void );
void Con_PageUp( int lines );
void Con_PageDown( int lines );
void Con_Top( void );
void Con_Bottom( void );
void Con_Close( void );
// The chat line opened, to all or to the team, and closed; Medal of Honor's
// client game opens it through import 159.
void Con_OpenChat( qboolean team );
void Con_CloseChat( void );

void CL_LoadConsoleHistory( void );
void CL_SaveConsoleHistory( void );

//
// cl_scrn.cpp
//
void	SCR_Init( void );
void	SCR_Done( void );
void	SCR_UpdateScreen( void );
qboolean CL_UIMenusAreVisible( void );
int CL_UIMenuForRestart( void );
void CL_RestoreUIMenu( int menu );

void	SCR_DebugGraph( float value );

int		SCR_GetBigStringWidth( const char *str );	// returns in virtual 640x480 coordinates

void	SCR_AdjustFrom640( float *x, float *y, float *w, float *h );
void	SCR_AdjustFrom640Uniform( float *x, float *y, float *w, float *h );
qboolean SCR_AdjustRetailConnectBackdropUV( qhandle_t shader,
	float x, float y, float width, float height,
	float *s0, float *t0, float *s1, float *t1 );
qboolean SCR_AdjustRetailCGameLoadingBackdropUV(
	float x, float y, float width, float height,
	float *s0, float *t0, float *s1, float *t1 );
void	SCR_FillRect( float x, float y, float width, float height, 
					 const float *color );
void	SCR_DrawPic( float x, float y, float width, float height, qhandle_t hShader );
void	SCR_DrawNamedPic( float x, float y, float width, float height, const char *picname );

void	SCR_DrawBigString( int x, int y, const char *s, float alpha, qboolean noColorEscape );			// draws a string with embedded color control characters with fade
void	SCR_DrawStringExt( int x, int y, float size, const char *string, const float *setColor, qboolean forceColor, qboolean noColorEscape );
void	SCR_DrawSmallStringExt( int x, int y, const char *string, const float *setColor, qboolean forceColor, qboolean noColorEscape );
void	SCR_DrawSmallChar( int x, int y, int ch );
void	SCR_DrawSmallString( int x, int y, const char *s, int len );
void CL_CopyQ3Glconfig( void *destination, const glconfig_t *source );
void CL_CopyRTCWGlconfig( void *destination, const glconfig_t *source );
int CL_RTCWReadToken( int handle, void *destination );
// RTCW MP translation output is module-owned (32,000 bytes).
void CL_RTCWReloadTranslations( void );
void CL_RTCWTranslateString( vm_t *vm, intptr_t source, intptr_t destination );
void CL_RTCWLimboChat( vm_t *vm, intptr_t source );
qboolean CL_RTCWGetLimboString( vm_t *vm, intptr_t index, intptr_t destination );
int CL_RTCWSourceFileAndLine( int handle, char *filename, int *line );
void CL_RTCWRegisterFont( const char *name, int pointSize, void *destination );
qboolean CL_Q3KeyIsDown( int key );
void CL_Q3SetBinding( int key, const char *binding );
void CL_Q3KeyString( int key, char *buffer, int size, qboolean binding );
int CL_Q3KeyForBinding( const char *binding );
qboolean CL_RTCWKeyIsDown( int key );
void CL_RTCWSetBinding( int key, const char *binding );
void CL_RTCWKeyString( int key, char *buffer, int size, qboolean binding );
int CL_RTCWKeyForBinding( const char *binding );
int CL_RTCWGetKeyCatcher( void );
void CL_RTCWSetKeyCatcher( int catcher );
void CL_RTCWPopup( const char *name, qboolean close );
qboolean CL_STVEFKeyIsDown( int key );
void CL_STVEFSetBinding( int key, const char *binding );
void CL_STVEFKeyString( int key, char *buffer, int size, qboolean binding );
qboolean CL_WolfETKeyIsDown( int key );
void CL_WolfETSetBinding( int key, const char *binding );
void CL_WolfETKeyString( int key, char *buffer, int size, qboolean binding );
int CL_WolfETKeyForBinding( const char *binding );
void CL_WolfETKeysForBinding( const char *binding, int *first, int *second );
int CL_WolfETGetKeyCatcher( void );
void CL_WolfETSetKeyCatcher( int catcher );
void CL_WolfETReloadTranslations( void );
void CL_WolfETTranslateString( vm_t *vm, intptr_t source, intptr_t destination );
// The engine's own strings, through RTCW MP's or ET's catalogue.
void CL_TranslateClientString( const char *text, char *buffer, int size );
void CL_CopyWolfETGlconfig( void *destination, const glconfig_t *source );
int CL_WolfETReadToken( int handle, void *destination );
int CL_WolfETSourceFileAndLine( int handle, char *filename, int *line );
void CL_WolfETRegisterFont( const char *name, int pointSize, void *destination );
void	CL_CopyRetailGlconfig( void *glconfig );

typedef struct {
	float x;
	float y;
} retailFramebufferScale_t;

retailFramebufferScale_t CL_GetRetailFramebufferScale( void );
void	RE_DrawScaledText( int x, int y, const char *text, int fontHandle, float scale, int limit, float *maxX, qboolean forceColor, const float *baseColor );
void	RE_MeasureScaledText( const char *text, const char *end, int fontHandle, float scale, int limit, float *bounds );

//
// cl_cin.cpp
//

void CL_PlayCinematic_f( void );
void CL_PlayInGameCinematic_f( void );
qboolean SCR_DrawSTVEFSPInGameCinematic( void );
qboolean CL_STVEFSPCinematicKey( int key );
void SCR_DrawCinematic (void);
void SCR_RunCinematic (void);
void SCR_StopCinematic (void);
int CIN_PlayCinematic( const char *arg0, int xpos, int ypos, int width, int height, int bits);
e_status CIN_StopCinematic(int handle);
e_status CIN_RunCinematic (int handle);
void CIN_DrawCinematic (int handle);
void CIN_DrawCinematicUI( int handle );
void CIN_SetExtents (int handle, int x, int y, int w, int h);
void CIN_UploadCinematic(int handle);
void CIN_CloseAllVideos(void);

//
// cl_hud.cpp -- legacy cgame presentation; inert for Quake Live and MoHAA
//
void CL_HudInit( void );
void CL_HudShutdown( void );
void CL_HudResetCGame( void );
void CL_HudBeginFrame( void );
void CL_HudEndFrame( void );
void CL_HudRegisterShaderName( qhandle_t shader, const char *name );
void CL_HudAdjustRefdef( refdef_t *refdef );
qboolean CL_HudAdjustPic( float *x, float *y, float *w, float *h, qhandle_t shader );
// A screen of the game's own that owns the frame: its HUD rules pause and the
// screen is one centred 4:3 composite, or it is drawn stretched as retail drew
// it (one holding world views of its own). Set by the client each frame.
enum { HUD_PANEL_NONE, HUD_PANEL_CENTERED, HUD_PANEL_STRETCHED };
void CL_HudSetGamePanel( int panel );
// The screen a client game about to read its glconfig is told it has: the 4:3
// frame while HUD correction is on, so its own views and the points it
// projects onto its 2D are 4:3 ones; unchanged otherwise.
void CL_HudModuleScreen( int *width, int *height );
void CL_HudDrawStretchPic( float x, float y, float w, float h,
	float s1, float t1, float s2, float t2, qhandle_t shader );
// The client game's own pictures and colour changes, in order. When the HUD
// rules place text by line, a glyph is held until its line is complete, so
// the line is placed as one; flush before anything else draws (a scene).
void CL_HudQueueStretchPic( float x, float y, float w, float h,
	float s1, float t1, float s2, float t2, qhandle_t shader );
void CL_HudQueueColor( const float *rgba );
void CL_HudFlushQueue( void );
// Raven's client games draw in a virtual 640x480 screen: from it to the pixels
// of the screen the game was told it has, which the calls above take.
void CL_HudFromVirtual( float *x, float *y, float *w, float *h );
// The area, in 640x480 units, of a string the engine draws for a client game,
// around the glyphs it draws: the rules place them by the whole string.
void CL_HudBeginTextLine( float x1, float y1, float x2, float y2 );
void CL_HudEndTextLine( void );

//
// cl_entity_editor.cpp -- the in-game entity editor, every profile's
//
void CL_EntityEditorInit( void );
void CL_EntityEditorShutdown( void );
qboolean CL_EntityEditorActive( void );
// Text the server printed back (rcon replies among it), for a reload the
// editor asked of another server.
void CL_EntityEditorServerPrint( const char *text );
// Each takes its event while the editor holds the keys and the console does
// not, and returns qfalse to leave it to the rest of the client.
qboolean CL_EntityEditorKeyEvent( int key, qboolean down, unsigned time );
qboolean CL_EntityEditorCharEvent( int ch );
qboolean CL_EntityEditorMouseEvent( int dx, int dy );
qboolean CL_EntityEditorMouseAbsoluteEvent( int x, int y );
// KEYCATCH_ENTITYEDITOR while the editor wants an absolute pointer, 0 while it
// turns its camera with relative motion: platform layers add it to the
// catchers they present a menu pointer for.
int CL_EntityEditorPointerMask( void );
// Every profile's scene adapter calls this with a client game's view, just
// before the renderer draws it: the editor's camera takes its place.
void CL_EntityEditorAdjustView( refdef_t *refdef );

// The FnQ liquid feeds (cl_cgame.cpp) for scene adapters outside the Quake III
// client-game path: a view about to be rendered gives the underwater view its
// medium and the camera its ripples; a snapshot entity's position gives that
// entity its own. Each takes the profile's liquid bits and entity types.
void CL_LiquidFeedView( const refdef_t *refdef );
void CL_LiquidFeedEntity( int entityNum, const vec3_t origin );
// Whether entity positions are wanted at all this frame (r_liquid and
// r_liquidRipples), for an adapter that must ask its client game for them.
qboolean CL_LiquidFeedWantsEntities( void );
// The current snapshot's entities in a profile's own records, each with its
// own entity type and flags, and the local player (cl_cgame.cpp's census).
typedef void ( *clLiquidCensusVisit_t )( int number, int eType, int eFlags, qboolean local );
// After the menus and before the console.
void CL_EntityEditorDraw( void );
// Each renderer start: the editor's pictures, uploaded under the last one, are
// uploaded again.
void CL_EntityEditorRendererStarted( void );
// The client game's entity tokens: the local server's .ent override when it
// runs this map with one, else the renderer's BSP text.
void CL_EntityOverrideCGameInit( void );
void CL_EntityOverrideCGameShutdown( void );
qboolean CL_GetEntityToken( char *buffer, int size );

//
// cl_cgame.cpp
//
void CL_InitCGame( void );
void CL_ShutdownCGame( void );
void CL_STVEFSPAttachCGame( qboolean attach );
qboolean CL_GameCommand( void );
void CL_CGameRendering( stereoFrame_t stereo );
void CL_SetCGameTime( void );
void CL_CheckCGameNativeImportIntegrity( void );
void CL_ShowFirstTrackedPlayer( void );
void CL_ShowSecondTrackedPlayer( void );
int CL_GetCGamePhysicsTime( void );

//
// cl_webpak.cpp
//
void CL_WebPak_Init( void );
void CL_WebPak_Shutdown( void );
qboolean CL_WebPak_Available( void );
qboolean CL_WebPak_Fetch( const char *virtualPath, void **outBuffer, int *outLength );
int CL_WebPak_GetFileList( const char *path, const char *extension, char *listbuf, int bufsize );
qboolean CL_WebRequestResolve( const char *virtualPath, void **outBuffer, int *outLength );
qboolean CL_LauncherRequestData( const char *virtualPath, void **outBuffer, int *outLength );

//
// cl_webui.cpp
//
void CL_RefreshOnlineServicesBridgeState( void );
qboolean CL_IsSubscribedApp( int appId );
qhandle_t CL_GetAvatarImageHandle( unsigned int identityLow, unsigned int identityHigh );
void CL_ClearAvatarImageHandles( void );
qboolean CL_Steam_OpenOverlayUrl( const char *url );
qhandle_t CL_Steam_RegisterShader( const char *url );
qboolean CL_Steam_RequestServers( int requestMode );
qboolean CL_Steam_RequestServerDetails( unsigned int serverIp, unsigned short serverPort );
qboolean CL_Steam_RefreshServerList( void );
qboolean CL_Steam_CreateLobby( void );
qboolean CL_Steam_LeaveLobby( void );
qboolean CL_Steam_JoinLobby( const char *lobbyId );
qboolean CL_Steam_SetLobbyServer( unsigned int serverIp, unsigned short serverPort );
qboolean CL_Steam_ShowInviteOverlay( void );
qboolean CL_Steam_Invite( const char *steamId );
qboolean CL_Steam_SayLobby( const char *message );
qboolean CL_Steam_RequestAllUGC( int filter );
qboolean CL_Steam_RequestUserStats( const char *steamId );
qboolean CL_Steam_ActivateOverlayToUser( const char *dialog, const char *steamId );
qboolean CL_Steam_GetItemDownloadInfo( unsigned int itemIdLow, unsigned int itemIdHigh, unsigned long long *outDownloaded, unsigned long long *outTotal );
void CL_Steam_OnRichPresenceJoinRequested( const char *command );
void CL_Steam_OnGameServerChangeRequested( const char *server, const char *password );
void CL_SteamP2PFrame( void );
void QLWebHost_RegisterCommands( void );
void QLWebHost_UnregisterCommands( void );
void CL_WebHost_Init( void );
void CL_WebHost_Shutdown( void );
void CL_WebHost_Frame( void );
void CL_WebHost_RefreshSurfaceSize( void );
void CL_WebHost_InvalidateFactoryCatalog( void );
void CL_WebHost_InvalidateMapCatalog( void );
void CL_WebHost_BootstrapAwesomiumMenu( void );
qboolean CL_WebHost_HasLiveView( void );
qboolean CL_WebHost_HasBoundWindowObject( void );
qboolean CL_WebHost_HasDrawableSurface( void );
void CL_WebHost_DrawBrowserSurface( void );
void *CL_WebHost_GetCursorHandle( void );
void *CL_WebHost_OnChangeCursor( int cursorType );
void CL_WebHost_OnChangeTooltip( const char *tooltip );
void CL_WebHost_SetCursorPosition( int x, int y );
qboolean CL_WebHost_GetCursorPosition( int *x, int *y );
qboolean CL_WebHost_RequestCursorPosition( int *x, int *y );
qboolean CL_WebHost_ShowAfterDisconnect( void );
void CL_WebHost_HideBrowser( void );
void CL_WebHost_HideForGameTransition( void );
void CL_WebHost_NotifySoundsStopped( void );
void CL_WebHost_NotifyAppActivation( qboolean active );
void CL_WebHost_InvalidateConfigSnapshot( void );
void CL_WebHost_InvalidateFriendSnapshot( void );
void CL_WebHost_OnServerInfoResponse( const netadr_t *address, const char *infoString, int ping );
qboolean CL_WebHost_OnServerStatusResponseInfo( const netadr_t *address, const char *infoString );
void CL_WebHost_OnServerStatusResponsePlayer( const netadr_t *address, const char *playerLine );
void CL_WebHost_OnServerStatusResponseComplete( const netadr_t *address );
void CL_WebView_PublishEvent( const char *name, const char *payload );
void CL_WebView_InvokeCommNotice( const char *message );
void CL_WebView_PublishTaggedInfoString( const char *messageType, const char *infoString );
void CL_WebView_PublishGameError( const char *message );
void CL_WebView_PublishGameEnd( void );
void CL_WebView_PublishCvarChange( const char *name, const char *value, qboolean replicate );
void CL_WebView_PublishBindChanged( const char *name, const char *value );
void CL_WebView_PublishGameStartForAddress( const netadr_t *serverAddress );
void CL_WebView_PublishGameStart( void );
void CL_WebView_PublishGameDemo( const char *id, const char *name );
void CL_WebView_PublishGameScreenshot( const char *id, const char *name );
void CL_WebView_OnMouseMove( int x, int y );
void CL_WebView_OnMouseButtonEvent( int key, qboolean down );
void CL_WebView_OnMouseWheelEvent( int direction );
void CL_WebView_OnKeyEvent( int key, qboolean down );
void CL_WebView_OnCharEvent( int codepoint );
qhandle_t CL_AdvertisementBridge_SetupAdvertCellShader( const char *defaultContent, const void *rect, int cellId );
qhandle_t CL_AdvertisementBridge_RefreshAdvertCellShader( const char *defaultContent, const void *rect, int cellId );
qhandle_t CL_AdvertisementBridge_SetupUIAdvertCellShader( const char *defaultContent, const void *rect, int cellId );
qhandle_t CL_AdvertisementBridge_RefreshUIAdvertCellShader( const char *defaultContent, const void *rect, int cellId );
void CL_AdvertisementBridge_InitCGame( void );
void CL_AdvertisementBridge_ShutdownCGame( void );
void CL_AdvertisementBridge_InitUI( void );
void CL_AdvertisementBridge_Reserved21C0( void );
void CL_AdvertisementBridge_SetActiveAdvert( int cellId );
void CL_AdvertisementBridge_ActivateAdvert( int cellId );
void CL_AdvertisementBridge_UpdateAdvert( int handleOrToken, int area );
void CL_AdvertisementBridge_SetMapPath( const char *mapPath );
void CL_AdvertisementBridge_UpdateViewParameters( void );
void CL_AdvertisementBridge_RefreshLoadingViewParameters( void );
void CL_AdvertisementBridge_UpdateLoadingViewParameters( void );
void CL_AdvertisementBridge_SetFrameTime( int frameTime );
void CL_AdvertisementBridge_ClearDelay( void );
qboolean CL_AdvertisementBridge_IsDelayElapsed( void );
int CL_AdvertisementBridge_GetCellDisplayState( int cellId );
void CL_AdvertisementBridge_GetCellLabel( int cellId, char *buffer, int bufferSize );
int CL_AdvertisementBridge_GetLabelList1Count( void );
void CL_AdvertisementBridge_GetLabelList1Entry( int index, char *buffer, int bufferSize );
int CL_AdvertisementBridge_GetLabelList2Count( void );
void CL_AdvertisementBridge_GetLabelList2Entry( int index, char *buffer, int bufferSize );
qboolean CL_Awesomium_RequestResource( const char *virtualPath, void **outBuffer, int *outLength );
qboolean CL_Awesomium_Startup( const char *runtimePath, const char *basePath, const char *retailPath, const char *playerName, unsigned int appId, unsigned int steamIdLow, unsigned int steamIdHigh, int width, int height, const char *initialConfigJson, const char *initialMapJson, const char *initialFactoryJson, const char *initialFriendJson );
qboolean CL_Awesomium_OpenURL( const char *url );
void CL_Awesomium_Update( void );
qboolean CL_Awesomium_Resize( int width, int height );
int CL_Awesomium_SurfaceWidth( void );
int CL_Awesomium_SurfaceHeight( void );
qboolean CL_Awesomium_SurfaceDirty( void );
qboolean CL_Awesomium_IsLoading( void );
qboolean CL_Awesomium_IsCrashed( void );
int CL_Awesomium_LastErrorCode( void );
qboolean CL_Awesomium_ExecuteJavascript( const char *script, const char *frame );
qboolean CL_Awesomium_ExecuteJavascriptInteger( const char *script, const char *frame, int *outValue );
qboolean CL_Awesomium_PopJavascriptRequest( char *buffer, int bufferSize );
qboolean CL_Awesomium_SetZoom( int zoomPercent );
void CL_Awesomium_PauseRendering( void );
void CL_Awesomium_Unfocus( void );
qboolean CL_Awesomium_CopySurface( byte *destination, int width, int height, int rowSpan );
void CL_Awesomium_InjectMouseMove( int x, int y );
void CL_Awesomium_InjectMouseDown( int button );
void CL_Awesomium_InjectMouseUp( int button );
void CL_Awesomium_InjectMouseWheel( int direction );
void CL_Awesomium_InjectKeyboardEvent( unsigned int eventType, unsigned int virtualKeyCode, long nativeKeyCode );
void CL_Awesomium_Stop( void );
void CL_Awesomium_ClearCache( void );
void CL_Awesomium_Reload( qboolean ignoreCache );
void CL_Awesomium_Shutdown( void );
const char *CL_Awesomium_LastError( void );

//
// cl_ui.cpp
//
void CL_InitUI( void );
void CL_ShutdownUI( void );
qboolean CL_UIModuleReady( void );
void CL_STVEFSPConnectionText( const char *text, qboolean message );
int Key_GetCatcher( void );
void Key_SetCatcher( int catcher );


//
// cl_net_chan.cpp
//
void CL_Netchan_Transmit( netchan_t *chan, msg_t *msg );
void CL_Netchan_Enqueue( netchan_t *chan, msg_t *msg, int times );
qboolean CL_Netchan_Process( netchan_t *chan, msg_t *msg );

//
// cl_avi.cpp
//
qboolean CL_OpenAVIForWriting( const char *filename, qboolean pipe, qboolean reopen );
void CL_TakeVideoFrame( void );
void CL_WriteAVIVideoFrame( const byte *imageBuffer, int size );
void CL_WriteAVIAudioFrame( const byte *pcmBuffer, int size );
qboolean CL_CloseAVI( qboolean reopen );
qboolean CL_VideoRecording( void );

//
// cl_jpeg.cpp
//
size_t	CL_SaveJPGToBuffer( byte *buffer, size_t bufSize, int quality, int image_width, int image_height, byte *image_buffer, int padding );
void	CL_SaveJPG( const char *filename, int quality, int image_width, int image_height, byte *image_buffer, int padding );
void	CL_LoadJPG( const char *filename, unsigned char **pic, int *width, int *height );


// base backend functions
void CL_STVEFSPFeedbackInit( void *instance, void *window );
void CL_STVEFSPFeedbackShutdown( void );
void CL_STVEFSPFeedbackSuspend( void );
void CL_STVEFSPFeedbackFrame( qboolean active );
void CL_STVEFSPFeedbackPlay( int id, qboolean ensure );
void CL_STVEFSPFeedbackStop( int id );
void CL_STVEFSPFeedbackStopAll( void );
void	HandleEvents( void );
void	CL_NotifyWindowResize( int width, int height, qboolean preserveWindow );
void	CL_CompleteWindowResize( void );
void	CL_CancelWindowResize( void );
qboolean CL_IsWindowResizeRestart( void );

// platform-specific
void	GLimp_InitGamma(glconfig_t *config);
void	GLimp_SetGamma(unsigned char red[256], unsigned char green[256], unsigned char blue[256]);
void	GLimp_QueryDisplayOutput(rendererDisplayOutput_t *output);
void	GLimp_InvalidateConfig( void );

// OpenGL
#ifdef USE_OPENGL_API
void	GLimp_Init( glconfig_t *config );
void	GLimp_Shutdown( qboolean unloadDLL );
void	GLimp_EndFrame( void );
void	*GL_GetProcAddress( const char *name );
#endif

// Vulkan
#ifdef USE_VULKAN_API
void	VKimp_Init( glconfig_t *config );
void	VKimp_Shutdown( qboolean unloadDLL );
void	*VK_GetInstanceProcAddr( VkInstance instance, const char *name );
qboolean VK_CreateSurface( VkInstance instance, VkSurfaceKHR* pSurface );
#endif

#ifdef __cplusplus
}
#endif
