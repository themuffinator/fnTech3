/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
/* The Q3Test releases' menus.
 *
 * 1.06 to 1.08 load uix86.dll, whose GetUIAPI takes no argument and returns
 * twelve function pointers; the first takes the API version (2 in every
 * release) and the import table, 51 words in 1.06 and 1.07 and 55 in 1.08.
 * The engine pushes the connect screen's strings into the module instead of
 * letting it read a client state. 1.05 has no menu module: its menus are
 * engine code, served here as this engine's own (cl_q3test_menus.cpp), behind
 * the same calls. Facts and addresses: docs/fntech3/Q3TEST.md and the UI
 * contract it cites. */
extern "C" {
#include "client.h"
}
#include "client_q3test.h"
#include "../compat/q3_key_adapter.h"
#include <array>
#include <cstdarg>
#include <cstring>
#include <string>
#include <vector>

namespace q3cl = fntech3::q3test::client;

namespace {

enum class Q3TestUIStyle { None, Engine105, Table106, Table108 };

struct Q3TestUI {
	Q3TestUIStyle style = Q3TestUIStyle::None;
	const std::intptr_t *exports = nullptr;  // the module's twelve words
	std::array<std::intptr_t, 55> imports{};
	int depth = 0;                           // export calls on the stack
	std::vector<std::string> deferred;       // commands that would unload the module under itself
};
Q3TestUI q3ui;

const q3testRelease_t &Q3TestRelease() { return *Com_Q3TestRelease(); }

/* ---- imports ------------------------------------------------------------------ */

void QDECL Q3UI_Printf( const char *fmt, ... ) {
	char text[MAXPRINTMSG];
	va_list args;
	va_start( args, fmt );
	Q_vsnprintf( text, sizeof( text ), fmt, args );
	va_end( args );
	Com_Printf( "%s", q3cl::Text( text ).c_str() );
}
/* Levels 0 fatal and 1 drop are the ones the module uses. */
void QDECL Q3UI_Error( int level, const char *fmt, ... ) {
	char text[MAXPRINTMSG];
	va_list args;
	va_start( args, fmt );
	Q_vsnprintf( text, sizeof( text ), fmt, args );
	va_end( args );
	Com_Error( level == 0 ? ERR_FATAL : ERR_DROP, "%s", q3cl::Text( text ).c_str() );
}
void QDECL Q3UI_CvarSet( const char *name, const char *value ) { Cvar_Set( name, value ); }
float QDECL Q3UI_CvarVariableValue( const char *name ) { return Cvar_VariableValue( name ); }
void QDECL Q3UI_CvarVariableStringBuffer( const char *name, char *buffer, int size ) {
	Cvar_VariableStringBuffer( name, buffer, size );
}
void QDECL Q3UI_CvarSetValue( const char *name, float value ) { Cvar_SetValue( name, value ); }
/* 1.06 forces the reset (0x4116a0); 1.07 and 1.08 respect protections (0x411700). */
void QDECL Q3UI_CvarReset( const char *name ) {
	Cvar_Set2( name, nullptr, Q3TestRelease().id == Q3TEST_RELEASE_1_06 ? qtrue : qfalse );
}
cvar_t *QDECL Q3UI_CvarGet( const char *name, const char *value, int flags ) {
	return Cvar_Get( name, value, q3cl::CvarFlags( flags ) );
}
void QDECL Q3UI_CvarInfoStringBuffer( int bit, char *buffer, int size ) {
	Q_strncpyz( buffer, Cvar_InfoString( q3cl::CvarFlags( bit ), nullptr ), size );
}
int QDECL Q3UI_Argc() { return Cmd_Argc(); }
void QDECL Q3UI_ArgvBuffer( int n, char *buffer, int size ) { Cmd_ArgvBuffer( n, buffer, size ); }

/* EXEC_NOW runs the whole text as one command, as retail's Cmd_ExecuteString
 * did. A command that reloads the menus or the level would free the module
 * while its code is on the stack: retail survived that only because Windows
 * mapped the reloaded library at the same base (4.8). Those wait for the next
 * client frame, when no call into the module is on the stack: VM_Free refuses
 * a module VM_Call has not left, even once its outermost export has
 * returned. */
bool Q3TestUIReloads( const char *text ) {
	char verb[MAX_TOKEN_CHARS];
	const char *p = text;
	while ( *p == ' ' || *p == '\t' ) ++p;
	std::size_t n = 0;
	while ( p[n] && p[n] != ' ' && p[n] != '\t' && p[n] != '\n' && p[n] != ';' && n + 1 < sizeof( verb ) ) {
		verb[n] = p[n];
		++n;
	}
	verb[n] = '\0';
	static const char *const reloads[] = { "vid_restart", "snd_restart", "map", "devmap", "spmap", "demo",
		"cinematic", "cvar_restart" };
	for ( const char *name : reloads ) {
		if ( !Q_stricmp( verb, name ) ) return true;
	}
	return false;
}
void QDECL Q3UI_ExecuteText( int when, const char *text ) {
	if ( !text ) return;
	switch ( when ) {
	case EXEC_NOW:
		if ( q3ui.depth > 0 && Q3TestUIReloads( text ) ) {
			q3ui.deferred.emplace_back( text );
			return;
		}
		Cmd_ExecuteString( text );
		return;
	case EXEC_INSERT:
	case EXEC_APPEND:
		Cbuf_ExecuteText( static_cast<cbufExec_t>( when ), text );
		return;
	default:
		Com_Error( ERR_FATAL, "Cbuf_ExecuteText: bad exec_when" );
	}
}
void QDECL Q3UI_TokenizeString( const char *text ) { Cmd_TokenizeString( text ); }

/* Handles 1..7 in retail; 0 means none. */
int QDECL Q3UI_FOpen( const char *qpath, fileHandle_t *f, int mode ) {
	if ( mode < FS_READ || mode > FS_APPEND ) {
		Com_Error( ERR_FATAL, "FSH_FOpenFile: bad mode" );
		return -1;
	}
	const int length = FS_VM_OpenFile( qpath, f, static_cast<fsMode_t>( mode ), H_Q3UI );
	return mode == FS_READ ? length : 0;
}
int QDECL Q3UI_Read( void *buffer, int length, fileHandle_t f ) { return FS_VM_ReadFile( buffer, length, f, H_Q3UI ); }
int QDECL Q3UI_Write( const void *buffer, int length, fileHandle_t f ) {
	return FS_VM_WriteFile( const_cast<void *>( buffer ), length, f, H_Q3UI );
}
void QDECL Q3UI_FClose( fileHandle_t f ) { FS_VM_CloseFile( f, H_Q3UI ); }
int QDECL Q3UI_GetFileList( const char *path, const char *extension, char *list, int size ) {
	return FS_GetFileList( path, extension, list, size );
}

/* The menus' art is two-dimensional: none of it is mipmapped. */
qhandle_t QDECL Q3UI_RegisterModel( const char *name ) { return re.RegisterModel( name ); }
qhandle_t QDECL Q3UI_RegisterSkin( const char *name ) { return re.RegisterSkin( name ); }
qhandle_t QDECL Q3UI_RegisterShader( const char *name ) {
	if ( !name || strlen( name ) >= MAX_QPATH ) return 0;
	return re.RegisterShaderNoMip( name );
}
void QDECL Q3UI_ClearScene() { re.ClearScene(); }
void QDECL Q3UI_AddRefEntity( const void *native ) { q3cl::AddRefEntity( native ); }
void QDECL Q3UI_AddPoly( qhandle_t shader, int numVerts, const polyVert_t *verts ) {
	if ( !shader ) {
		Com_DPrintf( S_COLOR_YELLOW "WARNING: RE_AddPolyToScene: NULL poly shader\n" );
		return;
	}
	re.AddPolyToScene( shader, numVerts, verts, 1 );
}
void QDECL Q3UI_AddLight( const float *origin, float intensity, float r, float g, float b ) {
	if ( intensity <= 0 ) return;
	re.AddLightToScene( origin, intensity, r, g, b );
}
void QDECL Q3UI_RenderScene( const void *native ) { q3cl::RenderScene( native ); }
void QDECL Q3UI_SetColor( const float *rgba ) { re.SetColor( rgba ); }
void QDECL Q3UI_DrawStretchPic( float x, float y, float w, float h, float s1, float t1, float s2, float t2, qhandle_t shader ) {
	re.DrawStretchPic( x, y, w, h, s1, t1, s2, t2, shader );
}
void QDECL Q3UI_UpdateScreen() { SCR_UpdateScreen(); }
void QDECL Q3UI_LerpTag( orientation_t *tag, clipHandle_t model, int startFrame, int endFrame, float frac, const char *name ) {
	q3cl::LerpTag( tag, model, startFrame, endFrame, frac, name );
}
clipHandle_t QDECL Q3UI_LoadModel( const char *name ) { return q3cl::CollisionModel( name ); }
// by name, on the listener's channel 1 (0x418300)
void QDECL Q3UI_StartLocalSound( const char *name ) {
	if ( !name || strlen( name ) >= MAX_QPATH ) return;
	S_StartLocalSound( S_RegisterSound( name, qfalse ), 1 );
}

/* Key numbers are Quake III's (5.8); this engine's sit four higher from the
 * mouse wheel up. */
int Q3TestKey( int key ) { return key < 0 ? -1 : FnTech3_Q3KeyToInternal( key ); }
void QDECL Q3UI_KeynumToStringBuf( int key, char *buffer, int size ) {
	Q_strncpyz( buffer, Key_KeynumToString( key < 0 ? key : Q3TestKey( key ) ), size );
}
void QDECL Q3UI_GetBindingBuf( int key, char *buffer, int size ) {
	const int internal = Q3TestKey( key );
	const char *binding = internal >= 0 && internal < MAX_KEYS ? Key_GetBinding( internal ) : nullptr;
	Q_strncpyz( buffer, binding ? binding : "", size );
}
void QDECL Q3UI_SetBinding( int key, const char *binding ) {
	const int internal = Q3TestKey( key );
	if ( internal < 0 || internal >= MAX_KEYS ) return;
	Key_SetBinding( internal, binding );
}
qboolean QDECL Q3UI_IsDown( int key ) {
	const int internal = Q3TestKey( key );
	return internal >= 0 && internal < MAX_KEYS ? Key_IsDown( internal ) : qfalse;
}
qboolean QDECL Q3UI_GetOverstrikeMode() { return Key_GetOverstrikeMode(); }
void QDECL Q3UI_SetOverstrikeMode( qboolean state ) { Key_SetOverstrikeMode( state ); }
void QDECL Q3UI_ClearStates() { Key_ClearStates(); }
/* Console 1, menu 2 and message 4 are retail's bits; the engine keeps its own. */
int QDECL Q3UI_GetCatcher() { return Key_GetCatcher() & 7; }
void QDECL Q3UI_SetCatcher( int catcher ) { Key_SetCatcher( ( Key_GetCatcher() & ~7 ) | ( catcher & 7 ) ); }
void QDECL Q3UI_GetClipboardData( char *buffer, int size ) {
	char *text = Sys_GetClipboardData();
	Q_strncpyz( buffer, text ? text : "", size );
	if ( text ) Z_Free( text );
}

/* The server lists: info strings and addresses, copied into the module's
 * 1024- and 32-byte buffers (the imports take no size, 5.7). */
int QDECL Q3UI_LocalServerCount() { return q3cl::LocalServerCount(); }
void QDECL Q3UI_LocalServerInfo( int n, char *buffer ) { Q_strncpyz( buffer, q3cl::LocalServerInfo( n ).c_str(), 1024 ); }
void QDECL Q3UI_LocalServerAddress( int n, char *buffer ) { Q_strncpyz( buffer, q3cl::LocalServerAddress( n ).c_str(), 32 ); }
int QDECL Q3UI_GlobalServerCount() { return q3cl::GlobalServerCount(); }
void QDECL Q3UI_GlobalServerInfo( int n, char *buffer ) { Q_strncpyz( buffer, q3cl::GlobalServerInfo( n ).c_str(), 1024 ); }
void QDECL Q3UI_GlobalServerAddress( int n, char *buffer ) { Q_strncpyz( buffer, q3cl::GlobalServerAddress( n ).c_str(), 32 ); }
int QDECL Q3UI_PingQueueCount() { return q3cl::PingQueueCount(); }
void QDECL Q3UI_ClearPing( int n ) { q3cl::ClearPing( n ); }
void QDECL Q3UI_GetPing( int n, char *address, int *time ) { q3cl::GetPing( n, address, 32, time ); }
void QDECL Q3UI_GetGlconfig( void *native ) { q3cl::WriteGlconfig( native ); }
/* Retail has no CA_AUTHORIZING, so every later state is one lower (5.1). */
int QDECL Q3UI_GetConnState() {
	const int state = cls.state;
	if ( state <= CA_DISCONNECTED ) return state;
	return state == CA_AUTHORIZING ? 2 : state - 1;
}
qboolean QDECL Q3UI_GetConfigString( int index, char *buffer, int size ) {
	if ( index < 0 || index >= MAX_CONFIGSTRINGS ) return qfalse;
	const int offset = cl.gameState.stringOffsets[index];
	if ( !offset ) return qfalse;
	Q_strncpyz( buffer, cl.gameState.stringData + offset, size );
	return qtrue;
}
int QDECL Q3UI_Milliseconds() { return Sys_Milliseconds(); }

template<class F> std::intptr_t Slot( F *function ) { return reinterpret_cast<std::intptr_t>( function ); }

/* CL_InitUI (1.06 0x408ed0, 1.08 0x409810): 1.08 inserts four slots, at 8,
 * 12, 53 and 54. */
void Q3TestFillImports( bool v108 ) {
	auto &t = q3ui.imports;
	t = {};
	int k = 0;
	auto put = [&]( std::intptr_t value ) { t[static_cast<std::size_t>( k++ )] = value; };
	put( Slot( Q3UI_Printf ) ); put( Slot( Q3UI_Error ) ); put( Slot( Q3UI_CvarSet ) ); put( Slot( Q3UI_CvarVariableValue ) );
	put( Slot( Q3UI_CvarVariableStringBuffer ) ); put( Slot( Q3UI_CvarSetValue ) ); put( Slot( Q3UI_CvarReset ) );
	put( Slot( Q3UI_CvarGet ) );
	if ( v108 ) put( Slot( Q3UI_CvarInfoStringBuffer ) );
	put( Slot( Q3UI_Argc ) ); put( Slot( Q3UI_ArgvBuffer ) ); put( Slot( Q3UI_ExecuteText ) );
	if ( v108 ) put( Slot( Q3UI_TokenizeString ) );
	put( Slot( Q3UI_FOpen ) ); put( Slot( Q3UI_Read ) ); put( Slot( Q3UI_Write ) ); put( Slot( Q3UI_FClose ) );
	put( Slot( Q3UI_GetFileList ) );
	put( Slot( Q3UI_RegisterModel ) ); put( Slot( Q3UI_RegisterSkin ) ); put( Slot( Q3UI_RegisterShader ) );
	put( Slot( Q3UI_ClearScene ) ); put( Slot( Q3UI_AddRefEntity ) ); put( Slot( Q3UI_AddPoly ) ); put( Slot( Q3UI_AddLight ) );
	put( Slot( Q3UI_RenderScene ) ); put( Slot( Q3UI_SetColor ) ); put( Slot( Q3UI_DrawStretchPic ) );
	put( Slot( Q3UI_UpdateScreen ) ); put( Slot( Q3UI_LerpTag ) ); put( Slot( Q3UI_LoadModel ) );
	put( Slot( Q3UI_StartLocalSound ) );
	put( Slot( Q3UI_KeynumToStringBuf ) ); put( Slot( Q3UI_GetBindingBuf ) ); put( Slot( Q3UI_SetBinding ) );
	put( Slot( Q3UI_IsDown ) ); put( Slot( Q3UI_GetOverstrikeMode ) ); put( Slot( Q3UI_SetOverstrikeMode ) );
	put( Slot( Q3UI_ClearStates ) ); put( Slot( Q3UI_GetCatcher ) ); put( Slot( Q3UI_SetCatcher ) );
	put( Slot( Q3UI_GetClipboardData ) );
	put( Slot( Q3UI_LocalServerCount ) ); put( Slot( Q3UI_LocalServerInfo ) ); put( Slot( Q3UI_LocalServerAddress ) );
	put( Slot( Q3UI_GlobalServerCount ) ); put( Slot( Q3UI_GlobalServerInfo ) ); put( Slot( Q3UI_GlobalServerAddress ) );
	put( Slot( Q3UI_PingQueueCount ) ); put( Slot( Q3UI_ClearPing ) ); put( Slot( Q3UI_GetPing ) );
	put( Slot( Q3UI_GetGlconfig ) ); put( Slot( Q3UI_GetConnState ) );
	if ( v108 ) {
		put( Slot( Q3UI_GetConfigString ) );
		put( Slot( Q3UI_Milliseconds ) );
	}
	if ( k != ( v108 ? 55 : 51 ) ) Com_Error( ERR_FATAL, "Q3Test UI import table has %i words", k );
}

/* ---- exports ------------------------------------------------------------------ */

template<class... A> intptr_t Q3TestUIExport( int word, A... args ) {
	using Fn = intptr_t ( QDECL * )( A... );
	return reinterpret_cast<Fn>( q3ui.exports[word] )( args... );
}

/* Every call into the module counts its depth, so that Q3UI_ExecuteText knows
 * its reloads must wait for CL_Q3TestUIFrame. */
template<class F> intptr_t Q3TestUICall( F &&call ) {
	++q3ui.depth;
	const intptr_t result = call();
	--q3ui.depth;
	return result;
}

/* The engine's calls, in its own numbering (4.1, 4.2). Keys reach the module
 * on key-down only; menus are opened by name. */
intptr_t QDECL CL_Q3TestUIEntry( int command, ... ) {
	va_list args;
	va_start( args, command );
	const intptr_t a0 = va_arg( args, intptr_t ), a1 = va_arg( args, intptr_t );
	va_end( args );
	if ( q3ui.style == Q3TestUIStyle::Engine105 ) return CL_Q3TestMenuEntry( command, a0, a1 );
	if ( !q3ui.exports ) return 0;
	switch ( command ) {
	case UI_INIT:
		return Q3TestUICall( [] { Q3TestUIExport( 0, 2, q3ui.imports.data() ); return intptr_t( 0 ); } );
	case UI_SHUTDOWN:
		return Q3TestUICall( [] { Q3TestUIExport( 1 ); return intptr_t( 0 ); } );
	case UI_KEY_EVENT:
		if ( !a1 ) return 0;
		return Q3TestUICall( [=] { Q3TestUIExport( 2, static_cast<int>( a0 ) ); return intptr_t( 0 ); } );
	case UI_MOUSE_EVENT:
		return Q3TestUICall( [=] { Q3TestUIExport( 3, static_cast<int>( a0 ), static_cast<int>( a1 ) ); return intptr_t( 0 ); } );
	case UI_REFRESH:
		return Q3TestUICall( [=] { Q3TestUIExport( 4, static_cast<int>( a0 ) ); return intptr_t( 0 ); } );
	case UI_IS_FULLSCREEN:
		return Q3TestUICall( [] {
			qboolean fullscreen = qfalse;
			Q3TestUIExport( 5, static_cast<char **>( nullptr ), &fullscreen );
			return intptr_t( fullscreen ? 1 : 0 );
		} );
	case UI_SET_ACTIVE_MENU: {
		const char *menu = a0 == UIMENU_NONE ? nullptr : a0 == UIMENU_MAIN ? "main" : a0 == UIMENU_INGAME ? "ingame" : "";
		if ( menu && !menu[0] ) return 0;
		return Q3TestUICall( [=] { Q3TestUIExport( 6, menu ); return intptr_t( 0 ); } );
	}
	case UI_CONSOLE_COMMAND:
		return Q3TestUICall( [] { return Q3TestUIExport( 7 ) & 0xff ? intptr_t( 1 ) : intptr_t( 0 ); } );
	case UI_DRAW_CONNECT_SCREEN:
		return Q3TestUICall( [=] {
			Q3TestUIExport( a0 ? 9 : 8, static_cast<const char *>( cls.servername ),
				static_cast<const char *>( cls.updateInfoString ) );
			return intptr_t( 0 );
		} );
	default:
		return 0;
	}
}

} // namespace

