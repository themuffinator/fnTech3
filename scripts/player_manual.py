"""Build the HTML edition of the player guides.

The Markdown guides in docs/ stay the only source of the text. This script
renders them, with the stylesheet and script kept in docs/manual/, into a
folder of linked pages that opens straight from disk with no network access.
The release packager stages that folder as manual/ beside README.html. Run it
directly to preview a build:

    python scripts/player_manual.py --output .tmp/manual

Only the Python standard library is used, so packaging needs nothing extra.
The Markdown reader covers what the guides use -- headings, paragraphs, lists,
tables, fenced code, block quotes with GitHub's [!NOTE]-style callouts, code
spans, emphasis and links -- and refuses raw HTML and images rather than
guessing at them. Every link between guides, and every section it names, is
checked, so a renamed heading fails the build instead of shipping a dead link.
"""
from __future__ import annotations

import argparse
import html
import json
import posixpath
import re
import shutil
import sys
from dataclasses import dataclass, field
from pathlib import Path
from urllib.parse import quote, unquote, urlsplit

sys.path.insert(0, str(Path(__file__).resolve().parent))

from fntech3_meta import base_metadata


ROOT = Path(__file__).resolve().parents[1]
THEME_DIR = ROOT / "docs" / "manual"
REPOSITORY_URL = "https://github.com/themuffinator/fnTech3"
MANUAL_DIR_NAME = "manual"


class ManualError(ValueError):
    """A guide uses Markdown this builder does not render, or links nowhere."""


@dataclass(frozen=True)
class Guide:
    source: str
    page: str
    title: str
    summary: str
    icon: str


# Navigation order. Summaries describe what a guide covers, never a game's
# status, which lives in the Games guide alone.
GUIDES: tuple[Guide, ...] = (
    Guide("docs/GETTING_STARTED.md", "getting-started.html", "Getting started",
          "Set fnTech3 up, start your first game and learn the console.", "start"),
    Guide("docs/LAUNCHER.md", "launcher.html", "Launcher",
          "Find the games on your computer and start one with a click.", "launcher"),
    Guide("docs/GAMES.md", "games.html", "Games",
          "Which games run, choosing one, playing online and notes for each game.", "games"),
    Guide("docs/DISPLAY.md", "display.html", "Graphics",
          "Window and fullscreen, sharper edges, lighting, shadows, water, fog and glow.",
          "display"),
    Guide("docs/ASPECT_CORRECTION.md", "widescreen.html", "Widescreen",
          "Field of view, HUD, menus and movies on widescreen and ultrawide screens.",
          "widescreen"),
    Guide("docs/VISUALS.md", "player-highlights.html", "Player highlights",
          "Outline or light up other players so they stand out.", "highlight"),
    Guide("docs/AUDIO.md", "audio.html", "Sound",
          "Headphones and surround, 3D sound, volume, devices and fixing sound problems.",
          "audio"),
    Guide("docs/CONSOLE.md", "console.html", "Console",
          "Typing commands, completion, copy and paste, chat and the console's look.",
          "console"),
    Guide("docs/SCREENSHOTS.md", "screenshots.html", "Screenshots",
          "Taking screenshots, naming them, watermarks, map previews and cube maps.",
          "camera"),
    Guide("docs/GLX.md", "glx.html", "GLx renderer",
          "The default renderer, and what to do when the picture looks wrong.", "chip"),
    Guide("docs/RTX.md", "rtx.html", "RTX ray tracing",
          "Path-traced lighting: what you need, turning it on and tuning it.", "rays"),
)

# Each game's section in the Games guide, with the icon shown beside it.
GAME_LINKS: tuple[tuple[str, str, str], ...] = (
    ("quake-iii-arena-quake3", "quake3", "Quake III Arena"),
    ("quake-live-quakelive", "quakelive", "Quake Live"),
    ("return-to-castle-wolfenstein-rtcw-sp-rtcw-mp", "rtcw-mp", "Return to Castle Wolfenstein"),
    ("wolfenstein-enemy-territory-wolfet", "wolfet", "Enemy Territory"),
    ("star-trek-voyager-elite-force-stvef-sp-stvef-mp", "stvef-sp", "Elite Force"),
    ("star-wars-jedi-knight-jedi-academy-jka-sp-jka-mp", "jka-sp", "Jedi Academy"),
    ("star-wars-jedi-knight-ii-jedi-outcast-jk2-sp-jk2-mp", "jk2-sp", "Jedi Outcast"),
    ("american-mcgees-alice-alice", "alice", "Alice"),
    ("heavy-metal-fakk2-fakk2", "fakk2", "F.A.K.K.2"),
    ("medal-of-honor-allied-assault-mohaa", "mohaa", "Medal of Honor"),
)

THEME_FILES = ("manual.css", "manual.js")
LOGO_SOURCES = {
    "fntech3.png": ROOT / "assets" / "icons" / "fntech3-128.png",
    "fntech3-256.png": ROOT / "assets" / "icons" / "fntech3-256.png",
    "favicon.png": ROOT / "assets" / "icons" / "fntech3-32.png",
}

CODE_LABELS = {
    "": "Example",
    "cfg": "Console or config file",
    "powershell": "PowerShell",
    "ps1": "PowerShell",
    "cmd": "Command Prompt",
    "bat": "Command Prompt",
    "sh": "Terminal",
    "bash": "Terminal",
    "shell": "Terminal",
    "json": "JSON",
    "ini": "INI file",
    "text": "Text",
    "txt": "Text",
}

ALERT_TITLES = {
    "NOTE": "Note",
    "TIP": "Tip",
    "IMPORTANT": "Important",
    "WARNING": "Warning",
    "CAUTION": "Caution",
}

