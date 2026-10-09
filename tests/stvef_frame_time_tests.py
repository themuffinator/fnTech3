"""Execute the engine's frame-time rule against both retail Elite Force engines.

Com_ModifyMsec turns a frame's elapsed wall time into game time. The user's
stvoy.exe 1.20 (0x412f70) and stvoyHM.exe 1.20 (inline in Com_Frame,
0x41c70f-0x41c77e) scale it by timescale with an x87 multiply truncated by
_ftol, leave it unscaled when timescale is 0, let no frame fall below 1 msec,
and cap it: single player at 200 msec, or 500 while its game skips a
cutscene at timescale 100 (`skippingCinematic`, stvoy.exe 0x412fae);
Holomatch at 5000 on a dedicated server, 200 while it hosts a game and 5000
otherwise. The production function is compiled with stubbed cvars; the other
profiles are controls that keep their existing rule. Both servers also keep
a fixed tick under timescale, neither restarts its frame clock after a level
loads, and a dedicated Holomatch server sleeps 5 ms between frames, as
stvoyHM.exe's WinMain does. No game is started.
"""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]


def function(source, signature):
    start = source.index(signature)
    return source[start:source.index('\n}\n', start) + 3]


HARNESS = r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
typedef enum { qfalse, qtrue } qboolean;
typedef struct { int integer; float value; } cvar_t;
enum { QUAKE3, QUAKELIVE, RTCW_SP, RTCW_MP, WOLFET, STVEF_SP, STVEF_MP };
static cvar_t fixedtime, timescale, cameraMode, dedicated, running;
static cvar_t *com_fixedtime = &fixedtime, *com_timescale = &timescale, *com_cameraMode = &cameraMode,
    *com_dedicated = &dedicated, *com_sv_running = &running;
static int profile, skipping, hitches;
static int Com_GameIsSTVEF( void ) { return profile == STVEF_SP || profile == STVEF_MP; }
static int Com_GameIsSTVEFSP( void ) { return profile == STVEF_SP; }
static int Cvar_VariableIntegerValue( const char *name ) {
    assert( !strcmp( name, "skippingCinematic" ) );
    return skipping;
}
static void Com_Printf( const char *format, ... ) {
    if ( strstr( format, "Hitch warning" ) ) ++hitches;
}

PRODUCTION

/* host: 0 client of no local game, 1 hosting a game, 2 dedicated server */
static int Frame( int game, int host, float scale, int fixed, int skip, int msec ) {
    profile = game; skipping = skip;
    dedicated.integer = host == 2; running.integer = host != 0;
    timescale.value = scale; timescale.integer = (int)scale;
    fixedtime.integer = fixed;
    cameraMode.integer = 0;
    return Com_ModifyMsec( msec );
}

