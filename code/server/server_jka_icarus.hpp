/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_SERVER_JKA_ICARUS_HPP
#define FNTECH3_SERVER_JKA_ICARUS_HPP

/* The engine's side of Jedi Academy's ICARUS boundary.
 *
 * The interpreter in code/icarus knows nothing about a game module; this is
 * what gives it one. It reads scripts out of the filesystem, keeps the level
 * clock, answers for entity names the game has associated, and hands every
 * command that touches an entity back to the module -- written into the buffer
 * the module lent the engine at startup, with the export called empty-handed,
 * which is how Raven's own engine passes them.
 *
 * See docs/fntech3/2026-09-22-jka-icarus-runtime.md. */

#include "../compat/jka_icarus_callbacks.h"
#include "server_raven_assets.hpp"
#include "../icarus/icarus_runtime.hpp"

#include <map>
#include <string>

namespace {

/* Raven's modules lend the engine one scratch buffer and pass the bulky
 * arguments of certain calls through it instead of the call itself. Every one
 * of those calls is an ICARUS command handed back to the game, which is why
 * the buffer lives here. It is a borrowed host pointer, dropped the moment the
 * module that lent it goes away. */
void *jkaSharedBuffer = nullptr;

/* One name a script may address, and the entity the game gave it. */
std::map<std::string, fntech3::icarus::EntityId> jkaIcarusNames;

class ServerIcarusHost final : public fntech3::icarus::Host {
public:
	std::size_t ReadScript( const char *name, void *destination,
		std::size_t capacity ) override
	{
		/* A script names itself without its directory or its extension, which
		 * is the convention its own play commands use for motion paths too. */
		char path[MAX_QPATH];
		Com_sprintf( path, sizeof( path ), "scripts/%s.ibi", name );
		void *bytes = nullptr;
		const int length = FS_ReadFile( path, &bytes );
		if ( length <= 0 || !bytes ) {
			if ( bytes ) FS_FreeFile( bytes );
			return 0;
		}
		const std::size_t size = std::size_t( length );
		if ( destination && capacity >= size ) std::memcpy( destination, bytes, size );
		FS_FreeFile( bytes );
		return size;
	}

	int Milliseconds() override { return sv.time; }

	fntech3::icarus::EntityId FindEntity( const char *name ) override
	{
		const auto found = jkaIcarusNames.find( name );
		return found == jkaIcarusNames.end() ? fntech3::icarus::NoEntity : found->second;
	}

	bool Dispatch( const fntech3::icarus::Request &request ) override;
	bool Query( fntech3::icarus::EntityId entity, fntech3::icarus::ValueKind kind,
		const char *name, fntech3::icarus::HostValue &value ) override;

	void Print( const char *text ) override { Com_Printf( "%s", text ); }

	void Fail( const char *reason ) override
	{
		/* A script that cannot go on stops the server rather than running half
		 * of itself. This is a developer build; a quiet half-run script is the
		 * failure mode this whole boundary exists to avoid. */
		Com_Error( ERR_DROP, "%s", reason );
	}

private:
	/* Write a record into the buffer the module lent us and call its export.
	 * Nothing else is passed: Raven's own engine calls these empty-handed. */
	template<class Record>
	bool Invoke( fntech3::jka::module::Call call, const Record &record, intptr_t *result )
	{
		static_assert( sizeof( Record ) <= FNTECH3_JKA_ICARUS_MAX_CALLBACK_BYTES,
			"an ICARUS callback record must fit the buffer the module lends" );
		if ( !jkaSharedBuffer || !gvm ) return false;
		const auto *descriptor = fntech3::jka::module::Describe( call );
		if ( !descriptor ) return false;
		std::memcpy( jkaSharedBuffer, &record, sizeof( record ) );
		const intptr_t returned = VM_Call( gvm, 0, descriptor->ordinal );
		if ( result ) *result = returned;
		return true;
	}

