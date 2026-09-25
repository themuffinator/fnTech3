/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
//
#ifndef __TR_TYPES_H
#define __TR_TYPES_H

#include "tr_q3_ihv_entities.h"

#define MAX_VIDEO_HANDLES	16

#define	MAX_DLIGHTS			32			// can't be increased, because bit flags are used on surfaces

// renderfx flags
#define	RF_MINLIGHT			0x0001		// always have some light (viewmodel, some items)
#define	RF_THIRD_PERSON		0x0002		// don't draw through eyes, only mirrors (player bodies, chat sprites)
#define	RF_FIRST_PERSON		0x0004		// only draw through eyes (view weapon, damage blood blob)
#define	RF_DEPTHHACK		0x0008		// for view weapon Z crunching

#define RF_CROSSHAIR		0x0010		// This item is a cross hair and will draw over everything similar to
										// DEPTHHACK in stereo rendering mode, with the difference that the
										// projection matrix won't be hacked to reduce the stereo separation as
										// is done for the gun.

#define	RF_NOSHADOW			0x0040		// don't add stencil shadows

#define RF_LIGHTING_ORIGIN	0x0080		// use refEntity->lightingOrigin instead of refEntity->origin
										// for lighting.  This allows entities to sink into the floor
										// with their origin going solid, and allows all parts of a
										// player to get the same lighting

#define	RF_SHADOW_PLANE		0x0100		// use refEntity->shadowPlane
#define	RF_WRAP_FRAMES		0x0200		// mod the model frames by the maxframes to allow continuous
										// animation without needing to know the frame count
#define	RF_CAP_FRAMES		0x0400		// Elite Force SP: hold a one-shot animation on the model's
										// last frame once it runs past it

// refdef flags
#define RDF_NOWORLDMODEL	0x0001		// used for player configuration screen
#define RDF_NOFOVCORRECTION 0x0002	// internal Q3/RTCW: retain authored viewport FOV
#define RDF_HYPERSPACE		0x0004		// teleportation effect
#define RDF_NOFIRSTPERSON	0x0008		// suppress first-person-only entities such as the view weapon

typedef struct {
	vec3_t		xyz;
	float		st[2];
	color4ub_t	modulate;
} polyVert_t;

// Canonical 2D geometry. A colorMask bit chooses an explicit corner color;
// other corners use the renderer's current 2D color when the command executes.
// This record is private to the engine/renderer API, never a module ABI.
typedef struct {
	vec2_t xy[4], st[4];
	color4ub_t colors[4];
	unsigned int colorMask;
	qhandle_t shader;
} refDrawQuad_t;

typedef struct poly_s {
	qhandle_t			hShader;
	int					numVerts;
	polyVert_t			*verts;
} poly_t;

typedef enum {
	RT_MODEL,
	RT_POLY,
	RT_SPRITE,
	RT_BEAM,
	RT_RAIL_CORE,
	RT_RAIL_RINGS,
	RT_LIGHTNING,
	RT_PORTALSURFACE,		// doesn't draw anything, just info for portals
	RT_SPLASH,
	RT_RAIL_CORE_TAPER,
	// Elite Force's procedural primitives; their shape is in refEntity_t.stvef.
	RT_STVEF_ORIENTEDSPRITE,
	RT_STVEF_ALPHAVERTPOLY,
	RT_STVEF_LINE,
	RT_STVEF_ORIENTEDLINE,
	RT_STVEF_LINE2,
	RT_STVEF_BEZIER,
	RT_STVEF_CYLINDER,
	RT_STVEF_ELECTRICITY,
	// American McGee's Alice's sprite model: a shader quad in world space.
	RT_ALICE_SPRITE,

	RT_MAX_REF_ENTITY_TYPE
} refEntityType_t;

