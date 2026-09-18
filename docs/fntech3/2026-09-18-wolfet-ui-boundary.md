# Enemy Territory UI import boundary

## Scope and authority

This work follows `4ed9df5` and completes the pair begun by the
[cgame boundary record](2026-09-18-wolfet-client-boundary.md): ET's 121 UI
imports now have a dispatcher, reusing the records, renderer bridge and key
vocabulary that record introduced. The ET client is still gated and nothing here
has run inside one.

Ordinal and behaviour facts come from the pinned original
[Enemy Territory](https://github.com/id-Software/Enemy-Territory/tree/40342a9e3690cb5b627a433d4d5cbf30e3c57698).
The reference is GPL-3.0-or-later with additional terms. No implementation text
is incorporated into fnTech3.

## Six imports ET's own engine refuses

Reading ET's own UI dispatcher against the imports its `ui_public.h` declares
shows six ordinals with no case at all, which its `default` turns into
`Bad UI system trap`:

| Ordinal | Import |
| --- | --- |
| 21 | `UI_FS_COPYFILE` |
| 38 | `UI_CM_LOADMODEL` |
| 57 | `UI_LAN_GETLOCALSERVERCOUNT` |
| 58 | `UI_LAN_GETLOCALSERVERADDRESSSTRING` |
| 59 | `UI_LAN_GETGLOBALSERVERCOUNT` |
| 60 | `UI_LAN_GETGLOBALSERVERADDRESSSTRING` |

The four LAN ordinals are a superseded pair of per-list counters and address
lookups, replaced by the source-parameterised ones at 82 and 83. fnTech3 refuses
all six, which is what retail ET does; answering them would be an invention, not
a compatibility gain.

## The browser's own numbering

ET numbers its server-browser sources Local 0, Global 1, Favorites 2. This
engine keeps a retired MPlayer slot at 1 and so numbers Global 2 and Favorites 3.
Every ET call that names a source is translated before it reaches the browser,
and a number outside ET's three becomes -1, which the browser reads as an empty
list. Without that translation, an ET menu asking for the internet list would
have been shown the favourites.

`UI_LAN_SERVERISINFAVORITELIST` is ET's own: it asks whether the server at an
index of some list is also a favourite, matching by address, and answers yes for
any valid index of the favourites list itself.

## Services that are absent

ET's UI reaches for several services fnTech3 does not provide. None of them is
made to look present:

- **Automatic updates.** `UI_CHECKAUTOUPDATE` prints once that updates are
  unavailable and establishes no update result; `UI_GET_AUTOUPDATE` says so and
  installs nothing. No legacy update server is contacted.
- **Anti-cheat status.** `UI_SET_PBCLSTATUS` and `UI_SET_PBSVSTATUS` accept and
  discard, as ET's own client does. No anti-cheat service is reported present.
- **Limbo chat.** `UI_CL_GETLIMBOSTRING` answers for ET's seven line slots with
  an empty line. Nothing in ET's own client ever writes that buffer either, so
  this is what retail returns; only the terminator is written, so a module
  buffer shorter than ET's 140 bytes is safe.
- **Opening a URL.** `UI_OPENURL` is refused. Handing a module-supplied string
  to the desktop's URL handler is not something this engine will do.

CD-key handling is the engine's existing path: the key is read and validated by
the engine, never synthesised, and a missing key stays missing.

## Coverage

Of the 121 imports ET's UI declares, **114 are handled**, counting what the
[sound](2026-09-18-wolfet-audio.md), [light](2026-09-18-wolfet-light.md),
[client services](2026-09-18-wolfet-client-services.md) and
[text and files](2026-09-18-wolfet-text-and-files.md) records added after this one. The dispatcher routes
ET's renderer services first, then its own records, keys and services, then a
table of imports whose meaning and argument shape are already this engine's. An
import reaching the end of that chain is refused by name and ordinal.

The 7 that remain open:

| Ordinals | Imports | Why |
| --- | --- | --- |
| 21, 38, 57, 58, 59, 60 | copy file, load collision model, the superseded LAN counters | ET's own engine refuses these; see above. |
| 109 | opening a URL | Refused deliberately; see above. |

## Verification

Clean Windows x86 RelWithDebInfo build; no errors or warnings. The complete
automated suite passes.

A source regression recomputes which of the 121 imports the dispatcher handles,
checks the open set is exactly the table above, checks the refusal path still
names the import, checks the browser source translation is present, checks the
key imports go through ET's translation rather than the engine's key numbers
directly, and checks no absent service is reported as available.

## Remaining gates

- **Nothing here runs.** The ET client stays gated, so no ET module has called
  any of these imports. The evidence is source and unit only.
- The renderer still has no MDM/MDX model type.
- Everything else in the [ET matrix](COMPATIBILITY.md#wolfet-gate-matrix)
  remains as recorded.

## Touched outside ET

The botlib export gained `PC_RemoveAllGlobalDefines`, which the parser already
implemented and only ET's UI asks for. Nothing else calls it and no existing
export moved.

One RTCW regression compiles a slice of the UI dispatcher into its own fixture
and was extended to know that an ET path exists, by stubbing the ET entry it now
mentions. What that regression asserts about RTCW is unchanged.

## Unrelated observations

None new in this slice.
