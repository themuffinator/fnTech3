/*
===========================================================================
Copyright (C) 2026 fnTech3 contributors

This file is part of fnTech3 and is distributed under the terms of the GNU
General Public License, version 2 or (at your option) any later version.
===========================================================================
*/

/* The ICARUS interpreter, driven against a host of this test's own making.
 * Every script here is written by this test; no retail file is used, and no
 * game module is involved. What the host records is what the engine decided
 * to hand a game, which is the whole of what this boundary promises. */

#include "../code/icarus/icarus_runtime.hpp"

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#define CHECK( x ) do { if ( !( x ) ) { \
	std::cerr << __LINE__ << ": " #x "\n"; std::exit( 1 ); } } while ( 0 )

namespace icarus = fntech3::icarus;
namespace format = fntech3::jka::icarus;
using icarus::Command;
using icarus::LoadError;
using icarus::RunState;
using Bytes = std::vector<std::uint8_t>;

/* --- writing a script the compiler would have written --------------------- */

static void AppendInt( Bytes &file, std::int32_t value )
{
	for ( int i = 0; i < 4; ++i ) file.push_back( std::uint8_t( std::uint32_t( value ) >> ( 8 * i ) ) );
}

static Bytes Header()
{
	Bytes file{ 'I', 'B', 'I', 0 };
	AppendInt( file, std::int32_t( format::VersionBits ) );
	return file;
}

static void Block( Bytes &file, format::BlockId id, int members )
{
	AppendInt( file, format::Number( id ) );
	AppendInt( file, members );
	file.push_back( 0 );
}

static void Text( Bytes &file, const std::string &value, std::int32_t id = format::MemberText )
{
	AppendInt( file, id );
	AppendInt( file, std::int32_t( value.size() + 1 ) );
	file.insert( file.end(), value.begin(), value.end() );
	file.push_back( 0 );
}

static void Number( Bytes &file, float value, std::int32_t id = format::MemberNumber )
{
	std::uint32_t word;
	std::memcpy( &word, &value, sizeof( word ) );
	AppendInt( file, id );
	AppendInt( file, 4 );
	for ( int i = 0; i < 4; ++i ) file.push_back( std::uint8_t( word >> ( 8 * i ) ) );
}

static void Marker( Bytes &file, std::int32_t id ) { Number( file, float( id ), id ); }

static void Vector( Bytes &file, float x, float y, float z )
{
	Marker( file, format::MemberVector );
	Number( file, x );
	Number( file, y );
	Number( file, z );
}

static void End( Bytes &file ) { Block( file, format::BlockId::ID_BLOCK_END, 0 ); }

/* --- a host that remembers what it was asked ------------------------------ */

struct Recorded {
	Command command;
	icarus::EntityId entity;
	std::int32_t taskId;
	std::string first, second;
	float origin[3], angles[3], duration;
};

class TestHost final : public icarus::Host {
public:
	std::map<std::string, Bytes> scripts;
	std::map<std::string, icarus::EntityId> entities;
	std::map<std::string, icarus::HostValue> queries;
	std::vector<std::string> queriedNames;
	std::vector<Recorded> dispatched;
	std::vector<std::string> printed;
	std::vector<std::string> failures;
	int now = 0;
	bool refuse = false;

	std::size_t ReadScript( const char *name, void *destination, std::size_t capacity ) override
	{
		const auto found = scripts.find( name );
		if ( found == scripts.end() ) return 0;
		if ( !destination || capacity < found->second.size() ) return found->second.size();
		std::memcpy( destination, found->second.data(), found->second.size() );
		return found->second.size();
	}
	int Milliseconds() override { return now; }
	icarus::EntityId FindEntity( const char *name ) override
	{
		const auto found = entities.find( name );
		return found == entities.end() ? icarus::NoEntity : found->second;
	}
	bool Dispatch( const icarus::Request &request ) override
	{
		if ( refuse ) return false;
		Recorded record{};
		record.command = request.command;
		record.entity = request.entity;
		record.taskId = request.taskId;
		record.first = request.first ? request.first : "";
		record.second = request.second ? request.second : "";
		std::memcpy( record.origin, request.origin, sizeof( record.origin ) );
		std::memcpy( record.angles, request.angles, sizeof( record.angles ) );
		record.duration = request.duration;
		dispatched.push_back( record );
		return true;
	}
	bool Query( icarus::EntityId, icarus::ValueKind kind, const char *name,
		icarus::HostValue &value ) override
	{
		queriedNames.push_back( name );
		const auto found = queries.find( name );
		if ( found == queries.end() || found->second.kind != kind ) return false;
		value = found->second;
		return true;
	}
	void Print( const char *text ) override { printed.push_back( text ); }
	void Fail( const char *reason ) override { failures.push_back( reason ); }
};

