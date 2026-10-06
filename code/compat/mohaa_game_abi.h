/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_COMPAT_MOHAA_GAME_ABI_H
#define FNTECH3_COMPAT_MOHAA_GAME_ABI_H

/* Frozen Medal of Honor: Allied Assault 1.00 module shapes and records.
 *
 * main/gamex86.dll exports GetGameAPI and main/cgamex86.dll GetCGameAPI, each
 * a flat table of pointer-sized slots rather than Quake III's vmMain. The
 * names are Ritual's, whose engine 2015 licensed, but the tables and records
 * are 2015's own and share little else with Alice's or F.A.K.K.2's.
 *
 * Every size here was measured in the two retail libraries, which are plain
 * PEs linked on 2001-12-06; MOHAA.exe is SafeDisc-wrapped and was never read.
 * The member names and orders are OpenMoHAA's (GPL-2.0-or-later), whose
 * structures describe the later 1.11 and 2.x modules. Where 1.00 differs the
 * retail libraries decide, and the difference is noted beside the member.
 * See docs/fntech3/2026-09-24-mohaa-contracts.md for every address.
 *
 * Nothing here loads, serves or calls anything. These are the layouts the
 * engine's adapters read and write at the module boundary; the engine's own
 * records stay its own. */

#include <stddef.h>

#ifdef __cplusplus
#include <cstdint>
#else
#include <stdint.h>
#endif

enum {
	/* GetGameAPI (0x310d7d10) writes 11 into export slot 0 before any
	 * pointer, then copies 0xa7 import dwords with one `rep movsd`. */
	FNTECH3_MOHAA_GAME_API_VERSION = 11,
	FNTECH3_MOHAA_GAME_IMPORT_SLOTS = 167,
	/* The export record the same routine fills at 0x31212ce0: the version,
	 * thirty-four function slots of which three stay null, the profiling
	 * pointer, and the entity array's bookkeeping, 0xa4 bytes in all. */
	FNTECH3_MOHAA_GAME_EXPORT_BYTES = 0xa4,

	/* GetCGameAPI (0x300237e4) takes no argument and fills 30 slots at
	 * 0x30265508, the last written null; the table is handed back through
	 * export 0, which copies 0xa8 import dwords (0x30023662). */
	FNTECH3_MOHAA_CGAME_IMPORT_SLOTS = 168,
	FNTECH3_MOHAA_CGAME_EXPORT_SLOTS = 30,

	/* GetGameAPI writes the entity size as an immediate (0x310d7e6f), and
	 * every LocateGameData site pushes it beside the client size
	 * (0x310d697d, 0x310d7497, 0x310ec07c). */
	FNTECH3_MOHAA_GENTITY_BYTES = 852,
	FNTECH3_MOHAA_GCLIENT_BYTES = 2272,

	/* The client game strides snapshot entities by 596 (0x30030c2b) and
	 * copies 0xa5 dwords of player state (0x30031289); its two snapshots
	 * lie 0x95ed0 apart (0x30033577). */
	FNTECH3_MOHAA_ENTITY_STATE_BYTES = 596,
	FNTECH3_MOHAA_PLAYER_STATE_BYTES = 660,
	FNTECH3_MOHAA_SNAPSHOT_BYTES = 614096,
	FNTECH3_MOHAA_MAX_SNAPSHOT_ENTITIES = 1024,
	FNTECH3_MOHAA_MAX_SERVER_SOUNDS = 64,

	FNTECH3_MOHAA_MAX_CLIENTS = 64,
	FNTECH3_MOHAA_MAX_GENTITIES = 1024,
	FNTECH3_MOHAA_ENTITYNUM_NONE = 1023,
	FNTECH3_MOHAA_ENTITYNUM_WORLD = 1022,