	/* Read back what the module wrote into the same buffer. */
	template<class Record>
	void Collect( Record &record ) const
	{
		if ( jkaSharedBuffer ) std::memcpy( &record, jkaSharedBuffer, sizeof( record ) );
	}
};

/* Copy text into one of Raven's fixed record fields, always terminated. */
template<std::size_t N>
void JKAIcarusText( char ( &field )[N], const char *text )
{
	Q_strncpyz( field, text ? text : "", int( N ) );
}

inline bool ServerIcarusHost::Dispatch( const fntech3::icarus::Request &request )
{
	using fntech3::icarus::Command;
	using Call = fntech3::jka::module::Call;

	switch ( request.command ) {
	case Command::Set: {
		fntech3_jka_icarus_set_t record{};
		record.taskID = request.taskId;
		record.entID = request.entity;
		JKAIcarusText( record.type_name, request.first );
		JKAIcarusText( record.data, request.second );
		return Invoke( Call::GAME_ICARUS_SET, record, nullptr );
	}
	case Command::PlaySound: {
		fntech3_jka_icarus_playsound_t record{};
		record.taskID = request.taskId;
		record.entID = request.entity;
		JKAIcarusText( record.channel, request.first );
		JKAIcarusText( record.name, request.second );
		return Invoke( Call::GAME_ICARUS_PLAYSOUND, record, nullptr );
	}
	case Command::Use: {
		fntech3_jka_icarus_use_t record{};
		record.entID = request.entity;
		JKAIcarusText( record.target, request.first );
		return Invoke( Call::GAME_ICARUS_USE, record, nullptr );
	}
	case Command::Kill: {
		fntech3_jka_icarus_kill_t record{};
		record.entID = request.entity;
		JKAIcarusText( record.name, request.first );
		return Invoke( Call::GAME_ICARUS_KILL, record, nullptr );
	}
	case Command::Remove: {
		fntech3_jka_icarus_remove_t record{};
		record.entID = request.entity;
		JKAIcarusText( record.name, request.first );
		return Invoke( Call::GAME_ICARUS_REMOVE, record, nullptr );
	}
	case Command::Play: {
		fntech3_jka_icarus_play_t record{};
		record.taskID = request.taskId;
		record.entID = request.entity;
		JKAIcarusText( record.type, request.first );
		JKAIcarusText( record.name, request.second );
		return Invoke( Call::GAME_ICARUS_PLAY, record, nullptr );
	}
	case Command::MoveOrigin: {
		fntech3_jka_icarus_lerp2origin_t record{};
		record.taskID = request.taskId;
		record.entID = request.entity;
		for ( int i = 0; i < 3; ++i ) record.origin[i] = request.origin[i];
		record.duration = request.duration;
		return Invoke( Call::GAME_ICARUS_LERP2ORIGIN, record, nullptr );
	}
	case Command::MoveAngles: {
		fntech3_jka_icarus_lerp2angles_t record{};
		record.taskID = request.taskId;
		record.entID = request.entity;
		for ( int i = 0; i < 3; ++i ) record.angles[i] = request.angles[i];
		record.duration = request.duration;
		return Invoke( Call::GAME_ICARUS_LERP2ANGLES, record, nullptr );
	}
	case Command::Move: {
		fntech3_jka_icarus_lerp2pos_t record{};
		record.taskID = request.taskId;
		record.entID = request.entity;
		for ( int i = 0; i < 3; ++i ) record.origin[i] = request.origin[i];
		for ( int i = 0; i < 3; ++i ) record.angles[i] = request.angles[i];
		record.duration = request.duration;
		record.nullAngles = request.hasAngles ? 0 : 1;
		return Invoke( Call::GAME_ICARUS_LERP2POS, record, nullptr );
	}
	case Command::LerpToStart: {
		fntech3_jka_icarus_lerp2start_t record{};
		record.entID = request.entity;
		record.taskID = request.taskId;
		record.duration = request.duration;
		return Invoke( Call::GAME_ICARUS_LERP2START, record, nullptr );
	}
	case Command::LerpToEnd: {
		fntech3_jka_icarus_lerp2end_t record{};
		record.entID = request.entity;
		record.taskID = request.taskId;
		record.duration = request.duration;
		return Invoke( Call::GAME_ICARUS_LERP2END, record, nullptr );
	}
	}
	return false;
}

inline bool ServerIcarusHost::Query( fntech3::icarus::EntityId entity,
	fntech3::icarus::ValueKind kind, const char *name, fntech3::icarus::HostValue &value )
{
	using Call = fntech3::jka::module::Call;
	/* The kind a script asked for is spelled with the same number the member
	 * stream uses for it, which is what the module expects to read back. */
	value = fntech3::icarus::HostValue{};
	value.kind = kind;
	switch ( kind ) {
	case fntech3::icarus::ValueKind::Number: {
		fntech3_jka_icarus_getfloat_t record{};
		record.entID = entity;
		record.type = fntech3::jka::icarus::MemberNumber;
		JKAIcarusText( record.name, name );
		intptr_t ok = 0;
		if ( !Invoke( Call::GAME_ICARUS_GETFLOAT, record, &ok ) || !ok ) return false;
		Collect( record );
		value.numbers[0] = record.value;
		return true;
	}
	case fntech3::icarus::ValueKind::Vector: {
		fntech3_jka_icarus_getvector_t record{};
		record.entID = entity;
		record.type = fntech3::jka::icarus::MemberVector;
		JKAIcarusText( record.name, name );
		intptr_t ok = 0;
		if ( !Invoke( Call::GAME_ICARUS_GETVECTOR, record, &ok ) || !ok ) return false;
		Collect( record );
		for ( int i = 0; i < 3; ++i ) value.numbers[i] = record.value[i];
		return true;
	}
	case fntech3::icarus::ValueKind::Text: {
		fntech3_jka_icarus_getstring_t record{};
		record.entID = entity;
		record.type = fntech3::jka::icarus::MemberText;
		JKAIcarusText( record.name, name );
		intptr_t ok = 0;
		if ( !Invoke( Call::GAME_ICARUS_GETSTRING, record, &ok ) || !ok ) return false;
		Collect( record );
		static char returned[FNTECH3_JKA_ICARUS_STRING_BYTES];
		Q_strncpyz( returned, record.value, sizeof( returned ) );
		value.text = returned;
		return true;
	}
	}
	return false;
}

ServerIcarusHost jkaIcarusHost;
fntech3::icarus::Interpreter jkaIcarus( jkaIcarusHost );

void SV_JKAIcarusClear()
{
	jkaIcarus.Shutdown();
	jkaIcarusNames.clear();
}

} // namespace

#endif
