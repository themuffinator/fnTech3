/*
===========================================================================
Copyright (C) 2026 fnTech3 contributors

This file is part of fnTech3 and is distributed under the terms of the GNU
General Public License, version 2 or (at your option) any later version.
===========================================================================
*/

/* American McGee's Alice's protocol-1 message bodies, against the shape read
 * out of retail alice.exe in docs/fntech3/2026-09-22-alice-wire.md. Nothing
 * here loads a retail module or replays a retail packet: it exercises the
 * codec against its own reader, and pins the bit counts a retail decoder
 * would need, so a change that quietly reshapes a message fails here. */

#include "../code/compat/alice_message_codec.hpp"
#include "../code/compat/alice_state_adapter.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace {

int failures;

void Expect( bool condition, const char *message ) {
	if ( !condition ) {
		std::fprintf( stderr, "FAIL: %s\n", message );
		++failures;
	}
}

/* A bit sink and source of the shape the codec expects, so the test does not
 * depend on the engine's msg_t. LSB first, which is what the retail writer
 * does. */
class Bits {
public:
	void Write( std::uint32_t value, int bits ) {
		if ( !good ) return;
		const int width = bits < 0 ? -bits : bits;
		if ( width < 1 || width > 32 ) { good = false; return; }
		for ( int i = 0; i < width; ++i ) {
			const std::size_t index = cursor + static_cast<std::size_t>( i );
			if ( index >= data.size() * 8 ) data.push_back( 0 );
			if ( ( value >> i ) & 1u ) {
				data[index >> 3] |= static_cast<std::uint8_t>( 1u << ( index & 7 ) );
			}
		}
		cursor += static_cast<std::size_t>( width );
	}
	std::uint32_t Read( int bits ) {
		if ( !good ) return 0;
		const int width = bits < 0 ? -bits : bits;
		if ( width < 1 || width > 32 || cursor + static_cast<std::size_t>( width )
			> data.size() * 8 ) {
			good = false;
			return 0;
		}
		std::uint32_t value = 0;
		for ( int i = 0; i < width; ++i ) {
			const std::size_t index = cursor + static_cast<std::size_t>( i );
			if ( ( data[index >> 3] >> ( index & 7 ) ) & 1u ) value |= 1u << i;
		}
		cursor += static_cast<std::size_t>( width );
		/* A negative width is a signed field: the retail reader sign-extends
		 * it, and so does the engine's own MSG_ReadBits. */
		if ( bits < 0 && width < 32 && ( value >> ( width - 1 ) ) & 1u ) {
			value |= ~0u << width;
		}
		return value;
	}
	bool Good() const { return good; }
	bool Fail() { good = false; return false; }

	void Rewind() { cursor = 0; good = true; }
	std::size_t Cursor() const { return cursor; }

	std::vector<std::uint8_t> data;
	std::size_t cursor = 0;
	bool good = true;
};

using namespace fntech3::alice::message;

/* A deterministic filler, so a failure is reproducible. */
std::uint32_t Next( std::uint32_t &seed ) {
	seed = seed * 1664525u + 1013904223u;
	return seed;
}

void FillEntity( Entity &entity, std::uint32_t &seed ) {
	auto *bytes = reinterpret_cast<std::uint8_t *>( &entity );
	for ( std::size_t i = 0; i < sizeof( entity ); ++i ) {
		bytes[i] = static_cast<std::uint8_t>( Next( seed ) >> 13 );
	}
	/* Every integer field has to fit the width the table gives it, or the
	 * writer truncates and the reader cannot give it back. Floats are left
	 * as whatever bit pattern the filler made, which exercises the
	 * thirty-two bit path. */
	for ( const Field &field : EntityFields ) {
		if ( !field.bits ) continue;
		const int width = field.bits < 0 ? -field.bits : field.bits;
		std::uint32_t value = Word( entity, field.offset );
		if ( width < 32 ) {
			value &= ( 1u << width ) - 1u;
			if ( field.bits < 0 && ( value >> ( width - 1 ) ) & 1u ) {
				value |= ~0u << width;
			}
		}
		SetWord( entity, field.offset, value );
	}
	entity.number = static_cast<std::int32_t>( Next( seed ) % 1000u );
}

