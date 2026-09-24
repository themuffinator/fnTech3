/*
===========================================================================
Copyright (C) 2026 fnTech3 contributors

This file is part of fnTech3 and is distributed under the terms of the GNU
General Public License, version 2 or (at your option) any later version.
===========================================================================
*/

/* Alice's alias lists (code/qcommon/alice_alias.cpp) against the behaviour
 * read out of retail alice.exe, 0x00422850 to 0x00423b12. The random source
 * and the clock are pinned, so every pick below is the one retail's formula
 * gives for that draw. No retail data is used. */

#include "alice_tiki.h"
#include "qcommon.h"

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

#define CHECK( x ) do { if ( !( x ) ) { \
	std::cerr << __LINE__ << ": " #x "\n"; std::exit( 1 ); } } while ( 0 )

using namespace fntech3::alice::alias;

static std::vector<std::string> printed;
static std::vector<std::string> developer;

static std::string Format( const char *fmt, va_list args )
{
	char text[4096];
	std::vsnprintf( text, sizeof( text ), fmt, args );
	return text;
}

extern "C" {

void QDECL Com_Printf( const char *fmt, ... )
{
	va_list args;
	va_start( args, fmt );
	printed.push_back( Format( fmt, args ) );
	va_end( args );
}

void QDECL Com_DPrintf( const char *fmt, ... )
{
	va_list args;
	va_start( args, fmt );
	developer.push_back( Format( fmt, args ) );
	va_end( args );
}

void QDECL Com_Error( errorParm_t, const char *fmt, ... )
{
	va_list args;
	va_start( args, fmt );
	std::cerr << "unexpected Com_Error: " << Format( fmt, args ) << "\n";
	va_end( args );
	std::exit( 1 );
}

int Sys_Milliseconds( void )
{
	return 0;
}

} // extern "C"

static int now;
static int Clock( void ) { return now; }

static std::vector<int> draws;
static std::size_t drawn;
static int Draw( void )
{
	CHECK( drawn < draws.size() );
	return draws[drawn++];
}

static void Script( std::vector<int> values )
{
	draws = values;
	drawn = 0;
}

static void Reset()
{
	printed.clear();
	developer.clear();
	now = 0;
	Script( {} );
}

static bool Is( const char *text, const char *expected )
{
	return text && !std::strcmp( text, expected );
}

static Record *Named( List *list, const char *name )
{
	for ( Record *r = list->head; r; r = r->next ) {
		if ( !std::strcmp( r->name, name ) ) return r;
	}
	return nullptr;
}

static Actor *ActorOf( Record *record, int actor )
{
	for ( Actor *a = record->actors; a; a = a->next ) {
		if ( a->actor == actor ) return a;
	}
	return nullptr;
}

/* The fields the game module's savegame code reads (fgamex86.dll 0x100a3cb0),
 * at the offsets it reads them, on the 32-bit build that loads it. */
static void Layout()
{
	if ( sizeof( void * ) != 4 ) return;
	CHECK( sizeof( List ) == 0x30 );
	CHECK( sizeof( Record ) == 0xc4 );
	CHECK( sizeof( Actor ) == 0x14 );

	Reset();
	List *list = CreateList( "fixture" );
	CHECK( Add( list, "line", "sound/line.wav", nullptr ) );
	UpdateDialog( list, "line", 3, 1, 1234 );
	UpdateActorDialog( list, "line", 7, 2, 1, 999 );

	auto u32 = []( const void *base, int offset ) {
		std::uint32_t v;
		std::memcpy( &v, static_cast<const unsigned char *>( base ) + offset, 4 );
		return v;
	};
	const void *head = reinterpret_cast<const void *>( static_cast<std::uintptr_t>( u32( list, 0x2c ) ) );
	CHECK( head == list->head );
	CHECK( !std::strcmp( static_cast<const char *>( head ), "line" ) );
	CHECK( u32( head, 0xb0 ) == 3 );
	CHECK( static_cast<const unsigned char *>( head )[0xb4] == 1 );
	CHECK( u32( head, 0xb8 ) == 1234 );
	const void *actor = reinterpret_cast<const void *>( static_cast<std::uintptr_t>( u32( head, 0xbc ) ) );
	CHECK( actor && u32( actor, 0 ) == 7 && u32( actor, 4 ) == 2 );
	CHECK( static_cast<const unsigned char *>( actor )[8] == 1 && u32( actor, 0xc ) == 999 );
	CHECK( u32( actor, 0x10 ) == 0 );
	CHECK( u32( head, 0xc0 ) == 0 );
	FreeList( list );
}

