# Each profile's notify lines and chat line

## Scope

`Con_DrawNotify` in `code/client/cl_console.cpp` draws what a shut console
leaves on screen. The console is FnQL's, and `Con_DrawNotify` drew retail
Quake Live 1.40.6's live chat overlay for every profile: a translucent strip
413 virtual lines down, a `say:` or `say team:` prompt on Quake Live's
unscaled 12x24 cell, and a field 73 characters wide. It drew no recent console
lines, because Quake Live registers no `con_notifytime` and draws none.

Quake III, RTCW, Enemy Territory and Elite Force draw the last four console
lines at the top of the screen for `con_notifytime` seconds, then a chat
prompt and field under them. Each engine does this its own way. This record
reads each retail executable and gives every profile but `ql` its own
engine's lines and chat line.

## Observed

Each retail executable was read with capstone. Addresses are virtual
addresses in each image.

| Executable | SHA-256 | `Con_DrawNotify` | Drawn when shut | `con_notifytime` |
| --- | --- | --- | --- | --- |
| `quake3.exe` Q3 1.32 | `80b0add0e014b863cf47cab6c997e0a54939236219a56910b7a3eb77dc2d44d4` | `0x407110` | active (`0x40761e`) | `"3"`, flags 0 (`0x406ce9`) |
| `WolfSP.exe` Wolf 1.32 | `07d206f61da5c80562ff60d68c645bca349b31adb5d7e0e7f186d61ad46683c7` | `0x407140` | disconnected under a menu, active, cinematic (`0x407600`) | `"3"` (`0x406dd9`) |
| `WolfMP.exe` Wolf 1.4-MP | `7cf99a938cf18ed2f5aecd0be542cbae8868959e0cce24b4b6eea08991e321f5` | `0x4073b0` | active (`0x40791e`) | `"7"` (`0x406f19`) |
| `et.exe` ET 2.60b | `4d1326c5936b61c5f878fc017e4aa0abddf13e008139e31fce2f13d57b26243e` | `0x406fb0` | active, with `con_drawnotify` set (`0x406e33`) | `"7"` (`0x4076e9`) |
| `stvoy.exe` ST:V EF v1.20 | `8f7ccadb52b4230689c5b3275177f7ec7060a30bfaa3ade4e7b3fb0cda0dff74` | `0x404890` | active (`0x404d69`, its state 7) | `"3"` (`0x404569`) |
| `stvoyHM.exe` ST:V HM v1.20 | `5b5b6e82ce7ac0d720a45ab1fa9df32930c846d6c51847fa0c158ebc86487dff` | `0x408590` | active (`0x408ad9`) | `"5"` (`0x4081e9`) |
| `quakelive_steam.exe` | `c926fe9f6c851e00b3b9332e88903ad01f28fdd60454873891c0158f5ded1299` | none | | no `con_notifytime` string |

### The lines

All six engines run the same loop, the one in the GPL Quake III source:

- It walks the four lines `con.current - 3` to `con.current` of a
  four-entry timestamp ring. A line with no time, or one older than
  `con_notifytime * 1000` milliseconds (`time > limit`, so a line exactly as
  old is still drawn), is skipped.
- Each non-space character is a `SCR_DrawSmallChar` 8x16 cell in screen
  pixels, at `(x + 1) * 8` from the console's left edge, starting in
  `g_color_table[7]`. A colour change sets `g_color_table[(c >> 8) & 7]`;
  `et.exe` masks with `& 0x1f`. Each drawn line moves the pen 16 pixels down,
  and the colour is cleared after the loop.
- Retail moves to a new, empty line at every line feed (`Con_Linefeed`,
  `CL_ConsolePrint` at `0x406dc0` in `quake3.exe`) and stamps that empty line
  at the end of the print. The ring therefore ends on it: after a line feed
  three printed lines show, and the empty one still takes a row.
  `[skipnotify]` zeroes the line just printed.
- `quake3.exe`, `WolfSP.exe`, `WolfMP.exe` and `et.exe` add
  `cl_conXOffset->integer` to each x. They skip a line while a UI or cgame
  key catcher is up (`test 0xa`), unless `cl.snap.ps.pm_type` is 5
  (`PM_INTERMISSION`).
- `stvoy.exe` and `stvoyHM.exe` have no `cl_conXOffset` string and test no
  catcher, so their lines show over menus too.
- `WolfMP.exe` first calls `Cvar_VariableIntegerValue( "ui_limboMode" )`
  (`0x4073b8`) and returns at once, lines and chat line alike, while it is
  set.

`WolfSP.exe`, `stvoy.exe`, `stvoyHM.exe` and the IHV build hold no
`[skipnotify]` string, so they would print the prefix as text. The GPL RTCW SP
source never prints it.

