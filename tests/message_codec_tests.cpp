extern "C" {
#include "q_shared.h"
#include "qcommon.h"
}

#include <array>
#include <cstdarg>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>

#define CHECK( expression ) do { if ( !( expression ) ) return __LINE__; } while ( false )

extern "C" int Test_CatchMessageDrop( void (*callback)( void * ), void *context );
extern "C" void Test_RaiseExpectedMessageDrop( void );

extern "C" void QDECL Com_Printf( const char *, ... ) {
}

extern "C" void QDECL Com_Error( errorParm_t, const char *, ... ) {
	Test_RaiseExpectedMessageDrop();
	std::abort();
}

namespace {

int TestBounds() {
	std::array<byte, 6> guarded{ 0xa5, 0, 0, 0, 0, 0x5a };
	msg_t writer{};
	MSG_InitOOB( &writer, guarded.data() + 1, 4 );
	MSG_WriteLong( &writer, 0x12345678 );
	CHECK( !writer.overflowed );
	CHECK( writer.cursize == 4 );
	MSG_WriteByte( &writer, 1 );
	CHECK( writer.overflowed );
	CHECK( writer.cursize == 4 );
	CHECK( guarded.front() == 0xa5 );
	CHECK( guarded.back() == 0x5a );

	msg_t reader{};
	MSG_InitOOB( &reader, guarded.data() + 1, 4 );
	reader.cursize = 1;
	CHECK( MSG_ReadLong( &reader ) == -1 );
	CHECK( reader.readcount > reader.cursize );
	CHECK( guarded.front() == 0xa5 );
	CHECK( guarded.back() == 0x5a );
	return 0;
}

int TestWireProfileUserCommandHash() {
	static const char percentCommand[] = "say 100% ready";
	CHECK( MSG_HashKeyForWireProfile(
		NETCHAN_WIRE_QL_RETAIL, percentCommand, 32 ) == 140014 );
	CHECK( MSG_HashKeyForWireProfile(
		NETCHAN_WIRE_LEGACY_Q3, percentCommand, 32 ) == 140893 );
	CHECK( MSG_HashKeyForWireProfile(
		NETCHAN_WIRE_IOQ3, percentCommand, 32 ) == 140893 );

	// Protocol 91 follows the signed-byte behavior of retail's Win32 build,
	// independent of the host compiler's default char signedness.
	static const char highBitCommand[] = { '\x80', '\xff', '%', '\0' };
	CHECK( MSG_HashKeyForWireProfile(
		NETCHAN_WIRE_QL_RETAIL, highBitCommand, 32 ) == -10865 );
	CHECK( MSG_HashKeyForWireProfile(
		NETCHAN_WIRE_LEGACY_Q3, highBitCommand, 32 ) == 16544 );

	std::array<byte, 64> storage{};
	usercmd_t baseline{};
	usercmd_t sent{};
	usercmd_t received{};
	sent.serverTime = 158600;
	sent.angles[PITCH] = 41556;
	sent.buttons = BUTTON_ATTACK;
	sent.forwardmove = 127;
	const int retailKey = 0x10203040 ^ MSG_HashKeyForWireProfile(
		NETCHAN_WIRE_QL_RETAIL, percentCommand, 32 );

	msg_t writer{};
	MSG_Init( &writer, storage.data(), static_cast<int>( storage.size() ) );
	CHECK( MSG_WriteDeltaUsercmdKeyForWireProfile( &writer, retailKey,
		&baseline, &sent, NETCHAN_WIRE_QL_RETAIL ) );
	CHECK( !writer.overflowed );

	msg_t reader{};
	MSG_Init( &reader, storage.data(), static_cast<int>( storage.size() ) );
	reader.cursize = writer.cursize;
	MSG_BeginReading( &reader );
	CHECK( MSG_ReadDeltaUsercmdKeyForWireProfile( &reader, retailKey,
		&baseline, &received, NETCHAN_WIRE_QL_RETAIL ) );
	CHECK( received.serverTime == sent.serverTime );
	CHECK( received.angles[PITCH] == sent.angles[PITCH] );
	CHECK( received.buttons == sent.buttons );
	CHECK( received.forwardmove == sent.forwardmove );
	return 0;
}

int TestWireProfileCommandStrings() {
	static const char command[] = { 'p', 'r', 'i', 'n', 't', ' ', '"',
		'c', 'a', 'f', '\xc3', '\xa9', ' ', '%', '"', '\0' };
	std::array<byte, 64> storage{};
	msg_t writer{};
	MSG_Init( &writer, storage.data(), static_cast<int>( storage.size() ) );
	MSG_WriteStringForWireProfile(
		&writer, command, NETCHAN_WIRE_QL_RETAIL );
	CHECK( !writer.overflowed );

	msg_t retailReader{};
	MSG_Init( &retailReader, storage.data(), static_cast<int>( storage.size() ) );
	retailReader.cursize = writer.cursize;
	MSG_BeginReading( &retailReader );
	const char *retail = MSG_ReadStringForWireProfile(
		&retailReader, NETCHAN_WIRE_QL_RETAIL );
	CHECK( static_cast<unsigned char>( retail[10] ) == 0xc3 );
	CHECK( static_cast<unsigned char>( retail[11] ) == 0xa9 );
	CHECK( retail[13] == '.' );
	CHECK( MSG_HashKeyForWireProfile(
		NETCHAN_WIRE_QL_RETAIL, retail, 32 ) == 109177 );

	msg_t commandReader{};
	MSG_Init( &commandReader, storage.data(), static_cast<int>( storage.size() ) );
	commandReader.cursize = writer.cursize;
	MSG_BeginReading( &commandReader );
	const char *wireCommand = MSG_ReadCommandStringForWireProfile(
		&commandReader, NETCHAN_WIRE_QL_RETAIL );
	CHECK( static_cast<unsigned char>( wireCommand[10] ) == 0xc3 );
	CHECK( static_cast<unsigned char>( wireCommand[11] ) == 0xa9 );
	CHECK( wireCommand[13] == '%' );
	CHECK( MSG_HashKeyForWireProfile(
		NETCHAN_WIRE_QL_RETAIL, wireCommand, 32 ) == 107782 );

	msg_t legacyReader{};
	MSG_Init( &legacyReader, storage.data(), static_cast<int>( storage.size() ) );
	legacyReader.cursize = writer.cursize;
	MSG_BeginReading( &legacyReader );
	const char *legacy = MSG_ReadStringForWireProfile(
		&legacyReader, NETCHAN_WIRE_LEGACY_Q3 );
	CHECK( legacy[10] == '.' );
	CHECK( legacy[11] == '.' );
	CHECK( legacy[13] == '.' );
	return 0;
}

int TestUserCommandWireLayouts() {
	std::array<byte, 64> q3Storage{};
	std::array<byte, 64> ioq3Storage{};
	std::array<byte, 64> qlStorage{};
	usercmd_t baseline{};
	usercmd_t sent{};
	baseline.serverTime = 1000;
	sent = baseline;
	sent.weaponPrimary = 7;
	sent.fov = 110;

	msg_t q3Writer{};
	MSG_Init( &q3Writer, q3Storage.data(),
		static_cast<int>( q3Storage.size() ) );
	MSG_RawBitstream( &q3Writer );
	CHECK( MSG_WriteDeltaUsercmdKeyForWireProfile( &q3Writer, 0x12345678,
		&baseline, &sent, NETCHAN_WIRE_LEGACY_Q3 ) );
	CHECK( q3Writer.bit == 10 );
	CHECK( q3Writer.cursize == 2 );
	// Independent no-change vector derived from FnQuake3 msg.c at
	// 7d8c9629cdc7979f09697266470359101ffd9eda: short time delta 0,
	// followed by the Q3 common-fields no-change bit.
	CHECK( q3Storage[0] == 0x01 );
	CHECK( q3Storage[1] == 0x00 );

	msg_t ioq3Writer{};
	MSG_Init( &ioq3Writer, ioq3Storage.data(),
		static_cast<int>( ioq3Storage.size() ) );
	MSG_RawBitstream( &ioq3Writer );
	CHECK( MSG_WriteDeltaUsercmdKeyForWireProfile( &ioq3Writer, 0x12345678,
		&baseline, &sent, NETCHAN_WIRE_IOQ3 ) );
	CHECK( ioq3Writer.bit == q3Writer.bit );
	CHECK( std::memcmp( q3Storage.data(), ioq3Storage.data(),
		static_cast<std::size_t>( q3Writer.cursize ) ) == 0 );

	msg_t qlWriter{};
	MSG_Init( &qlWriter, qlStorage.data(),
		static_cast<int>( qlStorage.size() ) );
	MSG_RawBitstream( &qlWriter );
	CHECK( MSG_WriteDeltaUsercmdKeyForWireProfile( &qlWriter, 0x12345678,
		&baseline, &sent, NETCHAN_WIRE_QL_RETAIL ) );
	CHECK( qlWriter.bit == 36 );
	CHECK( qlWriter.bit > q3Writer.bit );

	usercmd_t q3Base = baseline;
	q3Base.weaponPrimary = 0xa5;
	q3Base.fov = 0x5a;
	usercmd_t q3Received{};
	msg_t q3Reader{};
	MSG_Init( &q3Reader, q3Storage.data(),
		static_cast<int>( q3Storage.size() ) );
	q3Reader.cursize = q3Writer.cursize;
	MSG_BeginReading( &q3Reader );
	MSG_RawBitstream( &q3Reader );
	CHECK( MSG_ReadDeltaUsercmdKeyForWireProfile( &q3Reader, 0x12345678,
		&q3Base, &q3Received, NETCHAN_WIRE_LEGACY_Q3 ) );
	CHECK( q3Reader.bit == q3Writer.bit );
	CHECK( q3Received.serverTime == sent.serverTime );
	CHECK( q3Received.weaponPrimary == 0 );
	CHECK( q3Received.fov == 0 );

	usercmd_t qlReceived{};
	msg_t qlReader{};
	MSG_Init( &qlReader, qlStorage.data(),
		static_cast<int>( qlStorage.size() ) );
	qlReader.cursize = qlWriter.cursize;
	MSG_BeginReading( &qlReader );
	MSG_RawBitstream( &qlReader );
	CHECK( MSG_ReadDeltaUsercmdKeyForWireProfile( &qlReader, 0x12345678,
		&baseline, &qlReceived, NETCHAN_WIRE_QL_RETAIL ) );
	CHECK( qlReader.bit == qlWriter.bit );
	CHECK( qlReceived.weaponPrimary == sent.weaponPrimary );
	CHECK( qlReceived.fov == sent.fov );
	return 0;
}

int TestEntityWireLayouts() {
	std::array<byte, MAX_MSGLEN_BUF> q3Storage{};
	std::array<byte, MAX_MSGLEN_BUF> ioq3Storage{};
	std::array<byte, MAX_MSGLEN_BUF> qlStorage{};
	entityState_t baseline{};
	entityState_t sent{};
	sent.number = 17;
	sent.pos.gravity = 800.0f;
	sent.apos.gravity = -25.0f;
	sent.health = 125;
	sent.armor = 75;
	sent.location = 63;
	sent.jumpTime = 123456;
	sent.doubleJumped = 1;

	msg_t q3Writer{};
	MSG_Init( &q3Writer, q3Storage.data(),
		static_cast<int>( q3Storage.size() ) );
	MSG_RawBitstream( &q3Writer );
	CHECK( MSG_WriteDeltaEntityForWireProfile( &q3Writer, &baseline, &sent,
		qtrue, NETCHAN_WIRE_LEGACY_Q3 ) );
	CHECK( q3Writer.bit == 12 );
	// FnQuake3's forced no-delta entity record is the ten-bit entity number,
	// then the not-removed and no-delta bits.
	CHECK( q3Storage[0] == 0x11 );
	CHECK( q3Storage[1] == 0x00 );

	msg_t ioq3Writer{};
	MSG_Init( &ioq3Writer, ioq3Storage.data(),
		static_cast<int>( ioq3Storage.size() ) );
	MSG_RawBitstream( &ioq3Writer );
	CHECK( MSG_WriteDeltaEntityForWireProfile( &ioq3Writer, &baseline, &sent,
		qtrue, NETCHAN_WIRE_IOQ3 ) );
	CHECK( ioq3Writer.bit == q3Writer.bit );
	CHECK( std::memcmp( q3Storage.data(), ioq3Storage.data(),
		static_cast<std::size_t>( q3Writer.cursize ) ) == 0 );

	msg_t qlWriter{};
	MSG_Init( &qlWriter, qlStorage.data(),
		static_cast<int>( qlStorage.size() ) );
	MSG_RawBitstream( &qlWriter );
	CHECK( MSG_WriteDeltaEntityForWireProfile( &qlWriter, &baseline, &sent,
		qtrue, NETCHAN_WIRE_QL_RETAIL ) );
	CHECK( qlWriter.bit > q3Writer.bit );

	entityState_t q3Received{};
	msg_t q3Reader{};
	MSG_Init( &q3Reader, q3Storage.data(),
		static_cast<int>( q3Storage.size() ) );
	q3Reader.cursize = q3Writer.cursize;
	MSG_BeginReading( &q3Reader );
	MSG_RawBitstream( &q3Reader );
	const int q3Number = MSG_ReadEntitynum( &q3Reader );
	CHECK( q3Number == sent.number );
	CHECK( MSG_ReadDeltaEntityForWireProfile( &q3Reader, &sent, &q3Received,
		q3Number, NETCHAN_WIRE_LEGACY_Q3 ) );
	CHECK( q3Reader.bit == q3Writer.bit );
	CHECK( q3Received.number == sent.number );
	CHECK( q3Received.pos.gravity == 0.0f );
	CHECK( q3Received.apos.gravity == 0.0f );
	CHECK( q3Received.health == 0 );
	CHECK( q3Received.armor == 0 );
	CHECK( q3Received.location == 0 );
	CHECK( q3Received.jumpTime == 0 );
	CHECK( q3Received.doubleJumped == 0 );

	entityState_t qlReceived{};
	msg_t qlReader{};
	MSG_Init( &qlReader, qlStorage.data(),
		static_cast<int>( qlStorage.size() ) );
	qlReader.cursize = qlWriter.cursize;
	MSG_BeginReading( &qlReader );
	MSG_RawBitstream( &qlReader );
	const int qlNumber = MSG_ReadEntitynum( &qlReader );
	CHECK( qlNumber == sent.number );
	CHECK( MSG_ReadDeltaEntityForWireProfile( &qlReader, &baseline,
		&qlReceived, qlNumber, NETCHAN_WIRE_QL_RETAIL ) );
	CHECK( qlReader.bit == qlWriter.bit );
	CHECK( std::memcmp( &sent, &qlReceived, sizeof( sent ) ) == 0 );
	return 0;
}

int TestPlayerStateWireLayouts() {
	std::array<byte, MAX_MSGLEN_BUF> q3Storage{};
	std::array<byte, MAX_MSGLEN_BUF> ioq3Storage{};
	std::array<byte, MAX_MSGLEN_BUF> qlStorage{};
	playerState_t baseline{};
	playerState_t sent{};
	baseline.pm_flags = 0x1234;
	sent = baseline;
	sent.pm_flags = 0x00ab1234;
	sent.location = 14;
	sent.weaponPrimary = 2;
	sent.fov = 103;
	sent.jumpTime = 123456;
	sent.doubleJumped = 1;
	sent.crouchTime = 654321;
	sent.crouchSlideTime = 777;
	sent.forwardmove = -127;
	sent.rightmove = 100;
	sent.upmove = -4;

	msg_t q3Writer{};
	MSG_Init( &q3Writer, q3Storage.data(),
		static_cast<int>( q3Storage.size() ) );
	MSG_RawBitstream( &q3Writer );
	CHECK( MSG_WriteDeltaPlayerstateForWireProfile( &q3Writer, &baseline,
		&sent, NETCHAN_WIRE_LEGACY_Q3 ) );
	CHECK( q3Writer.bit == 9 );
	CHECK( q3Storage[0] == 0x00 );
	CHECK( q3Storage[1] == 0x00 );

	msg_t ioq3Writer{};
	MSG_Init( &ioq3Writer, ioq3Storage.data(),
		static_cast<int>( ioq3Storage.size() ) );
	MSG_RawBitstream( &ioq3Writer );
	CHECK( MSG_WriteDeltaPlayerstateForWireProfile( &ioq3Writer, &baseline,
		&sent, NETCHAN_WIRE_IOQ3 ) );
	CHECK( ioq3Writer.bit == q3Writer.bit );
	CHECK( std::memcmp( q3Storage.data(), ioq3Storage.data(),
		static_cast<std::size_t>( q3Writer.cursize ) ) == 0 );

	msg_t qlWriter{};
	MSG_Init( &qlWriter, qlStorage.data(),
		static_cast<int>( qlStorage.size() ) );
	MSG_RawBitstream( &qlWriter );
	CHECK( MSG_WriteDeltaPlayerstateForWireProfile( &qlWriter, &baseline,
		&sent, NETCHAN_WIRE_QL_RETAIL ) );
	CHECK( qlWriter.bit > q3Writer.bit );

	playerState_t q3Base = sent;
	q3Base.commandMirrorPad = 0x5a;
	q3Base.armorTier = 3;
	playerState_t q3Received{};
	msg_t q3Reader{};
	MSG_Init( &q3Reader, q3Storage.data(),
		static_cast<int>( q3Storage.size() ) );
	q3Reader.cursize = q3Writer.cursize;
	MSG_BeginReading( &q3Reader );
	MSG_RawBitstream( &q3Reader );
	CHECK( MSG_ReadDeltaPlayerstateForWireProfile( &q3Reader, &q3Base,
		&q3Received, NETCHAN_WIRE_LEGACY_Q3 ) );
	CHECK( q3Reader.bit == q3Writer.bit );
	CHECK( q3Received.pm_flags == 0x1234 );
	CHECK( q3Received.location == 0 );
	CHECK( q3Received.weaponPrimary == 0 );
	CHECK( q3Received.fov == 0 );
	CHECK( q3Received.jumpTime == 0 );
	CHECK( q3Received.doubleJumped == 0 );
	CHECK( q3Received.crouchTime == 0 );
	CHECK( q3Received.crouchSlideTime == 0 );
	CHECK( q3Received.forwardmove == 0 );
	CHECK( q3Received.rightmove == 0 );
	CHECK( q3Received.upmove == 0 );
	CHECK( q3Received.commandMirrorPad == 0 );
	CHECK( q3Received.armorTier == 0 );

	playerState_t qlReceived{};
	msg_t qlReader{};
	MSG_Init( &qlReader, qlStorage.data(),
		static_cast<int>( qlStorage.size() ) );
	qlReader.cursize = qlWriter.cursize;
	MSG_BeginReading( &qlReader );
	MSG_RawBitstream( &qlReader );
	CHECK( MSG_ReadDeltaPlayerstateForWireProfile( &qlReader, &baseline,
		&qlReceived, NETCHAN_WIRE_QL_RETAIL ) );
	CHECK( qlReader.bit == qlWriter.bit );
	CHECK( qlReceived.pm_flags == sent.pm_flags );
	CHECK( qlReceived.location == sent.location );
	CHECK( qlReceived.weaponPrimary == sent.weaponPrimary );
	CHECK( qlReceived.fov == sent.fov );
	CHECK( qlReceived.jumpTime == sent.jumpTime );
	CHECK( qlReceived.doubleJumped == sent.doubleJumped );
	CHECK( qlReceived.crouchTime == sent.crouchTime );
	CHECK( qlReceived.crouchSlideTime == sent.crouchSlideTime );
	CHECK( qlReceived.forwardmove == sent.forwardmove );
	CHECK( qlReceived.rightmove == sent.rightmove );
	CHECK( qlReceived.upmove == sent.upmove );
	return 0;
}

int TestPinnedQuake3RawVectors() {
	// These raw-bit vectors were derived field-by-field from the pinned
	// FnQuake3 msg.c table at 7d8c9629cdc7979f09697266470359101ffd9eda,
	// independently of fnTech3's tables. They lock the placement of Q3's
	// event field (index 9) and loopSound player field (index 47).
	static constexpr std::array<byte, 6> entityGolden{
		0x11, 0xa8, 0x00, 0xe0, 0x02, 0x00
	};
	std::array<byte, 64> entityStorage{};
	entityState_t entityBase{};
	entityState_t entitySent{};
	entitySent.number = 17;
	entitySent.event = 5;
	msg_t entityWriter{};
	MSG_Init( &entityWriter, entityStorage.data(),
		static_cast<int>( entityStorage.size() ) );
	MSG_RawBitstream( &entityWriter );
	CHECK( MSG_WriteDeltaEntityForWireProfile( &entityWriter, &entityBase,
		&entitySent, qtrue, NETCHAN_WIRE_LEGACY_Q3 ) );
	CHECK( entityWriter.bit == 41 );
	CHECK( std::memcmp( entityStorage.data(), entityGolden.data(),
		entityGolden.size() ) == 0 );

	entityState_t entityReceived{};
	msg_t entityReader{};
	MSG_Init( &entityReader, entityStorage.data(),
		static_cast<int>( entityStorage.size() ) );
	entityReader.cursize = entityWriter.cursize;
	MSG_BeginReading( &entityReader );
	MSG_RawBitstream( &entityReader );
	const int entityNumber = MSG_ReadEntitynum( &entityReader );
	CHECK( MSG_ReadDeltaEntityForWireProfile( &entityReader, &entityBase,
		&entityReceived, entityNumber, NETCHAN_WIRE_LEGACY_Q3 ) );
	CHECK( entityReader.bit == entityWriter.bit );
	CHECK( entityReceived.number == entitySent.number );
	CHECK( entityReceived.event == entitySent.event );

	static constexpr std::array<byte, 10> playerGolden{
		0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x34, 0x12, 0x00
	};
	std::array<byte, 64> playerStorage{};
	playerState_t playerBase{};
	playerState_t playerSent{};
	playerSent.loopSound = 0x1234;
	msg_t playerWriter{};
	MSG_Init( &playerWriter, playerStorage.data(),
		static_cast<int>( playerStorage.size() ) );
	MSG_RawBitstream( &playerWriter );
	CHECK( MSG_WriteDeltaPlayerstateForWireProfile( &playerWriter,
		&playerBase, &playerSent, NETCHAN_WIRE_LEGACY_Q3 ) );
	CHECK( playerWriter.bit == 73 );
	CHECK( std::memcmp( playerStorage.data(), playerGolden.data(),
		playerGolden.size() ) == 0 );

	playerState_t playerReceived{};
	msg_t playerReader{};
	MSG_Init( &playerReader, playerStorage.data(),
		static_cast<int>( playerStorage.size() ) );
	playerReader.cursize = playerWriter.cursize;
	MSG_BeginReading( &playerReader );
	MSG_RawBitstream( &playerReader );
	CHECK( MSG_ReadDeltaPlayerstateForWireProfile( &playerReader, &playerBase,
		&playerReceived, NETCHAN_WIRE_LEGACY_Q3 ) );
	CHECK( playerReader.bit == playerWriter.bit );
	CHECK( playerReceived.loopSound == playerSent.loopSound );
	return 0;
}

int TestInvalidDeltaWireProfile() {
	const auto invalidProfile = static_cast<netchanWireProfile_t>( 99 );
	std::array<byte, 64> storage{};
	storage.fill( 0xa5 );
	msg_t writer{};
	MSG_Init( &writer, storage.data(), static_cast<int>( storage.size() ) );
	MSG_RawBitstream( &writer );
	usercmd_t userFrom{};
	usercmd_t userTo{};
	entityState_t entityFrom{};
	entityState_t entityTo{};
	entityTo.number = 1;
	playerState_t playerFrom{};
	playerState_t playerTo{};
	CHECK( !MSG_WriteDeltaUsercmdKeyForWireProfile( &writer, 0,
		&userFrom, &userTo, invalidProfile ) );
	CHECK( !MSG_WriteDeltaEntityForWireProfile( &writer, &entityFrom,
		&entityTo, qtrue, invalidProfile ) );
	CHECK( !MSG_WriteDeltaPlayerstateForWireProfile( &writer, &playerFrom,
		&playerTo, invalidProfile ) );
	CHECK( writer.bit == 0 );
	CHECK( writer.cursize == 0 );
	CHECK( storage.front() == 0xa5 );

	msg_t reader{};
	MSG_Init( &reader, storage.data(), static_cast<int>( storage.size() ) );
	reader.cursize = static_cast<int>( storage.size() );
	MSG_BeginReading( &reader );
	MSG_RawBitstream( &reader );
	usercmd_t userOut{};
	userOut.serverTime = 0x12345678;
	const usercmd_t expectedUser = userOut;
	entityState_t entityOut{};
	entityOut.number = 77;
	const entityState_t expectedEntity = entityOut;
	playerState_t playerOut{};
	playerOut.commandTime = 0x23456789;
	const playerState_t expectedPlayer = playerOut;
	CHECK( !MSG_ReadDeltaUsercmdKeyForWireProfile( &reader, 0, &userFrom,
		&userOut, invalidProfile ) );
	CHECK( !MSG_ReadDeltaEntityForWireProfile( &reader, &entityFrom,
		&entityOut, 1, invalidProfile ) );
	CHECK( !MSG_ReadDeltaPlayerstateForWireProfile( &reader, &playerFrom,
		&playerOut, invalidProfile ) );
	CHECK( reader.bit == 0 );
	CHECK( reader.readcount == 0 );
	CHECK( std::memcmp( &userOut, &expectedUser, sizeof( userOut ) ) == 0 );
	CHECK( std::memcmp( &entityOut, &expectedEntity, sizeof( entityOut ) ) == 0 );
	CHECK( std::memcmp( &playerOut, &expectedPlayer, sizeof( playerOut ) ) == 0 );
	return 0;
}

template<typename State, typename ReadDelta>
int CheckTruncatedDeltaReads( const msg_t &writer, const State &sent,
	ReadDelta readDelta ) {
	// Exercise every incomplete byte prefix, including truncation inside a
	// Huffman symbol, a float, and the playerstate array masks and payloads.
	// A rejected packet must leave the caller's last usable state untouched.
	for ( int length = 0; length < ( writer.bit + 7 ) / 8; ++length ) {
		msg_t reader = writer;
		reader.cursize = length;
		MSG_BeginReading( &reader );
		if ( writer.rawbit ) MSG_RawBitstream( &reader );
		State received = sent;
		CHECK( !readDelta( &reader, &received ) );
		CHECK( reader.readcount > reader.cursize );
		CHECK( std::memcmp( &received, &sent, sizeof( sent ) ) == 0 );
	}

	msg_t reader = writer;
	MSG_BeginReading( &reader );
	if ( writer.rawbit ) MSG_RawBitstream( &reader );
	State received{};
	CHECK( readDelta( &reader, &received ) );
	CHECK( reader.bit == writer.bit );
	return 0;
}

int TestTruncatedDeltaReads() {
	for ( const auto profile : { NETCHAN_WIRE_LEGACY_Q3,
		NETCHAN_WIRE_IOQ3, NETCHAN_WIRE_QL_RETAIL } ) {
		for ( const bool raw : { false, true } ) {
			std::array<byte, 1024> storage{};
			msg_t writer{};
			MSG_Init( &writer, storage.data(), static_cast<int>( storage.size() ) );
			if ( raw ) MSG_RawBitstream( &writer );

			usercmd_t userBase{};
			usercmd_t userSent{};
			userSent.serverTime = 123456;
			userSent.angles[0] = 32767;
			userSent.angles[1] = 16384;
			userSent.buttons = 0x5a5a;
			userSent.forwardmove = -127;
			userSent.rightmove = 64;
			userSent.upmove = -12;
			userSent.weapon = 7;
			userSent.weaponPrimary = 2;
			userSent.fov = 100;
			CHECK( MSG_WriteDeltaUsercmdKeyForWireProfile( &writer, 0x12345678,
				&userBase, &userSent, profile ) );
			CHECK( !writer.overflowed );
			if ( const int result = CheckTruncatedDeltaReads( writer, userSent,
				[&]( msg_t *reader, usercmd_t *out ) {
					return MSG_ReadDeltaUsercmdKeyForWireProfile( reader,
						0x12345678, &userBase, out, profile );
				} ) ) return result;

			MSG_Clear( &writer );
			entityState_t entityBase{};
			entityState_t entitySent{};
			entitySent.number = 17;
			entitySent.pos.trBase[0] = 123.5f;
			entitySent.pos.trTime = 123456;
			entitySent.frame = 0x1234;
			entitySent.location = 5;
			CHECK( MSG_WriteDeltaEntityForWireProfile( &writer, &entityBase,
				&entitySent, qtrue, profile ) );
			CHECK( !writer.overflowed );
			if ( const int result = CheckTruncatedDeltaReads( writer, entitySent,
				[&]( msg_t *reader, entityState_t *out ) {
					(void)MSG_ReadEntitynum( reader );
					return MSG_ReadDeltaEntityForWireProfile( reader,
						&entityBase, out, entitySent.number, profile );
				} ) ) return result;

			MSG_Clear( &writer );
			playerState_t playerBase{};
			playerState_t playerSent{};
			playerSent.commandTime = 424242;
			playerSent.origin[0] = 12.5f;
			playerSent.weaponTime = -125;
			playerSent.viewheight = -16;
			playerSent.loopSound = 0x1234;
			playerSent.forwardmove = -127;
			playerSent.stats[15] = 321;
			playerSent.persistant[15] = -45;
			playerSent.ammo[15] = 200;
			playerSent.powerups[15] = 999999;
			CHECK( MSG_WriteDeltaPlayerstateForWireProfile( &writer, &playerBase,
				&playerSent, profile ) );
			CHECK( !writer.overflowed );
			if ( const int result = CheckTruncatedDeltaReads( writer, playerSent,
				[&]( msg_t *reader, playerState_t *out ) {
					return MSG_ReadDeltaPlayerstateForWireProfile( reader,
						&playerBase, out, profile );
				} ) ) return result;
		}
	}
	return 0;
}

template<typename State, typename ReadDelta>
int CheckMalformedDeltaDrop( const State &baseline, ReadDelta readDelta ) {
	State received = baseline;
	auto callback = [&]() {
		readDelta( &received );
	};
	const int dropped = Test_CatchMessageDrop( []( void *context ) {
		(*static_cast<decltype( callback ) *>( context ))();
	}, &callback );
	CHECK( dropped );
	CHECK( std::memcmp( &received, &baseline, sizeof( baseline ) ) == 0 );
	return 0;
}

int TestMalformedDeltaFieldCounts() {
	for ( const auto profile : { NETCHAN_WIRE_LEGACY_Q3,
		NETCHAN_WIRE_IOQ3, NETCHAN_WIRE_QL_RETAIL } ) {
		const int entityFields = profile == NETCHAN_WIRE_QL_RETAIL ? 58 : 51;
		const int playerFields = profile == NETCHAN_WIRE_QL_RETAIL ? 58 : 48;
		// Entity body: not removed, has delta, field count one past the
		// selected profile's table. No entity number is included in the body.
		std::array<byte, 2> entityBytes{
			static_cast<byte>( ( ( entityFields + 1 ) << 2 ) | 2 ), 0
		};
		msg_t entityReader{};
		MSG_Init( &entityReader, entityBytes.data(),
			static_cast<int>( entityBytes.size() ) );
		entityReader.cursize = static_cast<int>( entityBytes.size() );
		MSG_RawBitstream( &entityReader );
		entityState_t entityBase{};
		entityBase.number = 17;
		entityBase.pos.trTime = 123456;
		if ( const int result = CheckMalformedDeltaDrop( entityBase,
			[&]( entityState_t *out ) {
				MSG_ReadDeltaEntityForWireProfile( &entityReader,
					&entityBase, out, entityBase.number, profile );
			} ) ) return result;

		std::array<byte, 1> playerBytes{ static_cast<byte>( playerFields + 1 ) };
		msg_t playerReader{};
		MSG_Init( &playerReader, playerBytes.data(),
			static_cast<int>( playerBytes.size() ) );
		playerReader.cursize = static_cast<int>( playerBytes.size() );
		MSG_RawBitstream( &playerReader );
		playerState_t playerBase{};
		playerBase.commandTime = 424242;
		playerBase.stats[3] = 200;
		if ( const int result = CheckMalformedDeltaDrop( playerBase,
			[&]( playerState_t *out ) {
				MSG_ReadDeltaPlayerstateForWireProfile( &playerReader,
					&playerBase, out, profile );
			} ) ) return result;
	}
	return 0;
}

struct RawFloatVector {
	std::array<byte, 64> bytes{};
	int bits = 0;