void FillPlayer( Player &player, std::uint32_t &seed ) {
	auto *bytes = reinterpret_cast<std::uint8_t *>( &player );
	for ( std::size_t i = 0; i < sizeof( player ); ++i ) {
		bytes[i] = static_cast<std::uint8_t>( Next( seed ) >> 13 );
	}
	for ( const Field &field : PlayerFields ) {
		if ( !field.bits ) continue;
		const int width = field.bits < 0 ? -field.bits : field.bits;
		std::uint32_t value = Word( player, field.offset );
		if ( width < 32 ) {
			value &= ( 1u << width ) - 1u;
			if ( field.bits < 0 && ( value >> ( width - 1 ) ) & 1u ) {
				value |= ~0u << width;
			}
		}
		SetWord( player, field.offset, value );
	}
	/* The six masked arrays go out sixteen bits at a time and come back as
	 * signed shorts. */
	for ( std::size_t block = 0; block < kPlayerBlocks; ++block ) {
		for ( std::size_t i = 0; i < kPlayerBlockEntries; ++i ) {
			player.block[block][i] =
				static_cast<std::int16_t>( Next( seed ) & 0xffffu );
		}
	}
}

/* Put back everything a delta does not carry, so a round trip is a test of
 * the codec rather than of the filler. */
template<class Record, class Fields>
void RestoreOutside( Record &to, const Record &from, const Fields &fields,
	std::size_t reach, const bool *networked ) {
	auto *out = reinterpret_cast<std::uint8_t *>( &to );
	const auto *in = reinterpret_cast<const std::uint8_t *>( &from );
	(void)fields;
	for ( std::size_t word = 0; word * 4 < sizeof( Record ); ++word ) {
		if ( word * 4 < reach && networked[word] ) continue;
		std::memcpy( out + word * 4, in + word * 4, 4 );
	}
}

void Restore( Entity &to, const Entity &from ) {
	bool networked[512 / 4] = {};
	for ( const Field &field : EntityFields ) networked[field.offset / 4] = true;
	/* The thirty-two byte block is carried too, under its own mask. */
	for ( std::size_t i = 396; i < 428; i += 4 ) networked[i / 4] = true;
	RestoreOutside( to, from, EntityFields, sizeof( Entity ), networked );
}

void Restore( Player &to, const Player &from ) {
	bool networked[1048 / 4] = {};
	for ( const Field &field : PlayerFields ) networked[field.offset / 4] = true;
	for ( std::size_t i = 176; i < 944; i += 4 ) networked[i / 4] = true;
	RestoreOutside( to, from, PlayerFields, sizeof( Player ), networked );
}

void CheckTables() {
	Expect( EntityFields.size() == 79, "seventy-nine entity fields" );
	Expect( PlayerFields.size() == 49, "forty-nine player fields" );
	Expect( ChangeVectors.size() == 31, "thirty-one dictionary vectors" );
	Expect( sizeof( Entity ) == 512, "the entity state is 512 bytes" );
	Expect( sizeof( Player ) == 1048, "the player state is 1,048 bytes" );
	Expect( sizeof( Command ) == 20, "the user command is 20 bytes" );

	/* Every field has to address a whole word inside the part of the record
	 * its table covers, and no two may name the same word. */
	bool aligned = true, inside = true, unique = true;
	std::vector<std::size_t> seen;
	for ( const Field &field : EntityFields ) {
		aligned = aligned && ( field.offset & 3 ) == 0;
		inside = inside && field.offset + 4 <= 456;
		for ( std::size_t other : seen ) unique = unique && other != field.offset;
		seen.push_back( field.offset );
	}
	Expect( aligned && inside && unique,
		"entity fields are aligned, unique and inside the table's reach" );
	seen.clear();
	aligned = inside = unique = true;
	for ( const Field &field : PlayerFields ) {
		aligned = aligned && ( field.offset & 3 ) == 0;
		inside = inside && field.offset + 4 <= 1044;
		for ( std::size_t other : seen ) unique = unique && other != field.offset;
		seen.push_back( field.offset );
	}
	Expect( aligned && inside && unique,
		"player fields are aligned, unique and inside the table's reach" );

	/* No dictionary vector may select a field that does not exist, and none
	 * may repeat another: a repeat would make one of the two unreachable. */
	bool bounded = true, distinct = true;
	for ( std::size_t i = 0; i < ChangeVectors.size(); ++i ) {
		for ( std::size_t bit = EntityFields.size(); bit < kVectorBytes * 8; ++bit ) {
			bounded = bounded && !VectorHas( ChangeVectors[i], bit );
		}
		for ( std::size_t j = i + 1; j < ChangeVectors.size(); ++j ) {
			distinct = distinct && !( ChangeVectors[i] == ChangeVectors[j] );
		}
	}
	Expect( bounded, "no dictionary vector selects a field past the table" );
	Expect( distinct, "no dictionary vector repeats another" );
}