// Engine-owned Elite Force effect state. Its module record keeps these in one
// union after shaderTime; the client adapter unpacks the member each primitive
// type reads, and turns the Elite Force render flags that have no shared
// meaning into fields of their own.
typedef struct {
	float width, width2, stscale, height, bias, deviation;
	vec3_t control1, control2;
	qboolean wrap, taper;
	color4ub_t vertexColors[4];	// RT_STVEF_ALPHAVERTPOLY corners
	qboolean fullbright;		// retail renderfx 0x10: brighten after grid light
	qboolean darken;			// retail renderfx 0x1: darken after grid light
	qboolean forceAlpha;		// retail renderfx 0x800: shaderRGBA alpha on single-texture stages
} stvefRenderEntity_t;

// Engine-owned RTCW animation/effect state. Module records are frozen in
// compat/ and translated before crossing the renderer API.
typedef struct {
	vec3_t torsoAxis[3];
	int torsoFrame, oldTorsoFrame;
	float torsoBacklerp;
	vec3_t scale, fireRiseDir;
	int fadeStartTime, fadeEndTime;
	float hilightIntensity;
	int reFlags, entityNum;
	qboolean highlight, blink;
} rtcwRenderEntity_t;

// Engine-owned Enemy Territory animation/effect state. ET's reference entity
// carries a per-frame model handle beside every frame index, because MDM
// meshes and MDX skeletons are separate models; RTCW's single MDS does not.
// Kept apart from the RTCW record so neither profile inherits the other's
// fields.
typedef struct {
	vec3_t torsoAxis[3];
	int torsoFrame, oldTorsoFrame;
	float torsoBacklerp;
	qhandle_t frameModel, torsoFrameModel;
	qhandle_t oldframeModel, oldTorsoFrameModel;
	vec3_t fireRiseDir;
	int fadeStartTime, fadeEndTime;
	float hilightIntensity;
	int reFlags, entityNum;
	qboolean highlight, blink, forceNoLod;
} wolfetRenderEntity_t;

// Engine-owned American McGee's Alice animation state. Its client game's
// 460-byte record is translated into this before it reaches the renderer; a
// TIKI model is posed from these, not from the Quake III frame fields.
typedef struct {
	int bone;			// -1 for an unused controller
	float quat[4];
	float scale;
} aliceBoneController_t;

typedef struct {
	int anim, oldanim;
	int frame, oldframe;
	float backlerp;
	int torsoAnim, torsoOldanim;
	int torsoFrame, torsoOldframe;
	float torsoBacklerp;
	int crossblendAnim, crossblendFrame;
	float crossblendLerp;		// negative when there is no cross-blend
	int torsoCrossblendAnim, torsoCrossblendFrame;
	float torsoCrossblendLerp;
	qboolean useLegs, useTorso;
	qboolean useControllers;	// retail applies them only when useAngles is set
	qboolean customShaderPass;	// draw the skins, not the custom shader (RF 0x10000)
	int renderfx;				// Alice's own render flags, at its own bit positions
								// (code/compat/alice_render_abi.h FNTECH3_ALICE_RF_*)
	float scale;
	byte surfaces[32];			// per-surface state: skin offset and hidden bit
	short shaderData[2];
	aliceBoneController_t controllers[5];
	const void *bones;			// renderer scratch: this frame's posed skeleton
} aliceRenderEntity_t;

// Medal of Honor: Allied Assault's model state. Its client game's 412-byte
// render entity (code/compat/mohaa_cgame_abi.h) is translated into this; the
// engine poses a TIKI model from it (code/renderercommon/tr_mohaa_render.h).
typedef struct {
	int index;					// an animation of the definition
	float time;					// seconds into it
	float weight;				// its share of the pose
} mohaaFrameInfo_t;

typedef struct {
	const void *tiki;			// the engine's definition, or NULL
	int entityNumber;			// whose skeleton, ENTITYNUM_NONE for none
	mohaaFrameInfo_t frameInfo[16];
	int boneTag[5];				// -1 for an unused controller
	float boneQuat[5][4];
	float actionWeight;
} mohaaPoseInput_t;

typedef struct {
	mohaaPoseInput_t pose;
	float scale;
	int renderfx;				// 2015's own render flags, at its own bit positions
	byte surfaces[32];			// per-surface state: skin offset, hidden bit, crossfade
	float shaderData[2];
	const float *bones;			// renderer scratch: this frame's bone frame
	float lod;					// renderer scratch: its level-of-detail metric, negative for full detail
} mohaaRenderEntity_t;

