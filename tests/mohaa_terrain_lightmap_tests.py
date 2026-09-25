"""Medal of Honor's terrain is lit by its patches' lightmaps.

A "2015" map's terrain patches each name a lightmap and a block of it
(OpenMoHAA renderergl1 tr_terrain.c, reference only).
code/renderercommon/tr_mohaa_world.c builds every patch into a triangle
surface of the translated map carrying that lightmap number, and every static
model into one carrying the renderers' vertex-lit number. Quake III's loader
lit every triangle surface by vertex, and a terrain vertex is white, so every
map's terrain drew unlit: at night `m1l2a`'s sand glared white where its
lightmap, 23 of 255 on average, has it dark. No map's own surface lump holds
a triangle surface, so for this profile the surface's own number is used."""
import pathlib
import re
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
RENDERERS = ('renderer', 'renderervk', 'rendererrtx')


def function(source, signature):
    start = source.index(signature)
    return source[start:source.index('\n}\n', start) + 3]


class MohaaTerrainLightmapTests(unittest.TestCase):
    def test_every_renderer_keeps_a_medal_of_honor_triangle_surfaces_lightmap(self):
        for renderer in RENDERERS:
            with self.subTest(renderer=renderer):
                bsp = (ROOT / 'code' / renderer / 'tr_bsp.c').read_text(encoding='utf-8')
                parse = function(bsp, 'static void ParseTriSurf(')
                self.assertIn('ri.gameId == COM_GAME_WOLFET || ri.gameId == COM_GAME_MOHAA ? lightmapNum : '
                              'LIGHTMAP_BY_VERTEX', parse)
                # the merged-lightmap remap applies to the same surfaces
                self.assertIn('if ( lightmapNum >= 0 && tr.mergeLightmaps ) {', parse)
                common = (ROOT / 'code' / renderer / 'tr_common.h').read_text(encoding='utf-8')
                self.assertRegex(common, r'#define LIGHTMAP_BY_VERTEX\s+-3\b')

    def test_terrain_carries_its_lightmap_and_static_models_are_vertex_lit(self):
        world = (ROOT / 'code/renderercommon/tr_mohaa_world.c').read_text(encoding='utf-8')
        self.assertRegex(world, r'#define MOHAA_LIGHTMAP_BY_VERTEX\s+-3\b')
        calls = [call for call in re.findall(r'R_MOHAAInitSurface\( [^;]*;', world)
                 if not call.startswith('R_MOHAAInitSurface( dsurface_t')]
        # terrain, static models and permanent decals
        self.assertEqual(len(calls), 3, calls)
        self.assertEqual(sum('patch->iLightMap' in call for call in calls), 1, calls)
        self.assertEqual(sum('MOHAA_LIGHTMAP_BY_VERTEX' in call for call in calls), 2, calls)
        init = function(world, 'static void R_MOHAAInitSurface(')
        self.assertIn('surface->surfaceType = LittleLong( MST_TRIANGLE_SOUP );', init)
        self.assertIn('surface->lightmapNum = LittleLong( lightmapNum );', init)


if __name__ == '__main__':
    unittest.main()
