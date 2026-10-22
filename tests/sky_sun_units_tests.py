"""How a map's compiler stored a sky sun, shared by the raster sun shadows and
RTX's path tracer (code/renderercommon/tr_sky_sun_units.h).

id's q3map and Raven's stored q3map_sun's colour at unit length; q3map2, and
Return to Castle Wolfenstein's own compiler, kept its brightest channel at 1,
as the shadow edges in the installed maps' lightmaps show
(docs/fntech3/2026-10-22-fnq-shadow-formats.md, "The sun's units"). A
q3map2 map is known by worldspawn keys only q3map2 writes or reads.

Here:

* the rule and the worldspawn scan, compiled with the production string
  compares, on every profile, q3map2's keys, keys elsewhere, case, long and
  malformed text;
* the scan's keys are RTX's light parser's (RT_ParseLightEntity);
* where they are used: the raster's sun units (R_CSMSunLightmapColor), the
  entity-sun loader that sets them, and RT_FindSun;
* when legitimate installations are present, every installed Quake III,
  Quake Live, Wolfenstein and Enemy Territory map's worldspawn through the
  production scan. Set FNTECH3_Q3_ROOT, FNTECH3_QL_ROOT, FNTECH3_RTCW_ROOT and
  FNTECH3_WOLFET_ROOT to point at other copies; map bytes are piped to the
  host and nothing retail is written.
"""
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess
import tempfile
import unittest
import zipfile

from compiler_support import engine_fixture_args

ROOT = Path(__file__).resolve().parents[1]
RENDERERS = ('renderer', 'renderervk', 'rendererrtx')
INSTALLS = {
    'COM_GAME_QUAKE3': Path(os.environ.get('FNTECH3_Q3_ROOT',
                                           r'C:\Program Files (x86)\Steam\steamapps\common\Quake 3 Arena')) / 'baseq3',
    'COM_GAME_QUAKELIVE': Path(os.environ.get('FNTECH3_QL_ROOT',
                                              r'E:\SteamLibrary\steamapps\common\Quake Live')) / 'baseq3',
    'COM_GAME_RTCW_SP': Path(os.environ.get('FNTECH3_RTCW_ROOT',
                                            r'E:\SteamLibrary\steamapps\common\Return to Castle Wolfenstein')) / 'Main',
    'COM_GAME_WOLFET': Path(os.environ.get('FNTECH3_WOLFET_ROOT',
                                           r'E:\SteamLibrary\steamapps\common\Wolfenstein Enemy Territory')) / 'etmain',
}


def read(path):
    return (ROOT / path).read_text(encoding='utf-8')


def function(text, signature):
    start = text.index(signature)
    while ';' in text[start:text.index('{', start)]:
        start = text.index(signature, start + len(signature))
    return text[start:text.index('\n}\n', start) + 3]


def prelude():
    shared = read('code/qcommon/q_shared.c')
    return ('#include <cassert>\n#include <cstdio>\n#include <cstring>\n#include <string>\n#include <vector>\n'
            '#include "code/qcommon/q_shared.h"\n#include "code/qcommon/game_profile.h"\n'
            + function(shared, 'int Q_stricmpn( const char *s1, const char *s2, int n ) {')
            + function(shared, 'int Q_stricmp( const char *s1, const char *s2 )')
            + read('code/renderercommon/tr_sky_sun_units.h'))


def compile_and_run(test, source, stdin=None):
    compiler = shutil.which('clang++') or shutil.which('g++')
    if not compiler:
        test.skipTest('C++17 compiler required')
    (ROOT / '.tmp').mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='sky-sun-units-', dir=ROOT / '.tmp') as temp:
        cpp, exe = Path(temp) / 'probe.cpp', Path(temp) / 'probe.exe'
        cpp.write_text(source, encoding='utf-8')
        built = subprocess.run([compiler, *engine_fixture_args(compiler), '-std=c++17', '-O2', '-D_CRT_SECURE_NO_WARNINGS', '-I', str(ROOT),
                                str(cpp), '-o', str(exe)], cwd=ROOT, capture_output=True, text=True, timeout=120)
        test.assertEqual(built.returncode, 0, built.stdout + built.stderr)
        run = subprocess.run([str(exe)], cwd=ROOT, input=stdin, capture_output=True, timeout=300)
        test.assertEqual(run.returncode, 0, (run.stdout + run.stderr).decode('utf-8', 'replace'))
        return run.stdout.decode('latin-1')