typedef struct {
	refEntityType_t	reType;
	int			renderfx;

	qhandle_t	hModel;				// opaque type outside refresh

	// most recent data
	vec3_t		lightingOrigin;		// so multi-part models can be lit identically (RF_LIGHTING_ORIGIN)
	float		shadowPlane;		// projection shadows go here, stencils go slightly lower

	vec3_t		axis[3];			// rotation vectors
	qboolean	nonNormalizedAxes;	// axis are not normalized, i.e. they have scale
	float		origin[3];			// also used as MODEL_BEAM's "from"
	int			frame;				// also used as MODEL_BEAM's diameter

	// previous data for frame interpolation
	float		oldorigin[3];		// also used as MODEL_BEAM's "to"
	int			oldframe;
	float		backlerp;			// 0.0 = current, 1.0 = old

	// texturing
	int			skinNum;			// inline skin index
	qhandle_t	customSkin;			// NULL for default skin
	qhandle_t	customShader;		// use one image for the entire thing

	// misc
	color4ub_t	shader;
	float		shaderTexCoord[2];	// texture coordinates used by tcMod entity modifiers

	// subtracted from refdef time to control effect start times
	floatint_t	shaderTime;			// -EC- set to union

	// extra sprite information
	float		radius;
	float		rotation;
	rtcwRenderEntity_t rtcw;
	wolfetRenderEntity_t wolfet;
	stvefRenderEntity_t stvef;
	fnQ3IHVEntityState_t q3IHV;
	aliceRenderEntity_t alice;
	mohaaRenderEntity_t mohaa;
} refEntity_t;


#define	MAX_RENDER_STRINGS			8
#define	MAX_RENDER_STRING_LENGTH	32

typedef struct {
	int mode, hint, startTime, finishTime;
	vec4_t color;
	float start, end;
	qboolean useEndForClip;
	float density;
	qboolean registered, drawsky, clearscreen;
	int dirty;
} rtcwRenderFog_t;

typedef struct {
	qboolean skyboxPortal, drawSkybox, underwater, drawingSky, snooperView;
	rtcwRenderFog_t fog;
} rtcwRenderView_t;

// Engine-owned Enemy Territory view state. ET keeps RTCW's embedded fog
// description but has no snooper view and no separate draw-skybox flag.
typedef struct {
	int mode, hint, startTime, finishTime;
	vec4_t color;
	float start, end;
	qboolean useEndForClip;
	float density;
	qboolean registered, drawsky, clearscreen;
} wolfetRenderFog_t;

typedef struct {
	qboolean skyboxPortal, underwater, drawingSky;
	wolfetRenderFog_t fog;
} wolfetRenderView_t;

// Engine-owned American McGee's Alice view state: Ritual's far plane -- a
// distance at which the view fogs out to a colour and, optionally, stops
// drawing -- and its sky portal, a second view drawn behind the sky from its
// own origin and axis, with an alpha shaders read. keepDepth asks the renderer
// not to clear the depth buffer first.
typedef struct {
	float farplaneDistance;
	vec3_t farplaneColor;
	qboolean farplaneCull;
	qboolean skyPortal;
	float skyAlpha;
	vec3_t skyOrigin;
	vec3_t skyAxis[3];
	qboolean keepDepth;
} aliceRenderView_t;

// The profiles whose views have that far plane and sky portal: Alice and
// F.A.K.K.2, and Medal of Honor, whose engine inherits them from Ritual's.
// Medal of Honor is named here, not made a Ritual profile.
#define R_GAME_HAS_RITUAL_VIEW( id ) ( COM_GAME_ID_IS_RITUAL( id ) || ( id ) == COM_GAME_MOHAA )

