"""Enemy Territory reads a bot character's includes from the game root.

ET's BotLoadCharacterFromFile (etded.exe 0x45f5d0) sets the script folder to
botfiles only while it opens the file, and clears it before parsing, so ET's
bots/default_c.c includes "botfiles/chars.h". Quake III's keeps botfiles while
parsing, and its characters include "chars.h". Every other loader in both
engines keeps botfiles while it parses: ET's item and weapon weights include
"inv.h" beside them.

The engine's own character and weight loaders run here over synthetic trees
under each profile's rules, and every path they ask the filesystem for is
recorded. Mutations of the production loader must each fail the checks. The
profile table must start Quake III's four configured subsystems for ET, as
etded.exe's Export_BotLibSetup (0x436ed0) does, and change no other profile.
"""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest
from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]
HOST = ROOT / 'tests/fixtures/bot_character_host.cpp'
CHARACTER = 'code/botlib/be_ai_char.cpp'
SOURCES = ('code/botlib/be_ai_weight.cpp', 'code/botlib/l_precomp.cpp', 'code/botlib/l_script.cpp',
           'code/botlib/l_memory.cpp', 'code/botlib/l_libvar.cpp')
ET_CLEAR = 'if (Com_GameIsWolfET()) PC_SetBaseFolder("");'
OPEN = 'PC_SetBaseFolder(BOTFILESBASEFOLDER);\n\tsource = LoadSourceFile(charfile);'

CHARS = ('#define CHARACTERISTIC_NAME 0\n#define CHARACTERISTIC_ATTACK_SKILL 2\n'
         '#define CHARACTERISTIC_JUMPER 3\n#define CHARACTERISTIC_ITEMWEIGHTS 40\n')


def character(include):
    """A two-skill character whose characteristic names come from chars.h."""
    blocks = ''.join(f'skill {skill}\n{{\n\tCHARACTERISTIC_NAME "Tester"\n'
                     f'\tCHARACTERISTIC_ATTACK_SKILL {aim}\n\tCHARACTERISTIC_JUMPER {jump}\n'
                     f'\tCHARACTERISTIC_ITEMWEIGHTS "bots/default_i.c"\n}}\n'
                     for skill, aim, jump in ((1, '0.25', 1), (4, '0.75', 4)))
    return f'#include "{include}"\n{blocks}'


# Each layout names the include its character uses; the weight files are
# the same in both, and include from botfiles as ET's and Quake III's do.
LAYOUTS = {'et': 'botfiles/chars.h', 'quake': 'chars.h'}
FILES = {
    'botfiles/chars.h': CHARS,
    'botfiles/inv.h': '#define W_HEALTH 25\n',
    'botfiles/fw_items.c': 'weight "item_health"\n{\n\treturn W_HEALTH;\n}\n',
    'botfiles/bots/default_i.c': '#include "inv.h"\n#include "fw_items.c"\n',
}
LOADED = ['string 0 Tester', 'string 40 bots/default_i.c', 'float 2 0.75', 'integer 3 4']
WEIGHTS = ['open botfiles/bots/default_i.c found', 'open botfiles/inv.h found',
           'open botfiles/fw_items.c found', 'weights 1', 'weight item_health']


class BotCharacterIncludeTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.compiler = shutil.which('clang++') or shutil.which('g++')
        if not cls.compiler:
            raise unittest.SkipTest('C++17 compiler unavailable')
        scratch = ROOT / '.tmp/wolfet-bot-character-tests'
        scratch.mkdir(parents=True, exist_ok=True)
        cls.folder = Path(tempfile.mkdtemp(dir=scratch))
        target = compiler_target_args(cls.compiler)
        if os.name == 'nt' and not target:
            target = ['-DFNTECH3_Q3_ONLY']
        elif os.name != 'nt':
            target = ['-m32']
        # The loader's own headers resolve through the include path, so a
        # mutated copy compiles from the scratch folder like the original.
        cls.flags = [*target, '-std=c++17', '-DBOTLIB', '-D_CRT_SECURE_NO_WARNINGS',
                     '-I', str(ROOT), '-I', str(ROOT / 'code/botlib')]
        cls.objects = [cls.compile(ROOT / name, f'common{index}') for index, name in
                       enumerate((HOST.relative_to(ROOT), *SOURCES))]
        cls.trees = {}
        for layout, include in LAYOUTS.items():
            root = cls.folder / layout
            for path, text in {**FILES, 'botfiles/bots/default_c.c': character(include)}.items():
                (root / path).parent.mkdir(parents=True, exist_ok=True)
                (root / path).write_text(text, encoding='ascii')
            cls.trees[layout] = root
        cls.host = cls.link((ROOT / CHARACTER).read_text(encoding='utf-8'), 'production')

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.folder, ignore_errors=True)

    @classmethod
    def compile(cls, source, name):
        output = cls.folder / f'{name}.o'
        built = subprocess.run([cls.compiler, *cls.flags, '-c', str(source), '-o', str(output)],
                               cwd=ROOT, capture_output=True, text=True, timeout=300)
        if built.returncode:
            raise AssertionError(built.stdout + built.stderr)
        return output

    @classmethod
    def link(cls, text, name):
        source = cls.folder / f'{name}.cpp'
        source.write_text(text, encoding='utf-8')
        executable = cls.folder / (f'{name}.exe' if os.name == 'nt' else name)
        built = subprocess.run([cls.compiler, *cls.flags, str(source), *map(str, cls.objects),
                                '-o', str(executable)], cwd=ROOT, capture_output=True, text=True, timeout=300)
        if built.returncode:
            raise AssertionError(built.stdout + built.stderr)
        return executable

    def run_host(self, host, layout, wolfet, skill='4', weights=True):
        arguments = [str(host), str(self.trees[layout]), str(wolfet), 'bots/default_c.c', skill]
        run = subprocess.run(arguments + (['bots/default_i.c'] if weights else []),
                             capture_output=True, text=True, timeout=30)
        self.assertEqual(run.returncode, 0, run.stderr)
        return run.stdout.replace('\r', '').splitlines(), run.stderr.replace('\\', '/')

    def failures(self, host):
        """Each expectation the host breaks, as text; empty when all hold."""
        found = []

        def expect(condition, text):
            if not condition:
                found.append(text)

        # ET's character includes from the root; the file opens below botfiles.
        lines, _ = self.run_host(host, 'et', 1)
        expect(lines[:2] == ['open botfiles/bots/default_c.c found', 'open botfiles/chars.h found'],
               f'ET rules did not read botfiles/chars.h from the root: {lines[:2]}')
        expect(all(line in lines for line in LOADED) and 'character 0' not in lines,
               f'ET rules did not load the ET character: {lines}')
        expect(lines[-len(WEIGHTS):] == WEIGHTS, f'ET weights after the character: {lines[-len(WEIGHTS):]}')
        # Quake III's rules look for it below botfiles, as fnTech3 did.
        lines, errors = self.run_host(host, 'et', 0)
        expect('character 0' in lines, 'Quake III rules loaded the ET character')
        expect('open botfiles/botfiles/chars.h missing' in lines and 'open botfiles/chars.h found' not in lines,
               f'Quake III rules looked elsewhere for the ET include: {lines}')
        expect('file botfiles/chars.h not found' in errors and "couldn't load any skill" in errors,
               f'Quake III rules reported the ET include differently: {errors}')
        expect(lines[-len(WEIGHTS):] == WEIGHTS, 'Quake III weights after the failed ET character')
        # A Quake III character still loads by Quake III's rules...
        lines, _ = self.run_host(host, 'quake', 0)
        expect(lines[:2] == ['open botfiles/bots/default_c.c found', 'open botfiles/chars.h found'],
               f'Quake III rules did not read chars.h beside the character: {lines[:2]}')
        expect(all(line in lines for line in LOADED), f'Quake III rules did not load its character: {lines}')
        # ...and by ET's rules looks for chars.h at the root, as ET would.
        lines, _ = self.run_host(host, 'quake', 1)
        expect('character 0' in lines and 'open chars.h missing' in lines
               and 'open botfiles/chars.h found' not in lines,
               f'ET rules did not look for a Quake III include at the root: {lines}')
        # Skill 2 interpolates skills 1 and 4, each parsed by ET's rules.
        lines, _ = self.run_host(host, 'et', 1, skill='2', weights=False)
        aim = next((line for line in lines if line.startswith('float 2 ')), 'float 2 missing')
        expect(aim.split()[-1] != 'missing' and abs(float(aim.split()[-1]) - (0.25 + 0.5 / 3)) < 1e-4,
               f'ET skill 2 was not interpolated from skills 1 and 4: {lines}')
        return found

    def test_each_profile_reads_its_own_includes(self):
        self.assertEqual(self.failures(self.host), [])

    def test_mutations_of_the_loader_fail(self):
        production = (ROOT / CHARACTER).read_text(encoding='utf-8')
        self.assertEqual(production.count(ET_CLEAR), 1)
        self.assertEqual(production.count(OPEN), 1)
        mutations = {
            'no ET clear': production.replace(ET_CLEAR, ''),
            'a clear in every profile': production.replace(ET_CLEAR, 'PC_SetBaseFolder("");'),
            "Quake III's rules for ET only": production.replace(ET_CLEAR, 'if (!Com_GameIsWolfET()) PC_SetBaseFolder("");'),
            'ET opening from the root': production.replace(
                OPEN, 'PC_SetBaseFolder(Com_GameIsWolfET() ? "" : BOTFILESBASEFOLDER);\n\tsource = LoadSourceFile(charfile);'),
        }
        for index, (name, text) in enumerate(mutations.items()):
            with self.subTest(mutation=name):
                self.assertNotEqual(text, production)
                self.assertNotEqual(self.failures(self.link(text, f'mutation{index}')), [])