CHECKS = r'''
static qboolean Scan( const char *text, int game ) { return R_SkySunEntitiesQ3map2( text, game ); }
int main() {
    /* the rule: id's and Raven's at unit length, q3map2 never */
    const int unit[] = { COM_GAME_AUTO, COM_GAME_QUAKE3, COM_GAME_QUAKE3_IHV, COM_GAME_Q3TEST, COM_GAME_STVEF_SP,
                         COM_GAME_STVEF_MP, COM_GAME_JKA_SP, COM_GAME_JKA_MP, COM_GAME_JK2_SP, COM_GAME_JK2_MP,
                         COM_GAME_ALICE, COM_GAME_FAKK2, COM_GAME_MOHAA };
    for ( int game : unit ) {
        assert( R_SkySunUnitLengthColour( game, qfalse ) );
        assert( !R_SkySunUnitLengthColour( game, qtrue ) );
    }
    const int brightest[] = { COM_GAME_QUAKELIVE, COM_GAME_RTCW_SP, COM_GAME_RTCW_MP, COM_GAME_WOLFET };
    for ( int game : brightest ) {
        assert( !R_SkySunUnitLengthColour( game, qfalse ) && !R_SkySunUnitLengthColour( game, qtrue ) );
    }

    /* the scan: id's own worldspawn has none of q3map2's keys */
    assert( !Scan( "{\n\"classname\" \"worldspawn\"\n\"message\" \"Arena Gate\"\n\"music\" \"music/sonic2.wav\"\n}\n"
                   "{\n\"classname\" \"info_player_deathmatch\"\n}\n", COM_GAME_QUAKE3 ) );
    /* q3map2's version, command line and the keys only it reads, any case */
    assert( Scan( "{\n\"classname\" \"worldspawn\"\n\"_q3map2_version\" \"2.5.17n\"\n}\n", COM_GAME_QUAKE3 ) );
    assert( Scan( "{\n\"_q3map2_cmdline\" \"-light -fast\"\n\"classname\" \"worldspawn\"\n}\n", COM_GAME_JKA_MP ) );
    const char *only[] = { "_ambient", "_minlight", "_minvertexlight", "_mingridlight", "_noshadersun",
                           "_lightmapscale", "_ls", "_blocksize", "_celshader", "_foghull", "_keepLights",
                           "_floodlight", "_ignoreleaks", "_MINVERTEXLIGHT", "_Q3MAP2_VERSION" };
    for ( const char *key : only ) {
        std::string text = std::string( "{ \"classname\" \"worldspawn\" \"" ) + key + "\" \"1\" }";
        assert( Scan( text.c_str(), COM_GAME_QUAKE3 ) );
    }
    /* id's own keys and lookalikes are not q3map2's */
    const char *not_only[] = { "ambient", "_color", "light", "gridsize", "_q3map2", "_lightmap", "_minlight2", "" };
    for ( const char *key : not_only ) {
        std::string text = std::string( "{ \"classname\" \"worldspawn\" \"" ) + key + "\" \"1\" }";
        assert( !Scan( text.c_str(), COM_GAME_QUAKE3 ) );
    }
    /* _farplanedist: q3map2's vis, but Wolfenstein's tools read it too */
    assert( Scan( "{ \"_farplanedist\" \"4096\" }", COM_GAME_QUAKE3 ) );
    assert( !Scan( "{ \"_farplanedist\" \"4096\" }", COM_GAME_RTCW_MP ) );
    assert( !Scan( "{ \"_farplanedist\" \"4096\" }", COM_GAME_WOLFET ) );
    /* only the worldspawn counts */
    assert( !Scan( "{ \"classname\" \"worldspawn\" }\n{ \"_minlight\" \"5\" }", COM_GAME_QUAKE3 ) );
    /* a value is no key */
    assert( !Scan( "{ \"message\" \"_q3map2_version\" }", COM_GAME_QUAKE3 ) );
    /* nothing, no entity, cut-off or malformed text */
    assert( !Scan( nullptr, COM_GAME_QUAKE3 ) && !Scan( "", COM_GAME_QUAKE3 ) && !Scan( "   ", COM_GAME_QUAKE3 ) );
    assert( !Scan( "{", COM_GAME_QUAKE3 ) && !Scan( "{ \"_minlight", COM_GAME_QUAKE3 ) );
    assert( !Scan( "{ \"_minlight\"", COM_GAME_QUAKE3 ) && !Scan( "{ \"_minlight\" \"5", COM_GAME_QUAKE3 ) );
    assert( !Scan( "{ _minlight 5 }", COM_GAME_QUAKE3 ) );
    assert( Scan( "  \r\n\t{\"_minlight\"\"5\"}", COM_GAME_QUAKE3 ) );
    /* a key longer than the buffer is cut, not overrun */
    std::string longKey = "{ \"" + std::string( 4096, 'k' ) + "\" \"1\" \"_minlight\" \"2\" }";
    assert( Scan( longKey.c_str(), COM_GAME_QUAKE3 ) );
    std::string cut = "{ \"_q3map2_" + std::string( 200, 'x' ) + "\" \"1\" }";
    assert( Scan( cut.c_str(), COM_GAME_QUAKE3 ) );
    puts( "ok" );
    return 0;
}
'''