/* 0x00422cd0 and 0x00422a60: storage and every parameter. */
static void AddAndParameters()
{
	Reset();
	List *list = CreateList( "fixture" );
	CHECK( !list->dirty && !list->count && !list->head && !list->sorted );

	CHECK( Add( list, "Snd_Pain1", "sound/pain1.wav", nullptr ) );
	Record *r = list->head;
	CHECK( !std::strcmp( r->name, "snd_pain1" ) );
	CHECK( !std::strcmp( r->realName, "sound/pain1.wav" ) );
	CHECK( r->weight == 1.0f && r->maxUse == -1 && !r->global && !r->stop );
	CHECK( r->timeout == 0.0f && !r->timesPlayed && !r->played && !r->lastPlayed && !r->actors );
	CHECK( list->dirty == 1 && list->count == 1 );

	/* 31 characters of name, 127 of real name. */
	const std::string longReal( 200, 'x' );
	CHECK( Add( list, "Zabcdefghijklmnopqrstuvwxyz0123456789", longReal.c_str(), "" ) );
	CHECK( !std::strcmp( list->head->name, "zabcdefghijklmnopqrstuvwxyz0123" ) );
	CHECK( std::strlen( list->head->realName ) == 127 );

	CHECK( Add( list, "a", "x", "global stop timeout 2.5 maxuse 3 weight 0.25" ) );
	r = list->head;
	CHECK( r->global == 1 && r->stop == 1 && r->timeout == 2.5f && r->maxUse == 3 && r->weight == 0.25f );
	CHECK( printed.empty() );

	/* Out of range values are reported and ignored; an unknown word is
	 * reported and the next word is read as a parameter too. */
	CHECK( Add( list, "b", "x", "timeout 0 maxuse -2 weight -1 volume 5" ) );
	r = list->head;
	CHECK( r->timeout == 0.0f && r->maxUse == -1 && r->weight == 1.0f );
	CHECK( printed.size() == 5 );
	CHECK( printed[0] == "Timeout value out of range in b alias\n" );
	CHECK( printed[1] == "Maximum use value out of range in b alias\n" );
	CHECK( printed[2] == "Weight value out of range in b alias\n" );
	CHECK( printed[3] == "Unknown parameter for b alias\n" );
	CHECK( printed[4] == "Unknown parameter for b alias\n" );

	/* Keywords ignore case, values may be quoted, comments are skipped, and a
	 * keyword without its value ends the string. */
	printed.clear();
	CHECK( Add( list, "c", "x", "WEIGHT \"3\" // comment\n timeout" ) );
	CHECK( list->head->weight == 3.0f && list->head->timeout == 0.0f && printed.empty() );

	/* The run-time's atof has no inf; atoi takes the leading digits. */
	CHECK( Add( list, "d", "x", "weight inf maxuse 7x" ) );
	CHECK( list->head->weight == 1.0f && list->head->maxUse == 7 );
	CHECK( printed.size() == 1 && printed[0] == "Weight value out of range in d alias\n" );

	/* A block comment ends at the first slash after a star, its own included,
	 * and takes the rest of its line. */
	printed.clear();
	CHECK( Add( list, "e", "x", "/*/ weight 2\nweight 4" ) );
	CHECK( list->head->weight == 4.0f && printed.empty() );

	/* A quoted value running off the end is taken as it stands. */
	CHECK( Add( list, "f", "x", "weight \"0.5" ) );
	CHECK( list->head->weight == 0.5f && printed.empty() );

	/* Bytes from 0x80 are not whitespace. */
	CHECK( Add( list, "g", "x", "stop\xa0global" ) );
	CHECK( !list->head->stop && !list->head->global );
	CHECK( printed.size() == 1 && printed[0] == "Unknown parameter for g alias\n" );

	/* Retail dereferences both names; they are refused here. */
	CHECK( !Add( list, nullptr, "x", nullptr ) );
	CHECK( !Add( list, "h", nullptr, nullptr ) );
	CHECK( !Add( nullptr, "h", "x", nullptr ) );
	FreeList( list );
}

