/*
===========================================================================
Copyright (C) 2026 fnTech3 contributors

This file is part of fnTech3 and is distributed under the terms of the GNU
General Public License, version 2 or (at your option) any later version.
===========================================================================
*/

#ifndef FNTECH3_SERVER_GAME_DATA_H
#define FNTECH3_SERVER_GAME_DATA_H

#include "../qcommon/game_profile.h"
#include "../qcommon/q_shared.h"
#include "../compat/rtcw_game_abi.h"

#include <cstddef>
#include <cstdint>

namespace fntech3::server {

/* Resolve an original syscall offset, before the VM's ordinary masking.
 * The caller retains the complete remaining span for record-array checks. */
bool ResolveQvmGameDataPointer( void *dataBase, std::size_t dataSpanBytes,
	std::intptr_t offset, void *&pointer, std::size_t &remainingSpanBytes );

/*
 * This boundary accepts host pointers from native modules only.  QVM offsets
 * must first be translated and range-checked against the VM data segment by
 * the server integration layer; guessing a span from a QVM offset is unsafe.
 */
struct NativeGameDataBinding {
	void *entityBase = nullptr;
	std::size_t entitySpanBytes = 0;
	std::int32_t entityCount = 0;
	std::size_t entityStride = 0;
	void *clientBase = nullptr;
	std::size_t clientSpanBytes = 0;
	std::int32_t clientCount = 0;
	std::size_t clientStride = 0;
	// SP restores and links saved entities before publishing its new high-water
	// count. A nonzero capacity explicitly binds that larger, checked arena.
	std::int32_t entityCapacity = 0;
};

enum class GameDataBindError {
	None = 0,
	UnsupportedProfile,
	InvalidEntityCount,
	NullEntityBase,
	MisalignedEntityBase,
	EntityStrideTooSmall,
	MisalignedEntityStride,
	EntitySpanOverflow,
	EntitySpanTooSmall,
	EntityAddressOverflow,
	InvalidClientCount,
	NullClientBase,
	MisalignedClientBase,
	ClientStrideTooSmall,
	MisalignedClientStride,
	ClientSpanOverflow,
	ClientSpanTooSmall,
	ClientAddressOverflow
};

const char *GameDataBindErrorName( GameDataBindError error );

class ServerGameDataView;

/* References are invalidated by every bind attempt and by Invalidate(). */
struct GameEntityRef {
	const ServerGameDataView *owner = nullptr;
	std::uint64_t generation = 0;
	std::int32_t index = -1;
};

class ServerGameDataView final {
public:
	ServerGameDataView() = default;
	ServerGameDataView( const ServerGameDataView & ) = delete;
	ServerGameDataView &operator=( const ServerGameDataView & ) = delete;

	GameDataBindError BindNative( comGameId_t profile,
		const NativeGameDataBinding &binding );
	void Invalidate();

	bool IsBound() const;
	comGameId_t Profile() const;
	std::uint64_t Generation() const;
	std::int32_t EntityCount() const;
	std::int32_t ClientCount() const;
	std::size_t EntityStride() const;
	std::size_t ClientStride() const;

	static std::size_t EntityPrefixBytesForProfile( comGameId_t profile );
	static std::size_t PlayerStateBytesForProfile( comGameId_t profile );

	bool EntityRefAt( std::int32_t index, GameEntityRef &destination ) const;
	bool EntityRefForPointer( const void *pointer,
		GameEntityRef &destination ) const;
	bool IsCurrent( const GameEntityRef &reference ) const;
	void *EntityPointer( const GameEntityRef &reference );
	const void *EntityPointer( const GameEntityRef &reference ) const;
	void *ClientPointer( std::int32_t index );
	const void *ClientPointer( std::int32_t index ) const;

	bool ReadEntityState( const GameEntityRef &reference,
		entityState_t &destination ) const;
	bool WriteEntityState( const GameEntityRef &reference,
		const entityState_t &source );
	/* Frozen RTCW transfers retain all fields needed by its message codecs.
	 * Profile mismatches fail without publishing bytes. Canonical Q3/QL
	 * overloads cannot silently interpret a bound RTCW record as QL. */
	bool ReadEntityState( const GameEntityRef &reference,
		fntech3_rtcw_entityState_t &destination ) const;
	bool WriteEntityState( const GameEntityRef &reference,
		const fntech3_rtcw_entityState_t &source );
	bool GetNumber( const GameEntityRef &reference,
		std::int32_t &destination ) const;
	bool SetNumber( const GameEntityRef &reference, std::int32_t value );

