from __future__ import annotations

import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def read_text(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


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


if __name__ == "__main__":
    unittest.main()
