# Reject startup command overflow explicitly

The [renderer investigation](2026-09-08-renderer-stall-localization.md) exposed
silent truncation after the engine's 32 startup-command slots. A later `+exec`
or explicit profile argument could disappear while the process continued with
the remaining settings. The early parser now fails with an actionable diagnostic
instead of accepting a partial command line. The diagnostic does not echo
argument values. It recommends moving non-startup settings into a config file
while retaining the profile and installation paths on the command line.

The limit and separator rules are unchanged. An executable regression extracts
the production parser and exercises 0, 1, 31, 32, 33 and 64 command segments,
quoted plus signs, plus/newline/carriage-return separators and repeated early
parsing. The committed predecessor fails all six overflow cases; the correction
passes on Windows and Linux. This is parser execution evidence, not a full Linux
engine build or a claim of unlimited command-line support.

The rebuilt Windows x86 dedicated executable rejects four 33-command launches:
Q3 and QL profile arguments each placed first and last. Every process exits 1
with the expected diagnostic before creating its isolated home directory. No
retail input, game window or input control is needed for these early failures.
The native Windows and SDL builds pass. The complete native-Windows suite passes
110 targets, zero failures and one skip. The [structured record](2026-09-08-startup-command-bounds.json)
pins source and executable identities, fixture construction and validation logs.

This closes the observed silent-overflow issue. Broader profile/installation
gates and normal back-buffer audio continuity remain open. No new unrelated
issue was discovered and no external implementation was incorporated.
