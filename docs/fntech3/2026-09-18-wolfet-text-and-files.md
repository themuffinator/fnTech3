# Enemy Territory text and files

## Scope and authority

This work follows `09af9cd`. It carries ET's localisation lookup and its own
file deletion, on both module boundaries. The cgame boundary reaches 136 of 147
imports and the UI boundary 114 of 121. Of the seven UI imports still open, six
are ordinals retail ET refuses itself and the seventh is a deliberate refusal.
The ET client is still gated and nothing here has run inside one.

Behaviour facts come from the pinned original
[Enemy Territory](https://github.com/id-Software/Enemy-Territory/tree/40342a9e3690cb5b627a433d4d5cbf30e3c57698).
The reference is GPL-3.0-or-later with additional terms. No implementation text
is incorporated into fnTech3.

## The localisation lookup

ET's modules hand the engine a string and a buffer and get a localised string
back. Read against the pinned original, ET's localisation files carry the same
grammar as the other Wolfenstein profile's: an optional `#version`, then records
of an English key with the same four translations and an optional offset pair,
loaded from `scripts/translation.cfg` and every `translations/*.cfg`. That is an
observation about two file formats, not a shared contract, so ET keeps its own
catalogue and neither profile can see the other's records. `LoadTranslations`
reloads ET's from the console, as it does for the other profile.

The behaviour a module sees is ET's own. A null string answers `(null)`. English
and an unselected language come back unchanged, and so does a selected language
with no matching record — which is what ET returns on a lookup miss, so an
installation with no translation files behaves as retail does with none. Input
and output are both bounded at ET's own 32,000 bytes, and because a module may
hand in the same buffer it reads back, nothing is written until the translation
and every size check are complete.

## The delete

ET's delete is not a file delete. It reaches exactly three folders under the
home game directory — `save/`, `profiles/` and `demos/` — and when the path
names a directory it removes everything inside it. That is why the earlier
records refused the import rather than implement half of it.

It is now implemented as ET defines it: the three folders and nothing else, the
engine's existing path validation on top, and a directory removed with its
contents. The walk stops after four levels rather than following an unexpectedly
deep tree, because an ET profile directory is one level deep. Success is
reported only when the path is actually gone, as ET reports it.

Two platform primitives were added for this — removing an empty directory and
asking whether a path is one — on both the Windows and the Unix side.

`UI_FS_COPYFILE` stays refused: it is one of the six ordinals ET's own engine
has no case for.

## Coverage

The cgame boundary now handles **136 of the 147** imports ET declares and the UI
boundary **114 of the 121**. The remaining ones are tabled in the
[cgame](2026-09-18-wolfet-client-boundary.md) and
[UI](2026-09-18-wolfet-ui-boundary.md) records; on the UI side, all seven are
either ordinals retail ET refuses itself or the deliberate refusal to hand a
module-supplied URL to the desktop.

## Verification

Clean Windows x86 RelWithDebInfo build; no errors or warnings. The complete
automated suite passes.

A new source regression checks that the two profiles keep separate catalogues
and that ET's lookup cannot reach the other's, that input and output are bounded
and that the output is written only after every check, that a null string
answers as ET's does, that both boundaries reach the same lookup, that deletion
reaches only the three named folders and keeps the engine's path validation,
that the walk is depth-limited and uses the directory primitives, and that both
platforms supply them.

## Remaining gates

- **Nothing here runs.** The ET client stays gated, so no ET module has asked
  for a translation or deleted anything. No installation was modified.
- No translation file was loaded, because a stock installation ships none; the
  lookup has been exercised only through the catalogue's own regressions, which
  belong to the other profile's record.
- The renderer still has no MDM/MDX model type, and ET's camera path surface,
  decals, polygon buffer, global fog, dynamic shaders, render to texture and
  texture identity remain refused.
- Everything else in the [ET matrix](COMPATIBILITY.md#wolfet-gate-matrix)
  remains as recorded.

## Touched outside ET

The Windows and Unix platform layers each gained two entry points, and the
filesystem one function. The shared translation catalogue gained a second
instance and no change to its parser, so the other profile's records and
behaviour are untouched.

## Unrelated observations

None new in this slice.
