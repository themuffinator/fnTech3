/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_ICARUS_HOST_H
#define FNTECH3_ICARUS_HOST_H

/* What an ICARUS interpreter needs from the engine around it, and what it
 * hands back to the game module.
 *
 * Raven split this boundary already, and the split is visible in the retail
 * module's own export table: the engine sequences a script and owns its flow,
 * its variables and its timing, and every command that touches an entity is
 * handed to the game through an export of its own. This interface is that
 * split, written as the interpreter's view of it.
 *
 * Nothing here knows about a game module. The server implements this against
 * the retail module; a test implements it against nothing at all, which is how
 * the interpreter is exercised without one.
 *
 * See docs/fntech3/2026-09-21-jka-icarus.md. */

#include <cstddef>
#include <cstdint>

namespace fntech3 {
namespace icarus {

/* A value handed to or taken from the game, in the three kinds a script has. */
enum class ValueKind { Text, Number, Vector };

struct HostValue {
	ValueKind kind = ValueKind::Text;
	const char *text = nullptr;
	float numbers[3] = { 0.0f, 0.0f, 0.0f };
};

/* An entity, as the interpreter refers to one. The engine decides what the
 * number means; the interpreter only passes it back. -1 is no entity. */
using EntityId = std::int32_t;
inline constexpr EntityId NoEntity = -1;

/* A command the interpreter hands to the game. The numbers are the interpreter's
 * own; the server maps them to the module's exports. */
enum class Command {
	PlaySound,      /* channel, file */
	Set,            /* name, value */
	Move,           /* origin, angles, duration */
	MoveOrigin,     /* origin, duration */
	MoveAngles,     /* angles, duration */
	Use,            /* target */
	Kill,           /* name */
	Remove,         /* name */
	Play,           /* type, name */
	LerpToStart,    /* duration */
	LerpToEnd,      /* duration */
};

inline const char *CommandName( Command command ) {
	switch ( command ) {
	case Command::PlaySound: return "PlaySound";
	case Command::Set: return "Set";
	case Command::Move: return "Move";
	case Command::MoveOrigin: return "MoveOrigin";
	case Command::MoveAngles: return "MoveAngles";
	case Command::Use: return "Use";
	case Command::Kill: return "Kill";
	case Command::Remove: return "Remove";
	case Command::Play: return "Play";
	case Command::LerpToStart: return "LerpToStart";
	case Command::LerpToEnd: return "LerpToEnd";
	}
	return "unknown";
}

/* One command, with everything the game needs to carry it out. A command that
 * takes a task identifier completes later, and the game says when by calling
 * back with that identifier; one that takes none is finished when it returns. */
struct Request {
	Command command = Command::Set;
	EntityId entity = NoEntity;
	std::int32_t taskId = -1;        /* -1 when the command completes at once. */
	const char *first = nullptr;     /* name, channel, target or type. */
	const char *second = nullptr;    /* value or file. */
	float origin[3] = { 0.0f, 0.0f, 0.0f };
	float angles[3] = { 0.0f, 0.0f, 0.0f };
	bool hasAngles = false;
	float duration = 0.0f;
};

/* Where a script came from and how it reads: the interpreter never opens a
 * file itself, so an installation, a pak or a test's own bytes all look the
 * same to it. */
class Host {
public:
	virtual ~Host() = default;

	/* Read a whole script by its ICARUS name -- no directory, no extension --
	 * into the caller's buffer. Return its size, or 0 when there is no such
	 * script. A size larger than `capacity` means the script does not fit and
	 * nothing was written. */
	virtual std::size_t ReadScript( const char *name, void *destination,
		std::size_t capacity ) = 0;

	/* Milliseconds, on whatever clock the engine runs its level on. */
	virtual int Milliseconds() = 0;

	/* Find an entity by the name a script uses for it, or NoEntity. */
	virtual EntityId FindEntity( const char *name ) = 0;

	/* Hand a command to the game. Return false when the game will not do it,
	 * which stops the script rather than letting it run on regardless. */
	virtual bool Dispatch( const Request &request ) = 0;

	/* Ask the game for one of an entity's values. Return false when the game
	 * has no such value, which is a script error rather than a zero. */
	virtual bool Query( EntityId entity, ValueKind kind, const char *name,
		HostValue &value ) = 0;

	/* Say something on the console, for a script's own print. */
	virtual void Print( const char *text ) = 0;

	/* Report a script that cannot continue. The interpreter stops that
	 * sequencer; whether the engine stops with it is the engine's business. */
	virtual void Fail( const char *reason ) = 0;
};

} // namespace icarus
} // namespace fntech3

#endif