ICONS = {
    "start": '<path d="M8 5.5v13l10.5-6.5z"/>',
    "launcher": ('<rect x="3" y="4" width="18" height="16" rx="2.5"/>'
                 '<path d="M7 9h6M7 13h4M7 17h5"/><path d="M15.5 11v5l4-2.5z"/>'),
    "games": ('<rect x="2.5" y="7" width="19" height="11" rx="5.5"/>'
              '<path d="M7.5 10.5v4M5.5 12.5h4"/><circle cx="15.5" cy="11.5" r="1"/>'
              '<circle cx="18" cy="14" r="1"/>'),
    "display": '<rect x="3" y="4" width="18" height="12" rx="2"/><path d="M8 20h8M12 16v4"/>',
    "widescreen": ('<rect x="2" y="6" width="20" height="12" rx="2"/>'
                   '<path d="M8 10l-2 2 2 2M16 10l2 2-2 2"/>'),
    "highlight": ('<path d="M2.5 12s3.5-6 9.5-6 9.5 6 9.5 6-3.5 6-9.5 6-9.5-6-9.5-6z"/>'
                  '<circle cx="12" cy="12" r="3"/>'),
    "audio": ('<path d="M4 9h4l5-4v14l-5-4H4z"/>'
              '<path d="M16.5 8.5a5 5 0 0 1 0 7M19 6a8.5 8.5 0 0 1 0 12"/>'),
    "console": '<rect x="3" y="4" width="18" height="16" rx="2"/><path d="M7 9l3 3-3 3M12.5 15h4.5"/>',
    "camera": '<path d="M4 8h3l2-3h6l2 3h3v11H4z"/><circle cx="12" cy="13" r="3.5"/>',
    "chip": ('<rect x="6" y="6" width="12" height="12" rx="2"/>'
             '<path d="M9 2.5V6M15 2.5V6M9 18v3.5M15 18v3.5M2.5 9H6M2.5 15H6M18 9h3.5M18 15h3.5"/>'),
    "rays": ('<path d="M12 2.5v4M12 17.5v4M2.5 12h4M17.5 12h4M5.3 5.3l2.8 2.8M15.9 15.9l2.8 2.8'
             'M5.3 18.7l2.8-2.8M15.9 8.1l2.8-2.8"/><circle cx="12" cy="12" r="2.5"/>'),
    "menu": '<path d="M4 7h16M4 12h16M4 17h16"/>',
    "search": '<circle cx="11" cy="11" r="6.5"/><path d="M20 20l-4.2-4.2"/>',
    "sun": ('<circle cx="12" cy="12" r="4"/><path d="M12 2.5v2M12 19.5v2M2.5 12h2M19.5 12h2'
            'M5.3 5.3l1.4 1.4M17.3 17.3l1.4 1.4M5.3 18.7l1.4-1.4M17.3 6.7l1.4-1.4"/>'),
    "moon": '<path d="M20 14.5A8 8 0 1 1 9.5 4a6.5 6.5 0 0 0 10.5 10.5z"/>',
    "note": '<circle cx="12" cy="12" r="9"/><path d="M12 11v5M12 7.5v.5"/>',
    "tip": ('<path d="M9 18h6M10 21h4"/>'
            '<path d="M12 3a6 6 0 0 0-3.6 10.8c.7.5 1.1 1.3 1.1 2.2h5c0-.9.4-1.7 1.1-2.2A6 6 0 0 0 12 3z"/>'),
    "important": '<path d="M4 4h16v12H9l-5 4z"/><path d="M12 7.5V11M12 13.5v.5"/>',
    "warning": '<path d="M12 3.5l9 16H3z"/><path d="M12 10v4M12 16.5v.5"/>',
    "caution": ('<path d="M8.2 3h7.6L21 8.2v7.6L15.8 21H8.2L3 15.8V8.2z"/>'
                '<path d="M12 8v5M12 15.5v.5"/>'),
    "arrow-left": '<path d="M15 5l-7 7 7 7"/>',
    "arrow-right": '<path d="M9 5l7 7-7 7"/>',
    "copy": '<rect x="8" y="8" width="12" height="12" rx="2"/><path d="M16 8V5a1 1 0 0 0-1-1H5a1 1 0 0 0-1 1v10a1 1 0 0 0 1 1h3"/>',
}


def icon_svg(name: str, css_class: str = "icon") -> str:
    return (f'<svg class="{css_class}" viewBox="0 0 24 24" aria-hidden="true" focusable="false">'
            f"{ICONS[name]}</svg>")


# --- Markdown blocks -------------------------------------------------------

_FENCE_RE = re.compile(r"^( {0,3})(`{3,}|~{3,})[ \t]*([^\s`]*)[^`]*$")
_HEADING_RE = re.compile(r"^ {0,3}(#{1,6})(?:[ \t]+(.*?))?(?:[ \t]+#+)?[ \t]*$")
_HR_RE = re.compile(r"^ {0,3}([-*_])(?:[ \t]*\1){2,}[ \t]*$")
_BULLET_RE = re.compile(r"^( {0,3})([-*+])( {1,}|$)")
_ORDERED_RE = re.compile(r"^( {0,3})(\d{1,9})([.)])( {1,}|$)")
_QUOTE_RE = re.compile(r"^ {0,3}> ?")
_TABLE_SEP_RE = re.compile(r"^ {0,3}\|?[ \t]*:?-+:?[ \t]*(?:\|[ \t]*:?-+:?[ \t]*)*\|?[ \t]*$")
_HTML_BLOCK_RE = re.compile(r"^ {0,3}</?[A-Za-z][A-Za-z0-9-]*(?:[\s/>]|$)")
_ALERT_RE = re.compile(r"^\[!(NOTE|TIP|IMPORTANT|WARNING|CAUTION)\][ \t]*$", re.IGNORECASE)


