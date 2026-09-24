/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_ICARUS_RUNTIME_HPP
#define FNTECH3_ICARUS_RUNTIME_HPP

/* The ICARUS interpreter: what actually runs a compiled script.
 *
 * One sequencer per entity. A sequencer holds a stack of frames, each a span
 * of statements in some script, and runs them until it finishes or blocks. It
 * blocks on three things and no others: a time, a task group the game has not
 * finished, and a signal nobody has raised.
 *
 * The engine owns flow, variables and timing. Every command that touches an
 * entity is handed to the game through the Host, because that is where Raven
 * put the boundary and the retail module's export table still expects it
 * there. A command this does not implement stops the script by name rather
 * than being skipped, so a script never half-runs.
 *
 * Which block number means what is Raven's, recorded in
 * code/compat/jka_icarus_ids.def. How each one behaves is fnTech3's own, and
 * docs/fntech3/2026-09-22-jka-icarus-runtime.md says which behaviours the
 * scripts themselves fix and which are this engine's choice. */

#include "icarus_script.hpp"

#include <cstring>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace fntech3 {
namespace icarus {

/* A variable a script declared, in one of the three kinds it can be. */
struct Variable {
	ValueKind kind = ValueKind::Number;
	std::string text;
	float numbers[3] = { 0.0f, 0.0f, 0.0f };
};

/* How far a sequencer got, and why it stopped. */
enum class RunState {
	Idle,          /* nothing to run */
	Running,       /* more to do this tick */
	WaitingTime,   /* until a moment on the level clock */
	WaitingTask,   /* until the game finishes a task group */
	WaitingSignal, /* until something raises a signal */
	Failed,        /* the script stopped and will not continue */
};

inline const char *RunStateName( RunState state ) {
	switch ( state ) {
	case RunState::Idle: return "idle";
	case RunState::Running: return "running";
	case RunState::WaitingTime: return "waiting on the clock";
	case RunState::WaitingTask: return "waiting on the game";
	case RunState::WaitingSignal: return "waiting on a signal";
	case RunState::Failed: return "failed";
	}
	return "unknown";
}

class Interpreter;

/* One entity's script state. */
class Sequencer {
public:
	explicit Sequencer( EntityId entity = NoEntity ) : entity_( entity ) {}

	EntityId Entity() const { return entity_; }
	RunState State() const { return state_; }
	int Depth() const { return int( frames_.size() ); }
	bool Busy() const { return state_ != RunState::Idle && state_ != RunState::Failed; }

private:
	friend class Interpreter;

	enum class FrameKind { Body, Loop, Task };

	struct Frame {
		const Script *script = nullptr;
		int index = 0;             /* the next statement to run */
		int end = 0;               /* one past this body's last statement */
		FrameKind kind = FrameKind::Body;
		int loopStart = 0;
		int remaining = 0;         /* iterations left; -1 runs forever */
		int skipElseAt = -1;       /* an else to pass over, having taken the if */
		std::string group;         /* the task group this frame's work belongs to */
	};

	EntityId entity_ = NoEntity;
	RunState state_ = RunState::Idle;
	std::vector<Frame> frames_;
	int wakeAt_ = 0;
	std::string waitingFor_;                      /* task group or signal */
	std::unordered_map<std::string, int> taskOutstanding_;
	std::unordered_set<std::string> taskStarted_;
	/* A task body, recorded where it was declared. */
	struct TaskBody { const Script *script = nullptr; int begin = 0, end = 0; };
	std::unordered_map<std::string, TaskBody> tasks_;
};

/* The one interpreter a level has. */
class Interpreter {
public:
	explicit Interpreter( Host &host ) : host_( &host ) {}

	void Shutdown();
	bool Initialized() const { return initialized_; }
	void Initialize() { Shutdown(); initialized_ = true; }

	/* Give an entity a sequencer, or take it away. */
	Sequencer &Bind( EntityId entity );
	void Release( EntityId entity );
	bool Bound( EntityId entity ) const { return sequencers_.count( entity ) != 0; }
	Sequencer *Find( EntityId entity );

	/* Start a script on an entity. False when it could not be loaded, with
	 * `error` saying why. */
	bool RunScript( EntityId entity, const std::string &name, LoadError &error );

	/* Run every sequencer as far as it will go. */
	void Tick();

