from __future__ import annotations

import os
import re
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

from compiler_support import compiler_target_args


ROOT = Path(__file__).resolve().parents[1]

# Both retail key tables, quake3.exe 1.32's 107 rows and quakelive_steam.exe's
# 111, name key 176 KP_STAR and leave 130 and 157-159 unnamed; the two differ
# only by Quake Live's four extra mouse buttons
# (docs/fntech3/2026-09-19-q3-key-numbers.md). This engine's printed table has
# to agree with them there, so a config either retail engine wrote loads here
# and a config this engine writes loads in both of them.
UNNAMED_BY_RETAIL = (("POWER", "K_POWER", 130), ("F13", "K_F13", 157),
                     ("F14", "K_F14", 158), ("F15", "K_F15", 159))

PROBE = r"""
#include "code/client/keycodes.h"
#include <cassert>
#include <cstring>
#include <string>
typedef unsigned char byte;
static byte locaseTable[256];
static const byte *const locase = locaseTable;
static int Q_stricmp( const char *a, const char *b ) {
	for ( ;; ++a, ++b ) {
		const int x = locase[(byte)*a], y = locase[(byte)*b];
		if ( x != y ) return x < y ? -1 : 1;
		if ( !x ) return 0;
	}
}
static int Com_HexStrToInt( const char *str ) {
	if ( !str || str[0] != '0' || str[1] != 'x' || str[2] == '\0' ) return -1;
	int n = 0;
	for ( const char *p = str + 2; *p; ++p ) {
		const int folded = locase[(byte)*p];
		const int digit = folded >= '0' && folded <= '9' ? folded - '0'
			: folded >= 'a' && folded <= 'f' ? folded - 'a' + 10 : -1;
		if ( digit < 0 ) return -1;
		n = n * 16 + digit;
	}
	return n;
}
static int mohaa;
static int Com_GameIsMOHAA( void ) { return mohaa; }
TABLES
STRING_TO_KEYNUM
KEYNUM_TO_STRING
static std::string Name( int keynum ) { return Key_KeynumToString( keynum ); }
int main() {
	for ( int i = 0; i < 256; ++i )
		locaseTable[i] = (byte)( i >= 'A' && i <= 'Z' ? i - 'A' + 'a' : i );

	// Both retail tables name 176, so a retail config line binds it by name and
	// this engine writes the same name back.
	assert( K_KP_STAR == 176 );
	assert( Key_StringToKeynum( "KP_STAR" ) == K_KP_STAR );
	assert( Key_StringToKeynum( "kp_star" ) == K_KP_STAR );
	assert( Name( K_KP_STAR ) == "KP_STAR" );

	// Neither names 130 or 157-159, so this engine writes them as retail does.
	RETAIL_HEX

	// Their names still load, from the configs earlier builds of this engine
	// wrote, and are never written back.
	RETAIL_HEX_NAMES
	for ( int keynum = 0; keynum < MAX_KEYS; ++keynum ) {
		const std::string written = Name( keynum );
		for ( const keyname_t *kn = keyaliases; kn->name; ++kn ) assert( written != kn->name );
	}

	// Every name in either table reaches its own key, and the name this engine
	// writes for a key either table holds parses back to that key.
	for ( const keyname_t *table : { keynames, keyaliases } ) {
		for ( const keyname_t *kn = table; kn->name; ++kn ) {
			assert( Key_StringToKeynum( kn->name ) == kn->keynum );
			assert( Key_StringToKeynum( Name( kn->keynum ).c_str() ) == kn->keynum );
		}
	}

	// Medal of Honor's default.cfg binds the left modifiers as LALT, LCTRL and
	// LSHIFT. Only its profile reads those names, as the modifier keys
	// themselves, and nothing writes them back; every other profile refuses
	// them, and the tables every profile reads keep their keys under it.
	for ( const keyname_t *kn = keyaliasesMOHAA; kn->name; ++kn ) {
		mohaa = 0;
		assert( Key_StringToKeynum( kn->name ) == -1 );
		mohaa = 1;
		assert( Key_StringToKeynum( kn->name ) == kn->keynum );
		assert( Key_StringToKeynum( Name( kn->keynum ).c_str() ) == kn->keynum );
		for ( int keynum = 0; keynum < MAX_KEYS; ++keynum ) assert( Name( keynum ) != kn->name );
	}
	assert( Key_StringToKeynum( "lalt" ) == K_ALT && Key_StringToKeynum( "LCtrl" ) == K_CTRL );
	assert( Key_StringToKeynum( "lshift" ) == K_SHIFT );
	for ( const keyname_t *table : { keynames, keyaliases } )
		for ( const keyname_t *kn = table; kn->name; ++kn )
			assert( Key_StringToKeynum( kn->name ) == kn->keynum );
	mohaa = 0;
	return 0;
}
"""


