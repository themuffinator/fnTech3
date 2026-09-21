/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_STVEF_FORCE_FEEDBACK_HPP
#define FNTECH3_STVEF_FORCE_FEEDBACK_HPP

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>

namespace fntech3::stvef::feedback {

/* Observed stvoy.exe 1.20 effect descriptors and scheduler, independently
 * expressed here. These are the arguments to the installation's FFC10.dll,
 * not a replacement for that runtime. See the retail force-feedback fixture. */
constexpr int EffectCount = 32, SlotCount = 4, ComponentCount = 3, Empty = 33;
constexpr bool ValidEffect( int id ) { return id >= 0 && id < EffectCount; }

struct Envelope {
	std::uint32_t bytes = 0, attackLevel = 0, attackTime = 0, fadeLevel = 0, fadeTime = 0;
};
static_assert( sizeof( Envelope ) == 20 );

struct Component {
	// Zero is unused, one is constant; 0x73/74/77 select the observed GUID.
	unsigned kind = 0;
	std::uint32_t magnitude = 0, period = 0, duration = 0;
	std::int32_t angle = 0, offset = 0;
	std::uint32_t phase = 0;
	Envelope envelope{};
};
struct Effect { const char *name; std::array<Component, ComponentCount> components; };

constexpr Component Constant( std::uint32_t magnitude, std::uint32_t duration,
	std::int32_t angle, Envelope envelope = {} ) {
	return { 1, magnitude, 0, duration, angle, 0, 0, envelope };
}
constexpr Component Periodic( unsigned guid, std::uint32_t magnitude, std::uint32_t period,
	std::uint32_t duration, std::int32_t angle, std::int32_t offset = 0, Envelope envelope = {} ) {
	return { guid, magnitude, period, duration, angle, offset, 0, envelope };
}

inline constexpr Effect Effects[EffectCount] = {
	{ "RandomNoise", {{ Periodic( 0x73, 7500, 95, 10000, 0, 0 ), Periodic( 0x77, 8300, 160, 10000, 9000, 0 ), Periodic( 0x73, 8300, 34, 10000, 31000, 0 ) }} },
	{ "AircraftCarrierTakeOff", {{ Constant( 10000, 2500, 0, { 20, 0, 600000, 0, 750000 } ) }} },
	{ "BasketballDribble", {{ Constant( 5000, 150, 18000, { 20, 0, 40000, 0, 30000 } ) }} },
	{ "CarEngineIdle", {{ Periodic( 0x74, 2500, 50, 10000, 0, 0 ) }} },
	{ "ChainsawIdle", {{ Periodic( 0x73, 3600, 60, 1000, 9000, 0 ), Periodic( 0x73, 4000, 100, 1000, 18000, 4000 ) }} },
	{ "ChainsawInAction", {{ Periodic( 0x73, 6700, 60, 1000, 9000, 0 ), Periodic( 0x73, 5000, 100, 1000, 18000, 5000 ), Periodic( 0x73, 10000, 340, 1000, 18000, 4000 ) }} },
	{ "DieselEngineIdle", {{ Periodic( 0x77, 2000, 250, 10000, 18000, 0 ), Periodic( 0x74, 4000, 125, 10000, 18000, 1500 ) }} },
	{ "Jump", {{ Periodic( 0x73, 5000, 500, 300, 18000, 0 ) }} },
	{ "Land", {{ Periodic( 0x73, 1000, 750, 250, 0, 0, { 20, 6000, 200000, 3000, 50000 } ) }} },
	{ "MachineGun", {{ Periodic( 0x73, 3500, 70, 1000, 0, 2500 ) }} },
	{ "Punched", {{ Periodic( 0x73, 8000, 130, 70, 0, 0, { 20, 0, 0, 0, 50000 } ) }} },
	{ "RocketLaunch", {{ Constant( 10000, 400, 18000, { 20, 0, 200000, 0, 0 } ), Constant( 10000, 1000, 0, { 20, 0, 300000, 0, 100000 } ) }} },
	{ "SecretDoor", {{ Constant( 10000, 400, 0, { 20, 0, 400000, 0, 0 } ) }} },
	{ "SwitchClick", {{ Constant( 7000, 50, 18000 ) }} },
	{ "WindGust", {{ Periodic( 0x74, 6000, 1000, 500, 18000, 0 ) }} },
	{ "WindShear", {{ Periodic( 0x77, 10000, 80, 2000, 0, 0, { 20, 0, 1500000, 0, 500000 } ) }} },
	{ "Pistol", {{ Periodic( 0x73, 8500, 130, 50, 0, 0 ) }} },
	{ "Shotgun", {{ Periodic( 0x73, 6000, 100, 100, 18000, 0 ), Constant( 7000, 300, 0, { 20, 0, 100000, 0, 100000 } ) }} },
	{ "Laser1", {{ Periodic( 0x74, 6000, 25, 1000, 18000, 2000 ) }} },
	{ "Laser2", {{ Periodic( 0x74, 5000, 25, 1000, 0, 3000 ) }} },
	{ "Laser3", {{ Periodic( 0x74, 5000, 25, 1000, 9000, 0 ) }} },
	{ "Laser4", {{ Periodic( 0x74, 7000, 25, 1000, 9000, 0 ) }} },
	{ "Laser5", {{ Periodic( 0x74, 5000, 25, 1000, 0, 0 ) }} },
	{ "Laser6", {{ Periodic( 0x74, 5000, 25, 1000, 0, 2000 ) }} },
	{ "OutOfAmmo", {{ Constant( 6000, 10, 18000 ) }} },
	{ "LightningGun", {{ Periodic( 0x74, 1500, 250, 1000, 18000, 0 ), Periodic( 0x74, 6000, 50, 1000, 0, 0 ) }} },
	{ "Missile", {{ Constant( 10000, 250, 0, { 20, 0, 500000, 0, 200000 } ) }} },
	{ "GatlingGun", {{ Periodic( 0x77, 6000, 100, 1000, 0, 1000 ) }} },
	{ "ShortPlasma", {{ Constant( 0, 250, 0, { 20, 7000, 250000, 0, 0 } ), Periodic( 0x74, 5000, 30, 250, 0, 0, { 20, 0, 0, 0, 250000 } ) }} },
	{ "PlasmaCannon1", {{ Periodic( 0x73, 5000, 500, 400, 18000, -5000 ), Periodic( 0x74, 6000, 30, 250, 0, 0, { 20, 0, 250000, 0, 0 } ) }} },
	{ "PlasmaCannon2", {{ Periodic( 0x73, 4000, 1000, 800, 18000, -4000 ), Periodic( 0x74, 8000, 35, 500, 0, 0, { 20, 0, 500000, 0, 0 } ) }} },
	{ "Cannon", {{ Periodic( 0x73, 8000, 100, 100, 18000, 0 ), Constant( 10000, 300, 0, { 20, 0, 100000, 0, 100000 } ) }} },
};

inline std::uint32_t Magnitude( std::uint32_t base, float gain ) {
	// Retail multiplies in x87 precision before truncating. Reject malformed
	// gains and saturate before conversion instead of invoking an overflowing
	// float-to-integer cast. Fixed offsets and envelope levels are not scaled.
	if ( !std::isfinite( gain ) || gain < 0 ) return 0;
	return static_cast<std::uint32_t>( std::min( double( base ) * gain,
		double( std::numeric_limits<std::int32_t>::max() ) ) );
}

constexpr int Tension( int level ) {
	constexpr int values[] = { 0, 1000, 5000, 10000 };
	return values[level < 0 ? 0 : level > 3 ? 3 : level];
}

template <typename Backend> class Player {
public:
	explicit Player( Backend &backend ) : backend_( backend ) { ids_.fill( Empty ); }
	Player( const Player & ) = delete;
	Player &operator=( const Player & ) = delete;

	void Play( int id, float gain ) {
		if ( !ValidEffect( id ) || !backend_.Available() ) return;
		for ( int slot = 0; slot < SlotCount; ++slot ) {
			if ( ids_[slot] == id ) { backend_.Start( slot ); return; }
		}
		for ( int slot = 0; slot < SlotCount; ++slot ) {
			if ( ids_[slot] != Empty && !backend_.Playing( slot ) ) { Replace( slot, id, gain ); return; }
		}
		for ( int slot = 0; slot < SlotCount; ++slot ) {
			if ( ids_[slot] == Empty ) { Replace( slot, id, gain ); return; }
		}
		Replace( 1, id, gain );
	}
	// True only when retail would enter Play's spring-restoring wrapper.
	bool Ensure( int id, float gain ) {
		if ( !ValidEffect( id ) || !backend_.Available() ) return false;
		for ( int slot = 0; slot < SlotCount; ++slot ) {
			if ( ids_[slot] == id ) {
				if ( !backend_.Playing( slot ) ) backend_.Start( slot );
				return false;
			}
		}
		Play( id, gain );
		return true;
	}
	void Stop( int id ) {
		if ( !ValidEffect( id ) || !backend_.Available() ) return;
		for ( int slot = 0; slot < SlotCount; ++slot ) if ( ids_[slot] == id ) backend_.Stop( slot );
	}
	void StopAll() {
		if ( !backend_.Available() ) return;
		for ( int slot = 0; slot < SlotCount; ++slot ) if ( ids_[slot] != Empty ) backend_.Stop( slot );
	}
	void Clear() {
		for ( int slot = 0; slot < SlotCount; ++slot ) backend_.Clear( slot );
		ids_.fill( Empty );
	}
	int EffectInSlot( int slot ) const { return slot >= 0 && slot < SlotCount ? ids_[slot] : Empty; }

private:
	void Replace( int slot, int id, float gain ) {
		if ( ids_[slot] != Empty && backend_.Playing( slot ) && !backend_.Stop( slot ) ) return;
		backend_.Clear( slot );
		ids_[slot] = Empty;
		if ( backend_.Build( slot, id, gain ) ) {
			ids_[slot] = id;
			backend_.Start( slot );
		}
	}
	Backend &backend_;
	std::array<int, SlotCount> ids_{};
};

} // namespace fntech3::stvef::feedback
#endif