typedef struct {
	int			x, y, width, height;
	float		fov_x, fov_y;
	vec3_t		vieworg;
	vec3_t		viewaxis[3];		// transformation matrix

	// time in milliseconds for shader effects and other time dependent rendering issues
	int			time;

	int			rdflags;			// RDF_NOWORLDMODEL, etc

	// 1 bits will prevent the associated area from rendering at all
	byte		areamask[MAX_MAP_AREA_BYTES];

	// text messages for deform text shaders
	char		text[MAX_RENDER_STRINGS][MAX_RENDER_STRING_LENGTH];
	rtcwRenderView_t rtcw;
	wolfetRenderView_t wolfet;
	aliceRenderView_t alice;
} refdef_t;

/*
 * Visual-only liquid disturbance supplied by the engine to the renderer.
 * This is deliberately outside entityState/playerState and the VM trap ABI:
 * it must never affect prediction, snapshots, protocols, or demo playback.
 */
typedef enum {
	LIQUID_INTERACTION_PLAYER,
	LIQUID_INTERACTION_PROJECTILE
} liquidInteractionSource_t;

typedef struct {
	vec3_t		origin;
	float		radius;
	float		strength;
	int		time;
	liquidInteractionSource_t source;
} liquidInteraction_t;


/*
 * Visual-only submerged-view sample supplied by the engine to the renderer.
 * The engine owns the collision query because the renderer has no collision
 * model; the renderer owns the transition ramp because only it knows which
 * views are real world views.  Like the liquid disturbance record above, this
 * stays outside entityState/playerState and the VM trap ABI, so it cannot
 * affect prediction, snapshots, protocols, or demo playback.
 */
typedef struct {
	int		contents;	// liquid contents at the eye, zero when dry
	int		time;		// scene time of the sample, milliseconds
} underwaterView_t;


typedef enum {
	STEREO_CENTER,
	STEREO_LEFT,
	STEREO_RIGHT
} stereoFrame_t;


/*
** glconfig_t
**
** Contains variables specific to the OpenGL configuration
** being run right now.  These are constant once the OpenGL
** subsystem is initialized.
*/
typedef enum {
	TC_NONE,
	TC_S3TC,  // this is for the GL_S3_s3tc extension.
	TC_S3TC_ARB  // this is for the GL_EXT_texture_compression_s3tc extension.
} textureCompression_t;

typedef enum {
	GLDRV_ICD,					// driver is integrated with window system
								// WARNING: there are tests that check for
								// > GLDRV_ICD for minidriverness, so this
								// should always be the lowest value in this
								// enum set
	GLDRV_STANDALONE,			// driver is a non-3Dfx standalone driver
	GLDRV_VOODOO				// driver is a 3Dfx standalone driver
} glDriverType_t;

typedef enum {
	GLHW_GENERIC,			// where everything works the way it should
	GLHW_3DFX_2D3D,			// Voodoo Banshee or Voodoo3, relevant since if this is
							// the hardware type then there can NOT exist a secondary
							// display adapter
	GLHW_RIVA128,			// where you can't interpolate alpha
	GLHW_RAGEPRO,			// where you can't modulate alpha on alpha textures
	GLHW_PERMEDIA2			// where you don't have src*dst
} glHardwareType_t;

typedef enum {
	ROUTPUT_REQUEST_AUTO,
	ROUTPUT_REQUEST_SDR_SRGB,
	ROUTPUT_REQUEST_WINDOWS_SCRGB,
	ROUTPUT_REQUEST_HDR10_PQ,
	ROUTPUT_REQUEST_MACOS_EDR,
	ROUTPUT_REQUEST_LINUX_EXPERIMENTAL_HDR,
	ROUTPUT_REQUEST_COUNT
} rendererOutputRequest_t;

typedef enum {
	ROUTPUT_BACKEND_SDR_SRGB,
	ROUTPUT_BACKEND_WINDOWS_SCRGB,
	ROUTPUT_BACKEND_HDR10_PQ,
	ROUTPUT_BACKEND_MACOS_EDR,
	ROUTPUT_BACKEND_LINUX_EXPERIMENTAL_HDR,
	ROUTPUT_BACKEND_COUNT
} rendererOutputBackend_t;