void CheckEntityRoundTrip() {
	std::uint32_t seed = 0x9e3779b9u;
	for ( int trial = 0; trial < 64; ++trial ) {
		Entity from{}, to{}, decoded{};
		FillEntity( from, seed );
		/* A delta only carries what the table covers, so the two records
		 * have to agree everywhere else: the entity state's own tail and
		 * the three spans the table skips never cross a message. */
		to = from;
		FillEntity( to, seed );
		Restore( to, from );

		Bits io;
		Expect( WriteEntity( io, &from, &to, false ), "write an entity delta" );
		io.Rewind();
		Expect( io.Read( kEntityNumberBits ) == static_cast<std::uint32_t>( to.number ),
			"the entity number leads the delta" );
		Expect( ReadEntity( io, &from, &decoded, to.number ), "read it back" );
		Expect( std::memcmp( &to, &decoded, sizeof( to ) ) == 0,
			"every byte of the entity survives a round trip" );
	}
}

void CheckEntityShapes() {
	Entity base{};
	base.number = 7;

	/* Nothing changed and nothing forced: nothing at all, as retail's
	 * encoder writes (0x00445ef5..0x00445efb). */
	{
		Bits io;
		Expect( WriteEntity( io, &base, &base, false ), "write an unchanged entity" );
		Expect( io.Cursor() == 0, "an unchanged, unforced entity writes nothing" );
	}

	/* Nothing changed but forced: number, not-removed, no-delta. */
	{
		Bits io;
		Expect( WriteEntity( io, &base, &base, true ), "write an unchanged, forced entity" );
		Expect( io.Cursor() == kEntityNumberBits + 2,
			"an unchanged, forced entity costs its number and two bits" );
		Entity decoded{};
		io.Rewind();
		io.Read( kEntityNumberBits );
		Expect( ReadEntity( io, &base, &decoded, 7 ) && decoded.number == 7,
			"an unchanged entity reads back as itself" );
	}

	/* A removal is the number and one bit, and publishes 1023. */
	{
		Bits io;
		Expect( WriteEntity( io, &base, nullptr, false ), "write a removal" );
		Expect( io.Cursor() == kEntityNumberBits + 1,
			"a removal costs its number and one bit" );
		Entity decoded{};
		io.Rewind();
		io.Read( kEntityNumberBits );
		Expect( ReadEntity( io, &base, &decoded, 7 ) && decoded.number == 1023,
			"a removal publishes the reserved number" );
	}

	/* A change that is in the dictionary costs five bits, not seventy-nine.
	 * Vector zero selects the three position bases and two angle bases. */
	{
		Entity moved = base;
		for ( std::size_t i = 0; i < EntityFields.size(); ++i ) {
			if ( VectorHas( ChangeVectors[0], i ) ) {
				SetWord( moved, EntityFields[i].offset, FloatWord( 3.0f ) );
			}
		}
		Bits io;
		Expect( WriteEntity( io, &base, &moved, false ), "write a dictionary delta" );
		io.Rewind();
		io.Read( kEntityNumberBits );
		Expect( io.Read( 1 ) == 0 && io.Read( 1 ) == 1, "not removed, has a delta" );
		Expect( io.Read( 5 ) == 0, "the dictionary index rather than the vector" );
		Entity decoded{};
		io.Rewind();
		io.Read( kEntityNumberBits );
		Expect( ReadEntity( io, &base, &decoded, 7 ) &&
			std::memcmp( &moved, &decoded, sizeof( moved ) ) == 0,
			"a dictionary delta reads back exactly" );
	}

	/* A change the dictionary does not have escapes to the raw vector. */
	{
		Entity odd = base;
		odd.renderfx2 = 5;
		Bits io;
		Expect( WriteEntity( io, &base, &odd, false ), "write an escaped delta" );
		io.Rewind();
		io.Read( kEntityNumberBits );
		io.Read( 1 );
		io.Read( 1 );
		Expect( io.Read( 5 ) == kRawVectorSymbol, "symbol 31 escapes" );
		Entity decoded{};
		io.Rewind();
		io.Read( kEntityNumberBits );
		Expect( ReadEntity( io, &base, &decoded, 7 ) &&
			std::memcmp( &odd, &decoded, sizeof( odd ) ) == 0,
			"an escaped delta reads back exactly" );
	}

	/* A change only in the thirty-two byte block still sends a delta, with an
	 * all-zero field vector: the retail writer's quiet case needs both to be
	 * empty. */
	{
		Entity tagged = base;
		tagged.tagged[5] = 0xab;
		Bits io;
		Expect( WriteEntity( io, &base, &tagged, false ), "write a block-only delta" );
		io.Rewind();
		io.Read( kEntityNumberBits );
		Expect( io.Read( 1 ) == 0 && io.Read( 1 ) == 1,
			"a block-only change is still a delta" );
		Entity decoded{};
		io.Rewind();
		io.Read( kEntityNumberBits );
		Expect( ReadEntity( io, &base, &decoded, 7 ) && decoded.tagged[5] == 0xab,
			"the block crosses" );
	}

	/* An entity number outside the world is refused rather than truncated. */
	{
		Entity bad = base;
		bad.number = 1024;
		Bits io;
		Expect( !WriteEntity( io, &base, &bad, true ), "refuse entity 1024" );
		Entity decoded{};
		Bits reader;
		Expect( !ReadEntity( reader, &base, &decoded, -1 ), "refuse number -1" );
	}
}

