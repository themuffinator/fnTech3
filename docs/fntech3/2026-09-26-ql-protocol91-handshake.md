# Quake Live protocol-91 connectionless checks

`scripts/probe_protocol91.py` came from FnQL (`73b30286`, 2026-07-12) and had
no record in this tree; the
[QL compatibility roadmap](../fnql/QL_COMPATIBILITY_ROADMAP.md) names only its
session sibling, `probe_protocol91_session.py`. This record runs it against
the current dedicated server. No engine code changed. The
[machine-readable record](2026-09-26-ql-protocol91-handshake.json) holds the
server's identity, its command line and each check.

## Method

A local harness started `meson/build/win32-dev/fntech3.ded.exe` as `quakelive`
on `campgrounds` `ffa`, on an unswitched private desktop with an isolated
`fs_homepath`, bound to `127.0.0.1` on a free port, with pure mode and bots
off. Once the server answered `getstatus`, the probe sent four connectionless
packets from one loopback socket and read at most one reply to each:

| Check | Sent | Passes when |
| --- | --- | --- |
| protocol-91 info | `getinfo fnql-protocol91-probe` | the reply carries `\protocol\91` |
| FnQL nonce echo | `getchallenge 12345 FnQL` | the reply echoes ` 12345 91 FnQL`: the nonce, the protocol and the FnQL marker |
| retail binary challenge | `getchallenge ` followed by a little-endian SteamID and a 16-byte ticket | the reply is `challengeResponse <challenge>`, retail's form with no further token |
| malformed ticket silence | the same with a 3-byte ticket | no reply within one second |

## Results

All four checks passed, and the server was still running afterwards. The
retail form is answered at the challenge step without consulting Steam:
`SV_GetChallenge` keeps the SteamID and ticket for the connection that follows
(`code/server/retail_auth_challenge.hpp`), so the check holds with Steam
integration off. A malformed ticket is dropped with a developer-only message,
so it draws no reply at all.

## What this does not show

- No client connects. A retail `quakelive_steam.exe` joining with its ticket
  validated is the [authenticated-admission record](2026-09-08-ql-retail-auth-interop.md)'s
  evidence, from 2026-09-08 with the Steam provider enabled; this server ran
  without Steam.
- One run on loopback, over IPv4 only.
- Rate limiting is not exercised: the four requests stay under
  `SVC_RateLimitAddress`'s ten per second.

## Identity

- Commit `3089df2e0758a287ee2142316cf3b3945e53cc71`.
- Server `meson/build/win32-dev/fntech3.ded.exe`, SHA-256
  `37e548bd0f30ffc07d98ac302740cb9dc65e7d459ae7a042c537f144fca430f4`, the
  build described in the [release-profile record](2026-09-26-release-profiles.md).
- Windows 11 x64 host, 2026-09-26 at 20:49 UTC.
- Installation `E:\SteamLibrary\steamapps\common\Quake Live`, read only.
