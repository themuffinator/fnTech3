/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_COMPAT_RITUAL_LAYOUT_HPP
#define FNTECH3_COMPAT_RITUAL_LAYOUT_HPP

/* The per-profile numbers of the game module boundary the Ritual engine's two
 * titles share.
 *
 * American McGee's Alice and Heavy Metal: F.A.K.K.2 hand their engine the same
 * kind of game module, and the engine reads the same members of its entity
 * records in place -- linking, clipping and walking snapshots through byte
 * offsets, as both retail engines do. What differs is where those members sit,
 * how many configstrings each block holds, and how long each table is. Those
 * numbers live here, once per title, and the server's Ritual adapter reads
 * them through the one layout the running profile selects; nothing else in it
 * branches on the title for a number.
 *
 * Alice's are measured in alice.exe (code/compat/alice_module_abi.h);
 * F.A.K.K.2's in its retail modules (code/compat/fakk2_module_abi.h). */

#include "alice_game_abi.h"
#include "alice_module_abi.h"
#include "fakk2_game_abi.h"
#include "fakk2_module_abi.h"

#include "../qcommon/game_profile.h"

namespace fntech3::ritual {

struct ServerLayout {
	comGameId_t profile;
	const char *title;             /* for messages: "Alice's", "F.A.K.K.2's" */

	int gameApiVersion;
	int gameImportSlots;
	int gentitySize;
	int gclientSize;
	int entityStateBytes;

	/* The entity state's members the engine reads or writes in place. */
	int sNumber;
	int sOrigin;
	int sOrigin2;
	int sLoopSound;
	int sLoopSoundMinDist;
	int sParent;
	int sModelIndex;
	/* Quake III's `solid` word: the linker writes a clip handle here. */
	int sSolid;
	int sRenderfx;
	int sBindParent;

	/* The engine's half of the record. */
	int rInuse;
	int rLinked;
	int rSvFlags;
	int rBmodel;
	int rMins;
	int rMaxs;
	int rContents;
	int rAbsmin;
	int rAbsmax;
	int rRadius;
	int rCentroid;
	int rCurrentOrigin;
	int rCurrentAngles;
	int rSolidType;

	int psPing;