void CheckPlayerRoundTrip() {
	std::uint32_t seed = 0x12345678u;
	for ( int trial = 0; trial < 32; ++trial ) {
		Player from{}, to{}, decoded{};
		FillPlayer( from, seed );
		to = from;
		FillPlayer( to, seed );
		Restore( to, from );

		Bits io;
		Expect( WritePlayer( io, &from, &to ), "write a player delta" );
		io.Rewind();
		Expect( ReadPlayer( io, &from, &decoded ), "read it back" );
		Expect( std::memcmp( &to, &decoded, sizeof( to ) ) == 0,
			"every networked byte of the player state survives a round trip" );
	}
}

void CheckPlayerShapes() {
	Player base{};
	/* Nothing changed: one bit a field and one a block. */
	Bits io;
	Expect( WritePlayer( io, &base, &base ), "write an unchanged player" );
	Expect( io.Cursor() == PlayerFields.size() + kPlayerBlocks,
		"an unchanged player state costs one bit a field and one a block" );

	/* One changed block sends its mask and one value. */
	Player moved = base;
	moved.block[2][9] = 1234;
	Bits second;
	Expect( WritePlayer( second, &base, &moved ), "write a block change" );
	Player decoded{};
	second.Rewind();
	Expect( ReadPlayer( second, &base, &decoded ), "read the block change" );
	Expect( decoded.block[2][9] == 1234 && decoded.block[2][8] == 0,
		"only the changed entry crosses" );

	/* A negative value crosses as itself: the reader sign-extends its sixteen
	 * bits as retail's MSG_ReadShort does (0x00445920), so the -1 the game
	 * puts in the active weapon's slot is -1 on the client, not 65,535. */
	Player unarmed = base;
	unarmed.block[1][3] = -1;
	unarmed.block[0][0] = -40;
	Bits third;
	Expect( WritePlayer( third, &base, &unarmed ), "write negative block values" );
	Player signedDecoded{};
	third.Rewind();
	Expect( ReadPlayer( third, &base, &signedDecoded ), "read negative block values" );
	Expect( signedDecoded.block[1][3] == -1 && signedDecoded.block[0][0] == -40,
		"a block value is a signed short" );
}