### The chat line

After the lines, `quake3.exe`, `WolfSP.exe`, `WolfMP.exe` and `et.exe`
return if a UI or cgame catcher is up. Elite Force does not. Then, with the
message catcher up:

| Engine | Prompt | Field |
| --- | --- | --- |
| `quake3.exe`, `WolfSP.exe` | `SCR_DrawBigString( 8, v, "say_team:" or "say:", 1.0 )` | `Field_BigDraw` at `skip * 16`, skip 11 or 5 |
| `WolfMP.exe` | `CL_TranslateString` (`0x411070`) first, then the same big string | skip `strlen + 2` or `strlen + 1` of the translated prompt |
| `et.exe` | `say_team:`, then `say_fireteam:` when `chat_buddy`, then `say:`, each through `CL_TranslateString` (`0x410f90`) | skip `strlen + 2`, `strlen + 2` or `strlen + 1` |
| `stvoy.exe` | `say:` only, the big string (`0x409ee0`) | `Field_BigDraw` (`0x406360`) at 80 |
| `stvoyHM.exe` | `SCR_DrawSmallStringExt( 8, v, prompt, g_color_table[7], forced )` (`0x411e20`) | `Field_Draw` (`0x40ae90`, size 8) at `skip * 8`, skip 11 or 5 |

`v` is the lines' height in screen pixels. The big prompt is placed in
640x480 virtual coordinates, so it only sits directly under the lines at
640x480. Each field's width argument is unused, as the GPL source's is.

The message modes set the field's width as they open it:

| Engine | Commands | Widths |
| --- | --- | --- |
| `quake3.exe`, `WolfSP.exe`, `WolfMP.exe` | `messagemode`..`messagemode4` | 30, 25, 30, 30 |
| `stvoyHM.exe` | `messagemode`..`messagemode4` (`0x407df0`..`0x407ed0`) | 55, 50, 30, 30 |
| `et.exe` | `clMessageMode`, `clMessageMode2`, `clMessageMode3` (`0x407910`, `0x4078a0`, `0x4078d0`) | 30, 25, 26 |
| `stvoy.exe` | `messagemode` (`0x4042b0`) | 30 |

- `et.exe`'s engine names its commands `clMessageMode`, as its cgame
  registers `messagemode` for a chat menu of its own. Only `clMessageMode3`
  sets `chat_buddy` (`0x4078df`), and nothing clears it, so after a fireteam
  chat the other two keep the fireteam prompt. Its `Message_Key`
  (`0x40ade1`) then sends `say_buddy`. `WolfMP.exe` has no buddy chat: it
  holds no `buddy` or `fireteam` string.
- `stvoy.exe`'s `messagemode` clears the field and sets its width. It leaves
  the key catcher alone, so it never opens the chat line. Its `Key_SetCatcher`
  (`0x40a820`) has no direct callers; only a module can raise the catcher.
  Its `Message_Key` (`0x406a20`) sends only `say`.
- `et.exe` and `etded.exe`
  (`7eb606913a56d6901e358a598fb0079120b2730e113a10fc963f90b777580cdf`)
  register `con_drawnotify` as `"0"`, `CVAR_CHEAT`, in their common start-up
  (`0x42b15f`, `0x410c35`).
- Every engine above but Quake Live registers `cl_noprint` as `"0"` and
  returns from `CL_ConsolePrint` while it is set. This client already does.

The GPL RTCW SP, RTCW MP and ET sources match their executables. Lilium
Voyager does not match Elite Force here: it uses skip 6 for `say:`, and
defaults `con_notifytime` to 3 in Holomatch, where `stvoyHM.exe` uses 5.

FnQuake3 at `7d8c9629` draws Quake III's loop and chat line with three
changes: the lines use the console's own cell, and so scale with it; `v` is
divided by `vidHeight / 480` before the chat line is placed; and it skips 10
cells after `say_team:`, where `quake3.exe` skips 11. Like Quake3e, it waits
for the next character before it moves to a new line, so its ring ends on the
last printed line.

### Other engines, for later profiles

- `quake3.exe`, February 1999 IHV test
  (`e9fcaa0a5682e7f07d540aacee08cdb77f3ea40190719f5cecf31109a5135d37`):
  `Con_DrawNotify` at `0x401ef0` keeps float times, tests no catcher and has
  no `cl_conXOffset`. It draws `say_team:` or `say:` with its big string and
  skip 11 or 5. `con_notifytime` is `"3"`, and its two message modes set no
  width.
