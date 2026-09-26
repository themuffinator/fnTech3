# Interrupted HTTP response and fresh retry

At `6c7bfac`, both profiles were tested against a loopback HTTP server that
advertised the complete temporary add-on length, sent only its first 4,096
bytes, then closed the response. The add-on contained the selected retail map
from a legitimate local installation; all asset copies stayed in temporary
homes and were deleted after testing. The installed retail trees were unchanged.

Both clients reported `Transferred a partial file` with HTTP status 200.
After this expected drop, each home contained the 4,096-byte `.tmp` archive
and no final add-on archive. The owned probe process was then terminated:
`ERR_DROP` clears the pending command buffer, so that failure run is not
claimed as a normal scripted exit. A new client process reused the same home
and configuration while the HTTP server supplied the complete response.

Both retries produced an archive matching the server fixture SHA-256, completed
automatic HTTP reconnection and native/QVM pure admission as appropriate, and
continued receiving snapshots. Engine screenshots showed Q3 world/HUD and QL
native join UI/world. Retried clients and dedicated servers exited normally.
The [evidence record](2026-09-08-http-interruption.json) pins executable
hashes, exact startup scripts, failure messages, temporary-file observations,
retry hashes and process outcomes.

The probes disabled UDP fallback, forced windowed GLx, disabled input capture,
used isolated writable homes on an unswitched private desktop, and used only
engine screenshots. This proves rejection of a declared-length short response
and a full new download after restarting the client. It does not establish
byte-range resume, in-process recovery, remote packet-loss behavior, physical
storage-fault handling or clean shutdown of the first dropped client. No engine
change was needed. Existing image/font warnings and hosted CI's account
billing/spending blocker remain unresolved.
