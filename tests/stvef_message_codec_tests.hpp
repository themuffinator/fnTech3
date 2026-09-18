/* Elite Force Holomatch message tests. Included by message_codec_tests.cpp,
 * which supplies CHECK and the Com_Error/Com_Printf stubs.
 *
 * Expected bytes are hand-packed from the protocol-24 rules: plain bits, least
 * significant first; the 5-bit change-vector symbol or 31 and a raw 50-bit
 * vector; no per-field flags; floats as 0+(value+4096):13 or 1+raw:32. */

#include <array>
#include <cstring>

namespace stvef_message_tests {

template<std::size_t Size>
bool BytesAre( const msg_t &msg, const std::array<unsigned char, Size> &expected ) {
	return msg.cursize == static_cast<int>( Size ) &&
		std::memcmp( msg.data, expected.data(), Size ) == 0;
}

inline void BeginWrite( msg_t &msg, byte *storage, int size ) {
	std::memset( storage, 0xff, static_cast<std::size_t>( size ) );
	MSG_Init( &msg, storage, size );
	MSG_STVEFBitstream( &msg );
}

inline void BeginRead( msg_t &msg, byte *storage, int size, int length ) {
	MSG_Init( &msg, storage, size );
	msg.cursize = length;
	MSG_BeginReading( &msg );
	MSG_STVEFBitstream( &msg );
}

inline int TestAlignment() {
	std::array<byte, 32> storage{};
	msg_t msg{};
	BeginWrite( msg, storage.data(), static_cast<int>( storage.size() ) );
	MSG_WriteBits( &msg, 0x2ab, 10 );
	// A string after a partial byte starts one whole byte later; the writer
	// clears the unused bits and the skipped byte.
	MSG_WriteStringForWireProfile( &msg, "h\xe9%", NETCHAN_WIRE_STVEF_HM );
	CHECK( !msg.overflowed );
	CHECK( BytesAre( msg, std::array<unsigned char, 7>{ 0xab, 0x02, 0x00, 'h', 0xe9, '%', 0 } ) );
	MSG_WriteByte( &msg, 7 );
	// Already aligned: data follows directly.
	const byte payload[2] = { 0x11, 0x22 };
	MSG_WriteData( &msg, payload, 2 );
	CHECK( BytesAre( msg, std::array<unsigned char, 10>{ 0xab, 0x02, 0x00, 'h', 0xe9, '%', 0, 7, 0x11, 0x22 } ) );

	msg_t reader{};
	BeginRead( reader, storage.data(), static_cast<int>( storage.size() ), msg.cursize );
	CHECK( MSG_ReadEntitynum( &reader ) == 0x2ab );
	// High bytes survive; '%' is still made harmless on reading.
	CHECK( std::strcmp( MSG_ReadStringForWireProfile( &reader, NETCHAN_WIRE_STVEF_HM ), "h\xe9." ) == 0 );
	CHECK( MSG_ReadByte( &reader ) == 7 );
	byte copied[2]{};
	MSG_ReadData( &reader, copied, 2 );
	CHECK( copied[0] == 0x11 && copied[1] == 0x22 );
	// The end of a message is a read past its end; there is no EOF opcode.
	CHECK( MSG_ReadByte( &reader ) == -1 );

	// Other profiles keep Quake III's rule: bytes follow the last bit.
	BeginWrite( msg, storage.data(), static_cast<int>( storage.size() ) );
	MSG_RawBitstream( &msg );
	MSG_WriteBits( &msg, 0x2ab, 10 );
	MSG_WriteByte( &msg, 0x5a );
	CHECK( msg.cursize == 3 && msg.bit == 18 );
	return 0;
}

inline int TestEntityDeltas() {
	std::array<byte, 64> storage{};
	msg_t msg{};
	entityState_t from{}, to{};
	from.number = to.number = 5;
	to.pos.trBase[0] = 1.0f;
	to.pos.trBase[1] = -2.5f;
	BeginWrite( msg, storage.data(), static_cast<int>( storage.size() ) );
	CHECK( MSG_WriteDeltaEntityForWireProfile( &msg, &from, &to, qfalse, NETCHAN_WIRE_STVEF_HM ) );
	// Fields 5 and 6 match dictionary entry 2.
	CHECK( BytesAre( msg, std::array<unsigned char, 8>{ 0x05, 0x28, 0x04, 0xc0, 0x00, 0x00, 0x20, 0xc0 } ) );
	msg_t reader{};
	entityState_t decoded{};
	BeginRead( reader, storage.data(), static_cast<int>( storage.size() ), msg.cursize );
	CHECK( MSG_ReadEntitynum( &reader ) == 5 );
	CHECK( MSG_ReadDeltaEntityForWireProfile( &reader, &from, &decoded, 5, NETCHAN_WIRE_STVEF_HM ) );
	CHECK( decoded.pos.trBase[0] == 1.0f && decoded.pos.trBase[1] == -2.5f );

	// No dictionary entry: symbol 31 and the raw vector.
	entityState_t raw{};
	raw.number = 7;
	raw.eType = 3;
	raw.torsoAnim = 0x85;
	entityState_t baseline{};
	baseline.number = 7;
	BeginWrite( msg, storage.data(), static_cast<int>( storage.size() ) );
	CHECK( MSG_WriteDeltaEntityForWireProfile( &msg, &baseline, &raw, qfalse, NETCHAN_WIRE_STVEF_HM ) );
	CHECK( msg.bit == 83 );
	CHECK( BytesAre( msg, std::array<unsigned char, 11>{
		0x07, 0xf8, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1c, 0x28, 0x04 } ) );
	BeginRead( reader, storage.data(), static_cast<int>( storage.size() ), msg.cursize );
	CHECK( MSG_ReadEntitynum( &reader ) == 7 );
	CHECK( MSG_ReadDeltaEntityForWireProfile( &reader, &baseline, &decoded, 7, NETCHAN_WIRE_STVEF_HM ) );
	CHECK( decoded.eType == 3 && decoded.torsoAnim == 0x85 && decoded.number == 7 );

	// Removal, forced no-change and unforced no-change.
	entityState_t gone{};
	gone.number = 9;
	BeginWrite( msg, storage.data(), static_cast<int>( storage.size() ) );
	CHECK( MSG_WriteDeltaEntityForWireProfile( &msg, &gone, nullptr, qfalse, NETCHAN_WIRE_STVEF_HM ) );
	CHECK( msg.bit == 11 && storage[0] == 0x09 && ( storage[1] & 0x07 ) == 0x04 );
	BeginWrite( msg, storage.data(), static_cast<int>( storage.size() ) );
	CHECK( MSG_WriteDeltaEntityForWireProfile( &msg, &gone, &gone, qtrue, NETCHAN_WIRE_STVEF_HM ) );
	CHECK( msg.bit == 12 && storage[0] == 0x09 && ( storage[1] & 0x0f ) == 0x00 );
	BeginWrite( msg, storage.data(), static_cast<int>( storage.size() ) );
	CHECK( MSG_WriteDeltaEntityForWireProfile( &msg, &gone, &gone, qfalse, NETCHAN_WIRE_STVEF_HM ) );
	CHECK( msg.bit == 0 );

	// A removal reads back as the empty slot.
	BeginWrite( msg, storage.data(), static_cast<int>( storage.size() ) );
	CHECK( MSG_WriteDeltaEntityForWireProfile( &msg, &raw, nullptr, qfalse, NETCHAN_WIRE_STVEF_HM ) );
	BeginRead( reader, storage.data(), static_cast<int>( storage.size() ), msg.cursize );
	CHECK( MSG_ReadEntitynum( &reader ) == 7 );
	CHECK( MSG_ReadDeltaEntityForWireProfile( &reader, &raw, &decoded, 7, NETCHAN_WIRE_STVEF_HM ) );
	CHECK( decoded.number == MAX_GENTITIES - 1 && decoded.eType == 0 );

	// Engine-only fields and trajectory types Holomatch lacks are refused.
	entityState_t foreign = raw;
	foreign.pos.trType = TR_QL_ACCEL;
	BeginWrite( msg, storage.data(), static_cast<int>( storage.size() ) );
	CHECK( !MSG_WriteDeltaEntityForWireProfile( &msg, &baseline, &foreign, qfalse, NETCHAN_WIRE_STVEF_HM ) );
	return 0;
}

inline int TestPlayerDeltas() {
	std::array<byte, 1024> storage{};
	msg_t msg{};
	playerState_t sent{};
	sent.commandTime = 123456;
	sent.origin[0] = 12.0f;
	sent.origin[1] = 0.5f;
	sent.pm_time = -3;
	sent.viewheight = -8;
	sent.events[1] = 17;
	sent.stvef.events[0] = 21;
	sent.stvef.eventParms[1] = 200;
	sent.stvef.damageShieldCount = 30;
	sent.stvef.introTime = 99000;
	sent.stvef.rechargeTime = 5;
	sent.stvef.useTime = -2;
	sent.stats[0] = -5;
	sent.persistant[3] = 32767;
	sent.ammo[15] = 250;
	sent.powerups[2] = 0x12345678;
	sent.generic1 = 7; // not part of Holomatch's player state
	BeginWrite( msg, storage.data(), static_cast<int>( storage.size() ) );
	CHECK( MSG_WriteDeltaPlayerstateForWireProfile( &msg, nullptr, &sent, NETCHAN_WIRE_STVEF_HM ) );
	CHECK( !msg.overflowed );

	msg_t reader{};
	playerState_t decoded{};
	BeginRead( reader, storage.data(), static_cast<int>( storage.size() ), msg.cursize );
	CHECK( MSG_ReadDeltaPlayerstateForWireProfile( &reader, nullptr, &decoded, NETCHAN_WIRE_STVEF_HM ) );
	CHECK( reader.bit == msg.bit );
	CHECK( decoded.commandTime == 123456 && decoded.origin[0] == 12.0f && decoded.origin[1] == 0.5f );
	CHECK( decoded.pm_time == -3 && decoded.viewheight == -8 );
	CHECK( decoded.events[1] == 17 && decoded.stvef.events[0] == 21 && decoded.stvef.eventParms[1] == 200 );
	CHECK( decoded.stvef.damageShieldCount == 30 && decoded.stvef.introTime == 99000 );
	// Recharge and use times are module state that is never transmitted.
	CHECK( decoded.stvef.rechargeTime == 0 && decoded.stvef.useTime == 0 );
	CHECK( decoded.stats[0] == -5 && decoded.persistant[3] == 32767 && decoded.ammo[15] == 250 );
	CHECK( decoded.powerups[2] == 0x12345678 );
	CHECK( decoded.generic1 == 0 );

	// Unchanged: 48 clear field flags and four clear array flags.
	BeginWrite( msg, storage.data(), static_cast<int>( storage.size() ) );
	CHECK( MSG_WriteDeltaPlayerstateForWireProfile( &msg, &decoded, &decoded, NETCHAN_WIRE_STVEF_HM ) );
	CHECK( msg.bit == 52 );
	for ( int i = 0; i < 6; ++i ) CHECK( storage[i] == 0 );
	CHECK( ( storage[6] & 0x0f ) == 0 );
	return 0;
}

inline int TestCommands() {
	std::array<byte, 64> storage{};
	msg_t msg{};
	usercmd_t from{}, to{};
	to.serverTime = 100;
	to.angles[1] = 0x1234;
	to.forwardmove = -128;
	to.buttons = 0x81;
	to.weapon = 5;
	BeginWrite( msg, storage.data(), static_cast<int>( storage.size() ) );
	// Holomatch commands are not keyed: any key gives the same bits.
	CHECK( MSG_WriteDeltaUsercmdKeyForWireProfile( &msg, 0x5a5a5a5a, &from, &to, NETCHAN_WIRE_STVEF_HM ) );
	CHECK( msg.bit == 57 );
	CHECK( BytesAre( msg, std::array<unsigned char, 8>{ 0xc9, 0xa4, 0x91, 0x10, 0x90, 0x81, 0x0b, 0x00 } ) );
	msg_t reader{};
	usercmd_t decoded{};
	BeginRead( reader, storage.data(), static_cast<int>( storage.size() ), msg.cursize );
	CHECK( MSG_ReadDeltaUsercmdKeyForWireProfile( &reader, 0, &from, &decoded, NETCHAN_WIRE_STVEF_HM ) );
	// Retail keeps -128.
	CHECK( decoded.serverTime == 100 && decoded.angles[1] == 0x1234 && decoded.forwardmove == -128 );
	CHECK( decoded.buttons == 0x81 && decoded.weapon == 5 );

	// Buttons are one byte on this wire.
	to.buttons = 0x100;
	BeginWrite( msg, storage.data(), static_cast<int>( storage.size() ) );
	CHECK( !MSG_WriteDeltaUsercmdKeyForWireProfile( &msg, 0, &from, &to, NETCHAN_WIRE_STVEF_HM ) );
	return 0;
}

inline int TestSinglePlayerWireIsClosed() {
	std::array<byte, 64> storage{};
	msg_t msg{};
	entityState_t state{};
	usercmd_t command{};
	BeginWrite( msg, storage.data(), static_cast<int>( storage.size() ) );
	CHECK( !MSG_WriteDeltaEntityForWireProfile( &msg, &state, &state, qtrue, NETCHAN_WIRE_STVEF_SP ) );
	CHECK( !MSG_WriteDeltaUsercmdKeyForWireProfile( &msg, 0, &command, &command, NETCHAN_WIRE_STVEF_SP ) );
	return 0;
}

} // namespace stvef_message_tests

inline int TestSTVEFMessageCodecs() {
	using namespace stvef_message_tests;
	if ( const int result = TestAlignment() ) return result;
	if ( const int result = TestEntityDeltas() ) return result;
	if ( const int result = TestPlayerDeltas() ) return result;
	if ( const int result = TestCommands() ) return result;
	if ( const int result = TestSinglePlayerWireIsClosed() ) return result;
	return 0;
}
