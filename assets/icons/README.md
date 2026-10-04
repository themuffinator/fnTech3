# Application icon

The compact hot-orange flame and black **fn** monogram is the executable's
permanent identity. It is derived from the [fnTech3 logo](../logos/README.md).

| File | Purpose |
| --- | --- |
| `fntech3.ico` | Windows executable resource, 16/24/32/48/64/128/256 px, full alpha |
| `fntech3.icns` | macOS bundle icon, 16–1024 px including Retina representations |
| `fntech3-<size>.png` | Linux desktop icons and general use, 16–1024 px, transparent |
| `fntech3.desktop` | Linux launcher template, using the `fntech3` icon name |
| `fntech3-icon-source.png` | Unmodified generated artwork, retained for future exports |

Windows builds embed the ICO as the first icon group of the client and the
dedicated server, so Explorer shows it for both executables. SDL windows use an
embedded 128 px RGBA copy, so installation of a separate icon file is
unnecessary. Native X11 windows publish the same copy through `_NET_WM_ICON`
when they show the engine icon. macOS CMake and bundle tooling use the ICNS;
macOS client builds are currently disabled by the project's compatibility
gate, so these assets and the Cocoa integration do not imply macOS support.

On Linux, install each desired PNG as
`~/.local/share/icons/hicolor/<size>x<size>/apps/fntech3.png` and copy the desktop
template to `~/.local/share/applications/fntech3.desktop`. Set its `Exec` to the
installed executable (or put `fntech3` on `PATH`) and add the same profile and
installation arguments used to launch from the terminal. With `cl_gameIcon 0`
SDL advertises the matching `fntech3` application ID; while a game icon is
shown the ID is `fntech3-<profile key>`, matching that game's own launcher
template (see [game icons](games/README.md#platform-support)).
Meson installs the PNGs into `share/icons/hicolor` and the unconfigured launcher
template into `share/fntech3` beneath the chosen installation prefix.

## Active-game icon

`cl_gameIcon` chooses the running client window's icon. Run `vid_restart`
after changing it. It affects presentation only, never profile selection. The
game profile remains immutable for the process lifetime.

| `cl_gameIcon` | Window, taskbar and Dock icon |
| --- | --- |
| `0` | fnTech3 |
| `1` (default) | The active game's built-in [remake](games/README.md) of its original icon |
| `2` | The installation's own icon when one is found, otherwise the remake |

The remakes ship inside the client: Windows clients carry each game's ICO as a
resource, other clients link its 16 to 128 px frames. Every backend therefore
shows them without installed files or a retail executable. Windows binds exact
caption and taskbar frames for the window's DPI; SDL passes every frame to
Wayland and Cocoa; the native X11 backend publishes every size. Client and
dedicated system console windows also show the selected profile's built-in
remake, independently of `cl_gameIcon`. The executables' own icons in Explorer
keep fnTech3's.

For `cl_gameIcon 2` the lookup uses only canonical paths within the selected
`fs_basepath`, never mods, paks, downloads, `PATH`, or another installed game.
It reads icon resources without executing the retail binary. A missing,
malformed or unsupported icon falls back to the remake.

| Active profile | Windows executable candidates, in priority order | Optional PNG beside the installation's executable |
| --- | --- | --- |
| Q3 | `quake3.exe` | `quake3.png` |
| QL | `quakelive_steam.exe`, `quakelive.exe` | `quakelive.png` |
| RTCW SP | `WolfSP.exe` | `wolfsp.png`, then `wolfenstein.png` |
| RTCW MP | `WolfMP.exe` | `wolfmp.png`, then `wolfenstein.png` |
| ET | `et.exe` | none |
| Elite Force SP / Holomatch | `stvoy.exe` / `stvoyHM.exe` | none |
| Q3 IHV test | `quake3.exe` | none |
| Jedi Academy SP / MP | `jasp.exe` / `jamp.exe` | none |
| Jedi Outcast SP / MP | `jk2sp.exe` / `jk2mp.exe` | none |
| Alice | `alice.exe` | none |
| F.A.K.K.2 | `fakk2.exe` | none |

Windows SDL and native windows extract the selected executable's icon. SDL
converts both legacy masked icons and modern alpha icons into its own surface,
so subsequent window-style changes keep the game icon. SDL builds can also read
the PNG candidates above (requires SDL 3.4, the bundled version). PNG inputs are
limited to 4 MiB and 1024 px per dimension before decoding. SDL 3.2/3.3 builds
retain executable extraction and the remake, without PNG loading. The native
X11 backend has no installation lookup and shows the remake.

ELF binaries generally have no Windows-style icon resource, so Linux uses the
optional PNGs rather than guessing from a different game's executable. The
Cocoa bridge can inherit an installed bundle's declared icon using NSWorkspace:
`Quake3.app` / `Quake III Arena.app`, `Wolfenstein SP.app` /
`Return to Castle Wolfenstein.app`, or `Wolfenstein MP.app`, directly in the
selected installation root. There is no assumed native Quake Live bundle.
Without a matching bundle it retains the PNG or the remake.

No retail icons or executables are shipped with this implementation. Runtime
inheritance uses only the user's local installation; the remakes are original
artwork drawn for fnTech3.

## Re-exporting

Run `powershell -NoProfile -File scripts/generate-app-icons.ps1` on Windows.
The exporter resizes the preserved artwork with alpha, packages the ICO/ICNS,
and regenerates `code/platform/application_icon_data.h`. No image-generation
service is needed to reproduce the exports. The source artwork was created
using the built-in image generation tool; see [the prompt](PROMPT.md).

Implementation references: [SDL window icons](https://wiki.libsdl.org/SDL3/SDL_SetWindowIcon),
[Windows icon extraction](https://learn.microsoft.com/en-us/windows/win32/api/shellapi/nf-shellapi-extracticonexw),
[freedesktop launchers](https://specifications.freedesktop.org/desktop-entry-spec/latest/),
and [AppKit application icons](https://developer.apple.com/documentation/appkit/nsapplication/applicationiconimage).

## Validation

Verified on 2026-09-17: the Windows x86 development client builds; automated
tests cover profile isolation, invalid files, PNG limits and decoding, legacy
masked and alpha icons, native/SDL hidden-window assignment, fallback, repeated
replacement and handle lifetime. The native Win32 window source also compiles.
Read-only probes extracted and converted the locally installed Q3A, QL, RTCW SP
and RTCW MP executable icons. No retail executable was launched or copied.

The portable lookup tests also pass under Linux, including symlink containment,
and the native X11 window source passes an i686 syntax compile. All exported
PNG/ICO/ICNS containers and the existing macOS packaging tests pass. A live Linux
desktop and macOS Dock have not been visually tested; this work does not change
the project's game compatibility evidence or platform release gates.