void CheckCommandRoundTrip() {
	std::uint32_t seed = 0xfeedbeefu;
	for ( int trial = 0; trial < 64; ++trial ) {
		Command from{}, to{}, decoded{};
		auto fill = [&]( Command &command, bool near ) {
			command.serverTime = static_cast<std::int32_t>( Next( seed ) );
			command.buttons = static_cast<std::uint8_t>( Next( seed ) );
			command.unnamed_6 = static_cast<std::uint8_t>( Next( seed ) );
			for ( int i = 0; i < 3; ++i ) {
				command.angles[i] = static_cast<std::int16_t>( Next( seed ) );
				command.move[i] = static_cast<std::int8_t>( Next( seed ) );
			}
			if ( near ) command.serverTime = from.serverTime + 17;
		};
		fill( from, false );
		fill( to, trial % 2 == 0 );

		Bits io;
		Expect( WriteCommand( io, &from, &to ), "write a command" );
		io.Rewind();
		Expect( ReadCommand( io, &from, &decoded ), "read it back" );
		Expect( std::memcmp( &to, &decoded, sizeof( to ) ) == 0,
			"every command byte survives a round trip" );
	}

	/* A step inside a byte costs nine bits, and one outside costs
	 * thirty-three: the retail writer chooses between them on the step. */
	Command from{}, near{}, far{};
	near.serverTime = 200;
	far.serverTime = 100000;
	Bits close, distant;
	Expect( WriteCommand( close, &from, &near ), "write a near command" );
	Expect( WriteCommand( distant, &from, &far ), "write a far command" );
	Expect( close.Cursor() == 9u + 8u, "a near time costs a flag and a byte" );
	Expect( distant.Cursor() == 33u + 8u, "a far time costs a flag and a long" );
}

/* The engine's command crossing into Alice's twenty bytes. The retail
 * builder stores forward, right and up at +14, +15 and +16 and the buttons
 * at +5; +4, +7 and +17..+19 are never written or sent. Nothing this engine
 * fills for another title -- a weapon, a field of view -- may reach byte 6. */
