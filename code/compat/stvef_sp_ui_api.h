/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_STVEF_SP_UI_API_H
#define FNTECH3_STVEF_SP_UI_API_H

#include "stvef_game_abi.h"

/*
 * Elite Force SP's menus: efuix86.dll exports GetUIAPI, which takes nothing
 * and returns the menus' twelve-entry export table. Retail stvoy.exe 1.20
 * copies that table (0x30 bytes), fills the 59 imports in this order on its
 * stack (CL_InitUI 0x40a4a0) and passes them to the first export with API
 * version 2. It opens menus by name through the export at +0x18 and sends
 * Escape to the one at +8. Names follow Raven's SP SDK; no text from it is
 * incorporated. Win32 x86 only: every function is __cdecl.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* connstate_t without Quake III's authorizing state. */
enum {
	FNTECH3_STVEF_SP_CA_UNINITIALIZED = 0,
	FNTECH3_STVEF_SP_CA_DISCONNECTED = 1,
	FNTECH3_STVEF_SP_CA_CONNECTING = 2,
	FNTECH3_STVEF_SP_CA_CHALLENGING = 3,
	FNTECH3_STVEF_SP_CA_CONNECTED = 4,
	FNTECH3_STVEF_SP_CA_LOADING = 5,
	FNTECH3_STVEF_SP_CA_PRIMED = 6,
	FNTECH3_STVEF_SP_CA_ACTIVE = 7,
	FNTECH3_STVEF_SP_CA_CINEMATIC = 8
};

typedef struct fntech3_stvef_sp_ui_import_s {
	void (*Printf)( const char *fmt, ... );
	void (*Error)( int32_t level, const char *fmt, ... );
	void (*Cvar_Set)( const char *name, const char *value );
	float (*Cvar_VariableValue)( const char *name );
	void (*Cvar_VariableStringBuffer)( const char *name, char *buffer, int32_t size );
	void (*Cvar_SetValue)( const char *name, float value );
	void (*Cvar_Reset)( const char *name );
	void (*Cvar_Create)( const char *name, const char *value, int32_t flags );
	void (*Cvar_InfoStringBuffer)( int32_t bit, char *buffer, int32_t size );
	int32_t (*Argc)( void );
	void (*Argv)( int32_t n, char *buffer, int32_t size );
	void (*Cmd_ExecuteText)( int32_t when, const char *text );
	void (*Cmd_TokenizeString)( const char *text );
	int32_t (*FS_FOpenFile)( const char *qpath, int32_t *file, int32_t mode );
	int32_t (*FS_Read)( void *buffer, int32_t length, int32_t file );
	int32_t (*FS_Write)( const void *buffer, int32_t length, int32_t file );
	void (*FS_FCloseFile)( int32_t file );
	int32_t (*FS_GetFileList)( const char *path, const char *extension, char *list, int32_t size );
	int32_t (*FS_ReadFile)( const char *name, void **buffer );
	void (*FS_FreeFile)( void *buffer );
	int32_t (*R_RegisterModel)( const char *name );
	int32_t (*R_RegisterSkin)( const char *name );
	int32_t (*R_RegisterShader)( const char *name );
	int32_t (*R_RegisterShaderNoMip)( const char *name );
	void (*R_ClearScene)( void );
	void (*R_AddRefEntityToScene)( const fntech3_stvef_sp_refEntity_t *entity );
	void (*R_AddPolyToScene)( int32_t shader, int32_t numVerts, const fntech3_stvef_polyVert_t *verts );
	void (*R_AddLightToScene)( const float *origin, float intensity, float r, float g, float b );
	void (*R_RenderScene)( const fntech3_stvef_refdef_t *view );
	void (*R_SetColor)( const float *rgba );
	void (*R_DrawStretchPic)( float x, float y, float w, float h,
		float s1, float t1, float s2, float t2, int32_t shader );
	void (*R_ScissorPic)( float x, float y, float w, float h,
		float s1, float t1, float s2, float t2, int32_t shader );
	void (*UpdateScreen)( void );
	void (*PrecacheScreenshot)( void );
	void (*R_LerpTag)( fntech3_stvef_orientation_t *tag, int32_t model, int32_t startFrame,
		int32_t endFrame, float frac, const char *tagName );
	void (*S_StartLocalSound)( int32_t sfx, int32_t channel );
	int32_t (*S_RegisterSound)( const char *name );
	void (*S_StartLocalLoopingSound)( int32_t sfx );
	void (*DrawStretchRaw)( int32_t x, int32_t y, int32_t w, int32_t h, int32_t cols,
		int32_t rows, const uint8_t *data, float lightValue );
	int32_t (*SG_GetSaveImage)( const char *name, void *address );
	void *(*SG_GetSaveGameComment)( const char *name );
	int32_t (*SG_ValidateForLoadSaveScreen)( const char *name );
	int32_t (*SG_GameAllowedToSaveHere)( int32_t inCamera );
	void (*SG_StoreSaveGameComment)( const char *comment );
	uint8_t *(*SCR_GetScreenshot)( int32_t *valid );
	void (*Key_KeynumToStringBuf)( int32_t key, char *buffer, int32_t size );
	void (*Key_GetBindingBuf)( int32_t key, char *buffer, int32_t size );
	void (*Key_SetBinding)( int32_t key, const char *binding );
	int32_t (*Key_IsDown)( int32_t key );
	int32_t (*Key_GetOverstrikeMode)( void );
	void (*Key_SetOverstrikeMode)( int32_t state );
	void (*Key_ClearStates)( void );
	int32_t (*Key_GetCatcher)( void );
	void (*Key_SetCatcher)( int32_t catcher );
	void (*GetClipboardData)( char *buffer, int32_t size );
	void (*GetGlconfig)( fntech3_stvef_sp_glconfig_t *config );
	int32_t (*GetClientState)( void );
	void (*GetConfigString)( int32_t index, char *buffer, int32_t size );
	int32_t (*Milliseconds)( void );
} fntech3_stvef_sp_ui_import_t;

typedef struct fntech3_stvef_sp_ui_export_s {
	void (*UI_Init)( int32_t apiVersion, fntech3_stvef_sp_ui_import_t *import );
	void (*UI_Shutdown)( void );
	void (*UI_KeyEvent)( int32_t key );
	void (*UI_MouseEvent)( int32_t dx, int32_t dy );
	void (*UI_Refresh)( int32_t time );
	void (*UI_GetActiveMenu)( char **menuname, int32_t *fullscreen );
	void (*UI_SetActiveMenu)( const char *menuname, const char *menuID );
	int32_t (*UI_ConsoleCommand)( void );
	void (*UI_DrawConnect)( const char *servername, const char *updateInfoString );
	void (*UI_DrawConnectText)( const char *servername, const char *updateInfoString );
	void (*UI_UpdateConnectionString)( char *string );
	void (*UI_UpdateConnectionMessageString)( char *string );
} fntech3_stvef_sp_ui_export_t;

typedef fntech3_stvef_sp_ui_export_t *(*fntech3_stvef_sp_GetUIAPI_t)( void );

#ifdef __cplusplus
}
#endif

#endif /* FNTECH3_STVEF_SP_UI_API_H */
