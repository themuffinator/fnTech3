/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#include "../code/compat/jk2_message_codec.hpp"
#include "../code/compat/jk2_state_adapter.hpp"
#include "fixtures/jk2_wire_vectors.h"

#include <cstdio>
#include <limits>
#include <vector>

namespace {
int failures;
void Expect( bool value, const char *message ) {
	if ( !value ) { std::fprintf( stderr, "FAIL: %s\n", message ); ++failures; }
}

/* Body-only IO, independent of msg.cpp. The byte fixtures were emitted by
 * retail engine instructions, not by this writer or a related source codec. */
struct Bits {
	std::vector<std::uint8_t> data;
	std::size_t cursor = 0, limit = 1u << 20;
	bool good = true;
	bool Good() const { return good; }
	bool Fail() { good = false; return false; }
	void Write( std::uint32_t value, int bits ) {
		if ( !good ) return;
		const int width = bits < 0 ? -bits : bits;
		if ( width < 1 || width > 32 || cursor + width > limit ) { Fail(); return; }
		if ( data.size() * 8 < cursor + width ) data.resize( ( cursor + width + 7 ) / 8 );
		for ( int i = 0; i < width; ++i )
			data[( cursor + i ) >> 3] |= std::uint8_t( ( ( value >> i ) & 1u ) << ( ( cursor + i ) & 7 ) );
		cursor += width;
	}
	std::uint32_t Read( int bits ) {
		if ( !good ) return 0;
		const int width = bits < 0 ? -bits : bits;
		if ( width < 1 || width > 32 || cursor + width > limit ||
			cursor + width > data.size() * 8 ) { Fail(); return 0; }
		std::uint32_t value = 0;
		for ( int i = 0; i < width; ++i )
			value |= std::uint32_t( ( data[( cursor + i ) >> 3] >> ( ( cursor + i ) & 7 ) ) & 1u ) << i;
		cursor += width;
		if ( bits < 0 && width < 32 && ( value & ( 1u << ( width - 1 ) ) ) ) value |= ~0u << width;
		return value;
	}
	void Rewind() { limit = cursor; cursor = 0; good = true; }
};

using namespace fntech3::jk2::message;

template<class T> T Patched( const jk2_fixture::WordPatch *patches, unsigned count ) {
	T object{};
	for ( unsigned i = 0; i < count; ++i ) SetWord( object, patches[i].offset, patches[i].value );
	return object;
}

template<class T, class Write, class Read> void Fixture( const jk2_fixture::Vector &vector,
	Write write, Read read, int numberBits = 0 ) {
	const auto from = Patched<T>( vector.baseline, vector.baselineCount );
	const auto sent = Patched<T>( vector.sent, vector.sentCount );
	const auto expected = Patched<T>( vector.decoded, vector.decodedCount );
	Bits bits;
	Expect( write( bits, &from, &sent ), vector.name );
	Expect( bits.cursor == vector.wireBits && bits.data.size() == vector.wireBytes &&
		std::memcmp( bits.data.data(), vector.wire, vector.wireBytes ) == 0, "retail writer byte fixture" );
	bits.Rewind();
	if ( numberBits ) (void)bits.Read( numberBits );
	T decoded{};
	Expect( read( bits, &from, &decoded ), vector.name );
	Expect( std::memcmp( &decoded, &expected, sizeof( decoded ) ) == 0, "retail reader record fixture" );
	Expect( bits.cursor == vector.wireBits, "complete fixture consumed" );
	/* Every incomplete prefix must fail without publishing partial state. */
	for ( std::size_t cut = numberBits; cut < vector.wireBits; ++cut ) {
		Bits truncated;
		truncated.data.assign( vector.wire, vector.wire + vector.wireBytes );
		truncated.limit = cut;
		truncated.cursor = numberBits;
		T unchanged;
		std::memset( &unchanged, 0xa5, sizeof( unchanged ) );
		const T guard = unchanged;
		Expect( !read( truncated, &from, &unchanged ), "truncated body rejected" );
		Expect( std::memcmp( &unchanged, &guard, sizeof( unchanged ) ) == 0, "failed read is transactional" );
	}
}

void RetailFixtures() {
	for ( const auto &vector : jk2_fixture::Vectors ) {
		if ( vector.kind == jk2_fixture::Kind::Entity ) {
			const auto sent = Patched<Entity>( vector.sent, vector.sentCount );
			Fixture<Entity>( vector,
				[]( auto &io, auto *from, auto *to ) { return WriteEntity( io, from, to, true ); },
				[&]( auto &io, auto *from, auto *to ) { return ReadEntity( io, from, to, sent.number ); }, 10 );
		} else if ( vector.kind == jk2_fixture::Kind::Player ) {
			Fixture<Player>( vector,
				[]( auto &io, auto *from, auto *to ) { return WritePlayer( io, from, to ); },
				[]( auto &io, auto *from, auto *to ) { return ReadPlayer( io, from, to ); } );
		} else {
			Fixture<Command>( vector,
				[&]( auto &io, auto *from, auto *to ) { return WriteCommand( io, vector.key, from, to ); },
				[&]( auto &io, auto *from, auto *to ) { return ReadCommand( io, vector.key, from, to ); } );
		}
	}
}

std::uint32_t Encodable( const Field &field ) {
	if ( !field.bits ) return FloatWord( 123.25f );
	const auto width = field.bits < 0 ? -field.bits : field.bits;
	if ( field.bits < 0 ) return std::uint32_t( -3 );
	return width == 32 ? 0x89abcdefu : ( ( 1u << width ) - 1u );
}

void EveryWireField() {
	for ( const auto &field : EntityFields ) {
		Entity from{}, sent{}, decoded{};
		sent.number = 91;
		SetWord( sent, field.offset, Encodable( field ) );
		Bits bits;
		Expect( WriteEntity( bits, &from, &sent, true ), "every entity field writes" );
		bits.Rewind();
		Expect( bits.Read( 10 ) == 91 && ReadEntity( bits, &from, &decoded, 91 ), "every entity field reads" );
		Expect( std::memcmp( &sent, &decoded, sizeof( sent ) ) == 0, "entity field survives" );
	}
	for ( const auto &field : PlayerFields ) {
		Player from{}, sent{}, decoded{};
		/* These unknown bytes must stay with the baseline, without becoming
		 * new delta fields and without getting cleared during a valid read. */
		from.unnamed_192[3] = 0x7a;
		from.unnamed_992[17] = 0x3e;
		sent = from;
		SetWord( sent, field.offset, Encodable( field ) );
		Bits bits;
		Expect( WritePlayer( bits, &from, &sent ), "every player field writes" );
		bits.Rewind();
		Expect( ReadPlayer( bits, &from, &decoded ), "every player field reads" );
		Expect( std::memcmp( &sent, &decoded, sizeof( sent ) ) == 0, "player field and unknown bytes survive" );
	}
	// Build and compare exceptional values as words: the engine's fast-math
	// flags must not let the fixture itself discard their payloads.
	const std::uint32_t values[] = { 0xc5800000u, 0x457ff000u, 0x45800000u, 0xc5800800u,
		0, 0x80000000u, 0x3fa00000u, 0x7f800000u, 0xff800000u, 0x7fc00000u };
	for ( const auto value : values ) {
		Entity from{}, sent{}, decoded{};
		from.pos.trBase[0] = 4.0f;
		sent.number = 11;
		SetWord( sent, offsetof( Entity, pos.trBase ), value );
		Bits bits;
		Expect( WriteEntity( bits, &from, &sent, true ), "float writes safely" );
		bits.Rewind();
		(void)bits.Read( 10 );
		Expect( ReadEntity( bits, &from, &decoded, 11 ), "float reads safely" );
		Expect( Word( decoded, offsetof( Entity, pos.trBase ) ) ==
			( ( value & 0x7fffffffu ) ? value : 0 ), "float exact or zero-normalized" );
	}
}

void MalformedAndLimits() {
	Bits bits;
	Entity entity{};
	entity.number = -1;
	Expect( !WriteEntity( bits, nullptr, &entity, true ) && bits.cursor == 0, "negative entity number rejected" );
	bits = {};
	entity.number = MaxEntities;
	Expect( !WriteEntity( bits, nullptr, &entity, true ) && bits.cursor == 0, "oversized entity number rejected" );
	bits = {};
	entity.number = 9;
	Expect( WriteEntity( bits, &entity, nullptr, false ), "removal writes" );
	bits.Rewind();
	(void)bits.Read( 10 );
	Entity removed{};
	Expect( ReadEntity( bits, &entity, &removed, 9 ) && removed.number == 1023, "removal sentinel" );
	bits = {};
	Expect( WriteEntity( bits, &entity, &entity, false ) && bits.cursor == 0, "unchanged unforced entity omitted" );
	bits = {};
	bits.Write( 0, 1 ); bits.Write( 1, 1 ); bits.Write( 74, 8 ); bits.Rewind();
	Entity guard{};
	std::memset( &guard, 0xc3, sizeof( guard ) );
	const Entity before = guard;
	Expect( !ReadEntity( bits, nullptr, &guard, 9 ), "invalid entity last-field count rejected" );
	Expect( std::memcmp( &guard, &before, sizeof( guard ) ) == 0, "invalid count cannot publish" );
	bits = {};
	bits.Write( 106, 8 ); bits.Rewind();
	Player player{};
	Expect( !ReadPlayer( bits, nullptr, &player ), "invalid player last-field count rejected" );
	bits = {};
	Expect( !WritePlayer( bits, nullptr, nullptr ), "missing destination player rejected" );
	bits = {};
	Expect( !WriteCommand( bits, 0, nullptr, nullptr ), "missing destination command rejected" );
	bits = {};
	bits.limit = 8;
	Player full{};
	full.commandTime = 12;
	Expect( !WritePlayer( bits, nullptr, &full ) && !bits.Good(), "bounded writer overflow" );
}

void CanonicalRoundtrip() {
	Player external{};
	/* Fill every byte, including unknown fields: this is a storage round trip,
	 * not a claim that these arbitrary values fit each wire field's width. */
	auto *bytes = reinterpret_cast<std::uint8_t *>( &external );
	for ( std::size_t i = 0; i < sizeof( external ); ++i ) bytes[i] = std::uint8_t( i * 17 + 91 );
	playerState_t internal{};
	Player returned{};
	Expect( fntech3::jk2::state::ToInternal( external, internal ) &&
		fntech3::jk2::state::FromInternal( internal, returned ), "player adapter both directions" );
	Expect( std::memcmp( &external, &returned, sizeof( external ) ) == 0, "all 1368 player bytes preserved" );
	internal.commandTime = 991;
	internal.raven.fd_forcePower = 32;
	Expect( fntech3::jk2::state::FromInternal( internal, returned ), "updated canonical player writes" );
	Expect( returned.commandTime == 991 && returned.fd_forcePower == 32 &&
		std::memcmp( returned.unnamed_992, external.unnamed_992, sizeof( external.unnamed_992 ) ) == 0,
		"canonical edits overlay backing without corrupting unknown bytes" );
}
} // namespace

int main() {
	RetailFixtures();
	EveryWireField();
	MalformedAndLimits();
	CanonicalRoundtrip();
	if ( failures ) return 1;
	std::puts( "Jedi Outcast protocol 15: retail body fixtures, every field, transactional bounds and player storage pass" );
	return 0;
}