CENSUS = r'''
int main() {
    /* records: game id, length, entity text */
    int game, length;
    while ( std::scanf( "%d %d", &game, &length ) == 2 ) {
        std::getchar();
        std::vector<char> text( (size_t)length + 1, 0 );
        if ( length && std::fread( text.data(), 1, (size_t)length, stdin ) != (size_t)length ) return 2;
        const qboolean q3map2 = R_SkySunEntitiesQ3map2( text.data(), game );
        std::printf( "%d %d\n", q3map2 ? 1 : 0, R_SkySunUnitLengthColour( game, q3map2 ) ? 1 : 0 );
    }
    return 0;
}
'''


def worldspawn_records(root):
    for pk3 in sorted(root.glob('*.pk3')):
        try:
            archive = zipfile.ZipFile(pk3)
        except zipfile.BadZipFile:
            continue
        with archive:
            for name in archive.namelist():
                if name.lower().startswith('maps/') and name.lower().endswith('.bsp'):
                    data = archive.read(name)
                    if data[:4] not in (b'IBSP', b'RBSP') or len(data) < 16:
                        continue
                    offset, length = struct.unpack_from('<ii', data, 8)
                    yield pk3.name, name, data[offset:offset + length].split(b'\0', 1)[0]


class SkySunUnitsTests(unittest.TestCase):
    def test_the_rule_and_the_scan(self):
        self.assertIn('ok', compile_and_run(self, prelude() + CHECKS))

    def test_the_scan_reads_rtx_parsers_keys(self):
        header = read('code/renderercommon/tr_sky_sun_units.h')
        keys = set(re.findall(r'"(_[A-Za-z0-9_]+)"', header))
        lights = read('code/rendererrtx/rt/rt_lights.c')
        parser = lights[lights.index('!Q_stricmp( key, "_ambient" )'):]
        parser = parser[:parser.index('!Q_stricmp( key, "radius" )')]
        rtx = set()
        for branch in re.split(r'\} else if', parser):
            if 'ent.q3map2 = qtrue' in branch:
                rtx |= set(re.findall(r'Q_stricmpn?\( key, "(_[A-Za-z0-9_]+)"', branch))
        self.assertEqual(rtx, keys)
        self.assertIn('ri.gameId != COM_GAME_RTCW_SP && ri.gameId != COM_GAME_RTCW_MP', parser)

    def test_where_the_units_are_used(self):
        sun = read('code/renderercommon/tr_entity_sun.h')
        self.assertIn('#include "tr_sky_sun_units.h"', sun)
        load = function(sun, 'static void R_LoadEntitySun( void ) {')
        self.assertLess(load.index('tr.worldSun.unitColour = R_SkySunUnitLengthColour( ri.gameId,'),
                        load.index('if ( COM_GAME_ID_IS_RITUAL( ri.gameId ) ) {'))
        self.assertIn('R_SkySunEntitiesQ3map2( s_worldData.entityString, ri.gameId ) );', load)
        for renderer in RENDERERS:
            with self.subTest(renderer=renderer):
                self.assertIn('qboolean\tunitColour;', read(f'code/{renderer}/tr_local.h'))
                main = read(f'code/{renderer}/tr_main.c')
                units = function(main, 'static void R_CSMSunLightmapColor( vec3_t out )')
                self.assertIn('if ( tr.worldSun.unitColour ) {', units)
                self.assertNotIn('R_CSMSkySunUnitColour', main)
        vk = read('code/rendererrtx/vk.c')
        self.assertLess(vk.index('#include "../renderercommon/tr_sky_sun_units.h"'), vk.index('#include "rt/rt_lights.c"'))
        find = function(read('code/rendererrtx/rt/rt_lights.c'), 'static void RT_FindSun( void )')
        self.assertIn('if ( R_SkySunUnitLengthColour( ri.gameId, s_rtLights.q3map2 ) ) {', find)
        self.assertNotIn('if ( !s_rtLights.q3map2 ) {\n\t\t\tlength', find)

    def test_installed_maps(self):
        records, keys = [], []
        for game, root in INSTALLS.items():
            if root.is_dir():
                for pk3, name, text in worldspawn_records(root):
                    records.append(b'%d %d\n' % (GAME_IDS[game], len(text)) + text)
                    keys.append((game, pk3, name))
        if not records:
            self.skipTest('installation not present')
        output = compile_and_run(self, prelude() + CENSUS, stdin=b''.join(records)).split('\n')
        result = {}
        for (game, pk3, name), line in zip(keys, output):
            q3map2, unit = (int(x) for x in line.split())
            result.setdefault(game, []).append((pk3, name, q3map2, unit))
        # Wolfenstein's maps carry no q3map2 key and keep a brightest channel
        # of 1; Enemy Territory's all carry them; Quake Live's nearly all
        for game, rows in result.items():
            with self.subTest(game=game):
                q3map2 = sum(r[2] for r in rows)
                if game == 'COM_GAME_RTCW_SP':
                    self.assertEqual(q3map2, 0)
                elif game == 'COM_GAME_WOLFET':
                    self.assertEqual(q3map2, len(rows))
                elif game == 'COM_GAME_QUAKELIVE':
                    self.assertGreaterEqual(q3map2, 0.9 * len(rows))
                if game == 'COM_GAME_QUAKE3':
                    # id's own maps, in the retail paks, at unit length
                    retail = [r for r in rows if re.fullmatch(r'pak[0-8]\.pk3', r[0].lower())]
                    self.assertTrue(all(r[3] == 1 and r[2] == 0 for r in retail))
                else:
                    self.assertTrue(all(r[3] == 0 for r in rows))


def _game_ids():
    text = read('code/qcommon/game_profile.h')
    body = text[text.index('COM_GAME_AUTO = 0,'):text.index('COM_GAME_COUNT')]
    names = re.findall(r'^\s*(COM_GAME_[A-Z0-9_]+)', body, re.M)
    return {name: index for index, name in enumerate(names)}


GAME_IDS = _game_ids()

if __name__ == '__main__':
    unittest.main()
