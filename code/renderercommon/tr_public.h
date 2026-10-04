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
#ifndef __TR_PUBLIC_H
#define __TR_PUBLIC_H

#include "tr_types.h"
#include "tr_rtcw_marks.h"
#include "tr_rtcw_corona.h"
#include "tr_q3_ihv_particles.h"
#include "../qcommon/game_profile.h"
#include "vulkan/vulkan.h"

// 29: the IHV and Alice members were appended to both tables after a commit on
// another branch (b66b036) had already used 28 for a shorter layout.
// 30: Medal of Honor's model registration and the engine's TIKI table.
// 31: that branch's Elite Force SP scissor and raw-image members, merged after
// all of the above.
// 32: texture capabilities for profiles' frozen renderer records.
/* Version 33 appends engine-owned mutable MDR registration and pose updates. */
/* Version 34 appends the renderer-owned Academy world distance query. */
#define	REF_API_VERSION		34

//
// these are the functions exported by the refresh module
//
typedef enum {
	REF_KEEP_CONTEXT, // don't destroy window and context
	REF_KEEP_WINDOW,  // destroy context, keep window
	REF_DESTROY_WINDOW,
	REF_UNLOAD_DLL
} refShutdownCode_t;

typedef struct {
	// called before the library is unloaded
	// if the system is just reconfiguring, pass destroyWindow = qfalse,
	// which will keep the screen from flashing to the desktop.
	void	(*Shutdown)( refShutdownCode_t code );

	// All data that will be used in a level should be
	// registered before rendering any frames to prevent disk hits,
	// but they can still be registered at a later time
	// if necessary.
	//
	// BeginRegistration makes any existing media pointers invalid
	// and returns the current gl configuration, including screen width
	// and height, which can be used by the client to intelligently
	// size display elements
	void	(*BeginRegistration)( glconfig_t *config );
	qhandle_t (*RegisterModel)( const char *name );
	qhandle_t (*RegisterSkin)( const char *name );
	qhandle_t (*RegisterShader)( const char *name );
	qhandle_t (*RegisterShaderNoMip)( const char *name );
	void	(*LoadWorld)( const char *name );

	// the vis data is a large enough block of data that we go to the trouble
	// of sharing it with the clipmodel subsystem
	void	(*SetWorldVisData)( const byte *vis );

	// EndRegistration will draw a tiny polygon with each texture, forcing
	// them to be loaded into card memory
	void	(*EndRegistration)( void );

	// a scene is built up by calls to R_ClearScene and the various R_Add functions.
	// Nothing is drawn until R_RenderScene is called.
	void	(*ClearScene)( void );
	void	(*AddRefEntityToScene)( const refEntity_t *re, qboolean intShaderTime );
	void	(*AddPolyToScene)( qhandle_t hShader , int numVerts, const polyVert_t *verts, int num );
	int		(*LightForPoint)( vec3_t point, vec3_t ambientLight, vec3_t directedLight, vec3_t lightDir );
	void	(*AddLightToScene)( const vec3_t org, float intensity, float r, float g, float b );
	void	(*AddAdditiveLightToScene)( const vec3_t org, float intensity, float r, float g, float b );
	void	(*AddLinearLightToScene)( const vec3_t start, const vec3_t end, float intensity, float r, float g, float b );
	void	(*AddLiquidInteractionToScene)( const liquidInteraction_t *interaction );
	void	(*SetUnderwaterView)( const underwaterView_t *view );
	void	(*RenderScene)( const refdef_t *fd );
	void	(*AdvertisementBridge_UpdateLoadingViewParameters)( void );

	void	(*SetColor)( const float *rgba );	// NULL = 1,1,1,1
	void	(*DrawStretchPic) ( float x, float y, float w, float h,
		float s1, float t1, float s2, float t2, qhandle_t hShader );	// 0 = white

	// Draw images for cinematic rendering, pass as 32 bit rgba
	void	(*DrawStretchRaw)( int x, int y, int w, int h, int cols, int rows, const byte *data, int client, qboolean dirty );
	void	(*UploadCinematic)( int w, int h, int cols, int rows, const byte *data, int client, qboolean dirty );
	// Dedicated arbitrary-size RGBA surface for the client-owned WebUI. This
	// avoids cinematic handle collisions and legacy power-of-two constraints.
	void	(*DrawWebUISurface)( int x, int y, int w, int h, int cols, int rows, const byte *data, qboolean dirty );

	void	(*BeginFrame)( stereoFrame_t stereoFrame );

	// if the pointers are not NULL, timing info will be returned
	void	(*EndFrame)( int *frontEndMsec, int *backEndMsec );


	int		(*MarkFragments)( int numPoints, const vec3_t *points, const vec3_t projection,
				   int maxPoints, vec3_t pointBuffer, int maxFragments, markFragment_t *fragmentBuffer );
	int		(*MarkRTCWFragments)( int rotation, const vec3_t *corners, const vec3_t projection,
		int maxPoints, fnRTCWMarkPoint_t *points, int maxFragments, markFragment_t *fragments );
	void	(*AddRTCWCoronaToScene)( const fnRTCWCorona_t *corona );

	int		(*LerpTag)( orientation_t *tag,  qhandle_t model, int startFrame, int endFrame,
					 float frac, const char *tagName );
	void	(*ModelBounds)( qhandle_t model, vec3_t mins, vec3_t maxs );

#ifdef __USEA3D
	void    (*A3D_RenderGeometry) (void *pVoidA3D, void *pVoidGeom, void *pVoidMat, void *pVoidGeomStatus);
#endif
	void	(*RegisterFont)(const char *fontName, int pointSize, fontInfo_t *font);
	void	(*DrawScaledText)( int x, int y, const char *text, int fontHandle, float scale,
		int limit, float *maxX, qboolean forceColor );
	void	(*MeasureScaledText)( const char *text, const char *end, int fontHandle, float scale,
		int limit, float *bounds );
	qboolean (*GetScaledFontMetrics)( int fontHandle, float scale, float *outAscent,
		float *outDescent, float *outLineHeight );
	qhandle_t (*GetFontAtlasDebugShader)( int *outWidth, int *outHeight );
	void	(*RemapShader)(const char *oldShader, const char *newShader, const char *offsetTime);
	qboolean (*GetEntityToken)( char *buffer, int size );
	qboolean (*inPVS)( const vec3_t p1, const vec3_t p2 );

	void	(*TakeVideoFrame)( int h, int w, byte* captureBuffer, byte *encodeBuffer, qboolean motionJpeg );

	void	(*ThrottleBackend)( void );
	void	(*FinishBloom)( void );
	// Soften everything drawn so far, so an in-game menu drawn next is the only
	// sharp thing on screen. strength is the client's faded 0..1 amount.
	void	(*DrawMenuBlur)( float strength );

	void	(*SetColorMappings)( void );

	qboolean (*CanMinimize)( void ); // == fbo enabled

	const glconfig_t *(*GetConfig)( void );

	void	(*VertexLighting)( qboolean allowed );
	void	(*SyncRender)( void );

	// Registers caller-owned RGBA pixels as a stable 2D shader. The renderer
	// consumes the pixels synchronously and retains no pointer to the buffer.
	qhandle_t (*RegisterShaderFromRGBA)( const char *name, byte *rgba,
		int width, int height );

	// Retail Quake Live exposes the active view's projection helpers to native
	// cgame.  In particular, its POI path uses these after RenderScene to place
	// objective markers in the 640x480 HUD coordinate space. Keep these appended
	// so existing FnQ3 renderer exports retain their offsets.
	void	(*TransformModelToClip)( const vec3_t point, vec4_t eye, vec4_t clip );
	void	(*TransformClipToWindow)( const vec4_t clip, vec4_t normalized, vec4_t window );
	void	(*DrawQuad)( const refDrawQuad_t *quad );
	// RTCW enumerates named attachments using an entity and an inclusive start
	// index. A successful index of zero must not become a boolean failure.
	int		(*LerpTagEntity)( orientation_t *tag, const refEntity_t *entity,
		const char *name, int first );
	qboolean (*GetSkinModel)( qhandle_t skin, const char *type, char *name );
	qhandle_t (*GetShaderFromModel)( qhandle_t model, int surface, int withLightmap );
	void (*AddRTCWLightToScene)( const vec3_t origin, float intensity, float r, float g, float b, int flags );
	// ET names a radius and a brightness separately, and carries a shader and flags.
	void (*AddWolfETLightToScene)( const vec3_t origin, float radius, float intensity,
		float r, float g, float b, qhandle_t hShader, int flags );
	// ET's screen-space triangle fan, and the one saved view its portal sky needs.
	void (*AddWolfET2DPolys)( const polyVert_t *verts, int numVerts, qhandle_t hShader );
	// ET defines shaders at runtime instead of shipping them in a .shader file.
	qboolean (*LoadWolfETDynamicShader)( const char *name, const char *text );
	// ET asks for the index of an image it already loaded, or -1.
	int (*GetWolfETTextureId)( const char *name );
	void (*SaveWolfETViewParms)( void );
	void (*RestoreWolfETViewParms)( void );
	void (*SetRTCWFog)( int command, int first, int second, float r, float g, float b, float density );
	void (*SetWolfETGlobalFog)( qboolean restore, int duration, float r, float g, float b, float depthForOpaque );
	// ET's decals: a projector pressed onto the map, and every kept decal cleared.
	void (*ProjectWolfETDecal)( qhandle_t hShader, int numPoints, const vec3_t *points,
		const vec4_t projection, const vec4_t color, int lifeTime, int fadeTime );
	void (*ClearWolfETDecals)( void );
	// Elite Force registers effect shaders for the 3D scene: an image with no
	// script becomes a lit, depth-tested, mipmapped surface, not a 2D picture.
	qhandle_t (*RegisterShader3D)( const char *name );
	// Elite Force single player keeps a picture of the screen for its
	// savegames: the next frame drawn, averaged down to side by side RGBA
	// pixels with its bottom row first, and done set once they are there.
	// A picture of NULL withdraws a request that has not been met.
	void (*CaptureSTVEFThumbnail)( byte *rgba, int side, qboolean *done );
	// An image file's pixels as RGBA, its top row first, or NULL. The caller
	// frees them with Z_Free.
	byte *(*LoadImagePixels)( const char *name, int *width, int *height );
	// Points are copied synchronously; each rendered view constructs its own
	// IHV triangles. A rejected batch leaves the pending scene unchanged.
	qboolean (*AddIHVParticlesToScene)( int count, const fnQ3IHVParticle_t *particles );
	qboolean (*AddIHVEntitiesToScene)( int count, const fnQ3IHVEntity_t *entities );
	// American McGee's Alice: the model definition a registered model draws,
	// or -1 for a model that is not one; and the size of a shader's first
	// image, which its client game asks for to lay out its 2D art.
	int (*GetAliceTikiHandle)( qhandle_t model );
	void (*GetShaderImageSize)( qhandle_t shader, int *width, int *height );
	// Alice's swipes, the trails a swung weapon leaves: a swipe begins with its
	// time, lifetime and shader, and its points follow, each a pair of ends
	// and the time it was made.
	void (*AliceSwipeBegin)( float thistime, float life, qhandle_t shader );
	void (*AliceSwipePoint)( const vec3_t p1, const vec3_t p2, float time );
	// Alice's model radius: a definition's own, or half a sprite's larger side
	// times its scale; negative for a model that is neither.
	float (*GetAliceModelRadius)( qhandle_t model );
	// Alice's dynamic light with its client game's type bits: 1 a lens flare,
	// 2 a view lens flare (retail draws both as the light's flare), 4 an
	// additive light. NULL in a renderer that draws no Alice lens flares.
	void (*AddAliceLightToScene)( const vec3_t org, float intensity, float r, float g, float b, int type );
	// Alice's savegame picture, taken from the next frame drawn as retail's
	// `screenshot <path> <width> <height>` reduces its own read of the frame
	// (code/compat/alice_saveshot.h): width by height grey RGB pixels, or
	// Heavy Metal: F.A.K.K.2's in colour as blue, green, red, the bottom row
	// first, with done set once they are there. A picture of NULL withdraws a
	// request that has not been met.
	void (*CaptureAliceSaveshot)( byte *rgb, int width, int height, qboolean *done );
	// Medal of Honor: a model for one of the engine's TIKI definitions,
	// posed and skinned through the engine's table (ri.MOHAARender). NULL in
	// a renderer that draws none.
	qhandle_t (*RegisterMOHAATiki)( const char *name, const void *tiki );
	// Medal of Honor's decal fragments (tr_mohaa_marks_surface.h): model 0
	// cuts the world with its terrain, writing each fragment's terrain patch
	// plus one, or 0, to patches (which may be NULL); another model is a
	// brush model, cut in its own space, the decal given in the world's and
	// taken into it by angles and origin. radiusSquared, the decal's squared
	// size, keeps small decals off terrain. NULL in a renderer that cuts none.
	int (*MarkMOHAAFragments)( int model, const vec3_t angles, const vec3_t origin, int numPoints,
		const vec3_t *points, const vec3_t projection, int maxPoints, vec3_t pointBuffer, int maxFragments,
		markFragment_t *fragments, int *patches, float radiusSquared );
	// Elite Force SP's 2D clip rectangle, in framebuffer pixels. Negative x
	// restores the whole framebuffer. A new 3D/2D transition resets it too.
	void (*SetSTVEFScissor)( float x, float y, float w, float h );
	// Opaque SP UI image, modulated by its own brightness rather than the UI color.
	void (*DrawSTVEFRaw)( int x, int y, int w, int h, int cols, int rows,
		const byte *data, float lightValue );
	// Active backend capabilities for profiles whose frozen renderer record
	// includes these fields. No external module receives this engine table.
	void (*GetTextureCapabilities)( float *maximumAnisotropy, qboolean *clampToEdgeAvailable );

	/* Engine-only models: fresh ownerNonce for each registration lifetime. */
	qhandle_t (*RegisterRuntimeMDR)( uint64_t ownerNonce, const void *bytes, int length );
	qboolean (*UpdateRuntimeMDRPose)( qhandle_t handle, uint64_t ownerNonce,
		const float *matrices, int boneCount, const vec3_t mins, const vec3_t maxs );

	/* MP world visibility limit; false before load or after shutdown. */
	qboolean (*GetJKADistanceCull)( float *distance );

} refexport_t;