def read_text(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def function(source: str, signature: str) -> str:
    """The whole function, signature included, for splicing into a probe."""
    start = source.index(signature)
    return source[start:source.index("\n}\n", start) + 3]


def key_tables(source: str) -> str:
    """keyname_t and every key name table, from keys.c: the printed one, the
    aliases every profile reads, and Medal of Honor's own aliases."""
    start = source.index("typedef struct {")
    end = source.index("\n};\n", source.index("keyaliasesMOHAA[]", start)) + 4
    tables = source[start:end]
    assert "keynames[]" in tables and "keyaliases[]" in tables and "keyaliasesMOHAA[]" in tables
    return tables


def function_body(source: str, name: str) -> str:
    match = re.search(
        rf"(?:static\s+)?(?:const\s+char\s*\*\s*|(?:qboolean|void|int)\s+){name}\s*\([^)]*\)\s*\{{",
        source,
    )
    if not match:
        raise AssertionError(f"Missing function {name}")

    depth = 1
    for index in range(match.end(), len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return source[match.end() : index]
    raise AssertionError(f"Unterminated function {name}")


class KeyNameSourceTests(unittest.TestCase):
    """Retail Quake Live resolves key names without regard to case.

    `Key_StringToKeynum` folds single-character names to lowercase because key
    events are delivered as lowercased ASCII; returning the raw character
    accepts `bind A` and stores it on a keynum no input path can produce.
    """

    def test_single_character_key_names_fold_to_lowercase(self) -> None:
        body = function_body(read_text("code/qcommon/keys.c"), "Key_StringToKeynum")

        self.assertIn("return locase[ (byte)str[0] ];", body)
        self.assertNotIn("return str[0];", body)

    def test_named_keys_stay_case_insensitive(self) -> None:
        source = read_text("code/qcommon/keys.c")
        body = function_body(source, "Key_StringToKeynum")

        # The keynames scan and the gamepad aliases must keep using the
        # case-insensitive comparison, so "mwheelup" and "MWHEELUP" agree.
        self.assertIn("if ( !Q_stricmp( str, kn->name ) )", body)
        self.assertIn('!Q_stricmp( str, "PAD0_A" )', body)
        self.assertNotIn("strcmp( str, kn->name )", body)

    def test_hex_key_names_still_parse(self) -> None:
        body = function_body(read_text("code/qcommon/keys.c"), "Key_StringToKeynum")

        self.assertIn("Com_HexStrToInt( str )", body)

    def test_printable_range_matches_retail_and_round_trips(self) -> None:
        body = function_body(read_text("code/qcommon/keys.c"), "Key_KeynumToString")

        # Retail emits the full printable range, tilde included. Quote and
        # semicolon stay excluded because they would break config parsing.
        self.assertIn(
            "if ( keynum > ' ' && keynum < 127 && keynum != '\"' && keynum != ';' ) {",
            body,
        )
        self.assertNotIn("keynum < '~'", body)


class RetailKeyTableTests(unittest.TestCase):
    """The printed key table agrees with both retail tables on all five keys.

    `Key_KeynumToString` writes the config line and the name a menu shows, so a
    key retail leaves unnamed has to stay unnamed here. `Key_StringToKeynum`
    only reads, and may accept more: the four names this engine used to write
    still load from the configs that hold them.
    """

    def setUp(self) -> None:
        self.source = read_text("code/qcommon/keys.c")
        self.printed = self.source[:self.source.index("keyaliases[]")]
        self.aliases = self.source[self.source.index("keyaliases[]"):]
        self.aliases = self.aliases[:self.aliases.index("\n};\n")]

    def test_keypad_star_is_named_as_both_retail_tables_name_it(self) -> None:
        self.assertIn('{"KP_STAR",\t\t\tK_KP_STAR },', self.printed)
        self.assertNotIn('//{"KP_STAR"', self.source)

    def test_keys_retail_leaves_unnamed_are_never_written(self) -> None:
        for name, constant, _ in UNNAMED_BY_RETAIL:
            with self.subTest(name=name):
                self.assertNotIn('{"%s", %s}' % (name, constant), self.printed)
                self.assertIn('{"%s", %s}' % (name, constant), self.aliases)

    def test_only_the_printed_table_is_written_and_both_are_read(self) -> None:
        written = function_body(self.source, "Key_KeynumToString")
        read = function_body(self.source, "Key_StringToKeynum")

        self.assertIn("for ( kn = keynames ; kn->name ; kn++ ) {", written)
        self.assertNotIn("keyaliases", written)
        self.assertIn("for ( kn = keynames ; kn->name ; kn++ ) {", read)
        self.assertIn("for ( kn = keyaliases ; kn->name ; kn++ ) {", read)
        # The alias scan follows the printed one, so a name in both tables
        # keeps the key the printed table gives it.
        self.assertLess(read.index("kn = keynames"), read.index("kn = keyaliases"))

    def test_names_round_trip_through_both_tables(self) -> None:
        compiler = shutil.which("clang++") or shutil.which("g++")
        if not compiler:
            self.skipTest("compiler unavailable")
        target = compiler_target_args(compiler)
        if os.name == "nt" and not target:
            target = ["-DFNTECH3_Q3_ONLY"]

        hexes = "\n\t".join(
            'assert( %s == %d && Name( %s ) == "0x%02x" && Key_StringToKeynum( "0x%02x" ) == %s );'
            % (constant, keynum, constant, keynum, keynum, constant)
            for _, constant, keynum in UNNAMED_BY_RETAIL)
        names = "\n\t".join(
            'assert( Key_StringToKeynum( "%s" ) == %s && Key_StringToKeynum( "%s" ) == %s );'
            % (name, constant, name.lower(), constant)
            for name, constant, _ in UNNAMED_BY_RETAIL)
        source = (PROBE
                  .replace("TABLES", key_tables(self.source))
                  .replace("STRING_TO_KEYNUM", function(self.source, "int Key_StringToKeynum( const char *str ) {"))
                  .replace("KEYNUM_TO_STRING", function(self.source, "const char *Key_KeynumToString( int keynum ) {"))
                  .replace("RETAIL_HEX_NAMES", names)
                  .replace("RETAIL_HEX", hexes))

        (ROOT / ".tmp").mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(prefix="key-names-", dir=ROOT / ".tmp") as folder:
            cpp = Path(folder) / "probe.cpp"
            binary = Path(folder) / ("probe.exe" if os.name == "nt" else "probe")
            cpp.write_text(source, encoding="utf-8")
            built = subprocess.run([compiler, *target, "-std=c++17", "-Wall", "-Wextra",
                                    "-I", str(ROOT), str(cpp), "-o", str(binary)],
                                   capture_output=True, text=True, timeout=120)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            options = {"creationflags": subprocess.CREATE_NO_WINDOW} if os.name == "nt" else {}
            ran = subprocess.run([str(binary)], capture_output=True, text=True, timeout=30, **options)
            self.assertEqual(ran.returncode, 0, ran.stdout + ran.stderr)


if __name__ == "__main__":
    unittest.main()