void CheckCommandAdapter() {
	usercmd_t internal{};
	internal.serverTime = 123456;
	internal.angles[0] = 0x1234;
	internal.angles[1] = 0xfffb;
	internal.angles[2] = 0x7fff;
	internal.buttons = 0x80 | 0x08 | 0x01;
	internal.forwardmove = 127;
	internal.rightmove = -128;
	internal.upmove = -5;
	internal.weapon = 9;
	internal.weaponPrimary = 3;
	internal.fov = 90;

	Command external;
	std::memset( &external, 0xcc, sizeof( external ) );
	Expect( fntech3::alice::state::FromInternal( internal, external ),
		"the engine's command converts" );
	const auto *bytes = reinterpret_cast<const std::uint8_t *>( &external );
	Expect( bytes[14] == 127 && bytes[15] == 0x80 && bytes[16] == 0xfb,
		"forward, right and up land at bytes 14, 15 and 16" );
	Expect( bytes[5] == 0x89, "the buttons land at byte 5" );
	Expect( bytes[4] == 0 && bytes[7] == 0 && bytes[17] == 0 && bytes[18] == 0
		&& bytes[19] == 0, "the bytes retail never writes stay zero" );
	Expect( bytes[6] == 0, "byte 6 is not filled from another title's weapon" );
	Expect( external.serverTime == 123456 && external.angles[0] == 0x1234
		&& external.angles[1] == -5 && external.angles[2] == 0x7fff,
		"the time and the three angles cross" );

	/* Over the wire and back into the engine's own record. */
	const Command from{};
	Bits io;
	Expect( WriteCommand( io, &from, &external ), "write the converted command" );
	io.Rewind();
	Command decoded{};
	Expect( ReadCommand( io, &from, &decoded ), "read it back" );
	Expect( std::memcmp( &decoded, &external, sizeof( decoded ) ) == 0,
		"the converted command survives the wire" );

	usercmd_t back{};
	Expect( fntech3::alice::state::ToInternal( decoded, back ), "and converts back" );
	Expect( back.forwardmove == 127 && back.rightmove == -128 && back.upmove == -5,
		"movement comes back as forward, right and up" );
	Expect( back.buttons == 0x89 && back.serverTime == 123456
		&& back.angles[1] == -5, "the rest of the command comes back" );
	Expect( back.weapon == 0 && back.fov == 0,
		"nothing Alice does not carry is invented on the way in" );

	/* The byte at +6 is carried both ways as it is. */
	external.unnamed_6 = 0x5a;
	Expect( fntech3::alice::state::ToInternal( external, back )
		&& back.alice.unnamed == 0x5a, "byte 6 crosses in" );
	Command again;
	Expect( fntech3::alice::state::FromInternal( back, again )
		&& again.unnamed_6 == 0x5a, "and out" );
}

/* A float that is integral and in range goes as thirteen biased bits; one
 * that is not goes as thirty-two. Both have to come back bit for bit. */
void CheckFloatEncoding() {
	const float values[] = { 0.0f, 1.0f, -1.0f, 4095.0f, -4096.0f, 4096.0f,
		-4097.0f, 0.5f, -0.5f, 1e30f, -1e30f, 123456.0f };
	for ( float value : values ) {
		Bits io;
		WriteScalar( io, FloatWord( value ), 0 );
		const std::size_t cost = io.Cursor();
		io.Rewind();
		const std::uint32_t back = ReadScalar( io, 0 );
		float decoded;
		std::memcpy( &decoded, &back, 4 );
		Expect( std::memcmp( &value, &decoded, 4 ) == 0,
			"a float survives its encoding" );
		const bool compact = value >= -4096.0f && value < 4096.0f &&
			static_cast<float>( static_cast<int>( value ) ) == value;
		Expect( cost == ( compact ? 14u : 33u ),
			"a float costs thirteen bits when it is integral and in range" );
	}
	/* Neither a NaN nor an infinity may reach the compact path. */
	const std::uint32_t nan = 0x7fc00000u, inf = 0x7f800000u;
	for ( std::uint32_t pattern : { nan, inf } ) {
		Bits io;
		WriteScalar( io, pattern, 0 );
		Expect( io.Cursor() == 33u, "a NaN or an infinity goes out raw" );
		io.Rewind();
		Expect( ReadScalar( io, 0 ) == pattern, "and comes back unchanged" );
	}
}

/* A truncated message must fail rather than publish half a record. */
void CheckTruncation() {
	std::uint32_t seed = 7u;
	Entity from{}, to{};
	FillEntity( from, seed );
	FillEntity( to, seed );
	to.number = from.number;
	Bits full;
	Expect( WriteEntity( full, &from, &to, true ), "write a full delta" );
	for ( std::size_t cut = 1; cut < full.data.size(); cut += 3 ) {
		Bits partial;
		partial.data.assign( full.data.begin(), full.data.begin()
			+ static_cast<std::ptrdiff_t>( cut ) );
		partial.Read( kEntityNumberBits );
		Entity decoded{};
		const Entity guard{};
		if ( !ReadEntity( partial, &from, &decoded, to.number ) ) {
			Expect( std::memcmp( &decoded, &guard, sizeof( guard ) ) == 0,
				"a refused read publishes nothing" );
		}
	}
}

/* The snapshot's sound block, with a record of the tests' own that has the
 * members the codec names. */