PROFILES = r'''
#include "code/qcommon/game_profile.h"
#include <cstdio>
int main() {
    const struct { comGameId_t id; unsigned systems; } expected[] = {
        { COM_GAME_QUAKE3, COM_BOT_AI_QUAKE }, { COM_GAME_QUAKELIVE, COM_BOT_AI_QUAKE },
        { COM_GAME_RTCW_SP, COM_BOT_AI_MOVEMENT }, { COM_GAME_RTCW_MP, 0 },
        { COM_GAME_WOLFET, COM_BOT_AI_QUAKE },
        { COM_GAME_STVEF_SP, 0 }, { COM_GAME_STVEF_MP, COM_BOT_AI_QUAKE },
    };
    int failures = 0;
    for ( const auto &row : expected ) {
        const comGameProfile_t *profile = Com_GameProfileById( row.id );
        if ( !profile || profile->bots.aiSystems != row.systems ) {
            std::printf( "profile %u starts %u, not %u\n", (unsigned)row.id,
                profile ? profile->bots.aiSystems : 0u, row.systems );
            ++failures;
        }
    }
    return failures;
}
'''


class BotSubsystemPolicyTests(unittest.TestCase):
    def test_et_starts_quake_iii_configured_subsystems(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++17 compiler unavailable')
        scratch = ROOT / '.tmp/wolfet-bot-character-tests'
        scratch.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=scratch) as temporary:
            folder = Path(temporary)
            source = folder / 'profiles.cpp'
            source.write_text(PROFILES, encoding='utf-8')
            executable = folder / ('profiles.exe' if os.name == 'nt' else 'profiles')
            built = subprocess.run([compiler, *compiler_target_args(compiler), '-std=c++17', '-I', str(ROOT),
                                    str(source), str(ROOT / 'code/qcommon/game_profile.cpp'), '-o', str(executable)],
                                   cwd=ROOT, capture_output=True, text=True, timeout=120)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            run = subprocess.run([str(executable)], capture_output=True, text=True, timeout=30)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)


if __name__ == '__main__':
    unittest.main()