	/* Run one entity's sequencer, which is how a game module that maintains
	 * its entities one at a time drives this. */
	void TickEntity( EntityId entity );

	/* The game says a command it was handed has finished. */
	bool CompleteTask( std::int32_t taskId );
	bool TaskPending( std::int32_t taskId ) const { return pending_.count( taskId ) != 0; }

	/* Variables, which the ICARUS imports also reach directly. */
	bool Declared( const std::string &name ) const { return variables_.count( name ) != 0; }
	void Declare( const std::string &name, ValueKind kind );
	bool SetVariable( const std::string &name, const HostValue &value );
	bool GetVariable( const std::string &name, ValueKind kind, HostValue &value ) const;
	void Free( const std::string &name ) { variables_.erase( name ); }

	/* Signals, which a script raises for another script to wait on. */
	void Signal( const std::string &name ) { signals_.insert( name ); }
	bool SignalRaised( const std::string &name ) const { return signals_.count( name ) != 0; }

	std::size_t SequencerCount() const { return sequencers_.size(); }
	std::size_t ScriptCount() const { return cache_.Count(); }
	/* How many commands the game has been handed and not yet finished. */
	std::size_t PendingCount() const { return pending_.size(); }

private:
	enum class Step { Continue, Blocked, Finished, Failed };

	Step RunOne( Sequencer &sequencer );
	void Advance( Sequencer &sequencer, int now );
	Step Dispatch( Sequencer &sequencer, const Statement &statement,
		const Value *values, int count );
	bool Resolve( Sequencer &sequencer, const Value &value, ValueKind want,
		HostValue &out, std::string &storage );
	bool Compare( const HostValue &left, std::int32_t op, const HostValue &right,
		bool &result ) const;
	void Fail( Sequencer &sequencer, const std::string &reason );
	std::int32_t IssueTask( Sequencer &sequencer );

