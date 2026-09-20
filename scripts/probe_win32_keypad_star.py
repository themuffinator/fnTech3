#!/usr/bin/env python3
"""Report how Windows presents the keypad's operator keys to a window.

code/win32/win_wndproc.cpp maps a key press through s_scantokey, indexed by
the scancode in lParam, and then remaps the result in one of two branches
depending on lParam's extended bit. Which branch a key takes, and so which
of that file's cases can ever answer for it, is a property of the keyboard
layout rather than of the engine. This probe reads that property for the
keypad keys, alongside the character the same layout produces for them, and
prints the s_scantokey entry each one lands on.

It asks the layout, not the keyboard: MapVirtualKeyExW and ToUnicodeEx are
called against the layout of the calling thread with a synthetic all-keys-up
modifier state. No key press is read, recorded, injected or waited for, and
no window is needed. The layout is reported so a result can be compared
across machines; a layout without the keypad star would report it missing
rather than differently. The s_scantokey reader is the one the input test
suite uses, so this probe and the pins there cannot disagree about the table.

Written for the keypad star record, docs/fntech3/2026-09-20-keypad-star.md.
"""
from pathlib import Path
import argparse
import ctypes
import ctypes.wintypes as wintypes
import json
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tests'))
from input_system_source_tests import array_entries  # noqa: E402

WNDPROC = 'code/win32/win_wndproc.cpp'
SCANTOKEY = 'static const int s_scantokey[ 128 ] ='
MAPVK_VK_TO_VSC_EX = 4

# The keypad's operator keys, plus the three others whose scancodes or
# branches the file's keypad cases depend on: num lock reaches K_PAUSE, the
# main slash shares a scancode with the keypad's, and enter reaches K_ENTER.
KEYS = (
    ('keypad *', 0x6A, 'VK_MULTIPLY'),
    ('keypad /', 0x6F, 'VK_DIVIDE'),
    ('keypad +', 0x6B, 'VK_ADD'),
    ('keypad -', 0x6D, 'VK_SUBTRACT'),
    ('keypad .', 0x6E, 'VK_DECIMAL'),
    ('keypad 0', 0x60, 'VK_NUMPAD0'),
    ('num lock', 0x90, 'VK_NUMLOCK'),
    ('main /', 0xBF, 'VK_OEM_2'),
    ('enter', 0x0D, 'VK_RETURN'),
)


def probe() -> dict:
    user32 = ctypes.WinDLL('user32', use_last_error=True)
    user32.MapVirtualKeyExW.restype = wintypes.UINT
    user32.GetKeyboardLayout.restype = wintypes.HKL
    layout = user32.GetKeyboardLayout(0)

    up = (ctypes.c_ubyte * 256)()
    buffer = ctypes.create_unicode_buffer(8)
    entries = array_entries((ROOT / WNDPROC).read_text(encoding='utf-8'),
                            SCANTOKEY)

    keys = []
    for label, vk, vk_name in KEYS:
        scancode = user32.MapVirtualKeyExW(vk, MAPVK_VK_TO_VSC_EX, layout)
        if not scancode:
            # The layout has no such key; say so rather than reading
            # s_scantokey[0] as though it did.
            keys.append({
                'key': label,
                'virtualKey': vk_name,
                'scancode': None,
                'extended': None,
                'branch': 'absent from this layout',
                'character': None,
                'scantokey': None,
            })
            continue
        extended = (scancode & 0xE000) == 0xE000
        written = user32.ToUnicodeEx(
            vk, scancode & 0xFF, up, buffer, len(buffer), 0, layout)
        character = ''.join(buffer[i] for i in range(written)) \
            if written > 0 else None
        # A dead key leaves state behind for the next call; flush it with a
        # space, which no probed key is.
        user32.ToUnicodeEx(0x20, 0x39, up, buffer, len(buffer), 0, layout)
        keys.append({
            'key': label,
            'virtualKey': vk_name,
            'scancode': scancode,
            'extended': extended,
            'branch': 'extended' if extended else 'not extended',
            'character': character,
            'scantokey': entries[scancode & 0xFF],
        })

    return {
        'layout': '0x%08x' % (layout & 0xFFFFFFFF),
        'source': WNDPROC,
        'keys': keys,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument('--output', type=Path,
                        help='write the reading to this JSON file')
    args = parser.parse_args()

    if sys.platform != 'win32':
        print('This probe reads a Windows keyboard layout; '
              'nothing to report on %s.' % sys.platform, file=sys.stderr)
        return 2

    reading = probe()
    print('keyboard layout %s, s_scantokey from %s'
          % (reading['layout'], reading['source']))
    print('%-9s %-12s %-9s %-13s %-9s %s'
          % ('key', 'virtual key', 'scancode', 'MapKey branch',
             'character', 's_scantokey[scancode & 0xFF]'))
    for key in reading['keys']:
        character = 'none' if key['character'] is None \
            else repr(key['character'])
        scancode = '-' if key['scancode'] is None \
            else '0x%04X' % key['scancode']
        print('%-9s %-12s %-9s %-13s %-9s %s'
              % (key['key'], key['virtualKey'], scancode,
                 key['branch'], character, key['scantokey'] or '-'))

    if args.output:
        args.output.write_text(json.dumps(reading, indent=2) + '\n',
                               encoding='utf-8')
        print('\nwrote %s' % args.output)
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
