"""Raven's single-player menu files as fnTech3's engine reads them
(code/compat/raven_menu.hpp): each engine's keyword tables, named values and
tokens, measured in jasp.exe 1.01 and jk2sp.exe 1.02a; every retail menu
file read with no warnings when the installations are present; and the
engine menus' own rules in code/client/cl_jedi_sp_ui.cpp. See
docs/fntech3/2026-10-16-jedi-sp-menus.md."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest
import zipfile

from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]
HOST = ROOT / 'code/client/cl_jedi_sp_ui.cpp'
INSTALLS = {'jka': Path(r'E:/Games/Jedi Academy/base'), 'jk2': Path(r'E:/Games/Jedi Outcast/base')}

FIXTURE = r'''
#include "code/compat/raven_menu.hpp"
#include <cassert>
#include <cstdio>
#include <fstream>
#include <sstream>
using namespace fntech3::raven::menu;

static std::vector<std::string> warnings;
static void Warn(const Diagnostic &d) { warnings.push_back(d.message); }

static void Tokenizer() {
    Tokens t("// comment\n  word /* block\n comment */ \"two words\" next");
    assert(t.Next(true) == "word");
    assert(t.Next(true) == "two words");
    assert(t.Next(true) == "next");
    assert(t.Next(true).empty() && t.Ended());
    // Characters are signed: a byte above 0x7f counts as space.
    const std::string high = std::string("ab") + char(0xe9) + "cd";
    Tokens s(high);
    assert(s.Next(true) == "ab" && s.Next(true) == "cd");
    // Numbers come from the keyword's own line; strings may follow later.
    Tokens n("rect\n 1 2");
    int value = 0;
    assert(n.Next(true) == "rect" && !n.Int(value));
    Tokens m("name\n\n  \"later\"");
    std::string word;
    assert(m.Next(true) == "name" && m.String(word) && word == "later");
    // A word that reaches 1024 characters is discarded; 1023 are kept.
    const std::string longText = std::string(1024, 'x') + " after", keptText(1023, 'y');
    Tokens longWord(longText);
    assert(longWord.Next(true).empty() && longWord.Discarded() && longWord.Next(true) == "after");
    Tokens kept(keptText);
    assert(kept.Next(true).size() == 1023);
    Tokens rest("one two three");
    rest.Next(false);
    assert(rest.Rest() == " two three");
}

static void Tables() {
    auto count = [](auto &list) { return sizeof(list) / sizeof(list[0]); };
    assert(count(kAcademyMenuKeywords) == 37 && count(kAcademyCommands) == 33 && count(kAcademyItemKeywords) == 85);
    assert(count(kOutcastMenuKeywords) == 34 && count(kOutcastCommands) == 23 && count(kOutcastItemKeywords) == 69);
    for (const char *only : {"descTextStyle", "onAccept", "ignoreescape"}) {
        assert(IsMenuKeyword(Dialect::Academy, only) && !IsMenuKeyword(Dialect::Outcast, only));
    }
    assert(IsCommand(Dialect::Outcast, "playlooped") && !IsCommand(Dialect::Academy, "playlooped"));
    for (const char *only : {"playVoice", "stopVoice", "ffplay", "transition2", "transition3", "setitembackground",
                             "setitemtext", "setitemrect", "delay", "incrementfeeder", "decrementfeeder"}) {
        assert(IsCommand(Dialect::Academy, only) && !IsCommand(Dialect::Outcast, only));
    }
    // uiScript is no engine command: the game's own scripts take it.
    assert(!IsCommand(Dialect::Academy, "uiScript") && !IsCommand(Dialect::Outcast, "uiScript"));
    assert(IsItemKeyword(Dialect::Academy, "CVARSTRLIST") && IsItemKeyword(Dialect::Academy, "lineHeight"));
    assert(!IsItemKeyword(Dialect::Outcast, "lineHeight") && !IsItemKeyword(Dialect::Outcast, "isCharacter"));
    assert(TypeCount(Dialect::Academy) == 15 && TypeCount(Dialect::Outcast) == 14);
}

static const char *kMenu = R"(
{
    assetGlobalDef
    {
        smallFont "aurabesh" 18
        mediumFont "ergoec" 18
        cursor "cursor"
        itemFocusSound "sound/interface/menuroam.wav"
        precacheSound { "sound/a.wav" "sound/b.wav" }
        shadowColor 0.1 0.1 0.1 0.25
    }
    menuDef
    {
        name "testMenu"
        fullScreen 1
        rect 10 10 100 100
        visible 1
        descAlignment ITEM_ALIGN_CENTER
        onESC { play "sound/x.wav" ; close testMenu ; open other }
        itemDef
        {
            name list
            type ITEM_TYPE_LISTBOX
            feeder FEEDER_SAVEGAMES
            rect 5 6 7 8
            columns 2 0 40 10 50 60 20
            flag WINDOW_INACTIVE
            unknownKeyword
        }
        itemDef
        {
            name choice
            type ITEM_TYPE_MULTI
            cvarStrList { "@MENUS_ONE", "1" ; "@MENUS_TWO" "2" }
            cvar "ui_choice"
            action { uiScript update ui_choice ; exec "map mission" }
        }
        itemDef
        {
            name floats
            type ITEM_TYPE_MULTI
            cvarFloatList { "@LOW" 2 "@HIGH" 0 }
        }
        itemDef
        {
            name species
            type ITEM_TYPE_MULTI
            feeder 23
            cvarStrList feeder
        }
        itemDef
        {
            name scroll
            type ITEM_TYPE_TEXTSCROLL
            style WINDOW_STYLE_SHADER
            textalign ITEM_ALIGN_RIGHT
        }
    }
}
)";

static void Reading(Dialect dialect) {
    warnings.clear();
    Reader reader(dialect, Warn);
    Assets assets;
    std::vector<std::unique_ptr<Menu>> menus;
    std::string text = kMenu;
    if (dialect == Dialect::Outcast) {
        // Outcast has no precacheSound: an unknown keyword's own braces would
        // end the asset block early, so its menus are read without it here.
        const auto start = text.find("        precacheSound");
        text.erase(start, text.find('\n', start) - start + 1);
        Reader alone(dialect, Warn);
        Assets unused;
        std::vector<std::unique_ptr<Menu>> none;
        alone.File("ui/asset.menu", "{ assetGlobalDef { precacheSound } }", unused, none);
    }
    assert(reader.File("ui/test.menu", text, assets, menus));
    assert(menus.size() == 1);
    const Menu &menu = *menus[0];
    assert(menu.window.name == "testMenu" && menu.fullScreen);
    assert(menu.window.rect.x == 0 && menu.window.rect.w == 640 && menu.window.rect.h == 480);
    assert(menu.descAlignment == AlignCenter && (menu.window.flags & flag::Visible));
    assert(menu.onESC == "\"play\" \"sound/x.wav\" ; \"close\" \"testMenu\" ; \"open\" \"other\" ");
    assert(menu.items.size() == 5);
    const Item &list = *menu.items[0];
    assert(list.type == TypeListBox && list.special == 0 && list.listBox && list.listBox->numColumns == 2);
    assert(list.listBox->columns[1].pos == 50 && list.listBox->columns[1].maxChars == 20);
    assert(list.window.rect.x == 5 && list.window.rect.y == 6 && (list.window.flags & flag::Inactive));
    assert(list.parent == menus[0].get());
    const Item &choice = *menu.items[1];
    assert(choice.multi && choice.multi->strDef && choice.multi->cvarList.size() == 2);
    assert(choice.multi->cvarStr[0] == "1" && choice.multi->cvarList[1] == "@MENUS_TWO");
    assert(choice.action == "\"uiScript\" \"update\" \"ui_choice\" ; \"exec\" \"map mission\" ");
    const Item &floats = *menu.items[2];
    assert(floats.multi && !floats.multi->strDef && floats.multi->cvarValue[0] == 2 && floats.multi->cvarValue[1] == 0);
    assert(menu.items[3]->multi && menu.items[3]->multi->listFeeder && menu.items[3]->special == 23);
    const Item &scroll = *menu.items[4];
    assert(scroll.window.style == StyleShader && scroll.textAlignment == AlignRight);
    if (dialect == Dialect::Academy) {
        assert(scroll.type == TypeTextScroll && scroll.scroll);
        assert(assets.precacheSounds.size() == 2);
        assert(warnings.size() == 1 && warnings[0] == "Unknown item keyword 'unknownKeyword'");
    } else {
        // Outcast has no text scroll type and no precacheSound keyword.
        assert(scroll.type == TypeText);
        assert(assets.precacheSounds.empty());
        bool type = false, asset = false;
        for (const auto &w : warnings) {
            type = type || w == "Unknown item type value 'ITEM_TYPE_TEXTSCROLL'";
            asset = asset || w == "Unknown asset keyword 'precacheSound'";
        }
        assert(type && asset);
    }
    assert(assets.fonts.size() == 2 && assets.fonts[1].name == "ergoec" && assets.mediumFont == "ergoec");
    assert(assets.cursor == "cursor" && assets.shadowColor[3] == 0.25f);
    const auto names = Reader::MenuList("{ loadMenu { \"ui/a.menu\" \"ui/b.menu\" } loadMenu { \"ui/c.menu\" } }",
                                        nullptr, "ui/menus.txt");
    assert(names.size() == 3 && names[2] == "ui/c.menu");
}

static std::string Read(const std::string &path) {
    std::ifstream in(path, std::ios::binary);
    std::stringstream s;
    s << in.rdbuf();
    return s.str();
}

// Every menu file each list names, read with no warnings: "<lists> <files> <menus> <items> <warnings>".
static int Installed(const char *game, const std::string &root) {
    warnings.clear();
    Reader reader(std::string(game) == "jka" ? Dialect::Academy : Dialect::Outcast, Warn);
    Assets assets;
    std::vector<std::unique_ptr<Menu>> menus;
    int lists = 0, files = 0;
    std::size_t items = 0;
    for (const char *listName : {"ui/menus.txt", "ui/ingame.txt"}) {
        const std::string list = Read(root + "/" + listName);
        if (list.empty()) continue;
        ++lists;
        for (std::string name : Reader::MenuList(list, Warn, listName)) {
            for (char &c : name) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            const std::string text = Read(root + "/" + name);
            assert(!text.empty());
            reader.File(name, text, assets, menus);
            ++files;
        }
    }
    for (const auto &menu : menus) items += menu->items.size();
    for (const auto &w : warnings) std::printf("warning: %s\n", w.c_str());
    std::printf("%d %d %d %d %d\n", lists, files, static_cast<int>(menus.size()), static_cast<int>(items),
                static_cast<int>(warnings.size()));
    return 0;
}

int main(int argc, char **argv) {
    if (argc == 3) return Installed(argv[1], argv[2]);
    Tokenizer();
    Tables();
    Reading(Dialect::Academy);
    Reading(Dialect::Outcast);
    std::puts("ok");
    return 0;
}
'''


class RavenMenuReaderTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            raise unittest.SkipTest('C++17 compiler unavailable')
        scratch = ROOT / '.tmp/tests/raven-menu'
        scratch.mkdir(parents=True, exist_ok=True)
        cls.temporary = tempfile.TemporaryDirectory(dir=scratch)
        directory = Path(cls.temporary.name).resolve()
        source = directory / 'fixture.cpp'
        source.write_text(FIXTURE)
        cls.executable = directory / ('fixture.exe' if os.name == 'nt' else 'fixture')
        built = subprocess.run([compiler, *compiler_target_args(compiler), '-std=c++17', '-Werror', '-I', str(ROOT),
                                str(source), '-o', str(cls.executable)], cwd=directory, capture_output=True,
                               text=True, timeout=120)
        if built.returncode:
            raise AssertionError(built.stdout + built.stderr)
        cls.directory = directory

    @classmethod
    def tearDownClass(cls):
        try:
            cls.temporary.cleanup()
        except OSError:
            pass

    def test_tokens_tables_and_reading(self):
        ran = subprocess.run([str(self.executable)], capture_output=True, text=True, timeout=60)
        self.assertEqual(ran.returncode, 0, ran.stdout + ran.stderr)
        self.assertIn('ok', ran.stdout)

    def test_every_installed_menu_reads_cleanly(self):
        # Academy: 12 menu files in ui/menus.txt and 24 in ui/ingame.txt;
        # Outcast: 21 in ui/menus.txt and no ingame.txt.
        expected = {'jka': (2, 36, 36, 1344), 'jk2': (1, 21, 21, 757)}
        for game, base in INSTALLS.items():
            with self.subTest(game=game):
                if not base.is_dir():
                    self.skipTest(f'{base} is not installed')
                root = self.directory / game
                for pak in sorted(base.glob('*.pk3')):   # later paks override earlier ones
                    with zipfile.ZipFile(pak) as archive:
                        for name in archive.namelist():
                            low = name.lower()
                            if low.startswith('ui/') and low.endswith(('.menu', '.txt')):
                                target = root / low
                                target.parent.mkdir(parents=True, exist_ok=True)
                                target.write_bytes(archive.read(name))
                ran = subprocess.run([str(self.executable), game, str(root)], capture_output=True, text=True,
                                     timeout=120)
                self.assertEqual(ran.returncode, 0, ran.stdout + ran.stderr)
                lists, files, menus, items, warnings = map(int, ran.stdout.split()[-5:])
                self.assertEqual(warnings, 0, ran.stdout)
                self.assertEqual((lists, files, menus, items), expected[game])


class EngineMenuSourceTests(unittest.TestCase):
    def setUp(self):
        self.host = HOST.read_text(encoding='utf-8')

    def test_text_styles_follow_both_engines_tables(self):
        # jasp.exe 0x4951a0 and jk2sp.exe 0x4999d0: style 2 blinks, 3 to 6
        # shadow, and 1 -- the menus' buttons -- draws plain.
        body = self.host.split('void DrawText(', 1)[1].split('\n}', 1)[0]
        self.assertIn('case StylePulse: flags = kFontBlink; break;', body)
        self.assertIn('case StyleShadowed: case StyleOutlined: case StyleOutlineShadowed: case StyleShadowedMore:',
                      body)
        self.assertNotIn('case StyleBlink', body)
        self.assertIn('static_cast<int>( x )', body)

    def test_scripts_route_unknown_commands_to_the_games_scripts(self):
        run = self.host.split('void RunScript( rm::Item &item, const std::string &script ) {', 1)[1]
        run = run.split('\n}', 1)[0]
        self.assertIn('if ( rm::IsCommand( dialect, command ) ) {', run)
        self.assertIn('RunGameScript( args, item );', run)
        self.assertIn('if ( Same( name, "VideoSetup" ) ) {', self.host)

    def test_the_main_menu_and_its_error(self):
        main = self.host.split('void MainMenu() {', 1)[1].split('\n}', 1)[0]
        self.assertIn('Cvar_Set( "sv_killserver", "1" );', main)
        self.assertIn('Key_SetCatcher( KEYCATCH_UI );', main)
        self.assertIn('if ( !ActivateByName( "mainMenu" ) ) LoadMenus( "ui/menus.txt", false );', main)
        self.assertIn('ActivateByName( "error_popmenu" );', main)

    def test_outcast_registers_its_menu_strings_as_jk2sp_does(self):
        init = self.host.split('void Init( bool inGame ) {', 1)[1].split('\n}', 1)[0]
        self.assertIn('for ( int i = 0; i < 10; ++i ) {', init)
        self.assertIn('va( "menus%d", i ), kOutcastMenuStrings', init)
        self.assertIn('constexpr unsigned kOutcastMenuStrings = 4;', self.host)

    def test_menu_sounds_are_interface_sounds(self):
        play = self.host.split('void PlaySound( sfxHandle_t sfx, int channel ) {', 1)[1].split('\n}', 1)[0]
        self.assertIn('S_StartInterfaceSound( sfx, channel, 1.0f );', play)


if __name__ == '__main__':
    unittest.main()