/* Run until nothing moves, so a test never depends on how many ticks a thing
 * happens to take. */
static void Settle( icarus::Interpreter &interpreter, int ticks = 8 )
{
	for ( int i = 0; i < ticks; ++i ) interpreter.Tick();
}

/* --- the tests ------------------------------------------------------------ */

static void CheckSetSplitsBetweenEngineAndGame()
{
	Bytes file = Header();
	/* A declared variable the engine owns, and a property the game owns. */
	Block( file, format::BlockId::ID_DECLARE, 2 );
	Number( file, float( format::MemberNumber ) );
	Text( file, "fCount" );
	Block( file, format::BlockId::ID_SET, 2 );
	Text( file, "fCount" );
	Number( file, 7.0f );
	Block( file, format::BlockId::ID_SET, 2 );
	Text( file, "SET_SOLID" );
	Text( file, "false" );

	TestHost host;
	host.scripts["t"] = file;
	icarus::Interpreter interpreter( host );
	interpreter.Initialize();
	LoadError error = LoadError::None;
	CHECK( interpreter.RunScript( 3, "t", error ) );
	Settle( interpreter );

	CHECK( host.failures.empty() );
	/* The variable never reached the game. */
	CHECK( host.dispatched.size() == 1 );
	CHECK( host.dispatched[0].command == Command::Set );
	CHECK( host.dispatched[0].first == "SET_SOLID" && host.dispatched[0].second == "false" );
	CHECK( host.dispatched[0].entity == 3 && host.dispatched[0].taskId > 0 );

	icarus::HostValue value;
	CHECK( interpreter.Declared( "fCount" ) );
	CHECK( interpreter.GetVariable( "fCount", icarus::ValueKind::Number, value ) );
	CHECK( value.numbers[0] == 7.0f );
	CHECK( !interpreter.Declared( "SET_SOLID" ) );
}

static void CheckConditionalsTakeOneBranch()
{
	for ( int match = 0; match < 2; ++match ) {
		Bytes file = Header();
		Block( file, format::BlockId::ID_IF, 3 );
		Number( file, 1.0f );
		Marker( file, format::Number( format::Token::TK_EQUALS ) );
		Number( file, match ? 1.0f : 2.0f );
		Block( file, format::BlockId::ID_PRINT, 1 );
		Text( file, "then" );
		End( file );
		Block( file, format::BlockId::ID_ELSE, 0 );
		Block( file, format::BlockId::ID_PRINT, 1 );
		Text( file, "else" );
		End( file );
		Block( file, format::BlockId::ID_PRINT, 1 );
		Text( file, "after" );

		TestHost host;
		host.scripts["t"] = file;
		icarus::Interpreter interpreter( host );
		interpreter.Initialize();
		LoadError error = LoadError::None;
		CHECK( interpreter.RunScript( 1, "t", error ) );
		Settle( interpreter );
		CHECK( host.failures.empty() );
		CHECK( host.printed.size() == 2 );
		CHECK( host.printed[0] == ( match ? "then" : "else" ) );
		CHECK( host.printed[1] == "after" );
	}
}

static void CheckLoopsRunTheirCount()
{
	Bytes file = Header();
	Block( file, format::BlockId::ID_LOOP, 1 );
	Number( file, 3.0f );
	Block( file, format::BlockId::ID_PRINT, 1 );
	Text( file, "round" );
	End( file );
	Block( file, format::BlockId::ID_PRINT, 1 );
	Text( file, "done" );

	TestHost host;
	host.scripts["t"] = file;
	icarus::Interpreter interpreter( host );
	interpreter.Initialize();
	LoadError error = LoadError::None;
	CHECK( interpreter.RunScript( 1, "t", error ) );
	Settle( interpreter );
	CHECK( host.failures.empty() );
	CHECK( host.printed.size() == 4 );
	for ( int i = 0; i < 3; ++i ) CHECK( host.printed[std::size_t( i )] == "round" );
	CHECK( host.printed[3] == "done" );
}