@dataclass
class Block:
    kind: str
    text: str = ""
    level: int = 0
    lang: str = ""
    children: list = field(default_factory=list)
    items: list = field(default_factory=list)
    ordered: bool = False
    start: int = 1
    tight: bool = True
    alert: str = ""
    header: list = field(default_factory=list)
    aligns: list = field(default_factory=list)
    rows: list = field(default_factory=list)


@dataclass(frozen=True)
class ListMarker:
    ordered: bool
    char: str
    start: int
    indent: int
    content_indent: int
    rest: str


def _leading_spaces(line: str) -> int:
    return len(line) - len(line.lstrip(" "))


def _list_marker(line: str) -> ListMarker | None:
    match = _BULLET_RE.match(line)
    ordered = False
    if match:
        indent = len(match.group(1))
        marker_width = 1
        char = match.group(2)
        start = 1
        spaces = len(match.group(3))
    else:
        match = _ORDERED_RE.match(line)
        if not match:
            return None
        ordered = True
        indent = len(match.group(1))
        marker_width = len(match.group(2)) + 1
        char = match.group(3)
        start = int(match.group(2))
        spaces = len(match.group(4))
    if spaces == 0 or spaces > 4:
        spaces = 1
    content_indent = indent + marker_width + spaces
    rest = line[content_indent:] if len(line) >= content_indent else ""
    return ListMarker(ordered, char, start, indent, content_indent, rest)


def _starts_block(line: str) -> bool:
    return bool(
        _FENCE_RE.match(line)
        or _HEADING_RE.match(line)
        or _HR_RE.match(line)
        or _QUOTE_RE.match(line)
        or _list_marker(line)
    )


def _split_row(line: str) -> list[str]:
    text = line.strip()
    if text.startswith("|"):
        text = text[1:]
    if text.endswith("|") and not text.endswith("\\|"):
        text = text[:-1]
    cells: list[str] = []
    current: list[str] = []
    index = 0
    while index < len(text):
        char = text[index]
        if char == "\\" and index + 1 < len(text) and text[index + 1] == "|":
            current.append("|")
            index += 2
            continue
        if char == "|":
            cells.append("".join(current).strip())
            current = []
        else:
            current.append(char)
        index += 1
    cells.append("".join(current).strip())
    return cells


def _table_aligns(line: str) -> list[str]:
    aligns = []
    for cell in _split_row(line):
        cell = cell.strip()
        if cell.startswith(":") and cell.endswith(":"):
            aligns.append("center")
        elif cell.endswith(":"):
            aligns.append("right")
        else:
            aligns.append("")
    return aligns


def parse_blocks(lines: list[str], *, source: str) -> list[Block]:
    blocks: list[Block] = []
    index = 0
    count = len(lines)
    while index < count:
        line = lines[index]
        if not line.strip():
            index += 1
            continue

        fence = _FENCE_RE.match(line)
        if fence:
            indent = len(fence.group(1))
            marker = fence.group(2)
            body: list[str] = []
            index += 1
            closed = False
            while index < count:
                candidate = lines[index]
                stripped = candidate.strip()
                if stripped.startswith(marker[0] * len(marker)) and not stripped.strip(marker[0]):
                    closed = True
                    index += 1
                    break
                body.append(candidate[min(indent, _leading_spaces(candidate)):])
                index += 1
            if not closed:
                raise ManualError(f"{source}: unclosed code fence")
            blocks.append(Block("code", text="\n".join(body), lang=fence.group(3).lower()))
            continue

        heading = _HEADING_RE.match(line)
        if heading:
            blocks.append(Block("heading", text=(heading.group(2) or "").strip(),
                                level=len(heading.group(1))))
            index += 1
            continue

        if _HR_RE.match(line):
            blocks.append(Block("hr"))
            index += 1
            continue

        if _HTML_BLOCK_RE.match(line):
            raise ManualError(f"{source}: raw HTML is not rendered: {line.strip()[:60]}")

        if _QUOTE_RE.match(line):
            quoted: list[str] = []
            while index < count and _QUOTE_RE.match(lines[index]):
                quoted.append(_QUOTE_RE.sub("", lines[index], count=1))
                index += 1
            alert = ""
            if quoted and _ALERT_RE.match(quoted[0].strip()):
                alert = _ALERT_RE.match(quoted[0].strip()).group(1).upper()
                quoted = quoted[1:]
            blocks.append(Block("quote", alert=alert,
                                children=parse_blocks(quoted, source=source)))
            continue

        marker = _list_marker(line)
        if marker:
            block, index = _parse_list(lines, index, source=source)
            blocks.append(block)
            continue

        if (index + 1 < count and "|" in line and "|" in lines[index + 1]
                and _TABLE_SEP_RE.match(lines[index + 1])):
            header = _split_row(line)
            aligns = _table_aligns(lines[index + 1])
            if len(aligns) != len(header):
                raise ManualError(f"{source}: table separator does not match its header: {line.strip()[:60]}")
            rows = []
            index += 2
            while index < count and lines[index].strip() and "|" in lines[index]:
                cells = _split_row(lines[index])
                cells = (cells + [""] * len(header))[:len(header)]
                rows.append(cells)
                index += 1
            blocks.append(Block("table", header=header, aligns=aligns, rows=rows))
            continue

        if line.startswith("    ") or line.startswith("\t"):
            body = []
            while index < count and (not lines[index].strip()
                                     or lines[index].startswith("    ")):
                body.append(lines[index][4:])
                index += 1
            while body and not body[-1].strip():
                body.pop()
            blocks.append(Block("code", text="\n".join(body)))
            continue

        paragraph = [line.strip()]
        index += 1
        while index < count:
            candidate = lines[index]
            if not candidate.strip() or _starts_block(candidate) or _HTML_BLOCK_RE.match(candidate):
                break
            paragraph.append(candidate.strip())
            index += 1
        blocks.append(Block("paragraph", text="\n".join(paragraph)))
    return blocks


