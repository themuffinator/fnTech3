/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_MOHAA_TIKI_H
#define FNTECH3_MOHAA_TIKI_H

/* Medal of Honor: Allied Assault's model definitions (TIKI), as the engine's
 * MoHAA adapters reach them.
 *
 * The runtime is OpenMoHAA's TIKI and skeletor code, kept under code/mohaa/
 * (see code/mohaa/README.md). This header is the narrow C view of it that the
 * rest of the engine uses: the records stay opaque, except for the prefix the
 * retail modules themselves read, and every function here is one the game or
 * client game module is served, or one the engine needs to run the cache.
 * code/mohaa/mohaa_tiki_api.cpp includes both this header and the runtime's
 * own, so a declaration that drifts from its definition does not compile. */

#include "../qcommon/q_shared.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct dtiki_s dtiki_t;
typedef struct dtikianim_s dtikianim_t;
typedef struct tiki_cmd_s tiki_cmd_t;
typedef struct frameInfo_s frameInfo_t;
typedef struct AliasListNode_s AliasListNode_t;

/* The part of a definition the retail game module reads for itself: its name
 * and its animation record (gamex86 0x310c9087, 0x310126d0), whose name and
 * server init commands it walks (0x310c8f51..0x310c8f83). */
typedef struct mohaa_dtikicmd_s {
	int frame_num;
	int num_args;
	char **args;
} mohaa_dtikicmd_t;

/* ---- the cache -------------------------------------------------------- */

void TIKI_Begin( void );
void TIKI_End( void );
void TIKI_FinishLoad( void );
void TIKI_FreeAll( void );

dtiki_t *TIKI_RegisterTiki( const char *path );
dtiki_t *TIKI_RegisterTikiFlags( const char *path, qboolean use );
dtikianim_t *TIKI_RegisterTikiAnimFlags( const char *path, qboolean use );
dtiki_t *TIKI_FindTiki( const char *path );
void *TIKI_GetSkeletor( dtiki_t *tiki, int entnum );

/* ---- queries ---------------------------------------------------------- */

const char *TIKI_Name( dtiki_t *pmdl );
int TIKI_NumAnims( dtiki_t *pmdl );
int TIKI_NumSurfaces( dtiki_t *pmdl );
int TIKI_NumTags( dtiki_t *pmdl );
void TIKI_CalculateBounds( dtiki_t *pmdl, float scale, vec3_t mins, vec3_t maxs );
float TIKI_GlobalRadius( dtiki_t *pmdl );
int TIKI_NumHeadModels( dtikianim_t *tiki );
void TIKI_GetHeadModel( dtikianim_t *tiki, int num, char *name );
int TIKI_NumHeadSkins( dtikianim_t *tiki );
void TIKI_GetHeadSkin( dtikianim_t *tiki, int num, char *name );

const char *TIKI_Anim_NameForNum( dtiki_t *pmdl, int animnum );
int TIKI_Anim_NumForName( dtiki_t *pmdl, const char *name );
int TIKI_Anim_Random( dtiki_t *pmdl, const char *name );
int TIKI_Anim_NumFrames( dtiki_t *pmdl, int animnum );
float TIKI_Anim_Time( dtiki_t *pmdl, int animnum );
float TIKI_Anim_Frametime( dtiki_t *pmdl, int animnum );
void TIKI_Anim_Delta( dtiki_t *pmdl, int animnum, float *delta );
qboolean TIKI_Anim_HasDelta( dtiki_t *pmdl, int animnum );
void TIKI_Anim_DeltaOverTime( dtiki_t *pTiki, int iAnimnum, float fTime1, float fTime2, vec3_t vDelta );
int TIKI_Anim_Flags( dtiki_t *pmdl, int animnum );
int TIKI_Anim_FlagsSkel( dtiki_t *pmdl, int animnum );
qboolean TIKI_Anim_HasServerCommands( dtiki_t *pmdl, int animnum );
qboolean TIKI_Anim_HasClientCommands( dtiki_t *pmdl, int animnum );
float TIKI_Anim_CrossblendTime( dtiki_t *pmdl, int animnum );

qboolean TIKI_Frame_Commands_Server( dtiki_t *pmdl, int animnum, int framenum, tiki_cmd_t *tiki_cmd );
qboolean TIKI_Frame_Commands_Client( dtiki_t *pmdl, int animnum, int framenum, tiki_cmd_t *tiki_cmd );
qboolean TIKI_Frame_Commands_ClientTime( dtiki_t *pmdl, int animnum, float start, float end, tiki_cmd_t *tiki_cmd );

int TIKI_Surface_NameToNum( dtiki_t *pmdl, const char *name );
const char *TIKI_Surface_NumToName( dtiki_t *pmdl, int num );
int TIKI_Tag_NameToNum( dtiki_t *pmdl, const char *name );
const char *TIKI_Tag_NumToName( dtiki_t *pmdl, int iTagNum );

