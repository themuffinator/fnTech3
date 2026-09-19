"""Elite Force SP's cgame: the other half of efgamex86.dll, hosted in the client.

Retail stvoy.exe attaches the cgame half of the game library before the
game's Init, calls its vmMain directly (CG_INIT with the gamestate's command
sequence, CL_InitCGame 0x403280) and answers its 71 imports in one dispatcher
(0x402b20). It shuts the cgame down between the game's Shutdown and freeing
the library (SV_ShutdownGameProgs 0x425450), asks the cgame for a running
camera scene's position when it builds a snapshot (0x4283a3), and its
snapshots carry the number of the last command the server ran. The checks pin
the hosted VM, the import coverage, the lifecycle and those rules; the render
record test covers the rotated picture.
"""
from pathlib import Path
import os
import re
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]


def read(relative):
    return (ROOT / relative).read_text(encoding='utf-8').replace('\r\n', '\n')


def function(source, signature):
    start = source.index(signature)
    return source[start:source.index('\n}\n', start) + 3]


def sp_cgame_imports():
    calls = read('code/compat/stvef_module_calls.def')
    return {name: int(sp) for name, sp in
            re.findall(r'FNTECH3_STVEF_MODULE_CALL\(CGAME, IMPORT, (\w+), (-?\d+), -?\d+\)', calls)
            if int(sp) >= 0}


LAST_COMMAND = r'''
#include <cassert>
#define CMD_BACKUP 64
#define CMD_MASK ( CMD_BACKUP - 1 )
struct Command { int serverTime; };
struct Client { int cmdNumber; Command cmds[CMD_BACKUP]; } cl;
%s
int main() {
	// Nothing sent yet: command 0.
	assert( CL_STVEFSPLastCommandRun( 0 ) == 0 );
	// Commands 1-10 sent 16 ms apart from time 1000.
	for ( int n = 1; n <= 10; ++n ) cl.cmds[n & CMD_MASK].serverTime = 1000 + 16 * n;
	cl.cmdNumber = 10;
	assert( CL_STVEFSPLastCommandRun( 1016 + 16 * 4 ) == 5 );
	assert( CL_STVEFSPLastCommandRun( 1016 + 16 * 4 + 5 ) == 5 );
	assert( CL_STVEFSPLastCommandRun( 5000 ) == 10 );
	// Before the first command's time: none was run.
	assert( CL_STVEFSPLastCommandRun( 1000 ) == 0 );
	// A full backup whose every command is newer: the one before the oldest.
	for ( int n = 100; n < 100 + CMD_BACKUP; ++n ) cl.cmds[n & CMD_MASK].serverTime = 9000 + n;
	cl.cmdNumber = 100 + CMD_BACKUP - 1;
	assert( CL_STVEFSPLastCommandRun( 100 ) == 99 );
	assert( CL_STVEFSPLastCommandRun( 9000 + 120 ) == 120 );
}
'''