//
// these are the functions imported by the refresh module
//
typedef struct refimport_s {
	// print message on the local console
	void	FORMAT_PRINTF(2, 3) (QDECL *Printf)( printParm_t printLevel, const char *fmt, ... );

	// abort the game
	void	NORETURN_PTR FORMAT_PRINTF(2, 3)(QDECL *Error)( errorParm_t errorLevel, const char *fmt, ... );

	// milliseconds should only be used for profiling, never
	// for anything game related.  Get time from the refdef
	int		(*Milliseconds)( void );

	int64_t	(*Microseconds)( void );

	// stack based memory allocation for per-level things that
	// won't be freed
#ifdef HUNK_DEBUG
	void	*(*Hunk_AllocDebug)( int size, ha_pref pref, char *label, char *file, int line );
#else
	void	*(*Hunk_Alloc)( int size, ha_pref pref );
#endif
	void	*(*Hunk_AllocateTempMemory)( int size );
	void	(*Hunk_FreeTempMemory)( void *block );

	// dynamic memory allocator for things that need to be freed
	void	*(*Malloc)( int bytes );
	void	(*Free)( void *buf );
	void	(*FreeAll)( void );

	cvar_t	*(*Cvar_Get)( const char *name, const char *value, int flags );
	void	(*Cvar_Set)( const char *name, const char *value );
	void	(*Cvar_SetValue) (const char *name, float value);
	void	(*Cvar_CheckRange)( cvar_t *cv, const char *minVal, const char *maxVal, cvarValidator_t type );
	void	(*Cvar_SetDescription)( cvar_t *cv, const char *description );

	void	(*Cvar_SetGroup)( cvar_t *var, cvarGroup_t group );
	int		(*Cvar_CheckGroup)( cvarGroup_t group );
	void	(*Cvar_ResetGroup)( cvarGroup_t group, qboolean resetModifiedFlags );

	void	(*Cvar_VariableStringBuffer)( const char *var_name, char *buffer, int bufsize );
	const char *(*Cvar_VariableString)( const char *var_name );
	int		(*Cvar_VariableIntegerValue)( const char *var_name );

	void	(*Cmd_AddCommand)( const char *name, void(*cmd)(void) );
	void	(*Cmd_RemoveCommand)( const char *name );

	int		(*Cmd_Argc) (void);
	const char	*(*Cmd_Argv) (int i);

	void	(*Cmd_ExecuteText)( cbufExec_t exec_when, const char *text );

	byte	*(*CM_ClusterPVS)(int cluster);
	void	(*CM_BoxTrace)( trace_t *results, const vec3_t start, const vec3_t end,
						const vec3_t mins, const vec3_t maxs,
						clipHandle_t model, int brushmask, qboolean capsule );

	// visualization for debugging collision detection
	void	(*CM_DrawDebugSurface)( void (*drawPoly)(int color, int numPoints, float *points) );

	// a qfalse return means the file does not exist
	// NULL can be passed for buf to just determine existence
	//int		(*FS_FileIsInPAK)( const char *name, int *pCheckSum );
	int		(*FS_ReadFile)( const char *name, void **buf );
	void	(*FS_FreeFile)( void *buf );
	char **	(*FS_ListFiles)( const char *name, const char *extension, int *numfilesfound );
	void	(*FS_FreeFileList)( char **filelist );
	void	(*FS_WriteFile)( const char *qpath, const void *buffer, int size );
	qboolean (*FS_FileExists)( const char *file );

	// cinematic stuff
	void	(*CIN_UploadCinematic)( int handle );
	int		(*CIN_PlayCinematic)( const char *arg0, int xpos, int ypos, int width, int height, int bits );
	e_status (*CIN_RunCinematic)( int handle );

	void	(*CL_WriteAVIVideoFrame)( const byte *buffer, int size );

	size_t	(*CL_SaveJPGToBuffer)( byte *buffer, size_t bufSize, int quality, int image_width, int image_height, byte *image_buffer, int padding );
	void	(*CL_SaveJPG)( const char *filename, int quality, int image_width, int image_height, byte *image_buffer, int padding );
	void	(*CL_LoadJPG)( const char *filename, unsigned char **pic, int *width, int *height );

	qboolean (*CL_IsMinimized)( void );
	void	(*CL_SetScaling)( float factor, int captureWidth, int captureHeight );

	void	(*Sys_SetClipboardBitmap)( const byte *bitmap, int size );
	qboolean(*Sys_LowPhysicalMemory)( void );

	int		(*Com_RealTime)( qtime_t *qtime );
	void	(*SetClientMessageRendererNodeCount)( int nodeCount );
	void	(*PublishGameScreenshot)( const char *id, const char *name );
	void	(*AdvertisementBridge_RefreshLoadingViewParameters)( void );
	int		(*AdvertisementBridge_GetCellDisplayState)( int cellId );
	void	(*AdvertisementBridge_GetCellLabel)( int cellId, char *buffer, int bufferSize );
	int		(*AdvertisementBridge_GetLabelList1Count)( void );
	void	(*AdvertisementBridge_GetLabelList1Entry)( int index, char *buffer, int bufferSize );
	int		(*AdvertisementBridge_GetLabelList2Count)( void );
	void	(*AdvertisementBridge_GetLabelList2Entry)( int index, char *buffer, int bufferSize );

	// platform-dependent functions
	void(*GLimp_InitGamma)(glconfig_t *config);
	void(*GLimp_SetGamma)(unsigned char red[256], unsigned char green[256], unsigned char blue[256]);
	void(*GLimp_QueryDisplayOutput)(rendererDisplayOutput_t *output);

	// OpenGL
	void	(*GLimp_Init)( glconfig_t *config );
	void	(*GLimp_Shutdown)( qboolean unloadDLL );
	void	(*GLimp_EndFrame)( void );
	void*	(*GL_GetProcAddress)( const char *name );

	// Vulkan
	void	(*VKimp_Init)( glconfig_t *config );
	void	(*VKimp_Shutdown)( qboolean unloadDLL );
	void*	(*VK_GetInstanceProcAddr)( VkInstance instance, const char *name );
	qboolean (*VK_CreateSurface)( VkInstance instance, VkSurfaceKHR *pSurface );

	// The engine's immutable game selection owns ambiguous asset versions.
	comGameBspFormat_t (*GameBspFormat)( int version );
	// Snapshot of the immutable selection, supplied before renderer startup.
	comGameId_t gameId;

	// American McGee's Alice keeps one cache of model definitions for its
	// game, its client game and its renderer (code/qcommon/alice_tiki.h). The
	// renderer registers through it and reads the definition it hands back --
	// a const AliceTikiModel -- without ever freeing or changing it.
	int		(*AliceTikiRegister)( const char *name );
	const void *(*AliceTikiGet)( int handle );
	// Alice's debug lines: the game module's array of 48-byte records
	// (code/compat/alice_render_abi.h fntech3_alice_debugline_t) and its
	// count this frame, or null when the game has published none.
	const void *(*AliceDebugLines)( int *count );
	// Alice's client-game trace, the one its CG_Init leaves in the shared
	// block at +0x1c, as retail's renderer calls it for lens-flare occlusion
	// (alice.exe 0x004a78c6): a point trace from start to end that clips the
	// client game's entities, skipping none. Fills the fraction, surface flags
	// and contents; qfalse when there is no client game to ask, in which case
	// the renderer traces the world alone. May be NULL.
	qboolean (*AliceTrace)( const vec3_t start, const vec3_t end, int mask,
		float *fraction, int *surfaceFlags, int *contents );
	// Alice's loading-screen stage (alice.exe 0x0040b590), which the world
	// load sets to 7 as it starts and 8 as it ends (RE_LoadWorldMap
	// 0x0049afe0, 0x0049b8f4). May be NULL.
	void	(*AliceLoadingStage)( int stage );

	// Medal of Honor's TIKI definitions: their surfaces, an entity's pose and
	// a surface skinned by it (code/renderercommon/tr_mohaa_render.h). May be
	// NULL.
	const struct mohaaRenderImport_s *MOHAARender;

} refimport_t;

extern	refimport_t	ri;

// this is the only function actually exported at the linker level
// If the module can't init to a valid rendering state, NULL will be
// returned.
#ifdef USE_RENDERER_DLOPEN
typedef	refexport_t* (QDECL *GetRefAPI_t) (int apiVersion, refimport_t * rimp);
#else
refexport_t*GetRefAPI( int apiVersion, refimport_t *rimp );
#endif

#endif	// __TR_PUBLIC_H