static void CheckWaitHoldsOnTheClock()
{
	Bytes file = Header();
	Block( file, format::BlockId::ID_PRINT, 1 );
	Text( file, "before" );
	Block( file, format::BlockId::ID_WAIT, 1 );
	Number( file, 1000.0f );
	Block( file, format::BlockId::ID_PRINT, 1 );
	Text( file, "after" );

	TestHost host;
	host.now = 5000;
	host.scripts["t"] = file;
	icarus::Interpreter interpreter( host );
	interpreter.Initialize();
	LoadError error = LoadError::None;
	CHECK( interpreter.RunScript( 1, "t", error ) );

	interpreter.Tick();
	CHECK( host.printed.size() == 1 && host.printed[0] == "before" );
	CHECK( interpreter.Find( 1 )->State() == RunState::WaitingTime );

	host.now = 5999;
	interpreter.Tick();
	CHECK( host.printed.size() == 1 );

	host.now = 6000;
	interpreter.Tick();
	CHECK( host.printed.size() == 2 && host.printed[1] == "after" );
	CHECK( interpreter.Find( 1 )->State() == RunState::Idle );
}

/* A task's work is the game's, so a wait on it holds until the game says the
 * commands it was handed are finished. */
static void CheckWaitHoldsOnTheGame()
{
	Bytes file = Header();
	Block( file, format::BlockId::ID_TASK, 1 );
	Text( file, "speak" );
	Block( file, format::BlockId::ID_SOUND, 2 );
	Text( file, "CHAN_VOICE", format::MemberName );
	Text( file, "sound/one.mp3" );
	Block( file, format::BlockId::ID_SOUND, 2 );
	Text( file, "CHAN_VOICE", format::MemberName );
	Text( file, "sound/two.mp3" );
	End( file );
	Block( file, format::BlockId::ID_DO, 1 );
	Text( file, "speak" );
	Block( file, format::BlockId::ID_WAIT, 1 );
	Text( file, "speak" );
	Block( file, format::BlockId::ID_PRINT, 1 );
	Text( file, "spoken" );

	TestHost host;
	host.scripts["t"] = file;
	icarus::Interpreter interpreter( host );
	interpreter.Initialize();
	LoadError error = LoadError::None;
	CHECK( interpreter.RunScript( 2, "t", error ) );

	interpreter.Tick();
	CHECK( host.failures.empty() );
	CHECK( host.dispatched.size() == 2 );
	CHECK( host.dispatched[0].command == Command::PlaySound );
	CHECK( host.dispatched[0].first == "CHAN_VOICE" );
	CHECK( host.dispatched[0].second == "sound/one.mp3" );
	CHECK( host.printed.empty() );
	CHECK( interpreter.Find( 2 )->State() == RunState::WaitingTask );
	CHECK( interpreter.PendingCount() == 2 );

	/* One finished is not both. */
	CHECK( interpreter.CompleteTask( host.dispatched[0].taskId ) );
	interpreter.Tick();
	CHECK( host.printed.empty() );

	CHECK( interpreter.CompleteTask( host.dispatched[1].taskId ) );
	interpreter.Tick();
	CHECK( host.printed.size() == 1 && host.printed[0] == "spoken" );
	CHECK( interpreter.PendingCount() == 0 );
	/* A task identifier is finished once. */
	CHECK( !interpreter.CompleteTask( host.dispatched[0].taskId ) );
}

/* An affect runs its body on another entity, and a flush clears what that
 * entity was doing first. */
static void CheckAffectMovesToAnotherEntity()
{
	Bytes file = Header();
	Block( file, format::BlockId::ID_AFFECT, 2 );
	Text( file, "npc_kyle" );
	Number( file, float( format::Number( format::Type::TYPE_FLUSH ) ) );
	Block( file, format::BlockId::ID_SET, 2 );
	Text( file, "SET_ANIM_BOTH" );
	Text( file, "BOTH_STAND1" );
	End( file );
	Block( file, format::BlockId::ID_SET, 2 );
	Text( file, "SET_SOLID" );
	Text( file, "true" );

	TestHost host;
	host.scripts["t"] = file;
	host.entities["npc_kyle"] = 42;
	icarus::Interpreter interpreter( host );
	interpreter.Initialize();
	LoadError error = LoadError::None;
	CHECK( interpreter.RunScript( 7, "t", error ) );
	Settle( interpreter );

	CHECK( host.failures.empty() );
	CHECK( host.dispatched.size() == 2 );
	/* The body ran as the affected entity; the rest stayed with the runner. */
	const Recorded *anim = nullptr, *solid = nullptr;
	for ( const Recorded &record : host.dispatched ) {
		if ( record.first == "SET_ANIM_BOTH" ) anim = &record;
		if ( record.first == "SET_SOLID" ) solid = &record;
	}
	CHECK( anim && anim->entity == 42 );
	CHECK( solid && solid->entity == 7 );
	CHECK( interpreter.Bound( 42 ) );
}

