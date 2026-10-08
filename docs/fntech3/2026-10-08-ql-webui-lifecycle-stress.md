# Quake Live's web interface through renderer restarts, switches and a dead child, 2026-10-08

The ledger kept the web interface's lifecycle open: resize, renderer restart
and switch, and a crashed browser child, while keeping the native interface as
the fallback. One client run now takes it through each, with no input.
The [JSON companion](2026-10-08-ql-webui-lifecycle-stress.json) holds
identities, commands and capture hashes.

## Method

`.tmp/ql-webui-stress/probe.py` starts fnTech3's client on the `ql` profile.
It runs at 1280x720 in a window on a private desktop, with an isolated home,
the web interface on (`cl_webuiEnable 1`) and the installation's own
Awesomium runtime. An exec'd config then waits for the main menu and steps
through:

1. `vid_restart`;
2. `renderer_switch vk`, `renderer_switch rtx` and `renderer_switch glx`;
3. a resize to 1600x900 by `vid_restart`;
4. the death of the web child.

For the last step the config prints a marker, and the harness ends this
client's own `awesomium_process.exe`, found by parent process and killed by
PID. The client then waits, and runs `vid_restart` once more. Each step ends
with `web_status` and an engine `screenshotPNG`.

A control run does the same with the web interface off and no kill. The
installation was unchanged after both runs.

## Results

| Step | Web interface on | Web interface off (control) |
| --- | --- | --- |
| start, 1280x720 | main menu; document ready, not crashed | native screen |
| `vid_restart` | the same capture, byte for byte | the same native capture |
| Vulkan, RTX, back to GLx | the same capture, byte for byte, in each | the same native capture |
| resize to 1600x900 | main menu at the new size; document ready | native screen at the new size |
| child killed, 15 s on | "WebUI status is unavailable without a live browser"; the native screen, byte for byte the control's at 1600x900 | -- |
| 10 s later, then `vid_restart` | still the native screen, the same bytes | the same |

The process exited 0 without being stopped in both runs.

So the web document survives each restart and switch of all three renderers.
Its status reads the same at every step: ready, not crashed, no native error
(and `loading=1` throughout, as at the start). When its child dies, the client
falls back to the native interface and stays there across a further renderer
restart. It does not start a new child; a player gets the web
interface back by restarting the client.

Every renderer shutdown also logs three "Hunk_FreeTempMemory: not the final
block" lines, with the web interface on or off. That is a developer message
older than this work, not a lifecycle fault; it is reported separately.

## Open

Device loss, Linux and Proton, and a web interface brought back without
restarting the client remain open.
