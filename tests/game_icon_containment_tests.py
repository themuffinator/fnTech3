"""Installation icon paths must resolve links before checking containment."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]


class IconContainmentTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        compiler = os.environ.get('CXX') or shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            raise unittest.SkipTest('C++ compiler unavailable')
        scratch = ROOT / '.tmp' / 'icon-containment'
        scratch.mkdir(parents=True, exist_ok=True)
        cls.work = tempfile.TemporaryDirectory(prefix='test-', dir=scratch)
        cls.addClassCleanup(cls.work.cleanup)
        cls.folder = Path(cls.work.name)
        source = cls.folder / 'probe.cpp'
        cls.exe = cls.folder / 'probe.exe'
        source.write_text(r'''
#include "code/platform/game_icon.hpp"
#include <iostream>
// Transport UTF-8 as hex so the Windows CRT's argv code page cannot alter it.
static std::string Utf8(const char *hex) {
    const std::string input(hex);
    std::string output;
    for (size_t i = 0; i < input.size(); i += 2)
        output += static_cast<char>(std::stoi(input.substr(i, 2), nullptr, 16));
    return output;
}
int main(int argc, char **argv) {
    if (argc != 4) return 2;
    using namespace fntech3::icon;
    const auto source = argv[2][0] == 'b' ? Source::Bundle : Source::Png;
    const auto root = Utf8(argv[1]);
    const auto found = Resolve(root.c_str(), COM_GAME_QUAKE3, source, 0);
    if (found != std::filesystem::u8path(Utf8(argv[3]))) {
        std::cerr << "Unexpected icon: " << found.u8string() << '\n';
        return 1;
    }
}
''', encoding='utf-8')
        target_args = compiler_target_args(compiler)
        subprocess.run([compiler, *target_args, '-std=c++17',
                        '-I', str(ROOT), str(source), '-o', str(cls.exe)],
                       check=True, capture_output=True, text=True, cwd=ROOT)

    def setUp(self):
        self.fixture = tempfile.TemporaryDirectory(prefix='fixture-', dir=self.folder)
        self.addCleanup(self.fixture.cleanup)
        self.base = Path(self.fixture.name)
        self.root = self.base / 'game-é'
        self.root.mkdir()

    def check(self, source, expected=None, root=None):
        result = subprocess.run([str(self.exe), str(root or self.root).encode('utf-8').hex(), source,
                                 str(expected.resolve()).encode('utf-8').hex() if expected else ''],
                                capture_output=True, text=True, encoding='utf-8', cwd=ROOT)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def junction(self, link, target):
        # All paths belong to this fixture; remove the link itself before
        # TemporaryDirectory can recursively clean the ordinary directories.
        self.assertTrue(link.parent.resolve().is_relative_to(self.base.resolve()))
        self.assertTrue(target.resolve().is_relative_to(self.base.resolve()))
        result = subprocess.run(['cmd', '/d', '/c', 'mklink', '/J', str(link), str(target)],
                                capture_output=True, text=True, errors='replace',
                                creationflags=subprocess.CREATE_NO_WINDOW)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertTrue(os.lstat(link).st_file_attributes & 0x400)
        self.addCleanup(os.rmdir, link)

    def test_regular_missing_and_wrong_type(self):
        self.check('png')
        icon = self.root / 'quake3.png'
        icon.write_bytes(b'synthetic icon')
        self.check('png', icon)
        self.check('bundle')
        bundle = self.root / 'Quake3.app'
        bundle.write_bytes(b'not a directory')
        self.check('bundle')
        bundle.unlink()
        bundle.mkdir()
        self.check('bundle', bundle)

    @unittest.skipUnless(os.name == 'nt', 'Windows junction regression')
    def test_bundle_junction_cannot_escape(self):
        outside = self.base / 'outside.app'
        outside.mkdir()
        self.junction(self.root / 'Quake3.app', outside)
        self.check('bundle')

    @unittest.skipUnless(os.name == 'nt', 'Windows junction regression')
    def test_installation_junction_resolves_consistently(self):
        icon = self.root / 'quake3.png'
        icon.write_bytes(b'synthetic icon')
        alias = self.base / 'installation-alias'
        self.junction(alias, self.root)
        self.check('png', icon, alias)

    def test_file_symlink_cannot_escape(self):
        outside = self.base / 'outside.png'
        outside.write_bytes(b'synthetic icon')
        link = self.root / 'quake3.png'
        try:
            link.symlink_to(outside)
        except OSError as error:
            self.skipTest(f'Host does not permit file symlinks: {error}')
        self.addCleanup(link.unlink)
        self.check('png')

    def test_internal_file_symlink_resolves(self):
        icon = self.root / 'kept.png'
        icon.write_bytes(b'synthetic icon')
        link = self.root / 'quake3.png'
        try:
            link.symlink_to(icon)
        except OSError as error:
            self.skipTest(f'Host does not permit file symlinks: {error}')
        self.addCleanup(link.unlink)
        self.check('png', icon)


if __name__ == '__main__':
    unittest.main()
