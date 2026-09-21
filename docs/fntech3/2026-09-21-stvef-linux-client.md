# Elite Force Linux client interoperability

The Linux i686 client joins Windows retail Holomatch 1.20, Lilium Voyager 1.40
and fnTech3 servers, using the installed QVMs and four retail paks. Six local
sessions enter active play, record a demo, capture two engine images and exit
normally. Every client log identifies its actual renderer and first active
snapshot; every server reports `ClientBegin: 2` after its two bots.

| Windows server | Protocol | Pure | Verified Linux renderer | Recorded frames |
| --- | ---: | ---: | --- | ---: |
| Retail Holomatch | 24 | 0 | GLx | 40 |
| Retail Holomatch | 24 | 1 | Vulkan | 39 |
| Lilium Voyager | 26 | 0 | GLx | 32 |
| Lilium Voyager | 26 | 1 | RTX raster | 34 |
| fnTech3 | 24 | 1 | RTX raster | 39 |
| fnTech3 | 26 | 1 | Vulkan | 39 |

The local UDP relay observes both directions and the advertised protocol.
Protocol-26 sequence checksums have no mismatches. Protocol 24 does not carry
that checksum and is not described as passing it. The clients remain
stationary; bots provide world activity without controlling user input.

All six recordings play to normal completion in both the Linux fnTech3 client
and Windows Lilium. These twelve playbacks agree on each recording's frame
count shown above and produce an engine screenshot. The recorded extension
is `.efdemo` for protocol 24 and `.dm_26` for protocol 26.

Together with the [Linux server sessions](2026-09-21-stvef-linux-server.md),
this establishes both platform directions against retail and Lilium within
the measured protocol/pure combinations. The [renderer audit](2026-09-21-stvef-renderer-verification.md)
records the initial Linux client build and three software-rendered demo runs.
The same GCC 13.3 client and renderer modules are used here without production
code changes. Rendering uses Mesa llvmpipe on a private Xvfb display.

The first session's initial harness assertion looked for the server-only
`CS_ACTIVE` marker in the client log. The client had completed successfully;
its `CL_FirstSnapshot` message is emitted after setting `cls.state=CA_ACTIVE`.
Checking that message together with the server's `ClientBegin: 2` corrects the
assertion without relabelling a failed engine run or changing engine behavior.

The [manifest](evidence/2026-09-21-stvef-linux-client.json) records commands,
binary and asset identities, active-state/backend evidence, packet counts,
demo/frame hashes and capture hashes. Private logs, scripts and outputs remain
in `.tmp/stvef-gaps/linux-client-*` and `E:\Games\fnTech3-tests\linux-client-*`.
No passwords, GUIDs, proprietary media or screenshots are committed.

Clients are windowed with mouse and joystick disabled. Only engine screenshot
commands are used; installations are unchanged. The
[audio follow-up](2026-09-21-stvef-linux-audio.md) adds file-rendered output,
language-path selection and restart measurements. Physical input,
hardware ray tracing, long-duration gameplay and broader installations remain
outside this evidence. The Elite Force development gate remains enabled.
