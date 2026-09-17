# Enemy Territory redirected downloads

## Scope and authority

This work follows `7896acd`, with a clean worktree at the start except for
another session's in-flight RTCW files. It implements ET's server side of the
redirected ("www") download: the `svc_download` redirect message, the fallback
URL, the four progress subcommands a client reports back, the per-client state
those need, and the four `sv_www*` cvars. The ET client and default-release
gates are unchanged.

Interoperability facts come from the pinned original
[ET download handling](https://github.com/id-Software/Enemy-Territory/blob/40342a9e3690cb5b627a433d4d5cbf30e3c57698/src/server/sv_client.c),
[server cvars](https://github.com/id-Software/Enemy-Territory/blob/40342a9e3690cb5b627a433d4d5cbf30e3c57698/src/server/sv_init.c),
[server client record](https://github.com/id-Software/Enemy-Territory/blob/40342a9e3690cb5b627a433d4d5cbf30e3c57698/src/server/server.h)
and [download flags](https://github.com/id-Software/Enemy-Territory/blob/40342a9e3690cb5b627a433d4d5cbf30e3c57698/src/qcommon/dl_public.h).
The reference is GPL-3.0-or-later with additional terms. No implementation text
is incorporated into fnTech3.

## Contract

**The redirect.** When `sv_wwwDownload` is set and the requesting client
advertised `cl_wwwDownload` in its userinfo, the server writes `svc_download`
with a block number of `-1`, then the URL, the file size and a flag word, in
place of the first data block. `sv_wwwBaseURL` and the requested file name form
the URL. `sv_wwwDlDisconnected` sets the low flag bit, telling the client it may
disconnect while fetching. As with every other message to a connecting client,
the reliable-command acknowledgement and pending server commands precede it.

**The fallback URL.** While `sv_wwwFallbackURL` is set it replaces every other
strategy for a client that cannot or will not fetch directly: the same message
carries that URL, a zero size and flag value 2, which marks a plain URL rather
than a file the client should retrieve and verify.

**Falling through.** Returning no redirect leaves the ordinary block transfer to
run, so an unset base URL, a file whose size cannot be read, a client without
the capability and no fallback URL, or a previously failed fetch all degrade to
the transfer fnTech3 already had. Official and unreferenced paks are still
refused before any redirect is considered.

**Progress.** The client reports through `wwwdl`: `ack` starts the fetch and
suppresses block writing; `bbl8r` acknowledges disconnected mode and drops the
client; `done` clears the download; `fail` and `chkfail` clear it, mark the file
for the ordinary transfer and resend the game state, with `chkfail` also warning
the operator that the redirect configuration serves the wrong bytes. A `wwwdl`
before any redirect, or an unrecognised subcommand, drops that client, as the
original does.

**Profile scope.** ET is the only profile with this protocol. Under Q3, Quake
Live and RTCW the engine does not recognise `wwwdl` at all, so it continues to
reach the game module exactly as before, and the per-client state accessor
returns nothing. The state lives in ET's own engine-internal storage;
`client_t` gains no field, so no other profile's client record changes.

## Verification

Clean Windows x86 RelWithDebInfo build with `-Dwolfet-development=true` and
`-Drtcw-development=true`; no errors or warnings. The complete automated suite
passes.

New source regressions pin the redirect message's field order behind its
acknowledgement, each precondition and the fallback form, that a refused
redirect falls through to the block transfer while a client already fetching
receives none, all five progress subcommands with their four drop paths, that
`wwwdl` stays unknown to the other four profiles, that the state is reset per
file and never widens `client_t`, the four cvar names and defaults, and that the
capability is read from `cl_wwwDownload`.

A retail ET client still passes pure admission and joins both teams against the
rebuilt server, so the changed download path did not disturb admission.

## Remaining gates

- **No download has been transferred over this path.** Exercising it needs a
  referenced pak the client lacks and a web server to serve it, neither of which
  is staged here. The message shape and state machine have source and unit
  evidence only.
- fnTech3's client side of the redirect is untouched and ET client startup is
  still gated, so the reverse direction is not testable from here.
- The ordinary ET block transfer has likewise never carried a file end to end;
  see the [official-pak record](2026-09-17-wolfet-downloads.md).
- Everything else in the [ET matrix](COMPATIBILITY.md#wolfet-gate-matrix)
  remains as recorded.

## Unrelated observations

Adding `wwwdl` to the shared client-command table would have made it a known
command for every profile, turning a name the game module used to receive into
a client drop. The lookup now refuses it outside ET for that reason; no other
profile's command set changed.
