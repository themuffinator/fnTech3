"""The Ritual profiles' synthetic input, for probes that drive menus and play.

A window on a private desktop never has focus, and the platform layer drops
real input without it, so a probe that plays Heavy Metal: F.A.K.K.2 or
American McGee's Alice queues its keys, characters and mouse motion in the
engine's own event queue instead (`in_synthesize`). Only a client started
with `+set in_synthetic 1` accepts them. These checks pin that gate, that the
command exists under Ritual, Medal of Honor and Wolfenstein profiles (whose mouse
look its probes turn with it), and how scripts/probe_ritual_client.py turns an
--actions script into phases.
"""
from pathlib import Path
import importlib.util
import sys
import unittest

ROOT = Path(__file__).resolve().parents[1]


def read(relative):
    return (ROOT / relative).read_text(encoding='utf-8').replace('\r\n', '\n')


def function(source, signature):
    start = source.index(signature)
    return source[start:source.index('\n}\n', start) + 3]


def load_probe():
    spec = importlib.util.spec_from_file_location('probe_ritual_client', ROOT / 'scripts' / 'probe_ritual_client.py')
    module = importlib.util.module_from_spec(spec)
    sys.path.insert(0, str(ROOT / 'tests'))
    try:
        spec.loader.exec_module(module)
    except (ImportError, OSError, AttributeError) as error:
        raise unittest.SkipTest(f'probe script needs Windows: {error}')
    return module


class SyntheticInputSourceTests(unittest.TestCase):
    def test_only_a_client_started_with_the_switch_takes_it(self):
        keys = read('code/client/cl_keys.cpp')
        init = function(keys, 'void CL_InitSyntheticInput( void ) {')
        self.assertIn('in_synthetic = Cvar_Get( "in_synthetic", "0", CVAR_INIT );', init)
        self.assertIn('Cmd_AddCommand( "in_synthesize", CL_SynthesizeInput_f );', init)
        command = function(keys, 'void CL_SynthesizeInput_f( void ) {')
        gate = command.index('if ( !in_synthetic || !in_synthetic->integer ) {')
        self.assertLess(gate, command.index('Sys_QueEvent'))
        self.assertIn('return;', command[gate:command.index('}', gate)])
        # The startup gate covers the profiles with private-desktop probes;
        # Alice's alt and ctrl bindings stay Ritual's alone.
        common_keys = read('code/qcommon/keys.c')
        self.assertIn('if ( Com_GameIsRitual() ) {\n\t\tCL_AliceInitKeyCommands();\n\t}', common_keys)
        self.assertIn('if ( Com_GameIsRitual() || Com_GameIsMOHAA() || Com_GameIsRTCW() || Com_GameIsWolfET() || Com_GameIsSTVEF() ) {\n\t\tCL_InitSyntheticInput();\n\t}',
                      common_keys)
        self.assertNotIn('in_synthe', function(keys, 'void CL_AliceInitKeyCommands( void ) {'))
        self.assertEqual(keys.count('Cmd_AddCommand( "in_synthesize"'), 1)

    def test_each_kind_goes_where_platform_input_goes(self):
        command = function(read('code/client/cl_keys.cpp'), 'void CL_SynthesizeInput_f( void ) {')
        for event in ('Sys_QueEvent( now, SE_KEY, key, atoi( Cmd_Argv( 3 ) ) ? qtrue : qfalse, 0, nullptr );',
                      'Sys_QueEvent( now, SE_CHAR, static_cast<unsigned char>( *c ), 0, 0, nullptr );',
                      'Sys_QueEvent( now, SE_MOUSE, atoi( Cmd_Argv( 2 ) ), atoi( Cmd_Argv( 3 ) ), 0, nullptr );'):
            self.assertIn(event, command)
        # A console line is queued as typed text is: the queue frees it.
        self.assertIn('char *line = CopyString( Cmd_ArgsFrom( 2 ) );', command)
        self.assertIn('Sys_QueEvent( now, SE_CONSOLE, 0, 0, static_cast<int>( strlen( line ) + 1 ), line );', command)
        self.assertIn('if ( key <= 0 || key >= MAX_KEYS ) {', command)


class ActionScriptTests(unittest.TestCase):
    def test_point_waits_between_its_two_motions(self):
        probe = load_probe()
        phases = probe.compile_actions('point 596 258\n')
        self.assertEqual(phases, [['in_synthesize motion -100000 -100000', 'wait 200', 'in_synthesize motion 596 258']])

    def test_keys_and_buttons_end_their_phase(self):
        probe = load_probe()
        phases = probe.compile_actions('# a comment\nshot a\nclick 10 20\nwait 500\npress escape 90\ndown w\n'
                                       'motion 400 0\nup w\ntype hello there\nviewpos\n')
        self.assertEqual(phases[0], ['screenshotPNG a', 'in_synthesize motion -100000 -100000', 'wait 200',
                                     'in_synthesize motion 10 20', 'wait 150', 'in_synthesize key mouse1 1',
                                     'wait 80', 'in_synthesize key mouse1 0', 'wait 100',
                                     'in_synthesize console exec ritual-input-1.cfg'])
        self.assertEqual(phases[1], ['wait 500', 'in_synthesize key escape 1', 'wait 90', 'in_synthesize key escape 0',
                                     'wait 100', 'in_synthesize console exec ritual-input-2.cfg'])
        self.assertEqual(phases[2], ['in_synthesize key w 1', 'wait 100', 'in_synthesize console exec ritual-input-3.cfg'])
        self.assertEqual(phases[3], ['in_synthesize motion 400 0', 'in_synthesize key w 0', 'wait 100',
                                     'in_synthesize console exec ritual-input-4.cfg'])
        self.assertEqual(phases[4], ['in_synthesize char hello there', 'wait 100',
                                     'in_synthesize console exec ritual-input-5.cfg'])
        self.assertEqual(phases[5], ['viewpos'])

    def test_a_bad_line_is_refused(self):
        probe = load_probe()
        with self.assertRaises(SystemExit):
            probe.compile_actions('click 10\n')


if __name__ == '__main__':
    unittest.main()