def _parse_list(lines: list[str], index: int, *, source: str) -> tuple[Block, int]:
    first = _list_marker(lines[index])
    assert first is not None
    items: list[list[Block]] = []
    loose = False
    count = len(lines)
    marker = first
    while True:
        item_lines = [marker.rest]
        content_indent = marker.content_indent
        index += 1
        open_fence = bool(_FENCE_RE.match(marker.rest))
        while index < count:
            line = lines[index]
            if not line.strip():
                item_lines.append("")
                index += 1
                continue
            if _leading_spaces(line) >= content_indent:
                item_lines.append(line[content_indent:])
                if _FENCE_RE.match(line[content_indent:]):
                    open_fence = not open_fence
                index += 1
                continue
            if _list_marker(line):
                break
            if item_lines[-1] != "" and not open_fence and not _starts_block(line):
                item_lines.append(line.strip())
                index += 1
                continue
            break
        trailing_blank = 0
        while item_lines and item_lines[-1] == "":
            item_lines.pop()
            trailing_blank += 1
        if "" in item_lines and not _blank_lines_only_in_code(item_lines):
            loose = True
        items.append(parse_blocks(item_lines, source=source))
        if index >= count:
            break
        following = _list_marker(lines[index])
        if (not following or following.ordered != first.ordered or following.char != first.char
                or following.indent >= content_indent):
            break
        if trailing_blank:
            loose = True
        marker = following
    return Block("list", items=items, ordered=first.ordered, start=first.start, tight=not loose), index


def _blank_lines_only_in_code(lines: list[str]) -> bool:
    in_fence = False
    for line in lines:
        if _FENCE_RE.match(line):
            in_fence = not in_fence
            continue
        if line == "" and not in_fence:
            return False
    return True


# --- Inline text -----------------------------------------------------------

_ESCAPABLE = set("\\`*_{}[]()#+-.!|<>~\"'")
_STRONG_STAR_RE = re.compile(r"\*\*(?=[^\s*])(.+?)(?<=[^\s*])\*\*", re.S)
_STRONG_UNDER_RE = re.compile(r"(?<!\w)__(?=\S)(.+?)(?<=\S)__(?!\w)", re.S)
_EM_STAR_RE = re.compile(r"(?<![\w*])\*(?=[^\s*])(.+?)(?<=[^\s*])\*(?![\w*])", re.S)
_EM_UNDER_RE = re.compile(r"(?<!\w)_(?=\S)(.+?)(?<=\S)_(?!\w)", re.S)
_DASH_RE = re.compile(r"(?<=\s)--(?=\s)")
_PLACEHOLDER_RE = re.compile("\x00(\\d+)\x00")
_AUTOLINK_RE = re.compile(r"<(https?://[^\s<>]+)>")
_TAG_RE = re.compile(r"<[^>]+>")
_TERM_RE = re.compile(r"^[A-Za-z_+][\w.+-]*$")


def github_slug(text: str) -> str:
    """GitHub's heading anchor for a heading's plain text."""
    slug = re.sub(r"[^\w\- ]", "", text.strip().lower())
    return slug.replace(" ", "-")


@dataclass
class PageContext:
    guide: Guide
    anchors: set[str] = field(default_factory=set)
    slug_counts: dict[str, int] = field(default_factory=dict)
    headings: list[tuple[int, str, str]] = field(default_factory=list)
    title: str = ""
    # Setting and command names each section mentions, for the search box.
    section: str = ""
    terms: dict[str, list[str]] = field(default_factory=dict)


