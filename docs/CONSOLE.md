# Console Guide

The console is the drop-down window where you type commands and change
settings. fnTech3 gives every game the same improved console -- it scales to
your screen, scrolls smoothly, suggests commands as you type, and lets you
select, copy and paste -- drawn in each game's own font and style. This guide
covers how to use it and how to change its look.

Press the key **below Esc**, to the left of **1** (`~` on US keyboards), or
**Shift+Esc**, to open and close it. [Getting Started](GETTING_STARTED.md#the-console-and-settings)
explains how settings work.

## Typing commands

- Type a command or a setting and press **Enter**.
- Commands do not need a slash in front. To chat from the console, type `say`
  and your message, or turn on [console chat](#chatting-from-the-console).
- **Up** and **Down** bring back earlier commands.
- **Page Up** and **Page Down**, or the mouse wheel, scroll through earlier
  messages; **Ctrl+Page Up** and **Ctrl+Page Down** scroll a whole page.

### Suggestions as you type

As you type, a list of matching commands, settings and -- for commands that
load a map -- map names appears under the line.

- **Tab** takes the highlighted suggestion; press it again for the next one.
- **Shift+Tab** goes back to the previous one.
- Click a suggestion to use it.
- `con_completionPopup 0` turns the list off, and **Tab** then completes the
  classic way, printing the matches in the console.

## Selecting, copying and pasting

You can select text in the line you are typing and in the messages above it,
which makes it easy to reuse a command or copy an error.

- Drag with the mouse over the typing line or the messages to select text.
- Drag selected text into the typing line to move or reuse it.
- **Ctrl+C** copies, **Ctrl+V** or **Shift+Insert** pastes, and **Ctrl+X**
  cuts from the typing line.
- **Ctrl+A** selects the whole typing line, or every message when you are in
  the messages.
- **Shift** with the arrow keys, **Home** and **End** selects in the typing
  line, and **Ctrl+Shift+Left** and **Right** select a word at a time.
- In the messages, select with **Ctrl+Shift** and the arrows, **Page Up**,
  **Page Down**, **Home** and **End**.

Copy and paste use your computer's clipboard, so you can paste into other
programs and back.

## The mouse in the console

The mouse stays inside the game while the console is open, as an in-game
cursor, and goes back to controlling the game when you close it. A thin
scroll bar appears when there are more messages than fit; drag it to scroll,
and it widens when you point at it.

## Size and layout

- `con_scale`: the size of the console's text. The default is `1`.
- `con_scaleUniform`: `1` (the default) scales the text with your screen's
  resolution, so it looks the same size on every monitor; `0` keeps it at a
  fixed size in pixels.
- `con_screenExtents`: `0` (the default) uses the full width of the screen;
  `1` keeps the whole console in a centred 4:3 area.

The console rearranges itself straight away when you change these or the
screen size.

## Scrolling and opening

- `con_speed`: how quickly the console opens and closes.
- `con_scrollLines`: how many lines one scroll step moves.
- `con_scrollSmooth`: `1` (the default) scrolls smoothly, including when new
  messages push older ones up; `0` moves at once.
- `con_scrollSmoothSpeed`: how fast smooth scrolling is, in lines per second.
- `con_fade`: `1` fades the console in and out as it opens and closes; `0`
  (the default) does not.

## Appearance

- `con_backgroundStyle`: `1` (the default in every game) a plain shaded
  background; `0` the game's own textured console background.
- `con_backgroundColor`: the background's colour, as `R G B` from `0` to
  `255`.
- `con_backgroundOpacity`: how see-through the background is, from `0` to
  `1`.
- `con_lineColor`: the colour of the dividing line and scroll markers.
- `con_versionColor`: the colour of the version and clock text.
- `con_showVersion`: `1` (the default) shows fnTech3's version in the corner;
  `0` hides it.
- `con_showClock`: `1` shows the time, 24-hour; `2` 12-hour; `0` (the
  default) hides it.

The older `cl_conColor` setting still tints the background when
`con_backgroundColor` is empty.

**Each game's look.** Each game's console uses its own font: Quake III Arena,
Return to Castle Wolfenstein single player and F.A.K.K.2 their standard
characters, Return to Castle Wolfenstein multiplayer its HUD font, Enemy
Territory and Elite Force their console fonts, Alice a font made from the one
its own console used, and Quake Live its own typeface. In Return to Castle
Wolfenstein and Enemy Territory, the game's logo appears behind the text once
the console is at least half open, as in the originals. Alice and F.A.K.K.2
have no console artwork, so their background is always plain. How Quake
Live's console text is drawn is described in
[Font Compatibility](fnql/FONT_COMPATIBILITY.md).

A softer, centred console:

```cfg
seta con_screenExtents "1"
seta con_fade "1"
seta con_backgroundOpacity "0.85"
```

## Messages and chat on screen

When the console is closed, each game shows recent messages and its chat line
as its original did:

- **Quake III Arena, Return to Castle Wolfenstein, Enemy Territory and Elite
  Force** show the latest console messages at the top of the screen for
  `con_notifytime` seconds -- 3 by default, 7 in Return to Castle
  Wolfenstein multiplayer and Enemy Territory, 5 in Elite Force Holomatch --
  with the chat line under them when you type a message.
  - Enemy Territory's own HUD shows its chat and messages; the console's
    message lines only appear with `con_drawnotify` set, which is a cheat
    setting and needs cheats turned on. Its chat commands are
    `clMessageMode`, `clMessageMode2` and `clMessageMode3`.
  - Return to Castle Wolfenstein multiplayer shows nothing while its limbo
    menu is open. Single player shows the messages over movies too.
  - Elite Force single player shows the messages only, as the original did.
  - `cl_conXOffset` moves the messages sideways in Quake III Arena, Return to
    Castle Wolfenstein and Enemy Territory.
  - In Quake III Arena, Return to Castle Wolfenstein and Enemy Territory, a
    menu or the game's own typing line hides the chat line, and the messages
    too except at the end of a match; Elite Force hides neither.
- **Quake Live** shows its own chat bar near the bottom of the screen while
  you type, and no console messages.
- **Alice and F.A.K.K.2** show no console messages on screen, as the
  originals did.
- **Medal of Honor** shows the game's messages in its own message boxes, so
  the console's message lines start switched off (`con_notifytime 0`). Its
  chat line sits two thirds of the way down the screen, clear of the compass.

The chat line has the console's editing keys, and shows colour codes as you
type them.

### Chatting from the console

By default, everything you type in the console runs as a command, as in the
original games. Set `cl_allowConsoleChat 1` to send plain text typed while you
are in a game as chat instead; a line starting with `/` or `\` still runs as a
command. (`con_autoSay 1`, from FnQL, does the same.) With console chat on,
`con_sayRaw 1` sends your text exactly as typed, quotes and all, while
`con_sayRaw 0`, the default, sends it the classic way.

## Checking the controls

`inputstatus` shows which game is running, whether a menu or the console has
the keys, what the mouse is doing, and the last movement sent. It changes
nothing, so it is safe to try when the mouse or keys seem to stop working --
for example when an old settings file has turned the mouse off.

## The system console window

fnTech3 also has a separate system console window -- the one a dedicated
server shows, and that appears on Windows while fnTech3 starts. It takes the
colours and icon of the game that is running, in quiet, dark tones, and
fnTech3's own before a game is chosen. The `con_` settings above only change
the console inside the game.

## Related guides

- [Getting Started](GETTING_STARTED.md) for settings and settings files.
- [Widescreen guide](ASPECT_CORRECTION.md) for the HUD, menus and movies.
- [Graphics guide](DISPLAY.md) for the picture itself.
