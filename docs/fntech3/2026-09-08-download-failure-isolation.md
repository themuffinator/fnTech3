# Isolate a failed download from an already active client

This checkpoint extends the [server read-failure tests](2026-09-08-server-download-read-failure.md)
with two simultaneous clients. No engine source or binaries changed from
`1ae031e`; this is additional live evidence for download failure isolation.

Each run starts the merged Windows x86 dedicated server with a server-only
262,292-byte generated fixture pak, pure mode, two client slots and a bounded UDP
download rate. The first client starts in an empty isolated home, downloads the
pak and reaches CS_ACTIVE. Only then does a second client start in another empty
home. The probe waits for block zero of slot one's download and truncates the
fixture in the temporary server home to zero bytes. Asset installations remain
read-only.

The server drops the second client with its read-failure reason. The first
client subsequently sends the scheduled `fntech3_after_fault_client` marker,
which appears in the server's reliable client-command log after the failure.
That deliberately unknown command tests continued delivery, not game-command
semantics. The first client's screenshot is taken after the marker and before
its normal scripted quit. The server's active-to-zombie transition for that
client follows its explicit disconnect command, not the other client's fault.

The matrix includes both profiles with reference clients and both profiles with
the same merged x86 client executable. Clients connect directly over loopback;
the inherited runner's unused relay is not part of the traffic path. Each client
runs windowed with input disabled on a private unswitched desktop. Screenshots
come from the engine render target. No keyboard/mouse control or credentials are
used, and no proprietary inputs or screenshots are tracked.

All four runs pass their ordered-log, file-hash and exit checks. All twelve
processes exit zero without forced termination. First-active and truncation
observations are elapsed seconds from probe launch, not transfer durations:

| Profile / clients | First active | Fixture truncated | Total probe lifetime |
| --- | ---: | ---: | ---: |
| Q3 / reference | 6.922 s | 11.641 s | 55.281 s |
| QL / reference | 8.125 s | 9.406 s | 55.359 s |
| Q3 / merged | 8.469 s | 10.500 s | 55.282 s |
| QL / merged | 9.141 s | 11.312 s | 55.391 s |

Screenshots after the fault show the first Q3 client's world and HUD, and the
first QL client's native join UI over the loaded world, for both client variants.
The same merged dedicated binary serves every run; the same merged client
binary serves both self-pair profiles.

The generated pak's SHA-256 is
`43e24cbd82e3926f818d6a9476f0aa51c79947ba6b1884ae0c78cea5fadb6617`.
Its single 262,144-byte deterministic payload is generated using seed 20260908.
The initial client retains the exact pak; the failed client has no final pak.
The reference-client runs preserve Q3's pinned clean reference lineage and QL's
existing reference binary with unverified source revision. Self-pairs are
explicitly identified separately from reference interoperability.

The [evidence record](2026-09-08-download-failure-isolation.json) includes run
commands, configurations, executable/fixture hashes, ordered log excerpts,
screenshot hashes and process outcomes. The evidence concerns one bounded
failure while one client remains active. It does not establish simultaneous
independent downloads, disk-full behavior, HTTP fallback, long-duration stress,
or independent retail interoperability. QL join UI over the world is not proof
of active team gameplay. Existing reference-client temporary-hunk warnings and
Q3's CD-key prompt after disconnect remain outside this checkpoint's correction
scope. No new unrelated engine issue was found.
