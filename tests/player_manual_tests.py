from __future__ import annotations

import html.parser
import json
import re
import sys
import tempfile
import unittest
from pathlib import Path
from urllib.parse import urlsplit

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from scripts import player_manual, release


class _Links(html.parser.HTMLParser):
    def __init__(self) -> None:
        super().__init__()
        self.ids: set[str] = set()
        self.hrefs: list[str] = []
        self.sources: list[str] = []

    def handle_starttag(self, tag, attrs):
        values = dict(attrs)
        if "id" in values:
            self.ids.add(values["id"])
        if tag == "a" and values.get("href"):
            self.hrefs.append(values["href"])
        for name in ("src", "href"):
            if tag in ("img", "link", "script") and values.get(name):
                self.sources.append(values[name])


def scratch() -> tempfile.TemporaryDirectory:
    base = ROOT / ".tmp" / "player-manual-tests"
    base.mkdir(parents=True, exist_ok=True)
    return tempfile.TemporaryDirectory(dir=base)


def render(markdown: str, *, extra: dict[str, str] | None = None) -> str:
    """Render one guide, docs/A.md, beside an optional second, docs/B.md."""
    with scratch() as tmp:
        root = Path(tmp)
        (root / "docs").mkdir()
        (root / "docs" / "A.md").write_text(markdown, encoding="utf-8")
        guides = [player_manual.Guide("docs/A.md", "a.html", "A", "A guide.", "start")]
        for name, text in (extra or {}).items():
            (root / "docs" / name).write_text(text, encoding="utf-8")
            guides.append(player_manual.Guide(f"docs/{name}", name.lower().replace(".md", ".html"),
                                              name, "Another guide.", "games"))
        (root / "BUILD.md").write_text("# Build\n", encoding="utf-8")
        renderer = player_manual.Renderer(tuple(guides), root=root)
        rendered = [player_manual.render_guide(renderer, guide) for guide in guides]
        player_manual._check_links(renderer, rendered)
        return rendered[0].body


