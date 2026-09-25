/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */

/* The engine's wrappers over the vendored TIKI runtime; see mohaa_tiki.h.
 * The runtime's own headers are included beside the facade so that every
 * facade declaration is checked against its definition. */

#include "qcommon/q_shared.h"
#include "qcommon/qcommon.h"
#include "corepp/tiki.h"
#include "mohaa_tiki.h"

#include <cstddef>

/* The facade's view of the module-read prefix must be the runtime's. */
static_assert( sizeof( mohaa_dtikicmd_t ) == sizeof( dtikicmd_t ), "TIKI command record" );
/* dtiki_t is not standard-layout, which makes offsetof conditionally
 * supported; every compiler this tree builds with supports it. */
#if defined( __GNUC__ )
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Winvalid-offsetof"
#endif
static_assert( offsetof( dtiki_t, name ) == 0 && offsetof( dtiki_t, a ) == sizeof( void * ),
	"the retail game reads a TIKI's name and animation record at its start" );
#if defined( __GNUC__ )
#pragma GCC diagnostic pop
#endif
#if defined( _M_IX86 ) || defined( __i386__ )
/* gamex86 1.00 walks a->num_server_initcmds at +0x14 and the records at
 * +0x18 (0x310c8f51, 0x310c8f62). */
static_assert( offsetof( dtikianim_t, num_server_initcmds ) == 0x14, "server init command count" );
static_assert( offsetof( dtikianim_t, server_initcmds ) == 0x18, "server init commands" );
static_assert( sizeof( tiki_cmd_t ) == 4 + 8 * TIKI_MAX_COMMANDS, "frame command record" );
/* Both 1.00 modules read an alias record they are handed at these offsets:
 * pitch, volume and their spreads, the distances and the channel
 * (gamex86.dll 0x310ccd8b..0x310ccdfc), and the subtitle right after
 * `streamed` (0x310cce5e; cgamex86.dll 0x30015f5a). */
static_assert( offsetof( AliasListNode_t, pitch ) == 0xb4, "alias pitch" );
static_assert( offsetof( AliasListNode_t, volume ) == 0xb8, "alias volume" );
static_assert( offsetof( AliasListNode_t, pitchMod ) == 0xbc, "alias pitch spread" );
static_assert( offsetof( AliasListNode_t, volumeMod ) == 0xc0, "alias volume spread" );
static_assert( offsetof( AliasListNode_t, dist ) == 0xc4, "alias distance" );
static_assert( offsetof( AliasListNode_t, maxDist ) == 0xc8, "alias maximum distance" );
static_assert( offsetof( AliasListNode_t, channel ) == 0xcc, "alias channel" );
static_assert( offsetof( AliasListNode_t, subtitle ) == 0xd4, "alias subtitle" );
#endif