static void CheckAffectNamingNothingStops()
{
	Bytes file = Header();
	Block( file, format::BlockId::ID_AFFECT, 2 );
	Text( file, "npc_absent" );
	Number( file, float( format::Number( format::Type::TYPE_FLUSH ) ) );
	Block( file, format::BlockId::ID_PRINT, 1 );
	Text( file, "never" );
	End( file );

	TestHost host;
	host.scripts["t"] = file;
	icarus::Interpreter interpreter( host );
	interpreter.Initialize();
	LoadError error = LoadError::None;
	CHECK( interpreter.RunScript( 1, "t", error ) );
	Settle( interpreter );
	CHECK( host.printed.empty() );
	CHECK( host.failures.size() == 1 );
	CHECK( host.failures[0].find( "npc_absent" ) != std::string::npos );
	CHECK( interpreter.Find( 1 )->State() == RunState::Failed );
}

static void CheckSignalsPassBetweenEntities()
{
	Bytes waiter = Header();
	Block( waiter, format::BlockId::ID_WAITSIGNAL, 1 );
	Text( waiter, "doorOpen" );
	Block( waiter, format::BlockId::ID_PRINT, 1 );
	Text( waiter, "heard" );

	Bytes raiser = Header();
	Block( raiser, format::BlockId::ID_SIGNAL, 1 );
	Text( raiser, "doorOpen" );

	TestHost host;
	host.scripts["waiter"] = waiter;
	host.scripts["raiser"] = raiser;
	icarus::Interpreter interpreter( host );
	interpreter.Initialize();
	LoadError error = LoadError::None;
	CHECK( interpreter.RunScript( 1, "waiter", error ) );
	Settle( interpreter );
	CHECK( host.printed.empty() );
	CHECK( interpreter.Find( 1 )->State() == RunState::WaitingSignal );

	CHECK( interpreter.RunScript( 2, "raiser", error ) );
	Settle( interpreter );
	CHECK( host.printed.size() == 1 && host.printed[0] == "heard" );
	/* The signal was taken, not left standing for the next waiter. */
	CHECK( !interpreter.SignalRaised( "doorOpen" ) );
}

static void CheckGetReachesVariablesThenTheGame()
{
	Bytes file = Header();
	Block( file, format::BlockId::ID_DECLARE, 2 );
	Number( file, float( format::MemberNumber ) );
	Text( file, "fMine" );
	Block( file, format::BlockId::ID_SET, 2 );
	Text( file, "fMine" );
	Number( file, 4.0f );
	/* A get of the declared one never asks the game; a get of another does. */
	/* A block header counts members: a get is three of them, the operator one
	 * and the value it compares against one more. */
	Block( file, format::BlockId::ID_IF, 5 );
	Marker( file, format::MemberTyped );
	Number( file, float( format::MemberNumber ) );
	Text( file, "fMine" );
	Marker( file, format::Number( format::Token::TK_EQUALS ) );
	Number( file, 4.0f );
	Block( file, format::BlockId::ID_IF, 5 );
	Marker( file, format::MemberTyped );
	Number( file, float( format::MemberNumber ) );
	Text( file, "HEALTH" );
	Marker( file, format::Number( format::Token::TK_GREATER_THAN ) );
	Number( file, 50.0f );
	Block( file, format::BlockId::ID_PRINT, 1 );
	Text( file, "healthy" );
	End( file );
	End( file );

	TestHost host;
	host.scripts["t"] = file;
	icarus::HostValue health;
	health.kind = icarus::ValueKind::Number;
	health.numbers[0] = 100.0f;
	host.queries["HEALTH"] = health;

	icarus::Interpreter interpreter( host );
	interpreter.Initialize();
	LoadError error = LoadError::None;
	CHECK( interpreter.RunScript( 5, "t", error ) );
	Settle( interpreter );
	CHECK( host.failures.empty() );
	CHECK( host.printed.size() == 1 && host.printed[0] == "healthy" );
	CHECK( host.queriedNames.size() == 1 && host.queriedNames[0] == "HEALTH" );
}

