"""Hex cvar integers are a Quake Live retail rule, not a shared one.

Retail quakelive_steam.exe computes cvar_t::integer with sscanf( s, "0x%08x" )
for hex strings (ported from FnQL 6f504de); Quake III and every other profile's
retail engine uses atoi alone, so cvar.c must take the hex path only under the
Quake Live profile. tests/cvar_integer_tests.c pins the parse itself.
"""

from __future__ import annotations

import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


class CvarIntegerProfileTests(unittest.TestCase):
    def test_hex_parse_is_gated_on_quake_live(self) -> None:
        source = (ROOT / "code/qcommon/cvar.c").read_text(encoding="utf-8")
        helper = source[source.index("static int Cvar_IntegerValue( const char *s ) {") :]
        helper = helper[: helper.index("}") + 1]
        self.assertIn(
            "Com_GameIsQuakeLive() ? Cvar_ParseQLInteger( s ) : atoi( s )", helper
        )

    def test_every_integer_assignment_uses_the_profile_rule(self) -> None:
        source = (ROOT / "code/qcommon/cvar.c").read_text(encoding="utf-8")
        self.assertEqual(
            source.count("var->integer = Cvar_IntegerValue( var->string );"), 2
        )
        self.assertIsNone(re.search(r"var->integer\s*=\s*atoi", source))


if __name__ == "__main__":
    unittest.main()