/* CL_InitUI's library half: the release's module or its engine menus. */
vm_t *CL_Q3TestLoadUI( void ) {
	const q3testRelease_t &release = Q3TestRelease();
	q3ui = Q3TestUI{};
	if ( release.id <= Q3TEST_RELEASE_1_05 ) {
		q3ui.style = Q3TestUIStyle::Engine105;
		CL_Q3TestMenuReset();
		return VM_CreateHosted( VM_UI, CL_Q3TestUIEntry );
	}
	// The retail libraries are 32-bit; so must the process be that loads them.
	if ( sizeof( void * ) != 4 ) {
		Com_Error( ERR_FATAL, "Q3Test's menus are a Win32 x86 library; this build cannot load them" );
		return nullptr;
	}
	void *symbol = nullptr;
	vm_t *vm = VM_CreateTable( VM_UI, "GetUIAPI", CL_Q3TestUIEntry, &symbol );
	if ( !vm ) return nullptr;
	auto getter = reinterpret_cast<const std::intptr_t *( QDECL * )( void )>( symbol );
	q3ui.exports = getter();
	if ( !q3ui.exports ) {
		VM_Free( vm );
		Com_Error( ERR_FATAL, "Sys_GetUIAPI failed" );
		return nullptr;
	}
	const bool v108 = release.id >= Q3TEST_RELEASE_1_08;
	q3ui.style = v108 ? Q3TestUIStyle::Table108 : Q3TestUIStyle::Table106;
	Q3TestFillImports( v108 );
	return vm;
}

