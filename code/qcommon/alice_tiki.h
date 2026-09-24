/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_QCOMMON_ALICE_TIKI_H
#define FNTECH3_QCOMMON_ALICE_TIKI_H

/* American McGee's Alice's model definitions, loaded once per process and
 * shared by the game module, the client game and the renderer, as retail
 * alice.exe shares one cache between all three.
 *
 * A definition -- a TIKI -- names a skeleton or a set of vertex-animated
 * meshes, binds each of the model's surfaces to shaders, and lists the
 * model's animations with the commands the game and the client game run on
 * particular frames. Retail normally loads the compiled form, a `.cik` image
 * of its own in-memory record, in preference to the `.tik` text (0x0045c840),
 * and so does this runtime; a model that ships no `.cik` is compiled from its
 * text by the same rules. See docs/fntech3/2026-09-23-alice-tiki.md. */

#include "q_shared.h"

#ifdef __cplusplus

#include "../compat/alice_tiki_pose.hpp"
#include "../compat/alice_skb_format.hpp"
#include "../compat/alice_tan_format.hpp"
#include "../compat/alice_render_abi.h"

#include <deque>
#include <memory>
#include <string>
#include <vector>

namespace fntech3 {
namespace alice {

/* ---- alias lists --------------------------------------------------------- */

/* The engine's alias lists: one per model, filled by the game module and the
 * client game from the model's init commands, and the engine's own list,
 * named `Global`. They are kept in alice.exe's own layout because the game
 * module walks them: game import 85 hands it a model's list, and its savegame
 * code (fgamex86.dll 0x100a3cb0) follows `head` and `next` and reads the
 * history fields below directly. The offsets are the 32-bit ones the retail
 * modules see. See alice_alias.cpp. */
namespace alias {

/* One actor's history with one alias: 0x14 bytes (0x00422c10). */
struct Actor {
	int actor;               /* 0x00 the speaker's entity number */
	int timesPlayed;         /* 0x04 */
	unsigned char played;    /* 0x08 played since its last reset */
	int lastPlayed;          /* 0x0c Sys_Milliseconds when last played */
	Actor *next;             /* 0x10 */
};

/* An alias: 0xc4 bytes (0x00422910). */
struct Record {
	char name[32];           /* 0x00 lower case, at most 31 characters */
	char realName[128];      /* 0x20 at most 127 characters */
	float weight;            /* 0xa0 1 unless `weight` says otherwise */
	unsigned char global;    /* 0xa4 history kept here, not per actor */
	unsigned char stop;      /* 0xa5 never reset once played */
	float timeout;           /* 0xa8 seconds before a played line resets */
	int maxUse;              /* 0xac -1 for no limit */
	int timesPlayed;         /* 0xb0 */
	unsigned char played;    /* 0xb4 */
	int lastPlayed;          /* 0xb8 */
	Actor *actors;           /* 0xbc newest first */
	Record *next;            /* 0xc0 */
};

/* A list: 0x30 bytes (0x00422960). An addition is pushed at the head and
 * marks the list dirty; the next lookup sorts, drops duplicates, fills
 * `sorted` and relinks the chain in descending order. */
struct List {
	char name[32];           /* 0x00 copied with strncpy: unterminated at 32 */
	int dirty;               /* 0x20 */
	int count;               /* 0x24 */
	Record **sorted;         /* 0x28 */
	Record *head;            /* 0x2c */
};

/* Retail's list routines, one for each engine function. */
List *CreateList( const char *name );                           /* 0x00422960 */
void ClearList( List *list );                                   /* 0x004229f0 */
void FreeList( List *list );                                    /* 0x00422a40 */
/* The engine's own list, created on first use as its wrappers do. */
List *Global( void );
qboolean Add( List *list, const char *name, const char *realName,
	const char *parameters );                                   /* 0x00422cd0 */
void Sort( List *list );                                        /* 0x00422e30 */
const char *FindRandom( List *list, const char *name );         /* 0x004236c0 */
const char *FindDialog( List *list, const char *name, int random, int actor ); /* 0x004232f0 */
void UpdateDialog( List *list, const char *name, int timesPlayed, int played,
	int lastPlayed );                                           /* 0x00423200 */
void UpdateActorDialog( List *list, const char *name, int actor, int timesPlayed,
	int played, int lastPlayed );                               /* 0x00423270 */
void Dump( List *list );                                        /* 0x00423990 */

/* Where retail calls rand() and Sys_Milliseconds(). A null argument restores
 * the default; tests pin both. The random source is masked to 15 bits, as
 * retail masks rand(). */
void SetRandomSource( int ( *source )( void ) );
void SetClock( int ( *clock )( void ) );

struct ListDeleter {
	void operator()( List *list ) const;
};

} // namespace alias

namespace tiki {

/* Retail's limits: 13 arguments a command; 160 init commands a side; 192
 * client and 64 server commands an animation; 511 animations; four skins a
 * surface. */
enum {
	MaxCommandArgs = 13,
	MaxInitCommands = 160,
	MaxClientFrameCommands = 192,
	MaxServerFrameCommands = 64,
	MaxAnimations = 511,
	MaxSkins = 4
};

/* A command's frame: a frame number, or one of the three the definition
 * spells as a word. */
enum {
	FrameEvery = -1,
	FrameExit = -2,
	FrameEntry = -3
};

/* Surface flags, as `surface NAME flags X` sets them. */
enum {
	SurfaceSkinMask = 0x03,
	SurfaceNoDraw = 0x04,
	SurfaceCrossfade = 0x40,
	SurfaceNoDamage = 0x80,
	SurfaceNoMipmaps = 0x100,
	SurfaceNoPicmip = 0x200
};

/* Animation flags. */
enum {
	AnimDeltaDriven = 0x01,
	AnimDefaultAngles = 0x08,
	AnimNoTimeCheck = 0x10,
	AnimTorsoAdjust = 0x20
};

struct Command {
	int frame;
	int numArgs;
	const char *args[MaxCommandArgs];
};

struct Surface {
	char name[64];
	char shaders[MaxSkins][64];
	int numSkins;
	int flags;
	float damage;
};

/* A model file: a skeleton, a skeletal animation, or a vertex-animated mesh.
 * The bytes are kept exactly as read, because every consumer -- the tag
 * queries here and the renderer's skinning -- reads the file's own layout. */
struct File {
	enum class Kind { Skeleton, SkeletalAnim, Tan };
	char name[MAX_QPATH];
	Kind kind;
	std::vector<unsigned char> bytes;
	pose::Skeleton skeleton;
	/* A skeleton's surfaces in file order, as the full reader located them
	 * when the file was read; the renderer indexes these instead of walking
	 * the chain of surfaces. Empty for the other kinds. */
	std::vector<model::SkbSurface> skbSurfaces;
	pose::SkeletalAnim anim;
	/* A TAN's directory, from the bounded reader; its total time and delta
	 * sit in the header at 84 and 88. */
	model::Tan tan;
	float tanDelta[3] = { 0.0f, 0.0f, 0.0f };
};

struct Animation {
	char alias[48];
	char path[128];
	float weight;
	int crossblendMs;
	int flags;
	const File *file;
	std::vector<Command> client;
	std::vector<Command> server;
};

struct Model {
	char name[128];
	char skeletonName[128];
	const File *skeleton;             /* null for a vertex-animated model */
	float scale;
	float lodScale;
	float lodBias;
	float lightOffset[3];
	float origin[3];
	float mins[3];
	float maxs[3];
	float radius;
	int numTags;
	std::vector<Surface> surfaces;
	std::vector<Animation> animations;  /* in retail's sorted order */
	std::vector<Command> clientInit;
	std::vector<Command> serverInit;
	/* Created by the first alias added to the model (0x0045cc00). */
	std::unique_ptr<alias::List, alias::ListDeleter> aliasList;
	std::deque<std::string> strings;    /* every command argument; never moves */
	bool compiledFromText;
};

} // namespace tiki
} // namespace alice
} // namespace fntech3