static void CheckRunChainsScripts()
{
	Bytes first = Header();
	Block( first, format::BlockId::ID_PRINT, 1 );
	Text( first, "one" );
	Block( first, format::BlockId::ID_RUN, 1 );
	Text( first, "second" );
	Block( first, format::BlockId::ID_PRINT, 1 );
	Text( first, "three" );

	Bytes second = Header();
	Block( second, format::BlockId::ID_PRINT, 1 );
	Text( second, "two" );

	TestHost host;
	host.scripts["first"] = first;
	host.scripts["second"] = second;
	icarus::Interpreter interpreter( host );
	interpreter.Initialize();
	LoadError error = LoadError::None;
	CHECK( interpreter.RunScript( 1, "first", error ) );
	Settle( interpreter );
	CHECK( host.failures.empty() );
	CHECK( host.printed.size() == 3 );
	CHECK( host.printed[0] == "one" && host.printed[1] == "two" && host.printed[2] == "three" );
	CHECK( interpreter.ScriptCount() == 2 );
}

static void CheckMovementReachesTheGame()
{
	Bytes file = Header();
	Block( file, format::BlockId::ID_MOVE, 5 );
	Vector( file, 10.0f, -20.0f, 30.0f );
	Number( file, 2500.0f );
	Block( file, format::BlockId::ID_ROTATE, 5 );
	Vector( file, 0.0f, 90.0f, 0.0f );
	Number( file, 1000.0f );

	TestHost host;
	host.scripts["t"] = file;
	icarus::Interpreter interpreter( host );
	interpreter.Initialize();
	LoadError error = LoadError::None;
	CHECK( interpreter.RunScript( 9, "t", error ) );
	Settle( interpreter );
	CHECK( host.failures.empty() );
	CHECK( host.dispatched.size() == 2 );
	CHECK( host.dispatched[0].command == Command::MoveOrigin );
	CHECK( host.dispatched[0].origin[0] == 10.0f && host.dispatched[0].origin[1] == -20.0f );
	CHECK( host.dispatched[0].origin[2] == 30.0f && host.dispatched[0].duration == 2500.0f );
	CHECK( host.dispatched[1].command == Command::MoveAngles );
	CHECK( host.dispatched[1].angles[1] == 90.0f && host.dispatched[1].duration == 1000.0f );
}

