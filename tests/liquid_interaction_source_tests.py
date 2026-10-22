from __future__ import annotations

import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
CLIENT_CGAME = ROOT / "code" / "client" / "cl_cgame.cpp"


def source_section(source: str, start: str, end: str) -> str:
    begin = source.index(start)
    finish = source.index(end, begin)
    return source[begin:finish]


class LiquidInteractionSourceTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.source = CLIENT_CGAME.read_text(encoding="utf-8")

    def test_feed_is_renderer_optional_and_cvar_guarded(self) -> None:
        feed = source_section(
            self.source,
            "static qboolean CL_LiquidInteractionFeedEnabled",
            "static void CL_RebuildLiquidEntityKinds",
        )

        self.assertIn("re.AddLiquidInteractionToScene", feed)
        self.assertIn('Cvar_VariableIntegerValue( "r_liquid" ) > 0', feed)
        self.assertIn('Cvar_VariableValue( "r_liquidRipples" ) > 0.0f', feed)
        self.assertIn("cl_liquidFeedCheckFrame != cls.framecount", feed)
        self.assertIn("CL_ResetLiquidMotionTracks", feed)

    def test_only_bodies_and_projectiles_are_classified(self) -> None:
        classify = source_section(
            self.source,
            "static void CL_LiquidCensusVisit",
            "static qboolean CL_TraceLiquidHit",
        )

        self.assertIn("liquid.projectiles[i] != eType", classify)
        self.assertIn("liquid.bodies[i] != eType", classify)
        self.assertIn("visit( cl.snap.ps.clientNum, ET_PLAYER, cl.snap.ps.eFlags, qtrue );", classify)
        self.assertIn("SNAPFLAG_SERVERCOUNT", classify)
        # The three profiles that keep their frames in their own records are
        # read from them.
        for census in ("CL_MOHAALiquidCensus( CL_LiquidCensusVisit );",
                       "CL_Q3IHVLiquidCensus( CL_LiquidCensusVisit );",
                       "CL_Q3TestLiquidCensus( CL_LiquidCensusVisit );",
                       "CL_LiquidSnapshotCensus( CL_LiquidCensusVisit );"):
            self.assertIn(census, classify)

    def test_each_profile_takes_its_own_liquid_bits_and_entity_types(self) -> None:
        contract = source_section(
            self.source,
            "static const clLiquidContract_t &CL_LiquidContract( void ) {",
            "// The profile's liquid bits, the mask its collision queries take.",
        )

        # Quake III's, and every profile that keeps it.
        self.assertIn("quake3 = { EF_TELEPORT_BIT, { ET_PLAYER }, 1, { ET_MISSILE }, 1 };", contract)
        # Holomatch's alt-fire missiles are type 4.
        self.assertIn("holomatch = { EF_TELEPORT_BIT, { ET_PLAYER }, 1, { ET_MISSILE, 4 }, 2 };", contract)
        # Jedi Academy multiplayer: NPCs type 13, teleports bit 8.
        self.assertIn("academy = { 8, { ET_PLAYER, 13 }, 2, { ET_MISSILE }, 1 };", contract)
        # Ritual's and 2015's bodies are the ones their client games splash;
        # neither has a projectile type.
        self.assertIn("ritual = { 4, { 1, 0 }, 2, { -1 }, 0 };", contract)
        self.assertIn("mohaa = { 4, { 3, 2, 1, 0 }, 4, { -1 }, 0 };", contract)
        for profile in ("COM_GAME_STVEF_MP", "COM_GAME_JKA_MP", "COM_GAME_ALICE", "COM_GAME_FAKK2", "COM_GAME_MOHAA"):
            self.assertIn("case %s:" % profile, contract)

        # The liquid bits are the collision contract's, per profile.
        contents = source_section(self.source, "static int CL_LiquidMask", "static void CL_ResetLiquidMotionTracks")
        self.assertIn("CM_LiquidMaskForGame( CL_LiquidGame() )", contents)
        self.assertIn("CM_LiquidContentsForGame( CL_LiquidGame(), CM_PointContents( origin, 0 ) )", contents)
        game = (ROOT / "code" / "qcommon" / "cm_game_contract.h").read_text(encoding="utf-8")
        for expected in ("gameId == COM_GAME_JKA_SP || gameId == COM_GAME_JKA_MP",
                         "gameId == COM_GAME_JK2_SP || gameId == COM_GAME_JK2_MP",
                         "0x2 | 0x4 | 0x20000", "( contents & 0x20000 ) ? CONTENTS_SLIME"):
            self.assertIn(expected, game)

    def test_audio_takes_each_profiles_liquid_and_occlusion_bits(self) -> None:
        audio = (ROOT / "code" / "client" / "audio" / "openal" / "AudioSystemShared.inl").read_text(encoding="utf-8")
        self.assertIn("return CONTENTS_SOLID | CM_OpaqueLiquidMaskForGame( AudioGame() );", audio)
        self.assertIn("return CM_LiquidContentsForGame( AudioGame(), contents ) != 0;", audio)
        self.assertNotIn("kLiquidContents", audio)
        self.assertNotIn("kOcclusionMask", audio)

    def test_crossings_use_world_liquid_collision_and_reject_teleports(self) -> None:
        motion = source_section(
            self.source,
            "static qboolean CL_TraceLiquidHit",
            "static void CL_UpdateLiquidEntityPosition",
        )

        self.assertIn("CM_PointContents", self.source)
        self.assertIn("CM_BoxTrace", motion)
        self.assertIn("const int mask = CL_LiquidMask();", motion)
        self.assertIn("!( trace.contents & mask )", motion)
        self.assertIn("CL_TraceLiquidHit( track->origin, origin, boundary )", motion)
        self.assertIn("CL_TraceLiquidHit( origin, track->origin, exitBoundary )", motion)
        self.assertIn("( ( track->eFlags ^ eFlags ) & CL_LiquidContract().teleportBit )", motion)

    def test_equal_time_samples_are_idempotent_for_stereo_rendering(self) -> None:
        motion = source_section(
            self.source,
            "static void CL_UpdateLiquidMotionTrack",
            "static void CL_UpdateLiquidEntityPosition",
        )

        self.assertIn("time < track->time", motion)
        self.assertIn("if ( time == track->time )", motion)
        self.assertLess(
            motion.index("if ( time == track->time )"),
            motion.index("elapsed = (int64_t)time - (int64_t)track->time"),
        )
        self.assertIn("(int64_t)time - (int64_t)track->wakeTime", motion)

    def test_existing_cgame_calls_feed_demo_stable_times(self) -> None:
        sound_case = source_section(
            self.source,
            "case CG_S_UPDATEENTITYPOSITION:",
            "case CG_S_RESPATIALIZE:",
        )
        render_case = source_section(
            self.source,
            "case CG_R_RENDERSCENE:",
            "case CG_R_SETCOLOR:",
        )

        self.assertLess(sound_case.index("S_UpdateEntityPosition"), sound_case.index("CL_UpdateLiquidEntityPosition"))
        self.assertIn("origin, cl.serverTime, kind->eFlags", self.source)
        self.assertIn("CL_UpdateLocalViewLiquidInteraction( refdef )", render_case)
        self.assertIn("refdef->time", self.source)
        self.assertLess(
            render_case.index("CL_UpdateLocalViewLiquidInteraction"),
            render_case.index("re.RenderScene"),
        )

    def test_every_other_scene_adapter_feeds_its_view_and_entities(self) -> None:
        client = ROOT / "code" / "client"

        def text(name: str) -> str:
            return (client / name).read_text(encoding="utf-8").replace("\r\n", "\n")

        # The view, after the entity editor has placed its camera and before
        # the renderer draws it.
        views = {
            "client_rtcw_render_compat.hpp": "CL_EntityEditorAdjustView( &view );\n\t\t\tCL_LiquidFeedView( &view );",
            "client_wolfet_render_compat.hpp": "CL_EntityEditorAdjustView( &view );\n\t\t\tCL_LiquidFeedView( &view );",
            "client_alice_cgame.hpp": "CL_LiquidFeedView( &view );\n\tre.RenderScene( &view );",
            "cl_mohaa_cgame.cpp": "CL_LiquidFeedView( &view );\n\tre.RenderScene( &view );",
            "cl_q3ihv_cgame.cpp": "CL_LiquidFeedView( &view );\n\tre.RenderScene( &view );",
            "cl_q3test_cgame.cpp": "CL_LiquidFeedView( &fd );\n\tre.RenderScene( &fd );",
        }
        for name, needle in views.items():
            with self.subTest(view=name):
                self.assertIn(needle, text(name))

        # Each entity position the audio hears.
        for name in ("client_rtcw_audio_compat.hpp", "client_wolfet_audio_compat.hpp"):
            with self.subTest(entity=name):
                self.assertIn("S_UpdateEntityPosition( entity, origin.data() );\n\t\tCL_LiquidFeedEntity( entity, origin.data() );",
                              text(name))
        self.assertIn("S_UpdateEntityPosition( entnum, origin );\n\tCL_LiquidFeedEntity( entnum, origin );",
                      text("cl_q3test_cgame.cpp"))
        # Ritual's and 2015's carried entities are heard at the listener and
        # have no position of their own.
        self.assertIn("if ( origin && !useListener ) {\n\t\tCL_LiquidFeedEntity( entnum, origin );", text("client_alice_cgame.hpp"))
        self.assertIn("if ( !useListener ) {\n\t\t\tCL_LiquidFeedEntity( entityNumber, origin );", text("cl_mohaa_cgame.cpp"))
        # The IHV Test reports positions only through its listener callback.
        ihv = text("cl_q3ihv_cgame.cpp")
        self.assertIn("IhvLiquidPositions( position );", ihv)
        self.assertIn("if ( !position || !CL_LiquidFeedWantsEntities() ) return;", ihv)

    def test_cgame_lifecycle_resets_visual_tracking(self) -> None:
        self.assertGreaterEqual(self.source.count("CL_ClearLiquidInteractionState();"), 2)

    def test_renderer_ripple_age_is_checked_before_widened_subtraction(self) -> None:
        for relative_path in ("code/renderer/tr_shade.c", "code/renderervk/tr_shade.c"):
            shade = (ROOT / relative_path).read_text(encoding="utf-8")
            ripple = source_section(
                shade,
                "const liquidInteraction_t *interaction =",
                "life = 1.0f - age",
            )

            active_check = "R_LiquidInteractionActive( interaction, backEnd.refdef.time )"
            age_assignment = "age = (float)( (int64_t)backEnd.refdef.time -"
            self.assertIn("float age;", ripple)
            self.assertIn(active_check, ripple)
            self.assertIn(age_assignment, ripple)
            self.assertIn("(int64_t)interaction->time", ripple)
            self.assertLess(ripple.index(active_check), ripple.index(age_assignment))
            self.assertNotIn("backEnd.refdef.time - interaction->time", ripple)


if __name__ == "__main__":
    unittest.main()