using AliceTikiModel = fntech3::alice::tiki::Model;

/* ---- the cache ---------------------------------------------------------- */

/* The handle of a definition, loading it if it is new; -1 when it cannot be
 * loaded. Handles are small integers and stay valid until the cache is
 * cleared. */
int AliceTiki_Register( const char *name );
const AliceTikiModel *AliceTiki_Get( int handle );
int AliceTiki_Count( void );
void AliceTiki_Clear( void );

/* ---- queries both modules make ------------------------------------------ */

const char *AliceTiki_Name( int handle );
int AliceTiki_NumAnims( int handle );
int AliceTiki_NumSurfaces( int handle );
int AliceTiki_NumTags( int handle );
void AliceTiki_CalculateBounds( int handle, float scale, vec3_t mins, vec3_t maxs );
/* Fill the module's command list from the init block of one side. */
qboolean AliceTiki_InitCommands( int handle, qboolean client, fntech3_alice_tiki_cmd_t *out );

const char *AliceTiki_AnimName( int handle, int anim );
int AliceTiki_AnimNumForName( int handle, const char *name );
int AliceTiki_AnimRandom( int handle, const char *name );
int AliceTiki_AnimNumFrames( int handle, int anim );
float AliceTiki_AnimTime( int handle, int anim );
void AliceTiki_AnimDelta( int handle, int anim, vec3_t delta );
void AliceTiki_AnimAbsoluteDelta( int handle, int anim, vec3_t delta );
int AliceTiki_AnimFlags( int handle, int anim );
int AliceTiki_AnimCrossblendTime( int handle, int anim );
qboolean AliceTiki_AnimHasCommands( int handle, int anim, qboolean client );

