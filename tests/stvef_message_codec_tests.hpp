/* Elite Force message tests: Holomatch's retail bodies and single player's
 * loopback ones. Included by message_codec_tests.cpp, which supplies CHECK and
 * the Com_Error/Com_Printf stubs.
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

// A value the message cannot hold reads as -1 and leaves the position where it
// was, as retail's readers test before reading. A gamestate recorded in a demo
// ends at its zero byte, so its absent checksum feed must not end the message.
inline int TestMissingValues() {
	std::array<byte, 16> storage{};
	msg_t msg{};
	BeginWrite( msg, storage.data(), static_cast<int>( storage.size() ) );
	MSG_WriteBits( &msg, 0x155, 10 );
	MSG_WriteByte( &msg, 0 );
	CHECK( msg.cursize == 3 );

	msg_t reader{};
	BeginRead( reader, storage.data(), static_cast<int>( storage.size() ), msg.cursize );
	CHECK( MSG_ReadEntitynum( &reader ) == 0x155 );
	CHECK( MSG_ReadByte( &reader ) == 0 );
	const int bit = reader.bit;
	const int readcount = reader.readcount;
	CHECK( MSG_ReadLong( &reader ) == -1 );
	CHECK( MSG_ReadShort( &reader ) == -1 );
	CHECK( MSG_ReadByte( &reader ) == -1 );
	CHECK( reader.bit == bit && reader.readcount == readcount );
	CHECK( reader.readcount <= reader.cursize );

	// A value that fits exactly is read, and the end still reads as -1.
	BeginWrite( msg, storage.data(), static_cast<int>( storage.size() ) );
	MSG_WriteBits( &msg, 0x155, 10 );
	MSG_WriteByte( &msg, 0 );
	MSG_WriteLong( &msg, 0x12345678 );
	BeginRead( reader, storage.data(), static_cast<int>( storage.size() ), msg.cursize );
	CHECK( MSG_ReadEntitynum( &reader ) == 0x155 );
	CHECK( MSG_ReadByte( &reader ) == 0 );
	CHECK( MSG_ReadLong( &reader ) == 0x12345678 );
	CHECK( MSG_ReadByte( &reader ) == -1 );
	CHECK( reader.readcount <= reader.cursize );

	// Other raw-bit profiles keep the sticky past-end state.
	BeginRead( reader, storage.data(), static_cast<int>( storage.size() ), 3 );
	MSG_RawBitstream( &reader );
	CHECK( MSG_ReadEntitynum( &reader ) == 0x155 );
	CHECK( MSG_ReadByte( &reader ) == 0 );
	CHECK( MSG_ReadLong( &reader ) == -1 );
	CHECK( reader.readcount > reader.cursize );
	return 0;
}

inline void BeginPlainRead( msg_t &reader, byte *storage, int size, const msg_t &written ) {
	MSG_Init( &reader, storage, size );
	reader.cursize = written.cursize;
	MSG_BeginReading( &reader );
}

/* Single player's loopback bodies are fnTech3's own; through the message
 * layer, every field the retail records hold arrives exactly, the SP extras
 * included, and a removal publishes entity 1023. */