- `jamp.exe`, `jasp.exe`, `jk2mp.exe` and `jk2sp.exe` register
  `con_notifytime` as `"3"` (`0x4174ba`, `0x405809`, `0x405ed9`,
  `0x405549`). The multiplayer engines have `messagemode`..`messagemode4` and
  `cl_conXOffset`; the single-player ones have only `messagemode`. `jamp.exe`
  holds no `say:` string. `alice.exe` has none of these strings.

## Change

Quake Live's overlay runs as before. Every other profile draws
`Con_DrawClassicNotify`, from a per-profile `Con_NotifyContract`. A profile
without a row, and the client before a profile is chosen, get Quake III's.

- **Lines.** The loop above, drawn with `SCR_DrawSmallChar` in the console's
  own cell. That cell is the profile's: 8x16 on the 640x480 virtual screen, so
  16x32 at 1280x960, as FnQuake3 scales it. It starts at the console's left
  edge, which moves with `con_screenExtents`. `cl_conXOffset` applies, and UI
  and cgame catchers hide lines except at intermission, only where the engine
  does. A colour is the console's own stored index, as its scrollback draws
  it.
- **The ring.** `Con_NotifyLine` is the line retail is on: one past
  `con.current` while this console waits to open a new line. `Con_Linefeed`
  and `CL_ConsolePrint` stamp it, so the ring holds what retail's holds. The
  classic loop ends on it, and draws an empty row for a line not opened yet.
  `[skipnotify]` now clears the line it printed, not the one before.
- **Chat line.** The prompt and field each engine draws, with its skip.
  RTCW MP and ET translate the prompt through their catalogue, with the new
  `CL_TranslateClientString`, and count cells from the result. The big chat
  line is placed in virtual space after the lines' height is divided by
  `vidHeight / 480`, as FnQuake3 does, so it stays under them at any
  resolution. Holomatch's small prompt and field are drawn in the console's
  cell. Fields draw colour escapes literally, as FnQuake3 draws its chat
  field; the retail engines hide them, which moves the cursor.
- **When.** `Con_NotifyDrawn`: while active, except that RTCW SP also draws
  while disconnected and during a cinematic, and ET only with
  `con_drawnotify` set.
- **Cvars.** `con_notifytime` is registered with each engine's default for
  every profile but `ql`. `con_drawnotify` (`"0"`, `CVAR_CHEAT`) is
  registered for `wolfet` alone, in the client.
- **Chat commands.** Each profile registers its engine's set. Widths come
  from its row; Quake Live's still come from its cgame, and its field is
  still re-measured every frame. ET's `clMessageMode3` sets `chat_buddy`, and
  `Message_Key` sends ET's `say_buddy` for it. `stvef-sp`'s `messagemode`
  readies the field without opening it.
- `scripts/audit_rtcw_engine_commands.py` reads `cl_console.cpp`'s command
  table, as it reads `cl_input.cpp`'s and `sv_ccmds.cpp`'s.

## Evidence

### Tests

- `tests/console_notify_profile_tests.py` compiles the console's own
  `Con_DrawNotify`, `Con_DrawConsole`, chat modes, registration, `Message_Key`
  and print path with the screen, field and renderer calls recorded. It runs
  every profile id, and no profile, at 1280x960 on a 16x32 cell. It checks
  each character's cell, position and colour; the notify time and its
  boundary; `cl_conXOffset`, catchers and intermission; limbo; each prompt,
  translated prompts and their skips; the small and big chat lines; each
  command set and width; the draw gate by connection state; the ring through
  real prints with `[skipnotify]`; and ET's `say_buddy`. Under `ql` it checks
  that the overlay's calls are the old ones.
- Against the previous source every non-`ql` profile fails 40 to 65 checks,
  and `ql` passes. With `Con_NotifyLine` returning `con.current`, as the old
  stamping did, the ring checks fail for every non-`ql` profile.
- `cgame_native_bridge_source_tests.py`, `ql_font_source_tests.py` and
  `rtcw_engine_command_tests.py` pinned Quake Live's overlay, widths and
  missing `con_notifytime` for every profile. They now pin them for `ql`.
- The commit's tree, `7173858` with this change alone, was exported and
  built for Windows x86 with MSVC, `debugoptimized`, strict warnings (`/WX`)
  and the RTCW, Elite Force and ET development options, without a warning.
  Its C++ tests passed (116, 2 skipped), and so did its Python suite
  (1,831 passed, 76 skipped).

### Runtime

A Windows x86 client was built from `7173858` with this change, plus two
uncommitted hunks from other work: the profile's console cell
(`Con_ProfileCell`) and the renderer's Quake Live-only host-font lane.
`fntech3.exe` had SHA-256
`f4ead72cbc0e6f34c067777a839c4838629f28d02f5dd0671b4b48f24209cc46` and
`fnql_glx_x86.dll`
`9ee04bbeb8566d2e6bfa0456bdd5b31a298ae6efe5ce83dc8c69800979475cc3`.

