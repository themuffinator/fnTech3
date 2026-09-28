"""The parts of scripts/scan_game_import_calls.py that need no installation.

The scanner backs docs/fntech3/2026-09-28-aas-portal-reachability-index.md:
it takes each profile's import numbers from the engine's own tables and reads
a QVM's syscalls from its code. Both are checked here, on the engine's
headers and on a QVM written for the test. Its native-module readers need
retail modules; the record's own runs exercise them, with a control import
the games do call.
"""
from pathlib import Path
import struct
import sys
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'scripts'))
import scan_game_import_calls as scan  # noqa: E402


def op(code, value=None, width=4):
    return bytes([code]) + (b'' if value is None else value.to_bytes(width, 'little', signed=True))


def qvm(instructions):
    code = b''.join(instructions)
    header = struct.pack('<8i', 0x12721444, len(instructions), 32, len(code), 32 + len(code), 0, 0, 0)
    return header + code


class ScanGameImportCallsTests(unittest.TestCase):
    def test_ordinals_come_from_the_engine_tables(self):
        self.assertEqual(scan.ordinals('BOTLIB_AAS_POINT_REACHABILITY_AREA_INDEX'), {
            'q3': 577, 'ql': 68, 'rtcw-sp': 578, 'rtcw-mp': 578, 'stvef-sp': None, 'stvef-mp': None,
            'wolfet': 578, 'jka-mp': 577})
        area = scan.ordinals('BOTLIB_AAS_POINT_AREA_NUM')
        self.assertEqual((area['q3'], area['ql'], area['rtcw-sp'], area['stvef-mp'], area['wolfet'], area['jka-mp']),
                         (307, 67, 308, 307, 308, 307))
        self.assertEqual(area['stvef-sp'], None)
        printing = scan.ordinals('G_PRINT')
        self.assertEqual((printing['q3'], printing['ql']), (0, 2))

    def test_enum_rows_count_on_from_the_last_value_they_can_read(self):
        text = 'typedef enum {\n\tA, // a\n\tB = 10, /* b, c */\n\tC,\n\tD = B,\n\tE = UNKNOWN,\n\tF\n} t;'
        self.assertEqual(scan.enum_values(text, '} t;'), {'A': 0, 'B': 10, 'C': 11, 'D': 10, 'E': None, 'F': None})

    def test_qvm_syscalls_are_constants_below_zero_that_are_called(self):
        # ENTER; two syscalls; a call of the QVM's own function 40; the import's
        # address pushed but not called; LOCAL and ARG, whose operands must be
        # stepped over; one more syscall; LEAVE.
        data = qvm([op(3, 8), op(8, -578), op(5), op(8, -308), op(5), op(8, 40), op(5),
                    op(8, -578), op(9, 4), op(33, 1, 1), op(8, 1), op(8, -308), op(5), op(4, 8)])
        calls, detail, unresolved = scan.qvm_calls(data, 577)
        self.assertEqual(calls, {577: [2], 307: [4, 12]})
        self.assertEqual(detail['constantsOfImport'], 2)
        self.assertEqual(detail['decodedBytes'], detail['codeBytes'])
        self.assertEqual(unresolved, [])

    def test_a_file_that_is_not_a_qvm_is_refused(self):
        with self.assertRaises(ValueError):
            scan.qvm_calls(bytes(64), 577)


if __name__ == '__main__':
    unittest.main()