inline int TestSinglePlayerWire() {
	std::array<byte, 4096> storage{};
	const int size = static_cast<int>( storage.size() );
	msg_t msg{}, reader{};

	entityState_t from{}, to{}, decoded{};
	from.number = to.number = 12;
	to.pos.trType = TR_LINEAR;
	to.pos.trBase[2] = 96.5f;
	to.eFlags = 0x40001;
	to.stvef.modelindex3 = 17;
	to.stvef.torsoAnimTimer = 850;
	MSG_Init( &msg, storage.data(), size );
	CHECK( MSG_WriteDeltaEntityForWireProfile( &msg, &from, &to, qfalse, NETCHAN_WIRE_STVEF_SP ) );
	BeginPlainRead( reader, storage.data(), size, msg );
	CHECK( MSG_ReadEntitynum( &reader ) == 12 );
	CHECK( MSG_ReadDeltaEntityForWireProfile( &reader, &from, &decoded, 12, NETCHAN_WIRE_STVEF_SP ) );
	CHECK( decoded.number == 12 && decoded.pos.trType == TR_LINEAR && decoded.pos.trBase[2] == 96.5f );
	CHECK( decoded.eFlags == 0x40001 && decoded.stvef.modelindex3 == 17 && decoded.stvef.torsoAnimTimer == 850 );

	MSG_Init( &msg, storage.data(), size );
	CHECK( MSG_WriteDeltaEntityForWireProfile( &msg, &to, nullptr, qfalse, NETCHAN_WIRE_STVEF_SP ) );
	BeginPlainRead( reader, storage.data(), size, msg );
	CHECK( MSG_ReadEntitynum( &reader ) == 12 );
	CHECK( MSG_ReadDeltaEntityForWireProfile( &reader, &to, &decoded, 12, NETCHAN_WIRE_STVEF_SP ) );
	CHECK( decoded.number == MAX_GENTITIES - 1 );

	playerState_t player{}, playerDecoded{};
	player.commandTime = 4321;
	player.origin[1] = 12.25f;
	player.stats[0] = 100;
	player.stvef.leanofs = -5;
	player.stvef.borgAdaptHits[31] = 9;
	MSG_Init( &msg, storage.data(), size );
	CHECK( MSG_WriteDeltaPlayerstateForWireProfile( &msg, nullptr, &player, NETCHAN_WIRE_STVEF_SP ) );
	BeginPlainRead( reader, storage.data(), size, msg );
	CHECK( MSG_ReadDeltaPlayerstateForWireProfile( &reader, nullptr, &playerDecoded, NETCHAN_WIRE_STVEF_SP ) );
	CHECK( playerDecoded.commandTime == 4321 && playerDecoded.origin[1] == 12.25f && playerDecoded.stats[0] == 100 );
	CHECK( playerDecoded.stvef.leanofs == -5 && playerDecoded.stvef.borgAdaptHits[31] == 9 );

	// A value a narrow retail field cannot hold is refused, not truncated.
	player.stvef.leanofs = 200;
	MSG_Init( &msg, storage.data(), size );
	CHECK( !MSG_WriteDeltaPlayerstateForWireProfile( &msg, nullptr, &player, NETCHAN_WIRE_STVEF_SP ) );

	usercmd_t command{}, next{}, commandDecoded{};
	next.serverTime = 5000;
	next.buttons = 0x12345678;
	next.forwardmove = -127;
	next.angles[1] = 0x4000;
	MSG_Init( &msg, storage.data(), size );
	CHECK( MSG_WriteDeltaUsercmdKeyForWireProfile( &msg, 0, &command, &next, NETCHAN_WIRE_STVEF_SP ) );
	BeginPlainRead( reader, storage.data(), size, msg );
	CHECK( MSG_ReadDeltaUsercmdKeyForWireProfile( &reader, 0, &command, &commandDecoded, NETCHAN_WIRE_STVEF_SP ) );
	CHECK( commandDecoded.serverTime == 5000 && commandDecoded.buttons == 0x12345678 );
	CHECK( commandDecoded.forwardmove == -127 && commandDecoded.angles[1] == 0x4000 );
	return 0;
}

} // namespace stvef_message_tests

inline int TestSTVEFMessageCodecs() {
	using namespace stvef_message_tests;
	if ( const int result = TestAlignment() ) return result;
	if ( const int result = TestEntityDeltas() ) return result;
	if ( const int result = TestPlayerDeltas() ) return result;
	if ( const int result = TestCommands() ) return result;
	if ( const int result = TestMissingValues() ) return result;
	if ( const int result = TestSinglePlayerWire() ) return result;
	return 0;
}
