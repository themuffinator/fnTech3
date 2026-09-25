/*
===========================================================================
Copyright (C) 2026 fnTech3 contributors

This file is part of fnTech3 and is distributed under the terms of the GNU
General Public License, version 2 or (at your option) any later version.
===========================================================================
*/

/* Heavy Metal: F.A.K.K.2's message bodies against the shape read out of
 * retail fakk2.exe 1.02 (code/compat/fakk2_message_codec.hpp). Nothing here
 * loads a retail file: the tables are the generated ones, and the checks pin
 * the bits a retail reader walks, so a change that reshapes a F.A.K.K.2 body
 * -- or hands it Alice's -- fails here. */

#include "../code/compat/fakk2_message_codec.hpp"
#include "../code/compat/fakk2_convert.hpp"

#include <cstdio>
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

/* LSB-first bits, as fakk2.exe's MSG_WriteBits and MSG_ReadBits (0x00439520)
 * move them; a negative width reads sign-extended. */
class Bits {
public:
	void Write( std::uint32_t value, int bits ) {
		if ( !good ) return;
		const int width = bits < 0 ? -bits : bits;
		if ( width < 1 || width > 32 ) { good = false; return; }
		for ( int i = 0; i < width; ++i ) {
			const std::size_t index = cursor + static_cast<std::size_t>( i );
			if ( index >= data.size() * 8 ) data.push_back( 0 );
			if ( ( value >> i ) & 1u ) data[index >> 3] |= static_cast<std::uint8_t>( 1u << ( index & 7 ) );
		}
		cursor += static_cast<std::size_t>( width );
	}
	std::uint32_t Read( int bits ) {
		if ( !good ) return 0;
		const int width = bits < 0 ? -bits : bits;
		if ( width < 1 || width > 32 || cursor + static_cast<std::size_t>( width ) > data.size() * 8 ) {
			good = false;
			return 0;
		}
		std::uint32_t value = 0;
		for ( int i = 0; i < width; ++i ) {
			const std::size_t index = cursor + static_cast<std::size_t>( i );
			if ( ( data[index >> 3] >> ( index & 7 ) ) & 1u ) value |= 1u << i;
		}
		cursor += static_cast<std::size_t>( width );
		if ( bits < 0 && width < 32 && ( value >> ( width - 1 ) ) & 1u ) value |= ~0u << width;
		return value;
	}
	bool Good() const { return good; }
	bool Fail() { good = false; return false; }
	void Rewind() { cursor = 0; good = true; }
	std::size_t Cursor() const { return cursor; }

	std::vector<std::uint8_t> data;

private:
	std::size_t cursor = 0;
	bool good = true;
};

namespace fm = fntech3::fakk2::message;
namespace am = fntech3::alice::message;

void TestTables() {
	Expect( fm::EntityFields.size() == 73, "seventy-three entity fields (the reader stops at 0x49)" );
	Expect( fm::PlayerFields.size() == 49, "forty-nine player fields (the reader counts down from 0x31)" );
	/* The first forty-seven fields are Alice's, in Alice's order and widths. */
	for ( std::size_t i = 0; i < 47; ++i ) {
		Expect( fm::EntityFields[i].offset == am::EntityFields[i].offset
			&& fm::EntityFields[i].bits == am::EntityFields[i].bits, "the shared prefix of the entity table" );
	}
	/* Then F.A.K.K.2's own: five tags, fifteen angles, no scale, and the tail
	 * at its own offsets, with no second render-flags word. */
	for ( std::size_t i = 0; i < 5; ++i ) {
		Expect( fm::EntityFields[47 + i].offset == offsetof( fm::Entity, bone_tag ) + 4 * i
			&& fm::EntityFields[47 + i].bits == -8, "bone_tag[i], eight signed bits" );
	}
	for ( std::size_t i = 0; i < 15; ++i ) {
		Expect( fm::EntityFields[52 + i].offset == offsetof( fm::Entity, bone_angles ) + 4 * i
			&& fm::EntityFields[52 + i].bits == 0, "bone_angles[i][j], floats" );
	}
	Expect( fm::EntityFields[67].offset == offsetof( fm::Entity, clientNum ) && fm::EntityFields[67].bits == 8, "clientNum" );
	Expect( fm::EntityFields[72].offset == offsetof( fm::Entity, renderfx ) && fm::EntityFields[72].bits == 32, "renderfx ends the table" );
	/* No field reaches the module's own quaternions or the masked block. */
	for ( const auto &field : fm::EntityFields ) {
		const bool quat = field.offset >= offsetof( fm::Entity, bone_quat ) && field.offset < offsetof( fm::Entity, surfaces );
		const bool block = field.offset >= offsetof( fm::Entity, surfaces ) && field.offset < offsetof( fm::Entity, clientNum );
		Expect( !quat && !block && field.offset + 4 <= sizeof( fm::Entity ), "an entity field is a networked member" );
	}
	/* The player table is Alice's but for pm_flags and gravity. */
	for ( std::size_t i = 0; i < 49; ++i ) {
		const bool narrowed = i == 3 || i == 12;
		Expect( fm::PlayerFields[i].offset == am::PlayerFields[i].offset, "the player offsets are Alice's" );
		Expect( narrowed ? fm::PlayerFields[i].bits == 16 && am::PlayerFields[i].bits == 32
			: fm::PlayerFields[i].bits == am::PlayerFields[i].bits, "pm_flags and gravity in sixteen bits, the rest as Alice's" );
	}
	for ( const auto &vector : fm::ChangeVectors ) {
		bool beyond = false;
		for ( std::size_t i = 73; i < 80; ++i ) beyond |= fntech3::alice::message::VectorHas( vector, i );
		Expect( !beyond, "a dictionary vector selects only table fields" );
	}
	Expect( fm::ChangeVectors[4] == am::ChangeVector{ { 0xe0 } }, "entry 4 is the three pos.trBase fields" );
}

