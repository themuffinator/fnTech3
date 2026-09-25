"""Each profile scales the light grid's ambient by its own engine's r_ambientScale.

The grid lights every model its game does not light otherwise, and each retail
renderer multiplies the grid's ambient by r_ambientScale before its minimum
light (docs/fntech3/2026-09-25-ambient-scale.md):

- Quake III 1.32 registers "0.6", CVAR_CHEAT (quake3.exe 0x0045152a).
- American McGee's Alice and Heavy Metal: F.A.K.K.2 register "0.5", flags
  0x200 (alice.exe 0x004754ed, read at 0x004acb70; fakk2.exe 1.02 0x00461123,
  read at 0x0048f0b5).
- Quake Live registers "10", bounded 1 to 100, flags 0x81800 (PROTECTED,
  0x1000 and CLOUD), through its bounded Cvar_Get (quakelive_steam.exe
  0x00449013, bounds kept by 0x004cdd30 and Cvar_Set 0x004cd0a4), and offers it
  as the Video menu's "Ambient Light Scale". Here its bounds come from
  Cvar_CheckRange, since this engine's bit 0x1000 is CVAR_VM_CREATED.
- r_directedScale is "1", CVAR_CHEAT, in all of them.

Each renderer's registration block is compiled against a recording `ri` and
run for every profile, so the check reads what the engine registers, however
the expressions are spelled. The renderers name the Ritual profiles with the
model-light header's RITUAL_ML_PROFILE, compiled here as they see it, and
every profile COM_GAME_ID_IS_RITUAL names must take "0.5": Alice alone until
F.A.K.K.2's profile exists, then both.
"""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]
RENDERERS = ('renderer', 'renderervk', 'rendererrtx')

PROGRAM = r'''
#include "code/qcommon/q_shared.h"
#include "code/qcommon/game_profile.h"
#include <stdio.h>
#include <string.h>

/* The renderers' own name for the Ritual profiles, from the model-light header */
@RITUAL_PROFILE@
/* Every profile the tree names Ritual; before F.A.K.K.2's profile, Alice alone */
#ifndef COM_GAME_ID_IS_RITUAL
#define COM_GAME_ID_IS_RITUAL( id ) ( ( id ) == COM_GAME_ALICE )
#endif

typedef struct {
	char name[64];
	char value[64];
	int flags;
	int ranged;
	char mins[32];
	char maxs[32];
	int validator;
} registration_t;

static registration_t registrations[8];
static cvar_t cvars[8];
static int registrationCount;

static cvar_t *Probe_Cvar_Get( const char *name, const char *value, int flags ) {
	registration_t *r = &registrations[registrationCount];
	snprintf( r->name, sizeof( r->name ), "%s", name );
	snprintf( r->value, sizeof( r->value ), "%s", value );
	r->flags = flags;
	return &cvars[registrationCount++];
}

static void Probe_Cvar_CheckRange( cvar_t *cv, const char *minVal, const char *maxVal, cvarValidator_t type ) {
	registration_t *r = &registrations[cv - cvars];
	r->ranged++;
	snprintf( r->mins, sizeof( r->mins ), "%s", minVal ? minVal : "none" );
	snprintf( r->maxs, sizeof( r->maxs ), "%s", maxVal ? maxVal : "none" );
	r->validator = (int)type;
}

static void Probe_Cvar_SetDescription( cvar_t *cv, const char *description ) {
	(void)cv;
	(void)description;
}

static struct {
	comGameId_t gameId;
	cvar_t *(*Cvar_Get)( const char *name, const char *value, int flags );
	void (*Cvar_CheckRange)( cvar_t *cv, const char *minVal, const char *maxVal, cvarValidator_t type );
	void (*Cvar_SetDescription)( cvar_t *cv, const char *description );
} ri;

static cvar_t *r_ambientScale;
static cvar_t *r_directedScale;

static void Register( void ) {
@REGISTRATION@
}

int main( void ) {
	int game, i;
	ri.Cvar_Get = Probe_Cvar_Get;
	ri.Cvar_CheckRange = Probe_Cvar_CheckRange;
	ri.Cvar_SetDescription = Probe_Cvar_SetDescription;
	printf( "flags %d %d %d %d\n", CVAR_CHEAT, CVAR_PROTECTED | CVAR_CLOUD, (int)CV_FLOAT, (int)COM_GAME_QUAKE3 );
	for ( game = COM_GAME_QUAKE3; game < COM_GAME_COUNT; game++ ) {
		memset( registrations, 0, sizeof( registrations ) );
		memset( cvars, 0, sizeof( cvars ) );
		registrationCount = 0;
		ri.gameId = (comGameId_t)game;
		Register();
		if ( r_ambientScale != &cvars[0] || r_directedScale != &cvars[1] ) {
			printf( "pointers %d\n", game );
		}
		for ( i = 0; i < registrationCount; i++ ) {
			const registration_t *r = &registrations[i];
			printf( "cvar %d %d %d %s %s %d %d %s %s %d\n", game, game == COM_GAME_QUAKELIVE,
				COM_GAME_ID_IS_RITUAL( game ) ? 1 : 0, r->name, r->value, r->flags, r->ranged,
				r->ranged ? r->mins : "-", r->ranged ? r->maxs : "-", r->ranged ? r->validator : -1 );
		}
	}
	return 0;
}
'''


