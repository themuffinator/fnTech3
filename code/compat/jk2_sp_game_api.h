/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_COMPAT_JK2_SP_GAME_API_H
#define FNTECH3_COMPAT_JK2_SP_GAME_API_H

#include <stdint.h>

/* Retail Jedi Outcast 1.02a, Win32 x86. GetGameAPI at 0x200970a0
 * copies 0x67 words and fills the 19-word export table at 0x203abbd8.
 * jk2sp.exe 0x4368a0 constructs those imports and checks version 6.
 * OpenJK's current codeJK2 API is version 10 and must not be substituted.
 * Only the measured bootstrap portion is named here. World records, saves,
 * Ghoul2 and the remaining import signatures are deliberately opaque. */
#define FNTECH3_JK2_SP_GAME_API_VERSION 6
#define FNTECH3_JK2_SP_GAME_IMPORT_COUNT 103
#define FNTECH3_JK2_SP_GAME_IMPORT_BYTES 412
#define FNTECH3_JK2_SP_GAME_EXPORT_BYTES 76
#define FNTECH3_JK2_SP_GENTITY_BYTES 1288
#define FNTECH3_JK2_SP_GCLIENT_BYTES 3188
#define FNTECH3_JK2_SP_ENTITY_CLIENT_OFFSET 252

typedef struct fntech3_jk2_sp_cvar_s fntech3_jk2_sp_cvar_t;
struct fntech3_jk2_sp_cvar_s {
	char *name, *string, *resetString, *latchedString;
	int32_t flags, modified, modificationCount;
	float value;
	int32_t integer;
	fntech3_jk2_sp_cvar_t *next;
};

typedef void (*fntech3_jk2_sp_opaque_import_t)( void );
typedef struct fntech3_jk2_sp_game_import_s {
	void (*Printf)( const char *fmt, ... );                         /*   0 */
	void (*WriteCam)( const char *text );                         /*   4 */
	void (*Error)( int32_t level, const char *fmt, ... );           /*   8 */
	int32_t (*Milliseconds)( void );                             /*  12 */
	fntech3_jk2_sp_cvar_t *(*cvar)( const char *, const char *, int32_t );
	void (*cvar_set)( const char *, const char * );
	int32_t (*Cvar_VariableIntegerValue)( const char * );
	void (*Cvar_VariableStringBuffer)( const char *, char *, int32_t );
	int32_t (*argc)( void );
	char *(*argv)( int32_t );
	int32_t (*FS_FOpenFile)( const char *, int32_t *, int32_t );
	int32_t (*FS_Read)( void *, int32_t, int32_t );
	int32_t (*FS_Write)( const void *, int32_t, int32_t );
	void (*FS_FCloseFile)( int32_t );
	int32_t (*FS_ReadFile)( const char *, void ** );
	void (*FS_FreeFile)( void * );
	int32_t (*FS_GetFileList)( const char *, const char *, char *, int32_t );
	fntech3_jk2_sp_opaque_import_t AppendToSaveGame;                /*  68 */
	fntech3_jk2_sp_opaque_import_t ReadFromSaveGame;                /*  72 */
	fntech3_jk2_sp_opaque_import_t ReadFromSaveGameOptional;        /*  76 */
	void (*SendConsoleCommand)( const char * );                    /*  80 */
	void (*DropClient)( int32_t, const char * );
	void (*SendServerCommand)( int32_t, const char *, ... );
	void (*SetConfigstring)( int32_t, const char * );
	void (*GetConfigstring)( int32_t, char *, int32_t );
	void (*GetUserinfo)( int32_t, char *, int32_t );
	void (*SetUserinfo)( int32_t, const char * );
	void (*GetServerinfo)( char *, int32_t );
	fntech3_jk2_sp_opaque_import_t SetBrushModel;                   /* 112 */
	fntech3_jk2_sp_opaque_import_t trace;
	fntech3_jk2_sp_opaque_import_t pointcontents;
	int32_t (*inPVS)( const float *, const float * );
	int32_t (*inPVSIgnorePortals)( const float *, const float * );
	fntech3_jk2_sp_opaque_import_t AdjustAreaPortalState;
	int32_t (*AreasConnected)( int32_t, int32_t );
	fntech3_jk2_sp_opaque_import_t linkentity;
	fntech3_jk2_sp_opaque_import_t unlinkentity;
	fntech3_jk2_sp_opaque_import_t EntitiesInBox;
	fntech3_jk2_sp_opaque_import_t EntityContact;
	int32_t *VoiceVolume;                                        /* 156 */
	void *(*Malloc)( int32_t bytes, int32_t tag, int32_t zero );    /* 160 */
	void (*Free)( void * );                                      /* 164 */
	fntech3_jk2_sp_opaque_import_t unserved[61];                   /* 168 */
} fntech3_jk2_sp_game_import_t;