/* The duplicate rules of 0x00422cd0 and the sort at 0x00422e30. */
static void Duplicates()
{
	Reset();
	List *list = CreateList( "fixture" );

	/* Before a sort: a stored name that begins the new one refuses it,
	 * silently -- compared with the new name as given, case and all. */
	CHECK( Add( list, "idle", "idle.wav", nullptr ) );
	CHECK( Add( list, "idle2", "other.wav", nullptr ) );
	CHECK( list->count == 1 && developer.empty() );
	CHECK( Add( list, "IDLE3", "idle3.wav", nullptr ) );
	CHECK( list->count == 2 );

	/* The sort takes "idle3" first -- "idle" does not compare below it over
	 * its own four characters -- and then keeps "idle" as well. */
	Sort( list );
	CHECK( list->count == 2 && !list->dirty );
	CHECK( !std::strcmp( list->sorted[0]->name, "idle3" ) );
	CHECK( !std::strcmp( list->sorted[1]->name, "idle" ) );
	/* The chain is relinked descending: the last kept first. */
	CHECK( list->head == list->sorted[1] && list->head->next == list->sorted[0] );
	CHECK( !list->sorted[0]->next );

	/* After a sort: a line the new name prefixes refuses it, reported under
	 * developer only when the real names differ. */
	CHECK( Add( list, "idl", "elsewhere.wav", nullptr ) );
	CHECK( list->count == 2 && !list->dirty );
	CHECK( developer.size() == 1 && developer[0] == "Duplicate Aliases for idl in list fixture.\n" );
	CHECK( Add( list, "IDLE3", "idle3.wav", nullptr ) );
	CHECK( list->count == 2 && developer.size() == 1 );
	FreeList( list );

	/* A prefix taken first drops the longer name at the sort. */
	Reset();
	list = CreateList( "fixture" );
	CHECK( Add( list, "walk1", "a.wav", nullptr ) );
	CHECK( Add( list, "walk", "b.wav", nullptr ) );
	CHECK( Add( list, "run", "c.wav", nullptr ) );
	CHECK( list->count == 3 );
	Sort( list );
	CHECK( list->count == 2 );
	CHECK( !std::strcmp( list->sorted[0]->name, "run" ) && !std::strcmp( list->sorted[1]->name, "walk" ) );
	CHECK( developer.size() == 1 && developer[0] == "Duplicate Aliases for walk in list fixture.\n" );

	FreeList( list );

	/* The same name in another case gets past the check before a sort, and
	 * the sort keeps the newest: silently when the real names agree. */
	Reset();
	list = CreateList( "fixture" );
	CHECK( Add( list, "walk", "b.wav", nullptr ) );
	CHECK( Add( list, "WALK", "b.wav", nullptr ) );
	CHECK( list->count == 2 );
	Sort( list );
	CHECK( list->count == 1 && developer.empty() );
	CHECK( Add( list, "run", "r.wav", nullptr ) );
	CHECK( Add( list, "Walk", "c.wav", nullptr ) );
	CHECK( list->count == 3 );
	Sort( list );
	CHECK( list->count == 2 && Is( list->sorted[0]->name, "run" ) );
	CHECK( Is( list->sorted[1]->realName, "c.wav" ) );
	CHECK( developer.size() == 1 && developer[0] == "Duplicate Aliases for walk in list fixture.\n" );
	FreeList( list );

	/* Bytes compare unsigned. */
	Reset();
	list = CreateList( "fixture" );
	CHECK( Add( list, "\xe9t\xe9", "c.wav", nullptr ) );
	CHECK( Add( list, "z", "b.wav", nullptr ) );
	CHECK( Add( list, "a", "a.wav", nullptr ) );
	Sort( list );
	CHECK( list->count == 3 );
	CHECK( Is( list->sorted[0]->name, "a" ) && Is( list->sorted[1]->name, "z" ) );
	CHECK( Is( list->sorted[2]->name, "\xe9t\xe9" ) );
	FreeList( list );
}

