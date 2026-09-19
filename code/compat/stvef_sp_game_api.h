/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_STVEF_SP_GAME_API_H
#define FNTECH3_STVEF_SP_GAME_API_H

#include "stvef_game_abi.h"

/*
 * Elite Force SP's game boundary. efgamex86.dll exports GetGameAPI, which
 * takes the engine's import table and returns the game's export table; the
 * cgame in the same library uses dllEntry and vmMain instead. Retail
 * stvoy.exe 1.20 fills the 42 imports in this order (SV_InitGameProgs
 * 0x425490) and refuses any apiversion but 6. It calls Init at +4, Shutdown
 * at +8 and ConsoleCommand at +0x34, and reads the client pointer at +0xe8 of
 * a gentity. Names follow Raven's SP SDK; no text from it is incorporated.
 *
 * Every function is __cdecl and every pointer 32 bits: Raven shipped the
 * module for Win32 x86 only.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* Raven's error levels, which retail's Com_Error (0x411170) treats as fatal,
 * drop (after writing the "current" save), disconnect and missing CD. */
enum {
	FNTECH3_STVEF_SP_ERR_FATAL = 0,
	FNTECH3_STVEF_SP_ERR_DROP = 1,
	FNTECH3_STVEF_SP_ERR_DISCONNECT = 2,
	FNTECH3_STVEF_SP_ERR_NEED_CD = 3
};

/* SavedGameJustLoaded_e. */
enum {
	FNTECH3_STVEF_SP_SAVED_NO = 0,
	FNTECH3_STVEF_SP_SAVED_FULL = 1,
	FNTECH3_STVEF_SP_SAVED_AUTO = 2
};

typedef struct fntech3_stvef_sp_game_import_s {
	void (*Printf)( const char *fmt, ... );
	void (*WriteCam)( const char *text );
	void (*Error)( int level, const char *fmt, ... );
	int32_t (*Milliseconds)( void );
	fntech3_stvef_sp_cvar_t *(*cvar)( const char *name, const char *value, int32_t flags );
	void (*cvar_set)( const char *name, const char *value );
	int32_t (*Cvar_VariableIntegerValue)( const char *name );
	void (*Cvar_VariableStringBuffer)( const char *name, char *buffer, int32_t size );
	int32_t (*argc)( void );
	char *(*argv)( int32_t n );
	int32_t (*FS_FOpenFile)( const char *qpath, int32_t *file, int32_t mode );
	int32_t (*FS_Read)( void *buffer, int32_t length, int32_t file );
	int32_t (*FS_Write)( const void *buffer, int32_t length, int32_t file );
	void (*FS_FCloseFile)( int32_t file );
	int32_t (*FS_ReadFile)( const char *name, void **buffer );
	void (*FS_FreeFile)( void *buffer );
	int32_t (*FS_GetFileList)( const char *path, const char *extension, char *list, int32_t size );
	int32_t (*AppendToSaveGame)( uint32_t chunk, void *data, int32_t length );
	int32_t (*ReadFromSaveGame)( uint32_t chunk, void *address, int32_t length, void **addressPointer );
	int32_t (*ReadFromSaveGameOptional)( uint32_t chunk, void *address, int32_t length, void **addressPointer );
	void (*SendConsoleCommand)( const char *text );
	void (*DropClient)( int32_t clientNum, const char *reason );
	void (*SendServerCommand)( int32_t clientNum, const char *fmt, ... );
	void (*SetConfigstring)( int32_t num, const char *string );
	void (*GetConfigstring)( int32_t num, char *buffer, int32_t size );
	void (*GetUserinfo)( int32_t num, char *buffer, int32_t size );
	void (*SetUserinfo)( int32_t num, const char *buffer );
	void (*GetServerinfo)( char *buffer, int32_t size );
	void (*SetBrushModel)( fntech3_stvef_sp_gentity_t *entity, const char *name );
	void (*trace)( fntech3_stvef_trace_t *results, const float *start, const float *mins,
		const float *maxs, const float *end, int32_t passEntityNum, int32_t contentmask );
	int32_t (*pointcontents)( const float *point, int32_t passEntityNum );
	int32_t (*inPVS)( const float *p1, const float *p2 );
	int32_t (*inPVSIgnorePortals)( const float *p1, const float *p2 );
	void (*AdjustAreaPortalState)( fntech3_stvef_sp_gentity_t *entity, int32_t open );
	int32_t (*AreasConnected)( int32_t area1, int32_t area2 );
	void (*linkentity)( fntech3_stvef_sp_gentity_t *entity );
	void (*unlinkentity)( fntech3_stvef_sp_gentity_t *entity );
	int32_t (*EntitiesInBox)( const float *mins, const float *maxs,
		fntech3_stvef_sp_gentity_t **list, int32_t maxcount );
	int32_t (*EntityContact)( const float *mins, const float *maxs,
		const fntech3_stvef_sp_gentity_t *entity );
	/* The engine's per-entity voice volume, which the game reads for
	 * lip-sync (FNTECH3_STVEF_SP_LIPSYNC_ENTITIES entries). */
	int32_t *S_Override;
	void *(*Malloc)( int32_t bytes );
	void (*Free)( void *buffer );
} fntech3_stvef_sp_game_import_t;

typedef struct fntech3_stvef_sp_game_export_s {
	int32_t apiversion;
	void (*Init)( const char *mapname, const char *spawntarget, int32_t checkSum,
		const char *entstring, int32_t levelTime, int32_t randomSeed, int32_t globalTime,
		int32_t savedGameJustLoaded, int32_t loadTransition );
	void (*Shutdown)( void );
	void (*WriteLevel)( int32_t autosave );
	void (*ReadLevel)( int32_t autosave, int32_t loadTransition );
	int32_t (*GameAllowedToSaveHere)( void );
	char *(*ClientConnect)( int32_t clientNum, int32_t firstTime, int32_t savedGameJustLoaded );
	void (*ClientBegin)( int32_t clientNum, fntech3_stvef_sp_usercmd_t *cmd, int32_t savedGameJustLoaded );
	void (*ClientUserinfoChanged)( int32_t clientNum );
	void (*ClientDisconnect)( int32_t clientNum );
	void (*ClientCommand)( int32_t clientNum );
	void (*ClientThink)( int32_t clientNum, fntech3_stvef_sp_usercmd_t *cmd );
	void (*RunFrame)( int32_t levelTime );
	int32_t (*ConsoleCommand)( void );
	fntech3_stvef_sp_gentity_t *gentities;
	int32_t gentitySize;
	int32_t num_entities;
} fntech3_stvef_sp_game_export_t;

typedef fntech3_stvef_sp_game_export_t *(*fntech3_stvef_sp_GetGameAPI_t)( fntech3_stvef_sp_game_import_t *import );

#ifdef __cplusplus
}
#endif

#endif /* FNTECH3_STVEF_SP_GAME_API_H */
