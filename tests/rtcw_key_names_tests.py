"""RTCW modules get key names the way the original engines give them."""
from pathlib import Path
import os
import re
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]

CHECKS = r'''
#include "code/compat/rtcw_key_names.hpp"
#include <cassert>
#include <climits>
#include <cstring>
#include <initializer_list>
using fntech3::rtcw::keys::NameForModule;
static bool Is(int key, int language, bool mp, const char *expected) {
    char tiny[5];
    return !std::strcmp(NameForModule(key, language, mp, tiny), expected);
}
int main() {
    // Range and sentinel strings.
    assert(Is(-1, 0, true, "<KEY NOT FOUND>") && Is(-2, 1, false, "<OUT OF RANGE>"));
    assert(Is(256, 0, true, "<OUT OF RANGE>"));
    // Printable characters are themselves; the translated path keeps ';' too.
    assert(Is('a', 1, true, "a") && Is(';', 0, true, ";") && Is(';', 2, false, ";"));
    // '"' and space fall through to the table ("SPACE") or to hex.
    assert(Is(' ', 0, true, "SPACE") && Is(' ', 1, true, "ESPACE"));
    assert(Is('"', 0, true, "0x22"));
    // cl_language 1..4 pick French, German, Italian, Spanish; others English.
    assert(Is(FNTECH3_RTCW_K_ENTER, 0, true, "ENTER") && Is(FNTECH3_RTCW_K_ENTER, 1, true, "ENTREE"));
    assert(Is(FNTECH3_RTCW_K_MOUSE1, 1, true, "SOURIS1") && Is(FNTECH3_RTCW_K_MOUSE1, 2, true, "MAUS1"));
    assert(Is(FNTECH3_RTCW_K_BACKSPACE, 4, true, "RETROCESO") && Is(FNTECH3_RTCW_K_BACKSPACE, 3, true, "BACKSPACE"));
    assert(Is(FNTECH3_RTCW_K_BACKSPACE, 2, false, "R\xDC" "CKTASTE"));
    for (int language : {-1, 5, 6, INT_MIN, INT_MAX}) assert(Is(FNTECH3_RTCW_K_ENTER, language, true, "ENTER"));
    // The one difference between the modes: retail MP's German Shift.
    assert(Is(FNTECH3_RTCW_K_SHIFT, 2, false, "UMSCHALT") && Is(FNTECH3_RTCW_K_SHIFT, 2, true, "UMSCHALLT"));
    for (int language : {0, 1, 3, 4}) {
        char a[5], b[5];
        assert(!std::strcmp(NameForModule(FNTECH3_RTCW_K_SHIFT, language, false, a),
                            NameForModule(FNTECH3_RTCW_K_SHIFT, language, true, b)));
    }
    // A key number with no name is two lowercase hex digits.
    assert(Is(0, 0, true, "0x00") && Is(255, 1, true, "0xff") && Is(31, 0, false, "0x1f"));
}
'''


class RTCWKeyNameTests(unittest.TestCase):
    def test_translated_module_names(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++17 compiler required')
        target = compiler_target_args(compiler)
        if os.name == 'nt' and not target:
            target = ['-DFNTECH3_Q3_ONLY']
        (ROOT / '.tmp').mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='rtcw-key-names-', dir=ROOT / '.tmp') as folder:
            cpp = Path(folder) / 'names.cpp'
            binary = Path(folder) / ('names.exe' if os.name == 'nt' else 'names')
            cpp.write_text(CHECKS, encoding='utf-8')
            built = subprocess.run([compiler, *target, '-std=c++17', '-Wall', '-Wextra', '-I', str(ROOT),
                                    str(cpp), '-o', str(binary)], capture_output=True, text=True, timeout=60)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            run = subprocess.run([str(binary)], capture_output=True, text=True, timeout=15)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)

    def test_table_and_module_routing(self):
        table = (ROOT / 'code/compat/rtcw_keynames.def').read_text(encoding='ascii')
        rows = re.findall(r'^FNTECH3_RTCW_KEYNAME\((\w+), ', table, re.M)
        self.assertEqual(len(rows), 106)
        self.assertEqual(len(rows), len(set(rows)))
        keys = (ROOT / 'code/client/cl_keys.cpp').read_text(encoding='utf-8')
        start = keys.index('void CL_RTCWKeyString(')
        function = keys[start:keys.index('\n}', start)]
        self.assertIn('fntech3::rtcw::keys::NameForModule( key,', function)
        self.assertIn('Cvar_VariableIntegerValue( "cl_language" ), Com_GameIsRTCWMP() != 0', function)
        # Bindings still come from the engine's own binding table.
        self.assertIn('Key_GetBinding( FnTech3_RTCWKeyToInternal( key ) )', function)
        main = (ROOT / 'code/client/cl_main.cpp').read_text(encoding='utf-8')
        start = main.index('static void CL_AddRTCWCommands( void ) {')
        self.assertIn('Cvar_Get( "cl_language", "0", CVAR_ARCHIVE );', main[start:main.index('\n}', start)])


if __name__ == '__main__':
    unittest.main()
