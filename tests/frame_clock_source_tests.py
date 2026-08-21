"""Guard the per-frame clock contract.

Com_Milliseconds() is not a clock. Draining its event queue calls
Sys_SendKeyEvents(), which pumps the platform message loop; on Windows a single
pump was measured at 1.2-8.6 ms even when it produced no events, because the
windowing and input backends do their own device bookkeeping there. The value it
returns is only the Sys_Milliseconds() sample taken by the terminating null
event, so callers that want the current time must use Sys_Milliseconds().

Calling it from per-frame or per-object code turned the audio, Steam and
Workshop updates into ten OS event pumps per frame and produced recurring
8-17 ms gameplay hitches. These gates keep those paths on the plain timer while
leaving the legitimate journaled one-shot callers alone.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]

# Sources whose bodies run per frame, per voice, or per queued object. None of
# them may reach the event-pumping clock.
PUMP_FREE_SOURCES = (
    "code/client/audio/openal/AudioSystemWorld.inl",
    "code/client/audio/openal/AudioSystemStreams.inl",
    "code/client/audio/openal/AudioSystemBackend.inl",
    "code/client/audio/legacy/snd_dma.cpp",
    "code/client/cl_workshop.cpp",
    "code/platform/fnql_steam.cpp",
    "code/platform/fnql_workshop.cpp",
)

# Call sites that legitimately keep Com_Milliseconds: one-shot startup, seeding
# and module handoff, where journal-accurate replay is the point and the pump
# cost is paid once.
JOURNALED_ONE_SHOTS = {
    "code/client/cl_main.cpp": 1,
    "code/server/sv_game.cpp": 1,
    "code/server/sv_init.cpp": 1,
    "code/server/sv_factory.cpp": 1,
}

CALL = re.compile(r"(?<![A-Za-z0-9_])Com_Milliseconds\s*\(")
BLOCK_COMMENT = re.compile(r"^\s*(\*|/\*)")


def read(relative: str) -> str:
    return (ROOT / relative).read_text(encoding="utf-8", errors="replace")


def strip_comment(line: str) -> str:
    """Drop leading block-comment bodies and any trailing // remark."""
    if BLOCK_COMMENT.match(line):
        return ""
    return line.split("//", 1)[0]


def live_call_lines(text: str) -> list[int]:
    """Line numbers of Com_Milliseconds() calls that actually compile."""
    return [
        number
        for number, line in enumerate(text.splitlines(), start=1)
        if CALL.search(strip_comment(line))
    ]


def test_per_frame_sources_do_not_pump_the_event_queue() -> None:
    failures = []
    for relative in PUMP_FREE_SOURCES:
        hits = live_call_lines(read(relative))
        if hits:
            failures.append(
                f"{relative}: Com_Milliseconds() pumps the OS event queue and must "
                f"not run per frame; use Sys_Milliseconds() (lines {hits})"
            )
    assert not failures, "\n".join(failures)


def test_per_frame_sources_still_read_a_clock() -> None:
    """Guard against the sites being deleted rather than corrected."""
    failures = []
    for relative in PUMP_FREE_SOURCES:
        text = read(relative)
        if "Sys_Milliseconds" not in text and "msec" not in text:
            failures.append(f"{relative}: no millisecond clock left in the file")
    assert not failures, "\n".join(failures)


def test_audio_environment_probe_uses_the_plain_timer() -> None:
    """The measured hitch site: RefreshEnvironment() ran once per frame."""
    text = read("code/client/audio/openal/AudioSystemWorld.inl")
    start = text.find("void Q3SoundWorld::RefreshEnvironment()")
    assert start >= 0, "RefreshEnvironment() not found"
    body = text[start : start + 2000]
    assert "Sys_Milliseconds()" in body, "RefreshEnvironment() lost its plain timer"
    assert not CALL.search(body), "RefreshEnvironment() pumps the OS event queue again"


def test_journaled_one_shot_callers_are_unchanged() -> None:
    """Non-regression: the deliberate journaled callers stay as they are."""
    failures = []
    for relative, expected in JOURNALED_ONE_SHOTS.items():
        found = len(live_call_lines(read(relative)))
        if found != expected:
            failures.append(f"{relative}: expected {expected} Com_Milliseconds() call(s), found {found}")
    assert not failures, "\n".join(failures)


def test_com_milliseconds_documents_its_cost() -> None:
    text = read("code/qcommon/common.c")
    start = text.find("Com_Milliseconds")
    assert start >= 0
    header = text[start : text.find("int Com_Milliseconds( void ) {")]
    assert "Sys_Milliseconds()" in header, (
        "Com_Milliseconds() must point callers at the cheap clock"
    )


if __name__ == "__main__":
    failed = 0
    for name, fn in sorted(globals().items()):
        if name.startswith("test_") and callable(fn):
            try:
                fn()
                print(f"ok   {name}")
            except AssertionError as error:
                failed += 1
                print(f"FAIL {name}\n{error}")
    sys.exit(1 if failed else 0)