struct TestSound {
	float origin[3];
	int entityNumber, channel, soundIndex;
	float volume, minDist;
	int stop, hasOrigin;
};

void CheckSounds() {
	Bits none;
	Expect( WriteSounds( none, static_cast<const TestSound *>( nullptr ), 0 ), "an empty block writes" );
	Expect( none.Cursor() == 1u, "an empty block is one presence bit" );
	none.Rewind();
	TestSound sink[kMaxSnapshotSounds];
	int count = -1;
	Expect( ReadSounds( none, sink, count ) && count == 0, "and reads back empty" );

	const TestSound sent[3] = {
		{ { 0, 0, 0 }, 17, 3, 200, -1.0f, -1.0f, 0, 0 },
		{ { 1.5f, -2.0f, 1024.25f }, 1023, 63, 255, 0.5f, 160.0f, 0, 1 },
		{ { 0, 0, 0 }, 42, 6, 0, -1.0f, -1.0f, 1, 0 },
	};
	Bits io;
	Expect( WriteSounds( io, sent, 3 ), "a block of three writes" );
	/* 1 + 6; a start with no origin and both defaults is 1 + 1 + 10 + 6 + 8
	 * + 1 + 1 = 28; one with an origin and both values adds 96 + 64; a stop
	 * is 1 + 10 + 6 = 17. */
	Expect( io.Cursor() == 7u + 28u + 28u + 96u + 64u + 17u, "each sound costs what the retail block costs" );
	io.Rewind();
	count = 0;
	Expect( ReadSounds( io, sink, count ) && count == 3, "the block reads back" );
	Expect( sink[0].entityNumber == 17 && sink[0].channel == 3 && sink[0].soundIndex == 200
		&& sink[0].volume == -1.0f && sink[0].minDist == -1.0f && !sink[0].hasOrigin && !sink[0].stop,
		"a start without an origin keeps its defaults" );
	Expect( sink[1].hasOrigin && sink[1].origin[2] == 1024.25f && sink[1].volume == 0.5f
		&& sink[1].minDist == 160.0f && sink[1].entityNumber == 1023 && sink[1].channel == 63,
		"a start with an origin carries its floats raw" );
	Expect( sink[2].stop && sink[2].entityNumber == 42 && sink[2].channel == 6, "a stop names its entity and channel" );

	/* More than a snapshot holds is refused both ways. */
	TestSound many[kMaxSnapshotSounds + 1] = {};
	Bits over;
	Expect( !WriteSounds( over, many, kMaxSnapshotSounds + 1 ), "a writer refuses more than 32" );
	Bits forged;
	forged.Write( 1, 1 );
	forged.Write( 33, 6 );
	forged.Rewind();
	Expect( !ReadSounds( forged, sink, count ) && count == 0, "a reader refuses a count past 32" );

	/* A block cut short fails without claiming a count. */
	Bits full;
	WriteSounds( full, sent, 3 );
	for ( std::size_t cut = 1; cut < full.data.size(); ++cut ) {
		Bits partial;
		partial.data.assign( full.data.begin(), full.data.begin() + static_cast<std::ptrdiff_t>( cut ) );
		count = -1;
		Expect( !ReadSounds( partial, sink, count ) && count == 0, "a truncated block is refused" );
	}
}

/* The members retail's reader derives after a delta (0x004463e5..0x0044644a),
 * which the client game reads and nothing networks. */
