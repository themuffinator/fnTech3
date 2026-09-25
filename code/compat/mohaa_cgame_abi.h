/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_COMPAT_MOHAA_CGAME_ABI_H
#define FNTECH3_COMPAT_MOHAA_CGAME_ABI_H

/* Medal of Honor: Allied Assault 1.00's client-game records: the ones
 * main/cgamex86.dll hands the engine or has the engine fill, beside the ones
 * it shares with the game (code/compat/mohaa_game_abi.h).
 *
 * Every size here was measured in the retail library. The member names and
 * orders are OpenMoHAA's (GPL-2.0-or-later), which describe the later
 * modules; where 1.00 differs, the library decides and the note says how.
 * See docs/fntech3/2026-09-24-mohaa-contracts.md. */

#include "mohaa_game_abi.h"

enum {
	/* The client game's copies of the configstrings (cgs.gameState at
	 * 0x302a5b20): 2,736 offsets, then the text, which CG_ConfigString reads
	 * at 0x302a85e0 (0x30023504), then the count; the graphics configuration
	 * follows at 0x302b02e4 (0x3002350e), so the text is 32,000 bytes. */
	FNTECH3_MOHAA_MAX_GAMESTATE_CHARS = 32000,
	FNTECH3_MOHAA_GAMESTATE_BYTES = 42948,
	/* The graphics configuration: Quake III's older one, whose strings are
	 * three of MAX_STRING_CHARS (2,048 here) and extensions twice that, so the
	 * video width the client game reads is at 10,280 (0x3002351c), and its
	 * two screen scales follow the record at 10,324 (0x30023528). */
	FNTECH3_MOHAA_GLCONFIG_BYTES = 10324,
	/* A render entity: the client game clears 0x19c bytes before filling one
	 * (0x3002063f and nine more sites). */
	FNTECH3_MOHAA_REFENTITY_BYTES = 412,
	/* The view: CG_DrawActiveFrame clears 0xbc bytes of it (0x30047b1d). */
	FNTECH3_MOHAA_REFDEF_BYTES = 188,
	/* The HUD draw elements the engine keeps for the client game: 256 of
	 * 2,228 bytes, the stride its readers index by. */
	FNTECH3_MOHAA_MAX_HUDDRAW_ELEMENTS = 256,
	FNTECH3_MOHAA_HUDDRAW_ELEMENT_BYTES = 2228,
	/* The view-model animation state the engine keeps across client-game
	 * restarts: sixteen frame infos, eleven words and an 80-byte item name;
	 * the last member the client game reads is at 308 (0x30049828). */
	FNTECH3_MOHAA_CLIENT_ANIM_BYTES = 312,
	FNTECH3_MOHAA_MAX_RES_NAME = 64,
	/* The client game's gamestate, snapshot and command numbers. */
	FNTECH3_MOHAA_CMD_BACKUP = 128,
	FNTECH3_MOHAA_MAX_MAP_AREA_BYTES = 32,
	/* The most a permanent decal is cut into: the mark cutter both exports
	 * 16 and 17 call (0x30023b10) asks the renderer for at most 384 points
	 * in 128 fragments, and the caller's arrays take them. */
	FNTECH3_MOHAA_MARK_FRAGMENTS = 128,
	FNTECH3_MOHAA_MARK_POINTS = 384,
	/* A decal laid along a path, which exports 17 and 18 read and write:
	 * both touch every word up to 0x94. */
	FNTECH3_MOHAA_TREAD_MARK_BYTES = 152
};

