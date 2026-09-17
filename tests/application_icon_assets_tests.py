"""Validate the shipped icon containers without optional imaging dependencies."""
import struct
import unittest
import zlib
from pathlib import Path

ICONS = Path(__file__).resolve().parents[1] / "assets" / "icons"


class ApplicationIconAssetsTests(unittest.TestCase):
    def check_png(self, data, size):
        self.assertEqual(data[:8], b"\x89PNG\r\n\x1a\n")
        position = 8
        compressed = bytearray()
        while position < len(data):
            length, kind = struct.unpack_from(">I4s", data, position)
            payload = data[position + 8:position + 8 + length]
            crc, = struct.unpack_from(">I", data, position + 8 + length)
            self.assertEqual(zlib.crc32(kind + payload), crc)
            if kind == b"IHDR":
                width, height, depth, color, _, _, interlace = struct.unpack(">IIBBBBB", payload)
                self.assertEqual((width, height, depth, color, interlace), (size, size, 8, 6, 0))
            if kind == b"IDAT":
                compressed.extend(payload)
            position += length + 12
        self.assertEqual(position, len(data))
        self.assertEqual(len(zlib.decompress(compressed)), size * (size * 4 + 1))

    def test_png_sizes_have_alpha_and_valid_pixels(self):
        for size in (16, 24, 32, 48, 64, 128, 256, 512, 1024):
            with self.subTest(size=size):
                self.check_png((ICONS / f"fntech3-{size}.png").read_bytes(), size)

    def test_windows_frames_match_png_exports(self):
        data = (ICONS / "fntech3.ico").read_bytes()
        self.assertEqual(struct.unpack_from("<HHH", data), (0, 1, 7))
        sizes = (16, 24, 32, 48, 64, 128, 256)
        previous_end = 6 + 16 * len(sizes)
        for index, size in enumerate(sizes):
            w, h, _, _, planes, bits, length, offset = struct.unpack_from("<BBBBHHII", data, 6 + index * 16)
            self.assertEqual((w or 256, h or 256, planes, bits), (size, size, 1, 32))
            self.assertEqual(offset, previous_end)
            self.assertEqual(data[offset:offset + length], (ICONS / f"fntech3-{size}.png").read_bytes())
            previous_end = offset + length
        self.assertEqual(previous_end, len(data))

    def test_macos_includes_retina_representations(self):
        data = (ICONS / "fntech3.icns").read_bytes()
        self.assertEqual(struct.unpack_from(">4sI", data), (b"icns", len(data)))
        expected = {b"icp4":16, b"icp5":32, b"icp6":64, b"ic07":128,
                    b"ic08":256, b"ic09":512, b"ic10":1024,
                    b"ic11":32, b"ic12":64, b"ic13":256, b"ic14":512}
        offset = 8
        while offset < len(data):
            kind, length = struct.unpack_from(">4sI", data, offset)
            self.assertGreater(length, 8)
            size = expected.pop(kind)
            self.assertEqual(data[offset + 8:offset + length], (ICONS / f"fntech3-{size}.png").read_bytes())
            offset += length
        self.assertFalse(expected)
        self.assertEqual(offset, len(data))


if __name__ == "__main__":
    unittest.main()
