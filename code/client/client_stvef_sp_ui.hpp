/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_CLIENT_STVEF_SP_UI_HPP
#define FNTECH3_CLIENT_STVEF_SP_UI_HPP
#include "../compat/stvef_sp_ui_api.h"
#include "client_render_compat.hpp"
#include "client_stvef_records.hpp"

#include <cstdarg>

/*
 * Elite Force SP's menus (stvef_sp_ui_api.h): efuix86.dll's GetUIAPI returns
 * twelve exports, the first of which takes 59 imports. The client drives the
 * menus through uivm like any module: CL_STVEFSPUIEntry turns each UI export
 * call into a table call, and the imports below are the engine's side, in
 * retail stvoy.exe's order (CL_InitUI 0x40a4a0).
 */

namespace {

fntech3_stvef_sp_ui_export_t *stvefSpUI = nullptr;
fntech3_stvef_sp_ui_import_t stvefSpUIImport{};

void CL_STVEFSPUIPrintf( const char *fmt, ... ) {
	char text[MAXPRINTMSG];
	va_list ap;
	va_start( ap, fmt );
	Q_vsnprintf( text, sizeof( text ), fmt, ap );
	va_end( ap );
	Com_Printf( "%s", text );
}

// Raven's levels are fatal, drop, disconnect and missing CD.
void CL_STVEFSPUIError( std::int32_t level, const char *fmt, ... ) {
	char text[MAXPRINTMSG];
	va_list ap;
	va_start( ap, fmt );
	Q_vsnprintf( text, sizeof( text ), fmt, ap );
	va_end( ap );
	const errorParm_t code = level == 1 ? ERR_DROP : level == 2 ? ERR_DISCONNECT
		: level == 3 ? ERR_NEED_CD : ERR_FATAL;
	Com_Error( code, "%s", text );
}

void CL_STVEFSPUICvarSet( const char *name, const char *value ) { Cvar_Set( name, value ); }
float CL_STVEFSPUICvarValue( const char *name ) { return Cvar_VariableValue( name ); }
void CL_STVEFSPUICvarString( const char *name, char *buffer, std::int32_t size ) {
	Cvar_VariableStringBuffer( name, buffer, size );
}
void CL_STVEFSPUICvarSetValue( const char *name, float value ) { Cvar_SetValue( name, value ); }
void CL_STVEFSPUICvarReset( const char *name ) { Cvar_Reset( name ); }
void CL_STVEFSPUICvarCreate( const char *name, const char *value, std::int32_t flags ) {
	Cvar_Get( name, value, flags );
}
void CL_STVEFSPUICvarInfo( std::int32_t bit, char *buffer, std::int32_t size ) {
	Cvar_InfoStringBuffer( bit, buffer, size );
}
std::int32_t CL_STVEFSPUIArgc() { return Cmd_Argc(); }
void CL_STVEFSPUIArgv( std::int32_t n, char *buffer, std::int32_t size ) { Cmd_ArgvBuffer( n, buffer, size ); }
void CL_STVEFSPUIExecute( std::int32_t when, const char *text ) {
	if ( when < EXEC_NOW || when > EXEC_APPEND ) {
		Com_Error( ERR_DROP, "Elite Force SP menus executed text at %i", when );
	}
	Cbuf_ExecuteText( static_cast<cbufExec_t>( when ), text );
}
void CL_STVEFSPUITokenize( const char *text ) { Cmd_TokenizeString( text ); }

std::int32_t CL_STVEFSPUIOpenFile( const char *qpath, std::int32_t *file, std::int32_t mode ) {
	fileHandle_t handle = 0;
	const int length = FS_VM_OpenFile( qpath, file ? &handle : nullptr, static_cast<fsMode_t>( mode ), H_Q3UI );
	if ( file ) *file = handle;
	return length;
}
std::int32_t CL_STVEFSPUIRead( void *buffer, std::int32_t length, std::int32_t file ) {
	return FS_VM_ReadFile( buffer, length, file, H_Q3UI );
}
std::int32_t CL_STVEFSPUIWrite( const void *buffer, std::int32_t length, std::int32_t file ) {
	return FS_VM_WriteFile( const_cast<void *>( buffer ), length, file, H_Q3UI );
}
void CL_STVEFSPUICloseFile( std::int32_t file ) { FS_VM_CloseFile( file, H_Q3UI ); }
std::int32_t CL_STVEFSPUIFileList( const char *path, const char *extension, char *list, std::int32_t size ) {
	return FS_GetFileList( path, extension, list, size );
}
std::int32_t CL_STVEFSPUIReadFile( const char *name, void **buffer ) { return FS_ReadFile( name, buffer ); }
void CL_STVEFSPUIFreeFile( void *buffer ) { FS_FreeFile( buffer ); }

std::int32_t CL_STVEFSPUIRegisterModel( const char *name ) { return re.RegisterModel( name ); }
std::int32_t CL_STVEFSPUIRegisterSkin( const char *name ) { return re.RegisterSkin( name ); }
std::int32_t CL_STVEFSPUIRegisterShader( const char *name ) { return re.RegisterShader( name ); }
std::int32_t CL_STVEFSPUIRegisterShaderNoMip( const char *name ) { return re.RegisterShaderNoMip( name ); }
void CL_STVEFSPUIClearScene() { re.ClearScene(); }
void CL_STVEFSPUIAddEntity( const fntech3_stvef_sp_refEntity_t *entity ) {
	const refEntity_t internal = CL_ModuleRefEntity( uivm, reinterpret_cast<intptr_t>( entity ) );
	re.AddRefEntityToScene( &internal, qfalse );
}
void CL_STVEFSPUIAddPoly( std::int32_t shader, std::int32_t numVerts, const fntech3_stvef_polyVert_t *verts ) {
	static_assert( sizeof( fntech3_stvef_polyVert_t ) == sizeof( polyVert_t ) );
	re.AddPolyToScene( shader, numVerts, reinterpret_cast<const polyVert_t *>( verts ), 1 );
}
void CL_STVEFSPUIAddLight( const float *origin, float intensity, float r, float g, float b ) {
	re.AddLightToScene( origin, intensity, r, g, b );
}
void CL_STVEFSPUIRenderScene( const fntech3_stvef_refdef_t *view ) {
	refdef_t refdef = CL_ModuleRefdef( uivm, reinterpret_cast<intptr_t>( view ) );
	// A menu scene keeps its authored FOV.
	refdef.rdflags |= RDF_NOFOVCORRECTION;
	CL_UIAdjustRefdef( &refdef );
	re.RenderScene( &refdef );
}
void CL_STVEFSPUISetColor( const float *rgba ) { re.SetColor( rgba ); }
void CL_STVEFSPUIDrawStretchPic( float x, float y, float w, float h,
	float s1, float t1, float s2, float t2, std::int32_t shader ) {
	CL_UIAdjustStretchPic( &x, &y, &w, &h );
	re.DrawStretchPic( x, y, w, h, s1, t1, s2, t2, shader );
}
void CL_STVEFSPUIUpdateScreen() { SCR_UpdateScreen(); }
// The menus keep the screen as it is before they are drawn over it.
void CL_STVEFSPUIPrecacheScreenshot() { CL_STVEFSPPrecacheScreenshot(); }
void CL_STVEFSPUILerpTag( fntech3_stvef_orientation_t *tag, std::int32_t model, std::int32_t startFrame,
	std::int32_t endFrame, float frac, const char *tagName ) {
	static_assert( sizeof( fntech3_stvef_orientation_t ) == sizeof( orientation_t ) );
	re.LerpTag( reinterpret_cast<orientation_t *>( tag ), model, startFrame, endFrame, frac, tagName );
}
void CL_STVEFSPUIStartLocalSound( std::int32_t sfx, std::int32_t channel ) {
	S_StartLocalSound( sfx, CL_STVEFSPSoundChannel( channel ) );
}
std::int32_t CL_STVEFSPUIRegisterSound( const char *name ) { return S_RegisterSound( name, qfalse ); }
// A loop on the listener, entity 0 in single player, with no position
// (S_StartLocalLoopingSound 0x420970).
void CL_STVEFSPUIStartLocalLoopingSound( std::int32_t sfx ) {
	const vec3_t zero = { 0, 0, 0 };
	S_AddLoopingSound( 0, zero, zero, sfx );
}
void CL_STVEFSPUIDrawStretchRaw( std::int32_t x, std::int32_t y, std::int32_t w, std::int32_t h,
	std::int32_t cols, std::int32_t rows, const std::uint8_t *data, float ) {
	re.DrawStretchRaw( x, y, w, h, cols, rows, data, 0, qtrue );
}

// The load and save menus list the savegames, which the server keeps.
std::int32_t CL_STVEFSPUIGetSaveImage( const char *name, void *pixels ) {
	return SV_STVEFSPSaveImage( name, pixels );
}
void *CL_STVEFSPUIGetSaveComment( const char *name ) {
	return const_cast<char *>( SV_STVEFSPSaveComment( name ) );
}
std::int32_t CL_STVEFSPUIValidateSave( const char *name ) { return SV_STVEFSPValidateSave( name ); }
std::int32_t CL_STVEFSPUIAllowedToSave( std::int32_t inCamera ) {
	return SV_STVEFSPGameAllowedToSaveHere( inCamera ? qtrue : qfalse );
}
void CL_STVEFSPUIStoreSaveComment( const char *comment ) { SV_STVEFSPStoreSaveComment( comment ); }
// The kept picture of the screen, taken now if there is none (0x40a3a0).
std::uint8_t *CL_STVEFSPUIGetScreenshot( std::int32_t *valid ) {
	std::uint8_t *pixels = const_cast<std::uint8_t *>( CL_STVEFSPScreenshotPixels() );
	if ( valid ) *valid = CL_STVEFSPScreenshotValid();
	return pixels;
}

void CL_STVEFSPUIKeyName( std::int32_t key, char *buffer, std::int32_t size ) {
	CL_STVEFKeyString( key, buffer, size, qfalse );
}
void CL_STVEFSPUIKeyBinding( std::int32_t key, char *buffer, std::int32_t size ) {
	CL_STVEFKeyString( key, buffer, size, qtrue );
}
void CL_STVEFSPUISetBinding( std::int32_t key, const char *binding ) { CL_STVEFSetBinding( key, binding ); }
std::int32_t CL_STVEFSPUIKeyIsDown( std::int32_t key ) { return CL_STVEFKeyIsDown( key ); }
std::int32_t CL_STVEFSPUIGetOverstrike() { return Key_GetOverstrikeMode(); }
void CL_STVEFSPUISetOverstrike( std::int32_t state ) { Key_SetOverstrikeMode( state ? qtrue : qfalse ); }
void CL_STVEFSPUIClearStates() { Key_ClearStates(); }
// Elite Force's four catchers have Quake III's numbers.
constexpr int kSTVEFSPCatchers = KEYCATCH_CONSOLE | KEYCATCH_UI | KEYCATCH_MESSAGE | KEYCATCH_CGAME;
std::int32_t CL_STVEFSPUIGetCatcher() { return Key_GetCatcher() & kSTVEFSPCatchers; }
void CL_STVEFSPUISetCatcher( std::int32_t catcher ) {
	Key_SetCatcher( ( catcher & kSTVEFSPCatchers ) | ( Key_GetCatcher() & ~kSTVEFSPCatchers ) );
}
void CL_STVEFSPUIClipboard( char *buffer, std::int32_t size ) { CL_GetClipboardData( buffer, size ); }
void CL_STVEFSPUIGlconfig( fntech3_stvef_sp_glconfig_t *config ) { CL_CopySTVEFSPGlconfig( config, cls.glconfig ); }

// Single player has no authorizing state, so each later state is one lower.
std::int32_t CL_STVEFSPUIClientState() {
	switch ( cls.state ) {
	case CA_UNINITIALIZED: return FNTECH3_STVEF_SP_CA_UNINITIALIZED;
	case CA_DISCONNECTED: return FNTECH3_STVEF_SP_CA_DISCONNECTED;
	case CA_AUTHORIZING:
	case CA_CONNECTING: return FNTECH3_STVEF_SP_CA_CONNECTING;
	case CA_CHALLENGING: return FNTECH3_STVEF_SP_CA_CHALLENGING;
	case CA_CONNECTED: return FNTECH3_STVEF_SP_CA_CONNECTED;
	case CA_LOADING: return FNTECH3_STVEF_SP_CA_LOADING;
	case CA_PRIMED: return FNTECH3_STVEF_SP_CA_PRIMED;
	case CA_ACTIVE: return FNTECH3_STVEF_SP_CA_ACTIVE;
	case CA_CINEMATIC: return FNTECH3_STVEF_SP_CA_CINEMATIC;
	}
	return FNTECH3_STVEF_SP_CA_DISCONNECTED;
}
void CL_STVEFSPUIConfigString( std::int32_t index, char *buffer, std::int32_t size ) {
	GetConfigString( index, buffer, size );
}
std::int32_t CL_STVEFSPUIMilliseconds() { return Sys_Milliseconds(); }

void CL_STVEFSPUIFillImports( fntech3_stvef_sp_ui_import_t &import ) {
	import = {};
	import.Printf = CL_STVEFSPUIPrintf;
	import.Error = CL_STVEFSPUIError;
	import.Cvar_Set = CL_STVEFSPUICvarSet;
	import.Cvar_VariableValue = CL_STVEFSPUICvarValue;
	import.Cvar_VariableStringBuffer = CL_STVEFSPUICvarString;
	import.Cvar_SetValue = CL_STVEFSPUICvarSetValue;
	import.Cvar_Reset = CL_STVEFSPUICvarReset;
	import.Cvar_Create = CL_STVEFSPUICvarCreate;
	import.Cvar_InfoStringBuffer = CL_STVEFSPUICvarInfo;
	import.Argc = CL_STVEFSPUIArgc;
	import.Argv = CL_STVEFSPUIArgv;
	import.Cmd_ExecuteText = CL_STVEFSPUIExecute;
	import.Cmd_TokenizeString = CL_STVEFSPUITokenize;
	import.FS_FOpenFile = CL_STVEFSPUIOpenFile;
	import.FS_Read = CL_STVEFSPUIRead;
	import.FS_Write = CL_STVEFSPUIWrite;
	import.FS_FCloseFile = CL_STVEFSPUICloseFile;
	import.FS_GetFileList = CL_STVEFSPUIFileList;
	import.FS_ReadFile = CL_STVEFSPUIReadFile;
	import.FS_FreeFile = CL_STVEFSPUIFreeFile;
	import.R_RegisterModel = CL_STVEFSPUIRegisterModel;
	import.R_RegisterSkin = CL_STVEFSPUIRegisterSkin;
	import.R_RegisterShader = CL_STVEFSPUIRegisterShader;
	import.R_RegisterShaderNoMip = CL_STVEFSPUIRegisterShaderNoMip;
	import.R_ClearScene = CL_STVEFSPUIClearScene;
	import.R_AddRefEntityToScene = CL_STVEFSPUIAddEntity;
	import.R_AddPolyToScene = CL_STVEFSPUIAddPoly;
	import.R_AddLightToScene = CL_STVEFSPUIAddLight;
	import.R_RenderScene = CL_STVEFSPUIRenderScene;
	import.R_SetColor = CL_STVEFSPUISetColor;
	import.R_DrawStretchPic = CL_STVEFSPUIDrawStretchPic;
	// Retail leaves the scissor import empty (its fill at 0x40a4a0 skips it).
	import.R_ScissorPic = nullptr;
	import.UpdateScreen = CL_STVEFSPUIUpdateScreen;
	import.PrecacheScreenshot = CL_STVEFSPUIPrecacheScreenshot;
	import.R_LerpTag = CL_STVEFSPUILerpTag;
	import.S_StartLocalSound = CL_STVEFSPUIStartLocalSound;
	import.S_RegisterSound = CL_STVEFSPUIRegisterSound;
	import.S_StartLocalLoopingSound = CL_STVEFSPUIStartLocalLoopingSound;
	import.DrawStretchRaw = CL_STVEFSPUIDrawStretchRaw;
	import.SG_GetSaveImage = CL_STVEFSPUIGetSaveImage;
	import.SG_GetSaveGameComment = CL_STVEFSPUIGetSaveComment;
	import.SG_ValidateForLoadSaveScreen = CL_STVEFSPUIValidateSave;
	import.SG_GameAllowedToSaveHere = CL_STVEFSPUIAllowedToSave;
	import.SG_StoreSaveGameComment = CL_STVEFSPUIStoreSaveComment;
	import.SCR_GetScreenshot = CL_STVEFSPUIGetScreenshot;
	import.Key_KeynumToStringBuf = CL_STVEFSPUIKeyName;
	import.Key_GetBindingBuf = CL_STVEFSPUIKeyBinding;
	import.Key_SetBinding = CL_STVEFSPUISetBinding;
	import.Key_IsDown = CL_STVEFSPUIKeyIsDown;
	import.Key_GetOverstrikeMode = CL_STVEFSPUIGetOverstrike;
	import.Key_SetOverstrikeMode = CL_STVEFSPUISetOverstrike;
	import.Key_ClearStates = CL_STVEFSPUIClearStates;
	import.Key_GetCatcher = CL_STVEFSPUIGetCatcher;
	import.Key_SetCatcher = CL_STVEFSPUISetCatcher;
	import.GetClipboardData = CL_STVEFSPUIClipboard;
	import.GetGlconfig = CL_STVEFSPUIGlconfig;
	import.GetClientState = CL_STVEFSPUIClientState;
	import.GetConfigString = CL_STVEFSPUIConfigString;
	import.Milliseconds = CL_STVEFSPUIMilliseconds;
}

// A holodeck map's in-game menu is the holodeck one (CL_KeyEvent 0x407129).
bool CL_STVEFSPOnHolodeck() {
	char serverinfo[MAX_INFO_STRING];
	GetConfigString( CS_SERVERINFO, serverinfo, sizeof( serverinfo ) );
	return !Q_stricmpn( Info_ValueForKey( serverinfo, "mapname" ), "_holo", 5 );
}

/* uivm's entry point: the engine's UI export calls, as table calls. Menus
 * open by name: main, the in-game menu (the holodeck's on a holodeck map) and
 * the missing-CD menu; none closes them (retail CL_KeyEvent 0x4070e1 and
 * SCR_DrawScreenField 0x40a1ae). */
intptr_t QDECL CL_STVEFSPUIEntry( int command, ... ) {
	std::int32_t arg[3] = {};
	va_list ap;
	va_start( ap, command );
	for ( std::int32_t &value : arg ) value = va_arg( ap, std::int32_t );
	va_end( ap );

	fntech3_stvef_sp_ui_export_t *ui = stvefSpUI;
	if ( !ui ) {
		Com_Error( ERR_DROP, "Elite Force SP menus called after shutdown" );
		return 0;
	}
	switch ( command ) {
	case UI_GETAPIVERSION:
		return FNTECH3_STVEF_SP_UI_API_VERSION;
	case UI_INIT:
		ui->UI_Init( FNTECH3_STVEF_SP_UI_API_VERSION, &stvefSpUIImport );
		return 0;
	case UI_SHUTDOWN:
		ui->UI_Shutdown();
		stvefSpUI = nullptr;
		return 0;
	case UI_KEY_EVENT:
		// Presses only; the key handler sends no releases to Elite Force.
		ui->UI_KeyEvent( arg[0] );
		return 0;
	case UI_MOUSE_EVENT:
		ui->UI_MouseEvent( arg[0], arg[1] );
		return 0;
	case UI_REFRESH:
		ui->UI_Refresh( arg[0] );
		return 0;
	case UI_IS_FULLSCREEN: {
		std::int32_t fullscreen = 0;
		ui->UI_GetActiveMenu( nullptr, &fullscreen );
		return fullscreen;
	}
	case UI_SET_ACTIVE_MENU:
		switch ( arg[0] ) {
		case UIMENU_NONE: ui->UI_SetActiveMenu( nullptr, nullptr ); break;
		case UIMENU_INGAME: ui->UI_SetActiveMenu( "ingame", CL_STVEFSPOnHolodeck() ? "HOLO" : nullptr ); break;
		case UIMENU_NEED_CD: ui->UI_SetActiveMenu( "needcd", nullptr ); break;
		default: ui->UI_SetActiveMenu( "main", nullptr ); break;
		}
		return 0;
	case UI_CONSOLE_COMMAND:
		return ui->UI_ConsoleCommand();
	case UI_DRAW_CONNECT_SCREEN:
		// The connection screen, or its text over the loading cgame.
		if ( arg[0] ) ui->UI_DrawConnectText( cls.servername, cls.updateInfoString );
		else ui->UI_DrawConnect( cls.servername, cls.updateInfoString );
		return 0;
	case UI_HASUNIQUECDKEY:
		return qfalse;
	default:
		Com_DPrintf( "Elite Force SP menus have no export %i\n", command );
		return 0;
	}
}

vm_t *CL_STVEFSPLoadUI() {
	void *symbol = nullptr;
	vm_t *vm = VM_CreateTable( VM_UI, "GetUIAPI", CL_STVEFSPUIEntry, &symbol );
	if ( !vm ) {
		return nullptr;
	}
	fntech3_stvef_sp_ui_export_t *ui = reinterpret_cast<fntech3_stvef_sp_GetUIAPI_t>( symbol )();
	if ( !ui ) {
		VM_Free( vm );
		Com_Printf( "GetUIAPI returned no exports\n" );
		return nullptr;
	}
	CL_STVEFSPUIFillImports( stvefSpUIImport );
	stvefSpUI = ui;
	return vm;
}

/* The connection screen shows text the client hands the menus: the attempt
 * count while connecting, cleared when the gamestate arrives, and the
 * server's printed message (stvoy.exe 0x407a60, 0x4097e4, 0x407e7a). The
 * menus copy it and redraw the screen. */
void CL_STVEFSPUIConnectionText( const char *text, bool message ) {
	if ( !stvefSpUI ) return;
	char copy[MAX_STRING_CHARS];
	Q_strncpyz( copy, text ? text : "", sizeof( copy ) );
	if ( message ) stvefSpUI->UI_UpdateConnectionMessageString( copy );
	else stvefSpUI->UI_UpdateConnectionString( copy );
}

/* Retail's menu commands: genericmenu opens a named generic menu (the
 * holodeck's on a holodeck map) and endholodeckmenu the holodeck's closing
 * menu (0x40a8b0, 0x40a900). */
void CL_STVEFSPGenericMenu_f() {
	if ( stvefSpUI ) stvefSpUI->UI_SetActiveMenu( CL_STVEFSPOnHolodeck() ? "genericholomenu" : "genericmenu", Cmd_Argv( 1 ) );
}
void CL_STVEFSPEndHolodeckMenu_f() {
	if ( stvefSpUI ) stvefSpUI->UI_SetActiveMenu( "endholomenu", Cmd_Argv( 1 ) );
}

} // namespace

#endif