	/* The configstring blocks the index imports find and add in. */
	int csModels, maxModels;
	int csSounds, maxSounds;
	int csImages, maxImages;
	int csLightStyles;
	int csItems, maxItems;
	int maxConfigstrings;
};

inline const ServerLayout &AliceServerLayout() {
	static const ServerLayout layout{
		COM_GAME_ALICE, "Alice's",
		FNTECH3_ALICE_GAME_API_VERSION, FNTECH3_ALICE_GAME_IMPORT_SLOTS,
		FNTECH3_ALICE_GENTITY_SIZE, FNTECH3_ALICE_GCLIENT_SIZE,
		FNTECH3_ALICE_ENTITY_STATE_BYTES,
		FNTECH3_ALICE_S_NUMBER,
		static_cast<int>( offsetof( fntech3_alice_entityState_t, origin ) ),
		FNTECH3_ALICE_S_ORIGIN2,
		static_cast<int>( offsetof( fntech3_alice_entityState_t, loopSound ) ),
		static_cast<int>( offsetof( fntech3_alice_entityState_t, loopSoundMinDist ) ),
		static_cast<int>( offsetof( fntech3_alice_entityState_t, parent ) ),
		FNTECH3_ALICE_S_MODELINDEX, FNTECH3_ALICE_S_SOLID, FNTECH3_ALICE_S_RENDERFX,
		FNTECH3_ALICE_S_OWNERNUM,
		FNTECH3_ALICE_R_INUSE, FNTECH3_ALICE_R_LINKED, FNTECH3_ALICE_R_SVFLAGS,
		FNTECH3_ALICE_R_BMODEL, FNTECH3_ALICE_R_MINS, FNTECH3_ALICE_R_MAXS,
		FNTECH3_ALICE_R_CONTENTS, FNTECH3_ALICE_R_ABSMIN, FNTECH3_ALICE_R_ABSMAX,
		/* The far plane's sphere, read at 0x0045742d and 0x00457413. */
		0x24c, 0x250,
		FNTECH3_ALICE_R_CURRENTORIGIN, FNTECH3_ALICE_R_CURRENTANGLES,
		FNTECH3_ALICE_R_SOLIDTYPE,
		FNTECH3_ALICE_PS_PING,
		/* The index blocks retail's wrappers pass: models at 0x20, sounds at
		 * 0x220, images at 0x320, light styles at 0x360, items at 0x400. */
		0x20, 0x200, 0x220, 0x100, 0x320, 0x40, 0x360, 0x400, 0x40,
		FNTECH3_ALICE_MAX_CONFIGSTRINGS
	};
	return layout;
}

inline const ServerLayout &FAKK2ServerLayout() {
	static const ServerLayout layout{
		COM_GAME_FAKK2, "F.A.K.K.2's",
		FNTECH3_FAKK2_GAME_API_VERSION, FNTECH3_FAKK2_GAME_IMPORT_SLOTS,
		FNTECH3_FAKK2_GENTITY_SIZE, FNTECH3_FAKK2_GCLIENT_SIZE,
		FNTECH3_FAKK2_ENTITY_STATE_BYTES,
		FNTECH3_FAKK2_S_NUMBER, FNTECH3_FAKK2_S_ORIGIN, FNTECH3_FAKK2_S_ORIGIN2,
		FNTECH3_FAKK2_S_LOOPSOUND, FNTECH3_FAKK2_S_LOOPSOUNDMINDIST,
		FNTECH3_FAKK2_S_PARENT, FNTECH3_FAKK2_S_MODELINDEX, FNTECH3_FAKK2_S_SOLID,
		FNTECH3_FAKK2_S_RENDERFX, FNTECH3_FAKK2_S_BINDPARENT,
		FNTECH3_FAKK2_R_INUSE, FNTECH3_FAKK2_R_LINKED, FNTECH3_FAKK2_R_SVFLAGS,
		FNTECH3_FAKK2_R_BMODEL, FNTECH3_FAKK2_R_MINS, FNTECH3_FAKK2_R_MAXS,
		FNTECH3_FAKK2_R_CONTENTS, FNTECH3_FAKK2_R_ABSMIN, FNTECH3_FAKK2_R_ABSMAX,
		FNTECH3_FAKK2_R_RADIUS, FNTECH3_FAKK2_R_CENTROID,
		FNTECH3_FAKK2_R_CURRENTORIGIN, FNTECH3_FAKK2_R_CURRENTANGLES,
		FNTECH3_FAKK2_R_SOLIDTYPE,
		FNTECH3_FAKK2_PS_PING,
		FNTECH3_FAKK2_CS_MODELS, FNTECH3_FAKK2_MAX_MODELS,
		FNTECH3_FAKK2_CS_SOUNDS, FNTECH3_FAKK2_MAX_SOUNDS,
		FNTECH3_FAKK2_CS_IMAGES, FNTECH3_FAKK2_MAX_IMAGES,
		FNTECH3_FAKK2_CS_LIGHTSTYLES,
		FNTECH3_FAKK2_CS_ITEMS, FNTECH3_FAKK2_MAX_ITEMS,
		FNTECH3_FAKK2_MAX_CONFIGSTRINGS
	};
	return layout;
}

/* The layout for a profile, or null outside the two Ritual titles. */
inline const ServerLayout *ServerLayoutFor( comGameId_t profile ) {
	if ( profile == COM_GAME_ALICE ) return &AliceServerLayout();
	if ( profile == COM_GAME_FAKK2 ) return &FAKK2ServerLayout();
	return nullptr;
}

/* The same blocks in both titles, in the same order: each block starts where
 * the one before it ends, so a layout whose blocks overlap is a typo. */
static_assert( 0x20 + 0x200 == 0x220 && 0x220 + 0x100 == 0x320 && 0x320 + 0x40 == 0x360,
	"Alice's blocks tile" );
static_assert( FNTECH3_FAKK2_CS_MODELS + FNTECH3_FAKK2_MAX_MODELS == FNTECH3_FAKK2_CS_SOUNDS
	&& FNTECH3_FAKK2_CS_SOUNDS + FNTECH3_FAKK2_MAX_SOUNDS == FNTECH3_FAKK2_CS_IMAGES
	&& FNTECH3_FAKK2_CS_IMAGES + FNTECH3_FAKK2_MAX_IMAGES == FNTECH3_FAKK2_CS_LIGHTSTYLES
	&& FNTECH3_FAKK2_CS_LIGHTSTYLES + FNTECH3_FAKK2_MAX_LIGHTSTYLES == FNTECH3_FAKK2_CS_PLAYERS
	&& FNTECH3_FAKK2_CS_PLAYERS + FNTECH3_FAKK2_MAX_CLIENTS == FNTECH3_FAKK2_CS_ITEMS
	&& FNTECH3_FAKK2_CS_ITEMS + FNTECH3_FAKK2_MAX_ITEMS == FNTECH3_FAKK2_CS_AMMO,
	"F.A.K.K.2's blocks tile" );

} // namespace fntech3::ritual

#endif
