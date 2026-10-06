/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_SERVER_JK2_GAME_HPP
#define FNTECH3_SERVER_JK2_GAME_HPP

/* The engine side of Jedi Outcast multiplayer's game boundary.
 *
 * Raven shipped this one as Quake III bytecode -- vm/jk2mpgame.qvm inside
 * base/assets0.pk3 -- so the interpreter that runs it already exists here. The
 * numbering it calls through does not, and it is neither Quake III's nor Jedi
 * Academy's.
 *
 * Retail jk2mp.exe's own dispatcher settles the shape of it. The switch bounds
 * its argument at 599 and indexes a byte table, so the imports run 0 to 599
 * with 221 of those numbers handled and the rest falling to
 * "Bad game system trap". The handled ones sit in runs that begin at round
 * numbers, the way Raven's other titles do:
 *
 *     0-51, 100-111, 200-211, 300-318, 400-425, 500-599
 *
 * Only the second run is served here. Its twelve are Quake III's own memory
 * and arithmetic calls in Quake III's own order -- 100 takes three arguments
 * and passes them straight to a memory fill, and 103 is a single `fsin` -- so
 * the shared dispatcher already answers them correctly. Jedi Academy kept the
 * same twelve at the same numbers; Jedi Outcast stops two short of it, with no
 * handler for the two debug prints that would follow.
 *
 * Everything else is refused by number. Running an unnamed number through
 * Quake III's table on the assumption that the two agree is exactly the guess
 * that would look like compatibility and behave like corruption.
 *
 * See docs/fntech3/2026-09-22-jk2-contracts.md. */

#include "../qcommon/game_profile.h"
#include "server_raven_assets.hpp"
#include "../compat/jk2_state_adapter.hpp"

/* Read from retail jk2mp.exe: the bound its dispatcher compares against. */
inline constexpr int FNTECH3_JK2_GAME_IMPORT_HIGHEST = 599;
/* How many of those numbers it has a handler for. */
inline constexpr int FNTECH3_JK2_GAME_IMPORT_HANDLED = 221;
/* Quake III's memory and arithmetic block, which this title keeps verbatim. */
inline constexpr int FNTECH3_JK2_SHARED_FIRST = 100;
inline constexpr int FNTECH3_JK2_SHARED_LAST = 111;
/* The Ghoul2 model initializer, read from retail jk2mp.exe's own dispatcher.
 * Jedi Academy puts the same call at 590. */
inline constexpr int FNTECH3_JK2_G2_INIT_MODEL = 588;
/* And the two bolt calls beside it, which Academy puts at 593 and 594. */
inline constexpr int FNTECH3_JK2_G2_HAVE_MODELS = 584;
inline constexpr int FNTECH3_JK2_G2_ADD_BOLT = 589;
inline constexpr int FNTECH3_JK2_G2_SET_BOLT_INFO = 590;