/* 0x004236c0 over the run 0x00423770 finds. */
static void FindRandomPicks()
{
	Reset();
	SetRandomSource( Draw );
	List *list = CreateList( "fixture" );
	CHECK( Add( list, "pain1", "one", nullptr ) );
	CHECK( Add( list, "pain2", "two", "weight 3" ) );
	CHECK( Add( list, "pain_big", "big", nullptr ) );

	/* Total 4: the draw is fraction x 4 against the running weights 1, 4. */
	Script( { 8191, 8192, 32767, 0x18000 } );
	CHECK( Is( FindRandom( list, "PAIN" ), "one" ) );    /* 0.99988 < 1 */
	CHECK( Is( FindRandom( list, "pain" ), "two" ) );    /* 1.0 */
	CHECK( Is( FindRandom( list, "pain" ), "two" ) );    /* 3.99988 */
	CHECK( Is( FindRandom( list, "pain" ), "one" ) );    /* masked to 15 bits: 0 */
	/* "pain_big" answers only for its own name or a longer one. */
	Script( { 0 } );
	CHECK( Is( FindRandom( list, "pain_b" ), "big" ) );
	/* A draw is made even when nothing matches. */
	Script( { 0 } );
	CHECK( !FindRandom( list, "nothing" ) && drawn == 1 );
	/* A name of 32 characters or more matches nothing. */
	Script( { 0 } );
	CHECK( !FindRandom( list, "pain1pain1pain1pain1pain1pain1pain1" ) );
	FreeList( list );

	/* A refused line inside the run adds its weight to the walk though not to
	 * the total: "pain_x" sorts between "pain1" and "paina". */
	list = CreateList( "fixture" );
	CHECK( Add( list, "pain1", "one", nullptr ) );
	CHECK( Add( list, "pain_x", "refused", "weight 100" ) );
	CHECK( Add( list, "paina", "a", "weight 2" ) );
	Script( { 0, 16384, 32767 } );
	CHECK( Is( FindRandom( list, "pain" ), "one" ) );
	CHECK( Is( FindRandom( list, "pain" ), "refused" ) );
	CHECK( Is( FindRandom( list, "pain" ), "refused" ) );
	FreeList( list );

	/* At most 64 lines answer, counted down from the one the binary search
	 * lands on and then up. */
	list = CreateList( "fixture" );
	for ( int i = 0; i < 70; ++i ) {
		char name[8], real[8];
		std::snprintf( name, sizeof( name ), "n%02d", i );
		std::snprintf( real, sizeof( real ), "r%02d", i );
		CHECK( Add( list, name, real, nullptr ) );
	}
	Script( { 0, 32767 } );
	CHECK( Is( FindRandom( list, "n" ), "r00" ) );
	CHECK( Is( FindRandom( list, "n" ), "r63" ) );
	FreeList( list );
	SetRandomSource( nullptr );
}