def read(relative):
    return (ROOT / relative).read_text(encoding='utf-8').replace('\r\n', '\n')


def registration_block(source):
    """From the r_ambientScale registration to the end of r_directedScale's description."""
    start_text = '\tr_ambientScale = ri.Cvar_Get( "r_ambientScale",'
    end_text = '\tri.Cvar_SetDescription( r_directedScale,'
    if source.count(start_text) != 1 or source.count(end_text) != 1:
        raise AssertionError('expected exactly one r_ambientScale and one r_directedScale registration')
    if source.count('"r_ambientScale"') != 1 or source.count('"r_directedScale"') != 1:
        raise AssertionError('a cvar is registered twice')
    start = source.index(start_text)
    end = source.index('\n', source.index(end_text, start)) + 1
    return source[start:end]


def ritual_profile_macro():
    """The #ifdef block defining RITUAL_ML_PROFILE, as the renderers see it."""
    source = read('code/renderercommon/tr_ritual_model_light.h')
    start = source.index('#ifdef COM_GAME_ID_IS_RITUAL\n#define RITUAL_ML_PROFILE')
    end = source.index('#endif\n', start) + len('#endif\n')
    return source[start:end]


def evaluate(renderer, folder):
    compiler = shutil.which('clang')
    if not compiler:
        raise unittest.SkipTest('Clang unavailable')
    program = (PROGRAM.replace('@RITUAL_PROFILE@', ritual_profile_macro())
               .replace('@REGISTRATION@', registration_block(read(f'code/{renderer}/tr_init.c'))))
    c = folder / f'{renderer}_ambient.c'
    exe = folder / f'{renderer}_ambient.exe'
    c.write_text(program, encoding='utf-8')
    command = [compiler, *compiler_target_args(compiler), '-std=c11', '-w', '-I', str(ROOT), str(c), '-o', str(exe)]
    built = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, timeout=120)
    if built.returncode != 0:
        raise AssertionError(built.stdout + built.stderr)
    run = subprocess.run([str(exe)], capture_output=True, text=True, timeout=30)
    if run.returncode != 0:
        raise AssertionError(run.stdout + run.stderr)
    lines = run.stdout.splitlines()
    flags = [int(value) for value in lines[0].split()[1:]]
    rows = {}
    for line in lines[1:]:
        fields = line.split()
        if fields[0] != 'cvar':
            raise AssertionError(line)
        game, ql, ritual = int(fields[1]), fields[2] == '1', fields[3] == '1'
        rows.setdefault(game, {'ql': ql, 'ritual': ritual, 'cvars': []})['cvars'].append(
            (fields[4], fields[5], int(fields[6]), int(fields[7]), fields[8], fields[9], int(fields[10])))
    return flags, rows


class AmbientScaleDefaultsTests(unittest.TestCase):
    def test_every_renderer_registers_each_engines_ambient_scale(self):
        (ROOT / '.tmp').mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=ROOT / '.tmp') as temp:
            for renderer in RENDERERS:
                with self.subTest(renderer=renderer):
                    (cheat, ql_flags, cv_float, quake3), rows = evaluate(renderer, Path(temp))
                    self.assertGreater(len(rows), 12)
                    self.assertEqual(sum(row['ql'] for row in rows.values()), 1)
                    self.assertGreaterEqual(sum(row['ritual'] for row in rows.values()), 1)
                    self.assertFalse(rows[quake3]['ql'] or rows[quake3]['ritual'])
                    for game, row in rows.items():
                        ambient, directed = row['cvars']
                        self.assertEqual(directed, ('r_directedScale', '1', cheat, 0, '-', '-', -1), f'profile {game}')
                        if row['ql']:
                            expected = ('r_ambientScale', '10', ql_flags, 1, '1', '100', cv_float)
                        elif row['ritual']:
                            expected = ('r_ambientScale', '0.5', cheat, 0, '-', '-', -1)
                        else:
                            # Quake III's value; several other engines differ
                            # (see the record), and keep it for now.
                            expected = ('r_ambientScale', '0.6', cheat, 0, '-', '-', -1)
                        self.assertEqual(ambient, expected, f'profile {game}')


if __name__ == '__main__':
    unittest.main()