class Renderer:
    def __init__(self, guides: tuple[Guide, ...], *, root: Path = ROOT) -> None:
        self.root = root
        self.guides = guides
        self.by_source = {guide.source: guide for guide in guides}
        self.links: list[tuple[str, str, str, str]] = []

    # Links between guides are checked once every page has its anchors.
    def resolve_href(self, href: str, context: PageContext) -> tuple[str, bool]:
        parts = urlsplit(href)
        if parts.scheme in ("http", "https", "mailto"):
            return href, True
        if parts.scheme or parts.netloc:
            raise ManualError(f"{context.guide.source}: unsupported link {href}")
        fragment = parts.fragment
        if not parts.path:
            self.links.append((context.guide.source, context.guide.page, fragment, href))
            return f"#{fragment}", False
        base = posixpath.dirname(context.guide.source)
        target = posixpath.normpath(posixpath.join(base, unquote(parts.path)))
        if target.startswith("../") or target == "..":
            raise ManualError(f"{context.guide.source}: link leaves the repository: {href}")
        guide = self.by_source.get(target)
        if guide:
            self.links.append((context.guide.source, guide.page, fragment, href))
            return guide.page + (f"#{fragment}" if fragment else ""), False
        path = self.root / target
        if not path.exists():
            raise ManualError(f"{context.guide.source}: link to a missing file: {href}")
        kind = "tree" if path.is_dir() else "blob"
        url = f"{REPOSITORY_URL}/{kind}/main/{quote(target)}"
        return url + (f"#{fragment}" if fragment else ""), True

    def inline(self, text: str, context: PageContext, *, typography: bool = True) -> str:
        held: list[str] = []

        def hold(fragment: str) -> str:
            held.append(fragment)
            return f"\x00{len(held) - 1}\x00"

        plain: list[str] = []
        index = 0
        length = len(text)
        while index < length:
            char = text[index]
            if char == "\\" and index + 1 < length and text[index + 1] in _ESCAPABLE:
                plain.append(hold(html.escape(text[index + 1], quote=False)))
                index += 2
                continue
            if char == "`":
                run = len(text[index:]) - len(text[index:].lstrip("`"))
                close = self._closing_backticks(text, index + run, run)
                if close >= 0:
                    code = text[index + run:close].replace("\n", " ")
                    if len(code) > 1 and code.startswith(" ") and code.endswith(" ") and code.strip():
                        code = code[1:-1]
                    if typography:
                        self._remember_term(code, context)
                    plain.append(hold(f"<code>{html.escape(code, quote=False)}</code>"))
                    index = close + run
                    continue
                plain.append("`" * run)
                index += run
                continue
            if char == "!" and text.startswith("[", index + 1) and self._parse_link(text, index + 1):
                raise ManualError(f"{context.guide.source}: images are not rendered: {text[index:index + 40]}")
            if char == "[":
                link = self._parse_link(text, index)
                if link:
                    label, destination, title, end = link
                    href, external = self.resolve_href(destination, context)
                    attributes = f' href="{html.escape(href)}"'
                    if title:
                        attributes += f' title="{html.escape(title)}"'
                    if external:
                        attributes += ' class="external" rel="noopener"'
                    plain.append(hold(f"<a{attributes}>{self.inline(label, context, typography=typography)}</a>"))
                    index = end
                    continue
            if char == "<":
                match = _AUTOLINK_RE.match(text, index)
                if match:
                    url = match.group(1)
                    plain.append(hold(f'<a href="{html.escape(url)}" class="external" rel="noopener">'
                                      f"{html.escape(url, quote=False)}</a>"))
                    index = match.end()
                    continue
            plain.append(char)
            index += 1

        rendered = html.escape("".join(plain), quote=False)
        if typography:
            rendered = _DASH_RE.sub("\u2013", rendered)
        rendered = _STRONG_STAR_RE.sub(r"<strong>\1</strong>", rendered)
        rendered = _STRONG_UNDER_RE.sub(r"<strong>\1</strong>", rendered)
        rendered = _EM_STAR_RE.sub(r"<em>\1</em>", rendered)
        rendered = _EM_UNDER_RE.sub(r"<em>\1</em>", rendered)
        while "\x00" in rendered:
            rendered = _PLACEHOLDER_RE.sub(lambda match: held[int(match.group(1))], rendered)
        return rendered

    @staticmethod
    def _remember_term(code: str, context: PageContext) -> None:
        words = code.split()
        term = words[0] if words else ""
        if len(term) < 3 or len(term) > 48 or not _TERM_RE.match(term):
            return
        terms = context.terms.setdefault(context.section, [])
        if term not in terms:
            terms.append(term)

    @staticmethod
    def _closing_backticks(text: str, start: int, run: int) -> int:
        index = start
        while True:
            index = text.find("`" * run, index)
            if index < 0:
                return -1
            end = index + run
            before = text[index - 1] if index > 0 else ""
            after = text[end] if end < len(text) else ""
            if before != "`" and after != "`":
                return index
            index = end + len(text[end:]) - len(text[end:].lstrip("`"))

    def _parse_link(self, text: str, start: int):
        depth = 0
        index = start
        length = len(text)
        while index < length:
            char = text[index]
            if char == "\\":
                index += 2
                continue
            if char == "`":
                run = len(text[index:]) - len(text[index:].lstrip("`"))
                close = self._closing_backticks(text, index + run, run)
                index = close + run if close >= 0 else index + run
                continue
            if char == "[":
                depth += 1
            elif char == "]":
                depth -= 1
                if depth == 0:
                    break
            index += 1
        if index >= length or depth != 0 or index + 1 >= length or text[index + 1] != "(":
            return None
        label = text[start + 1:index]
        cursor = index + 2
        depth = 1
        begin = cursor
        while cursor < length:
            char = text[cursor]
            if char == "\\":
                cursor += 2
                continue
            if char == "(":
                depth += 1
            elif char == ")":
                depth -= 1
                if depth == 0:
                    break
            cursor += 1
        if cursor >= length:
            return None
        inner = text[begin:cursor].strip()
        match = re.match(r'^(<[^>]*>|\S+)(?:\s+"([^"]*)")?$', inner, re.S)
        if not match:
            return None
        destination = match.group(1)
        if destination.startswith("<"):
            destination = destination[1:-1]
        return label, destination, match.group(2), cursor + 1

    def heading_id(self, text: str, context: PageContext) -> str:
        plain = html.unescape(_TAG_RE.sub("", self.inline(text, context, typography=False)))
        slug = github_slug(plain) or "section"
        count = context.slug_counts.get(slug, 0)
        context.slug_counts[slug] = count + 1
        if count:
            slug = f"{slug}-{count}"
        context.anchors.add(slug)
        return slug

    def blocks(self, blocks: list[Block], context: PageContext, *, tight: bool = False) -> str:
        out: list[str] = []
        for block in blocks:
            if block.kind == "heading":
                out.append(self.heading(block, context))
            elif block.kind == "paragraph":
                text = self.inline(block.text, context)
                out.append(text if tight else f"<p>{text}</p>")
            elif block.kind == "code":
                out.append(self.code(block))
            elif block.kind == "hr":
                out.append("<hr>")
            elif block.kind == "quote":
                out.append(self.quote(block, context))
            elif block.kind == "list":
                out.append(self.list(block, context))
            elif block.kind == "table":
                out.append(self.table(block, context))
            else:
                raise ManualError(f"{context.guide.source}: unknown block {block.kind}")
        return "\n".join(out)

    def heading(self, block: Block, context: PageContext) -> str:
        if not block.text:
            raise ManualError(f"{context.guide.source}: empty heading")
        anchor = self.heading_id(block.text, context)
        if block.level <= 3:
            context.section = anchor if block.level > 1 else ""
        content = self.inline(block.text, context)
        if block.level == 1:
            if context.title:
                raise ManualError(f"{context.guide.source}: more than one top-level heading")
            context.title = content
            return ""
        context.headings.append((block.level, anchor, content))
        decoration = ""
        if context.guide.page == "games.html":
            for game_anchor, icon, _name in GAME_LINKS:
                if game_anchor == anchor:
                    decoration = (f'<img class="game-icon" src="assets/games/{icon}-64.png" '
                                  f'srcset="assets/games/{icon}-128.png 2x" width="40" height="40" alt="">')
        return (f'<h{block.level} id="{anchor}">{decoration}<span>{content}</span>'
                f'<a class="heading-link" href="#{anchor}" aria-label="Link to this section">#</a>'
                f"</h{block.level}>")

    @staticmethod
    def code(block: Block) -> str:
        label = CODE_LABELS.get(block.lang, block.lang.upper() or "Example")
        body = html.escape(block.text, quote=False)
        return ('<div class="code-block">'
                f'<div class="code-head"><span class="code-lang">{html.escape(label)}</span>'
                '<button type="button" class="copy-button" aria-label="Copy to the clipboard">'
                f'{icon_svg("copy")}<span>Copy</span></button></div>'
                f"<pre><code>{body}</code></pre></div>")

    def quote(self, block: Block, context: PageContext) -> str:
        inner = self.blocks(block.children, context)
        if not block.alert:
            return f"<blockquote>{inner}</blockquote>"
        kind = block.alert.lower()
        return (f'<aside class="callout callout-{kind}" role="note">'
                f'<p class="callout-title">{icon_svg(kind)}{ALERT_TITLES[block.alert]}</p>'
                f"{inner}</aside>")

    def list(self, block: Block, context: PageContext) -> str:
        tag = "ol" if block.ordered else "ul"
        start = f' start="{block.start}"' if block.ordered and block.start != 1 else ""
        items = "".join(f"<li>{self.blocks(item, context, tight=block.tight)}</li>"
                        for item in block.items)
        return f"<{tag}{start}>{items}</{tag}>"

    def table(self, block: Block, context: PageContext) -> str:
        def cell(tag: str, text: str, align: str) -> str:
            style = f' class="align-{align}"' if align else ""
            return f"<{tag}{style}>{self.inline(text, context)}</{tag}>"

        head = "".join(cell("th", text, align) for text, align in zip(block.header, block.aligns))
        rows = "".join(
            "<tr>" + "".join(cell("td", text, align) for text, align in zip(row, block.aligns)) + "</tr>"
            for row in block.rows
        )
        return (f'<div class="table-wrap"><table><thead><tr>{head}</tr></thead>'
                f"<tbody>{rows}</tbody></table></div>")


