# Profile-specific bot grapple default

The [transition series](2026-09-08-retail-transition-series.md) exposed conflicting
bot_grapple registration defaults. Read-only inspection of the legitimate retail
QL executable confirms an engine registration of `1`, matching its game module.
fnTech3 had retained Q3's `0` in both profiles, so the engine preempted QL's default.
Bot initialization now selects `1` for QL and retains `0` for Q3. Existing explicit
values remain authoritative through normal Cvar_Get behavior.

The same inspection confirms that the g_ammoPack warning has a different cause:
retail itself registers startup default `1` and map-path default `0`, both latched.
fnTech3 already implements that contract, documented in the inherited
[factory evidence](../fnql/FACTORY_COMPATIBILITY.md). Its behavior remains intact.

The [evidence record](2026-09-08-bot-profile-defaults.json) pins the retail
executable hash, argument locations, resolved strings and registration flags.
Addresses are specific to that exact executable; initial argument addresses alone
are insufficient without resolving their string contents. These are observed
contract facts, not incorporated implementation text. No retail code was copied,
patched or redistributed.

Four isolated Windows x86 dedicated probes queried bot_grapple before and after
an immediate restart using legitimate modules/assets:

| Profile | Explicit startup value | Observed value before/after restart | Observed reset default |
| --- | --- | --- | --- |
| Q3 | none | 0 / 0 | 0 |
| QL | none | 1 / 1 | 1 |
| Q3 | 1 | 1 / 1 | 0 |
| QL | 0 | 0 / 0 | 1 |

All four exited normally with status zero, without forced stops or bot_grapple
default warnings. Launch/configuration details, redacted loopback RCON replies,
engine/source hashes and revalidated input hashes are retained in the evidence.
Probes used isolated homes, private unswitched desktops and read-only assets;
there was no input-device control or screen capture. In the Q3 probe, querying
unregistered g_ammoPack fell through to a game console broadcast; this is not
evidence that Q3 registered that QL cvar.

An executable regression invokes the production SV_BotInitCvars body under both
profiles and checks registration defaults and retained explicit values. It passes
on Windows and WSL Ubuntu; actual runtime probes above additionally exercise the
real cvar implementation. Windows x86 client/dedicated and the existing Linux i686
dedicated scratch build pass. The latter is incremental, not a clean release build.
The full Windows suite passes 110 tests with one OpenAL skip; its Python aggregate
runs 1,278 tests with five skips, including enabled native runtime probes.

This verifies the default contract, not grapple navigation or complete bot parity.
The known optional Q3 config/symbol diagnostics, retail g_ammoPack registration
warning, timestamp-only logging anomaly and disabled-service diagnostics remain.
Linux compilation also reports existing unused-parameter warnings in bot debug
helpers. No additional unrelated behavior defect was found.