extern "C" {

void *MOHAA_TIKI_TransformInternal( dtiki_t *tiki, int entnum, int tagnum ) {
	return TIKI_TransformInternal( tiki, entnum, tagnum );
}

dtikianim_t *MOHAA_TIKI_Anim( dtiki_t *tiki ) {
	return tiki ? tiki->a : nullptr;
}

/* OpenMoHAA's PF_Alias_Add and its neighbours (code/server/sv_game.c,
 * GPL-2.0-or-later): a model's list is created the first time something is
 * added to it, and a query of a model with no list finds nothing. */
qboolean MOHAA_TIKI_AliasAdd( dtiki_t *tiki, const char *alias, const char *name, const char *parameters ) {
	if ( !tiki || !tiki->a ) {
		return qfalse;
	}
	if ( !tiki->a->alias_list ) {
		tiki->a->alias_list = AliasList_New( tiki->a->name );
	}
	return Alias_ListAdd( static_cast<AliasList_t *>( tiki->a->alias_list ), alias, name, parameters );
}

const char *MOHAA_TIKI_AliasFindRandom( dtiki_t *tiki, const char *alias, AliasListNode_t **ret ) {
	if ( !tiki || !tiki->a || !tiki->a->alias_list ) {
		return nullptr;
	}
	return Alias_ListFindRandom( static_cast<AliasList_t *>( tiki->a->alias_list ), alias, ret );
}

void MOHAA_TIKI_AliasDump( dtiki_t *tiki ) {
	if ( tiki && tiki->a && tiki->a->alias_list ) {
		Alias_ListDump( static_cast<AliasList_t *>( tiki->a->alias_list ) );
	}
}

void MOHAA_TIKI_AliasUpdateDialog( dtikianim_t *tiki, const char *alias ) {
	if ( tiki && tiki->alias_list ) {
		Alias_ListUpdateDialog( static_cast<AliasList_t *>( tiki->alias_list ), alias );
	}
}

qboolean MOHAA_GlobalAliasAdd( const char *alias, const char *name, const char *parameters ) {
	return Alias_Add( alias, name, parameters );
}

const char *MOHAA_GlobalAliasFindRandom( const char *alias, AliasListNode_t **ret ) {
	return Alias_FindRandom( alias, ret );
}

void MOHAA_GlobalAliasDump( void ) {
	Alias_Dump();
}

void MOHAA_GlobalAliasClear( void ) {
	Alias_Clear();
}

void *MOHAA_AliasListNew( const char *name ) {
	return AliasList_New( name ? name : "" );
}

qboolean MOHAA_AliasListAdd( void *list, const char *alias, const char *name, const char *parameters ) {
	if ( !list || !alias || !name ) {
		return qfalse;
	}
	return Alias_ListAdd( static_cast<AliasList_t *>( list ), alias, name, parameters ? parameters : "" );
}

const char *MOHAA_AliasListFindRandom( void *list, const char *alias, AliasListNode_t **ret ) {
	if ( ret ) {
		*ret = nullptr;
	}
	if ( !list || !alias ) {
		return nullptr;
	}
	return Alias_ListFindRandom( static_cast<AliasList_t *>( list ), alias, ret );
}

void MOHAA_AliasListFindRandomRange( void *list, const char *alias, int *minIndex, int *maxIndex,
	float *totalWeight ) {
	if ( !list || !alias || !minIndex || !maxIndex || !totalWeight ) {
		return;
	}
	Alias_ListFindRandomRange( static_cast<AliasList_t *>( list ), alias, minIndex, maxIndex, totalWeight );
}

void *MOHAA_AliasGetGlobalList( void ) {
	return Alias_GetGlobalList();
}

int MOHAA_TIKI_BoneNumFromName( dtiki_t *tiki, const char *name ) {
	return tiki && name ? tiki->GetBoneNumFromName( name ) : -1;
}

qboolean MOHAA_TIKI_IsCharacter( dtiki_t *tiki ) {
	return tiki && tiki->a && tiki->a->bIsCharacter ? qtrue : qfalse;
}

int MOHAA_TIKI_NumServerInitCommands( dtiki_t *tiki ) {
	return tiki && tiki->a ? tiki->a->num_server_initcmds : 0;
}

/* OpenMoHAA's UI_IsResourceLoaded (code/client/cl_ui.cpp, GPL-2.0-or-later)
 * for the kinds the runtime caches: a model's definition and its parts, and
 * a skeletal animation. */
qboolean MOHAA_ResourceCached( const char *name ) {
	if ( !name || !name[0] ) {
		return qfalse;
	}
	switch ( name[0] ) {
	case 'a':
	case 'b':
	case 'c':
	case 'd':
	case 'e':
	case 'h':
		return TIKI_FindTikiAnim( name + 1 ) ? qtrue : qfalse;
	case 'g':
		return SkeletorCacheFindFilename( name + 1, nullptr ) ? qtrue : qfalse;
	default:
		return qfalse;
	}
}

} // extern "C"