[`scripts/probe_notify_lines.py`](../../scripts/probe_notify_lines.py) ran
each profile windowed on an unswitched private desktop, with an isolated
home, mouse, joystick and sound off, and no input. After the map, the active
action echoed three coloured lines, took `screenshotPNG`, ran `messagemode`,
took two more captures 16 frames apart for the blinking cursor, switched to
`messagemode2`, took a fourth, and quit. Every run exited 0 and left its
installation's file inventory unchanged.

- `q3`, Steam Quake III, `q3dm17`, 1280x960: three lines in 16x32 cells, an
  empty row, then `say:` in 32-pixel big cells at 128, its cursor at 160;
  `say_team:` with its cursor at 352.
- `q3` at 640x480, beside retail `quake3.exe` on the same map with the same
  actions (`screenshot`, TGA): in every capture the lines, the empty row and
  the prompt fall in the same 16-pixel rows, and `say:` spans x 12-63 in
  both. The earlier build showed four lines and no empty row here, where
  retail shows three. `echo` differs: retail prints each argument apart, so a
  colour code colours only its own word.
- `rtcw-sp`, `escape1` loaded with `devmap`, which runs it as FFA and skips
  the mission briefing: Quake III's lines and chat line.
- `rtcw-mp`, `mp_beach`: nothing but the limbo menu while it is open, then,
  after `CloseLimboMenu`, the lines and chat line. The round timer at 5,48
  shares the empty row, as it would in retail.
- `wolfet`, `oasis`, with `openlimbomenu` and then `con_drawnotify 1`,
  and ET's `clMessageMode` and `clMessageMode3`: the lines in ET's console
  charset, then `say:` and `say_fireteam:`. The game's
  `[skipnotify]` referee message stayed out of the lines.
- `stvef-sp`, `borg1`: the lines. `messagemode` opens nothing and
  `messagemode2` does not exist.
- `stvef-mp`, `hm_voy1`, 1280x960 and 640x480: the lines, then the small,
  white `say:` in the console's cell, its cursor at five cells, and
  `say_team:` at eleven.
- `ql`, `campgrounds` (`ffa`): no lines, and the overlay as before: the strip
  near the bottom and `say: |` in its host face.

The [evidence file](2026-09-23-profile-notify-lines.json) holds each
run's command, action, outcome and capture hashes.

A retail `stvoyHM.exe` comparison was also tried, with the same isolated
`fs_homepath`. Retail Elite Force ignores it, so it did not find the probe's
config. It loaded `hm_voy1`, shut its client down within a second on the
private desktop, took no capture, and was stopped after 150 seconds. By then
it had rewritten the installation's `BaseEF/hmconfig.cfg` with the probe's
`r_mode 3`, `r_fullscreen 0`, `in_mouse 0`, `in_joystick 0`,
`com_maxfps 60` and `r_ignorehwgamma 1`, and created `qconsole.log` and
`games.log` there. The two logs were moved out. The earlier `hmconfig.cfg`
could not be recovered. The probe no longer runs Elite Force's retail
engines; [the Holomatch sessions record](2026-09-18-stvef-holomatch-sessions.md)
runs them from a copy.

## Open

- Rows for profiles not yet committed: `q3-ihv` (above), and the Jedi
  profiles' defaults of 3.
- Colour indexes. The console stores fnTech3's extended index
  (`ColorIndexFromChar`), and notify lines draw it as the scrollback does.
  Retail stores `(c - '0') & 7` (`& 31` in ET) as it prints, so escapes past
  `^7` differ, as the large-text record notes.
- `stvef-sp`: the shared `Message_Key` can still send `tell` or `say_team`
  if a module raises the message catcher with that state set; `stvoy.exe`
  sends only `say`.
- `con_drawnotify` is registered in the client only; `etded.exe` also has
  it.
- `WolfSP.exe` shows no console at all while connecting unless `con_debug`
  is set, and its `con_debug 2` draws the console twice as deep. This client
  has no `con_debug`.
- Elite Force draws a full-screen console when disconnected unless a UI
  catcher is up; Quake III tests UI and cgame catchers there. This client
  tests UI, cgame and browser catchers for every profile.
- `echo` joins its arguments before printing; `quake3.exe` prints each one
  separately, so a colour code does not carry past its word there.
- This client strips and honours `[skipnotify]` in every profile.
  `WolfSP.exe` and Elite Force's engines would print it as text.