qboolean AliceTiki_FrameCommands( int handle, int anim, int frame, qboolean client,
	fntech3_alice_tiki_cmd_t *out );
void AliceTiki_FrameDelta( int handle, int anim, int frame, vec3_t delta );
float AliceTiki_FrameTime( int handle, int anim, int frame );
void AliceTiki_FrameBounds( int handle, int anim, int frame, float scale, vec3_t mins, vec3_t maxs );
float AliceTiki_FrameRadius( int handle, int anim, int frame );

int AliceTiki_SurfaceNameToNum( int handle, const char *name );
const char *AliceTiki_SurfaceNumToName( int handle, int surface );
int AliceTiki_SurfaceFlags( int handle, int surface );
int AliceTiki_SurfaceNumSkins( int handle, int surface );
float AliceTiki_SurfaceDamageMultiplier( int handle, int surface );

int AliceTiki_TagNumForName( int handle, const char *name );
const char *AliceTiki_TagNameForNum( int handle, int tag );

/* A tag's orientation for one frame of one animation. Retail's skeletal path
 * ignores `scale`; the controller records are the refEntity's or the entity
 * state's five 36-byte controllers, or null. */
void AliceTiki_TagOrientation( int handle, int anim, int frame, int tag, float scale,
	const fntech3_alice_bonectrl_t *controllers, float origin[3], float axis[3][3] );
/* The same with cross-blend and the legs/torso split. */
void AliceTiki_TagOrientationEx( int handle, int anim, int frame, int tag, float scale,
	const fntech3_alice_bonectrl_t *controllers, int crossblendAnim, int crossblendFrame,
	float crossblendLerp, qboolean useLegs, qboolean useTorso, int torsoAnim, int torsoFrame,
	int torsoCrossblendAnim, int torsoCrossblendFrame, float torsoCrossblendLerp,
	float origin[3], float axis[3][3] );
/* The orientation for the pose a render entity describes. */
void AliceTiki_TagLerpedOrientation( int handle, const fntech3_alice_refEntity_t *entity,
	int tag, float origin[3], float axis[3][3] );

/* ---- aliases ------------------------------------------------------------ */

/* The first four take a model's handle for the model's own list -- retail's
 * TIKI_Alias_* wrappers, 0x0045cc00 to 0x0045cc90 -- or -1 for the engine's
 * global list, 0x00423a40 to 0x00423af0. Only Add creates a model's list;
 * every global call creates the global one. A handle that names no model is
 * reported as TIKI_GetTiki reports it (0x0045c910) and does nothing.
 * Callers holding a model handle must therefore not pass -1 for "none". */
qboolean AliceAlias_Add( int handle, const char *alias, const char *realName,
	const char *parameters );
const char *AliceAlias_FindRandom( int handle, const char *alias );
void AliceAlias_Dump( int handle );
void AliceAlias_Clear( int handle );

/* These three exist for models only. A negative handle answers null or does
 * nothing, silently, as the game's wrappers do for a model index that has no
 * definition (0x00453ff0 to 0x00454090).
 *
 * The next line the model says for `alias`: `random` picks among the lines
 * not yet played, otherwise the first in sorted order; `entityNumber` is the
 * speaker, whose own history is kept unless the line is `global` or the
 * number is -1 (0x0045ccf0). */
const char *AliceAlias_FindDialog( int handle, const char *alias, int random, int entityNumber );
/* Restore a line's history as the game module does from a save: for the line
 * itself when `actor` is -1 (game import 86, 0x0045cd30), otherwise for that
 * actor, adding the actor if it is new (import 87, 0x0045cd70). Nothing
 * changes unless `alias` selects exactly one line. `played` is stored as a
 * byte. */
void AliceAlias_UpdateDialog( int handle, const char *alias, int actor, int timesPlayed,
	int played, int lastPlayed );
/* The model's list itself, in retail's layout, or null before anything was
 * added (game import 85, 0x0045cdb0). */
fntech3::alice::alias::List *AliceAlias_GetList( int handle );

/* ---- the file cache, for the renderer ----------------------------------- */

const fntech3::alice::tiki::File *AliceTiki_FindFile( const char *name );

#endif /* __cplusplus */

#endif