/* 0x004232f0 in order, with the resets of 0x00423000. */
static void DialogInOrder()
{
	Reset();
	SetClock( Clock );
	List *list = CreateList( "fixture" );
	CHECK( Add( list, "d1", "l1", nullptr ) );
	CHECK( Add( list, "d2", "l2", nullptr ) );
	CHECK( Add( list, "d3", "l3", nullptr ) );

	now = 1000;
	CHECK( Is( FindDialog( list, "d", 0, -1 ), "l1" ) );
	Record *d1 = Named( list, "d1" );
	CHECK( d1->played == 1 && d1->lastPlayed == 1000 && d1->timesPlayed == 1 );
	CHECK( Is( FindDialog( list, "d", 0, -1 ), "l2" ) );
	CHECK( Is( FindDialog( list, "d", 0, -1 ), "l3" ) );
	/* All played in this same millisecond: no timeout has passed yet. */
	CHECK( !FindDialog( list, "d", 0, -1 ) );
	now = 1001;
	CHECK( Is( FindDialog( list, "d", 0, -1 ), "l1" ) );
	CHECK( d1->timesPlayed == 2 && d1->lastPlayed == 1001 );
	FreeList( list );

	/* A timeout must be exceeded, in seconds of Sys_Milliseconds. */
	list = CreateList( "fixture" );
	CHECK( Add( list, "t1", "t", "timeout 2" ) );
	now = 1000;
	CHECK( Is( FindDialog( list, "t", 0, -1 ), "t" ) );
	now = 3000;
	CHECK( !FindDialog( list, "t", 0, -1 ) );
	now = 3001;
	CHECK( Is( FindDialog( list, "t", 0, -1 ), "t" ) );
	FreeList( list );

	/* maxuse stops the resets after that many plays; stop allows none. */
	list = CreateList( "fixture" );
	CHECK( Add( list, "m1", "m", "maxuse 2" ) );
	CHECK( Add( list, "s1", "s", "stop" ) );
	now = 0;
	CHECK( Is( FindDialog( list, "m", 0, -1 ), "m" ) );
	now = 1;
	CHECK( Is( FindDialog( list, "m", 0, -1 ), "m" ) );
	now = 100000;
	CHECK( !FindDialog( list, "m", 0, -1 ) );
	CHECK( Named( list, "m1" )->timesPlayed == 2 );
	CHECK( Is( FindDialog( list, "s", 0, -1 ), "s" ) );
	now = 200000;
	CHECK( !FindDialog( list, "s", 0, -1 ) );
	CHECK( !FindDialog( list, "none", 0, -1 ) );
	FreeList( list );
	SetClock( nullptr );
}

/* Per-actor history, and `global` lines shared by every actor. */
static void DialogPerActor()
{
	Reset();
	SetClock( Clock );
	List *list = CreateList( "fixture" );
	CHECK( Add( list, "v1", "l1", nullptr ) );
	CHECK( Add( list, "v2", "l2", nullptr ) );

	now = 50;
	CHECK( Is( FindDialog( list, "v", 0, 5 ), "l1" ) );
	Record *v1 = Named( list, "v1" ), *v2 = Named( list, "v2" );
	/* Counting gave both lines a history for actor 5. */
	CHECK( ActorOf( v1, 5 ) && ActorOf( v2, 5 ) );
	CHECK( ActorOf( v1, 5 )->played == 1 && ActorOf( v1, 5 )->timesPlayed == 1 );
	CHECK( ActorOf( v1, 5 )->lastPlayed == 50 && !ActorOf( v2, 5 )->played );
	CHECK( !v1->played && !v1->timesPlayed );
	/* Actor 7 has its own. */
	CHECK( Is( FindDialog( list, "v", 0, 7 ), "l1" ) );
	CHECK( Is( FindDialog( list, "v", 0, 5 ), "l2" ) );
	/* Actor -1 is the line's own history. */
	CHECK( Is( FindDialog( list, "v", 0, -1 ), "l1" ) );
	CHECK( v1->played == 1 );
	/* Newest actor first in a line's chain. */
	CHECK( v1->actors->actor == 7 && v1->actors->next->actor == 5 );
	FreeList( list );

	list = CreateList( "fixture" );
	CHECK( Add( list, "g1", "g", "global" ) );
	CHECK( Is( FindDialog( list, "g", 0, 5 ), "g" ) );
	CHECK( !Named( list, "g1" )->actors && Named( list, "g1" )->played );
	CHECK( !FindDialog( list, "g", 0, 7 ) );
	FreeList( list );
	SetClock( nullptr );
}

/* The random pick: (int)(fraction x available + 0.5). */
static void DialogAtRandom()
{
	Reset();
	SetClock( Clock );
	SetRandomSource( Draw );
	List *list = CreateList( "fixture" );
	CHECK( Add( list, "r1", "l1", nullptr ) );
	CHECK( Add( list, "r2", "l2", nullptr ) );
	CHECK( Add( list, "r3", "l3", nullptr ) );

	/* Three available: 27307 x 3 / 32768 + 0.5 is 3.00003, past the last
	 * line -- no line, nothing marked. 27306 gives 2.99994, the third. */
	Script( { 27307, 27306 } );
	CHECK( !FindDialog( list, "r", 1, -1 ) );
	CHECK( !Named( list, "r1" )->played && !Named( list, "r2" )->played && !Named( list, "r3" )->played );
	CHECK( Is( FindDialog( list, "r", 1, -1 ), "l3" ) );
	/* Two left, r1 and r2: 5461 gives 0.83, the first. Then one: 16384 gives
	 * 1.0, past it. */
	Script( { 5461 } );
	CHECK( Is( FindDialog( list, "r", 1, -1 ), "l1" ) );
	Script( { 16384 } );
	CHECK( !FindDialog( list, "r", 1, -1 ) );
	Script( { 0 } );
	CHECK( Is( FindDialog( list, "r", 1, -1 ), "l2" ) );
	/* No draw is made when nothing matches. */
	Script( {} );
	CHECK( !FindDialog( list, "x", 1, -1 ) );
	FreeList( list );
	SetRandomSource( nullptr );
	SetClock( nullptr );
}

