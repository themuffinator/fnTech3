# Player Guide (HTML edition)

Release packages include the player guides as a set of web pages in their
`manual` folder, styled in the colours of the fnTech3 flame. Players open
`manual/index.html`, or follow the links from the package's `README.html`; the
pages work offline in any browser.

The Markdown guides in [`docs/`](../README.md#player-guides) are the only
source of the text. [`scripts/player_manual.py`](../../scripts/player_manual.py)
renders them, and this folder holds the two files it adds unchanged:

- [`manual.css`](manual.css) -- the stylesheet: the flame's orange `#fc5c00`
  on black, with the social banner's ember red, a matching light theme, and
  layouts for phones and print;
- [`manual.js`](manual.js) -- the theme switch, the guide drawer on narrow
  screens, copy buttons on examples, the "On this page" highlight and the
  heading search.

The pages read fully without the script.

## Building it

```sh
python scripts/player_manual.py --output .tmp/manual
```

writes a preview to `.tmp/manual`. The release packager builds the same pages
into every package (`copy_docs` in [`scripts/release.py`](../../scripts/release.py)),
and refuses to package if a guide links to a page or section that does not
exist.

## Writing for it

- Add a guide to `GUIDES` in `scripts/player_manual.py` to put it in the
  navigation, with a one-line summary of what it covers. Keep game status out
  of summaries; it lives in the Games guide.
- The reader understands headings, paragraphs, lists, tables, fenced code,
  block quotes, GitHub's `> [!NOTE]`, `> [!TIP]`, `> [!IMPORTANT]`,
  `> [!WARNING]` and `> [!CAUTION]` callouts, code spans, emphasis and links.
  It refuses raw HTML and images rather than guessing at them.
- Links to another guide become links between the pages, and every section a
  link names must exist. Links to any other file in the repository open it on
  GitHub.
- Write for players: what a feature does, how to turn it on, and what to try
  when something goes wrong. Explain a setting the first time it appears, and
  keep technical evidence in the [compatibility ledger](../fntech3/COMPATIBILITY.md)
  and its records.