	void Append( std::uint32_t value, int width ) {
		for ( int i = 0; i < width; ++i, ++bits ) {
			bytes[bits / 8] |= static_cast<byte>( ( ( value >> i ) & 1u ) << ( bits % 8 ) );
		}
	}
};

std::uint32_t FloatBits( const float &value ) {
	std::uint32_t bits;
	std::memcpy( &bits, &value, sizeof( bits ) );
	return bits;
}

int TestFloatDeltaBitPatterns() {
	struct FloatCase {
		std::uint32_t bits;
		bool compact;
		int integer;
	};
	// The expected representation is specified independently for each bit
	// pattern. Compact floats use the original 13-bit, bias-4096 contract.
	static constexpr FloatCase cases[] = {
		{ 0x00000000u, true, 0 },       // +0
		{ 0x80000000u, true, 0 },       // -0 keeps the legacy zero normalization
		{ 0x3f800000u, true, 1 },
		{ 0xbf800000u, true, -1 },
		{ 0x457ff000u, true, 4095 },    // compact upper integer
		{ 0xc5800000u, true, -4096 },   // compact lower integer
		{ 0x45800000u, false, 0 },      // +4096
		{ 0xc5800800u, false, 0 },      // -4097
		{ 0x457ff800u, false, 0 },      // +4095.5
		{ 0xc5800200u, false, 0 },      // -4096.25
		{ 0x3fa00000u, false, 0 },      // +1.25
		{ 0x00000001u, false, 0 },      // smallest positive subnormal
		{ 0x80000001u, false, 0 },      // smallest negative subnormal
		{ 0x4f000000u, false, 0 },      // +2^31, outside int range
		{ 0xcf000000u, false, 0 },      // -2^31
		{ 0xcf000001u, false, 0 },      // below -2^31
		{ 0x7f7fffffu, false, 0 },      // FLT_MAX
		{ 0xff7fffffu, false, 0 },      // -FLT_MAX
		{ 0x7f800000u, false, 0 },      // +infinity
		{ 0xff800000u, false, 0 },      // -infinity
		{ 0x7fc12345u, false, 0 },      // quiet NaN payload
		{ 0xffc54321u, false, 0 },      // negative quiet NaN payload
		{ 0x7f800001u, false, 0 }       // signaling NaN payload
	};
	for ( const auto profile : { NETCHAN_WIRE_LEGACY_Q3,
		NETCHAN_WIRE_IOQ3, NETCHAN_WIRE_QL_RETAIL } ) {
		for ( const bool raw : { false, true } ) {
			for ( const FloatCase &sample : cases ) {
				std::array<byte, 256> storage{};
				msg_t writer{};
				MSG_Init( &writer, storage.data(), static_cast<int>( storage.size() ) );
				if ( raw ) MSG_RawBitstream( &writer );
				entityState_t entityBase{};
				entityBase.number = 17;
				entityBase.pos.trBase[0] = 2.0f;
				entityState_t entitySent = entityBase;
				std::memcpy( &entitySent.pos.trBase[0], &sample.bits, sizeof( sample.bits ) );
				CHECK( MSG_WriteDeltaEntityForWireProfile( &writer, &entityBase,
					&entitySent, qtrue, profile ) );
				CHECK( !writer.overflowed );
				if ( raw ) {
					RawFloatVector expected;
					expected.Append( 17, 10 ); // entity number
					expected.Append( 0, 1 );  // not removed
					expected.Append( 1, 1 );  // delta
					expected.Append( 2, 8 );  // pos.trBase[0] is field 1 in both layouts
					expected.Append( 0, 1 );  // unchanged pos.trTime
					expected.Append( 1, 1 );  // changed pos.trBase[0]
					const bool zero = ( sample.bits & 0x7fffffffu ) == 0;
					expected.Append( zero ? 0 : 1, 1 );
					if ( !zero ) {
						expected.Append( sample.compact ? 0 : 1, 1 );
						expected.Append( sample.compact ?
							static_cast<std::uint32_t>( sample.integer + 4096 ) : sample.bits,
							sample.compact ? 13 : 32 );
					}
					CHECK( writer.bit == expected.bits );
					CHECK( std::memcmp( storage.data(), expected.bytes.data(),
						static_cast<std::size_t>( writer.cursize ) ) == 0 );
				}
				msg_t reader = writer;
				MSG_BeginReading( &reader );
				if ( raw ) MSG_RawBitstream( &reader );
				entityState_t entityReceived{};
				const int number = MSG_ReadEntitynum( &reader );
				CHECK( MSG_ReadDeltaEntityForWireProfile( &reader, &entityBase,
					&entityReceived, number, profile ) );
				const std::uint32_t expectedBits =
					sample.bits == 0x80000000u ? 0 : sample.bits;
				CHECK( FloatBits( entityReceived.pos.trBase[0] ) == expectedBits );
				CHECK( reader.bit == writer.bit );

				MSG_Clear( &writer );
				storage.fill( 0 );
				playerState_t playerBase{};
				playerBase.origin[0] = 2.0f;
				playerState_t playerSent = playerBase;
				std::memcpy( &playerSent.origin[0], &sample.bits, sizeof( sample.bits ) );
				CHECK( MSG_WriteDeltaPlayerstateForWireProfile( &writer, &playerBase,
					&playerSent, profile ) );
				CHECK( !writer.overflowed );
				if ( raw ) {
					RawFloatVector expected;
					expected.Append( 2, 8 ); // origin[0] is field 1 in both layouts
					expected.Append( 0, 1 ); // unchanged commandTime
					expected.Append( 1, 1 ); // changed origin[0]
					expected.Append( sample.compact ? 0 : 1, 1 );
					expected.Append( sample.compact ?
						static_cast<std::uint32_t>( sample.integer + 4096 ) : sample.bits,
						sample.compact ? 13 : 32 );
					expected.Append( 0, 1 ); // unchanged arrays
					CHECK( writer.bit == expected.bits );
					CHECK( std::memcmp( storage.data(), expected.bytes.data(),
						static_cast<std::size_t>( writer.cursize ) ) == 0 );
				}
				reader = writer;
				MSG_BeginReading( &reader );
				if ( raw ) MSG_RawBitstream( &reader );
				playerState_t playerReceived{};
				CHECK( MSG_ReadDeltaPlayerstateForWireProfile( &reader, &playerBase,
					&playerReceived, profile ) );
				CHECK( FloatBits( playerReceived.origin[0] ) == expectedBits );
				CHECK( reader.bit == writer.bit );
			}
		}
	}
	return 0;
}

int TestUserCommandRoundTrip() {
	std::array<byte, 256> storage{};
	msg_t writer{};
	usercmd_t from{};
	usercmd_t sent{};
	usercmd_t received{};

	from.serverTime = 1000;
	from.fov = 100;
	sent = from;
	sent.serverTime = 1008;
	sent.angles[0] = 1234;
	sent.angles[1] = 63191;
	sent.angles[2] = 32767;
	sent.buttons = 0x5a5a;
	sent.weapon = 9;
	sent.weaponPrimary = 3;
	sent.fov = 110;
	sent.forwardmove = -127;
	sent.rightmove = 64;
	sent.upmove = -12;

	MSG_Init( &writer, storage.data(), static_cast<int>( storage.size() ) );
	CHECK( MSG_WriteDeltaUsercmdKeyForWireProfile( &writer, 0x10203040,
		&from, &sent, NETCHAN_WIRE_QL_RETAIL ) );
	CHECK( !writer.overflowed );

	msg_t reader{};
	MSG_Init( &reader, storage.data(), static_cast<int>( storage.size() ) );
	reader.cursize = writer.cursize;
	MSG_BeginReading( &reader );
	CHECK( MSG_ReadDeltaUsercmdKeyForWireProfile( &reader, 0x10203040,
		&from, &received, NETCHAN_WIRE_QL_RETAIL ) );
	CHECK( sent.serverTime == received.serverTime );
	CHECK( sent.angles[0] == received.angles[0] );
	CHECK( sent.angles[1] == received.angles[1] );
	CHECK( sent.angles[2] == received.angles[2] );
	CHECK( sent.buttons == received.buttons );
	CHECK( sent.weapon == received.weapon );
	CHECK( sent.weaponPrimary == received.weaponPrimary );
	CHECK( sent.fov == received.fov );
	CHECK( sent.forwardmove == received.forwardmove );
	CHECK( sent.rightmove == received.rightmove );
	CHECK( sent.upmove == received.upmove );
	CHECK( reader.readcount <= reader.cursize );
	return 0;
}

int TestUserCommandClockWrap() {
	std::array<byte, 64> storage{};
	msg_t writer{};
	usercmd_t from{};
	usercmd_t sent{};
	usercmd_t received{};

	from.serverTime = ( std::numeric_limits<int>::max )() - 3;
	sent = from;
	// Eight milliseconds later in the protocol's wrapping 32-bit clock.
	sent.serverTime = ( std::numeric_limits<int>::min )() + 4;

	MSG_Init( &writer, storage.data(), static_cast<int>( storage.size() ) );
	CHECK( MSG_WriteDeltaUsercmdKeyForWireProfile( &writer, 0x10203040,
		&from, &sent, NETCHAN_WIRE_QL_RETAIL ) );
	CHECK( !writer.overflowed );

	msg_t reader{};
	MSG_Init( &reader, storage.data(), static_cast<int>( storage.size() ) );
	reader.cursize = writer.cursize;
	MSG_BeginReading( &reader );
	CHECK( MSG_ReadDeltaUsercmdKeyForWireProfile( &reader, 0x10203040,
		&from, &received, NETCHAN_WIRE_QL_RETAIL ) );
	CHECK( received.serverTime == sent.serverTime );
	CHECK( reader.readcount <= reader.cursize );
	return 0;
}

int TestEntityRoundTrip() {
	std::array<byte, MAX_MSGLEN_BUF> storage{};
	msg_t writer{};
	entityState_t baseline{};
	entityState_t sent{};
	entityState_t received{};

	sent.number = 17;
	sent.pos.trType = TR_QL_ACCEL;
	sent.pos.trTime = 0x10203040;
	sent.pos.trBase[0] = 123.5f;
	sent.pos.trDelta[2] = -42.25f;
	sent.pos.gravity = 800.0f;
	sent.event = 777;
	sent.eFlags = 0x54321;
	sent.weapon = 8;
	sent.health = 125;
	sent.armor = 75;
	sent.location = 63;
	sent.doubleJumped = 1;

	MSG_Init( &writer, storage.data(), static_cast<int>( storage.size() ) );
	CHECK( MSG_WriteDeltaEntityForWireProfile( &writer, &baseline, &sent,
		qtrue, NETCHAN_WIRE_QL_RETAIL ) );
	CHECK( !writer.overflowed );

	msg_t reader{};
	MSG_Init( &reader, storage.data(), static_cast<int>( storage.size() ) );
	reader.cursize = writer.cursize;
	MSG_BeginReading( &reader );
	const int entityNumber = MSG_ReadEntitynum( &reader );
	CHECK( entityNumber == sent.number );
	CHECK( MSG_ReadDeltaEntityForWireProfile( &reader, &baseline, &received,
		entityNumber, NETCHAN_WIRE_QL_RETAIL ) );
	CHECK( std::memcmp( &sent, &received, sizeof( sent ) ) == 0 );
	CHECK( reader.readcount <= reader.cursize );
	return 0;
}

int TestPlayerStateRoundTrip() {
	std::array<byte, MAX_MSGLEN_BUF> storage{};
	msg_t writer{};
	playerState_t baseline{};
	playerState_t sent{};
	playerState_t received{};

	sent.commandTime = 424242;
	sent.origin[0] = 12.5f;
	sent.velocity[2] = -320.0f;
	sent.weaponTime = -125;
	sent.pm_flags = 0x123456;
	sent.weapon = 7;
	sent.weaponPrimary = 2;
	sent.fov = 103;
	sent.location = 14;
	sent.forwardmove = -127;
	sent.rightmove = 100;
	sent.upmove = -4;
	sent.stats[3] = 321;
	sent.persistant[7] = -45;
	sent.ammo[6] = 200;
	sent.powerups[2] = 999999;

	MSG_Init( &writer, storage.data(), static_cast<int>( storage.size() ) );
	CHECK( MSG_WriteDeltaPlayerstateForWireProfile( &writer, &baseline,
		&sent, NETCHAN_WIRE_QL_RETAIL ) );
	CHECK( !writer.overflowed );

	msg_t reader{};
	MSG_Init( &reader, storage.data(), static_cast<int>( storage.size() ) );
	reader.cursize = writer.cursize;
	MSG_BeginReading( &reader );
	CHECK( MSG_ReadDeltaPlayerstateForWireProfile( &reader, &baseline,
		&received, NETCHAN_WIRE_QL_RETAIL ) );
	CHECK( std::memcmp( &sent, &received, sizeof( sent ) ) == 0 );
	CHECK( reader.readcount <= reader.cursize );
	return 0;
}

int TestUserCommandProfileKeyMask() {
	for ( const auto profile : { NETCHAN_WIRE_LEGACY_Q3,
		NETCHAN_WIRE_IOQ3, NETCHAN_WIRE_QL_RETAIL } ) {
		for ( const int key : { 0, 0x10000 } ) {
			for ( const int time : { 1, 0x10000 } ) {
				for ( const bool inherited : { false, true } ) {
					usercmd_t baseline{};
					if ( inherited ) {
						baseline.angles[0] = baseline.angles[1] =
							baseline.angles[2] = baseline.buttons = 0x1ffff;
					}
					usercmd_t sent = baseline;
					sent.serverTime = time;
					sent.weapon = 7;
					if ( !inherited ) {
						sent.angles[0] = sent.angles[1] = sent.angles[2] = sent.buttons = 0xffff;
					}
					std::array<byte, 128> packet{};
					msg_t message{};
					MSG_Init( &message, packet.data(), static_cast<int>( packet.size() ) );
					CHECK( MSG_WriteDeltaUsercmdKeyForWireProfile( &message, key,
						&baseline, &sent, profile ) );
					MSG_BeginReading( &message );
					usercmd_t received{}, expected = sent;
					if ( profile == NETCHAN_WIRE_QL_RETAIL && !inherited ) {
						const int extraKeyBit = ( key ^ time ) & 0x10000;
						for ( auto &angle : expected.angles ) angle ^= extraKeyBit;
						expected.buttons ^= extraKeyBit;
					}
					CHECK( MSG_ReadDeltaUsercmdKeyForWireProfile( &message, key,
						&baseline, &received, profile ) );
					CHECK( std::memcmp( &received, &expected, sizeof( expected ) ) == 0 );
				}
			}
		}
	}
	// Synthetic command encoded by retail with key -1 and serverTime 1.
	// Retail delivers the extra key bit in all changed angle/button words.
	std::array<byte, 8> packet{{ 0xf7, 0xdb, 0xdb, 0x5b, 0xdc, 0xde, 0xf7, 0x0d }};
	usercmd_t baseline{}, expected{}, received{};
	baseline.forwardmove = baseline.rightmove = baseline.upmove = -128;
	expected = baseline;
	expected.serverTime = 1;
	expected.angles[0] = expected.angles[1] = expected.angles[2] = expected.buttons = 0x1ffff;
	expected.weapon = expected.weaponPrimary = expected.fov = 255;
	msg_t reader{};
	MSG_Init( &reader, packet.data(), static_cast<int>( packet.size() ) );
	reader.cursize = static_cast<int>( packet.size() );
	MSG_BeginReading( &reader );
	CHECK( MSG_ReadDeltaUsercmdKeyForWireProfile( &reader, -1,
		&baseline, &received, NETCHAN_WIRE_QL_RETAIL ) );
	CHECK( std::memcmp( &received, &expected, sizeof( expected ) ) == 0 );
	CHECK( reader.bit == 60 && reader.readcount == 8 );
	return 0;
}

int TestUserCommandMovementMinimum() {
	for ( const auto profile : { NETCHAN_WIRE_LEGACY_Q3,
		NETCHAN_WIRE_IOQ3, NETCHAN_WIRE_QL_RETAIL } ) {
		for ( const bool inherited : { false, true } ) {
			usercmd_t baseline{};
			if ( inherited ) {
				baseline.forwardmove = baseline.rightmove = baseline.upmove = -128;
			}
			usercmd_t sent = baseline;
			sent.forwardmove = sent.rightmove = sent.upmove = -128;
			sent.weapon = 7; // Enter changed-command decoding even for inherited bytes.
			std::array<byte, 128> packet{};
			msg_t message{};
			MSG_Init( &message, packet.data(), static_cast<int>( packet.size() ) );
			CHECK( MSG_WriteDeltaUsercmdKeyForWireProfile( &message, 0,
				&baseline, &sent, profile ) );
			MSG_BeginReading( &message );
			usercmd_t received{};
			CHECK( MSG_ReadDeltaUsercmdKeyForWireProfile( &message, 0,
				&baseline, &received, profile ) );
			usercmd_t expected = sent;
			if ( profile != NETCHAN_WIRE_QL_RETAIL ) {
				expected.forwardmove = expected.rightmove = expected.upmove = -127;
			}
			CHECK( std::memcmp( &received, &expected, sizeof( expected ) ) == 0 );
		}
	}
	// Key zero, zero baseline: observed retail packet changes all three
	// movement bytes to -128. No engine encoder constructs this oracle.
	std::array<byte, 5> retailPacket{{ 0x8d, 0xdf, 0xef, 0x07, 0x00 }};
	msg_t reader{};
	MSG_Init( &reader, retailPacket.data(), static_cast<int>( retailPacket.size() ) );
	reader.cursize = static_cast<int>( retailPacket.size() );
	MSG_BeginReading( &reader );
	usercmd_t baseline{}, expected{}, received{};
	expected.forwardmove = expected.rightmove = expected.upmove = -128;
	CHECK( MSG_ReadDeltaUsercmdKeyForWireProfile( &reader, 0,
		&baseline, &received, NETCHAN_WIRE_QL_RETAIL ) );
	CHECK( std::memcmp( &received, &expected, sizeof( expected ) ) == 0 );
	CHECK( reader.bit == 32 && reader.readcount == 5 );
	return 0;
}

int TestRetailEntityFieldBoundaries() {
	struct Fixture {
		std::size_t offset;
		std::uint32_t value;
		int bits;
		int bytes;
		std::array<byte, 13> packet;
	};
	std::array<Fixture, 58> fixtures{{
#include "fixtures/ql_retail_entity_fields.inc"
	}};
	for ( auto &fixture : fixtures ) {
		entityState_t baseline{}, expected{};
		baseline.number = expected.number = 17;
		CHECK( fixture.offset + sizeof( fixture.value ) <= sizeof( expected ) );
		std::memcpy( reinterpret_cast<byte *>( &expected ) + fixture.offset,
			&fixture.value, sizeof( fixture.value ) );
		struct {
			std::uint32_t before = 0x13579bdf;
			entityState_t state{};
			std::uint32_t after = 0x2468ace0;
		} output;
		msg_t reader{};
		MSG_Init( &reader, fixture.packet.data(), static_cast<int>( fixture.packet.size() ) );
		reader.cursize = fixture.bytes;
		MSG_BeginReading( &reader );
		const int number = MSG_ReadEntitynum( &reader );
		CHECK( number == expected.number );
		CHECK( MSG_ReadDeltaEntityForWireProfile( &reader, &baseline,
			&output.state, number, NETCHAN_WIRE_QL_RETAIL ) );
		CHECK( std::memcmp( &output.state, &expected, sizeof( expected ) ) == 0 );
		CHECK( reader.bit == fixture.bits && reader.readcount == fixture.bytes );
		CHECK( output.before == 0x13579bdf && output.after == 0x2468ace0 );
	}
	return 0;
}

int TestRetailPlayerFieldBoundaries() {
	struct Fixture {
		std::size_t offset;
		std::size_t width;
		std::uint32_t value;
		int bits;
		int bytes;
		std::array<byte, 11> packet;
	};
	std::array<Fixture, 58> fixtures{{
#include "fixtures/ql_retail_player_fields.inc"
	}};
	for ( auto &fixture : fixtures ) {
		playerState_t baseline{}, expected{};
		CHECK( fixture.offset + fixture.width <= sizeof( expected ) );
		if ( fixture.width == 1 ) {
			const byte value = static_cast<byte>( fixture.value );
			std::memcpy( reinterpret_cast<byte *>( &expected ) + fixture.offset,
				&value, sizeof( value ) );
		} else {
			CHECK( fixture.width == sizeof( fixture.value ) );
			std::memcpy( reinterpret_cast<byte *>( &expected ) + fixture.offset,
				&fixture.value, sizeof( fixture.value ) );
		}
		struct {
			std::uint32_t before = 0x13579bdf;
			playerState_t state{};
			std::uint32_t after = 0x2468ace0;
		} output;
		msg_t reader{};
		MSG_Init( &reader, fixture.packet.data(), static_cast<int>( fixture.packet.size() ) );
		reader.cursize = fixture.bytes;
		MSG_BeginReading( &reader );
		CHECK( MSG_ReadDeltaPlayerstateForWireProfile( &reader, &baseline,
			&output.state, NETCHAN_WIRE_QL_RETAIL ) );
		CHECK( std::memcmp( &output.state, &expected, sizeof( expected ) ) == 0 );
		CHECK( reader.bit == fixture.bits && reader.readcount == fixture.bytes );
		CHECK( output.before == 0x13579bdf && output.after == 0x2468ace0 );
	}
	return 0;
}

int TestRetailMovementBytePayloads() {
	// Synthetic states serialized by the pinned retail encoder under CPU
	// emulation. See docs/fntech3/2026-09-08-retail-playerstate-wire.md.
	// Retail's wider comparisons can mark adjacent movement bytes changed.
	std::array<std::array<byte, 11>, 2> packets{{
		{{ 0xf1, 0, 0, 0, 0, 0, 0, 0, 0xfd, 0x18, 0 }},
		{{ 0x99, 0x01, 0, 0, 0, 0, 0, 0, 0xfd, 0x78, 0x0a }}
	}};
	for ( int i = 0; i < 2; ++i ) {
		playerState_t baseline{};
		if ( i == 1 ) baseline.rightmove = 99;
		playerState_t expected = baseline;
		expected.rightmove = 99;
		if ( i == 1 ) expected.upmove = -127;
		struct {
			std::uint32_t before = 0x13579bdf;
			playerState_t state{};
			std::uint32_t after = 0x2468ace0;
		} output;
		msg_t reader{};
		MSG_Init( &reader, packets[i].data(), static_cast<int>( packets[i].size() ) );
		reader.cursize = i == 0 ? 10 : 11;
		MSG_BeginReading( &reader );
		CHECK( MSG_ReadDeltaPlayerstateForWireProfile( &reader, &baseline,
			&output.state, NETCHAN_WIRE_QL_RETAIL ) );
		CHECK( std::memcmp( &output.state, &expected, sizeof( expected ) ) == 0 );
		CHECK( reader.bit == ( i == 0 ? 78 : 86 ) );
		CHECK( reader.readcount == reader.cursize );
		CHECK( output.before == 0x13579bdf && output.after == 0x2468ace0 );

		// Snapshot fields need not begin at a byte boundary. Relocate the
		// observed compressed bits without using our encoder as an oracle.
		const int payloadBits = reader.bit;
		for ( int offset = 0; offset < 8; ++offset ) {
			std::array<byte, 16> shifted{};
			for ( int bit = 0; bit < payloadBits; ++bit ) {
				shifted[( bit + offset ) / 8] |= static_cast<byte>(
					( ( packets[i][bit / 8] >> ( bit % 8 ) ) & 1 )
					<< ( ( bit + offset ) % 8 ) );
			}
			// Compressed messages retain a trailing byte when the final bit
			// lands exactly on a byte boundary (MSG_WriteBits cursize rule).
			const int completeBytes = ( offset + payloadBits ) / 8 + 1;
			for ( int length = 0; length <= completeBytes; ++length ) {
				for ( const bool inPlace : { false, true } ) {
					output.state = baseline;
					// Use a distinct destination to detect even a premature
					// baseline copy when decoding fails.
					if ( !inPlace ) output.state.commandTime = 0x12345678;
					const playerState_t untouched = output.state;
					MSG_Init( &reader, shifted.data(), static_cast<int>( shifted.size() ) );
					reader.cursize = length;
					MSG_BeginReading( &reader );
					reader.bit = offset;
					reader.readcount = offset ? 1 : 0;
					const bool complete = length == completeBytes;
					CHECK( !!MSG_ReadDeltaPlayerstateForWireProfile( &reader,
						inPlace ? &output.state : &baseline, &output.state,
						NETCHAN_WIRE_QL_RETAIL ) == complete );
					CHECK( std::memcmp( &output.state,
						complete ? &expected : &untouched, sizeof( expected ) ) == 0 );
					CHECK( output.before == 0x13579bdf && output.after == 0x2468ace0 );
					if ( complete ) {
						CHECK( reader.bit == offset + payloadBits );
					} else {
						CHECK( reader.readcount > reader.cursize );
					}
				}
			}
		}
	}
	return 0;
}

} // namespace