static bool SV_JK2GameCall( const intptr_t *args, intptr_t &operation, intptr_t &result )
{
	(void)operation;
	result = 0;
	/* This title's own model store, which keeps the earlier behaviour
	 * (server_raven_assets.hpp). */
	fntech3::ghoul2::Store &ravenGhoul2 = ravenGhoul2Outcast;

	const int trap = static_cast<int>( args[0] );

	/* The arithmetic run, which the shared dispatcher already speaks. */
	if ( trap >= FNTECH3_JK2_SHARED_FIRST && trap <= FNTECH3_JK2_SHARED_LAST ) return false;

	/* The part of the first run that carries nothing of this title's own.
	 *
	 * Every number here is Quake III's, shown to be so by what retail
	 * jk2mp.exe's own handler does with it, and every one of them passes only
	 * strings, numbers, vectors or the engine's own buffers. None passes an
	 * entity, a player state, a trace or an input frame, because those are
	 * Raven's records and not one of them is frozen yet. A call that carries
	 * one is refused below rather than handed to a dispatcher that would read
	 * it at Quake III's offsets, which would look like compatibility and
	 * behave like corruption.
	 *
	 * The cvar pair is here on evidence rather than resemblance: retail
	 * jk2mp.exe reads the handle at offset 0 and the modification count at 4,
	 * and both its register and its update copy 256 bytes of value, so its
	 * cvar record is Quake III's 272-byte one exactly. */
	switch ( trap ) {
	case 0:   /* print */
	case 1:   /* error */
	case 2:   /* milliseconds */
	case 3:   /* cvar register */
	case 4:   /* cvar update */
	case 5:   /* cvar set */
	case 6:   /* cvar integer value */
	case 7:   /* cvar string buffer */
	case 8:   /* argument count */
	case 9:   /* argument */
	case 10:  /* open file */
	case 11:  /* read */
	case 12:  /* write */
	case 13:  /* close file */
	case 14:  /* console command */
	case 16:  /* drop client */
	case 17:  /* server command */
	case 18:  /* set configstring */
	case 19:  /* get configstring */
	case 20:  /* get userinfo */
	case 21:  /* set userinfo */
	case 22:  /* get serverinfo */
	case 29:  /* areas connected */
	case 37:  /* entity token */
	case 38:  /* file list */
	case 41:  /* real time */
	case 42:  /* snap vector */
	case 45:  /* seek */
		return false;
	default:
		break;
	}

	/* Two more of the bot-library run, named from their own handlers. 210
	 * takes a client, a converted buffer and a length; 211 computes
	 * `&clients[n]` by the client stride and hands it a converted pointer,
	 * which is this engine's own client think. 209, the snapshot entity, is
	 * left refused: there are no snapshots to read one out of. */
	if ( trap == 210 ) {
		VM_CHECKBOUNDS( gvm, args[2], args[3] );
		result = SV_BotGetConsoleMessage( static_cast<int>( args[1] ),
			static_cast<char *>( VM_ArgPtr( args[2] ) ), static_cast<int>( args[3] ) );
		return true;
	}
	if ( trap == 211 ) {
		const int clientNum = static_cast<int>( args[1] );
		if ( !SV_IsClientIndex( clientNum ) ) return true;
		VM_CHECKBOUNDS( gvm, args[2], sizeof( fntech3_jk2_usercmd_t ) );
		if ( !args[2] ) Com_Error( ERR_DROP, "Jedi Outcast bot command is null" );
		fntech3_jk2_usercmd_t external{};
		std::memcpy( &external, VM_ArgPtr( args[2] ), sizeof( external ) );
		usercmd_t command{};
		if ( !fntech3::jk2::state::ToInternal( external, command ) ) {
			Com_Error( ERR_DROP, "Jedi Outcast bot command does not convert" );
		}
		SV_ClientThink( &SV_ClientForIndex( clientNum ), &command );
		return true;
	}

	/* --- how a bot presses its keys --------------------------------------
	 *
	 * Sixteen of these, at the same numbers Jedi Academy uses, and the reason
	 * is read rather than assumed. Every handler reaches the bot-library
	 * export table at 0xb0d230 -- the one the setup call at 200 reaches -- and
	 * the slots they reach are consecutive in the order that library declares
	 * its elementary actions in: 0xac, 0xb0, 0xb4, 0xb8 and 0xbc are move,
	 * view, end-of-frame, get-input and reset-input, and the handlers for 421,
	 * 422, 424 and 425 use exactly those four of them, in that order, with
	 * exactly the arguments each takes.
	 *
	 * As in Academy, the two Raven inserted into the middle of Quake III's
	 * block have no counterpart in the library this engine carries. They are
	 * answered without doing anything, and that is a stated gap: a bot here
	 * moves, turns, jumps, crouches, fires and changes weapon, and does not
	 * alt-fire or use a force power. */
	switch ( trap ) {
	case 400:
		botlib_export->ea.EA_Say( static_cast<int>( args[1] ),
			static_cast<const char *>( VM_ArgPtr( args[2] ) ) );
		return true;
	case 401:
		botlib_export->ea.EA_SayTeam( static_cast<int>( args[1] ),
			static_cast<const char *>( VM_ArgPtr( args[2] ) ) );
		return true;
	case 406: botlib_export->ea.EA_Attack( static_cast<int>( args[1] ) ); return true;
	case 407:
	case 408:
		/* Raven's own two. Nothing is pressed, and nothing pretends to be. */
		return true;
	case 409: botlib_export->ea.EA_Use( static_cast<int>( args[1] ) ); return true;
	case 411: botlib_export->ea.EA_Crouch( static_cast<int>( args[1] ) ); return true;
	case 412: botlib_export->ea.EA_MoveUp( static_cast<int>( args[1] ) ); return true;
	case 413: botlib_export->ea.EA_MoveDown( static_cast<int>( args[1] ) ); return true;
	case 414: botlib_export->ea.EA_MoveForward( static_cast<int>( args[1] ) ); return true;
	case 415: botlib_export->ea.EA_MoveBack( static_cast<int>( args[1] ) ); return true;
	case 416: botlib_export->ea.EA_MoveLeft( static_cast<int>( args[1] ) ); return true;
	case 417: botlib_export->ea.EA_MoveRight( static_cast<int>( args[1] ) ); return true;
	case 418:
		botlib_export->ea.EA_SelectWeapon( static_cast<int>( args[1] ),
			static_cast<int>( args[2] ) );
		return true;
	case 419: botlib_export->ea.EA_Jump( static_cast<int>( args[1] ) ); return true;
	case 420: botlib_export->ea.EA_DelayedJump( static_cast<int>( args[1] ) ); return true;
	case 421:
		VM_CHECKBOUNDS( gvm, args[2], sizeof( vec3_t ) );
		botlib_export->ea.EA_Move( static_cast<int>( args[1] ),
			static_cast<float *>( VM_ArgPtr( args[2] ) ), VMF( 3 ) );
		return true;
	case 422:
		VM_CHECKBOUNDS( gvm, args[2], sizeof( vec3_t ) );
		botlib_export->ea.EA_View( static_cast<int>( args[1] ),
			static_cast<float *>( VM_ArgPtr( args[2] ) ) );
		return true;
	case 423:
		botlib_export->ea.EA_EndRegular( static_cast<int>( args[1] ), VMF( 2 ) );
		return true;
	case 424:
		/* The input frame the library built. It is the library's own record,
		 * not this title's: the retail engine hands the module's pointer
		 * straight to the library and lets it write, so the two must already
		 * agree, and this engine's library writes the same one. */
		VM_CHECKBOUNDS( gvm, args[3], sizeof( bot_input_t ) );
		botlib_export->ea.EA_GetInput( static_cast<int>( args[1] ), VMF( 2 ),
			static_cast<bot_input_t *>( VM_ArgPtr( args[3] ) ) );
		return true;
	case 425: botlib_export->ea.EA_ResetInput( static_cast<int>( args[1] ) ); return true;
	default: break;
	}

	/* --- the state a bot keeps ------------------------------------------
	 *
	 * Eleven of these, at the same numbers Jedi Academy uses, and that is not
	 * taken on resemblance. Each handler in retail jk2mp.exe reaches the same
	 * bot-library export table at 0xb0d230 that the setup call at 200 reaches,
	 * and the four that were read have the shapes the allocators have: 546
	 * takes a client number and returns what it made, 547 takes one and
	 * returns nothing, and 555 and 561 take none at all. Their slots in that
	 * table -- 0x198, 0x19c, 0x1c0, 0x1dc -- sit in the order the library
	 * declares its goal, move and weapon calls in.
	 *
	 * The library holds all three states without any of the configuration
	 * files this game does not ship, which is why the profile starts none of
	 * the configured subsystems. */
	switch ( trap ) {
	case 525: botlib_export->ai.BotResetGoalState( static_cast<int>( args[1] ) ); return true;
	case 526: botlib_export->ai.BotResetAvoidGoals( static_cast<int>( args[1] ) ); return true;
	case 546: result = botlib_export->ai.BotAllocGoalState( static_cast<int>( args[1] ) ); return true;
	case 547: botlib_export->ai.BotFreeGoalState( static_cast<int>( args[1] ) ); return true;
	case 548: botlib_export->ai.BotResetMoveState( static_cast<int>( args[1] ) ); return true;
	case 551: botlib_export->ai.BotResetAvoidReach( static_cast<int>( args[1] ) ); return true;
	case 555: result = botlib_export->ai.BotAllocMoveState(); return true;
	case 556: botlib_export->ai.BotFreeMoveState( static_cast<int>( args[1] ) ); return true;
	case 561: result = botlib_export->ai.BotAllocWeaponState(); return true;
	case 562: botlib_export->ai.BotFreeWeaponState( static_cast<int>( args[1] ) ); return true;
	case 563: botlib_export->ai.BotResetWeaponState( static_cast<int>( args[1] ) ); return true;
	default: break;
	}

	/* --- recorded motion paths, at this title's own numbers --------------
	 *
	 * Academy puts these at 52 to 56; Outcast puts them at 47 to 51, five
	 * lower, and that is read rather than shifted. Retail jk2mp.exe's routine
	 * at 0x435a30 -- the one trap 48 calls -- is the one that reports
	 * `^1ROFF System Error:`, and the `ROFF` name itself is referenced from
	 * the same run of code at 0x4351c7. The arities agree with Academy's
	 * block member for member: nothing, nothing, a name, three numbers, one
	 * number.
	 *
	 * As with Academy, starting a path stays refused. How this engine should
	 * move an entity along one is not measured for either title, and the same
	 * store answers both. */
	if ( trap == 47 ) { ravenRoff.Clear(); result = 1; return true; }
	if ( trap == 48 ) {
		ravenRoff.Advance( sv.time, []( const fntech3::roff::Playing &,
			const fntech3::jka::roff::Sample &, const fntech3::roff::Cached & ) {
			/* Nothing reaches here while starting a path is refused. */
		} );
		return true;
	}
	if ( trap == 49 ) {
		result = ravenRoff.Cache( static_cast<const char *>( VM_ArgPtr( args[1] ) ) );
		return true;
	}

	/* --- the bot library, at Quake III's own numbers ---------------------
	 *
	 * This is where the two Jedi titles diverge completely. Academy's run at
	 * 200 is Raven's navigation graph; Outcast's is Quake III's bot library,
	 * unmoved. Retail jk2mp.exe says so in its own words: handler 200 checks
	 * whether bots are enabled, then whether the library was ever bound, and
	 * if not prints `Error: SV_BotLibSetup without SV_BotInitBotLib` -- the
	 * engine names the function for us -- before tail-jumping into the
	 * library's own setup. Handler 201 is the same shape for the shutdown.
	 *
	 * The five this title's module calls in that run are 200, 201, 209, 210
	 * and 211, and their argument counts match Quake III's setup, shutdown,
	 * snapshot entity, console message and user command exactly. Only the two
	 * that carry no record of this title's are served here. */
	if ( trap == 200 ) { result = SV_BotLibSetup(); return true; }
	if ( trap == 201 ) { result = SV_BotLibShutdown(); return true; }

	/* --- the world, named from retail jk2mp.exe's own handlers -----------
	 *
	 * These are Quake III's numbers, and that is read rather than assumed.
	 * Each handler in the retail dispatcher says how many arguments it takes,
	 * how many of them it converts from module addresses, and what it hands
	 * them to:
	 *
	 *   23  two arguments, both pointers        -> a brush model by name
	 *   25  two, one pointer                    -> the contents at a point
	 *   26  two, both pointers                  -> two points in one set
	 *   28  two, one pointer                    -> an area portal's state
	 *   30  one pointer, and it calls 0x4502d0  -> linking an entity
	 *   31  one pointer, and it calls 0x450270  -> unlinking one
	 *   32  four, three pointers                -> the entities in a box
	 *   33  three, all pointers                 -> whether one touches a box
	 *
	 * 0x4502d0 and 0x450270 are the same two routines the engine's own link
	 * and unlink offsets were read out of, so those two are not a resemblance
	 * argument at all. See docs/fntech3/2026-09-22-jk2-records.md.
	 *
	 * Because they are Quake III's numbers and carry Quake III's arguments,
	 * the shared dispatcher serves them; this title's own entity state is
	 * converted underneath by the checked view, which now has an adapter for
	 * it.
	 *
	 * The trace at 24 is here on its own evidence, because Jedi Academy's is
	 * 48 bytes where Quake III's is 56 and a resemblance argument would have
	 * been worth nothing. Retail jk2mp.exe's own trace routine at 0x450b00
	 * hands the module's record to the collision tracer and then reads the
	 * fraction back at eight bytes into it, and writes the entity number at
	 * fifty-two -- both exactly where Quake III keeps them, and neither where
	 * Academy does. Its module passes Quake III's seven arguments, not the
	 * nine Academy passes. So this title's trace really is Quake III's, and
	 * the shared dispatcher may have it. */
	switch ( trap ) {
	case 23:  /* set brush model */
	case 25:  /* point contents */
	case 26:  /* in pvs */
	case 27:  /* in pvs ignoring portals */
	case 28:  /* adjust area portal state */
	case 30:  /* link entity */
	case 31:  /* unlink entity */
	case 32:  /* entities in box */
	case 33:  /* entity contact */
	/* And the two that hand a bot a client slot. Handler 34 takes no
	 * arguments and answers inline, which is what Quake III's allocator is;
	 * 35 frees the slot and clears the bot flag on the entity, which it reads
	 * and writes through the checked view, so the flag is cleared in this
	 * title's own record and not in a Quake III one. */
	case 34:  /* allocate a bot client */
	case 35:  /* free one */
		return false;
	default:
		break;
	}

	/* --- the run at 500, one call at a time, each named from the engine ---
	 *
	 * Retail jk2mp.exe's dispatcher is a byte index table at 0x44cc50 and a
	 * jump table at 0x44c8d8, so each import's handler can be read on its own
	 * rather than guessed from another title's names. This title's Ghoul2 run
	 * is measurably not Jedi Academy's: of the numbers both modules call, the
	 * elementary-action and AI-state blocks agree on every argument count and
	 * this one does not. See docs/fntech3/2026-09-22-jk2-records.md. */

	if ( trap == 36 ) {
		/* The input frame a client last sent. The engine copies this record
		 * whole -- its own reader is a seven-dword `rep movsd` -- so nothing
		 * about the inside of it follows from the engine, and the inside was
		 * read out of the client that builds one instead. The adapter places
		 * every field by name; a frame that does not convert is refused. */
		VM_CHECKBOUNDS( gvm, args[2], sizeof( fntech3_jk2_usercmd_t ) );
		usercmd_t internal{};
		SV_GetUsercmd( static_cast<int>( args[1] ), &internal );
		fntech3_jk2_usercmd_t external{};
		if ( !fntech3::jk2::state::FromInternal( internal, external ) ) {
			Com_Error( ERR_DROP, "Jedi Outcast cannot be told this input frame" );
		}
		std::memcpy( VM_ArgPtr( args[2] ), &external, sizeof( external ) );
		return true;
	}

	if ( trap == 24 ) {
		/* The world trace. This title's trace record really is Quake III's --
		 * its own engine reads the fraction eight bytes in and writes the
		 * entity number at fifty-two -- so the record needs no conversion.
		 * What it does need is the pass entity handled.
		 *
		 * This module passes -1 for "ignore nothing". Retail jk2mp.exe does
		 * not recognise that: its clip compares only against 1023 and would
		 * then index its entity array at -1, reading whatever lies in front of
		 * it for an owner number. The only defined reading of -1 is that there
		 * is no pass entity, and that is what the garbage it reads amounts to
		 * in practice, so it is said here deliberately rather than reproduced
		 * by reading out of bounds. Anything else is passed through untouched
		 * and refused by the server's own checks if it is not an entity. */
		const int passEntity = static_cast<int>( args[6] );
		trace_t traced{};
		SV_Trace( &traced, static_cast<const float *>( VM_ArgPtr( args[2] ) ),
			static_cast<const float *>( VM_ArgPtr( args[3] ) ),
			static_cast<const float *>( VM_ArgPtr( args[4] ) ),
			static_cast<const float *>( VM_ArgPtr( args[5] ) ),
			passEntity < 0 ? ENTITYNUM_NONE : passEntity,
			static_cast<int>( args[7] ), qfalse );
		VM_CHECKBOUNDS( gvm, args[1], sizeof( trace_t ) );
		std::memcpy( VM_ArgPtr( args[1] ), &traced, sizeof( traced ) );
		return true;
	}

	if ( trap == FNTECH3_JK2_G2_HAVE_MODELS ) {
		/* Whether an instance holds a model at all. Handler 0x44c64e tails
		 * into 0x4a7890, which walks the instance's models and stops at the
		 * first whose handle is not -1 -- so the question is whether any
		 * model is still there, not how many places there are. Academy asks
		 * the same question at 585. */
		const fntech3::ghoul2::Instance *instance = ravenGhoul2.Resolve( VM_ArgPtr( args[1] ) );
		result = instance && instance->HoldsAny() ? 1 : 0;
		return true;
	}

	if ( trap == FNTECH3_JK2_G2_ADD_BOLT ) {
		/* A named place on a model. Handler 0x44c72d converts the third
		 * argument and passes it to 0x4a75a0, which bounds-checks the model
		 * index against the instance's own count, takes that model, and hands
		 * the name to a lookup over two lists inside it -- the two a bolt can
		 * name, a surface and a bone. That is the call Jedi Academy makes at
		 * 593, and the same store answers it. */
		result = ravenGhoul2.AddBolt( VM_ArgPtr( args[1] ), static_cast<int>( args[2] ),
			static_cast<const char *>( VM_ArgPtr( args[3] ) ) );
		return true;
	}

	if ( trap == FNTECH3_JK2_G2_SET_BOLT_INFO ) {
		/* Which bolt a model is using. Handler 0x44c74a converts nothing and
		 * passes three numbers to 0x4a75f0, which bounds-checks the model
		 * index the same way and stores the third as a single int inside the
		 * model. Academy makes the same call at 594. */
		result = ravenGhoul2.SetBoltInfo( VM_ArgPtr( args[1] ), static_cast<int>( args[2] ),
			static_cast<int>( args[3] ) ) ? 1 : 0;
		return true;
	}

	if ( trap == FNTECH3_JK2_G2_INIT_MODEL ) {
		/* Handler 0x44c6ed. It converts the first two arguments from module
		 * addresses and passes them with five more to 0x4a6e90, which takes a
		 * pointer to the module's own instance pointer and a file name: if the
		 * pointer is null it allocates an instance and stores it back, then
		 * loads the named model into it. That is the same call Jedi Academy
		 * makes at 590, and the same store answers it.
		 *
		 * The model is really read, by the same bounded readers that swept
		 * both installations. A model that does not load is reported as not
		 * loaded, which is what this call returns for one, and the instance
		 * pointer handed back is owned here and checked on the way in. */
		if ( !args[1] ) { result = -1; return true; }
		VM_CHECKBOUNDS( gvm, args[1], sizeof( void * ) );
		auto **slot = static_cast<void **>( VM_ArgPtr( args[1] ) );
		const char *name = static_cast<const char *>( VM_ArgPtr( args[2] ) );
		fntech3::ghoul2::LoadError error = fntech3::ghoul2::LoadError::None;
		const int index = ravenGhoul2.AddModel( slot, name, error );
		if ( index < 0 ) {
			Com_Printf( "Ghoul2: %s: %s\n", name ? name : "(no name)",
				fntech3::ghoul2::LoadErrorName( error ) );
		}
		result = index;
		return true;
	}

	/* The call that publishes the module's arrays. Its records are measured
	 * now -- a 296-byte state, the engine's hundred-byte half at 296, an
	 * 828-byte entity and a 1,368-byte player state -- so the engine's checked
	 * view can bind them and the shared dispatcher does it. */
	if ( trap == 15 ) return false;

	if ( trap >= 0 && trap <= FNTECH3_JK2_GAME_IMPORT_HIGHEST ) {
		Com_Error( ERR_DROP, "Jedi Outcast game import %d is not served yet: its module "
			"boundary is open past Quake III's shared block. Retail jk2mp.exe handles %d "
			"numbers between 0 and %d, in runs at 0, 100, 200, 300, 400 and 500, and only "
			"the run at %d is named here. See docs/fntech3/COMPATIBILITY.md",
			trap, FNTECH3_JK2_GAME_IMPORT_HANDLED, FNTECH3_JK2_GAME_IMPORT_HIGHEST,
			FNTECH3_JK2_SHARED_FIRST );
	}
	Com_Error( ERR_DROP, "Jedi Outcast game import %d is past the %d its own engine will "
		"dispatch, so it is not one of this title's imports at all",
		trap, FNTECH3_JK2_GAME_IMPORT_HIGHEST );
	return true;
}

#endif