class STVEFSPCgameTests(unittest.TestCase):
    def test_every_import_is_answered(self):
        imports = sp_cgame_imports()
        # Retail's dispatcher handles 0-70 and errors beyond (0x402b2a).
        self.assertEqual(sorted(imports.values()), list(range(71)))
        host = read('code/client/client_stvef_sp_cgame.hpp')
        shared = read('code/client/client_stvef_cgame.hpp')
        shared_calls = set(re.findall(r'STVEF_SHARED_CALL\((\w+)\)', shared))
        for name in imports:
            self.assertTrue(f'case Call::{name}:' in host or name in shared_calls, name)
        self.assertIn('"Bad cgame system trap: %ld"', function(host, 'static bool CL_STVEFSPCgameCall('))

    def test_hosted_vm_owns_no_library(self):
        vm = function(read('code/qcommon/vm.c'), 'vm_t *VM_CreateHosted( vmIndex_t index, vmMainFunc_t dispatcher )')
        self.assertIn('vm->entryPoint = dispatcher;', vm)
        self.assertNotIn('dllHandle', vm)
        self.assertIn('vm_t	*VM_CreateHosted( vmIndex_t index, vmMainFunc_t dispatcher );', read('code/qcommon/qcommon.h'))

    def test_engine_calls_become_vmmain_calls(self):
        host = read('code/client/client_stvef_sp_cgame.hpp')
        entry = function(host, 'static intptr_t QDECL CL_STVEFSPCgameEntry( int command, ... )')
        self.assertIn('return cgame( FNTECH3_STVEF_SP_CG_INIT, clc.serverCommandSequence );', entry)
        self.assertIn('return cgame( FNTECH3_STVEF_SP_CG_DRAW_ACTIVE_FRAME, arg[0], arg[1], 0 );', entry)
        self.assertIn('return cgame( FNTECH3_STVEF_SP_CG_CONSOLE_COMMAND );', entry)
        self.assertIn('return cgame( FNTECH3_STVEF_SP_CG_SHUTDOWN );', entry)
        self.assertIn('case CG_LAST_ATTACKER:\n\t\t// vmMain\'s answer for an export it does not have.\n\t\treturn -1;', entry)
        cgame = read('code/client/cl_cgame.cpp')
        init = function(cgame, 'void CL_InitCGame( void ) {')
        self.assertIn('cgvm = VM_CreateHosted( VM_CGAME, CL_STVEFSPCgameEntry );', init)
        self.assertLess(init.index('SV_STVEFSPCgameMain()'), init.index('VM_CreateHosted'))
        self.assertIn('if ( Com_GameIsSTVEFSP() ) {\n\t\treturn;\n\t}', function(cgame, 'void CL_RegisterCGameCvars( void ) {'))
        dispatcher = function(cgame, 'static intptr_t CL_CgameSystemCalls( intptr_t *args ) {')
        self.assertIn('if ( CL_STVEFSPCgameCall( args, operation, stvefSpResult ) ) return stvefSpResult;', dispatcher)

    def test_client_lends_the_server_its_side(self):
        cgame = read('code/client/cl_cgame.cpp')
        self.assertIn('return cgvm ? CL_CgameSystemCalls( args ) : CL_STVEFSPEarlyCgameCall( args );',
                      function(cgame, 'static intptr_t CL_STVEFSPCgameSyscall( intptr_t *args ) {'))
        self.assertIn('SV_STVEFSPAttachClient( CL_STVEFSPCgameSyscall, CL_ShutdownCGame );',
                      function(cgame, 'void CL_STVEFSPAttachCGame( qboolean attach ) {'))
        main = read('code/client/cl_main.cpp')
        self.assertIn('CL_STVEFSPAttachCGame( qtrue );', function(main, 'void CL_Init( void ) {'))
        shutdown = function(main, 'void CL_Shutdown( const char *finalmsg, qboolean quit ) {')
        self.assertLess(shutdown.index('CL_ShutdownVMs();'), shutdown.index('CL_STVEFSPAttachCGame( qfalse );'))

    def test_cgame_shuts_down_before_the_library_unloads(self):
        host = read('code/server/server_stvef_sp_game.hpp')
        shutdown = host[host.index('case GAME_SHUTDOWN:'):host.index('case GAME_CLIENT_CONNECT:')]
        order = ['game->Shutdown();', 'if ( stvefSpClientDetach ) stvefSpClientDetach();', 'stvefSpCgameMain = nullptr;']
        positions = [shutdown.index(text) for text in order]
        self.assertEqual(positions, sorted(positions))
        game = read('code/server/sv_game.cpp')
        self.assertIn('stvefSpClientSyscalls = syscalls;\n\tstvefSpClientDetach = detach;',
                      function(game, 'void SV_STVEFSPAttachClient('))

    def test_level_spawn_calls_before_the_cgame_starts(self):
        early = function(read('code/client/client_stvef_sp_cgame.hpp'), 'static intptr_t CL_STVEFSPEarlyCgameCall(')
        for register in ('RegisterModel', 'RegisterSkin', 'RegisterShader', 'RegisterShaderNoMip'):
            self.assertIn(f'return cls.rendererStarted ? re.{register}( text( 1 ) ) : 0;', early)
        # Sounds start only once the cgame has; the scene waits for it.
        for waiting in ('CG_S_STARTSOUND', 'CG_S_STARTLOCALSOUND', 'CG_S_ADDLOOPINGSOUND', 'CG_R_RENDERSCENE',
                        'CG_GETSNAPSHOT'):
            self.assertNotIn(f'case Call::{waiting}:', early)
        self.assertIn('// Sounds, the scene, snapshots and commands wait for the cgame.\n\t\treturn 0;', early)

    def test_snapshots_in_single_players_record(self):
        host = read('code/client/client_stvef_sp_cgame.hpp')
        snapshot = function(host, 'static qboolean CL_GetSTVEFSPSnapshot(')
        self.assertIn('if ( !source || ( source->snapFlags & SNAPFLAG_NOT_ACTIVE ) ) return qfalse;', snapshot)
        self.assertIn('result.snapFlags = source->snapFlags & SNAPFLAG_RATE_DELAYED;', snapshot)
        self.assertIn('result.cmdNum = source->cmdNum;', snapshot)
        self.assertIn('count = FNTECH3_STVEF_MAX_ENTITIES_IN_SNAPSHOT;', snapshot)
        # The fields retail leaves alone keep the cgame's values.
        self.assertIn('result = *destination;', snapshot)
        parse = read('code/client/cl_parse.cpp')
        self.assertIn('newSnap.cmdNum = CL_STVEFSPLastCommandRun( newSnap.ps.commandTime );', parse)

    def test_last_command_run(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++17 compiler unavailable')
        source = LAST_COMMAND % function(read('code/client/cl_parse.cpp'), 'static int CL_STVEFSPLastCommandRun(')
        target = compiler_target_args(compiler)
        (ROOT / '.tmp').mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='stvef-sp-cgame-', dir=ROOT / '.tmp') as folder:
            cpp = Path(folder) / 'probe.cpp'
            binary = Path(folder) / ('probe.exe' if os.name == 'nt' else 'probe')
            cpp.write_text(source, encoding='utf-8')
            built = subprocess.run([compiler, *target, '-std=c++17', str(cpp), '-o', str(binary)],
                                   capture_output=True, text=True, timeout=120)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            options = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
            ran = subprocess.run([str(binary)], capture_output=True, text=True, timeout=30, **options)
            self.assertEqual(ran.returncode, 0, ran.stdout + ran.stderr)

    def test_snapshots_are_seen_from_the_camera_or_the_leaning_eye(self):
        game = read('code/server/sv_game.cpp')
        camera = function(game, 'qboolean SV_STVEFSPCameraOrigin( vec3_t origin ) {')
        self.assertIn('stvefSpCgameMain( FNTECH3_STVEF_SP_CG_CAMERA_POS, reinterpret_cast<intptr_t>( camera ) )', camera)
        view = function(read('code/server/sv_snapshot.cpp'), 'static void SV_SnapshotViewOrigin(')
        single = view[view.index('if ( Com_GameIsSTVEFSP() ) {'):view.index('VectorCopy( state.origin, origin );\n\tif ( Com_GameIsWolfET() )')]
        order = ['SV_STVEFSPCameraOrigin( origin )', 'origin[2] += state.viewheight;',
                 'angles[ROLL] += state.stvef.leanofs * 0.5f;',
                 'origin[axis] += state.stvef.leanofs * right[axis];']
        positions = [single.index(text) for text in order]
        self.assertEqual(positions, sorted(positions))

    def test_single_player_only_imports(self):
        host = read('code/client/client_stvef_sp_cgame.hpp')
        common = function(host, 'static intptr_t CL_STVEFSPCgameCommonCall(')
        self.assertIn('return S_STVEFSoundLength( static_cast<sfxHandle_t>( args[1] ) );', common)
        self.assertIn('return cls.realtime;', common[common.index('case Call::CG_S_ADDLOCALSET:'):])
        self.assertIn('return -1;', common[common.index('case Call::CG_AS_GETBMODELSOUND:'):])
        self.assertIn('if ( !args[3] ) return 0;', common[common.index('case Call::CG_FS_READ:'):])
        call = function(host, 'static bool CL_STVEFSPCgameCall(')
        self.assertIn('fntech3::stvef::render::RotatedQuad(', call)
        lighting = call[call.index('case Call::CG_R_GETLIGHTING:'):call.index('case Call::CG_R_DRAWSCREENSHOT:')]
        self.assertIn('if ( !re.LightForPoint( VMA(1), ambient, directed, direction ) ) {', lighting)
        self.assertIn('S_ClearLoopingSounds( qfalse );', call)
        # Single player's sound channels become the engine's.
        self.assertIn('S_StartSound( VMA(1), static_cast<int>( args[2] ), CL_STVEFSPSoundChannel( static_cast<int>( args[3] ) ),',
                      call)
        self.assertIn('CL_STVEFSPSoundChannel( static_cast<int>( args[2] ) )',
                      call[call.index('case Call::CG_S_STARTLOCALSOUND:'):])
        self.assertIn('S_StartLocalSound( sfx, CL_STVEFSPSoundChannel( channel ) );',
                      read('code/client/client_stvef_sp_ui.hpp'))
        sound = read('code/client/audio/snd_main.cpp')
        self.assertIn('return length < 0 ? 0 : length;', function(sound, 'extern "C" int S_STVEFSoundLength('))

    def test_renderers_hold_frames_and_take_noise_waves(self):
        self.assertIn('#define	RF_CAP_FRAMES		0x0400', read('code/renderercommon/tr_types.h'))
        for renderer in ('renderer', 'renderervk', 'rendererrtx'):
            mesh = read(f'code/{renderer}/tr_mesh.c')
            cap = mesh.index('} else if ( ent->e.renderfx & RF_CAP_FRAMES ) {')
            self.assertLess(cap, mesh.index('} else if ( ent->e.renderfx & RF_WRAP_FRAMES ) {'))
            self.assertIn('if ( ent->e.frame > frameCount - 1 ) ent->e.frame = frameCount - 1;', mesh)
            waves = function(read(f'code/{renderer}/tr_shade_calc.c'), 'static float EvalWaveForm( const waveForm_t *wf )')
            self.assertIn('if ( wf->func == GF_NOISE && ( ri.gameId == COM_GAME_STVEF_SP || ri.gameId == COM_GAME_STVEF_MP ) ) {',
                          waves)
            self.assertIn('R_NoiseGet4f( 0, 0, 0, ( backEnd.refdef.floatTime + wf->phase ) * wf->frequency )', waves)
            self.assertLess(waves.index('GF_NOISE'), waves.index('table = TableForFunc( wf->func );'))


if __name__ == '__main__':
    unittest.main()