int main() {
	if ( const int result = TestBounds() ) return result;
	if ( const int result = TestWireProfileUserCommandHash() ) return result;
	if ( const int result = TestWireProfileCommandStrings() ) return result;
	if ( const int result = TestUserCommandWireLayouts() ) return result;
	if ( const int result = TestEntityWireLayouts() ) return result;
	if ( const int result = TestPlayerStateWireLayouts() ) return result;
	if ( const int result = TestPinnedQuake3RawVectors() ) return result;
	if ( const int result = TestInvalidDeltaWireProfile() ) return result;
	if ( const int result = TestTruncatedDeltaReads() ) return result;
	if ( const int result = TestMalformedDeltaFieldCounts() ) return result;
	if ( const int result = TestFloatDeltaBitPatterns() ) return result;
	if ( const int result = TestUserCommandRoundTrip() ) return result;
	if ( const int result = TestUserCommandClockWrap() ) return result;
	if ( const int result = TestEntityRoundTrip() ) return result;
	if ( const int result = TestPlayerStateRoundTrip() ) return result;
	if ( const int result = TestRetailMovementBytePayloads() ) return result;
	if ( const int result = TestRetailPlayerFieldBoundaries() ) return result;
	if ( const int result = TestRetailEntityFieldBoundaries() ) return result;
	if ( const int result = TestUserCommandMovementMinimum() ) return result;
	if ( const int result = TestUserCommandProfileKeyMask() ) return result;
	return 0;
}
