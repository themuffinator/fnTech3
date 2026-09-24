/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_ALICE_INTERNAL_STATE_H
#define FNTECH3_ALICE_INTERNAL_STATE_H

/* Engine-owned American McGee's Alice extensions, included by q_shared.h.
 *
 * Alice's entity state networks seventy-nine members and its player state
 * forty-nine. This engine's own records already have a member for 36 and 22
 * of them by name and meaning; these carry the rest, so a snapshot keeps
 * everything the module put in it. Both are generated from the delta tables
 * in compat/alice_wire_fields.def together with the mapping the adapter
 * uses, and the generator refuses to emit either unless the mapped and
 * carried members cover a table exactly.
 *
 * These are not module or wire records: compat/alice_game_abi.h owns the
 * external layout and compat/alice_state_adapter.hpp moves between the two.
 * Nothing here is interpreted -- a bone modifier, a reverb type and a camera
 * offset are moved between a module and a packet, and the game decides what
 * they mean. */

/* One of the five bone modifiers an entity carries, holding only what its
 * delta table networks: the sixteen bytes the module keeps between the angles
 * and the scale never leave it. */
typedef struct {
	int tag;
	float angles[3];
	float scale;
} aliceBoneModifier_t;

/* What Alice's entity sends that this engine's record has no member for:
 * five bone modifiers, an attachment to another entity's tag, a torso
 * animation of its own, and per-entity render state. */
typedef struct {
	float loopSoundVolume;
	float loopSoundMinDist;
	int parent;
	int tag_num;
	int attach_use_angles;
	float attach_offset[3];
	int skinNum;
	int anim;
	int crossblend_time;
	int torso_anim;
	int torso_frame;
	int torso_crossblend_time;
	/* Five bone modifiers: a tag, three angles and
	 * a scale each, which is what the delta table
	 * networks of the module's 36-byte record. */
	aliceBoneModifier_t bonemod[5];
	float scale;
	float alpha;
	int renderfx;
	int renderfx2;
	/* The thirty-two byte block the delta writer sends one byte at a
	 * time under a mask of its own. */
	unsigned char tagged[32];
} aliceEntityStateExtension_t;

/* And its player state's: the music, reverb and camera state a Quake III
 * client game would have owned. */
typedef struct {
	int pm_runtime;
	int feetfalling;
	int current_music_mood;
	int fallback_music_mood;
	float music_volume;
	float music_volume_fade_time;
	int reverb_type;
	float reverb_level;
	float blend[4];
	float fov;
	float camera_angles[3];
	float camera_origin[3];
	float camera_offset[3];
	float damage_angles[3];
	float camera_time;
	int camera_flags;
	/* Six thirty-two entry arrays, each under a mask of its own,
	 * sixteen bits to a value. */
	int block[6][32];
} alicePlayerStateExtension_t;

/* A sound the game started or stopped, queued for one client and carried in
 * its next snapshot. Retail keeps thirty-two a client (0x00457697) and sends
 * them after the packet entities; a volume or minimum distance of -1 means
 * the sound's own default and is not sent. */
#define ALICE_MAX_SERVER_SOUNDS 32

typedef struct {
	float origin[3];
	int entityNumber;
	int channel;
	int soundIndex;
	float volume;
	float minDist;
	qboolean stop;
	qboolean hasOrigin;
} aliceServerSound_t;

#endif
