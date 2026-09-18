"""Run the Holomatch cgame and UI import dispatchers against recording stubs.

The probe compiles the shipping client headers. It checks that every import
number retail's own dispatchers accept is handled (stvoyHM.exe 1.20: cgame
0x402ed0 takes 0-20, 22-60 and 100-110; UI 0x4125a0 takes 0-29, 31-58 and
100-108), that every other number is refused as retail refuses it, that the
calls shared with this engine land on the matching internal operation, and
that each Elite Force-specific import keeps its retail contract.
"""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]

PROBE = r'''
#include "code/qcommon/q_shared.h"
#include "code/qcommon/qcommon.h"
#include "code/renderercommon/tr_types.h"
#include "code/cgame/cg_public.h"
#include "code/ui/ui_public.h"
#include <algorithm>
#include <cassert>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

void Q_strncpyz( char *dest, const char *src, int destsize ) {
	int index = 0;
	while ( index + 1 < destsize && src[index] ) { dest[index] = src[index]; ++index; }
	dest[index] = '\0';
}

struct ProbeError { std::string text; };
static std::string printed;
void QDECL Com_Printf( const char *fmt, ... ) {
	char text[1024]; va_list list; va_start( list, fmt ); vsnprintf( text, sizeof( text ), fmt, list ); va_end( list );
	printed += text;
}
void NORETURN QDECL Com_Error( errorParm_t, const char *fmt, ... ) {
	char text[1024]; va_list list; va_start( list, fmt ); vsnprintf( text, sizeof( text ), fmt, list ); va_end( list );
	throw ProbeError{ text };
}

// Module memory is flat in the probe; bounds checks are recorded.
static vm_t *cgvm = reinterpret_cast<vm_t *>( 1 ), *uivm = reinterpret_cast<vm_t *>( 2 );
struct Bounds { const vm_t *vm; intptr_t address, bytes; };
static std::vector<Bounds> bounds;
#undef VM_CHECKBOUNDS
#define VM_CHECKBOUNDS( vm, address, bytes ) bounds.push_back( { vm, (intptr_t)( address ), (intptr_t)( bytes ) } )
static void *VM_ArgPtr( intptr_t address ) { return reinterpret_cast<void *>( address ); }
class VmArgPtr {
public:
	explicit VmArgPtr( void *ptr ) : ptr_( ptr ) {}
	template <typename T> operator T *() const { return static_cast<T *>( ptr_ ); }
	operator void *() const { return ptr_; }
	const char *String() const { return static_cast<const char *>( ptr_ ); }
private:
	void *ptr_;
};
#undef VMA
#define VMA(x) VmArgPtr( VM_ArgPtr( args[x] ) )

struct FileRead { void *buffer; int length; fileHandle_t file; handleOwner_t owner; };
static std::vector<FileRead> reads;
int FS_VM_ReadFile( void *buffer, int len, fileHandle_t f, handleOwner_t owner ) {
	reads.push_back( { buffer, len, f, owner } ); return len;
}

static std::vector<std::string> sounds;
void S_ClearLoopingSounds( qboolean killall ) { sounds.push_back( killall ? "clear all" : "clear" ); }
sfxHandle_t S_RegisterSound( const char *name, qboolean compressed ) {
	sounds.push_back( std::string( "register " ) + name + ( compressed ? " compressed" : "" ) );
	return 41;
}
void S_StopBackgroundTrack( void ) { sounds.push_back( "stop music" ); }
void S_StartBackgroundTrack( const char *intro, const char *loop ) {
	sounds.push_back( std::string( "music " ) + intro + " " + loop );
}

static glconfig_t rendererConfig;
static std::vector<std::string> shaders;
static qhandle_t RegisterShader3D( const char *name ) { shaders.push_back( name ); return 77; }
static const glconfig_t *GetConfig( void ) { return &rendererConfig; }
static struct {
	qhandle_t ( *RegisterShader3D )( const char *name );
	const glconfig_t *( *GetConfig )( void );
} re = { RegisterShader3D, GetConfig };
static std::vector<std::string> hudNames;
void CL_HudRegisterShaderName( qhandle_t shader, const char *name ) {
	hudNames.push_back( std::to_string( shader ) + " " + name );
}

#define MAX_PARSE_ENTITIES ( PACKET_BACKUP * 256 )
typedef struct {
	int snapFlags, ping, serverTime;
	byte areamask[MAX_MAP_AREA_BYTES];
	playerState_t ps;
	int numEntities, parseEntitiesNum, serverCommandNum;
} clSnapshot_t;
static struct { entityState_t parseEntities[MAX_PARSE_ENTITIES]; gameState_t gameState; } cl;
static struct { glconfig_t glconfig; } cls;
static const clSnapshot_t *CL_FindSnapshot( int ) { return nullptr; }
static qboolean CL_GetUserCmd( int, usercmd_t * ) { return qfalse; }

// One cvar, "model", with the engine's userinfo bookkeeping.
int cvar_modifiedFlags;
static cvar_t model;
static bool modelExists;
static std::string modelValue;
unsigned Cvar_Flags( const char * ) { return modelExists ? model.flags : CVAR_NONEXISTENT; }
cvar_t *Cvar_Set2( const char *, const char *value, qboolean force ) {
	assert( force );
	if ( !modelExists ) { modelExists = true; model.flags = 0; }
	modelValue = value; model.modified = qtrue;
	if ( model.flags & CVAR_USERINFO ) cvar_modifiedFlags |= CVAR_USERINFO;
	return &model;
}

struct LanCall { int source, index, length; };
static std::vector<LanCall> lan;
static int LAN_GetServerCount( int source ) { lan.push_back( { source, -1, 0 } ); return source == AS_LOCAL ? 3 : 250; }
static void LAN_GetServerAddressString( int source, int n, char *buf, int buflen ) {
	lan.push_back( { source, n, buflen } ); Q_strncpyz( buf, source == AS_LOCAL ? "10.0.0.2:27960" : "192.0.2.9:27960", buflen );
}

#include "code/client/client_stvef_cgame.hpp"
#include "code/client/client_stvef_ui.hpp"

using namespace fntech3::stvef::module;

static char scratch[3][65536];
struct Outcome { bool handled; intptr_t operation, result; std::string error; };
static Outcome Dispatch( bool ui, intptr_t *args ) {
	Outcome outcome{};
	try {
		outcome.handled = ui ? CL_STVEFUICall( args, outcome.operation, outcome.result )
			: CL_STVEFCgameCall( args, outcome.operation, outcome.result );
	} catch ( const ProbeError &error ) {
		outcome.error = error.text;
	}
	return outcome;
}
static Outcome Dispatch( bool ui, std::initializer_list<intptr_t> values ) {
	intptr_t args[16]{};
	std::copy( values.begin(), values.end(), args );
	return Dispatch( ui, args );
}
static intptr_t Address( void *pointer ) { return reinterpret_cast<intptr_t>( pointer ); }
static int Number( bool ui, Call call ) {
	(void)ui;
	return Ordinal( COM_GAME_STVEF_MP, call );
}

static bool RetailAccepts( bool ui, int number ) {
	if ( ui ) return ( number >= 0 && number <= 29 ) || ( number >= 31 && number <= 58 ) || ( number >= 100 && number <= 108 );
	return ( number >= 0 && number <= 20 ) || ( number >= 22 && number <= 60 ) || ( number >= 100 && number <= 110 );
}

static void Coverage() {
	for ( bool ui : { false, true } ) {
		for ( int number = -2; number <= 140; ++number ) {
			std::memset( scratch, 0, sizeof( scratch ) );
			intptr_t args[16]{};
			args[0] = number; args[1] = Address( scratch[0] ); args[2] = Address( scratch[1] ); args[3] = 16;
			const Outcome outcome = Dispatch( ui, args );
			if ( RetailAccepts( ui, number ) ) {
				if ( !outcome.error.empty() ) { std::fprintf( stderr, "%s %d: %s\n", ui ? "ui" : "cgame", number, outcome.error.c_str() ); }
				assert( outcome.error.empty() );
				if ( !outcome.handled ) assert( outcome.operation >= 0 );
			} else {
				const std::string expected = std::string( ui ? "Bad UI system trap: " : "Bad cgame system trap: " ) + std::to_string( number );
				assert( outcome.error == expected );
			}
		}
	}
}

// Calls shared with this engine keep Quake III 1.32's operation numbers: all of
// the cgame's, the UI's first 46, and the UI's moved calls by name.
static void SharedNumbers() {
	for ( int number = 0; number <= 110; ++number ) {
		if ( !RetailAccepts( false, number ) ) continue;
		intptr_t args[16]{ number, Address( scratch[0] ), Address( scratch[1] ), 16 };
		const Outcome outcome = Dispatch( false, args );
		if ( !outcome.handled ) assert( outcome.operation == number );
	}
	const struct { int number; int operation; } moved[] = {
		{ 50, UI_LAN_GETPINGQUEUECOUNT }, { 51, UI_LAN_CLEARPING }, { 52, UI_LAN_GETPING },
		{ 53, UI_LAN_GETPINGINFO }, { 54, UI_CVAR_REGISTER }, { 55, UI_CVAR_UPDATE },
		{ 56, UI_MEMORY_REMAINING }, { 58, UI_R_MODELBOUNDS },
	};
	for ( int number = 0; number <= 108; ++number ) {
		if ( !RetailAccepts( true, number ) ) continue;
		intptr_t args[16]{ number, Address( scratch[0] ), Address( scratch[1] ), 16 };
		const Outcome outcome = Dispatch( true, args );
		if ( outcome.handled ) continue;
		if ( number <= 45 || number >= 100 ) { assert( outcome.operation == number ); continue; }
		bool found = false;
		for ( const auto &entry : moved ) if ( entry.number == number ) { assert( outcome.operation == entry.operation ); found = true; }
		assert( found );
	}
}

static void Files() {
	for ( bool ui : { false, true } ) {
		const int read = Number( ui, ui ? Call::UI_FS_READ : Call::CG_FS_READ );
		bounds.clear(); reads.clear();
		// Retail reads nothing from handle 0, before it looks at the length.
		Outcome outcome = Dispatch( ui, { read, Address( scratch[0] ), -1, 0 } );
		assert( outcome.handled && outcome.error.empty() && bounds.empty() && reads.empty() );
		outcome = Dispatch( ui, { read, Address( scratch[0] ), 12, 5 } );
		assert( outcome.handled && bounds.size() == 1 && reads.size() == 1 );
		assert( bounds[0].vm == ( ui ? uivm : cgvm ) && bounds[0].address == Address( scratch[0] ) && bounds[0].bytes == 12 );
		assert( reads[0].buffer == scratch[0] && reads[0].length == 12 && reads[0].file == 5 );
		assert( reads[0].owner == ( ui ? H_Q3UI : H_CGAME ) );
	}
}

static void Sounds() {
	sounds.clear();
	std::strcpy( scratch[0], "sound/weapons/phaser.wav" );
	Outcome outcome = Dispatch( false, { Number( false, Call::CG_S_REGISTERSOUND ), Address( scratch[0] ), 1 } );
	assert( outcome.handled && outcome.result == 41 );
	outcome = Dispatch( true, { Number( true, Call::UI_S_REGISTERSOUND ), Address( scratch[0] ), 1 } );
	assert( outcome.handled && outcome.result == 41 );
	outcome = Dispatch( false, { Number( false, Call::CG_S_CLEARLOOPINGSOUNDS ), 1 } );
	assert( outcome.handled );
	// Elite Force has no stop import: an empty intro stops the music.
	std::strcpy( scratch[1], "music/loop.mp3" );
	scratch[0][0] = '\0';
	outcome = Dispatch( false, { Number( false, Call::CG_S_STARTBACKGROUNDTRACK ), Address( scratch[0] ), Address( scratch[1] ) } );
	assert( outcome.handled );
	std::strcpy( scratch[0], "music/intro.mp3" );
	outcome = Dispatch( false, { Number( false, Call::CG_S_STARTBACKGROUNDTRACK ), Address( scratch[0] ), Address( scratch[1] ) } );
	assert( outcome.handled );
	const std::vector<std::string> expected = { "register sound/weapons/phaser.wav", "register sound/weapons/phaser.wav",
		"clear", "stop music", "music music/intro.mp3 music/loop.mp3" };
	assert( sounds == expected );
}

static void Renderer() {
	shaders.clear(); hudNames.clear();
	std::strcpy( scratch[0], "gfx/misc/blue_portashield" );
	const Outcome outcome = Dispatch( false, { Number( false, Call::CG_R_REGISTERSHADER3D ), Address( scratch[0] ) } );
	assert( outcome.handled && outcome.result == 77 );
	assert( shaders.size() == 1 && shaders[0] == "gfx/misc/blue_portashield" );
	assert( hudNames.size() == 1 && hudNames[0] == "77 gfx/misc/blue_portashield" );

	// The UI reads the renderer's configuration and the cgame the client's copy.
	Q_strncpyz( rendererConfig.renderer_string, "renderer", sizeof( rendererConfig.renderer_string ) );
	Q_strncpyz( cls.glconfig.renderer_string, "client copy", sizeof( cls.glconfig.renderer_string ) );
	rendererConfig.vidWidth = 1024; cls.glconfig.vidWidth = 640;
	bounds.clear();
	Dispatch( true, { Number( true, Call::UI_GETGLCONFIG ), Address( scratch[2] ) } );
	auto *config = reinterpret_cast<fntech3_stvef_mp_glconfig_t *>( scratch[2] );
	assert( !std::strcmp( config->renderer_string, "renderer" ) && config->vidWidth == 1024 );
	assert( config->textureFilterAnisotropicAvailable == 1 );
	assert( bounds.size() == 1 && bounds[0].bytes == (intptr_t)sizeof( fntech3_stvef_mp_glconfig_t ) );
	Dispatch( false, { Number( false, Call::CG_GETGLCONFIG ), Address( scratch[2] ) } );
	assert( !std::strcmp( config->renderer_string, "client copy" ) && config->vidWidth == 640 );
}

static void Cvars() {
	// A new cvar is simply created, and no userinfo update is announced.
	std::strcpy( scratch[0], "model" ); std::strcpy( scratch[1], "munro/red" );
	modelExists = false; cvar_modifiedFlags = 0;
	Dispatch( false, { Number( false, Call::CG_CVAR_SET_NO_MODIFY ), Address( scratch[0] ), Address( scratch[1] ) } );
	assert( modelExists && modelValue == "munro/red" && model.flags == 0 && model.modified == qtrue );
	assert( !( cvar_modifiedFlags & CVAR_USERINFO ) );
	// An existing one becomes userinfo, unmodified, and still announces nothing.
	model.flags = CVAR_ARCHIVE | CVAR_USERINFO; cvar_modifiedFlags = 0;
	std::strcpy( scratch[1], "munro/blue" );
	Dispatch( false, { Number( false, Call::CG_CVAR_SET_NO_MODIFY ), Address( scratch[0] ), Address( scratch[1] ) } );
	assert( modelValue == "munro/blue" && ( model.flags & CVAR_USERINFO ) && model.modified == qfalse );
	assert( !( cvar_modifiedFlags & CVAR_USERINFO ) );
	// An update already pending stays pending.
	cvar_modifiedFlags = CVAR_USERINFO;
	Dispatch( false, { Number( false, Call::CG_CVAR_SET_NO_MODIFY ), Address( scratch[0] ), Address( scratch[1] ) } );
	assert( cvar_modifiedFlags & CVAR_USERINFO );
}

static void Prints() {
	printed.clear();
	std::strcpy( scratch[0], "count: " );
	Dispatch( false, { Number( false, Call::CG_TESTPRINTINT ), Address( scratch[0] ), -42 } );
	float value = 1.5f; int32_t bits; std::memcpy( &bits, &value, 4 );
	Dispatch( false, { Number( false, Call::CG_TESTPRINTFLOAT ), Address( scratch[0] ), bits } );
	assert( printed == "count: -42\ncount: 1.500000\n" );
}

static void ServerLists() {
	lan.clear();
	Outcome outcome = Dispatch( true, { Number( true, Call::UI_LAN_GETLOCALSERVERCOUNT ) } );
	assert( outcome.handled && outcome.result == 3 );
	outcome = Dispatch( true, { Number( true, Call::UI_LAN_GETGLOBALSERVERCOUNT ) } );
	assert( outcome.handled && outcome.result == 250 );
	bounds.clear();
	outcome = Dispatch( true, { Number( true, Call::UI_LAN_GETLOCALSERVERADDRESSSTRING ), 2, Address( scratch[0] ), 64 } );
	assert( outcome.handled && !std::strcmp( scratch[0], "10.0.0.2:27960" ) );
	assert( bounds.size() == 1 && bounds[0].vm == uivm && bounds[0].bytes == 64 );
	outcome = Dispatch( true, { Number( true, Call::UI_LAN_GETGLOBALSERVERADDRESSSTRING ), 7, Address( scratch[0] ), 64 } );
	assert( outcome.handled && !std::strcmp( scratch[0], "192.0.2.9:27960" ) );
	assert( lan.size() == 4 );
	assert( lan[0].source == AS_LOCAL && lan[1].source == AS_GLOBAL );
	assert( lan[2].source == AS_LOCAL && lan[2].index == 2 && lan[2].length == 64 );
	assert( lan[3].source == AS_GLOBAL && lan[3].index == 7 && lan[3].length == 64 );
	for ( intptr_t length : { 0, -1 } ) {
		lan.clear();
		outcome = Dispatch( true, { Number( true, Call::UI_LAN_GETLOCALSERVERADDRESSSTRING ), 2, Address( scratch[0] ), length } );
		assert( outcome.error == "Invalid Elite Force server address buffer" && lan.empty() );
	}
}

static void CdKey() {
	// No key is accepted or stored: the authorize service is gone.
	std::strcpy( scratch[0], "ABCD1234EFGH5678" );
	const Outcome outcome = Dispatch( true, { Number( true, Call::UI_SET_CDKEY ), Address( scratch[0] ) } );
	assert( outcome.handled && outcome.result == qfalse && outcome.error.empty() );
}

static void GameState() {
	// Configstring 0 set, every other one the shared empty string at offset 0.
	const char info[] = "\\sv_hostname\\holodeck";
	gameState_t &state = cl.gameState;
	std::memset( &state, 0, sizeof( state ) );
	std::memcpy( state.stringData + 1, info, sizeof( info ) );
	state.stringOffsets[0] = 1;
	state.dataCount = 1 + (int)sizeof( info );
	bounds.clear();
	Outcome outcome = Dispatch( false, { Number( false, Call::CG_GETGAMESTATE ), Address( scratch[2] ) } );
	assert( outcome.handled && outcome.error.empty() );
	assert( bounds.size() == 1 && bounds[0].bytes == (intptr_t)sizeof( fntech3_stvef_gameState_t ) );
	const auto *copy = reinterpret_cast<const fntech3_stvef_gameState_t *>( scratch[2] );
	assert( copy->dataCount == state.dataCount && copy->stringOffsets[0] == 1 && copy->stringOffsets[1] == 0 );
	assert( !std::strcmp( copy->stringData + copy->stringOffsets[0], info ) );
	// A gamestate whose offsets leave its own text is refused, not published.
	state.stringOffsets[5] = state.dataCount;
	outcome = Dispatch( false, { Number( false, Call::CG_GETGAMESTATE ), Address( scratch[2] ) } );
	assert( outcome.error == "Invalid Elite Force gamestate module transfer" );
	std::memset( &state, 0, sizeof( state ) );
	state.dataCount = 1;
}

static void Records() {
	std::memset( scratch[2], 0x5a, sizeof( scratch[2] ) );
	Outcome outcome = Dispatch( false, { Number( false, Call::CG_GETSNAPSHOT ), 12, Address( scratch[2] ) } );
	assert( outcome.handled && outcome.result == qfalse );
	outcome = Dispatch( false, { Number( false, Call::CG_GETUSERCMD ), 12, Address( scratch[2] ) } );
	assert( outcome.handled && outcome.result == qfalse );
	// A refused snapshot or command leaves the module's buffer as it was.
	for ( int i = 0; i < 4096; ++i ) assert( scratch[2][i] == 0x5a );
}

int main() {
	// The client always holds at least the empty string every unset
	// configstring points at.
	cl.gameState.dataCount = 1;
	Coverage();
	SharedNumbers();
	Files();
	Sounds();
	Renderer();
	Cvars();
	Prints();
	ServerLists();
	CdKey();
	GameState();
	Records();
	std::puts( "ok" );
	return 0;
}
'''


class STVEFClientModuleDispatchTests(unittest.TestCase):
    def test_holomatch_cgame_and_ui_imports_follow_retail(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++17 compiler unavailable')
        (ROOT / '.tmp').mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='stvef-dispatch-', dir=ROOT / '.tmp') as temp:
            folder = Path(temp)
            source = folder / 'probe.cpp'
            executable = folder / ('probe.exe' if os.name == 'nt' else 'probe')
            source.write_text(PROBE, encoding='utf-8')
            target = compiler_target_args(compiler)
            if os.name == 'nt' and not target:
                target = ['-DFNTECH3_Q3_ONLY']
            elif os.name != 'nt':
                target = ['-m32']
            build = subprocess.run([compiler, *target, '-std=c++17', '-D_CRT_SECURE_NO_WARNINGS',
                '-I', str(ROOT), str(source),
                '-o', str(executable)], cwd=ROOT, capture_output=True, text=True, timeout=120)
            self.assertEqual(build.returncode, 0, build.stdout + build.stderr)
            options = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
            run = subprocess.run([str(executable)], cwd=ROOT, capture_output=True, text=True, timeout=30, **options)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)
            self.assertIn('ok', run.stdout)


if __name__ == '__main__':
    unittest.main()