void TestNullEntity() {
	fm::Entity fakk2Null;
	fm::InitNullEntity( fakk2Null );
	Expect( fakk2Null.constantLight == 0x00ffffff && fakk2Null.parent == 1023 && fakk2Null.scale == 1.0f
		&& fakk2Null.alpha == 1.0f && fakk2Null.renderfx == 0x10 && fakk2Null.bone_tag[4] == -1, "0x0043a220's values" );
	fntech3_alice_entityState_t converted{}, aliceNull{};
	fntech3::fakk2::convert::EntityToAlice( fakk2Null, converted );
	am::InitNullEntity( aliceNull );
	Expect( std::memcmp( &converted, &aliceNull, sizeof( aliceNull ) ) == 0,
		"F.A.K.K.2's null entity converts to Alice's exactly" );
}

/* A move of the three pos.trBase words is dictionary entry 4: number, not
 * removed, changed, symbol 4, then three 13-bit integral floats and no block. */
void TestDictionaryHitBits() {
	fm::Entity from{}, to{};
	from.number = to.number = 37;
	to.pos.trBase[0] = 10.0f;
	to.pos.trBase[1] = -20.0f;
	to.pos.trBase[2] = 30.0f;
	Bits bits;
	Expect( fm::WriteEntity( bits, &from, &to, false ), "write a dictionary delta" );
	Expect( bits.Cursor() == 10 + 1 + 1 + 5 + 3 * ( 1 + 13 ) + 1, "sixty-one bits" );
	bits.Rewind();
	Expect( bits.Read( 10 ) == 37 && bits.Read( 1 ) == 0 && bits.Read( 1 ) == 1 && bits.Read( 5 ) == 4, "symbol 4" );
	Expect( bits.Read( 1 ) == 0 && bits.Read( 13 ) == 10 + 4096, "the first as a biased integer" );
	bits.Rewind();
	bits.Read( 10 );
	fm::Entity decoded{};
	Expect( fm::ReadEntity( bits, &from, &decoded, 37 ) && std::memcmp( &decoded, &to, sizeof( to ) ) == 0,
		"reads back" );
}

/* A bone controller's angle is not in Alice's place on F.A.K.K.2's wire:
 * the same change encodes differently, so neither reader can take the other's. */
void TestBoneFieldsAreFAKK2s() {
	fm::Entity from{}, to{};
	fm::InitNullEntity( from );
	to = from;
	from.number = to.number = 5;
	to.bone_tag[2] = 7;
	to.bone_angles[2][1] = 0.5f;
	Bits fakk2;
	Expect( fm::WriteEntity( fakk2, &from, &to, false ), "write a controller change" );
	fakk2.Rewind();
	Expect( fakk2.Read( 10 ) == 5 && fakk2.Read( 1 ) == 0 && fakk2.Read( 1 ) == 1 && fakk2.Read( 5 ) == 31, "a raw vector" );
	std::uint32_t vector[10]{};
	for ( int i = 0; i < 9; ++i ) vector[i] = fakk2.Read( 8 );
	vector[9] = fakk2.Read( 1 );
	Expect( ( vector[49 >> 3] >> ( 49 & 7 ) & 1u ) && ( vector[59 >> 3] >> ( 59 & 7 ) & 1u ),
		"bone_tag[2] is field 49 and bone_angles[2][1] field 59" );
	Expect( fakk2.Read( -8 ) == 7u, "the tag in eight signed bits" );
	Expect( fakk2.Read( 1 ) == 1, "a non-integral float goes raw" );

	fntech3_alice_entityState_t aliceFrom{}, aliceTo{};
	fntech3::fakk2::convert::EntityToAlice( from, aliceFrom );
	fntech3::fakk2::convert::EntityToAlice( to, aliceTo );
	Bits alice;
	Expect( am::WriteEntity( alice, &aliceFrom, &aliceTo, false ), "Alice's codec writes the same change" );
	Expect( alice.data != fakk2.data, "and its bits are not F.A.K.K.2's" );
}

