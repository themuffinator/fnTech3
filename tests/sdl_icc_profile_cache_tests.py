"""The SDL display-output query keeps the window's ICC profile instead of
reading it from disk on every call.

GLx asks for the display output every 15 frames (glx_postprocess.cpp), and
SDL_GetWindowICCProfile opens and reads the profile file each time: on a
frozen Medal of Honor view a twentieth of the GL client's main-thread stack
samples were in that read
(docs/fntech3/2026-10-28-mohaa-rtx-light-entities.md). The profile is read
again only for another window or display, or a second after the last read.
"""
import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def read_text(relative_path: str) -> str:
    return (ROOT / relative_path).read_text(encoding="utf-8").replace("\r\n", "\n")


def function_body(source: str, signature: str) -> str:
    start = source.index(signature)
    open_brace = source.index("{", start)
    depth = 0
    for index in range(open_brace, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return source[open_brace:index + 1]
    raise AssertionError(f"unterminated {signature}")


class SdlIccProfileCacheTests(unittest.TestCase):
    def test_the_profile_is_read_in_one_place_behind_its_cache(self) -> None:
        glimp = read_text("code/sdl/sdl_glimp.cpp")

        self.assertEqual(1, glimp.count("SDL_GetWindowICCProfile("))
        read = function_body(glimp, "static void GLW_ReadIccProfile( SDL_DisplayID display )")
        self.assertIn("SDL_GetWindowICCProfile( SDL_window, &iccSize )", read)
        # the cache answers for the same window and display within the interval
        guard = read[:read.index("SDL_GetWindowICCProfile")]
        for condition in ("glw_iccProfile.known", "glw_iccProfile.window == SDL_window",
                          "glw_iccProfile.display == display", "GLW_ICC_REFRESH_MSEC"):
            self.assertIn(condition, guard)
        # a clock that went backwards reads the profile again
        self.assertIn("now - glw_iccProfile.readTime >= 0", guard)
        self.assertRegex(glimp, r"#define GLW_ICC_REFRESH_MSEC 1000\b")

    def test_the_query_reports_the_kept_profile(self) -> None:
        glimp = read_text("code/sdl/sdl_glimp.cpp")
        query = function_body(glimp, "void GLimp_QueryDisplayOutput( rendererDisplayOutput_t *output )")

        self.assertIn("GLW_ReadIccProfile( display );", query)
        self.assertNotIn("SDL_GetWindowICCProfile", query)
        self.assertIn("output->iccProfileAvailable = qtrue;", query)
        self.assertTrue(re.search(r"output->iccProfileBytes = \( glw_iccProfile\.bytes > INT_MAX \)", query))
        # the read happens after the display is known, so a move to another
        # monitor reads that monitor's profile
        self.assertLess(query.index("display = SDL_GetDisplayForWindow( SDL_window );"),
                        query.index("GLW_ReadIccProfile( display );"))


if __name__ == "__main__":
    unittest.main()