void CheckReaderTail() {
	using namespace fntech3::alice::state;
	const auto near = []( float a, float b ) { return std::fabs( a - b ) < 1e-6f; };

	float quat[4];
	const float zero[3] = { 0.0f, 0.0f, 0.0f };
	reader::EulerToQuat( zero, quat );
	Expect( quat[0] == 0.0f && quat[1] == 0.0f && quat[2] == 0.0f && quat[3] == 1.0f,
		"zero angles are the identity" );

	/* The axis rows are forward, left and up, so the quaternion is the
	 * conjugate of the textbook one: a yaw of 90 turns about -z. */
	const float yaw90[3] = { 0.0f, 90.0f, 0.0f };
	float axis[9];
	reader::AnglesToAxis( yaw90, axis );
	Expect( near( axis[1], 1.0f ) && near( axis[3], -1.0f ) && near( axis[8], 1.0f ),
		"yaw 90 faces +y with left along -x" );
	reader::EulerToQuat( yaw90, quat );
	Expect( near( quat[0], 0.0f ) && near( quat[1], 0.0f ) && near( quat[2], -0.70710678f )
		&& near( quat[3], 0.70710678f ), "yaw 90 takes the positive-trace form" );

	/* A yaw of 180 has a negative trace, so z leads. */
	const float yaw180[3] = { 0.0f, 180.0f, 0.0f };
	reader::EulerToQuat( yaw180, quat );
	Expect( near( quat[0], 0.0f ) && near( quat[1], 0.0f ) && near( quat[2], 1.0f )
		&& near( quat[3], 0.0f ), "yaw 180 takes the largest-diagonal form" );

	/* Pitch leads when it dominates: pitch 180 turns about y. */
	const float pitch180[3] = { 180.0f, 0.0f, 0.0f };
	reader::EulerToQuat( pitch180, quat );
	Expect( near( quat[0], 0.0f ) && near( std::fabs( quat[1] ), 1.0f ) && near( quat[2], 0.0f )
		&& near( quat[3], 0.0f ), "pitch 180 leads with y" );

	/* A whole record: the origin and angles are the net ones word for word, the
	 * quaternion follows them, and only tagged bone modifiers get one. */
	entityState_t internal{};
	internal.number = 12;
	fntech3_alice_entityState_t module{};
	module.number = 12;
	module.netorigin[0] = 64.0f;
	module.netorigin[1] = -32.5f;
	module.netorigin[2] = 8.25f;
	module.netangles[1] = 90.0f;
	module.bonemod[0].tag = 3;
	module.bonemod[0].angles[1] = 180.0f;
	module.bonemod[1].tag = -1;
	module.bonemod[1].angles[1] = 90.0f;
	module.ownerNum = 77;
	Expect( ToInternal( module, internal ), "the record converts" );
	fntech3_alice_entityState_t client;
	std::memset( &client, 0xcd, sizeof( client ) );
	ForClientGame( internal, client );
	Expect( std::memcmp( client.origin, module.netorigin, sizeof( client.origin ) ) == 0,
		"the origin is the net origin" );
	Expect( std::memcmp( client.angles_unnamed, module.netangles, sizeof( client.angles_unnamed ) ) == 0,
		"the angles are the net angles" );
	float entityQuat[4];
	std::memcpy( entityQuat, client.unnamed, sizeof( entityQuat ) );
	Expect( near( entityQuat[2], -0.70710678f ) && near( entityQuat[3], 0.70710678f ),
		"the entity's quaternion follows its angles" );
	float boneQuat[4];
	std::memcpy( boneQuat, client.bonemod[0].unnamed, sizeof( boneQuat ) );
	Expect( near( boneQuat[2], 1.0f ), "a tagged bone modifier gets its quaternion" );
	const std::int32_t untouched[4] = {};
	Expect( std::memcmp( client.bonemod[1].unnamed, untouched, sizeof( untouched ) ) == 0,
		"an untagged one does not" );
	Expect( client.ownerNum == 0 && client.axis[0][0] == 0.0f,
		"members nothing networks or derives start at zero" );
	Expect( client.number == 12 && client.netorigin[1] == -32.5f, "the networked members cross" );
}

} // namespace

int main() {
	CheckTables();
	CheckEntityRoundTrip();
	CheckEntityShapes();
	CheckPlayerRoundTrip();
	CheckPlayerShapes();
	CheckCommandRoundTrip();
	CheckCommandAdapter();
	CheckFloatEncoding();
	CheckTruncation();
	CheckSounds();
	CheckReaderTail();

	if ( failures ) {
		std::fprintf( stderr, "%d Alice message codec checks failed\n", failures );
		return 1;
	}
	std::printf( "Alice message codec tests passed\n" );
	return 0;
}