void TestEntityRoundTrips() {
	fm::Entity base{};
	fm::InitNullEntity( base );
	base.number = 200;
	fm::Entity next = base;
	next.eType = 3;
	next.eFlags = 0x40;
	next.pos.trType = 2;
	next.pos.trTime = 123456;
	next.pos.trDelta[2] = -300.25f;
	next.netorigin[0] = 1024.0f;
	next.netangles[1] = 90.0f;
	next.loopSound = 44;
	next.loopSoundMinDist = 160.0f;
	next.tag_num = 12;
	next.attach_offset[2] = 8.0f;
	next.modelindex = 301;
	next.anim = 2047;
	next.frame = -1 & 1023;
	next.crossblend_time = -250;
	next.torso_anim = 5;
	for ( int i = 0; i < 5; ++i ) {
		next.bone_tag[i] = i - 2;
		next.bone_angles[i][0] = static_cast<float>( i * 45 );
	}
	next.surfaces[0] = 1;
	next.surfaces[31] = 0x80;
	next.clientNum = 0;
	next.groundEntityNum = 1022;
	next.solid = 0x00ffffff;
	next.scale = 0.75f;
	next.alpha = 0.5f;
	next.renderfx = 0x10 | 0x400;
	/* The module's own quaternions and matrix are never sent: they arrive as
	 * the reader's baseline had them. */
	next.bone_quat[1][3] = 1.0f;
	next.mat[2][2] = 1.0f;
	Bits bits;
	Expect( fm::WriteEntity( bits, &base, &next, false ), "write a wide delta" );
	bits.Rewind();
	Expect( bits.Read( 10 ) == 200, "the number" );
	fm::Entity decoded{};
	Expect( fm::ReadEntity( bits, &base, &decoded, 200 ), "read the wide delta" );
	fm::Entity expected = next;
	std::memcpy( expected.bone_quat, base.bone_quat, sizeof( expected.bone_quat ) );
	std::memcpy( expected.mat, base.mat, sizeof( expected.mat ) );
	Expect( std::memcmp( &decoded, &expected, sizeof( expected ) ) == 0, "every networked member round-trips" );

	Bits unchanged;
	Expect( fm::WriteEntity( unchanged, &next, &next, false ) && unchanged.Cursor() == 0, "an unchanged entity writes nothing" );
	Bits forced;
	Expect( fm::WriteEntity( forced, &next, &next, true ) && forced.Cursor() == 12, "forced: number, kept, no delta" );
	Bits removed;
	Expect( fm::WriteEntity( removed, &next, nullptr, false ) && removed.Cursor() == 11, "a removal" );
	removed.Rewind();
	removed.Read( 10 );
	Expect( fm::ReadEntity( removed, &next, &decoded, 200 ) && decoded.number == 1023, "reads as 1023" );

	Bits truncated;
	Expect( fm::WriteEntity( truncated, &base, &next, false ), "write again" );
	truncated.data.resize( truncated.data.size() / 2 );
	truncated.Rewind();
	truncated.Read( 10 );
	fm::Entity untouched = base;
	Expect( !fm::ReadEntity( truncated, &base, &untouched, 200 )
		&& std::memcmp( &untouched, &base, sizeof( base ) ) == 0, "a short message publishes nothing" );
}

void TestPlayer() {
	fm::Player from{}, to{};
	to.commandTime = 5000;
	to.pm_flags = 0x12345;
	to.gravity = 0x10320;
	to.viewangles[1] = 45.0f;
	to.current_music_mood = 3;
	to.block[0][1] = -1;
	to.block[5][31] = 250;
	Bits bits;
	Expect( fm::WritePlayer( bits, &from, &to ), "write a player delta" );
	bits.Rewind();
	Expect( bits.Read( 1 ) == 1 && bits.Read( 32 ) == 5000, "commandTime" );
	Expect( bits.Read( 1 ) == 0 && bits.Read( 1 ) == 0, "pm_type and bobCycle unchanged" );
	Expect( bits.Read( 1 ) == 1 && bits.Read( 16 ) == 0x2345, "pm_flags in sixteen bits" );
	bits.Rewind();
	fm::Player decoded{};
	Expect( fm::ReadPlayer( bits, &from, &decoded ), "read it back" );
	Expect( decoded.pm_flags == 0x2345 && decoded.gravity == 0x0320, "the high halves do not travel" );
	Expect( decoded.commandTime == 5000 && decoded.viewangles[1] == 45.0f && decoded.block[0][1] == -1
		&& decoded.block[5][31] == 250, "the rest does" );

	Bits alice;
	Expect( am::WritePlayer( alice, &from, &to ), "Alice's codec" );
	Expect( alice.Cursor() == bits.Cursor() + 32, "Alice sends thirty-two more bits for the two words" );
}

} // namespace

int main() {
	TestTables();
	TestNullEntity();
	TestDictionaryHitBits();
	TestBoneFieldsAreFAKK2s();
	TestEntityRoundTrips();
	TestPlayer();
	if ( failures ) {
		std::fprintf( stderr, "%d failure(s)\n", failures );
		return 1;
	}
	std::puts( "fakk2 message codec: ok" );
	return 0;
}
