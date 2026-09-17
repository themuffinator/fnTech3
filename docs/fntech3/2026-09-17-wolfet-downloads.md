# Profile-owned official-pak policy

## Scope and authority

This work follows `369984c`, with a clean worktree at the start. It replaces the
compile-time official-pak test on the download paths with one that uses the
active profile's base game directory, and gives ET its `mp_bin` rule. ET
downloads are the motivation; the correction necessarily covers every profile,
because the old test could only ever be right for the one directory Quake
compiles in. The ET client and default-release gates are unchanged.

Interoperability facts come from the pinned original
[ET filesystem](https://github.com/id-Software/Enemy-Territory/blob/40342a9e3690cb5b627a433d4d5cbf30e3c57698/src/qcommon/files.c)
and [ET download handling](https://github.com/id-Software/Enemy-Territory/blob/40342a9e3690cb5b627a433d4d5cbf30e3c57698/src/server/sv_client.c),
plus the base-game constant in
[qcommon.h](https://github.com/id-Software/Enemy-Territory/blob/40342a9e3690cb5b627a433d4d5cbf30e3c57698/src/qcommon/qcommon.h).
The reference is GPL-3.0-or-later with additional terms. No implementation text
is incorporated into fnTech3.

## The defect

`FS_idPak` compares a pak name against `BASEGAME`, a compile-time constant that
is `baseq3`. Both download paths — the server deciding whether to serve a
referenced pak, and the client deciding what it may auto-download — passed that
constant. Under a profile whose base game is not `baseq3`, no comparison could
ever match, so the refusal never fired:

| Profile | Base game | Official paks refused before |
| --- | --- | --- |
| Quake III Arena | `baseq3` | yes |
| Quake Live | `baseq3` | yes |
| RTCW SP and MP | `main` | no |
| Enemy Territory | `etmain` | no |

An ET or RTCW server would therefore have offered the game's own retail paks to
any client that asked, which diverges from what those engines do and is a form
of redistributing retail assets. That outweighed keeping the RTCW profiles
untouched, so this correction is deliberately not ET-only; RTCW's runtime
behaviour changes from serving `main/pak0` to refusing it, which is what retail
RTCW does.

## Contract

`FS_OfficialPak( "gamedir/name" )` replaces both call sites. It refuses the
numbered paks `pak0` through `pak8` of the active profile's own base game, and
additionally:

- Profiles whose base game is Quake's own directory, which is Quake III Arena
  and Quake Live, refuse Team Arena's `missionpack/pak0` through `pak3`. RTCW
  and ET have no such pack and no longer consult that list.
- Quake Live refuses retail's two-digit `baseq3/pak00`, which stays downloadable
  under Quake III Arena so an ordinary add-on of that name still works.
- Enemy Territory refuses `etmain/mp_bin`, the pak holding its client modules,
  which retail ET names explicitly rather than by number.

Comparison remains case-folded with the two path separators treated alike, and
a null or empty name is never official. Quake III Arena and Quake Live keep
exactly the behaviour they had.

## Verification

Clean Windows x86 RelWithDebInfo build with `-Dwolfet-development=true` and
`-Drtcw-development=true`; no errors or warnings. The complete automated suite
passes.

A new executable regression drives the real policy across all five profiles:
each profile's own numbered paks and its extra name are refused, another
profile's are not, `pak9` and `pak00` are not official except where retail says
otherwise, Team Arena stays refused under both Quake profiles and nowhere else,
mixed case and back-slash separators match, and null and empty names are
refused. The existing Q3/QL pak-download fixture also gained the new policy and
still passes unchanged otherwise. A second check pins the base game directory of
every catalog entry, because the policy is only meaningful against the real
names, and a third pins that neither download path still passes `BASEGAME`.

A retail ET client still passes pure admission and joins both teams against the
rebuilt server, so the changed download path did not disturb admission.

This is otherwise a source and unit-level correction. No download was
transferred in this slice: exercising an ET auto-download needs a referenced pak the client lacks,
which is staged separately from a retail installation and is still open.

## Remaining gates

- No ET download has been transferred end to end, over UDP or HTTP. The
  `sv_wwwDownload` redirect, `sv_wwwFallbackURL` and the disconnected-download
  flag are implemented by the
  [redirect record](2026-09-17-wolfet-web-downloads.md) with source and unit
  evidence, but nothing has been transferred over them either.
- RTCW's newly effective refusal has source and unit evidence only; no retail
  RTCW client run was made.
- Everything else in the [ET matrix](COMPATIBILITY.md#wolfet-gate-matrix)
  remains as recorded.

## Unrelated observations

The RTCW profiles were affected by the same defect, as described above. That is
the one product issue this slice found outside ET.