	bool GetLinked( const GameEntityRef &reference,
		std::int32_t &destination ) const;
	bool SetLinked( const GameEntityRef &reference, std::int32_t value );
	bool GetLinkcount( const GameEntityRef &reference,
		std::int32_t &destination ) const;
	bool SetLinkcount( const GameEntityRef &reference, std::int32_t value );
	bool GetSvFlags( const GameEntityRef &reference,
		std::int32_t &destination ) const;
	bool SetSvFlags( const GameEntityRef &reference, std::int32_t value );
	bool GetWolfETSnapshotCallback( const GameEntityRef &reference,
		std::int32_t &destination ) const;
	bool GetSingleClient( const GameEntityRef &reference,
		std::int32_t &destination ) const;
	bool SetSingleClient( const GameEntityRef &reference, std::int32_t value );
	bool GetBmodel( const GameEntityRef &reference,
		std::int32_t &destination ) const;
	bool SetBmodel( const GameEntityRef &reference, std::int32_t value );
	bool GetContents( const GameEntityRef &reference,
		std::int32_t &destination ) const;
	bool SetContents( const GameEntityRef &reference, std::int32_t value );
	bool GetOwnerNum( const GameEntityRef &reference,
		std::int32_t &destination ) const;
	bool SetOwnerNum( const GameEntityRef &reference, std::int32_t value );

	bool GetMins( const GameEntityRef &reference, float destination[3] ) const;
	bool SetMins( const GameEntityRef &reference, const float value[3] );
	bool GetMaxs( const GameEntityRef &reference, float destination[3] ) const;
	bool SetMaxs( const GameEntityRef &reference, const float value[3] );
	bool GetAbsmin( const GameEntityRef &reference, float destination[3] ) const;
	bool SetAbsmin( const GameEntityRef &reference, const float value[3] );
	bool GetAbsmax( const GameEntityRef &reference, float destination[3] ) const;
	bool SetAbsmax( const GameEntityRef &reference, const float value[3] );
	bool GetCurrentOrigin( const GameEntityRef &reference,
		float destination[3] ) const;
	bool SetCurrentOrigin( const GameEntityRef &reference,
		const float value[3] );
	bool GetCurrentAngles( const GameEntityRef &reference,
		float destination[3] ) const;
	bool SetCurrentAngles( const GameEntityRef &reference,
		const float value[3] );
	bool GetSharedStateOrigin2( const GameEntityRef &reference,
		float destination[3] ) const;
	bool SetSharedStateOrigin2( const GameEntityRef &reference,
		const float value[3] );

	bool ReadPlayerState( std::int32_t clientIndex,
		playerState_t &destination ) const;
	bool ReadPlayerState( std::int32_t clientIndex,
		fntech3_rtcw_sp_playerState_t &destination ) const;
	bool ReadPlayerState( std::int32_t clientIndex,
		fntech3_rtcw_mp_playerState_t &destination ) const;
	bool GetPlayerPing( std::int32_t clientIndex,
		std::int32_t &destination ) const;
	bool SetPlayerPing( std::int32_t clientIndex, std::int32_t value );

private:
	struct Layout;
	static const Layout *FindLayout( comGameId_t profile );

	void AdvanceGeneration();
	void ClearBinding();
	bool ReadEntityBytes( const GameEntityRef &reference, std::size_t offset,
		void *destination, std::size_t byteCount ) const;
	bool WriteEntityBytes( const GameEntityRef &reference, std::size_t offset,
		const void *source, std::size_t byteCount );
	bool ReadClientBytes( std::int32_t clientIndex, std::size_t offset,
		void *destination, std::size_t byteCount ) const;
	bool WriteClientBytes( std::int32_t clientIndex, std::size_t offset,
		const void *source, std::size_t byteCount );

	const Layout *layout_ = nullptr;
	unsigned char *entityBase_ = nullptr;
	std::size_t entitySpanBytes_ = 0;
	std::int32_t entityCount_ = 0;
	std::int32_t entityCapacity_ = 0;
	std::size_t entityStride_ = 0;
	unsigned char *clientBase_ = nullptr;
	std::size_t clientSpanBytes_ = 0;
	std::int32_t clientCount_ = 0;
	std::size_t clientStride_ = 0;
	std::uint64_t generation_ = 0;
};

} // namespace fntech3::server

#endif /* FNTECH3_SERVER_GAME_DATA_H */
