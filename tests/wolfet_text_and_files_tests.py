"""ET's localisation lookup and its own file deletion."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
MAIN = ROOT/'code/client/cl_main.cpp'
FILES = ROOT/'code/qcommon/files.c'
CGAME = ROOT/'code/client/cl_cgame.cpp'
UI = ROOT/'code/client/cl_ui.cpp'
RECORD = ROOT/'docs/fntech3/2026-09-18-wolfet-text-and-files.md'


def _function(path, signature):
    lines = path.read_text(encoding='utf-8').split('\n')
    first = next(i for i, line in enumerate(lines) if line.startswith(signature))
    last = next(i for i in range(first + 1, len(lines)) if lines[i] == '}')
    return '\n'.join(lines[first:last + 1])


class WolfETTextAndFileTests(unittest.TestCase):
    def test_the_two_profiles_keep_separate_catalogues(self):
        """A record loaded for one profile must not answer for the other."""
        text = MAIN.read_text(encoding='utf-8')
        self.assertIn('static fntech3::rtcw::translation::Catalog rtcwTranslations;', text)
        self.assertIn('static fntech3::rtcw::translation::Catalog wolfetTranslations;', text)
        region = _function(MAIN, 'void CL_WolfETTranslateString')
        self.assertIn('wolfetTranslations.Translate', region)
        self.assertNotIn('rtcwTranslations', region)
        self.assertIn('Com_GameIsWolfET()', region)

    def test_translation_bounds_its_input_and_output(self):
        region = _function(MAIN, 'void CL_WolfETTranslateString')
        self.assertIn('VM_CHECKBOUNDS( vm, destination, OutputBytes );', region)
        self.assertIn('ET translation input is too long', region)
        self.assertIn('ET translation output is too long', region)
        # The module may hand in the same buffer it reads back.
        self.assertIn('input may alias the output', region)

    def test_a_null_string_answers_as_et_does(self):
        region = _function(MAIN, 'void CL_WolfETTranslateString')
        self.assertIn('std::string result = "(null)";', region)

    def test_both_boundaries_reach_the_same_lookup(self):
        self.assertIn('CL_WolfETTranslateString( cgvm, args[1], args[2] );',
                      _function(CGAME, 'static bool CL_WolfETCgameCall'))
        self.assertIn('CL_WolfETTranslateString( uivm, args[1], args[2] );',
                      _function(UI, 'static bool CL_WolfETUICall'))

    def test_deletion_reaches_only_the_three_named_folders(self):
        """Anywhere else in the installation must stay out of reach."""
        region = _function(FILES, 'int FS_WolfETDelete')
        for folder in ('"save/", 5', '"profiles/", 9', '"demos/", 6'):
            self.assertIn(folder, region, folder)
        self.assertIn('FS_WriteQpathIsValid( path )', region)
        self.assertIn('FS_CheckFilenameIsNotAllowed', region)

    def test_deletion_walks_no_deeper_than_et_needs(self):
        region = _function(FILES, 'static int FS_WolfETDeleteTree')
        self.assertIn('depth > 4', region)
        self.assertIn('Sys_IsDirectory', region)
        self.assertIn('Sys_RemoveDirectory', region)

    def test_both_platforms_supply_the_directory_primitives(self):
        for path in ('code/win32/win_main.cpp', 'code/unix/unix_shared.cpp'):
            text = (ROOT/path).read_text(encoding='utf-8')
            self.assertIn('Sys_RemoveDirectory( const char *ospath )', text, path)
            self.assertIn('Sys_IsDirectory( const char *ospath )', text, path)

    def test_the_record_states_the_coverage(self):
        record = RECORD.read_text(encoding='utf-8')
        self.assertIn('136 of the 147', record)
        self.assertIn('114 of the 121', record)


if __name__ == '__main__':
    unittest.main()
