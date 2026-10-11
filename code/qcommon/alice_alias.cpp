/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */

/* American McGee's Alice's alias lists: the per-model lists the game module
 * and the client game fill from a model's init commands, and the engine's
 * own list named `Global`. See alice_tiki.h for the record layout.
 *
 * Everything here follows retail alice.exe, 0x00422850 to 0x00423b12; each
 * function names the routine it follows. Retail is followed exactly --
 * including the rules that look accidental, because they decide which line a
 * model says -- except where it would read or write out of bounds, where this
 * stops safely instead; each such place says so.
 *
 * Arithmetic follows the x87 code under the run-time's default 53-bit
 * precision, which alice.exe leaves alone (its only _controlfp call,
 * 0x0046ca68, sets the exception and infinity masks): a list's total weight
 * is summed in single precision, the running sum a pick is compared against
 * in double. */

#include "alice_tiki.h"
#include "qcommon.h"

#include <cstddef>
#include <cstdlib>
#include <cstring>

namespace fntech3 {
namespace alice {
namespace alias {

static_assert( offsetof( Actor, timesPlayed ) == 0x04, "Actor.timesPlayed" );
static_assert( offsetof( Actor, played ) == 0x08, "Actor.played" );
static_assert( offsetof( Actor, lastPlayed ) == 0x0c, "Actor.lastPlayed" );
static_assert( offsetof( Record, realName ) == 0x20, "Record.realName" );
static_assert( offsetof( Record, weight ) == 0xa0, "Record.weight" );
static_assert( offsetof( Record, global ) == 0xa4, "Record.global" );
static_assert( offsetof( Record, stop ) == 0xa5, "Record.stop" );
static_assert( offsetof( Record, timeout ) == 0xa8, "Record.timeout" );
static_assert( offsetof( Record, maxUse ) == 0xac, "Record.maxUse" );
static_assert( offsetof( Record, timesPlayed ) == 0xb0, "Record.timesPlayed" );
static_assert( offsetof( Record, played ) == 0xb4, "Record.played" );
static_assert( offsetof( Record, lastPlayed ) == 0xb8, "Record.lastPlayed" );
static_assert( offsetof( List, dirty ) == 0x20, "List.dirty" );
static_assert( offsetof( List, count ) == 0x24, "List.count" );
/* The pointers sit where the game module reads them only on the 32-bit build,
 * the only one that loads it. */
static_assert( sizeof( void * ) != 4 || ( sizeof( Actor ) == 0x14
	&& offsetof( Actor, next ) == 0x10 ), "Actor layout" );
static_assert( sizeof( void * ) != 4 || ( sizeof( Record ) == 0xc4
	&& offsetof( Record, actors ) == 0xbc && offsetof( Record, next ) == 0xc0 ), "Record layout" );
static_assert( sizeof( void * ) != 4 || ( sizeof( List ) == 0x30
	&& offsetof( List, sorted ) == 0x28 && offsetof( List, head ) == 0x2c ), "List layout" );

namespace {

/* At most this many lines answer one name (0x00532928, 64 entries). */
constexpr int MaxMatches = 64;

List *globalList;

int DefaultRandom( void ) {
	return std::rand();
}

int DefaultClock( void ) {
	return Sys_Milliseconds();
}

int ( *randomSource )( void ) = DefaultRandom;
int ( *clockSource )( void ) = DefaultClock;

/* 0x004236a0: rand() & 0x7fff scaled by 1/32768, so [0, 32767/32768]. */
double RandomFraction( void ) {
	return static_cast<double>( randomSource() & 0x7fff ) * ( 1.0 / 32768.0 );
}

/* ---- the run-time helpers retail calls ---------------------------------- */

/* tolower, 0x004e7346, in the C locale. */
int Lower( int c ) {
	return ( c >= 'A' && c <= 'Z' ) ? c + ( 'a' - 'A' ) : c;
}

/* strncmp, 0x004e7480: bytes compared unsigned. */
int CompareN( const char *a, const char *b, std::size_t n ) {
	for ( ; n; --n, ++a, ++b ) {
		const unsigned char ca = static_cast<unsigned char>( *a );
		const unsigned char cb = static_cast<unsigned char>( *b );
		if ( ca != cb ) {
			return ca < cb ? -1 : 1;
		}
		if ( !ca ) {
			break;
		}
	}
	return 0;
}

/* _stricmp, 0x004e8760, in the C locale. */
bool SameWord( const char *a, const char *b ) {
	for ( ;; ++a, ++b ) {
		const int ca = Lower( static_cast<unsigned char>( *a ) );
		const int cb = Lower( static_cast<unsigned char>( *b ) );
		if ( ca != cb ) {
			return false;
		}
		if ( !ca ) {
			return true;
		}
	}
}

/* strncpy, 0x004e7240: pads with zeros and leaves `dst` unterminated when
 * `src` fills it. */
void CopyN( char *dst, const char *src, std::size_t n ) {
	std::size_t i = 0;
	for ( ; i < n && src[i]; ++i ) {
		dst[i] = src[i];
	}
	for ( ; i < n; ++i ) {
		dst[i] = '\0';
	}
}

bool IsSpace( char c ) {
	return c == ' ' || ( c >= '\t' && c <= '\r' );
}

bool IsDigit( char c ) {
	return c >= '0' && c <= '9';
}

enum { TokenBytes = 1024 };

/* atof, 0x004e6f0e. The 2000 run-time reads a sign, digits, a fraction and
 * an exponent and nothing else, so `inf`, `nan` and hexadecimal read as 0;
 * the conversion itself is left to strtod. */
double ReadFloat( const char *s ) {
	char text[TokenBytes + 1];
	std::size_t n = 0;
	bool digits = false;
	while ( IsSpace( *s ) ) {
		++s;
	}
	if ( *s == '+' || *s == '-' ) {
		text[n++] = *s++;
	}
	for ( ; IsDigit( *s ) && n < TokenBytes; digits = true ) {
		text[n++] = *s++;
	}
	if ( *s == '.' && n < TokenBytes ) {
		text[n++] = *s++;
		for ( ; IsDigit( *s ) && n < TokenBytes; digits = true ) {
			text[n++] = *s++;
		}
	}
	if ( !digits ) {
		return 0.0;
	}
	if ( ( *s == 'e' || *s == 'E' ) && n + 2 < TokenBytes ) {
		const char *e = s + 1;
		const bool sign = *e == '+' || *e == '-';
		if ( IsDigit( e[sign ? 1 : 0] ) ) {
			text[n++] = 'e';
			if ( sign ) {
				text[n++] = *e++;
			}
			while ( IsDigit( *e ) && n < TokenBytes ) {
				text[n++] = *e++;
			}
		}
	}
	text[n] = '\0';
	return std::strtod( text, nullptr );
}

/* atoi, 0x004e6ff0: whitespace, a sign, digits; overflow wraps. */
int ReadInt( const char *s ) {
	while ( IsSpace( *s ) ) {
		++s;
	}
	const char sign = *s;
	if ( sign == '+' || sign == '-' ) {
		++s;
	}
	unsigned int value = 0;
	while ( IsDigit( *s ) ) {
		value = value * 10u + static_cast<unsigned int>( *s++ - '0' );
	}
	return static_cast<int>( sign == '-' ? 0u - value : value );
}

/* 0x0044b640, the engine's tokenizer, as the parameter reader calls it:
 * crossing line breaks. It is not Quake III's COM_ParseExt. Whitespace is a
 * byte from 0 to 32 read as a signed char, so bytes from 0x80 belong to
 * tokens; `//` runs to the line break; a block comment ends at the first `/`
 * whose previous byte is `*` -- so `/` `*` `/` is a whole comment -- and the
 * rest of that line is skipped too; a quoted string turns `\"` into `"` and
 * `\n` into a line break and keeps 1,024 bytes; a word of 1,024 bytes or more
 * comes back empty. An empty token is the end. Retail steps past the
 * terminator of a quoted string that runs off the end of the text and reads
 * on; this stops there. */
const char *NextToken( const char **data, char ( &token )[TokenBytes + 1] ) {
	int length = 0;
	token[0] = '\0';
	const char *p = *data;
	if ( !p ) {
		return token;
	}
	int c;
	for ( ;; ) {
		c = static_cast<signed char>( *p );
		if ( c >= 0 && c <= ' ' ) {
			if ( !c ) {
				*data = nullptr;
				return token;
			}
			++p;
			continue;
		}
		if ( c == '/' && p[1] == '/' ) {
			while ( *p && *p != '\n' ) {
				++p;
			}
			continue;
		}
		if ( c == '/' && p[1] == '*' ) {
			++p;
			while ( *p && !( p[-1] == '*' && p[0] == '/' ) ) {
				++p;
			}
			while ( *p && *p != '\n' ) {
				++p;
			}
			continue;
		}
		break;
	}
	if ( c == '"' ) {
		++p;
		for ( ;; ) {
			c = static_cast<signed char>( *p );
			if ( !c ) {
				token[length] = '\0';
				*data = p;
				return token;
			}
			++p;
			if ( c == '\\' && *p == '"' ) {
				if ( length < TokenBytes ) {
					token[length++] = '"';
				}
				++p;
				continue;
			}
			if ( c == '"' ) {
				token[length] = '\0';
				*data = p;
				return token;
			}
			if ( length < TokenBytes ) {
				if ( c == '\\' && *p == 'n' ) {
					token[length] = '\n';
					++p;
				} else {
					token[length] = static_cast<char>( c );
				}
				++length;
			}
		}
	}
	do {
		if ( length < TokenBytes ) {
			token[length++] = static_cast<char>( c );
		}
		++p;
		c = static_cast<signed char>( *p );
	} while ( c > ' ' || c < 0 );
	if ( length == TokenBytes ) {
		length = 0;
	}
	token[length] = '\0';
	*data = p;
	return token;
}

/* ---- records ------------------------------------------------------------ */

void *Allocate( std::size_t size, const char *what ) {
	void *memory = std::calloc( 1, size ? size : 1 );
	if ( !memory ) {
		Com_Error( ERR_FATAL, "%s", what );
	}
	return memory;
}

void FreeActors( Record *record ) {
	while ( Actor *actor = record->actors ) {
		record->actors = actor->next;
		std::free( actor );
	}
}

/* 0x00422a60: the parameter string, token by token. `global`, `stop`,
 * `timeout F`, `maxuse N` and `weight F` are all there are; a value must be
 * above zero, and a missing value ends the string. */
void ReadParameters( Record *record, const char *parameters ) {
	if ( !parameters ) {
		return;
	}
	char token[TokenBytes + 1];
	const char *p = parameters;
	for ( ;; ) {
		if ( !*NextToken( &p, token ) ) {
			return;
		}
		if ( SameWord( token, "global" ) ) {
			record->global = 1;
		} else if ( SameWord( token, "stop" ) ) {
			record->stop = 1;
		} else if ( SameWord( token, "timeout" ) ) {
			if ( !*NextToken( &p, token ) ) {
				return;
			}
			/* Compared as read, stored in single precision. */
			const double value = ReadFloat( token );
			if ( value > 0.0 ) {
				record->timeout = static_cast<float>( value );
			} else {
				Com_Printf( "Timeout value out of range in %s alias\n", record->name );
			}
		} else if ( SameWord( token, "maxuse" ) ) {
			if ( !*NextToken( &p, token ) ) {
				return;
			}
			const int value = ReadInt( token );
			if ( value > 0 ) {
				record->maxUse = value;
			} else {
				Com_Printf( "Maximum use value out of range in %s alias\n", record->name );
			}
		} else if ( SameWord( token, "weight" ) ) {
			if ( !*NextToken( &p, token ) ) {
				return;
			}
			const double value = ReadFloat( token );
			if ( value > 0.0 ) {
				record->weight = static_cast<float>( value );
			} else {
				Com_Printf( "Weight value out of range in %s alias\n", record->name );
			}
		} else {
			Com_Printf( "Unknown parameter for %s alias\n", record->name );
		}
	}
}

/* Copy a name lowered into `key`, which holds 31 characters. Retail copies
 * into a 32-byte stack buffer without looking; a longer name cannot match a
 * stored one, which holds at most 31, so it is refused here instead. */
bool MakeKey( const char *name, char ( &key )[32], std::size_t &length ) {
	length = std::strlen( name );
	if ( length + 1 > sizeof( key ) ) {
		return false;
	}
	for ( std::size_t i = 0; i < length; ++i ) {
		key[i] = static_cast<char>( Lower( static_cast<signed char>( name[i] ) ) );
	}
	key[length] = '\0';
	return true;
}

/* 0x004235d0: a line the name prefixes, by binary search -- not necessarily
 * the name itself. */
Record *FindRecord( List *list, const char *name ) {
	if ( !list || !name ) {
		return nullptr;
	}
	if ( list->dirty ) {
		Sort( list );
	}
	char key[32];
	std::size_t length;
	if ( !MakeKey( name, key, length ) ) {
		return nullptr;
	}
	int lo = 0, hi = list->count - 1;
	while ( hi >= lo ) {
		const int mid = ( lo + hi ) >> 1;
		const int order = CompareN( key, list->sorted[mid]->name, length );
		if ( order < 0 ) {
			hi = mid - 1;
		} else if ( order > 0 ) {
			lo = mid + 1;
		} else {
			return list->sorted[mid];
		}
	}
	return nullptr;
}

/* 0x00423770: the run of sorted lines a name answers. Binary search finds one
 * line the name prefixes; from there the scan runs down, then up, over every
 * line the name prefixes, accepting those whose next character is not `_`
 * until 64 are accepted -- "idle" takes "idle2" but not "idle_talk". `first`
 * and `last` are the lowest and highest lines accepted, but start at the line
 * the search found even when it is not accepted itself, and the run between
 * them includes any line a later caller would have refused: every caller
 * walks the whole run. `total` is the accepted lines' weight, summed in
 * single precision downwards and then upwards. Both ends are -1 when nothing
 * is accepted. */
void FindRange( List *list, const char *name, int *first, int *last, float *total ) {
	*first = -1;
	*last = -1;
	if ( !list || !name ) {
		return;
	}
	if ( list->dirty ) {
		Sort( list );
	}
	char key[32];
	std::size_t length;
	if ( !MakeKey( name, key, length ) ) {
		return;
	}
	int lo = 0, hi = list->count - 1, mid = 0;
	if ( hi < 0 ) {
		return;
	}
	for ( ;; ) {
		mid = ( lo + hi ) >> 1;
		const int order = CompareN( key, list->sorted[mid]->name, length );
		if ( !order ) {
			break;
		}
		if ( order < 0 ) {
			hi = mid - 1;
		} else {
			lo = mid + 1;
		}
		if ( hi < lo ) {
			return;
		}
	}

	int accepted = 0;
	float sum = 0.0f;
	*first = mid;
	*last = mid;
	for ( int i = mid; i >= 0; --i ) {
		const Record *record = list->sorted[i];
		if ( CompareN( key, record->name, length ) ) {
			break;
		}
		if ( accepted < MaxMatches && record->name[length] != '_' ) {
			++accepted;
			sum = static_cast<float>( static_cast<double>( sum ) + record->weight );
			*first = i;
		}
	}
	for ( int i = mid + 1; i < list->count; ++i ) {
		const Record *record = list->sorted[i];
		if ( CompareN( key, record->name, length ) ) {
			break;
		}
		if ( accepted < MaxMatches && record->name[length] != '_' ) {
			++accepted;
			sum = static_cast<float>( static_cast<double>( sum ) + record->weight );
			*last = i;
		}
	}
	if ( accepted ) {
		*total = sum;
		return;
	}
	*first = -1;
	*last = -1;
}

/* 0x00423130: whether a line's history is its own rather than the actor's --
 * for the line's own lookups (actor -1) and for `global` lines. */
bool UseRecord( const Record *record, int actor ) {
	return actor == -1 || record->global;
}

/* 0x00422c40 */
Actor *FindActor( Record *record, int actor ) {
	if ( !record ) {
		return nullptr;
	}
	for ( Actor *node = record->actors; node; node = node->next ) {
		if ( node->actor == actor ) {
			return node;
		}
	}
	return nullptr;
}

/* 0x00422c70: a fresh history for the actor, first in the line's chain;
 * never for actor -1. */
Actor *AddActor( Record *record, int actor ) {
	if ( !record || actor == -1 ) {
		return nullptr;
	}
	if ( FindActor( record, actor ) ) {
		Com_Printf( "Actor %d already added to this %s alias\n", actor, record->name );
		return nullptr;
	}
	Actor *node = static_cast<Actor *>( Allocate( sizeof( Actor ), "Alias_AddActor : Out of memory" ) );
	node->actor = actor;
	node->next = record->actors;
	record->actors = node;
	return node;
}

/* 0x00423150: the lines of a run not yet played -- by the line itself or by
 * this actor. Counting gives every line of the run a history for the actor
 * if it had none. */
int CountAvailable( List *list, int first, int last, int actor ) {
	if ( first == -1 || last == -1 ) {
		return 0;
	}
	int count = 0;
	for ( int i = first; i <= last; ++i ) {
		Record *record = list->sorted[i];
		if ( UseRecord( record, actor ) ) {
			if ( !record->played ) {
				++count;
			}
			continue;
		}
		Actor *node = FindActor( record, actor );
		if ( !node ) {
			node = AddActor( record, actor );
		}
		if ( node && !node->played ) {
			++count;
		}
	}
	return count;
}

int ElapsedMs( int now, int then ) {
	return static_cast<int>( static_cast<unsigned int>( now ) - static_cast<unsigned int>( then ) );
}

/* Whether a played line may play again: never when `stop`; only once more
 * than `timeout` seconds have passed on Sys_Milliseconds -- with no timeout,
 * once a millisecond has; and not once it has played `maxuse` times. */
bool MayReset( const Record *record, int timesPlayed, int lastPlayed ) {
	if ( record->stop ) {
		return false;
	}
	const double elapsed = static_cast<double>( ElapsedMs( clockSource(), lastPlayed ) ) * 0.001;
	if ( !( static_cast<double>( record->timeout ) < elapsed ) ) {
		return false;
	}
	return record->maxUse == -1 || timesPlayed < record->maxUse;
}

/* 0x00423000: mark the lines of a run that may play again as unplayed. */
void ResetRun( List *list, int first, int last, int actor ) {
	if ( first == -1 || last == -1 ) {
		return;
	}
	for ( int i = first; i <= last; ++i ) {
		Record *record = list->sorted[i];
		if ( UseRecord( record, actor ) ) {
			if ( MayReset( record, record->timesPlayed, record->lastPlayed ) ) {
				record->played = 0;
			}
			continue;
		}
		Actor *node = FindActor( record, actor );
		if ( node && MayReset( record, node->timesPlayed, node->lastPlayed ) ) {
			node->played = 0;
		}
	}
}

/* 0x004232f0's bookkeeping for the line it answers. */
template<typename History>
void MarkPlayed( History *history ) {
	history->played = 1;
	history->lastPlayed = clockSource();
	history->timesPlayed++;
}

} // namespace

/* ======================================================================== */

List *CreateList( const char *name ) {
	List *list = static_cast<List *>( Allocate( sizeof( List ), "Alias_CreateList : Out of memory" ) );
	if ( name ) {
		CopyN( list->name, name, sizeof( list->name ) );
	}
	return list;
}

void ClearList( List *list ) {
	if ( !list ) {
		return;
	}
	for ( Record *record = list->head; record; record = record->next ) {
		FreeActors( record );
	}
	std::free( list->sorted );
	for ( Record *record = list->head; record; ) {
		Record *next = record->next;
		std::free( record );
		record = next;
	}
	list->count = 0;
	list->dirty = 0;
	list->sorted = nullptr;
	list->head = nullptr;
}

void FreeList( List *list ) {
	ClearList( list );
	std::free( list );
}

/* 0x004229b0: every line keeps its own history and loses its speakers'. */
void ClearActors( List *list ) {
	if ( !list ) {
		return;
	}
	for ( Record *record = list->head; record; record = record->next ) {
		FreeActors( record );
	}
}

void ListDeleter::operator()( List *list ) const {
	FreeList( list );
}

List *Global( void ) {
	if ( !globalList ) {
		globalList = CreateList( "Global" );
	}
	return globalList;
}

/* 0x00422cd0. Duplicates are refused two ways, depending on whether the list
 * has been sorted since its last addition:
 *
 *   - sorted: a line the new name prefixes, found by FindRecord, refuses it,
 *     reported under `developer` only when the real names differ;
 *   - not sorted: a line whose whole name begins the new name, compared case
 *     sensitively against the name as given, refuses it silently.
 *
 * Either way the answer is success. The sort drops more (see Sort). */
qboolean Add( List *list, const char *name, const char *realName, const char *parameters ) {
	if ( !list ) {
		return qfalse;
	}
	/* Retail dereferences both. */
	if ( !name || !realName ) {
		return qfalse;
	}
	if ( !list->dirty ) {
		const Record *found = FindRecord( list, name );
		if ( found ) {
			if ( std::strcmp( realName, found->realName ) ) {
				Com_DPrintf( "Duplicate Aliases for %s in list %.32s.\n", name, list->name );
			}
			return qtrue;
		}
	} else {
		for ( const Record *record = list->head; record; record = record->next ) {
			if ( !CompareN( record->name, name, std::strlen( record->name ) ) ) {
				return qtrue;
			}
		}
	}

	Record *record = static_cast<Record *>( Allocate( sizeof( Record ), "Alias_ListAdd : Out of memory" ) );
	record->weight = 1.0f;
	CopyN( record->name, name, sizeof( record->name ) - 1 );
	for ( char &c : record->name ) {
		c = static_cast<char>( Lower( static_cast<signed char>( c ) ) );
	}
	CopyN( record->realName, realName, sizeof( record->realName ) - 1 );
	record->maxUse = -1;
	ReadParameters( record, parameters );

	list->count++;
	list->dirty = 1;
	record->next = list->head;
	list->head = record;
	return qtrue;
}

/* 0x00422e30. A selection sort over the chain as it stands -- newest first,
 * then what the last sort left in descending order -- taking the least line
 * each pass, where a line is less than the current least when it compares
 * below it over its own length. A line whose name the previously kept line's
 * whole name begins -- the same name, or one the kept name prefixes -- is
 * dropped, reported under `developer` when the real names differ. So a
 * prefix and a longer name cannot usually both stay; but as the comparison
 * stops at the shorter name, a longer name taken first survives its prefix.
 * Kept lines are relinked at the head, leaving the chain descending. */
void Sort( List *list ) {
	if ( !list || !list->dirty ) {
		return;
	}
	Record *pending = list->head;
	list->head = nullptr;
	std::free( list->sorted );
	const int count = list->count;
	list->sorted = static_cast<Record **>( Allocate( sizeof( Record * ) * ( count > 0 ? count : 1 ),
		"Alias_SortList : Out of memory" ) );

	Record *previous = nullptr;
	int kept = 0;
	for ( int pass = 0; pass < count && pending; ++pass ) {
		Record *least = pending, *beforeLeast = nullptr;
		for ( Record *before = pending, *record = pending->next; record; before = record, record = record->next ) {
			if ( CompareN( record->name, least->name, std::strlen( record->name ) ) < 0 ) {
				beforeLeast = before;
				least = record;
			}
		}
		if ( least == pending ) {
			pending = least->next;
		} else {
			beforeLeast->next = least->next;
		}

		if ( previous && !CompareN( previous->name, least->name, std::strlen( previous->name ) ) ) {
			if ( std::strcmp( previous->realName, least->realName ) ) {
				Com_DPrintf( "Duplicate Aliases for %s in list %.32s.\n", previous->name, list->name );
			}
			/* Retail frees the line but not its actors. */
			FreeActors( least );
			std::free( least );
			continue;
		}
		list->sorted[kept++] = least;
		previous = least;
		least->next = list->head;
		list->head = least;
	}
	list->dirty = 0;
	list->count = kept;
}

/* 0x004236c0: a weighted pick over the run. The draw is rand() scaled by the
 * accepted lines' total weight and rounded to single precision; the lines of
 * the run are then walked in order, and the first whose running weight
 * exceeds the draw answers. A refused line inside the run still adds its
 * weight to the walk though not to the total, and rounding can leave the
 * draw unreached: both answer as retail does. The draw is made even when
 * nothing matches. */
const char *FindRandom( List *list, const char *name ) {
	if ( !list ) {
		return nullptr;
	}
	int first, last;
	float total = 0.0f;
	FindRange( list, name, &first, &last, &total );
	const int n = ( first != -1 && last != -1 ) ? last - first + 1 : 0;
	const float draw = static_cast<float>( RandomFraction() * static_cast<double>( total ) );
	double running = 0.0;
	for ( int i = 0; i < n; ++i ) {
		const Record *record = list->sorted[first + i];
		running += static_cast<double>( record->weight );
		if ( static_cast<double>( draw ) < running ) {
			return record->realName;
		}
	}
	return nullptr;
}

/* 0x004232f0. The run's unplayed lines are counted for the speaker; when none
 * is left, those that may play again (see MayReset) are reset and counted
 * again, and with none still, there is no line. In order, the first unplayed
 * line answers; at random, the pick is (int)(fraction x available + 0.5),
 * so the first line is half as likely as the others and one pick in about
 * 2 x available is past the last line: there is no line then either. The
 * line answering is marked played, stamped and counted. */
const char *FindDialog( List *list, const char *name, int random, int actor ) {
	if ( !list ) {
		return nullptr;
	}
	int first, last;
	float total;
	FindRange( list, name, &first, &last, &total );
	if ( first == -1 || last == -1 ) {
		return nullptr;
	}
	int available = CountAvailable( list, first, last, actor );
	if ( !available ) {
		ResetRun( list, first, last, actor );
		available = CountAvailable( list, first, last, actor );
		if ( !available ) {
			return nullptr;
		}
	}
	const int pick = random ? static_cast<int>( RandomFraction() * available + 0.5 ) : 0;
	int seen = 0;
	for ( int i = first; i <= last; ++i ) {
		Record *record = list->sorted[i];
		if ( UseRecord( record, actor ) ) {
			if ( record->played ) {
				continue;
			}
			if ( !random || pick == seen ) {
				MarkPlayed( record );
				return record->realName;
			}
			++seen;
			continue;
		}
		Actor *node = FindActor( record, actor );
		if ( !node || node->played ) {
			continue;
		}
		if ( !random || pick == seen ) {
			MarkPlayed( node );
			return record->realName;
		}
		++seen;
	}
	return nullptr;
}

/* 0x00423200 */
void UpdateDialog( List *list, const char *name, int timesPlayed, int played, int lastPlayed ) {
	int first, last;
	float total;
	FindRange( list, name, &first, &last, &total );
	if ( first == -1 || first != last ) {
		return;
	}
	Record *record = list->sorted[first];
	record->played = static_cast<unsigned char>( played );
	record->lastPlayed = lastPlayed;
	record->timesPlayed = timesPlayed;
}

/* 0x00423270. Retail writes through a null history for actor -1; that is
 * refused here. */
void UpdateActorDialog( List *list, const char *name, int actor, int timesPlayed, int played,
	int lastPlayed ) {
	int first, last;
	float total;
	FindRange( list, name, &first, &last, &total );
	if ( first == -1 || first != last ) {
		return;
	}
	Record *record = list->sorted[first];
	Actor *node = FindActor( record, actor );
	if ( !node ) {
		node = AddActor( record, actor );
	}
	if ( !node ) {
		return;
	}
	node->played = static_cast<unsigned char>( played );
	node->actor = actor;
	node->lastPlayed = lastPlayed;
	node->timesPlayed = timesPlayed;
}

/* 0x00423990: under `developer` only, as all of retail's alias output but its
 * parameter warnings. The list's name prints at most its 32 bytes. */
void Dump( List *list ) {
	if ( !list ) {
		return;
	}
	if ( list->dirty ) {
		Sort( list );
	}
	Com_DPrintf( "Alias List: %.32s\n", list->name );
	Com_DPrintf( "Alias Name / Weight / Real Name\n" );
	for ( int i = 0; i < list->count; ++i ) {
		const Record *record = list->sorted[i];
		Com_DPrintf( "%32s %3.2f %s\n", record->name, static_cast<double>( record->weight ), record->realName );
	}
	Com_DPrintf( "%d total aliases\n", list->count );
}

void SetRandomSource( int ( *source )( void ) ) {
	randomSource = source ? source : DefaultRandom;
}

void SetClock( int ( *clock )( void ) ) {
	clockSource = clock ? clock : DefaultClock;
}

} // namespace alias
} // namespace alice
} // namespace fntech3
