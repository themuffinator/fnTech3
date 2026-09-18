/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_SERVER_STVEF_POLICY_HPP
#define FNTECH3_SERVER_STVEF_POLICY_HPP

#include <array>
#include <cstddef>

/* The retail Holomatch engine, not only its game, owns the Expansion Pack's
 * five exclusive game modes ("pMods"). stvoyHM.exe registers them as latched
 * serverinfo cvars, resolves conflicts before every map (0x42e160, from its
 * map command and again when the server spawns), reloads the map on
 * map_restart when one changed, clears them for spmap, and advertises a
 * server running one with "vv 1" in its info reply. */
namespace fntech3::stvef::server {

enum PMod : std::size_t {
	Assimilation, Disintegration, ActionHero, Specialties, Elimination, PModCount
};

inline constexpr std::array<const char *, PModCount> PModNames{
	"g_pModAssimilation", "g_pModDisintegration", "g_pModActionHero",
	"g_pModSpecialties", "g_pModElimination"
};

using PModSet = std::array<bool, PModCount>;

// Retail's rules, applied in its order: Assimilation needs team play (3),
// Elimination and Action Hero are refused by team and later game types, and
// the modes that cannot coexist switch each other off.
inline constexpr PModSet ResolvePMods( int gameType, PModSet on ) {
	if ( gameType != 3 ) on[Assimilation] = false;
	if ( gameType >= 4 ) on[Elimination] = false;
	if ( gameType >= 3 ) on[ActionHero] = false;
	if ( on[Assimilation] ) on[Disintegration] = on[Elimination] = on[ActionHero] = false;
	if ( on[Specialties] ) on[Disintegration] = on[ActionHero] = false;
	if ( on[Disintegration] ) on[Assimilation] = on[ActionHero] = on[Specialties] = false;
	if ( on[Elimination] ) on[Assimilation] = on[ActionHero] = false;
	if ( on[ActionHero] ) {
		on[Assimilation] = on[Specialties] = on[Elimination] = on[Disintegration] = false;
	}
	return on;
}

} // namespace fntech3::stvef::server
#endif