static void CheckRefusals()
{
	/* A command with no implementation stops by name rather than passing. */
	{
		Bytes file = Header();
		Block( file, format::BlockId::ID_CAMERA, 2 );
		Number( file, float( format::Number( format::Type::TYPE_PAN ) ) );
		Number( file, 1.0f );
		TestHost host;
		host.scripts["t"] = file;
		icarus::Interpreter interpreter( host );
		interpreter.Initialize();
		LoadError error = LoadError::None;
		CHECK( interpreter.RunScript( 1, "t", error ) );
		Settle( interpreter );
		CHECK( host.failures.size() == 1 );
		CHECK( host.failures[0].find( "ID_CAMERA" ) != std::string::npos );
		CHECK( host.failures[0].find( "not served yet" ) != std::string::npos );
	}
	/* A game that will not take a command stops the script too. */
	{
		Bytes file = Header();
		Block( file, format::BlockId::ID_USE, 1 );
		Text( file, "door1" );
		TestHost host;
		host.scripts["t"] = file;
		host.refuse = true;
		icarus::Interpreter interpreter( host );
		interpreter.Initialize();
		LoadError error = LoadError::None;
		CHECK( interpreter.RunScript( 1, "t", error ) );
		Settle( interpreter );
		CHECK( host.failures.size() == 1 );
		CHECK( host.failures[0].find( "refused" ) != std::string::npos );
	}
	/* A script that is not there is a load failure, named. */
	{
		TestHost host;
		icarus::Interpreter interpreter( host );
		interpreter.Initialize();
		LoadError error = LoadError::None;
		CHECK( !interpreter.RunScript( 1, "absent", error ) );
		CHECK( error == LoadError::Missing );
		CHECK( !std::strcmp( icarus::LoadErrorName( error ), "no such script" ) );
		/* Asked twice, it is not read twice. */
		CHECK( !interpreter.RunScript( 1, "absent", error ) );
	}
	/* A body that never closes cannot be run. */
	{
		Bytes file = Header();
		Block( file, format::BlockId::ID_LOOP, 1 );
		Number( file, 1.0f );
		Block( file, format::BlockId::ID_PRINT, 1 );
		Text( file, "inside" );
		TestHost host;
		host.scripts["t"] = file;
		icarus::Interpreter interpreter( host );
		interpreter.Initialize();
		LoadError error = LoadError::None;
		CHECK( !interpreter.RunScript( 1, "t", error ) );
		CHECK( error == LoadError::Nesting );
	}
	/* A task nobody declared, and a wait on one nobody started. */
	{
		Bytes file = Header();
		Block( file, format::BlockId::ID_DO, 1 );
		Text( file, "nowhere" );
		TestHost host;
		host.scripts["t"] = file;
		icarus::Interpreter interpreter( host );
		interpreter.Initialize();
		LoadError error = LoadError::None;
		CHECK( interpreter.RunScript( 1, "t", error ) );
		Settle( interpreter );
		CHECK( host.failures.size() == 1 );
		CHECK( host.failures[0].find( "nowhere" ) != std::string::npos );
	}
	for ( LoadError error : { LoadError::None, LoadError::Missing, LoadError::TooLarge,
		LoadError::Container, LoadError::Values, LoadError::Nesting } ) {
		const char *name = icarus::LoadErrorName( error );
		CHECK( name && name[0] && std::strcmp( name, "unknown" ) );
	}
	for ( RunState state : { RunState::Idle, RunState::Running, RunState::WaitingTime,
		RunState::WaitingTask, RunState::WaitingSignal, RunState::Failed } ) {
		CHECK( std::strcmp( icarus::RunStateName( state ), "unknown" ) );
	}
}

static void CheckShutdownForgetsEverything()
{
	Bytes file = Header();
	Block( file, format::BlockId::ID_DECLARE, 2 );
	Number( file, float( format::MemberNumber ) );
	Text( file, "fKept" );
	Block( file, format::BlockId::ID_SOUND, 2 );
	Text( file, "CHAN_AUTO", format::MemberName );
	Text( file, "sound/x.mp3" );

	TestHost host;
	host.scripts["t"] = file;
	icarus::Interpreter interpreter( host );
	interpreter.Initialize();
	LoadError error = LoadError::None;
	CHECK( interpreter.RunScript( 1, "t", error ) );
	Settle( interpreter );
	CHECK( interpreter.Declared( "fKept" ) && interpreter.SequencerCount() == 1 );
	CHECK( interpreter.PendingCount() == 1 && interpreter.ScriptCount() == 1 );

	interpreter.Shutdown();
	CHECK( !interpreter.Initialized() && !interpreter.Declared( "fKept" ) );
	CHECK( interpreter.SequencerCount() == 0 && interpreter.PendingCount() == 0 );
	CHECK( interpreter.ScriptCount() == 0 );
	/* Nothing runs once it has shut down. */
	interpreter.Tick();
	CHECK( host.printed.empty() );

	/* Releasing an entity takes its outstanding work with it. */
	interpreter.Initialize();
	CHECK( interpreter.RunScript( 4, "t", error ) );
	Settle( interpreter );
	CHECK( interpreter.PendingCount() == 1 );
	interpreter.Release( 4 );
	CHECK( interpreter.SequencerCount() == 0 && interpreter.PendingCount() == 0 );
}

int main()
{
	CheckSetSplitsBetweenEngineAndGame();
	CheckConditionalsTakeOneBranch();
	CheckLoopsRunTheirCount();
	CheckWaitHoldsOnTheClock();
	CheckWaitHoldsOnTheGame();
	CheckAffectMovesToAnotherEntity();
	CheckAffectNamingNothingStops();
	CheckSignalsPassBetweenEntities();
	CheckGetReachesVariablesThenTheGame();
	CheckRunChainsScripts();
	CheckMovementReachesTheGame();
	CheckRefusals();
	CheckShutdownForgetsEverything();
	std::cout << "Jedi Academy ICARUS interpreter tests passed\n";
	return 0;
}