class MarkdownRenderingTests(unittest.TestCase):
    def test_headings_take_githubs_anchors_and_the_title_is_lifted_out(self) -> None:
        body = render("# Title\n\n## When To Use `vid_restart`\n\n## Alice And F.A.K.K.2 Far Plane\n\n"
                      "## Fast Windowed/Fullscreen Toggle\n\n## Quake III Arena (`quake3`)\n\n## Repeat\n\n## Repeat\n")
        for anchor in ("when-to-use-vid_restart", "alice-and-fakk2-far-plane",
                       "fast-windowedfullscreen-toggle", "quake-iii-arena-quake3", "repeat", "repeat-1"):
            self.assertIn(f'id="{anchor}"', body)
        self.assertNotIn("<h1", body)
        self.assertIn("<code>vid_restart</code>", body)

    def test_inline_text_is_escaped_and_formatted(self) -> None:
        body = render("# T\n\nA **bold** and *soft* word, `<map> & r_mode` code, fs_homepath and"
                      " r_mode_x stay plain, \\*not emphasis\\*, a -- dash.\n")
        self.assertIn("<strong>bold</strong>", body)
        self.assertIn("<em>soft</em>", body)
        self.assertIn("<code>&lt;map&gt; &amp; r_mode</code>", body)
        self.assertIn("fs_homepath and r_mode_x stay plain", body)
        self.assertIn("*not emphasis*", body)
        self.assertIn("a – dash", body)
        self.assertNotIn("<em>homepath", body)

    def test_links_between_guides_become_pages_and_others_open_on_github(self) -> None:
        body = render("# T\n\nSee [B](B.md#part), [here](#t-part), [build](../BUILD.md#x) and "
                      "[site](https://example.com/a?b=1).\n\n## T part\n",
                      extra={"B.md": "# B\n\n## Part\n"})
        self.assertIn('href="b.html#part"', body)
        self.assertIn('href="#t-part"', body)
        self.assertIn(f'href="{player_manual.REPOSITORY_URL}/blob/main/BUILD.md#x" class="external"', body)
        self.assertIn('href="https://example.com/a?b=1" class="external"', body)

    def test_broken_links_and_missing_sections_stop_the_build(self) -> None:
        cases = (
            "# T\n\n[gone](#nowhere)\n",
            "# T\n\n[gone](B.md#nowhere)\n",
            "# T\n\n[gone](MISSING.md)\n",
            "# T\n\n[out](../../outside.md)\n",
        )
        for markdown in cases:
            with self.subTest(markdown=markdown), self.assertRaises(player_manual.ManualError):
                render(markdown, extra={"B.md": "# B\n\n## Part\n"})

    def test_lists_tables_code_and_callouts(self) -> None:
        body = render(
            "# T\n\n"
            "- one\n  continued\n- two\n  1. nested\n  2. again\n\n"
            "3. three\n4. four\n\n"
            "| Setting | Default |\n| --- | :---: |\n| `r_a \\| b` | `1` |\n\n"
            "```cfg\nseta r_mode \"-2\"\n```\n\n"
            "> [!TIP]\n> Use `vid_restart`.\n\n"
            "> Plain quote.\n"
        )
        self.assertIn("<ul><li>one\ncontinued</li><li>two\n<ol><li>nested</li><li>again</li></ol></li></ul>", body)
        self.assertIn('<ol start="3"><li>three</li><li>four</li></ol>', body)
        self.assertIn('<th class="align-center">Default</th>', body)
        self.assertIn("<code>r_a | b</code>", body)
        self.assertIn("Console or config file", body)
        self.assertIn("seta r_mode &quot;-2&quot;", body.replace('"', "&quot;"))
        self.assertIn('class="callout callout-tip"', body)
        self.assertIn("<blockquote><p>Plain quote.</p></blockquote>", body)

    def test_loose_list_items_keep_their_paragraphs(self) -> None:
        body = render("# T\n\n- first\n\n- second\n")
        self.assertIn("<ul><li><p>first</p></li><li><p>second</p></li></ul>", body)

    def test_unsupported_markdown_is_refused(self) -> None:
        for markdown in ("# T\n\n<details>\n", "# T\n\n![logo](x.png)\n", "# T\n\n```cfg\nopen\n",
                         "# T\n\n# Second title\n", "Missing title\n",
                         "# T\n\n| a | b |\n| --- |\n"):
            with self.subTest(markdown=markdown), self.assertRaises(player_manual.ManualError):
                render(markdown)


class PlayerManualBuildTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.temporary = scratch()
        cls.output = Path(cls.temporary.name) / "manual"
        cls.files = player_manual.build_manual(cls.output)

    @classmethod
    def tearDownClass(cls) -> None:
        cls.temporary.cleanup()

    def test_every_guide_is_built_and_listed(self) -> None:
        built = sorted(f"{player_manual.MANUAL_DIR_NAME}/{path.relative_to(self.output).as_posix()}"
                       for path in self.files)
        self.assertEqual(built, player_manual.manual_entries())
        for guide in player_manual.GUIDES:
            self.assertTrue((ROOT / guide.source).is_file(), guide.source)
            self.assertTrue((self.output / guide.page).is_file(), guide.page)
        self.assertEqual(len({guide.page for guide in player_manual.GUIDES}), len(player_manual.GUIDES))

    def test_pages_link_only_to_pages_sections_and_assets_that_exist(self) -> None:
        parsed = {}
        for path in self.output.glob("*.html"):
            parser = _Links()
            parser.feed(path.read_text(encoding="utf-8"))
            parsed[path.name] = parser
        for name, parser in parsed.items():
            for href in parser.hrefs:
                link = urlsplit(href)
                if link.scheme in ("http", "https", "mailto"):
                    continue
                page = link.path or name
                with self.subTest(page=name, href=href):
                    self.assertIn(page, parsed)
                    if link.fragment:
                        self.assertIn(link.fragment, parsed[page].ids)
            for source in parser.sources:
                with self.subTest(page=name, source=source):
                    self.assertTrue((self.output / source).is_file())

    def test_pages_carry_no_unrendered_markdown(self) -> None:
        for path in self.output.glob("*.html"):
            text = re.sub(r"<(code|pre)>.*?</\1>", "", path.read_text(encoding="utf-8"), flags=re.S)
            with self.subTest(page=path.name):
                self.assertNotIn("](", text)
                self.assertNotIn("**", text)
                self.assertNotRegex(text, r"(?m)^\s*(#{2,6} |\|.*\||[-*] )")

    def test_build_is_deterministic(self) -> None:
        with scratch() as tmp:
            again = Path(tmp) / "manual"
            player_manual.build_manual(again)
            for path in self.files:
                relative = path.relative_to(self.output)
                with self.subTest(file=relative.as_posix()):
                    self.assertEqual(path.read_bytes(), (again / relative).read_bytes())

    def test_theme_uses_the_flame_colours_and_a_light_theme(self) -> None:
        css = (self.output / "assets" / "manual.css").read_text(encoding="utf-8")
        self.assertIn("--accent: #fc5c00;", css)
        self.assertIn("--ember: #e02000;", css)
        self.assertIn('@media (prefers-color-scheme: light)', css)
        self.assertIn(':root[data-theme="light"]', css)
        page = (self.output / "index.html").read_text(encoding="utf-8")
        self.assertIn("assets/manual.css", page)
        self.assertNotRegex(page, r"(?:src|href)=\"//")

    def test_search_finds_sections_by_the_settings_they_explain(self) -> None:
        script = (self.output / "assets" / "search-index.js").read_text(encoding="utf-8")
        entries = json.loads(script.split("=", 1)[1].strip().rstrip(";"))
        by_href = {entry["h"]: entry for entry in entries}
        self.assertIn("s_alHrtf", by_href["audio.html#headphones-and-speakers"]["k"].split())
        self.assertIn("r_liquid", by_href["display.html#enhanced-liquids"]["k"].split())
        self.assertEqual(by_href["games.html"]["t"], "Games Guide")
        for guide in player_manual.GUIDES:
            self.assertIn(guide.page, by_href)

    def test_game_sections_carry_their_icons(self) -> None:
        games = (self.output / "games.html").read_text(encoding="utf-8")
        for anchor, icon, _label in player_manual.GAME_LINKS:
            with self.subTest(anchor=anchor):
                self.assertRegex(games, rf'id="{re.escape(anchor)}"><img class="game-icon" '
                                        rf'src="assets/games/{re.escape(icon)}-64.png"')


class PackagedManualTests(unittest.TestCase):
    def test_release_packages_stage_the_manual_beside_readme(self) -> None:
        for entry in ("manual/index.html", "manual/getting-started.html", "manual/assets/manual.css"):
            self.assertIn(entry, release.REQUIRED_RELEASE_ARCHIVE_ENTRIES)
        with scratch() as tmp:
            stage = Path(tmp)
            release.copy_docs(stage)
            for entry in player_manual.manual_entries():
                with self.subTest(entry=entry):
                    self.assertTrue((stage / entry).is_file())
            readme = (stage / "README.html").read_text(encoding="utf-8")
            for target in re.findall(r'href="(manual/[^"#]+)', readme):
                with self.subTest(target=target):
                    self.assertTrue((stage / target).is_file())
            self.assertIn('src="manual/assets/fntech3-256.png"', readme)

    def test_landing_page_and_guides_share_the_flame_palette(self) -> None:
        template = (ROOT / "docs" / "templates" / "install-readme.html.in").read_text(encoding="utf-8")
        self.assertIn("--accent: #fc5c00;", template)
        self.assertIn("--ember: #e02000;", template)
        self.assertIn('href="manual/index.html"', template)


if __name__ == "__main__":
    unittest.main()