# --- Pages -----------------------------------------------------------------

@dataclass
class RenderedGuide:
    guide: Guide
    title: str
    body: str
    headings: list[tuple[int, str, str]]
    anchors: set[str]
    terms: dict[str, list[str]]


def render_guide(renderer: Renderer, guide: Guide) -> RenderedGuide:
    path = renderer.root / guide.source
    text = path.read_text(encoding="utf-8").replace("\r\n", "\n").expandtabs(4)
    if not text.startswith("# "):
        raise ManualError(f"{guide.source}: a guide starts with its # title")
    context = PageContext(guide)
    body = renderer.blocks(parse_blocks(text.split("\n"), source=guide.source), context)
    return RenderedGuide(guide, context.title, body, context.headings, context.anchors,
                         context.terms)


def _sidebar(guides: tuple[Guide, ...], current: str) -> str:
    entries = []
    home_class = ' class="current" aria-current="page"' if current == "index.html" else ""
    entries.append(f'<li><a href="index.html"{home_class}>'
                   f'<img class="nav-logo" src="assets/favicon.png" width="18" height="18" alt="">'
                   "Guide home</a></li>")
    for guide in guides:
        css = ' class="current" aria-current="page"' if guide.page == current else ""
        entries.append(f'<li><a href="{guide.page}"{css}>{icon_svg(guide.icon)}'
                       f"{html.escape(guide.title)}</a></li>")
    return ('<nav class="sidebar" id="sidebar" aria-label="Guides">'
            '<p class="sidebar-label">Guides</p>'
            f'<ul class="guide-nav">{"".join(entries)}</ul></nav>')


def _toc(headings: list[tuple[int, str, str]]) -> str:
    items = []
    for level, anchor, content in headings:
        if level > 3:
            continue
        css = "toc-sub" if level == 3 else "toc-main"
        items.append(f'<li class="{css}"><a href="#{anchor}">{_TAG_RE.sub("", content)}</a></li>')
    if not items:
        return ""
    return ('<aside class="toc" aria-label="On this page"><p class="toc-label">On this page</p>'
            f'<ul>{"".join(items)}</ul></aside>')