	Host *host_ = nullptr;
	bool initialized_ = false;
	ScriptCache cache_;
	std::unordered_map<EntityId, Sequencer> sequencers_;
	std::unordered_map<std::string, Variable> variables_;
	std::unordered_set<std::string> signals_;
	/* A handed-out task identifier, and whose group it belongs to. */
	struct PendingTask { EntityId entity = NoEntity; std::string group; };
	std::unordered_map<std::int32_t, PendingTask> pending_;
	std::int32_t nextTask_ = 1;
	/* A script's random is the engine's, not the C library's, so a probe of
	 * the same level repeats exactly. */
	std::uint32_t seed_ = 0x13572468u;
};

inline void Interpreter::Shutdown()
{
	initialized_ = false;
	cache_.Clear();
	sequencers_.clear();
	variables_.clear();
	signals_.clear();
	pending_.clear();
	nextTask_ = 1;
}

inline Sequencer &Interpreter::Bind( EntityId entity )
{
	const auto found = sequencers_.find( entity );
	if ( found != sequencers_.end() ) return found->second;
	return sequencers_.emplace( entity, Sequencer( entity ) ).first->second;
}

inline void Interpreter::Release( EntityId entity )
{
	sequencers_.erase( entity );
	for ( auto it = pending_.begin(); it != pending_.end(); ) {
		if ( it->second.entity == entity ) it = pending_.erase( it );
		else ++it;
	}
}

inline Sequencer *Interpreter::Find( EntityId entity )
{
	const auto found = sequencers_.find( entity );
	return found == sequencers_.end() ? nullptr : &found->second;
}

inline void Interpreter::Declare( const std::string &name, ValueKind kind )
{
	Variable &variable = variables_[name];
	variable = Variable{};
	variable.kind = kind;
}

inline bool Interpreter::SetVariable( const std::string &name, const HostValue &value )
{
	const auto found = variables_.find( name );
	if ( found == variables_.end() ) return false;
	Variable &variable = found->second;
	if ( variable.kind != value.kind ) return false;
	if ( value.kind == ValueKind::Text ) variable.text = value.text ? value.text : "";
	else std::memcpy( variable.numbers, value.numbers, sizeof( variable.numbers ) );
	return true;
}

inline bool Interpreter::GetVariable( const std::string &name, ValueKind kind,
	HostValue &value ) const
{
	const auto found = variables_.find( name );
	if ( found == variables_.end() || found->second.kind != kind ) return false;
	value = HostValue{};
	value.kind = kind;
	if ( kind == ValueKind::Text ) value.text = found->second.text.c_str();
	else std::memcpy( value.numbers, found->second.numbers, sizeof( value.numbers ) );
	return true;
}

inline bool Interpreter::RunScript( EntityId entity, const std::string &name, LoadError &error )
{
	const Script *script = cache_.Acquire( *host_, name, error );
	if ( !script ) return false;
	Sequencer &sequencer = Bind( entity );
	Sequencer::Frame frame;
	frame.script = script;
	frame.index = 0;
	frame.end = script->StatementCount();
	frame.kind = Sequencer::FrameKind::Body;
	sequencer.frames_.push_back( frame );
	sequencer.state_ = RunState::Running;
	return true;
}

inline void Interpreter::Fail( Sequencer &sequencer, const std::string &reason )
{
	sequencer.state_ = RunState::Failed;
	sequencer.frames_.clear();
	host_->Fail( reason.c_str() );
}

inline std::int32_t Interpreter::IssueTask( Sequencer &sequencer )
{
	const std::int32_t id = nextTask_++;
	PendingTask task;
	task.entity = sequencer.Entity();
	task.group = sequencer.frames_.empty() ? std::string() : sequencer.frames_.back().group;
	if ( !task.group.empty() ) ++sequencer.taskOutstanding_[task.group];
	pending_.emplace( id, task );
	return id;
}

inline bool Interpreter::CompleteTask( std::int32_t taskId )
{
	const auto found = pending_.find( taskId );
	if ( found == pending_.end() ) return false;
	const EntityId entity = found->second.entity;
	const std::string group = found->second.group;
	pending_.erase( found );
	Sequencer *sequencer = Find( entity );
	if ( sequencer && !group.empty() ) {
		const auto counted = sequencer->taskOutstanding_.find( group );
		if ( counted != sequencer->taskOutstanding_.end() && counted->second > 0 ) {
			--counted->second;
		}
	}
	return true;
}

/* Turn one of a statement's values into something the game or a comparison can
 * use. A get reaches a variable or the game; a random picks inside its bounds. */
inline bool Interpreter::Resolve( Sequencer &sequencer, const Value &value,
	ValueKind want, HostValue &out, std::string &storage )
{
	out = HostValue{};
	switch ( value.kind ) {
	case format::ValueKind::Text:
	case format::ValueKind::Name:
		out.kind = ValueKind::Text;
		out.text = value.text;
		return true;
	case format::ValueKind::Number:
		out.kind = ValueKind::Number;
		out.numbers[0] = value.numbers[0];
		return true;
	case format::ValueKind::Vector:
		out.kind = ValueKind::Vector;
		for ( int i = 0; i < 3; ++i ) out.numbers[i] = value.numbers[i];
		return true;
	case format::ValueKind::Range: {
		/* A range is asked for a number inside it. The generator is the
		 * engine's own and is seeded per level, so a probe repeats. */
		out.kind = ValueKind::Number;
		seed_ = seed_ * 1103515245u + 12345u;
		const float span = value.numbers[1] - value.numbers[0];
		out.numbers[0] = value.numbers[0]
			+ span * ( float( ( seed_ >> 16 ) & 0x7fffu ) / 32767.0f );
		return true;
	}
	case format::ValueKind::Typed: {
		/* A get: a declared variable first, then the game. */
		const ValueKind kind = value.type == format::MemberText ? ValueKind::Text
			: value.type == format::MemberVector ? ValueKind::Vector : ValueKind::Number;
		if ( GetVariable( value.text, kind, out ) ) {
			if ( kind == ValueKind::Text ) { storage = out.text; out.text = storage.c_str(); }
			return true;
		}
		if ( !host_->Query( sequencer.Entity(), kind, value.text, out ) ) return false;
		out.kind = kind;
		return true;
	}
	case format::ValueKind::Tagged:
		/* A tag asks the game where something is. Nothing serves it yet, so
		 * the script stops here by name rather than reading a zero. */
		return false;
	}
	(void)want;
	return false;
}

inline bool Interpreter::Compare( const HostValue &left, std::int32_t op,
	const HostValue &right, bool &result ) const
{
	const bool equals = op == format::Number( format::Token::TK_EQUALS );
	const bool differs = op == format::Number( format::Token::TK_NOT );
	if ( left.kind == ValueKind::Text || right.kind == ValueKind::Text ) {
		if ( left.kind != right.kind ) return false;
		const int order = std::strcmp( left.text ? left.text : "", right.text ? right.text : "" );
		if ( equals ) { result = order == 0; return true; }
		if ( differs ) { result = order != 0; return true; }
		/* Ordering two pieces of text is not something a retail script does,
		 * and nothing here decides what it would mean. */
		return false;
	}
	const float a = left.numbers[0], b = right.numbers[0];
	if ( equals ) { result = a == b; return true; }
	if ( differs ) { result = a != b; return true; }
	if ( op == format::Number( format::Token::TK_GREATER_THAN ) ) { result = a > b; return true; }
	if ( op == format::Number( format::Token::TK_LESS_THAN ) ) { result = a < b; return true; }
	return false;
}

inline Interpreter::Step Interpreter::Dispatch( Sequencer &sequencer,
	const Statement &statement, const Value *values, int count )
{
	using Frame = Sequencer::Frame;
	Frame &frame = sequencer.frames_.back();
	const Script &script = *frame.script;
	const int here = frame.index;
	const std::int32_t block = statement.block;
	const auto id = []( BlockId which ) { return format::Number( which ); };

	/* Push a body of `opener` onto `target`, which may be another entity's. */
	const auto pushBody = [&]( Sequencer &target, int opener, Sequencer::FrameKind kind,
		int remaining, const std::string &group ) {
		Frame body;
		body.script = &script;
		body.index = opener + 1;
		body.end = script.BodyEnd( opener ) - 1;   /* the close itself is not run */
		body.kind = kind;
		body.loopStart = opener + 1;
		body.remaining = remaining;
		body.group = group;
		target.frames_.push_back( body );
		if ( target.state_ == RunState::Idle ) target.state_ = RunState::Running;
	};

	std::string storage[3];
	HostValue resolved[3];
	const int resolvable = count < 3 ? count : 3;
	for ( int i = 0; i < resolvable; ++i ) {
		if ( !Resolve( sequencer, values[i], ValueKind::Text, resolved[i], storage[i] ) ) {
			Fail( sequencer, std::string( "ICARUS " ) + script.Name() + ": "
				+ format::BlockName( block ) + " cannot read its value" );
			return Step::Failed;
		}
	}

	/* --- flow the engine owns ------------------------------------------- */

	if ( block == id( BlockId::ID_AFFECT ) ) {
		if ( count < 2 || resolved[0].kind != ValueKind::Text ) return Step::Failed;
		const EntityId target = host_->FindEntity( resolved[0].text );
		if ( target == NoEntity ) {
			Fail( sequencer, std::string( "ICARUS " ) + script.Name()
				+ ": affect names no entity: " + resolved[0].text );
			return Step::Failed;
		}
		const bool flush = int( resolved[1].numbers[0] )
			== format::Number( format::Type::TYPE_FLUSH );
		Sequencer &other = Bind( target );
		if ( flush ) other.frames_.clear();
		const std::string group = frame.group;
		frame.index = script.BodyEnd( here );
		pushBody( other, here, Sequencer::FrameKind::Body, 0, group );
		return &other == &sequencer ? Step::Continue : Step::Continue;
	}

	if ( block == id( BlockId::ID_IF ) ) {
		if ( count < 2 || statement.compare == 0 ) return Step::Failed;
		bool taken = false;
		if ( !Compare( resolved[0], statement.compare, resolved[1], taken ) ) {
			Fail( sequencer, std::string( "ICARUS " ) + script.Name()
				+ ": if compares two values it cannot compare" );
			return Step::Failed;
		}
		const int after = script.BodyEnd( here );
		frame.index = after;
		if ( taken ) {
			frame.skipElseAt = after;
			pushBody( sequencer, here, Sequencer::FrameKind::Body, 0, frame.group );
		}
		return Step::Continue;
	}

	if ( block == id( BlockId::ID_ELSE ) ) {
		frame.index = script.BodyEnd( here );
		pushBody( sequencer, here, Sequencer::FrameKind::Body, 0, frame.group );
		return Step::Continue;
	}

	if ( block == id( BlockId::ID_LOOP ) ) {
		if ( count < 1 ) return Step::Failed;
		const int iterations = int( resolved[0].numbers[0] );
		frame.index = script.BodyEnd( here );
		if ( iterations == 0 ) return Step::Continue;
		pushBody( sequencer, here, Sequencer::FrameKind::Loop,
			iterations < 0 ? -1 : iterations - 1, frame.group );
		return Step::Continue;
	}

	if ( block == id( BlockId::ID_TASK ) ) {
		if ( count < 1 || resolved[0].kind != ValueKind::Text ) return Step::Failed;
		Sequencer::TaskBody body;
		body.script = &script;
		body.begin = here + 1;
		body.end = script.BodyEnd( here ) - 1;
		sequencer.tasks_[resolved[0].text] = body;
		frame.index = script.BodyEnd( here );
		return Step::Continue;
	}

	if ( block == id( BlockId::ID_DO ) ) {
		if ( count < 1 || resolved[0].kind != ValueKind::Text ) return Step::Failed;
		const std::string name = resolved[0].text;
		const auto found = sequencer.tasks_.find( name );
		if ( found == sequencer.tasks_.end() ) {
			Fail( sequencer, std::string( "ICARUS " ) + script.Name()
				+ ": do names no task: " + name );
			return Step::Failed;
		}
		++frame.index;
		Frame body;
		body.script = found->second.script;
		body.index = found->second.begin;
		body.end = found->second.end;
		body.kind = Sequencer::FrameKind::Task;
		body.group = name;
		sequencer.taskStarted_.insert( name );
		sequencer.taskOutstanding_[name] += 0;
		sequencer.frames_.push_back( body );
		return Step::Continue;
	}

	if ( block == id( BlockId::ID_WAIT ) ) {
		if ( count < 1 ) return Step::Failed;
		++frame.index;
		if ( resolved[0].kind == ValueKind::Number ) {
			sequencer.wakeAt_ = host_->Milliseconds() + int( resolved[0].numbers[0] );
			sequencer.state_ = RunState::WaitingTime;
			return Step::Blocked;
		}
		sequencer.waitingFor_ = resolved[0].text;
		sequencer.state_ = RunState::WaitingTask;
		return Step::Blocked;
	}

	if ( block == id( BlockId::ID_WAITSIGNAL ) ) {
		if ( count < 1 || resolved[0].kind != ValueKind::Text ) return Step::Failed;
		++frame.index;
		sequencer.waitingFor_ = resolved[0].text;
		sequencer.state_ = RunState::WaitingSignal;
		return Step::Blocked;
	}

	if ( block == id( BlockId::ID_SIGNAL ) ) {
		if ( count < 1 || resolved[0].kind != ValueKind::Text ) return Step::Failed;
		Signal( resolved[0].text );
		++frame.index;
		return Step::Continue;
	}

	if ( block == id( BlockId::ID_DECLARE ) ) {
		if ( count < 2 || resolved[1].kind != ValueKind::Text ) return Step::Failed;
		const int type = int( resolved[0].numbers[0] );
		Declare( resolved[1].text, type == format::MemberText ? ValueKind::Text
			: type == format::MemberVector ? ValueKind::Vector : ValueKind::Number );
		++frame.index;
		return Step::Continue;
	}

	if ( block == id( BlockId::ID_FREE ) ) {
		if ( count < 1 || resolved[0].kind != ValueKind::Text ) return Step::Failed;
		Free( resolved[0].text );
		++frame.index;
		return Step::Continue;
	}

	if ( block == id( BlockId::ID_PRINT ) ) {
		if ( count < 1 || resolved[0].kind != ValueKind::Text ) return Step::Failed;
		host_->Print( resolved[0].text );
		++frame.index;
		return Step::Continue;
	}

	if ( block == id( BlockId::ID_REM ) ) { ++frame.index; return Step::Continue; }

	if ( block == id( BlockId::ID_FLUSH ) ) {
		/* Stop what this entity is doing: its pending work goes, and the
		 * commands the game still owes it stop counting against a wait. */
		sequencer.frames_.clear();
		sequencer.taskOutstanding_.clear();
		sequencer.state_ = RunState::Idle;
		return Step::Finished;
	}

	if ( block == id( BlockId::ID_RUN ) ) {
		if ( count < 1 || resolved[0].kind != ValueKind::Text ) return Step::Failed;
		const std::string name = resolved[0].text;
		++frame.index;
		LoadError error = LoadError::None;
		const Script *next = cache_.Acquire( *host_, name, error );
		if ( !next ) {
			Fail( sequencer, std::string( "ICARUS " ) + script.Name() + ": run "
				+ name + ": " + LoadErrorName( error ) );
			return Step::Failed;
		}
		Frame body;
		body.script = next;
		body.index = 0;
		body.end = next->StatementCount();
		body.group = sequencer.frames_.back().group;
		sequencer.frames_.push_back( body );
		return Step::Continue;
	}

	if ( block == id( BlockId::ID_SET ) ) {
		/* A declared variable is the engine's; anything else is the game's.
		 * That is the split a retail installation's own scripts show: their
		 * SET_ names are the game's properties and the lowercase ones are
		 * variables a script declared for itself. */
		if ( count < 2 || resolved[0].kind != ValueKind::Text ) return Step::Failed;
		if ( Declared( resolved[0].text ) ) {
			if ( !SetVariable( resolved[0].text, resolved[1] ) ) {
				Fail( sequencer, std::string( "ICARUS " ) + script.Name() + ": set "
					+ resolved[0].text + " with the wrong kind of value" );
				return Step::Failed;
			}
			++frame.index;
			return Step::Continue;
		}
		Request request;
		request.command = Command::Set;
		request.entity = sequencer.Entity();
		request.taskId = IssueTask( sequencer );
		request.first = resolved[0].text;
		std::string rendered;
		if ( resolved[1].kind == ValueKind::Text ) request.second = resolved[1].text;
		else {
			rendered = std::to_string( resolved[1].numbers[0] );
			request.second = rendered.c_str();
		}
		if ( !host_->Dispatch( request ) ) {
			Fail( sequencer, std::string( "ICARUS " ) + script.Name()
				+ ": the game refused set " + resolved[0].text );
			return Step::Failed;
		}
		++frame.index;
		return Step::Continue;
	}

	/* --- commands the game carries out ---------------------------------- */

	struct Simple { BlockId block; Command command; int values; bool task; };
	static const Simple simple[] = {
		{ BlockId::ID_SOUND, Command::PlaySound, 2, true },
		{ BlockId::ID_USE, Command::Use, 1, false },
		{ BlockId::ID_KILL, Command::Kill, 1, false },
		{ BlockId::ID_REMOVE, Command::Remove, 1, false },
		{ BlockId::ID_PLAY, Command::Play, 2, true },
	};
	for ( const Simple &entry : simple ) {
		if ( block != id( entry.block ) ) continue;
		if ( count < entry.values ) return Step::Failed;
		Request request;
		request.command = entry.command;
		request.entity = sequencer.Entity();
		request.first = resolved[0].kind == ValueKind::Text ? resolved[0].text : nullptr;
		if ( entry.values > 1 ) {
			request.second = resolved[1].kind == ValueKind::Text ? resolved[1].text : nullptr;
		}
		if ( !request.first || ( entry.values > 1 && !request.second ) ) return Step::Failed;
		if ( entry.task ) request.taskId = IssueTask( sequencer );
		if ( !host_->Dispatch( request ) ) {
			Fail( sequencer, std::string( "ICARUS " ) + script.Name() + ": the game refused "
				+ CommandName( entry.command ) );
			return Step::Failed;
		}
		++frame.index;
		return Step::Continue;
	}

	if ( block == id( BlockId::ID_MOVE ) || block == id( BlockId::ID_ROTATE ) ) {
		/* The shape a retail script uses most: somewhere to go and how long to
		 * take. Anything else this does not read stops by name below. */
		const bool angles = block == id( BlockId::ID_ROTATE );
		if ( count == 2 && resolved[0].kind == ValueKind::Vector
			&& resolved[1].kind == ValueKind::Number ) {
			Request request;
			request.command = angles ? Command::MoveAngles : Command::MoveOrigin;
			request.entity = sequencer.Entity();
			request.taskId = IssueTask( sequencer );
			request.duration = resolved[1].numbers[0];
			for ( int i = 0; i < 3; ++i ) {
				( angles ? request.angles : request.origin )[i] = resolved[0].numbers[i];
			}
			request.hasAngles = angles;
			if ( !host_->Dispatch( request ) ) {
				Fail( sequencer, std::string( "ICARUS " ) + script.Name()
					+ ": the game refused " + CommandName( request.command ) );
				return Step::Failed;
			}
			++frame.index;
			return Step::Continue;
		}
	}

	Fail( sequencer, std::string( "ICARUS " ) + script.Name() + ": "
		+ ( format::BlockName( block ) ? format::BlockName( block ) : "an unnamed block" )
		+ " is not served yet, with " + std::to_string( count ) + " values" );
	return Step::Failed;
}

inline Interpreter::Step Interpreter::RunOne( Sequencer &sequencer )
{
	using Frame = Sequencer::Frame;
	if ( sequencer.frames_.empty() ) {
		sequencer.state_ = RunState::Idle;
		return Step::Finished;
	}
	Frame &frame = sequencer.frames_.back();

	if ( frame.index >= frame.end ) {
		if ( frame.kind == Sequencer::FrameKind::Loop && frame.remaining != 0 ) {
			if ( frame.remaining > 0 ) --frame.remaining;
			frame.index = frame.loopStart;
			return Step::Continue;
		}
		sequencer.frames_.pop_back();
		return sequencer.frames_.empty() ? Step::Finished : Step::Continue;
	}

	/* An else belonging to an if that was taken is passed over. */
	if ( frame.skipElseAt == frame.index ) {
		frame.skipElseAt = -1;
		const Statement &next = frame.script->At( frame.index );
		if ( next.block == format::Number( BlockId::ID_ELSE ) ) {
			frame.index = frame.script->BodyEnd( frame.index );
			return Step::Continue;
		}
	}

	const Statement &statement = frame.script->At( frame.index );
	if ( ClosesBody( statement.block ) ) {
		Fail( sequencer, "ICARUS reached the end of a body it never entered" );
		return Step::Failed;
	}
	return Dispatch( sequencer, statement, frame.script->Values( statement ), statement.values );
}

inline void Interpreter::Tick()
{
	if ( !initialized_ ) return;
	const int now = host_->Milliseconds();

	/* An affect can give another entity a sequencer while this runs, which
	 * would invalidate an iterator over the map, so the tick works from the
	 * entities that existed when it started. One created now runs next tick. */
	std::vector<EntityId> running;
	running.reserve( sequencers_.size() );
	for ( const auto &entry : sequencers_ ) running.push_back( entry.first );

	for ( const EntityId entity : running ) {
		Sequencer *found = Find( entity );
		if ( found ) Advance( *found, now );
	}
}

inline void Interpreter::TickEntity( EntityId entity )
{
	if ( !initialized_ ) return;
	Sequencer *found = Find( entity );
	if ( found ) Advance( *found, host_->Milliseconds() );
}

inline void Interpreter::Advance( Sequencer &sequencer, int now )
{
	/* Whatever this sequencer was waiting for, see whether it has happened. */
	if ( sequencer.state_ == RunState::WaitingTime ) {
		if ( now < sequencer.wakeAt_ ) return;
		sequencer.state_ = RunState::Running;
	} else if ( sequencer.state_ == RunState::WaitingTask ) {
		if ( !sequencer.taskStarted_.count( sequencer.waitingFor_ ) ) {
			Fail( sequencer, "ICARUS waits on a task that was never started: "
				+ sequencer.waitingFor_ );
			return;
		}
		const auto counted = sequencer.taskOutstanding_.find( sequencer.waitingFor_ );
		if ( counted != sequencer.taskOutstanding_.end() && counted->second > 0 ) return;
		sequencer.state_ = RunState::Running;
	} else if ( sequencer.state_ == RunState::WaitingSignal ) {
		if ( !SignalRaised( sequencer.waitingFor_ ) ) return;
		signals_.erase( sequencer.waitingFor_ );
		sequencer.state_ = RunState::Running;
	} else if ( sequencer.state_ != RunState::Running ) {
		return;
	}

	/* A script that never blocks must still not hold the frame forever, so a
	 * run is bounded and one that spins is stopped by name. */
	int budget = 100000;
	while ( sequencer.state_ == RunState::Running && budget-- > 0 ) {
		const Step step = RunOne( sequencer );
		if ( step == Step::Continue ) continue;
		if ( step == Step::Finished ) { sequencer.state_ = RunState::Idle; break; }
		break;   /* blocked or failed; both set their own state */
	}
	if ( budget <= 0 && sequencer.state_ == RunState::Running ) {
		Fail( sequencer, "ICARUS script ran without pausing for too long" );
	}
}

} // namespace icarus
} // namespace fntech3

#endif