typedef struct rendererDisplayOutput_s {
	qboolean				valid;
	int						displayIndex;
	char					videoDriver[64];
	char					displayName[128];
	char					reason[256];
	rendererOutputBackend_t	nativeBackend;
	qboolean				hdrEnabled;
	qboolean				hdrHeadroomValid;
	float					sdrWhiteNits;
	float					hdrHeadroom;
	float					maxLuminanceNits;
	float					maxFullFrameLuminanceNits;
	qboolean				iccProfileAvailable;
	int						iccProfileBytes;
	qboolean				windowsAdvancedColor;
	qboolean				windowsScRgbSupported;
	qboolean				windowsHdr10Supported;
	qboolean				macosEdrSupported;
	qboolean				linuxHdrExperimental;
	qboolean				waylandColorProtocol;
	qboolean				explicitLinuxHdrProtocol;
} rendererDisplayOutput_t;

static ID_INLINE const char *RendererOutputRequestName( rendererOutputRequest_t request )
{
	switch ( request ) {
	case ROUTPUT_REQUEST_AUTO:
		return "auto";
	case ROUTPUT_REQUEST_SDR_SRGB:
		return "sdr-srgb";
	case ROUTPUT_REQUEST_WINDOWS_SCRGB:
		return "windows-scrgb";
	case ROUTPUT_REQUEST_HDR10_PQ:
		return "hdr10-pq";
	case ROUTPUT_REQUEST_MACOS_EDR:
		return "macos-edr";
	case ROUTPUT_REQUEST_LINUX_EXPERIMENTAL_HDR:
		return "linux-experimental-hdr";
	default:
		return "unknown";
	}
}

static ID_INLINE const char *RendererOutputBackendName( rendererOutputBackend_t backend )
{
	switch ( backend ) {
	case ROUTPUT_BACKEND_SDR_SRGB:
		return "sdr-srgb";
	case ROUTPUT_BACKEND_WINDOWS_SCRGB:
		return "windows-scrgb";
	case ROUTPUT_BACKEND_HDR10_PQ:
		return "hdr10-pq";
	case ROUTPUT_BACKEND_MACOS_EDR:
		return "macos-edr";
	case ROUTPUT_BACKEND_LINUX_EXPERIMENTAL_HDR:
		return "linux-experimental-hdr";
	default:
		return "unknown";
	}
}

typedef struct glconfig_s {
	char					renderer_string[MAX_STRING_CHARS];
	char					vendor_string[MAX_STRING_CHARS];
	char					version_string[MAX_STRING_CHARS];
	char					extensions_string[BIG_INFO_STRING];

	int						maxTextureSize;			// queried from GL
	int						numTextureUnits;		// multitexture ability

	int						colorBits, depthBits, stencilBits;

	glDriverType_t			driverType;
	glHardwareType_t		hardwareType;

	qboolean				deviceSupportsGamma;
	textureCompression_t	textureCompression;
	qboolean				textureEnvAddAvailable;

	int						vidWidth, vidHeight;
	// aspect is the screen's physical width / height, which may be different
	// than scrWidth / scrHeight if the pixels are non-square
	// normal screens should be 4/3, but wide aspect monitors may be 16/9
	float					windowAspect;

	int						displayFrequency;

	// synonymous with "does rendering consume the entire screen?", therefore
	// a Voodoo or Voodoo2 will have this set to TRUE, as will a Win32 ICD that
	// used CDS.
	qboolean				isFullscreen;
	qboolean				stereoEnabled;
	qboolean				smpActive;		// UNUSED, present for compatibility
} glconfig_t;

#define	myftol(x) ((int)(x))

#if defined(_WIN32)
#define OPENGL_DRIVER_NAME	"opengl32"
#elif defined(MACOS_X)
#define OPENGL_DRIVER_NAME	"/System/Library/Frameworks/OpenGL.framework/Libraries/libGL.dylib"
#else
#define OPENGL_DRIVER_NAME	"libGL.so.1"
#endif

#endif	// __TR_TYPES_H