def _shell(*, title: str, description: str, page: str, guides: tuple[Guide, ...],
           main: str, toc: str, meta: dict[str, object]) -> str:
    version = html.escape(str(meta["version"]))
    name = html.escape(str(meta["project_name_short"]))
    body_class = "page-home" if page == "index.html" else "page-guide"
    return f"""<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<meta name="color-scheme" content="dark light">
<meta name="description" content="{html.escape(description)}">
<title>{html.escape(title)}</title>
<link rel="icon" type="image/png" href="assets/favicon.png">
<link rel="stylesheet" href="assets/manual.css">
<script>try{{var t=localStorage.getItem("fntech3-manual-theme");if(t==="light"||t==="dark")document.documentElement.dataset.theme=t;}}catch(e){{}}document.documentElement.classList.add("js");</script>
</head>
<body class="{body_class}">
<a class="skip-link" href="#content">Skip to the guide</a>
<header class="topbar">
<button type="button" class="icon-button nav-toggle" aria-controls="sidebar" aria-expanded="false" aria-label="Show the guides">{icon_svg("menu")}</button>
<a class="brand" href="index.html"><img src="assets/fntech3.png" width="34" height="34" alt=""><span class="brand-name">{name}</span><span class="brand-sub">Player Guide</span></a>
<div class="search" role="search">
{icon_svg("search", "icon search-icon")}<input type="search" id="search-input" placeholder="Search the guide" aria-label="Search the guide" autocomplete="off" spellcheck="false">
<ul class="search-results" id="search-results" role="listbox" hidden></ul>
</div>
<button type="button" class="icon-button theme-toggle" aria-label="Switch between light and dark">{icon_svg("sun", "icon icon-sun")}{icon_svg("moon", "icon icon-moon")}</button>
</header>
<div class="layout">
{_sidebar(guides, page)}
<main id="content" class="content" tabindex="-1">
{main}
</main>
{toc}
</div>
<footer class="site-footer">
<p><img src="assets/favicon.png" width="16" height="16" alt=""> {name} {version} &middot; Player Guide</p>
<p>{name} is engine-only: it plays the games you already own and never includes their files. Source code, issues and the full documentation are on <a class="external" rel="noopener" href="{REPOSITORY_URL}">GitHub</a>.</p>
</footer>
<script src="assets/search-index.js"></script>
<script src="assets/manual.js"></script>
</body>
</html>
"""


def _guide_page(rendered: RenderedGuide, index: int, guides: tuple[Guide, ...],
                meta: dict[str, object]) -> str:
    guide = rendered.guide
    previous = guides[index - 1] if index > 0 else None
    following = guides[index + 1] if index + 1 < len(guides) else None
    pager = []
    if previous:
        pager.append(f'<a class="pager-prev" href="{previous.page}">{icon_svg("arrow-left")}'
                     f'<span><small>Previous</small>{html.escape(previous.title)}</span></a>')
    else:
        pager.append('<a class="pager-prev" href="index.html">'
                     f'{icon_svg("arrow-left")}<span><small>Back to</small>Guide home</span></a>')
    if following:
        pager.append(f'<a class="pager-next" href="{following.page}"><span><small>Next</small>'
                     f'{html.escape(following.title)}</span>{icon_svg("arrow-right")}</a>')
    main = (f'<article class="guide">'
            f'<header class="guide-header"><p class="eyebrow">{icon_svg(guide.icon)}'
            f'{html.escape(guide.title)}</p><h1>{rendered.title}</h1></header>'
            f'<div class="guide-body">{rendered.body}</div>'
            f'<nav class="pager" aria-label="More guides">{"".join(pager)}</nav>'
            f"</article>")
    title = f"{_TAG_RE.sub('', rendered.title)} \u2013 {meta['project_name_short']} Player Guide"
    return _shell(title=html.unescape(title), description=guide.summary, page=guide.page,
                  guides=guides, main=main, toc=_toc(rendered.headings), meta=meta)


def _index_page(guides: tuple[Guide, ...], meta: dict[str, object]) -> str:
    name = html.escape(str(meta["project_name_short"]))
    first, rest = guides[0], guides[1:]
    cards = "".join(
        f'<a class="guide-card" href="{guide.page}">{icon_svg(guide.icon)}'
        f"<strong>{html.escape(guide.title)}</strong>"
        f"<span>{html.escape(guide.summary)}</span></a>"
        for guide in rest
    )
    games_page = next(guide.page for guide in guides if guide.source == "docs/GAMES.md")
    games = "".join(
        f'<a class="game-tile" href="{games_page}#{anchor}">'
        f'<img src="assets/games/{icon}-64.png" srcset="assets/games/{icon}-128.png 2x" '
        f'width="56" height="56" alt=""><span>{html.escape(label)}</span></a>'
        for anchor, icon, label in GAME_LINKS
    )
    main = f"""<div class="home">
<section class="hero">
<img class="hero-logo" src="assets/fntech3-256.png" width="148" height="148" alt="The {name} flame">
<div class="hero-text">
<p class="eyebrow">Player Guide &middot; version {html.escape(str(meta["version"]))}</p>
<h1>Classic games, modern engine.</h1>
<p class="hero-lede">{name} is a free engine that runs Quake III Arena, Quake Live and other classics from the id Tech 3 era on today's computers &ndash; with sharper graphics, 3D sound and widescreen support &ndash; using the games you already own.</p>
<div class="hero-actions">
<a class="button button-primary" href="{first.page}">{icon_svg(first.icon)}Get started</a>
<a class="button button-secondary" href="{games_page}">{icon_svg("games")}See the games</a>
</div>
</div>
</section>
<section class="home-section" aria-labelledby="home-games">
<h2 id="home-games">Your games</h2>
<p class="section-lede">Pick a game to see whether it is ready to play, what you need and what to expect.</p>
<div class="game-grid">{games}</div>
</section>
<section class="home-section" aria-labelledby="home-guides">
<h2 id="home-guides">Guides</h2>
<p class="section-lede">Short, practical guides to every part of {name}. Each one starts with the basics and keeps the fine-tuning for later.</p>
<div class="card-grid">{cards}</div>
</section>
<section class="home-section home-notes" aria-labelledby="home-notes">
<h2 id="home-notes">Good to know</h2>
<div class="note-grid">
<div class="note"><strong>Bring your own games</strong><span>{name} contains no game files. It plays the copy you installed from Steam, GOG or a disc, and never changes it.</span></div>
<div class="note"><strong>Settings are typed in the console</strong><span>Most options are short commands you type in the drop-down console or save in a config file. <a href="{first.page}#the-console-and-settings">Here is how.</a></span></div>
<div class="note"><strong>Still in development</strong><span>Some games are only partly working. The <a href="{games_page}">Games guide</a> says honestly what works today.</span></div>
</div>
</section>
</div>"""
    return _shell(title=f"{meta['project_name_short']} Player Guide",
                  description=f"How to set up and play games with {meta['project_name_short']}.",
                  page="index.html", guides=guides, main=main, toc="", meta=meta)