#ifdef __cplusplus
namespace fntech3::mohaa {

/* The client game's gamestate, at 1.00's sizes. */
struct GameState {
	std::int32_t stringOffsets[FNTECH3_MOHAA_MAX_CONFIGSTRINGS];
	char stringData[FNTECH3_MOHAA_MAX_GAMESTATE_CHARS];
	std::int32_t dataCount;
};
static_assert( sizeof( GameState ) == FNTECH3_MOHAA_GAMESTATE_BYTES );

struct GlConfig {
	char renderer_string[FNTECH3_MOHAA_MAX_STRING_CHARS];
	char vendor_string[FNTECH3_MOHAA_MAX_STRING_CHARS];
	char version_string[FNTECH3_MOHAA_MAX_STRING_CHARS];
	char extensions_string[2 * FNTECH3_MOHAA_MAX_STRING_CHARS];
	std::int32_t maxTextureSize;
	std::int32_t numTextureUnits;
	std::int32_t colorBits;
	std::int32_t depthBits;
	std::int32_t stencilBits;
	std::int32_t driverType;
	std::int32_t hardwareType;
	std::int32_t deviceSupportsGamma;
	std::int32_t textureCompression;
	std::int32_t textureEnvAddAvailable;
	std::int32_t vidWidth;
	std::int32_t vidHeight;
	float windowAspect;
	std::int32_t displayFrequency;
	std::int32_t isFullscreen;
	std::int32_t stereoEnabled;
	std::int32_t smpActive;
	std::int32_t registerCombinerAvailable;
	std::int32_t secondaryColorAvailable;
	std::int32_t VAR;
	std::int32_t fence;
};
static_assert( sizeof( GlConfig ) == FNTECH3_MOHAA_GLCONFIG_BYTES );
static_assert( offsetof( GlConfig, vidWidth ) == 10280 );
static_assert( offsetof( GlConfig, vidHeight ) == 10284 );

/* The render entity. OpenMoHAA's is 428 bytes and ordered after ioquake3's;
 * 1.00's is 412 and keeps F.A.K.K.2's order with the skeletal members in
 * place of its animation numbers. Every offset below was read off the client
 * game's own stores: CG_Mover (0x30020630) puts the origin at 72, the same
 * origin again at 288, the axis at 32, `(cg.time >> 6) & 1` at 300 and
 * ENTITYNUM_NONE at 328, exactly as Quake III's CG_Mover fills origin,
 * oldorigin, axis, skinNum and (2015's) entityNumber; the model animator
 * (0x3002a23c) writes the first frame info's weight of 1.0 at 92 and the
 * action weight at 276, the old model at 12, the surfaces at 332, two floats
 * of shader data at 364, the bone pointers at 372 and 376, and tests the
 * definition pointer at 388; the sprite code writes the radius at 404. */
struct RefEntity {
	std::int32_t reType;                               /* 0 */
	std::int32_t renderfx;                             /* 4 */
	std::int32_t hModel;                               /* 8 */
	std::int32_t hOldModel;                            /* 12 */
	float lightingOrigin[3];                           /* 16 */
	float shadowPlane;                                 /* 28 */
	float axis[3][3];                                  /* 32 */
	std::int32_t nonNormalizedAxes;                    /* 68 */
	float origin[3];                                   /* 72 */
	FrameInfo frameInfo[FNTECH3_MOHAA_MAX_FRAMEINFOS]; /* 84 */
	float actionWeight;                                /* 276 */
	std::int16_t wasframe;                             /* 280 */
	float scale;                                       /* 284 */
	float oldorigin[3];                                /* 288 */
	std::int32_t skinNum;                              /* 300 */
	std::int32_t customSkin;                           /* 304 */
	std::int32_t customShader;                         /* 308 */
	std::uint8_t shaderRGBA[4];                        /* 312 */
	float shaderTexCoord[2];                           /* 316 */
	float shaderTime;                                  /* 324 */
	std::int32_t entityNumber;                         /* 328 */
	std::uint8_t surfaces[FNTECH3_MOHAA_MAX_MODEL_SURFACES]; /* 332 */
	float shader_data[2];                              /* 364 */
	std::uint32_t bone_tag;                            /* 372, int * */
	std::uint32_t bone_quat;                           /* 376, vec4_t * */
	std::uint32_t of;                                  /* 380, renderer use */
	std::uint32_t nf;                                  /* 384, renderer use */
	std::uint32_t tiki;                                /* 388, dtiki_t * */
	std::int32_t bonestart;                            /* 392, renderer use */
	std::int32_t morphstart;                           /* 396, renderer use */
	std::int32_t hasMorph;                             /* 400, renderer use */
	float radius;                                      /* 404 */
	float rotation;                                    /* 408 */
};
static_assert( sizeof( RefEntity ) == FNTECH3_MOHAA_REFENTITY_BYTES );
static_assert( offsetof( RefEntity, axis ) == 32 );
static_assert( offsetof( RefEntity, origin ) == 72 );
static_assert( offsetof( RefEntity, frameInfo ) == 84 );
static_assert( offsetof( RefEntity, actionWeight ) == 276 );
static_assert( offsetof( RefEntity, scale ) == 284 );
static_assert( offsetof( RefEntity, oldorigin ) == 288 );
static_assert( offsetof( RefEntity, skinNum ) == 300 );
static_assert( offsetof( RefEntity, shaderRGBA ) == 312 );
static_assert( offsetof( RefEntity, entityNumber ) == 328 );
static_assert( offsetof( RefEntity, surfaces ) == 332 );
static_assert( offsetof( RefEntity, shader_data ) == 364 );
static_assert( offsetof( RefEntity, tiki ) == 388 );
static_assert( offsetof( RefEntity, radius ) == 404 );

/* The render entity types, Quake III's (RT_MODEL 0 .. RT_PORTALSURFACE 7). */
enum RefEntityType {
	kRtModel = 0,
	kRtPoly = 1,
	kRtSprite = 2,
	kRtBeam = 3,
	kRtRailCore = 4,
	kRtRailRings = 5,
	kRtLightning = 6,
	kRtPortalSurface = 7,
	kRtMax = 8
};

/* The view: Quake III's without its deform text, then the far plane and the
 * sky portal. CG_DrawActiveFrame's view setup (0x30047b17) copies the far
 * plane distance, colour and cull flag to 112..128, the portal flag and alpha
 * to 132 and 136, the portal origin to 140 and its axis to 152. */
struct Refdef {
	std::int32_t x, y, width, height;
	float fov_x, fov_y;
	float vieworg[3];
	float viewaxis[3][3];
	std::int32_t time;
	std::int32_t rdflags;
	std::uint8_t areamask[FNTECH3_MOHAA_MAX_MAP_AREA_BYTES];
	float farplane_distance;
	float farplane_color[3];
	std::int32_t farplane_cull;
	std::int32_t sky_portal;
	float sky_alpha;
	float sky_origin[3];
	float sky_axis[3][3];
};
static_assert( sizeof( Refdef ) == FNTECH3_MOHAA_REFDEF_BYTES );
static_assert( offsetof( Refdef, farplane_distance ) == 112 );
static_assert( offsetof( Refdef, sky_portal ) == 132 );
static_assert( offsetof( Refdef, sky_axis ) == 152 );

/* Quake III's polygon vertex; the client game submits four at a time. */
struct PolyVert {
	float xyz[3];
	float st[2];
	std::uint8_t modulate[4];
};
static_assert( sizeof( PolyVert ) == 24 );

/* A decal's fragment: the client game walks them 12 bytes apart and reads
 * the point count at 4 (0x30023bb8, 0x30023bcc). The third member names the
 * terrain patch or brush model the fragment lies on. */
struct MarkFragment {
	std::int32_t firstPoint;
	std::int32_t numPoints;
	std::int32_t iIndex;
};
static_assert( sizeof( MarkFragment ) == 12 );

/* A decal laid along a path by the decal editor, which keeps it; exports 17
 * (0x300275d6) and 18 (0x30028061) read and write it. Export 18 compares the
 * time at 4 with the frame's and clears 0x14..0x60 once it is more than 500
 * ms old; both touch every word to 0x94 except the shader at 8. The
 * names are OpenMoHAA's treadMark_t (q_shared.h), whose offsets these fit. */
struct TreadMark {
	std::int32_t iReferenceNumber;
	std::int32_t iLastTime;
	std::int32_t hTreadShader;
	std::int32_t iState;
	float fWidth;
	float vStartDir[3];
	float vStartVerts[2][3];
	float fStartTexCoord;
	float fStartAlpha;
	float vMidPos[3];
	float vMidVerts[2][3];
	float fMidTexCoord;
	float fMidAlpha;
	float vEndPos[3];
	float vEndVerts[2][3];
	float fEndTexCoord;
	float fEndAlpha;
};
static_assert( sizeof( TreadMark ) == FNTECH3_MOHAA_TREAD_MARK_BYTES );
static_assert( offsetof( TreadMark, iLastTime ) == 0x04 );
static_assert( offsetof( TreadMark, vStartDir ) == 0x14 );
static_assert( offsetof( TreadMark, vMidPos ) == 0x40 );
static_assert( offsetof( TreadMark, vMidVerts ) == 0x4c );
static_assert( offsetof( TreadMark, vEndVerts ) == 0x78 );
static_assert( offsetof( TreadMark, fEndAlpha ) == 0x94 );

/* The engine-kept HUD element the server's draw messages fill. */
struct HudDrawElement {
	std::int32_t hShader;
	char shaderName[FNTECH3_MOHAA_MAX_RES_NAME];
	std::int32_t iX;
	std::int32_t iY;
	std::int32_t iWidth;
	std::int32_t iHeight;
	float vColor[4];
	std::int32_t iHorizontalAlign;
	std::int32_t iVerticalAlign;
	std::int32_t bVirtualScreen;
	char string[FNTECH3_MOHAA_MAX_STRING_CHARS];
	char fontName[FNTECH3_MOHAA_MAX_RES_NAME];
	std::uint32_t pFont;	/* fontheader_t * */
};
static_assert( sizeof( HudDrawElement ) == FNTECH3_MOHAA_HUDDRAW_ELEMENT_BYTES );

/* The view-model animation state. The frame infos come first; the client
 * game compares the last animation at 192 with -1 thirteen times, reads the
 * slot at 200, the duration at 204, clears the crossblend flag at 208, reads
 * the last equipped weapon at 212 and the prefix index at 296, and adds the
 * position offset at 300..308. */
struct ClientAnim {
	FrameInfo g_VMFrameInfo[FNTECH3_MOHAA_MAX_FRAMEINFOS];
	std::int32_t g_iLastVMAnim;
	std::int32_t g_iLastVMAnimChanged;
	std::int32_t g_iCurrentVMAnimSlot;
	std::int32_t g_iCurrentVMDuration;
	std::int32_t g_bCrossblending;
	std::int32_t g_iLastEquippedWeaponStat;
	char g_szLastActiveItem[80];
	std::int32_t g_iLastAnimPrefixIndex;
	float g_vCurrentVMPosOffset[3];
};
static_assert( sizeof( ClientAnim ) == FNTECH3_MOHAA_CLIENT_ANIM_BYTES );
static_assert( offsetof( ClientAnim, g_iLastVMAnim ) == 192 );
static_assert( offsetof( ClientAnim, g_iLastAnimPrefixIndex ) == 296 );

/* The stopwatch the server's CGM sets: the client game reads its start at 0
 * and end at 4 (0x3001fc33, 0x3001fc45); 1.00 has no third member. */
struct StopWatch {
	std::int32_t iStartTime;
	std::int32_t iEndTime;
};
static_assert( sizeof( StopWatch ) == 8 );

/* A map shader's name and flags, as GetShader hands them back: the first
 * three members of the map's 140-byte shader record. */
struct BaseShader {
	char shader[64];
	std::int32_t surfaceFlags;
	std::int32_t contentFlags;
};
static_assert( sizeof( BaseShader ) == 72 );

} // namespace fntech3::mohaa
#endif

#endif