orientation_t TIKI_OrientationInternal( dtiki_t *tiki, int entnum, int tagnum, float scale );
qboolean TIKI_IsOnGroundInternal( dtiki_t *tiki, int entnum, int tagnum, float threshold );
void TIKI_SetPoseInternal( void *skeletor, const frameInfo_t *frameInfo, const int *bone_tag,
	const vec4_t *bone_quat, float actionWeight );
float TIKI_GetRadiusInternal( dtiki_t *tiki, int entnum, float scale );
float TIKI_GetCentroidRadiusInternal( dtiki_t *tiki, int entnum, float scale, float *centroid );
void TIKI_SetEyeTargetPos( dtiki_t *tiki, int entnum, vec3_t pos );

/* ---- the engine's wrappers (mohaa_tiki_api.cpp) ----------------------- */

/* The tag's world transform, the skeletor's own SkelMat4 record (four rows
 * of three floats), which game import 114 hands the module as a pointer. */
void *MOHAA_TIKI_TransformInternal( dtiki_t *tiki, int entnum, int tagnum );

/* The definition's animation record, or null. */
dtikianim_t *MOHAA_TIKI_Anim( dtiki_t *tiki );

/* Per-model alias lists, created on first use (game imports 119..123). */
qboolean MOHAA_TIKI_AliasAdd( dtiki_t *tiki, const char *alias, const char *name, const char *parameters );
const char *MOHAA_TIKI_AliasFindRandom( dtiki_t *tiki, const char *alias, AliasListNode_t **ret );
void MOHAA_TIKI_AliasDump( dtiki_t *tiki );
void MOHAA_TIKI_AliasUpdateDialog( dtikianim_t *tiki, const char *alias );

/* The engine's global alias list (game imports 125..128): ubersound's and
 * uberdialog's aliases, which the game adds as it parses them. */
qboolean MOHAA_GlobalAliasAdd( const char *alias, const char *name, const char *parameters );
const char *MOHAA_GlobalAliasFindRandom( const char *alias, AliasListNode_t **ret );
void MOHAA_GlobalAliasDump( void );
void MOHAA_GlobalAliasClear( void );

/* The alias lists the client game keeps for itself (client-game imports 120
 * to 127), each an AliasList_t it only ever handles by pointer. */
void *MOHAA_AliasListNew( const char *name );
qboolean MOHAA_AliasListAdd( void *list, const char *alias, const char *name, const char *parameters );
const char *MOHAA_AliasListFindRandom( void *list, const char *alias, AliasListNode_t **ret );
void MOHAA_AliasListFindRandomRange( void *list, const char *alias, int *minIndex, int *maxIndex,
	float *totalWeight );
void *MOHAA_AliasGetGlobalList( void );

/* A skeleton bone's number by name, or -1 (OpenMoHAA's dtiki_t
 * GetBoneNumFromName), and whether the definition is a character, whose
 * hits a deep trace resolves to a body location. */
int MOHAA_TIKI_BoneNumFromName( dtiki_t *tiki, const char *name );
qboolean MOHAA_TIKI_IsCharacter( dtiki_t *tiki );

/* A definition's server and client init command counts, for diagnostics. */
int MOHAA_TIKI_NumServerInitCommands( dtiki_t *tiki );

/* What the model cache holds (mohaa_engine.cpp). */
void MOHAA_ModelMemory( size_t *bytes, size_t *blocks );

/* ---- level of detail (mohaa_render_api.cpp) ---------------------------- */

/* 2015's level-of-detail cvars and its LOD tool's, for the client. */
void MOHAA_LodRegister( void );
/* Once a frame of the world view, from the view: while the game's LOD tool
 * runs, its sliders become the record of the mesh it edits, `lod_save`
 * writes it, and the model is placed where it measures `lod_LOD`. */
void MOHAA_LodToolFrame( const vec3_t viewOrigin, const vec3_t viewAxis[3], float fovX );

/* Ritual's path canonicaliser, which game import 30 also serves. */
void FS_CanonicalFilename( char *filename );

/* The renderers' view of a definition (mohaa_render_api.cpp,
 * code/renderercommon/tr_mohaa_render.h). */
struct mohaaRenderImport_s;
const struct mohaaRenderImport_s *MOHAA_RenderImport( void );

/* ---- the loading screen's progress ------------------------------------ */

/* Each resource a level's load has finished with, its kind in the first
 * letter: the runtime's own loads, and those the game and the client game
 * report through their LoadResource imports. The engine's menus count them
 * against the map's load record (cl_mohaa_ui.cpp); without a hook nothing
 * happens. */
void MOHAA_LoadResource( const char *name );
void MOHAA_SetLoadResourceHook( void ( *hook )( const char *name ) );
/* Whether a load record's entry is already in the runtime's caches, so that
 * this load will not report it (OpenMoHAA UI_IsResourceLoaded). */
qboolean MOHAA_ResourceCached( const char *name );

#ifdef __cplusplus
}
#endif

#endif
