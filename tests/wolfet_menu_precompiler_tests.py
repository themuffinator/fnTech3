"""The script precompiler: ET's macro arguments, and skipped branches staying skipped."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest
from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]
HOST = ROOT/'tests/fixtures/precompiler_host.cpp'
SOURCES = ('code/botlib/l_precomp.cpp', 'code/botlib/l_script.cpp', 'code/botlib/l_memory.cpp')

# Each case: files to write, the file to read, the ET flag, defines, and the
# exact token stream the precompiler must produce.
CASES = [
    # A nested pair inside an argument, and a macro named inside one. ET's own
    # menus do both; Quake 3's reading ends the call at the nested ")".
    ('nested', {
        'a.menu': '#define W 100\n'
                  '#define B( X, T, A ) [ X T A ]\n'
                  'B( W-6, "go", open ( "x" ) ( "y" ) )\n'
                  'end\n',
    }, 'a.menu', 1, [], ['[', '100', '-', '6', 'go', 'open', '(', 'x', ')', '(', 'y', ')', ']', 'end']),
    # The same call read by Quake 3's rules is left as it was: nested "("
    # dropped and the call ended early, with a warning.
    ('quake', {
        'a.menu': '#define B( X, A ) [ X A ]\n'
                  'B( 1, 2 )\n'
                  'end\n',
    }, 'a.menu', 0, [], ['[', '1', '2', ']', 'end']),
    # A string inside a skipped branch must never be returned, whichever rules
    # read it: the look-ahead for an adjacent string used to cross the #else.
    ('skipped-else', {
        'a.menu': '#define M( A, B ) [ A B ]\n'
                  '#ifdef X\nM( 1, "one" )\n#else\nM( 2, "two" )\n#endif\nend\n',
    }, 'a.menu', 1, [], ['[', '2', 'two', ']', 'end']),
    ('skipped-if', {
        'a.menu': '#define M( A, B ) [ A B ]\n'
                  '#ifdef X\nM( 1, "one" )\n#else\nM( 2, "two" )\n#endif\nend\n',
    }, 'a.menu', 1, ['X'], ['[', '1', 'one', ']', 'end']),
    ('skipped-quake', {
        'a.menu': '#ifdef X\n"skipped"\nplain\n#endif\nend\n',
    }, 'a.menu', 0, [], ['end']),
    # Adjacent strings outside any skipped branch still join.
    ('join', {
        'a.menu': '"ab" "cd"\nend\n',
    }, 'a.menu', 0, [], ['abcd', 'end']),
]


class MenuPrecompilerTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            raise unittest.SkipTest('C++17 compiler unavailable')
        scratch = ROOT/'.tmp/menu-precompiler-tests'
        scratch.mkdir(parents=True, exist_ok=True)
        cls.folder = Path(tempfile.mkdtemp(dir=scratch))
        cls.host = cls.folder/('host.exe' if os.name == 'nt' else 'host')
        # Aligned with the active MSVC library environment, as every other
        # standalone probe is; the engine's 32-bit guard is satisfied either by
        # that target or, without one, by the Quake 3 only switch.
        target = compiler_target_args(compiler)
        if os.name == 'nt' and not target:
            target = ['-DFNTECH3_Q3_ONLY']
        elif os.name != 'nt':
            target = ['-m32']
        built = subprocess.run([compiler, *target, '-std=c++17', '-DBOTLIB',
            '-I', str(ROOT), str(HOST), *[str(ROOT/s) for s in SOURCES],
            '-o', str(cls.host)], cwd=ROOT, capture_output=True, text=True, timeout=300)
        if built.returncode:
            raise AssertionError(built.stdout + built.stderr)

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.folder, ignore_errors=True)

    def test_token_streams(self):
        for name, files, entry, wolfet, defines, expected in CASES:
            with self.subTest(case=name):
                root = self.folder/name
                root.mkdir(exist_ok=True)
                for path, text in files.items():
                    (root/path).write_text(text, encoding='utf-8')
                run = subprocess.run([str(self.host), str(root), str(wolfet), '1', entry, *defines],
                                     capture_output=True, text=True, timeout=30)
                tokens = [line for line in run.stdout.replace('\r', '').split('\n') if line]
                self.assertEqual(tokens, expected, run.stderr)

    def test_the_two_readings_differ_only_where_et_needs_them_to(self):
        """A nested pair in an argument reads differently under the two rules;
        a call without one reads the same."""
        root = self.folder/'differ'
        root.mkdir(exist_ok=True)
        (root/'nested.menu').write_text('#define B( X, A ) [ X A ]\nB( 1, open ( "x" ) )\nend\n',
                                        encoding='utf-8')
        (root/'plain.menu').write_text('#define B( X, A ) [ X A ]\nB( 1, 2 )\nend\n', encoding='utf-8')
        streams = {}
        for entry in ('nested.menu', 'plain.menu'):
            for wolfet in ('0', '1'):
                run = subprocess.run([str(self.host), str(root), wolfet, '1', entry],
                                     capture_output=True, text=True, timeout=30)
                streams[entry, wolfet] = run.stdout.replace('\r', '').split()
        self.assertNotEqual(streams['nested.menu', '0'], streams['nested.menu', '1'])
        self.assertEqual(streams['nested.menu', '1'], ['[', '1', 'open', '(', 'x', ')', ']', 'end'])
        self.assertEqual(streams['plain.menu', '0'], streams['plain.menu', '1'])

    def test_et_rules_are_asked_for_by_et_alone(self):
        for client in ('code/client/cl_ui.cpp', 'code/client/cl_cgame.cpp'):
            text = (ROOT/client).read_text(encoding='utf-8')
            self.assertIn('Com_GameIsWolfET() ? botlib_export->PC_LoadWolfETSourceHandle', text, client)
        precomp = (ROOT/'code/botlib/l_precomp.cpp').read_text(encoding='utf-8')
        # The skip check sits before the adjacent-string look-ahead.
        self.assertLess(precomp.index('if (source->skip) continue;'),
                        precomp.index('// recursively concatenate strings'))


if __name__ == '__main__':
    unittest.main()