	/* 2015 widened Quake III's string limits: paths are 256 bytes, command
	 * strings 2,048 and info strings 1,350 -- the game passes 1,350 to
	 * GetUserinfo (0x310d9b4f). */
	FNTECH3_MOHAA_MAX_QPATH = 256,
	FNTECH3_MOHAA_MAX_STRING_CHARS = 2048,
	FNTECH3_MOHAA_MAX_INFO_STRING = 1350,

	FNTECH3_MOHAA_MAX_STATS = 32,
	FNTECH3_MOHAA_MAX_ACTIVE_ITEMS = 8,
	FNTECH3_MOHAA_MAX_AMMO = 16,
	FNTECH3_MOHAA_MAX_FRAMEINFOS = 16,
	FNTECH3_MOHAA_NUM_BONE_CONTROLLERS = 5,
	FNTECH3_MOHAA_MAX_MODEL_SURFACES = 32,

	/* Configstrings. The client game bounds an index at 2,736 (0x300234dd)
	 * and dispatches these blocks (0x30022f9a..0x3002326e); the game writes
	 * the objectives at CS_OBJECTIVES + n (0x310518d1). Below 32, 1.00 is not
	 * 1.11: its music is 8, fog 9 and sky 10, where 1.11 moved them to 6..8. */
	FNTECH3_MOHAA_MAX_CONFIGSTRINGS = 2736,
	FNTECH3_MOHAA_CS_SERVERINFO = 0,
	FNTECH3_MOHAA_CS_SYSTEMINFO = 1,
	/* The worldspawn's message and the name a trigger_save gives the next save
	 * (OpenMoHAA bg_public.h; below 6 the numbers are 1.11's too). */
	FNTECH3_MOHAA_CS_MESSAGE = 2,
	FNTECH3_MOHAA_CS_SAVENAME = 3,
	FNTECH3_MOHAA_CS_MUSIC = 8,
	FNTECH3_MOHAA_CS_FOGINFO = 9,
	FNTECH3_MOHAA_CS_SKYINFO = 10,
	FNTECH3_MOHAA_CS_MODELS = 32,
	FNTECH3_MOHAA_MAX_MODELS = 1024,
	FNTECH3_MOHAA_CS_OBJECTIVES = 1056,
	FNTECH3_MOHAA_MAX_OBJECTIVES = 20,
	FNTECH3_MOHAA_CS_SOUNDS = 1076,
	FNTECH3_MOHAA_MAX_SOUNDS = 512,
	FNTECH3_MOHAA_CS_IMAGES = 1588,
	FNTECH3_MOHAA_MAX_IMAGES = 64,
	FNTECH3_MOHAA_CS_LIGHTSTYLES = 1652,
	FNTECH3_MOHAA_MAX_LIGHTSTYLES = 32,
	FNTECH3_MOHAA_CS_PLAYERS = 1684
};