/* 0x00423200 and 0x00423270: a history restored from a save. */
static void Restore()
{
	Reset();
	List *list = CreateList( "fixture" );
	CHECK( Add( list, "u1", "a", nullptr ) );
	CHECK( Add( list, "u2", "b", nullptr ) );
	CHECK( Add( list, "ux", "c", nullptr ) );

	UpdateDialog( list, "U1", 4, 257, 1234 );
	Record *u1 = Named( list, "u1" );
	CHECK( u1->timesPlayed == 4 && u1->played == 1 && u1->lastPlayed == 1234 );
	/* Only a name selecting exactly one line. */
	UpdateDialog( list, "u", 9, 1, 9 );
	CHECK( u1->timesPlayed == 4 && !Named( list, "u2" )->timesPlayed );

	UpdateActorDialog( list, "u2", 9, 3, 1, 500 );
	Actor *a = ActorOf( Named( list, "u2" ), 9 );
	CHECK( a && a->timesPlayed == 3 && a->played == 1 && a->lastPlayed == 500 );
	UpdateActorDialog( list, "u2", 9, 5, 0, 600 );
	CHECK( a->timesPlayed == 5 && !a->played && a->lastPlayed == 600 && !a->next );
	/* Retail writes through a null history here. */
	UpdateActorDialog( list, "u2", -1, 1, 1, 1 );
	CHECK( Named( list, "u2" )->actors == a && !a->next );
	FreeList( list );
}

/* 0x00423990 and the global list. */
static void DumpAndClear()
{
	Reset();
	const char *name = "models/characters/fixture/a_really_long_name.tik";
	List *list = CreateList( name );
	CHECK( !std::memcmp( list->name, name, 32 ) );
	CHECK( Add( list, "b", "sound/b.wav", "weight 0.5" ) );
	CHECK( Add( list, "a", "sound/a.wav", nullptr ) );
	Dump( list );
	CHECK( developer.size() == 5 );
	CHECK( developer[0] == "Alias List: " + std::string( name, 32 ) + "\n" );
	CHECK( developer[1] == "Alias Name / Weight / Real Name\n" );
	CHECK( developer[2] == std::string( 31, ' ' ) + "a 1.00 sound/a.wav\n" );
	CHECK( developer[3] == std::string( 31, ' ' ) + "b 0.50 sound/b.wav\n" );
	CHECK( developer[4] == "2 total aliases\n" );
	CHECK( printed.empty() );

	UpdateActorDialog( list, "a", 3, 1, 1, 1 );
	ClearList( list );
	CHECK( !list->count && !list->dirty && !list->head && !list->sorted );
	CHECK( Add( list, "c", "sound/c.wav", nullptr ) );
	CHECK( list->count == 1 );
	FreeList( list );

	List *global = Global();
	CHECK( global && global == Global() && Is( global->name, "Global" ) );
	CHECK( Add( global, "menu_click", "sound/menu/click.wav", nullptr ) );
	Script( { 0 } );
	SetRandomSource( Draw );
	CHECK( Is( FindRandom( global, "menu_click" ), "sound/menu/click.wav" ) );
	SetRandomSource( nullptr );
	ClearList( global );
	CHECK( Global() == global && !global->count );
}

int main()
{
	Layout();
	AddAndParameters();
	Duplicates();
	FindRandomPicks();
	DialogInOrder();
	DialogPerActor();
	DialogAtRandom();
	Restore();
	DumpAndClear();
	std::cout << "alice alias tests passed\n";
	return 0;
}