def _search_index(rendered: list[RenderedGuide]) -> str:
    entries = []
    for item in rendered:
        guide_title = item.guide.title
        entries.append({"t": html.unescape(_TAG_RE.sub("", item.title)), "g": guide_title,
                        "h": item.guide.page, "k": " ".join(item.terms.get("", []))})
        for level, anchor, content in item.headings:
            if level > 3:
                continue
            entries.append({"t": html.unescape(_TAG_RE.sub("", content)), "g": guide_title,
                            "h": f"{item.guide.page}#{anchor}",
                            "k": " ".join(item.terms.get(anchor, []))})
    payload = json.dumps(entries, ensure_ascii=True, separators=(",", ":"))
    return ("// Generated by scripts/player_manual.py: each guide section's heading and the\n"
            "// setting and command names it mentions, for the search box.\n"
            f"window.FNTECH3_MANUAL_INDEX = {payload};\n")


def _check_links(renderer: Renderer, rendered: list[RenderedGuide]) -> None:
    anchors = {item.guide.page: item.anchors for item in rendered}
    problems = []
    for source, page, fragment, href in renderer.links:
        if page not in anchors:
            problems.append(f"{source}: {href} names a page that is not built")
        elif fragment and fragment not in anchors[page]:
            problems.append(f"{source}: {href} names a section that does not exist")
    games = next((item for item in rendered if item.guide.source == "docs/GAMES.md"), None)
    if games:
        for anchor, _icon, label in GAME_LINKS:
            if anchor not in games.anchors:
                problems.append(f"docs/GAMES.md: no section #{anchor} for {label}")
    if problems:
        raise ManualError("Broken links in the player guides:\n  " + "\n  ".join(problems))


def _write(path: Path, content: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content, encoding="utf-8", newline="\n")


def manual_entries(guides: tuple[Guide, ...] = GUIDES) -> list[str]:
    """Every file the manual folder holds, relative to the package root."""
    names = ["index.html", *(guide.page for guide in guides),
             *(f"assets/{name}" for name in THEME_FILES), "assets/search-index.js",
             *(f"assets/{name}" for name in LOGO_SOURCES)]
    for _anchor, icon, _label in GAME_LINKS:
        names.extend((f"assets/games/{icon}-64.png", f"assets/games/{icon}-128.png"))
    return sorted(f"{MANUAL_DIR_NAME}/{name}" for name in names)


def build_manual(output: Path, *, guides: tuple[Guide, ...] = GUIDES, root: Path = ROOT,
                 meta: dict[str, object] | None = None) -> list[Path]:
    """Write the manual into output, replacing what was there; return its files."""
    meta = meta or base_metadata()
    renderer = Renderer(guides, root=root)
    rendered = [render_guide(renderer, guide) for guide in guides]
    _check_links(renderer, rendered)

    if output.exists():
        shutil.rmtree(output)
    _write(output / "index.html", _index_page(guides, meta))
    for index, item in enumerate(rendered):
        _write(output / item.guide.page, _guide_page(item, index, guides, meta))
    assets = output / "assets"
    for name in THEME_FILES:
        _write(assets / name, (THEME_DIR / name).read_text(encoding="utf-8").replace("\r\n", "\n"))
    _write(assets / "search-index.js", _search_index(rendered))
    for name, source in LOGO_SOURCES.items():
        shutil.copyfile(source, assets / name)
    (assets / "games").mkdir(parents=True, exist_ok=True)
    for _anchor, icon, _label in GAME_LINKS:
        for size in (64, 128):
            shutil.copyfile(root / "assets" / "icons" / "games" / icon / f"{icon}-{size}.png",
                            assets / "games" / f"{icon}-{size}.png")
    return sorted(path for path in output.rglob("*") if path.is_file())


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Build the HTML edition of the player guides.")
    parser.add_argument("--output", type=Path, default=ROOT / ".tmp" / "manual",
                        help="folder to write; it is replaced (default: .tmp/manual)")
    args = parser.parse_args(argv)
    try:
        files = build_manual(args.output.resolve())
    except ManualError as exc:
        print(f"player_manual.py: {exc}", file=sys.stderr)
        return 1
    print(f"Wrote {len(files)} files to {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