typedef struct fntech3_jk2_sp_game_export_s {
	int32_t apiversion;
	void (*Init)( const char *mapname, const char *spawntarget, int32_t checksum,
		const char *entities, int32_t levelTime, int32_t seed, int32_t globalTime,
		int32_t savedGameJustLoaded, int32_t loadTransition );
	void (*Shutdown)( void );
	void (*WriteLevel)( int32_t );
	void (*ReadLevel)( int32_t, int32_t );
	int32_t (*GameAllowedToSaveHere)( void );
	char *(*ClientConnect)( int32_t, int32_t, int32_t );
	void (*ClientBegin)( int32_t, void *, int32_t );
	void (*ClientUserinfoChanged)( int32_t );
	void (*ClientDisconnect)( int32_t );
	void (*ClientCommand)( int32_t );
	void (*ClientThink)( int32_t, void * );
	void (*RunFrame)( int32_t );
	int32_t (*ConsoleCommand)( void );                            /*  52 */
	void (*PrintEntClassname)( int32_t );                         /*  56 */
	fntech3_jk2_sp_opaque_import_t unnamed_60;                     /*  60 */
	void *gentities;                                             /*  64 */
	int32_t gentitySize;                                         /*  68 */
	int32_t num_entities;                                        /*  72 */
} fntech3_jk2_sp_game_export_t;

typedef fntech3_jk2_sp_game_export_t *(*fntech3_jk2_sp_GetGameAPI_t)( fntech3_jk2_sp_game_import_t * );

#ifdef __cplusplus
#include <cstddef>
namespace fntech3::jk2::sp {
enum class ExportError { None, MissingTable, Version, MissingFunction, EntityStride, EntityCount };
inline ExportError ValidateExports( const fntech3_jk2_sp_game_export_t *game ) {
	if ( !game ) return ExportError::MissingTable;
	if ( game->apiversion != FNTECH3_JK2_SP_GAME_API_VERSION ) return ExportError::Version;
	if ( !game->Init || !game->Shutdown || !game->WriteLevel || !game->ReadLevel ||
		!game->GameAllowedToSaveHere || !game->ClientConnect || !game->ClientBegin ||
		!game->ClientUserinfoChanged || !game->ClientDisconnect || !game->ClientCommand ||
		!game->ClientThink || !game->RunFrame || !game->ConsoleCommand || !game->PrintEntClassname ||
		!game->unnamed_60 ) return ExportError::MissingFunction;
	if ( game->gentitySize != FNTECH3_JK2_SP_GENTITY_BYTES ) return ExportError::EntityStride;
	if ( game->num_entities < 0 || game->num_entities > 1024 ||
		( game->num_entities && !game->gentities ) ) return ExportError::EntityCount;
	return ExportError::None;
}
}
#if UINTPTR_MAX == UINT32_MAX
static_assert( sizeof( fntech3_jk2_sp_game_import_t ) == FNTECH3_JK2_SP_GAME_IMPORT_BYTES );
static_assert( sizeof( fntech3_jk2_sp_game_export_t ) == FNTECH3_JK2_SP_GAME_EXPORT_BYTES );
static_assert( offsetof( fntech3_jk2_sp_game_import_t, Error ) == 8 );
static_assert( offsetof( fntech3_jk2_sp_game_import_t, VoiceVolume ) == 156 );
static_assert( offsetof( fntech3_jk2_sp_game_import_t, Malloc ) == 160 );
static_assert( offsetof( fntech3_jk2_sp_game_import_t, unserved ) == 168 );
static_assert( offsetof( fntech3_jk2_sp_game_export_t, ConsoleCommand ) == 52 );
static_assert( offsetof( fntech3_jk2_sp_game_export_t, gentities ) == 64 );
static_assert( offsetof( fntech3_jk2_sp_cvar_t, value ) == 28 );
static_assert( offsetof( fntech3_jk2_sp_cvar_t, integer ) == 32 );
#endif
#endif
#endif