#ifdef __cplusplus
namespace fntech3::mohaa {

using vec3 = float[3];

/* An animation blended into an entity's pose: the animation's index in its
 * TIKI, the time into it, and its weight. */
struct FrameInfo {
	std::int32_t index;
	float time;
	float weight;
};
static_assert( sizeof( FrameInfo ) == 12 );

/* The entity state, 596 bytes. OpenMoHAA's is 612: 1.11 added the `pos`
 * trajectory after eFlags -- protocol 8 is protocol 6 plus that trajectory
 * on the wire -- and without it the client game's 596-byte stride and the
 * game's 852-byte entity both come out exactly. */
struct EntityState {
	std::int32_t number;
	std::int32_t eType;
	std::int32_t eFlags;
	float netorigin[3];
	float origin[3];
	float origin2[3];
	float netangles[3];
	float angles[3];
	std::int32_t constantLight;
	std::int32_t loopSound;
	float loopSoundVolume;
	float loopSoundMinDist;
	float loopSoundMaxDist;
	float loopSoundPitch;
	std::int32_t loopSoundFlags;
	std::int32_t parent;
	std::int32_t tag_num;
	std::int32_t attach_use_angles;
	float attach_offset[3];
	std::int32_t beam_entnum;
	std::int32_t modelindex;
	std::int32_t usageIndex;
	std::int32_t skinNum;
	std::int32_t wasframe;
	FrameInfo frameInfo[FNTECH3_MOHAA_MAX_FRAMEINFOS];
	float actionWeight;
	std::int32_t bone_tag[FNTECH3_MOHAA_NUM_BONE_CONTROLLERS];
	float bone_angles[FNTECH3_MOHAA_NUM_BONE_CONTROLLERS][3];
	float bone_quat[FNTECH3_MOHAA_NUM_BONE_CONTROLLERS][4];
	std::uint8_t surfaces[FNTECH3_MOHAA_MAX_MODEL_SURFACES];
	std::int32_t clientNum;
	std::int32_t groundEntityNum;
	std::int32_t solid;
	float scale;
	float alpha;
	std::int32_t renderfx;
	float shader_data[2];
	float shader_time;
	float quat[4];
	float eyeVector[3];
};
static_assert( sizeof( EntityState ) == FNTECH3_MOHAA_ENTITY_STATE_BYTES );
static_assert( offsetof( EntityState, modelindex ) == 128 );
static_assert( offsetof( EntityState, frameInfo ) == 144 );
static_assert( offsetof( EntityState, surfaces ) == 500 );
static_assert( offsetof( EntityState, solid ) == 540 );

struct Plane {
	float normal[3];
	float dist;
	std::uint8_t type;
	std::uint8_t signbits;
	std::uint8_t pad[2];
};
static_assert( sizeof( Plane ) == 20 );

/* 2015's trace: Quake III's with the hit shader between the surface flags
 * and the contents, and a hit location and the hit entity's record after
 * the entity number. */
struct Trace {
	std::int32_t allsolid;
	std::int32_t startsolid;
	float fraction;
	float endpos[3];
	Plane plane;
	std::int32_t surfaceFlags;
	std::int32_t shaderNum;
	std::int32_t contents;
	std::int32_t entityNum;
	std::int32_t location;
	std::uint32_t ent;	/* gentity_t *, a 32-bit pointer in the module */
};
static_assert( sizeof( Trace ) == 68 );

/* The player state, 660 bytes, which is OpenMoHAA's 688 without four of its
 * members. The client game's copy of it (0x303e4244) is read at the offsets
 * below, and they settle which: the origin is at 20, so one of the five words
 * before it is gone (the networked flags word is kept at 12 and 1.11's
 * private pm_flags dropped); the view model's networked animation and its
 * change counter are at 184 and 188 (0x30048dc8..0x30048e07), so 1.11's
 * unnetworked iViewModelAnim is gone and the stats begin at 192, where the
 * team stat is compared with 3 twenty times; the music moods are at 544
 * and 548 (MUSIC_UpdateMood, 0x300331cf); and the camera block ends in the
 * damage angles at 644. That leaves exactly the ping at 656: Team Assault's
 * radar word and vote flag and 2.0's eye position were never in 1.00. */
struct PlayerState {
	std::int32_t commandTime;
	std::int32_t pm_type;
	std::int32_t bobCycle;
	std::int32_t pm_flags;
	std::int32_t pm_time;
	float origin[3];
	float velocity[3];
	std::int32_t gravity;
	std::int32_t speed;
	std::int32_t delta_angles[3];
	std::int32_t groundEntityNum;
	std::int32_t walking;
	std::int32_t groundPlane;
	std::int32_t feetfalling;
	float falldir[3];
	Trace groundTrace;
	std::int32_t clientNum;
	float viewangles[3];
	std::int32_t viewheight;
	float fLeanAngle;
	std::int32_t iNetViewModelAnim;
	std::int32_t iViewModelAnimChanged;
	std::int32_t stats[FNTECH3_MOHAA_MAX_STATS];
	std::int32_t activeItems[FNTECH3_MOHAA_MAX_ACTIVE_ITEMS];
	std::int32_t ammo_name_index[FNTECH3_MOHAA_MAX_AMMO];
	std::int32_t ammo_amount[FNTECH3_MOHAA_MAX_AMMO];
	std::int32_t max_ammo_amount[FNTECH3_MOHAA_MAX_AMMO];
	std::int32_t current_music_mood;
	std::int32_t fallback_music_mood;
	float music_volume;
	float music_volume_fade_time;
	std::int32_t reverb_type;
	float reverb_level;
	float blend[4];
	float fov;
	float camera_origin[3];
	float camera_angles[3];
	float camera_time;
	float camera_offset[3];
	float camera_posofs[3];
	std::int32_t camera_flags;
	float damage_angles[3];
	std::int32_t ping;
};
static_assert( sizeof( PlayerState ) == FNTECH3_MOHAA_PLAYER_STATE_BYTES );
static_assert( offsetof( PlayerState, origin ) == 20 );
static_assert( offsetof( PlayerState, groundEntityNum ) == 64 );
static_assert( offsetof( PlayerState, clientNum ) == 160 );
static_assert( offsetof( PlayerState, fLeanAngle ) == 180 );
static_assert( offsetof( PlayerState, iNetViewModelAnim ) == 184 );
static_assert( offsetof( PlayerState, stats ) == 192 );
static_assert( offsetof( PlayerState, current_music_mood ) == 544 );
static_assert( offsetof( PlayerState, fov ) == 584 );
static_assert( offsetof( PlayerState, camera_flags ) == 640 );
static_assert( offsetof( PlayerState, damage_angles ) == 644 );
static_assert( offsetof( PlayerState, blend ) == 568 );

/* Two stats the engine itself draws from (OpenMoHAA bg_public.h
 * playerstat_t, whose order 1.00's HUD menus confirm through 16): the screen
 * fade in `blend` is added rather than laid over when stats[14] is set, and a
 * cinematic's letterbox covers stats[15] / 32767 of the screen's height at
 * the top and again at the bottom. The game writes the letterbox there,
 * scaled by 32767 (gamex86.dll 0x3111f5bc..0x3111f5cd, `[client + 0xfc]`). */
constexpr int kStatAddFade = 14;
constexpr int kStatLetterbox = 15;
constexpr int kLetterboxScale = 32767;
/* The team (stats[20], which the client game compares with 3) and, after it
 * in OpenMoHAA's order, the kills a status reports as a score and the deaths
 * GameSpy's player list reports beside them. */
constexpr int kStatTeam = 20;
constexpr int kStatKills = 21;
constexpr int kStatDeaths = 22;
static_assert( offsetof( PlayerState, stats ) + kStatLetterbox * 4 == 0xfc );

/* The engine's half of the game's entity record, which follows the entity
 * state and the game's client pointer and in-use flag. OpenMoHAA's ends in a
 * four-entry non-PVS sound list that 1.00 does not have: without it the
 * record, the six private members after it and the game's own tail add up
 * to 852 exactly, and the TIKI pointer lands at 720, where the game reads it
 * (0x3108e277). */
struct EntityShared {
	std::int32_t linked;
	std::int32_t linkcount;
	std::int32_t svFlags;
	std::int32_t singleClient;
	std::int32_t lastNetTime;
	std::int32_t bmodel;
	float mins[3];
	float maxs[3];
	std::int32_t contents;
	float absmin[3];
	float absmax[3];
	float radius;
	float centroid[3];
	std::int32_t areanum;
	float currentAngles[3];
	std::int32_t ownerNum;
};
static_assert( sizeof( EntityShared ) == 112 );

/* The part of the game's 852-byte entity the engine reads and writes. The
 * rest belongs to the game: its Entity object, spawn times, name, clip mask
 * and list links. */
struct GEntity {
	EntityState s;
	std::uint32_t client;	/* gclient_t *, whose record starts with its PlayerState */
	std::int32_t inuse;
	EntityShared r;
	std::int32_t solid;	/* solid_t */
	std::uint32_t tiki;	/* dtiki_t * */
	float mat[3][3];
};
static_assert( offsetof( GEntity, client ) == 596 );
static_assert( offsetof( GEntity, r ) == 604 );
static_assert( offsetof( GEntity, tiki ) == 720 );
static_assert( sizeof( GEntity ) <= FNTECH3_MOHAA_GENTITY_BYTES );

/* The first of the game's own members, straight after the engine's half: the
 * entity's Entity object, null once the game has freed it. The game's
 * SoundCallback reads it there (gamex86.dll 0x310d78ee) and throws a script
 * error out of the module when it is null (0x310d79e1). */
constexpr int kGEntityObjectOffset = 760;
static_assert( sizeof( GEntity ) == kGEntityObjectOffset );

/* The user command: Quake III's without a weapon byte, the weapon being in
 * the button bits, and with the button word widened to sixteen bits. */
struct UserCmd {
	std::int32_t serverTime;
	std::uint8_t msec;
	std::uint16_t buttons;
	std::int16_t angles[3];
	std::int8_t forwardmove;
	std::int8_t rightmove;
	std::int8_t upmove;
};
static_assert( sizeof( UserCmd ) == 20 );
static_assert( offsetof( UserCmd, buttons ) == 6 );
static_assert( offsetof( UserCmd, forwardmove ) == 14 );

/* Where the player's eyes are, sent beside each command. */
struct UserEyes {
	std::int8_t ofs[3];
	float angles[2];
};
static_assert( sizeof( UserEyes ) == 12 );

/* A sound the server tells the client to start or stop, carried in the
 * snapshot rather than as an entity event. */
struct ServerSound {
	float origin[3];
	std::int32_t entity_number;
	std::int32_t channel;
	std::int16_t sound_index;
	float volume;
	float min_dist;
	float maxDist;
	float pitch;
	std::int32_t stop_flag;
	std::int32_t streamed;
};
static_assert( sizeof( ServerSound ) == 48 );

struct Snapshot {
	std::int32_t snapFlags;
	std::int32_t ping;
	std::int32_t serverTime;
	std::uint8_t areamask[32];
	PlayerState ps;
	std::int32_t numEntities;
	EntityState entities[FNTECH3_MOHAA_MAX_SNAPSHOT_ENTITIES];
	std::int32_t numServerCommands;
	std::int32_t serverCommandSequence;
	std::int32_t number_of_sounds;
	ServerSound sounds[FNTECH3_MOHAA_MAX_SERVER_SOUNDS];
};
static_assert( offsetof( Snapshot, ps ) == 0x2c );
static_assert( offsetof( Snapshot, numEntities ) == 0x2c0 );
static_assert( offsetof( Snapshot, number_of_sounds ) == 0x952cc );
static_assert( offsetof( Snapshot, sounds ) == 0x952d0 );
static_assert( sizeof( Snapshot ) == FNTECH3_MOHAA_SNAPSHOT_BYTES );

struct Orientation {
	float origin[3];
	float axis[3][3];
};
static_assert( sizeof( Orientation ) == 48 );

/* The console variable record both modules read directly. The retail game
 * reads `integer` at +0x20 (0x310e2815); these nine members are Quake III's,
 * in Quake III's order. */
struct Cvar {
	std::uint32_t name;
	std::uint32_t string;
	std::uint32_t resetString;
	std::uint32_t latchedString;
	std::int32_t flags;
	std::int32_t modified;
	std::int32_t modificationCount;
	float value;
	std::int32_t integer;
	std::uint32_t next;
	std::uint32_t hashNext;
};
static_assert( offsetof( Cvar, integer ) == 0x20 );

} // namespace fntech3::mohaa
#endif

#endif
