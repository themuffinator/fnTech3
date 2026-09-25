"""RTCW single player's snooper scope in every renderer.

The scope's heat view draws the world with every lightmap white: RTCW's own
renderer binds tr.whiteImage for a lightmap bundle while the view carries
RDF_SNOOPERVIEW (its tr_shade.c R_BindAnimatedImage and DrawMultitextured).
fnTech3's renderers follow it where they choose a bundle's image; the GL and
RTX renderers bind through R_BindAnimatedImage, and the Vulkan renderer also
builds material descriptors through R_AnimatedImageDescriptor.
"""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]


def read(relative):
    return (ROOT / relative).read_text(encoding="utf-8")


def body(text, signature):
    start = text.index(signature)
    return text[start:text.index("\n}\n", start)]


WHITE_BIND = ("if ( bundle->lightmap != LIGHTMAP_INDEX_NONE && ri.gameId == COM_GAME_RTCW_SP &&\n"
              "\t\tbackEnd.refdef.rtcw.snooperView ) {\n\t\tGL_Bind( tr.whiteImage );\n\t\treturn;")


class RTCWSnooperViewTests(unittest.TestCase):
    def test_every_renderer_draws_the_snooper_view_with_white_lightmaps(self):
        for renderer in ("renderer", "renderervk", "rendererrtx"):
            bind = body(read(f"code/{renderer}/tr_shade.c"), "R_BindAnimatedImage( const textureBundle_t *bundle ) {")
            self.assertIn(WHITE_BIND, bind, renderer)
            # before a lightmap's own image, animated or not, is bound
            self.assertLess(bind.index(WHITE_BIND), bind.index("if ( bundle->numImageAnimations <= 1 ) {"), renderer)
        descriptor = body(read("code/renderervk/tr_shade.c"),
                          "static VkDescriptorSet R_AnimatedImageDescriptor( const textureBundle_t *bundle )")
        white = ("if ( bundle->lightmap != LIGHTMAP_INDEX_NONE && ri.gameId == COM_GAME_RTCW_SP &&\n"
                 "\t\tbackEnd.refdef.rtcw.snooperView ) {\n\t\treturn R_ImageDescriptor( tr.whiteImage );")
        self.assertIn(white, descriptor)
        self.assertLess(descriptor.index(white), descriptor.index("if ( bundle->numImageAnimations <= 1 ) {"))

    def test_only_single_player_has_the_scope(self):
        # RDF_SNOOPERVIEW exists in RTCW SP alone; the flag is read for that
        # profile, as the renderers' fog already reads it
        self.assertIn("ri.gameId == COM_GAME_RTCW_SP && backEnd.refdef.rtcw.snooperView",
                      read("code/renderercommon/tr_rtcw_fog_vk.h"))


if __name__ == "__main__":
    unittest.main()