int main( void ) {
    int game, host;
    for ( game = STVEF_SP; game <= STVEF_MP; ++game ) {
        for ( host = 0; host < 3; ++host ) {
            if ( game == STVEF_SP && host == 2 ) continue;
            assert( Frame( game, host, 1.0f, 0, 0, 16 ) == 16 );
            assert( Frame( game, host, 3.0f, 0, 0, 16 ) == 48 );
            /* A zero timescale leaves the frame unscaled. */
            assert( Frame( game, host, 0.0f, 0, 0, 16 ) == 16 );
            /* The exact product 6.99999988 truncates to 6; a float multiply
               would round it to 7 first. */
            assert( Frame( game, host, 0.7f, 0, 0, 10 ) == 6 );
            assert( Frame( game, host, 0.3f, 0, 0, 10 ) == 3 );
            /* No frame falls below 1 msec, whatever the timescale. */
            assert( Frame( game, host, 0.01f, 0, 0, 16 ) == 1 );
            assert( Frame( game, host, -1.0f, 0, 0, 16 ) == 1 );
            assert( Frame( game, host, 1.0f, 7, 0, 16 ) == 7 );
            assert( Frame( game, host, 0.0f, -3, 0, 16 ) == 1 );
        }
    }
    /* Single player caps every frame, even with no game running. */
    for ( host = 0; host < 2; ++host ) {
        assert( Frame( STVEF_SP, host, 1.0f, 0, 0, 1000 ) == 200 );
        assert( Frame( STVEF_SP, host, 1.0f, 0, 1, 1000 ) == 500 );
        assert( Frame( STVEF_SP, host, 100.0f, 0, 0, 16 ) == 200 );
        assert( Frame( STVEF_SP, host, 100.0f, 0, 1, 16 ) == 500 );
        assert( Frame( STVEF_SP, host, 100.0f, 0, 1, 4 ) == 400 );
    }
    /* Holomatch keeps the shared caps and the dedicated hitch warning. */
    hitches = 0;
    assert( Frame( STVEF_MP, 2, 1.0f, 0, 1, 6000 ) == 5000 && hitches == 1 );
    assert( Frame( STVEF_MP, 2, 1.0f, 0, 0, 500 ) == 500 && hitches == 1 );
    assert( Frame( STVEF_MP, 1, 1.0f, 0, 1, 1000 ) == 200 );
    assert( Frame( STVEF_MP, 0, 1.0f, 0, 1, 1000 ) == 1000 );
    assert( Frame( STVEF_MP, 0, 1.0f, 0, 0, 6000 ) == 5000 );

    /* Controls: the other profiles keep their rule. A zero timescale stops
       game time and a negative one is not raised to 1 msec. */
    for ( game = QUAKE3; game <= WOLFET; ++game ) {
        for ( host = 0; host < 3; ++host ) {
            assert( Frame( game, host, 1.0f, 0, 1, 16 ) == 16 );
            assert( Frame( game, host, 3.0f, 0, 0, 16 ) == 48 );
            assert( Frame( game, host, 0.0f, 0, 0, 16 ) == 0 );
            assert( Frame( game, host, -1.0f, 0, 0, 16 ) == -16 );
            assert( Frame( game, host, 0.01f, 0, 0, 16 ) == 1 );
        }
        assert( Frame( game, 1, 1.0f, 0, 1, 1000 ) == 200 );
        assert( Frame( game, 0, 1.0f, 0, 1, 1000 ) == 1000 );
        assert( Frame( game, 2, 1.0f, 0, 1, 6000 ) == 5000 );
    }
    puts( "Elite Force frame-time rules and five control profiles passed" );
    return 0;
}
'''


class STVEFFrameTimeTests(unittest.TestCase):
    def test_frame_time_follows_both_retail_engines(self):
        compiler = shutil.which('clang') or shutil.which('gcc')
        if not compiler:
            self.skipTest('C compiler unavailable')
        common = (ROOT / 'code/qcommon/common.c').read_text(encoding='utf-8').replace('\r\n', '\n')
        production = function(common, 'static int Com_ModifyMsec( int msec ) {')
        with tempfile.TemporaryDirectory(prefix='stvef-frame-time-') as folder:
            source = Path(folder) / 'probe.c'
            exe = Path(folder) / ('probe.exe' if os.name == 'nt' else 'probe')
            source.write_text(HARNESS.replace('PRODUCTION', production), encoding='utf-8')
            built = subprocess.run([compiler, *compiler_target_args(compiler), '-std=c99', '-Wall', '-Wextra',
                                    '-Wno-unused-function', '-D_CRT_SECURE_NO_WARNINGS', str(source), '-o', str(exe)],
                                   capture_output=True, text=True, timeout=120)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            options = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
            ran = subprocess.run([str(exe)], capture_output=True, text=True, timeout=30, **options)
            self.assertEqual(ran.returncode, 0, ran.stdout + ran.stderr)
            self.assertIn('passed', ran.stdout)

    def test_both_servers_keep_a_fixed_tick(self):
        main = (ROOT / 'code/server/sv_main.cpp').read_text(encoding='utf-8').replace('\r\n', '\n')
        frame = function(main, 'void SV_Frame( int msec, int realMsec ) {')
        fixed = frame.index('if ( Com_GameIsSTVEF() ) {')
        self.assertLess(fixed, frame.index('frameMsec = 1000 / sv_fps->integer;'))
        self.assertLess(frame.index('frameMsec = 1000 / sv_fps->integer;'),
                        frame.index('} else if ( com_timescale->value > 0.0f ) {'))
        # Only single player bounds a frame's game time to a second.
        self.assertIn('if ( stvefSP && msec > 1000 ) {', frame)

    def test_a_dedicated_holomatch_server_sleeps_between_frames(self):
        # stvoyHM.exe 1.20 sleeps 5 ms before each Com_Frame of a dedicated
        # server (WinMain 0x440a1a) and leaves SV_Frame at once while no tick
        # is owed (0x43529c, tests/stvef_sp_cadence_tests.py). Every other
        # profile's dedicated server waits for its next tick.
        common = (ROOT / 'code/qcommon/common.c').read_text(encoding='utf-8').replace('\r\n', '\n')
        frame = function(common, 'void Com_Frame( qboolean noDelay ) {')
        self.assertIn('\tif ( com_dedicated->integer ) {\n', frame)
        self.assertIn('minMsec = Com_GameIsSTVEFMP() ? com_frameEndTime + 6 - com_frameTime : SV_FrameMsec();',
                      frame)
        self.assertTrue(frame.endswith('\tcom_frameNumber++;\n\tcom_frameEndTime = Sys_Milliseconds();\n}\n'))
        self.assertEqual(common.count('com_frameEndTime = '), 1)
        self.assertEqual(common.count('minMsec = SV_FrameMsec()'), 0)

    def test_a_level_load_counts_into_the_next_frame(self):
        # Neither retail engine sets its frame clock outside Com_Frame
        # (stvoy.exe 0x413061/0x41307b, stvoyHM.exe 0x41c6da/0x41c6fb), so the
        # frame after a load is capped rather than restarted.
        init = (ROOT / 'code/server/sv_init.cpp').read_text(encoding='utf-8').replace('\r\n', '\n')
        spawn = function(init, 'void SV_SpawnServer( const char *mapname, qboolean killBots ) {')
        self.assertEqual(spawn.count('Com_FrameInit();'), 1)
        self.assertIn('\tif ( !Com_GameIsSTVEF() ) {\n\t\tCom_FrameInit();\n\t}\n}\n', spawn)


if __name__ == '__main__':
    unittest.main()