void CL_Q3TestShutdownUI( void ) {
	q3ui = Q3TestUI{};
}

/* The connect screen's two strings, pushed by the engine (exports 10 and 11;
 * 1.05 0x41f1a0 and 0x41f1c0): each is stored and the screen redrawn. */
void CL_Q3TestUIConnectCount( const char *text ) {
	if ( !uivm || !Com_GameIsQ3Test() ) return;
	if ( q3ui.style == Q3TestUIStyle::Engine105 ) {
		CL_Q3TestMenuConnectText( text, false );
		return;
	}
	if ( !q3ui.exports ) return;
	Q3TestUICall( [=] { Q3TestUIExport( 10, text ); return intptr_t( 0 ); } );
}

void CL_Q3TestUIServerMessage( const char *text ) {
	if ( !uivm || !Com_GameIsQ3Test() ) return;
	if ( q3ui.style == Q3TestUIStyle::Engine105 ) {
		CL_Q3TestMenuConnectText( text, true );
		return;
	}
	if ( !q3ui.exports ) return;
	Q3TestUICall( [=] { Q3TestUIExport( 11, text ); return intptr_t( 0 ); } );
}

/* The start of a client frame, outside every call into the module: the
 * reloads it asked for run here, in order, each as one command. A call that
 * ended in an error left its depth behind. */
void CL_Q3TestUIFrame( void ) {
	q3ui.depth = 0;
	if ( q3ui.deferred.empty() ) return;
	const std::vector<std::string> commands = std::move( q3ui.deferred );
	q3ui.deferred.clear();
	for ( const std::string &command : commands ) Cmd_ExecuteString( command.c_str() );
}
