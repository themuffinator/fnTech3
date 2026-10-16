/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_SERVER_JEDI_SP_SNAPSHOT_HPP
#define FNTECH3_SERVER_JEDI_SP_SNAPSHOT_HPP
#include <cmath>
#include <cstdint>

/* Raven's single-player snapshot rules, read in retail jasp.exe (Jedi Academy,
 * SV_AddEntitiesVisibleFromPoint 0x43c710, its add 0x43c5c0, its Force Sight
 * test 0x43c600) and jk2sp.exe (Jedi Outcast, 0x43a170 and 0x43a380). See
 * docs/fntech3/2026-10-16-jedi-sp-snapshot-walk.md. */
namespace fntech3::jedi::sp::snapshot {

/* Academy's state eFlags: a permanent entity is never sent, as the client
 * game draws it from the game state alone (0x43c7a9); one Force Sight always
 * shows is seen at any distance (0x43c685). */
constexpr std::uint32_t kAcademyPermanent = 0x80000u;
constexpr std::uint32_t kAcademyForceVisible = 0x800000u;
/* Force Sight's bit in the player's forcePowersActive (FP_SEE, 15), read at
 * player state +628 in the eye's pass only (0x43c762). */
constexpr std::uint32_t kAcademyForceSight = 1u << 15;
/* The client game's camera position and angles, asked through its vmMain
 * (jasp.exe 0x43c613, 0x43c62a; the builder asks the position at 0x43c9d1). */
constexpr int kAcademyCgameCameraPosition = 5;
constexpr int kAcademyCgameCameraAngles = 6;

/* Each engine stops adding at 1,024 entities, and on a snapshot whose counter
 * is odd at one fewer than its ring of stored states holds (jasp.exe
 * 0x43c5d6..0x43c5ef, ring 512 from 0x439bb7; jk2sp.exe 0x43a39e..0x43a3b7,
 * ring 256 from 0x437092). */
constexpr int kSnapshotEntities = 1024;
constexpr int kAcademyStateRing = 512;
constexpr int kOutcastStateRing = 256;
constexpr int OddSnapshotLimit(bool academy) noexcept {
	return (academy ? kAcademyStateRing : kOutcastStateRing) - 1;
}

/* Force Sight's test (0x43c600): the entity is seen from the client game's
 * camera when it is within 128 units, or within the level's range and close
 * enough to straight ahead, the cone narrowing from a dot product of 0.25 at
 * the camera to 0.99 at the range's end; a force-visible entity skips both
 * distance tests. Ranges are jasp.exe's table at 0x527d10. Retail indexes it
 * unchecked; a level past it has no range here, as level 0. */
inline bool ForceSightSees(const float camera[3], const float forward[3], const float target[3],
	std::uint32_t eFlags, int level) noexcept {
	static constexpr float kRanges[6] = { 0.0f, 1024.0f, 2048.0f, 4096.0f, 4096.0f, 4096.0f };
	const float range = level >= 0 && level < 6 ? kRanges[level] : 0.0f;
	float direction[3] = { target[0] - camera[0], target[1] - camera[1], target[2] - camera[2] };
	const float distance = std::sqrt(direction[0] * direction[0] + direction[1] * direction[1]
		+ direction[2] * direction[2]);
	if (distance != 0.0f) {
		for (float &axis : direction) axis /= distance;
	}
	if (!(eFlags & kAcademyForceVisible)) {
		if (distance < 128.0f) return true;
		if (distance > range) return false;
	}
	// Retail's x87 order (0x43c6d9..0x43c6ed); a NaN threshold sees.
	const float threshold = distance / range * 0.74f + 0.25f;
	const float facing = forward[0] * direction[0] + forward[1] * direction[1] + forward[2] * direction[2];
	return !(facing < threshold);
}

}
#endif
