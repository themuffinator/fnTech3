from __future__ import annotations

import math
import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def read(relative_path: str) -> str:
    return (ROOT / relative_path).read_text(encoding="utf-8", errors="strict")


def cvar_default(source: str, name: str) -> str:
    match = re.search(
        rf'ri\.Cvar_Get\(\s*"{re.escape(name)}"\s*,\s*"([^"]+)"', source
    )
    if match is None:
        raise AssertionError(f"renderer does not register {name}")
    return match.group(1)


def source_section(source: str, start: str, end: str) -> str:
    start_index = source.index(start)
    return source[start_index : source.index(end, start_index)]


def source_section_last(source: str, start: str, end: str) -> str:
    """Select a definition when the same signature also has a prototype."""
    start_index = source.rindex(start)
    return source[start_index : source.index(end, start_index)]


class RtxRendererContractSourceTests(unittest.TestCase):
    _LIGHT_SAFETY_MARKER = re.compile(
        r"(?:finite|isfinite|valid|safe|bounded|within[_A-Za-z]*range)",
        re.IGNORECASE,
    )

    def assert_light_validation_precedes_append(
        self,
        block: str,
        append_anchor: str,
        subjects: tuple[str, ...],
        label: str,
    ) -> None:
        append = block.index(append_anchor)
        prefix = block[:append]
        marker = self._LIGHT_SAFETY_MARKER.search(prefix)

        self.assertIsNotNone(marker, f"{label} has no finite/range validation")
        for subject in subjects:
            self.assertIn(
                subject,
                prefix,
                f"{label} does not validate {subject} before reserving a GPU slot",
            )

        assert marker is not None
        rejection = re.search(r"(?:continue|return)\b[^;]*;", prefix[marker.start() :])
        self.assertIsNotNone(
            rejection,
            f"{label} validation does not reject the light before the append",
        )

    def test_rtx_is_part_of_the_supported_three_renderer_contract(self) -> None:
        options = read("meson_options.txt")
        meson = read("meson.build")
        cmake = read("CMakeLists.txt")
        makefile = read("Makefile")
        workflow = read(".github/workflows/vulkan-verification.yml")

        self.assertIn("choices: ['glx', 'vk', 'rtx']", options)
        renderers_option = options[
            options.index("option('renderers'") : options.index("option('renderer-default'")
        ]
        self.assertIn("value: ['glx', 'vk', 'rtx']", renderers_option)
        self.assertIn("renderer_prefix + '_rtx_' + renderer_arch", meson)
        self.assertIn("renderer_static_rtx", meson)
        self.assertIn("-DRENDERER_RTX", meson)
        self.assertIn("renderer_default == 'rtx'", meson)

        focused_ci = source_section(
            workflow,
            "- name: Configure focused Meson Vulkan-family build",
            "  vulkan-gate-plans:",
        )
        self.assertIn("-Dbuild-client=true", focused_ci)
        self.assertIn("-Drenderer-default=${{ matrix.renderer }}", focused_ci)
        self.assertIn(
            '"fnql_${{ matrix.renderer }}_x86"',
            focused_ci,
        )
        self.assertIn("--cross-file misc/meson/linux-x86.ini", focused_ci)
        self.assertIn("ELF 32-bit LSB", focused_ci)

        # Static clients also provide ABI-level host-font fallbacks. Every
        # supported build frontend must keep the renderer's implementations
        # private so the two definitions cannot collide at final link time.
        for symbol in ("RE_DrawScaledText", "RE_MeasureScaledText"):
            remap = f"{symbol}=R_RTX_{symbol.removeprefix('RE_')}"
            self.assertIn(f"-D{remap}", meson)
            self.assertIn(remap, cmake)
            self.assertIn(f"-D{remap}", makefile)

        self.assertNotIn("x86_64", focused_ci)

    def test_rtx_exports_the_complete_current_renderer_abi_surface(self) -> None:
        public = read("code/renderercommon/tr_public.h")
        init = read("code/rendererrtx/tr_init.c")
        vulkan_init = read("code/renderervk/tr_init.c")
        common_font = read("code/renderercommon/tr_font.c")
        stash = read("code/renderercommon/tr_font_stash.c")

        # 30: Medal of Honor's model registration joined the table; 31: Elite
        # Force single player's scissor and raw-image members after it.
        self.assertIn("#define\tREF_API_VERSION\t\t35", public)
        self.assertIn("Com_Memset( &re, 0, sizeof( re ) );", init)
        exports = (
            "re.RegisterMOHAATiki = RE_RegisterMOHAATiki;",
            "re.AdvertisementBridge_UpdateLoadingViewParameters = AdvertisementBridge_UpdateLoadingViewParameters;",
            "re.DrawWebUISurface = RE_DrawWebUISurface;",
            "re.RegisterShaderFromRGBA = RE_RegisterShaderFromRGBA;",
            "re.DrawScaledText = RE_DrawScaledText;",
            "re.MeasureScaledText = RE_MeasureScaledText;",
            "re.GetScaledFontMetrics = RE_GetScaledFontMetrics;",
            "re.GetFontAtlasDebugShader = RE_GetFontAtlasDebugShader;",
            "re.SetUnderwaterView = RE_SetUnderwaterView;",
            "re.AddRTCWLightToScene = RE_AddRTCWLightToScene;",
            "re.SetRTCWFog = RE_SetRTCWFog;",
            "re.RegisterShader3D = RE_RegisterShader3D;",
        )
        for export in exports:
            self.assertIn(export, init)

        assignment_pattern = re.compile(r"\bre\.([A-Za-z_][A-Za-z0-9_]*)\s*=")
        self.assertEqual(
            set(assignment_pattern.findall(init)),
            set(assignment_pattern.findall(vulkan_init)),
            "RTX must export the complete current FnQL renderer ABI, not only the newest entries",
        )

        self.assertIn("R_InitHostFonts();", common_font)
        self.assertIn("R_DoneHostFonts();", common_font)
        self.assertIn("defined( RENDERER_RTX )", stash)

    def test_quake_live_bsp_47_and_advertisements_are_first_class(self) -> None:
        bsp = read("code/rendererrtx/tr_bsp.c")
        world = read("code/rendererrtx/tr_world.c")
        scene = read("code/rendererrtx/tr_scene.c")
        init = read("code/rendererrtx/tr_init.c")
        public = read("code/renderercommon/tr_public.h")

        self.assertIn('"../qcommon/ql_bsp.h"', bsp)
        self.assertIn("header->version != BSP_VERSION_QL", bsp)
        self.assertIn("QLBSP_LumpRangeValid", bsp)
        self.assertIn("QLBSP_ReadAdvertisementLump", bsp)
        self.assertIn("QLBSP_AdvertisementLumpShapeValid", bsp)
        self.assertIn("R_LoadAdvertisements( &qlAdvertisementsLump );", bsp)
        self.assertIn("void R_UpdateAdvertisements( void )", world)
        self.assertIn("advertisement->cellId", world)
        self.assertIn("AdvertisementBridge_GetCellDisplayState", public)
        self.assertIn("AdvertisementBridge_GetCellLabel", public)
        self.assertIn("AdvertisementBridge_UpdateLoadingViewParameters", scene)
        self.assertIn('ri.Cmd_AddCommand( "advertlist", R_AdvertisementList_f );', init)
        self.assertIn('ri.Cmd_RemoveCommand( "advertlist" );', init)

    def test_rtx_keeps_ql_view_flags_and_uses_profile_gated_fov_policy(self) -> None:
        renderer_sources = "\n".join(
            path.read_text(encoding="utf-8", errors="ignore")
            for path in (ROOT / "code" / "rendererrtx").glob("*.[ch]")
        )
        main = read("code/rendererrtx/tr_main.c")

        self.assertIn("RDF_NOFIRSTPERSON", main)
        self.assertNotIn("r_fovCorrection", renderer_sources)
        self.assertNotIn("RDF_NOFOVCORRECTION", renderer_sources)
        self.assertIn("R_ApplyProfileFovCorrection", renderer_sources)
        fov = read("code/renderercommon/tr_q3_fov.h")
        self.assertIn('ri.Cvar_VariableString( "com_gameProfile" )', fov)
        # Every profile registers the correction: no game is gated out of it.
        self.assertIn('quakeLive = !Q_stricmp( profile, "quakelive" ) ? qtrue : qfalse;\n'
                      '\t\tcorrection = ri.Cvar_Get( "r_fovCorrection", "1", CVAR_ARCHIVE_ND );', fov)
        # Quake Live's cgame fits its own Hor+ to four shapes; it takes a rule of
        # its own for the rest, never the 4:3 one.
        ql_branch = fov.split('if ( quakeLive ) {\n\t\tR_CorrectQLViewportFov(', 1)[1]
        self.assertLess(ql_branch.index('return;'), ql_branch.index('R_CorrectQ3ViewportFov('))

    def test_new_presentation_and_rt_features_are_conservative_by_default(self) -> None:
        init = read("code/rendererrtx/tr_init.c")
        expected_defaults = {
            "r_globalFog": "0",
            "r_fogMode": "0",
            # FnQ3's RTX path enables its raster HDR staging by default; this
            # is distinct from enabling ray tracing or HDR display output.
            "r_hdr": "1",
            "r_liquid": "0",
            "r_surfaceLightProxies": "0",
            # the RTX renderer path traces unless asked not to; a device that
            # cannot falls back to its raster path (rtx_rt_require 0)
            "rtx_rt_mode": "2",
            "rtx_rt_require": "0",
            "rtx_rt_async_overlap": "0",
        }
        for cvar, expected in expected_defaults.items():
            self.assertEqual(cvar_default(init, cvar), expected, cvar)
        # the replaced tracer's controls are gone rather than left inert
        for removed in ("rtx_rt_dynamic_blas", "rtx_rt_material_heuristics", "rtx_rt_raster_reference",
                        "rtx_rt_surface_light_max", "rtx_rt_temporal_enable", "rtx_rt_quality_preset"):
            self.assertNotIn(f'"{removed}"', init, removed)

    def test_global_fog_sidecar_is_not_touched_when_feature_is_disabled(self) -> None:
        bsp = read("code/rendererrtx/tr_bsp.c")
        loader = bsp[bsp.index("static void R_LoadGlobalFog") : bsp.index("static void R_LoadEntities")]
        gate = loader.index("!r_globalFog || !r_globalFog->integer")
        read_file = loader.index("ri.FS_ReadFile")
        self.assertLess(gate, read_file)
        self.assertIn("GLOBAL_FOG_SIDECAR_MAX_BYTES", loader)
        self.assertIn("int expectedSize;", loader)
        self.assertIn("int loadedSize;", loader)
        self.assertIn("loadedSize != expectedSize", loader)

    def test_webui_upload_is_exported_and_bounded_before_byte_arithmetic(self) -> None:
        backend = read("code/rendererrtx/tr_backend.c")
        init = read("code/rendererrtx/tr_init.c")
        block = backend[
            backend.index("void RE_DrawWebUISurface") : backend.index("RB_DebugPolygon")
        ]
        bounds = block.index("cols > 4096 || rows > 4096")
        multiplication = block.index("cols * rows * 4")
        self.assertLess(bounds, multiplication)
        self.assertIn("vk_upload_image_data", block)
        self.assertIn("re.DrawWebUISurface = RE_DrawWebUISurface;", init)

    def test_rt_buffers_do_not_confuse_vulkan_allocation_padding_with_capacity(self) -> None:
        header = read("code/rendererrtx/vk.h")
        vk = read("code/rendererrtx/vk.c")
        backend = read("code/rendererrtx/rt/rt_backend.c")
        create = source_section_last(
            vk, "static qboolean vk_rt_create_buffer( VkDeviceSize size", "static void vk_rt_destroy_buffer"
        )
        upload = source_section_last(vk, "static qboolean vk_rt_upload_buffer_data", "\n}\n")
        ensure = source_section(backend, "static qboolean RT_EnsureMapped(", "\n}\n")

        self.assertIn("VkDeviceSize capacity;", header)
        self.assertIn("VkDeviceSize allocationSize;", header)
        self.assertIn("outBuffer->capacity = size;", create)
        self.assertIn("outBuffer->allocationSize = memoryRequirements.size;", create)
        self.assertNotIn("outBuffer->capacity = memoryRequirements.size;", create)
        self.assertIn("uploadSize > buffer->capacity", upload)
        self.assertIn("flushSize > buffer->allocationSize", upload)
        # the path tracer's mapped buffers are reused only while their logical
        # capacity covers the request, and grow by doubling without wrapping
        self.assertIn("buffer->buffer.capacity >= minSize", ensure)
        self.assertIn("size > ( (VkDeviceSize)-1 ) / 2", ensure)
        self.assertLess(ensure.index("size > ( (VkDeviceSize)-1 ) / 2"), ensure.index("size *= 2;"))

    def test_rt_geometry_growth_is_checked_and_bad_indices_are_rejected_by_triple(self) -> None:
        vk = read("code/rendererrtx/vk.c")
        growth = source_section(
            vk, "static qboolean vk_rt_cpu_array_growth", "static qboolean vk_rt_cpu_geometry_reserve_materials"
        )
        triangles = source_section_last(
            vk, "static qboolean vk_rt_append_surface_triangles_geometry", "\n}\n"
        )

        self.assertIn("(size_t)INT_MAX / elementSize", growth)
        self.assertIn("additionalVertices > UINT32_MAX - geometry->numVertices", growth)
        self.assertIn("additionalIndices > UINT32_MAX - geometry->numIndices", growth)
        self.assertIn("vk_rt_checked_u32_multiply", vk)
        self.assertGreaterEqual(vk.count("vk_rt_checked_u32_multiply("), 6)

        self.assertIn("completeIndexCount = surface->numIndexes - surface->numIndexes % 3", triangles)
        self.assertGreaterEqual(triangles.count("i += 3"), 2)
        validation = triangles.index("surface->indexes[i + 0] < 0")
        append = triangles.index("vk_rt_cpu_geometry_add_triangle")
        self.assertLess(validation, append)
        self.assertIn("surface->indexes[i + 1]", triangles[validation:append])
        self.assertIn("surface->indexes[i + 2]", triangles[validation:append])

    def test_static_light_sidecar_is_bounded_strict_and_not_recursive(self) -> None:
        bsp = read("code/rendererrtx/tr_bsp.c")
        parser = bsp[bsp.index("#define STATIC_MAP_LIGHTS_SIDECAR_MAX_BYTES") : bsp.index("void R_StaticMapLightsReload_f")]
        loader = parser[parser.index("static void R_LoadStaticMapLightsForWorld") :]

        self.assertIn("STATIC_MAP_LIGHTS_SIDECAR_MAX_BYTES ( 256 * 1024 )", parser)
        self.assertIn("STATIC_MAP_LIGHTS_MAX_TOKENS 65536", parser)
        self.assertIn("STATIC_MAP_LIGHTS_MAX_DEPTH 32", parser)
        self.assertIn("char closers[STATIC_MAP_LIGHTS_MAX_DEPTH]", parser)
        self.assertIn("strtod( token, &end )", parser)
        self.assertIn("strtol( token, &end, 10 )", parser)
        self.assertIn("*end != '\\0'", parser)
        self.assertIn("errno == ERANGE", parser)
        self.assertIn("parsed >= -(double)FLT_MAX && parsed <= (double)FLT_MAX", parser)
        self.assertIn("R_StaticMapLightsMarkField", parser)
        self.assertIn("tr.staticMapLights.version != 1", parser)
        self.assertIn(
            "return ( *token || s_staticMapLightsTokenCount >= STATIC_MAP_LIGHTS_MAX_TOKENS ) ?",
            parser,
        )

        self.assertIn("expectedSize = ri.FS_ReadFile( filename, NULL );", loader)
        self.assertIn("loadedSize = ri.FS_ReadFile( filename, &buffer.v );", loader)
        self.assertIn("loadedSize != expectedSize", loader)
        self.assertIn("memchr( buffer.c, '\\0', (size_t)loadedSize )", loader)

    def test_rt_dlights_keep_raster_shadow_state_and_independent_rt_policy(self) -> None:
        header = read("code/rendererrtx/tr_local.h")
        scene = read("code/rendererrtx/tr_scene.c")
        dlight = source_section(
            header,
            "typedef struct dlight_s {",
            "} dlight_t;",
        )

        self.assertIn("qboolean castsRtShadows;", dlight)
        for raster_field in (
            "qboolean shadowEligible;",
            "qboolean shadowPlanned;",
            "int shadowIndex;",
            "int shadowAtlasBaseFace;",
            "int shadowAtlasFaceSize;",
            "int shadowAtlasX[DLIGHT_SHADOW_FACES];",
            "int shadowAtlasY[DLIGHT_SHADOW_FACES];",
            "int shadowReceiverCount;",
            "float shadowPriority;",
            "float shadowPriorityMultiplier;",
            "int shadowSpotSource;",
            "int shadowSpotSourceIndex;",
        ):
            self.assertIn(raster_field, dlight)

        dynamic = source_section(
            scene,
            "void RE_AddDynamicLightToScene",
            "void RE_AddLinearLightToScene",
        )
        linear = source_section(
            scene,
            "void RE_AddLinearLightToScene",
            "void RE_AddAdditiveLightToScene",
        )
        self.assertIn("dl->castsRtShadows = shadowEligible;", dynamic)
        self.assertIn("dl->shadowEligible = shadowEligible;", dynamic)
        self.assertIn(
            "additive, qtrue );",
            dynamic,
        )
        self.assertIn("dl->castsRtShadows = qtrue;", linear)
        self.assertIn("dl->shadowEligible = qtrue;", linear)

        for label, block in (("point", dynamic), ("linear", linear)):
            with self.subTest(light=label):
                self.assertIn("dl->shadowPlanned = qfalse;", block)
                self.assertIn("dl->shadowIndex = -1;", block)
                self.assertIn("dl->shadowPriorityMultiplier = 1.0f;", block)

        static_lights = source_section(
            scene,
            "static void R_AddStaticMapLightsToScene",
            "static float R_SurfaceLightProxy",
        )
        self.assertIn("dl->castsRtShadows = light->castsShadows;", static_lights)
        self.assertIn("dl->shadowSpotSource = SHADOW_SPOT_SOURCE_STATIC_MAP;", static_lights)
        self.assertIn("dl->shadowEligible = ( light->type == MAP_LIGHT_POINT", static_lights)
        self.assertIn("r_staticLightShadows && r_staticLightShadows->integer", static_lights)
        self.assertIn("dl->shadowPriorityMultiplier = light->designerPriority", static_lights)

    def test_raster_shadow_planning_is_unconditional_and_precedes_rt_trace(self) -> None:
        main = read("code/rendererrtx/tr_main.c")
        backend = read("code/rendererrtx/tr_backend.c")
        generate = source_section(
            main,
            "static void R_GenerateDrawSurfs( void )",
            "R_RenderView",
        )

        self.assertIn("R_AddEntitySurfaces();", generate)
        self.assertIn("R_PlanFrameShadows();", generate)
        entity_surfaces = generate.index("R_AddEntitySurfaces();")
        shadow_plan = generate.index("R_PlanFrameShadows();")
        self.assertLess(entity_surfaces, shadow_plan)
        self.assertNotIn(
            "rtx_rt_",
            generate,
            "raster shadow planning must still run when rtx_rt_mode is 0",
        )

        draw = source_section(
            backend,
            "static const void *RB_DrawSurfs( const void *data )",
            "static const void *RB_DrawBuffer( const void *data )",
        )
        ordered = (
            "tr.shadowManager = cmd->shadowManager;",
            "RB_RunScheduledShadowManagerPreMainPasses( cmd->drawSurfs, cmd->numDrawSurfs );",
            "RB_BeginDrawingView();",
            "RB_RenderDrawSurfList(",
            "vk_rt_trace_frame();",
        )
        for token in ordered:
            self.assertIn(token, draw)
        positions = [draw.index(token) for token in ordered]
        self.assertEqual(positions, sorted(positions))

    def test_authored_world_sun_state_drives_csm_without_losing_rt_inputs(self) -> None:
        header = read("code/rendererrtx/tr_local.h")
        bsp = read("code/rendererrtx/tr_bsp.c")
        main = read("code/rendererrtx/tr_main.c")

        self.assertIn("} worldSun_t;", header)
        for field_type, field_name in (
            ("worldSun_t", "worldSun"),
            ("vec3_t", "sunLight"),
            ("vec3_t", "sunColor"),
            ("float", "sunIntensity"),
            ("qboolean", "sunParmsValid"),
            ("vec3_t", "sunDirection"),
        ):
            self.assertRegex(header, rf"\b{field_type}\s+{field_name}\s*;")

        clear = source_section(
            bsp,
            "static void R_ClearWorldSun( void )",
            "static void R_SetWorldSunFromShader(",
        )
        authored = source_section(
            bsp,
            "static void R_SetWorldSunFromShader(",
            "static void R_ClearStaticMapLights(",
        )
        self.assertIn("Com_Memset( &tr.worldSun", clear)
        self.assertIn("tr.sunParmsValid = qfalse;", clear)
        self.assertIn("tr.worldSun.valid = qtrue;", authored)
        self.assertIn("VectorCopy( tr.worldSun.light, tr.sunLight );", authored)
        self.assertIn("VectorCopy( tr.worldSun.color, tr.sunColor );", authored)
        self.assertIn("tr.sunIntensity = tr.worldSun.intensity;", authored)
        self.assertIn("tr.sunParmsValid = qtrue;", authored)

        planner = source_section(
            main,
            "static void R_PlanCascadedShadows( void )",
            "/*\n=============\nR_PlaneForSurface",
        )
        self.assertIn("!tr.worldSun.valid", planner)
        self.assertIn("!tr.sunParmsValid", planner)
        self.assertIn("tr.sunIntensity <= 0.0f", planner)
        self.assertIn("tr.worldSun.lightDirection", planner)
        self.assertIn("tr.sunColor", planner)

    def test_rt_gpu_light_inputs_are_finite_and_bounded_before_append(self) -> None:
        lights = read("code/rendererrtx/rt/rt_lights.c")
        backend = read("code/rendererrtx/rt/rt_backend.c")

        # the test survives -ffast-math, which may fold v == v away
        valid = source_section(lights, "static qboolean RT_FloatValid(", "\n}\n")
        self.assertIn("( ( bits.u >> 23 ) & 0xffu ) == 0xffu", valid)
        self.assertLess(valid.index("0xffu ) == 0xffu"), valid.index("v >= -limit && v <= limit"))

        point = source_section(lights, "static void RT_AddPointLight(", "\n}\n")
        with self.subTest(input="point light"):
            self.assert_light_validation_precedes_append(
                point,
                "RT_AddLight( origin, reach )",
                ("photons", "origin", "color", "fade", "spotAxis", "radiusByDist"),
                "point light",
            )

        triangle = source_section(lights, "static void RT_AddTriangleLight(", "\n}\n")
        with self.subTest(input="surface light"):
            self.assert_light_validation_precedes_append(
                triangle,
                "RT_AddLight(",
                ("v0", "v1", "v2", "normal", "radiance"),
                "surface light triangle",
            )

        gameplay = source_section(
            backend,
            "for ( i = 0; i < (int)backEnd.refdef.num_dlights",
            "s_rt.numDlights = count;",
        )
        with self.subTest(input="gameplay dlight"):
            self.assert_light_validation_precedes_append(
                gameplay,
                "Com_Memset( light, 0, sizeof( *light ) );",
                ("dl->origin", "dl->color", "dl->origin2", "dl->radius"),
                "gameplay dynamic light",
            )

        parse = source_section(lights, "static int RT_ParseLightEntities(", "\n}\n")
        entity = source_section(lights, "static void RT_AddEntityLights( void )", "\n}\n")
        with self.subTest(input="world entity"):
            self.assertIn("RT_Vec3Valid( ent.origin, RT_LIGHT_MAX_COORD )", parse)
            self.assertIn("RT_Vec3Valid( ent.color, RT_LIGHT_MAX_VALUE )", parse)
            self.assertIn("!ent->hasOrigin", entity)
            self.assertLess(
                entity.index("RT_FloatValid( intensity, RT_LIGHT_MAX_VALUE )"),
                entity.index("RT_AddPointLight("),
            )
            self.assertLess(
                entity.index("RT_FloatValid( radius, RT_LIGHT_MAX_COORD )"),
                entity.index("RT_AddPointLight( ent->origin, color, intensity * RT_LIGHT_POINT_SCALE, qfalse, qtrue, 1.0f, axis"),
            )

        uniforms = source_section(backend, "static void RT_WriteFrameUniforms(", "\n}\n")
        sun = source_section(lights, "static void RT_FindSun( void )", "\n}\n")
        with self.subTest(input="sun"):
            accept = sun.index("s_rtLights.sunValid = qtrue;")
            self.assertLess(sun.index("RT_Vec3Valid( s_rtLights.sunLight, RT_LIGHT_MAX_VALUE )"), accept)
            self.assertLess(sun.index("RT_Vec3Valid( shader->rtSunDirection, RT_LIGHT_MAX_COORD )"), accept)
            self.assertLess(sun.index("VectorNormalize2( shader->rtSunDirection, s_rtLights.sunDirection ) == 0.0f"), accept)
            self.assertLess(uniforms.index("if ( s_rtLights.sunValid ) {"), uniforms.index("RT_FRAME_SUN"))

        grid = source_section(lights, "static qboolean RT_BuildLightGrid( void )", "\n}\n")
        with self.subTest(input="world bounds"):
            size = grid.index("VectorSubtract( worldModel->bounds[1], worldModel->bounds[0], size );")
            self.assertLess(grid.index("RT_Vec3Valid( worldModel->bounds[0], RT_LIGHT_MAX_COORD )"), size)
            self.assertLess(grid.index("RT_Vec3Valid( worldModel->bounds[1], RT_LIGHT_MAX_COORD )"), size)
            # a far-flung but valid world must not wrap the cell count past its cap
            self.assertIn(
                "(uint64_t)s_rtLights.dims[0] * (uint64_t)s_rtLights.dims[1] * (uint64_t)s_rtLights.dims[2]",
                grid,
            )

    def test_world_entity_duplicate_classname_uses_the_last_value(self) -> None:
        lights = read("code/rendererrtx/rt/rt_lights.c")
        parse = source_section(lights, "static int RT_ParseLightEntities(", "\n}\n")
        entity = source_section(lights, "static void RT_AddEntityLights( void )", "\n}\n")
        classname = source_section(
            parse,
            'if ( !Q_stricmp( key, "classname" ) ) {',
            '} else if ( !Q_stricmp( key, "origin" ) ) {',
        )

        # a repeated key overwrites the earlier value; the entity is judged a
        # light only once all its keys are read, so a later non-light
        # classname clears an earlier light classification
        self.assertIn("Q_strncpyz( ent.classname, token, sizeof( ent.classname ) );", classname)
        self.assertNotRegex(parse, r"\bisLight\b")
        self.assertIn('strncmp( ent->classname, "light", 5 )', entity)
        # q3map reads "light" and falls back to "_light" only when it is zero
        self.assertIn(
            "intensity = ent->light != 0.0f ? ent->light : ent->lightUnderscore != 0.0f ? ent->lightUnderscore : 300.0f;",
            entity,
        )

    def test_hybrid_frame_has_complementary_base_overlay_and_fail_closed_modes(self) -> None:
        backend = read("code/rendererrtx/tr_backend.c")
        rt = read("code/rendererrtx/rt/rt_backend.c")
        draw = source_section(
            backend,
            "static const void *RB_DrawSurfs( const void *data )",
            "static const void *RB_DrawBuffer( const void *data )",
        )
        included = source_section(backend, "static qboolean RB_DrawSurfIncluded(", "\n}\n")

        self.assertIn("vk_rt_primary_view_eligible()", draw)
        # base pass, trace, overlay in that order; the overlay's fog-only
        # passes for traced surfaces only follow a completed trace
        ordered = (
            "rtComposition ? RB_DRAWSURFS_RT_BASE : RB_DRAWSURFS_ALL",
            "rtTraceCompleted = vk_rt_trace_frame();",
            "rb_rtFogOverlay = qtrue;",
            "RB_RenderDrawSurfList( cmd->drawSurfs, cmd->numDrawSurfs, RB_DRAWSURFS_RT_OVERLAY );",
            "rb_rtFogOverlay = qfalse;",
        )
        positions = [draw.index(token) for token in ordered]
        self.assertEqual(positions, sorted(positions))
        self.assertIn(
            "rtx_rt_require 1 primary-view dispatch or scene-color copy failed; refusing silent raster fallback",
            draw,
        )
        # A depth-only base cannot stand as fallback. Failure restores all
        # shadow consumers then redraws the full list, with no extra overlay.
        failure = source_section(draw, "if ( !rtTraceCompleted ) {", "} else {")
        self.assertIn("rb_rtCompositionActive = qfalse;", failure)
        self.assertIn("RB_RunScheduledShadowManagerPreMainPasses", failure)
        self.assertIn("RB_BeginDrawingView();", failure)
        self.assertIn("RB_DRAWSURFS_ALL", failure)
        self.assertNotIn("RB_DRAWSURFS_RT_OVERLAY", failure)
        # every surface is drawn exactly once across the two lists
        self.assertIn("return owner != RT_SURFACE_RASTER ? qtrue : qfalse;", included)
        self.assertIn("return owner == RT_SURFACE_RASTER ? qtrue : qfalse;", included)
        self.assertIn("owner == RT_SURFACE_TRACED_FOGGED && fogOnly && rb_rtFogOverlay", included)
        # a failed pipeline hands every surface back to raster
        owner = source_section(rt, "rtSurfaceOwner_t vk_rt_surface_owner(", "\n}\n")
        self.assertIn("vk.caps.activeRtMode != 2 || s_rt.failed", owner)

    def test_traced_map_lights_follow_the_map_compilers_rules(self) -> None:
        backend = read("code/rendererrtx/rt/rt_backend.c")
        lights = read("code/rendererrtx/rt/rt_lights.c")
        bsp = read("code/rendererrtx/tr_bsp.c")
        lighting = read("code/rendererrtx/shaders/rt_lighting.glsl")
        common = read("code/rendererrtx/shaders/rt_common.glsl")

        # q3map and q3map2 cast no shadows from translucent surfaces that do
        # not filter light, nor from patches unless run with -patchshadows
        # (id's own maps were: rtx_rt_patch_shadows 1). Those world surfaces
        # go to their own instance, which the map's lights and sun ignore and
        # dynamic lights do not
        extract = (source_section(backend, "static qboolean RT_ExtractWorldSurface(", "\n}\n") +
                   source_section(backend, "static qboolean RT_ExtractWorld(", "\n}\n"))
        self.assertIn("shader->rtCompiledTranslucent ||", extract)
        self.assertIn(
            "*surface->data == SF_GRID && rtx_rt_patch_shadows && !rtx_rt_patch_shadows->integer", extract
        )
        self.assertEqual(cvar_default(read("code/rendererrtx/tr_init.c"), "rtx_rt_patch_shadows"), "1")
        trace = source_section(backend, "static qboolean RT_TraceFrame( void )", "\n}\n")
        self.assertLess(trace.index("rtx_rt_patch_shadows->modified"), trace.index("RT_BuildWorld()"))
        self.assertIn("if ( dsh->contentFlags & CONTENTS_TRANSLUCENT ) {", bsp)
        partition = source_section(backend, "static qboolean RT_PartitionTriangles(", "\n}\n")
        self.assertIn("bucket += RT_PART_NOSHADOW_OPAQUE;", partition)

        # alphashadow (and, for q3map2, lightfilter) surfaces filter the map's
        # light instead: a filter instance only filter rays see, traced ones
        # also in the no-shadow partition, blended ones there alone
        self.assertIn("shader->rtCompiledFilter |= RT_COMPILED_ALPHASHADOW;", bsp)
        self.assertIn("shader->rtCompiledFilter |= RT_COMPILED_LIGHTFILTER;", bsp)
        self.assertIn("RT_MapIsQ3map2() ? ( RT_COMPILED_ALPHASHADOW | RT_COMPILED_LIGHTFILTER ) :", extract)
        self.assertIn("if ( owner == RT_OWNER_RASTER && !filters ) {", extract)
        self.assertLess(extract.index("if ( filters ) {"), extract.index("shader->rtCompiledTranslucent ||"))
        self.assertIn("if ( marker & ( RT_MARK_NOSHADOW | RT_MARK_FILTER ) ) {", partition)
        self.assertIn("pass == RT_PART_FILTER && ( marker & RT_MARK_FILTER )", partition)
        self.assertIn("RT_CLASS_WORLD_FILTER,\n\t\t\tRT_MASK_FILTER", backend)
        for mask in ("RT_MASK_PRIMARY", "RT_MASK_SECONDARY", "RT_MASK_SHADOW"):
            value = re.search(rf"const uint {mask} = ([^;]*);", common)
            assert value is not None
            self.assertNotIn("RT_MASK_FILTER", value.group(1), mask)
        sample = source_section(lighting, "vec3 rt_sample_lights(", "\n}\n")
        self.assertIn("filtered = rt_trace_filter(origin, chosen.L, 0.01, chosen.dist - 0.1);\n"
                      "\t\ttransmission *= filtered;", sample)
        # (rtx_rt_debug_light traces its filters even where a surface blocks)
        self.assertIn("if ((visibility > 0.0 || debugLight != 0u) && !dynamicLight) {", sample)
        sun_visibility = source_section(lighting, "vec3 rt_sun_visibility(", "\n}\n")
        self.assertIn("return rt_trace_filter(origin, L, 0.01, skyT);", sun_visibility)
        filters = read("code/rendererrtx/shaders/rt_filter.glsl")
        self.assertIn("t = texel.a < 0.5 ? vec3(1.0) : vec3(0.0);", filters)
        self.assertIn("t *= 1.0 - texel.a;", filters)
        self.assertIn("t *= texel.rgb;", filters)
        # ... read, texel by texel, from the image the compilers filtered
        # with: q3map_lightImage, qer_editorimage or the shader's own name,
        # else a white, opaque image
        shader_text = read("code/rendererrtx/tr_shader.c")
        light_image = source_section(shader_text, "static void R_RtFindLightImage( void )", "\n}\n")
        self.assertLess(light_image.index("names[0] = rtLightImage;"), light_image.index("names[1] = rtEditorImage;"))
        self.assertLess(light_image.index("names[1] = rtEditorImage;"), light_image.index("names[2] = shader.name;"))
        self.assertIn("shader.rtLightImageMissing = qtrue;", light_image)
        self.assertIn("texel = texelFetch(u_textures[nonuniformEXT(image)], at, 0);", filters)
        self.assertIn("ivec2 at = clamp(ivec2(fract(uv) * vec2(size)), ivec2(0), size - 1);", filters)
        self.assertIn("if ((m.lightImage.y & RT_LIGHT_IMAGE_MISSING) == 0u) {", filters)

        # The compilers' light rays stop at any surface that neither lets
        # light through (trans) nor filters it, alpha-tested ones included:
        # the map's lights and sun trace solid rays, a dynamic light's rays
        # keep the alpha test
        self.assertIn("rt_trace_shadow_mask(origin, chosen.L, chosen.dist - 0.1, mask, !dynamicLight, reactive);", sample)
        self.assertIn("if (!trace_solid(origin, L, 0.01, 1e5, g_shadowMask | RT_MASK_SPECIAL)) {", sun_visibility)
        # the composite eases specular light with the irradiance near a lamp
        composite = read("code/rendererrtx/shaders/rt_composite.comp")
        self.assertIn("vec3 eased = shoulder_irradiance(E, 0.8, 1.25);", composite)
        self.assertIn("S *= peak > 0.0 ? max(max(eased.r, eased.g), eased.b) / peak : 1.0;", composite)
        self.assertIn("RT_CLASS_WORLD_NOSHADOW,\n\t\t\tRT_MASK_NOSHADOW", backend)
        shadow_mask = re.search(r"const uint RT_MASK_SHADOW = ([^;]*);", common)
        assert shadow_mask is not None
        self.assertNotIn("RT_MASK_NOSHADOW", shadow_mask.group(1))
        for mask in ("RT_MASK_PRIMARY", "RT_MASK_SECONDARY"):
            value = re.search(rf"const uint {mask} = ([^;]*);", common)
            assert value is not None
            self.assertIn("RT_MASK_NOSHADOW", value.group(1), mask)
        self.assertIn("bool dynamicLight = chosenIndex >= u_frame.counts.w;", lighting)
        self.assertIn("uint mask = dynamicLight ? (g_shadowMask | RT_MASK_NOSHADOW) : g_shadowMask;", lighting)

        # a surface on a light-grid cell boundary takes the lit side's cell
        sample = source_section(lighting, "vec3 rt_sample_lights(", "\n}\n")
        self.assertIn("rt_light_cell(P + Ng)", sample)
        self.assertNotIn("rt_light_cell(P)", sample)

        # the sun as the compiler found it: id's q3map from the first sky
        # surface's shader (sky by its BSP flags too), unit-length colour;
        # q3map2 from the first shader with one, largest component 1
        sun = source_section(lights, "static void RT_FindSun( void )", "\n}\n")
        self.assertIn("!RT_ShaderIsSky( shader )", sun)
        self.assertIn("if ( s_rtLights.noShaderSun ) {", sun)
        self.assertIn("VectorScale( color, 1.0f / length, color );", sun)
        self.assertLess(sun.index("if ( R_SkySunUnitLengthColour( ri.gameId, s_rtLights.q3map2 ) ) {"),
                        sun.index("1.0f / length"))
        self.assertIn("if ( dsh->surfaceFlags & SURF_SKY ) {", bsp)

    def test_native_material_ownership_is_conservative_for_retail_content(self) -> None:
        shader = read("code/rendererrtx/tr_shader.c")
        material = read("code/rendererrtx/rt/rt_material.c")
        classify = source_section(material, "static rtOwner_t RT_ClassifyOwner(", "\n}\n")

        # no name or surface-flag guesses: a material is its shader's stages
        self.assertNotIn("R_RtShaderNativeSupported", shader)
        self.assertNotIn("SURF_METALSTEPS", material)
        self.assertNotIn('"metal"', material)
        self.assertIn("R_RtSnapshotStages( stage );", shader)
        # the trace owns only opaque or masked depth-writing surfaces; blends,
        # deforms, decals, screen maps and skies stay raster
        for rule in (
            "shader->polygonOffset",
            "shader->numDeforms > 0",
            "shader->hasScreenMap",
            "SURF_NODRAW",
            "shader->sort > SS_OPAQUE && shader->sort != SS_SEE_THROUGH",
            "GLS_DEPTHMASK_TRUE",
            "GLS_DEPTHTEST_DISABLE | GLS_POLYMODE_LINE",
            "s->isScreenMap",
        ):
            self.assertIn(rule, classify, rule)
        self.assertLess(classify.index("RT_ShaderIsSky( shader )"), classify.index("shader->rtNumStages <= 0"))
        self.assertIn("return RT_OWNER_BASE_RASTER;", classify)
        # sky as the map's compiler saw it, whatever a later script says
        sky = source_section(material, "static qboolean RT_ShaderIsSky( const shader_t *shader )", "\n}\n")
        self.assertIn("shader->rtCompiledSky", sky)
        self.assertIn("if ( dsh->surfaceFlags & SURF_SKY ) {", read("code/rendererrtx/tr_bsp.c"))

    def test_companion_material_maps_follow_rend2_names_and_stay_path_tracer_only(self) -> None:
        shader = read("code/rendererrtx/tr_shader.c")
        find = source_section(shader, "static void R_RtFindMaterialMaps( void )", "\n}\n")
        # only a tracing renderer looks, and only for a lit material
        self.assertIn("!vk_rt_tracing()", find)
        self.assertIn("!R_RtSnapshotLit()", find)
        self.assertEqual(cvar_default(read("code/rendererrtx/tr_init.c"), "rtx_rt_material_maps"), "1")
        # OpenJK rend2's names, in its order: _nh, _n; _specGloss, _rmo, _orm
        order = [find.index(f'"{suffix}"') for suffix in ("_nh", "_n", "_specGloss", "_rmo", "_orm")]
        self.assertEqual(order, sorted(order))
        # data maps are neither light-scaled, compressed nor sRGB-decoded
        for flag in ("IMGFLAG_NOLIGHTSCALE", "IMGFLAG_NO_COMPRESSION", "IMGFLAG_COLORSPACE_DATA"):
            self.assertIn(flag, find)
        companion = source_section(shader, "static image_t *R_RtCompanionMap(", "\n}\n")
        # through the shader image policy, and never an image the shader draws
        self.assertIn("R_FindShaderImage( name, flags )", companion)
        self.assertIn("rtStageSnapshot[i].images[n] == image", companion)
        finish = source_section(shader, "static shader_t *FinishShader(", "\n}\n")
        self.assertLess(finish.index("R_RtSnapshotStages( stage );"), finish.index("R_RtFindMaterialMaps();"))
        # r_mapGreyScale leaves a normal map's directions alone
        self.assertIn("!( flags & IMGFLAG_COLORSPACE_DATA )", read("code/rendererrtx/tr_image.c"))

        # the GPU material names the maps, and the shaders' constants agree
        local = read("code/rendererrtx/tr_local.h")
        common = read("code/rendererrtx/shaders/rt_common.glsl")
        for name in ("SPECGLOSS", "RMO", "ORM"):
            host = re.search(rf"#define RT_MATERIAL_MAP_{name}\s+(\d+)", local)
            glsl = re.search(rf"const uint RT_MATERIAL_MAP_{name} = (\d+)u;", common)
            assert host is not None and glsl is not None
            self.assertEqual(host.group(1), glsl.group(1), name)
        translate = source_section(read("code/rendererrtx/rt/rt_material.c"), "static qboolean RT_TranslateMaterial(", "\n}\n")
        self.assertIn("(uint32_t)shader->rtMaterialStage < out->stageCount", translate)
        self.assertIn("out->flags |= RT_MF_MATERIAL_MAPS;", translate)

        # rend2's normal decoding and tangent frame; a metal's colour is its F0
        path = read("code/rendererrtx/shaders/rt_path.glsl")
        shading = source_section(path, "RtShading rt_shading(", "\n}\n")
        self.assertIn("vec2 xy = rt_stage_texel(s, h, time, lodBias).rg * 2.0 - 1.0;", shading)
        self.assertIn("float z = sqrt(max(1.0 - dot(xy, xy), 0.0));", shading)
        self.assertIn("if (dot(B, tdir) < 0.0) {", shading)
        self.assertIn("sh.F0 = mix(vec3(0.04), min(surf.reflectance, vec3(1.0)), metallic);", shading)
        self.assertIn("sh.diffuse = surf.reflectance * (1.0 - metallic);", shading)
        self.assertIn("sh.roughness = 1.0 - texel.a;", shading)
        # a material without maps shades as before
        early = shading[: shading.index("RtStage s = ")]
        self.assertIn("return sh;", early)
        for line in ("sh.N = h.normal;", "sh.diffuse = surf.reflectance;", "sh.F0 = vec3(material.params.z);",
                     "sh.roughness = material.params.x;"):
            self.assertIn(line, early)
        main = source_section(path, "void rt_path_main(", "\n}\n")
        self.assertIn("vec3 N = sh.N;", main)
        self.assertIn("vec3 reflectance = finite_or_zero(sh.diffuse);", main)
        self.assertIn("RtShading sh = rt_shading(material, tri, h, surf, geometric, lodBias);",
                      source_section(path, "vec3 bounce_radiance(", "\n}\n"))

    def test_glossy_reflections_share_surface_lights_with_light_sampling(self) -> None:
        # Multiple importance sampling (the balance heuristic) between the light
        # sample and the GGX reflection ray, for the q3map surface lights a
        # reflection can meet: the light sample keeps pLight / (pLight + pRefl)
        # of its specular light and the reflection the rest of the light it sees.
        path = read("code/rendererrtx/shaders/rt_path.glsl")
        lighting = read("code/rendererrtx/shaders/rt_lighting.glsl")
        weight = source_section(path, "float rt_light_weight(", "\n}\n")
        self.assertIn("return pLight + pReflection > 0.0 ? pLight / (pLight + pReflection) : 1.0;", weight)
        self.assertIn("float pLight = rt_chosen_light_pdf(choice, N, L);", weight)
        # a sky's emitters go to the reflection, which sees the sky itself: the
        # light sample would count them a second time
        self.assertLess(weight.index("RT_LIGHT_SKY"), weight.index("RT_LIGHT_TRACED"))
        self.assertIn("return 0.0;", weight[weight.index("RT_LIGHT_SKY"):weight.index("RT_LIGHT_TRACED")])
        reflected = source_section(path, "vec3 rt_reflected_light(", "\n}\n")
        self.assertIn("float share = pReflection + pLight > 0.0 ? pReflection / (pReflection + pLight) : 1.0;",
                      reflected)
        self.assertIn("return PI * light.color.rgb * share;", reflected)
        # both shares are taken along the path from the reflecting point, with
        # the reflection sampler's density where it traces rays at all
        self.assertIn("vec3 toLight = origin + dir * hit.hitT - g_misP;", reflected)
        pdf = source_section(path, "float ggx_reflection_pdf(", "\n}\n")
        self.assertIn("if (dot(N, L) <= 0.0 || dot(Ng, L) <= 0.0) {", pdf)
        self.assertIn("return ggx_d(NdotH, alpha) * NdotH / (4.0 * VdotH);", pdf)
        bounce = source_section(path, "vec3 bounce_radiance(", "\n}\n")
        self.assertIn("if (seen && g_mis && (material.flags & RT_MF_SURFACE_LIGHT) != 0u && !tri.dynamic) {",
                      bounce)
        main = source_section(path, "void rt_path_main(", "\n}\n")
        self.assertIn("bool mis = reflections && (u_frame.flags.x & RT_FRAME_MIS) != 0u;", main)
        self.assertLess(main.index("g_mis = !liquid && lobe == 1u && g_misLights;"), main.index("bounce_radiance(rayOrigin, L,"))

        # The resampling's normalizer is random, so the reflection side takes
        # the light sampler's density with the point it met in place of the
        # candidate that could have drawn it (the light's own place in an
        # enumerated cell, else the first candidate): the two shares then sum
        # to one on every path. Each candidate draws from its own stream, so
        # the reflection side draws the replaced one again.
        sample = source_section(lighting, "vec3 rt_sample_lights(", "\n}\n")
        self.assertIn("rt_light_candidate(P, N, cell, total, enumerate, globalShare, exclude, rt_choice_stream(choice, k), k,",
                      sample)
        self.assertLess(sample.index("choice.weightSum = wSum;"), sample.index("if (!(wSum > 0.0) || !(pSel > 0.0)) {"))
        at = source_section(lighting, "float rt_light_pdf_at(", "\n}\n")
        self.assertIn("slot = enumerate ? rt_local_light_slot(c.cell, index) : 0u;", at)
        self.assertIn("rt_light_candidate(P, N, c.cell, total, enumerate, c.globalShare, c.exclude, rt_choice_stream(c, slot), slot,",
                      at)
        self.assertIn(
            "float weightSum = max(c.weightSum - drawnWeight, 0.0) + (enumerate ? target : target / proposal);", at)
        candidate = source_section(lighting, "bool rt_light_candidate(", "\n}\n")
        self.assertIn("uint pick = enumerate ? k : min(uint(rand01(stream) * float(total)), total - 1u);", candidate)
        self.assertIn("if (!rt_light_eval(index, P, N, exclude, stream, s)) {", candidate)
        self.assertIn("global ? rt_light_proposal_pdf(cell, total, globalShare, index)", candidate)
        self.assertLess(candidate.index("if (!(p > 0.0))"), candidate.index("float proposal ="))
        self.assertIn("weight = p / proposal;", candidate)

        # a world triangle names the light it emits as; the frame flag follows
        # rtx_rt_mis, and changing it (or any setting the trace reads) starts
        # the still-frame average over
        backend = read("code/rendererrtx/rt/rt_backend.c")
        self.assertIn("RT_MapPrimitiveLights( &geometry, geometry.numIndices / 3u - counts[RT_PART_FILTER] )", backend)
        self.assertIn("( ( !rtx_rt_mis || rtx_rt_mis->integer ) ? RT_FRAME_MIS : 0u ) |", backend)
        self.assertEqual(cvar_default(read("code/rendererrtx/tr_init.c"), "rtx_rt_mis"), "1")
        settings = source_section(backend, "static int RT_TraceSettingsCount( void )", "\n}\n")
        for name in ("rtx_rt_lighting", "rtx_rt_bounces", "rtx_rt_specular", "rtx_rt_mis", "rtx_rt_sun_intensity"):
            self.assertIn(name, settings)
        uniforms = source_section(backend, "static void RT_WriteFrameUniforms(", "\n}\n")
        reset = uniforms[uniforms.index("if ( settings != s_rt.settingsCount ) {"):uniforms.index("still = (")]
        self.assertIn("s_rt.historyValid = qfalse;", reset)
        self.assertIn("s_rt.accumulated = 0;", reset)
        gpu = read("code/rendererrtx/rt/rt_gpu.h")
        common = read("code/rendererrtx/shaders/rt_common.glsl")
        for name in ("RT_FRAME_MIS", "RT_LIGHT_TRACED", "RT_LIGHT_SKY"):
            host = re.search(rf"#define {name}\s+\(\s*1u << (\d+)\s*\)", gpu)
            glsl = re.search(rf"const uint {name} = 1u << (\d+);", common)
            assert host is not None and glsl is not None, name
            self.assertEqual(host.group(1), glsl.group(1), name)

    def test_q3map2_extended_sun_spreads_as_the_compiler_did(self) -> None:
        # q3map_sunExt r g b intensity degrees elevation deviance samples: q3map2
        # makes that many suns, the first in the sun's direction and the rest
        # uniform over a disc of the deviance in azimuth and elevation
        # (shaders.c, light.c CreateSunLight)
        shader = read("code/rendererrtx/tr_shader.c")
        parse = shader[shader.index('!Q_stricmp( token, "q3map_sunExt2" ) ) {'):]
        parse = parse[:parse.index("continue;")]
        self.assertIn('const qboolean extended = !Q_stricmp( token, "q3map_sunExt" ) ? qtrue : qfalse;', parse)
        self.assertLess(parse.index("skySunDeviance = Q_atof( token );"), parse.index("skySunSamples = token[0] ? atoi( token ) : 0;"))
        self.assertIn("shader.rtSunDeviance = skySunDeviance;", shader)
        self.assertIn("shader.rtSunSamples = skySunSamples;", shader)
        backend = read("code/rendererrtx/rt/rt_backend.c")
        self.assertIn("u->sunShape[1] = 1.0f / (float)s_rtLights.sunSamples;", backend)
        lighting = read("code/rendererrtx/shaders/rt_lighting.glsl")
        sun = source_section(lighting, "vec3 rt_sample_sun(", "\n}\n")
        self.assertIn("if (rand01(seed) >= u_frame.sunShape.y) {", sun)
        self.assertIn("float r = deviance * sqrt(u);", sun)
        self.assertIn("L = vec3(cos(azimuth) * cos(elevation), sin(azimuth) * cos(elevation), sin(elevation));", sun)
        # without one, the sun keeps its small cone
        self.assertIn("float cosMax = u_frame.sunDirection.w;", sun)
        # id's q3map never read q3map_sunExt: a map lit by one is q3map2's,
        # though its build wrote no _q3map2_version, as are maps with
        # worldspawn keys only q3map2 reads; either is known before the
        # light entities are read
        self.assertIn("shader.rtSunExtended = skySunExtended;", shader)
        lights = read("code/rendererrtx/rt/rt_lights.c")
        build = source_section(lights, "static qboolean RT_BuildLights( void )", "\n}\n")
        self.assertLess(build.index("s_rtLights.q3map2Shaders = RT_WorldUsesQ3map2Shaders();"),
                        build.index("RT_AddEntityLights();"))
        self.assertIn("s_rtLights.q3map2 = ent->q3map2 || s_rtLights.q3map2Shaders;", lights)
        for key in ("_lightmapscale", "_blocksize", "_celshader", "_farplanedist", "_keepLights", "_floodlight"):
            self.assertIn(f'!Q_stricmp( key, "{key}" )', lights)

    def test_q3map2_sky_light_is_its_dome_of_suns(self) -> None:
        # q3map_skyLight <value> <iterations>: a negative value none, at least
        # two iterations (shaders.c); lit in the surface-light colour
        shader = read("code/rendererrtx/tr_shader.c")
        parse = shader[shader.index('!Q_stricmp( token, "q3map_skyLight" )'):]
        parse = parse[:parse.index("continue;")]
        self.assertIn("shader.rtSkyLightValue = MAX( Q_atof( token ), 0.0f );", parse)
        self.assertIn("shader.rtSkyLightIterations = MAX( atoi( token ), 2 );", parse)
        emit = source_section(shader, "static void R_RtResolveEmitColor( void )", "\n}\n")
        self.assertIn("!( shader.rtSkyLightValue > 0.0f )", emit)
        lights = read("code/rendererrtx/rt/rt_lights.c")
        build = source_section(lights, "static qboolean RT_BuildLights( void )", "\n}\n")
        compilers = build[build.index("\n\t} else {"):]
        self.assertLess(compilers.index("RT_FindSun();"), compilers.index("RT_FindSkyLight();"))
        self.assertIn("shader->rtSkyLightValue > 0.0f", source_section(lights, "static qboolean RT_WorldUsesQ3map2Shaders( void )", "\n}\n"))
        # the light samples reach the sky, not the diffuse bounces
        backend = read("code/rendererrtx/rt/rt_backend.c")
        self.assertIn("u->skyLight[3] = (float)s_rtLights.skyLightIterations;", backend)
        self.assertIn("u->skyImages[1][3] |= RT_SKY_LIGHTS;", backend)

        # the shader's dome is q3map2's CreateSkyLights loop in closed form
        lighting = read("code/rendererrtx/shaders/rt_lighting.glsl")
        dome = source_section(lighting, "vec3 rt_sample_sky_light(", "\n}\n")
        self.assertIn("uint perRow = rows * 4u;", dome)
        self.assertIn("uint suns = perRow * rows + 1u;", dome)
        self.assertIn("float elevation = (0.5 * PI / iterations) * (float(row) + 0.5);", dome)
        self.assertIn("float angle = (2.0 * PI / float(perRow)) * (float(column) + float(row) / float(rows));", dome)
        self.assertIn("light = u_frame.skyLight.rgb * cosR;", dome)
        self.assertIn("return light;", dome)
        # one visibility ray for either sky: a second call site stops the
        # 32-bit compute pipeline from being created
        self.assertNotIn("rt_sun_visibility(", dome)
        for iterations in (2, 3, 4, 6):
            with self.subTest(iterations=iterations):
                # CreateSkyLights, as q3map2 walks it
                rows, per_row = iterations - 1, (iterations - 1) * 4
                step, elevation_step = 2 * math.pi / per_row, math.radians(90.0 / iterations)
                loop = []
                angle, elevation = 0.0, elevation_step * 0.5
                for _ in range(rows):
                    for _ in range(per_row):
                        loop.append((angle, elevation))
                        angle += step
                    elevation += elevation_step
                    angle += step / rows
                closed = [(step * (column + row / rows), (0.5 * math.pi / iterations) * (row + 0.5))
                          for row in range(rows) for column in range(per_row)]
                self.assertEqual(len(loop) + 1, per_row * rows + 1)
                for (a1, e1), (a2, e2) in zip(loop, closed):
                    self.assertAlmostEqual(math.cos(a1), math.cos(a2), places=9)
                    self.assertAlmostEqual(math.sin(a1), math.sin(a2), places=9)
                    self.assertAlmostEqual(e1, e2, places=9)
        path = read("code/rendererrtx/shaders/rt_path.glsl")
        direct = source_section(path, "ShadeResult shade_direct(", "\n}\n")
        # a bounce's point samples one of the emitters, the sun and the sky
        # (oneSample), the one drawn counted for all
        self.assertIn("Esky *= share * rt_sun_visibility(origin, Lsky, skyReactive);",
                      direct)
        self.assertIn("if ((u_frame.flags.x & RT_FRAME_SPECULAR) != 0u && (sky == 0u || !reflections)) {", direct)

    def test_diffuse_paths_take_the_bounces_asked_for(self) -> None:
        # rtx_rt_bounces is the number of diffuse bounces (0-4); each hit adds
        # its light sample, weighted by the reflectances before it, and a
        # baked surface ends the path
        path = read("code/rendererrtx/shaders/rt_path.glsl")
        bounce = source_section(path, "vec3 bounce_radiance(", "\n}\n")
        self.assertIn("for (uint step = 0u; step < depth + 8u && d < depth; step++) {", bounce)
        self.assertIn("radiance += throughput * (sh.diffuse * irradiance + direct.additive + emission * u_frame.params0.y + "
                      "lightSeen);", bounce)
        self.assertIn("if (baked || !surf.lit || d + 1u >= depth) {", bounce)
        self.assertIn("throughput *= sh.diffuse;", bounce)
        # the reflection's MIS share belongs to its own hit
        self.assertIn("bool seen = reflection && d == 0u;", bounce)
        main = source_section(path, "void rt_path_main(", "\n}\n")
        self.assertIn("bool visible = lobe != 0u;", main)
        self.assertIn("bounce_radiance(rayOrigin, L, h.coneWidth, bakedLighting, visible, depth, medium, footprint, receiverReflectance, seed)", main)
        backend = read("code/rendererrtx/rt/rt_backend.c")
        self.assertIn("u->flags[1] = rtx_rt_bounces ? (uint32_t)Com_Clamp( 0, 4, rtx_rt_bounces->integer ) : 1u;", backend)
        self.assertIn('ri.Cvar_CheckRange( rtx_rt_bounces, "0", "4", CV_INTEGER );', read("code/rendererrtx/tr_init.c"))

    def test_dynamic_lights_reach_what_the_raster_light_pass_reaches(self) -> None:
        path = read("code/rendererrtx/shaders/rt_path.glsl")
        lighting = read("code/rendererrtx/shaders/rt_lighting.glsl")
        backend = read("code/rendererrtx/rt/rt_backend.c")
        upload = source_section(backend, "static qboolean RT_UploadLights( rtFrame_t *frame )", "\n}\n")
        uniforms = source_section(backend, "static void RT_WriteFrameUniforms(", "\n}\n")

        # the raster light pass's falloff, eased toward smoothstep by
        # r_dlightFalloff (light_frag.tmpl, tr_shade.c VK_SetLightParams)
        fragment = read("code/rendererrtx/shaders/light_frag.tmpl")
        self.assertIn("float smoothFactor = intensFactor * intensFactor * (3.0 - 2.0 * intensFactor);", fragment)
        self.assertIn("intensFactor = mix(intensFactor, smoothFactor, clamp(dlightFactors.x, 0.0, 1.0));", fragment)
        self.assertIn("uniformData->dlightFactors[0] = r_dlightFalloff ? r_dlightFalloff->value : 1.0f;",
                      read("code/rendererrtx/tr_shade.c"))
        evaluate = source_section(lighting, "bool rt_light_eval(", "\n}\n")
        self.assertIn("falloff = mix(falloff, falloff * falloff * (3.0 - 2.0 * falloff), "
                      "clamp(u_frame.dlightShape.x, 0.0, 1.0));", evaluate)
        self.assertIn("u->dlightShape[0] = r_dlightFalloff ? Com_Clamp( 0.0f, 1.0f, r_dlightFalloff->value ) : 1.0f;",
                      uniforms)

        # a shader without a lighting stage (nodlight, sky and the like) gets
        # no dynamic light in raster (tr_world.c), nor in the trace
        self.assertIn("if ( surf->shader->lightingStage < 0 ) {", read("code/rendererrtx/tr_world.c"))
        material = read("code/rendererrtx/rt/rt_material.c")
        nodlight = material[material.index("if ( shader->lightingStage < 0 ) {"):]
        self.assertIn("out->flags |= RT_MF_NODLIGHT;", nodlight[:nodlight.index("}")])
        direct = source_section(path, "ShadeResult shade_direct(", "\n}\n")
        excluded = direct[direct.index("if ((material.flags & RT_MF_NODLIGHT) != 0u) {"):]
        self.assertIn("exclude |= RT_LIGHT_DYNAMIC;", excluded[:excluded.index("}")])
        self.assertIn("light->color[3] = (float)( RT_LIGHT_DLIGHT | RT_LIGHT_DYNAMIC | ( additiveLight ? RT_LIGHT_ADDITIVE : 0u ) );",
                      upload)

        # rtx_rt_lighting 1 traces the dynamic lights over a lightmapped
        # surface's lightmap, and only them (dynamicOnly is `baked`)
        baked = direct[direct.index("if (baked) {"):direct.index("} else if (oneSample) {")]
        self.assertIn("r.irradiance = surf.baked;", baked)
        self.assertIn("pick = (u_frame.flags.w > 0u && (exclude & RT_LIGHT_DYNAMIC) == 0u) ? 1u : 0u;", baked)
        sample = source_section(lighting, "vec3 rt_sample_lights(", "\n}\n")
        self.assertIn("uvec2 cell = dynamicOnly ? uvec2(0u) : rt_light_cell(P + Ng);", sample)
        # A flash must invalidate quiet history even without a moving blocker.
        self.assertIn("if (dynamicLight) reactive = 1.0;", sample)
        self.assertIn("rt_sample_lights(P, N, Ng, candidates, passExclude, baked || pass == 1u, seed, L,\n"
                      "\t\t\tfound, passChoice, reactive) * share",
                      direct)

        # raster's stand-ins for surface lights are lights the trace casts
        # from the surfaces already; RTCW's light materials are no lights
        # (tests/rtx_dynamic_light_tests.py runs how they multiply)
        ordinary = upload[:upload.index("s_rt.numDlights = count;")]
        self.assertLess(ordinary.index("if ( dl->sourceFlags & DLIGHT_SOURCE_SURFACE ) {"),
                        ordinary.index("Com_Memset( light, 0, sizeof( *light ) );"))
        self.assertIn("if ( dl->rtcwShader ||", ordinary)
        self.assertIn("light->color[3] = (float)RT_LIGHT_MATERIAL;", upload)
        self.assertIn("u->materialLights[0] = s_rtLights.numLights + s_rt.numDlights;", uniforms)
        self.assertIn("u->materialLights[1] = s_rt.numMaterialLights;", uniforms)
        main = source_section(path, "void rt_path_main(", "\n}\n")
        # on world surfaces and brush models only, as raster; before the clamp
        self.assertIn("(!tri.dynamic || tri.v0.lightmapData.w > 0.5)) {", main)
        self.assertIn("vec3 factor = rt_material_light_factor(P);", main)
        for term in ("irradiance *= factor;", "specular *= factor;", "emission *= factor;"):
            self.assertLess(main.index(term), main.index("irradiance = clamp(finite_or_zero(irradiance)"))
        self.assertIn("if ( dl->rtcwShader ) continue; // Original material lights do not light models.",
                      read("code/rendererrtx/tr_mesh.c"))

        gpu = read("code/rendererrtx/rt/rt_gpu.h")
        common = read("code/rendererrtx/shaders/rt_common.glsl")
        for name in ("RT_MF_NODLIGHT", "RT_LIGHT_DYNAMIC"):
            host = re.search(rf"#define {name}\s+\(\s*1u << (\d+)\s*\)", gpu)
            glsl = re.search(rf"const uint {name} = 1u << (\d+);", common)
            assert host is not None and glsl is not None, name
            self.assertEqual(host.group(1), glsl.group(1), name)
        self.assertEqual(re.search(r"#define RT_LIGHT_MATERIAL\s+(\d+)u", gpu).group(1),
                         re.search(r"const uint RT_LIGHT_MATERIAL = (\d+)u;", common).group(1))
        self.assertLess(common.index("vec4 dlightShape;"), common.index("uvec4 materialLights;"))
        self.assertLess(gpu.index("float		dlightShape[4];"), gpu.index("uint32_t	materialLights[4];"))

    def test_the_lightmaps_light_the_world_only_when_asked(self) -> None:
        # The default lights every map with its own lights, traced: the light
        # entities it keeps, its sky and the surfaces that emit, nothing
        # baked. The lightmaps (1), the compilers' own lights and models (2)
        # and the emitters alone (3, as Q2RTX) stay for comparison; q3map2
        # left the light entities of every Quake Live and Enemy Territory
        # retail map out of its BSP, which the load reports there.
        lights = read("code/rendererrtx/rt/rt_lights.c")
        backend = read("code/rendererrtx/rt/rt_backend.c")
        build = source_section(lights, "static qboolean RT_BuildLights( void )", "\n}\n")
        emitters = build[build.index("if ( RT_LightingEmits( s_rtLights.mode ) ) {"):build.index("\n\t} else {")]
        level = emitters[emitters.index("if ( s_rtLights.mode == RT_LIGHTING_LEVEL ) {"):]
        self.assertIn("RT_AddEntityLights();", level[:level.index("}")])
        # the level's sky is the compilers' own; the emitters' an even one
        self.assertIn("RT_FindSkyLight();", emitters)
        self.assertIn("RT_FindSkyLightColor( s_rtLights.skyColor );", emitters)
        for call in ("RT_AddSurfaceLights();", "RT_AddEmissiveSurfaces();", "RT_FindSun();",
                     "RT_FindSkyLightColor( s_rtLights.skyColor );"):
            self.assertIn(call, emitters)
        compilers = build[build.index("\n\t} else {"):]
        self.assertLess(compilers.index("RT_AddEntityLights();"),
                        compilers.index("s_rtLights.lightsStripped = ( s_rtLights.q3map2 && s_rtLights.entityLights == 0 ) ? qtrue : qfalse;"))
        entities = source_section(lights, "static void RT_AddEntityLights( void )", "\n}\n")
        counted = entities[entities.index('if ( strncmp( ent->classname, "light", 5 ) ) {'):]
        self.assertLess(counted.index("s_rtLights.entityLights++;"), counted.index("if ( !ent->hasOrigin ) {"))
        uniforms = source_section(backend, "static void RT_WriteFrameUniforms(", "\n}\n")
        self.assertIn("const qboolean bakedLighting = s_rtLights.mode == RT_LIGHTING_LIGHTMAPS ? qtrue : qfalse;", uniforms)
        init = read("code/rendererrtx/tr_init.c")
        self.assertIn('ri.Cvar_CheckRange( rtx_rt_lighting, "0", "3", CV_INTEGER );', init)
        self.assertEqual(cvar_default(init, "rtx_rt_lighting"), "0")
        material = read("code/rendererrtx/rt/rt_material.c")
        self.assertIn("#define RT_LIGHTING_LEVEL\t\t0", material)
        self.assertIn("#define RT_LIGHTING_LIGHTMAPS\t1", material)
        self.assertIn("#define RT_LIGHTING_COMPILERS\t2", material)
        self.assertIn("#define RT_LIGHTING_EMITTERS\t3", material)
        # the load says which way the world is lit
        self.assertIn("q3map2 kept none of the map's light entities", build)
        self.assertIn("RTX RT: lit by an even sky of", build)
        # a change rebuilds the lights and which shaders are lights
        frame = backend[backend.index("if ( ( rtx_rt_lighting && rtx_rt_lighting->modified ) ||"):]
        frame = frame[:frame.index("if ( s_rt.defaultShader != tr.defaultShader ) {")]
        for line in ("RT_MaterialsFree();", "s_rt.worldReady = qfalse;"):
            self.assertIn(line, frame)

    def test_one_light_can_be_traced_alone(self) -> None:
        # rtx_rt_debug_light N lights what the trace sees with light N alone,
        # through the light sample's own shadow and filter rays (a second
        # traversal in the compute shader left the 32-bit driver unable to
        # build its pipeline); rtx_rt_debug_light_mode shows it unshadowed,
        # its shadow rays' blocker by class, or its light filters
        init = read("code/rendererrtx/tr_init.c")
        self.assertIn('rtx_rt_debug_light = ri.Cvar_Get( "rtx_rt_debug_light", "-1", CVAR_TEMP );', init)
        self.assertIn('ri.Cvar_CheckRange( rtx_rt_debug_light_mode, "0", "3", CV_INTEGER );', init)
        backend = read("code/rendererrtx/rt/rt_backend.c")
        uniforms = source_section(backend, "static void RT_WriteFrameUniforms(", "\n}\n")
        self.assertIn("(uint32_t)rtx_rt_debug_light->integer < u->materialLights[0] ) ? "
                      "(uint32_t)rtx_rt_debug_light->integer + 1u : 0u;", uniforms)
        settings = source_section(backend, "static int RT_TraceSettingsCount( void )", "\n}\n")
        self.assertIn("rtx_rt_debug_light, rtx_rt_debug_light_mode", settings)
        lighting = read("code/rendererrtx/shaders/rt_lighting.glsl")
        sample = source_section(lighting, "vec3 rt_sample_lights(", "\n}\n")
        alone = sample[sample.index("uint debugLight = dynamicOnly ? 0u : u_frame.materialLights.z;"):]
        alone = alone[:alone.index("choice.weightSum = wSum;")]
        for line in ("chosen = s;", "chosenIndex = debugLight - 1u;", "wSum = 1.0;", "pSel = 1.0;", "enumerate = true;"):
            self.assertIn(line, alone)
        self.assertEqual(lighting.count("rt_trace_shadow_mask("), 1)
        self.assertEqual(lighting.count("rt_trace_filter("), 2)
        self.assertIn("return mode == 1u ? chosen.E : (mode == 2u ? (visibility > 0.0 ? vec3(4.0) : blocked) :", sample)
        self.assertIn("RtHitPayload g_shadowBlocker;", read("code/rendererrtx/shaders/rt_trace_sw.glsl"))
        self.assertIn("RtHitPayload g_shadowBlocker;", read("code/rendererrtx/shaders/rt_trace_hw.glsl"))

    def test_the_traced_view_is_fogged_as_the_raster_pass_fogs_it(self) -> None:
        # RTCW's and ET's global fog (and Alice's far plane) fog each raster
        # draw by its distance from the eye (tr_rtcw_fog_vk.h, gen_frag.tmpl);
        # the trace skips those draws, so it takes the same fog per pixel
        path = read("code/rendererrtx/shaders/rt_path.glsl")
        composite = read("code/rendererrtx/shaders/rt_composite.comp")
        backend = read("code/rendererrtx/rt/rt_backend.c")
        raster = read("code/renderercommon/tr_rtcw_fog_vk.h")
        fragment = read("code/rendererrtx/shaders/gen_frag.tmpl")
        vertex = read("code/rendererrtx/shaders/gen_vert.tmpl")

        # the one fog equation, shared with the raster draws
        self.assertIn('#include "../../renderercommon/rtcw_fog.glsl"', fragment)
        self.assertIn('#include "../../renderercommon/rtcw_fog.glsl"', path)
        self.assertIn("rtcwFogDistance = rtcwFogParams.w != 0.0 ? length(eye) : abs(eye.z);", vertex)
        main = source_section(path, "void rt_path_main(", "\n}\n")
        fog = main[main.index("float fogVisibility = 1.0;"):main.index("info |= RT_PIXEL_OWNED;")]
        self.assertIn("if (u_frame.fogColor.w != 0.0 && (material.flags & RT_MF_NOFOG) == 0u) {", fog)
        # camera_ray is normalised, so the hit distance is the radial distance
        self.assertIn("return normalize(dir);", source_section(path, "vec3 camera_ray(", "\n}\n"))
        self.assertIn("rtcwFogVisibility(u_frame.fogColor, u_frame.fogParams,\n"
                      "\t\t\tu_frame.fogParams.w != 0.0 ? primary.hitT : abs(viewZ));", fog)
        self.assertIn("imageStore(u_outEmission, pixel, vec4(emission, fogVisibility));", main)
        # the composite blends toward the fog colour, in the final view only,
        # after the light is resolved and exposed (the fog never enters
        # accumulation); the fog colour is what raster shows through the fog
        final = composite[composite.index("if (view == RT_VIEW_FINAL) {"):composite.index("} else if (view == RT_VIEW_REFLECTANCE)")]
        self.assertIn("color = mix(pc.fog.rgb, color, clamp(emissionFog.a, 0.0, 1.0));", final)
        self.assertLess(final.index("uint mode = uint(pc.exposure.x + 0.5);"),
                        final.index("color = mix(pc.fog.rgb, color, clamp(emissionFog.a, 0.0, 1.0));"))
        self.assertIn("mix(rtcwFogColor.rgb, base.rgb,", fragment)

        # the draw a traced surface would get: not sky, not 2D, radial
        self.assertIn("FnRTCWFogDraw( &backEnd.refdef.rtcwFog,\n\t\ttess.shader && tess.shader->isSky ? qtrue : qfalse,",
                      raster)
        view_fog = source_section(backend, "static fnRTCWFogDraw_t RT_ViewFog( void )", "\n}\n")
        self.assertIn("return FnRTCWFogDraw( &backEnd.refdef.rtcwFog, qfalse, qfalse, qfalse, qfalse, qfalse, qtrue );",
                      view_fog)
        # ... or Alice's far plane, as VK_AliceFarFogDraw sets it
        for line in ("draw.color[3] = 1.0f;", "draw.params[1] = backEnd.viewParms.aliceFarDistance;",
                     "draw.params[2] = 1.0f;", "draw.params[3] = 1.0f;"):
            self.assertIn(line, view_fog)
            self.assertIn(line, raster)
        self.assertIn("backEnd.viewParms.aliceFarColor[channel] * tr.identityLight", view_fog)
        uniforms = source_section(backend, "static void RT_WriteFrameUniforms(", "\n}\n")
        self.assertIn("Com_Memcpy( u->fogColor, fog.color, sizeof( u->fogColor ) );", uniforms)
        self.assertIn("Com_Memcpy( u->fogParams, fog.params, sizeof( u->fogParams ) );", uniforms)
        self.assertIn("Com_Memcpy( push.fog, fog.color, sizeof( push.fog ) );", backend)

        # a nofog material stays clear unless a stage turns fog back on
        material = read("code/rendererrtx/rt/rt_material.c")
        nofog = material[material.index("if ( shader->rtcwNoFog ) {"):]
        nofog = nofog[:nofog.index("out->flags |= RT_MF_NOFOG;")]
        self.assertIn("if ( shader->stages[i]->active && shader->stages[i]->rtcwFog ) {", nofog)
        self.assertIn("tess.shader && tess.shader->rtcwNoFog ? qtrue : qfalse,", raster)
        self.assertIn("stage && stage->rtcwFog ? qtrue : qfalse,", raster)

        # the host and shader layouts agree
        gpu = read("code/rendererrtx/rt/rt_gpu.h")
        common = read("code/rendererrtx/shaders/rt_common.glsl")
        denoise = read("code/rendererrtx/shaders/rt_denoise_common.glsl")
        self.assertEqual(re.search(r"#define RT_MF_NOFOG\s+\(\s*1u << (\d+)\s*\)", gpu).group(1),
                         re.search(r"const uint RT_MF_NOFOG = 1u << (\d+);", common).group(1))
        self.assertLess(gpu.index("uint32_t	materialLights[4];"), gpu.index("float		fogColor[4];"))
        self.assertLess(gpu.index("float		fogColor[4];"), gpu.index("float		fogParams[4];"))
        self.assertLess(common.index("uvec4 materialLights;"), common.index("vec4 fogColor;"))
        self.assertLess(common.index("vec4 fogColor;"), common.index("vec4 fogParams;"))
        self.assertLess(gpu.index("uint32_t	view[4];"), gpu.index("float		fog[4];"))
        self.assertLess(denoise.index("uvec4 view;"), denoise.index("vec4 fog;"))
        # every Vulkan device offers 128 bytes of push constants; the exposure
        # takes the last 16
        self.assertIn("sizeof( rtDenoisePush_t ) == 128", gpu)
        self.assertLess(gpu.index("float		fog[4];"), gpu.index("float		exposure[4];"))
        self.assertLess(denoise.index("vec4 fog;"), denoise.index("vec4 exposure;"))

    def test_still_frames_start_over_when_a_frozen_scene_changes(self) -> None:
        # the still-frame average stands for a view whose camera, clock and
        # scene are unchanged: a model or dynamic light changed while the
        # clock stood still restarts it, as a setting does
        backend = read("code/rendererrtx/rt/rt_backend.c")
        extract = source_section(backend, "static qboolean RT_ExtractDynamic( void )", "\n}\n")
        self.assertLess(extract.index("s_rt.sceneHash = 2166136261u;"),
                        extract.index("RT_HashVertices( s_rt.sceneHash, geometry->vertices, geometry->numVertices );"))
        # only a frame whose clock stood still can be averaged, so only it
        # hashes the models (0.3 ms for 100,000 vertices)
        self.assertLess(extract.index("if ( s_rt.accumulateTime == backEnd.refdef.floatTime ) {"),
                        extract.index("RT_HashVertices( s_rt.sceneHash, geometry->vertices, geometry->numVertices );"))
        upload = source_section(backend, "static qboolean RT_UploadLights( rtFrame_t *frame )", "\n}\n")
        # not a light's radius, which Quake III redraws at random for its
        # powerups and flags even with the clock stopped
        self.assertIn("s_rt.sceneHash = RT_HashWords( s_rt.sceneHash, light->position, 3 );", upload)
        self.assertIn("s_rt.sceneHash = RT_HashWords( s_rt.sceneHash, light->color, 4 );", upload)
        frame = source_section(backend, "static qboolean RT_TraceFrame( void )", "\n}\n")
        order = [frame.index(token) for token in ("RT_ExtractDynamic()", "RT_UploadLights( frame )",
                                                   "RT_WriteFrameUniforms( frame, classPrims, weapon );")]
        self.assertEqual(order, sorted(order))
        uniforms = source_section(backend, "static void RT_WriteFrameUniforms(", "\n}\n")
        changed = uniforms[uniforms.index("if ( still && s_rt.sceneHash != s_rt.prevSceneHash ) {"):]
        changed = changed[:changed.index("s_rt.prevSceneHash = s_rt.sceneHash;")]
        self.assertIn("still = qfalse;", changed)
        # once: a scene that keeps changing is averaged as a moving one is
        self.assertIn("if ( s_rt.accumulated > 0 ) {\n\t\t\ts_rt.historyValid = qfalse;", changed)
        self.assertLess(uniforms.index("s_rt.prevSceneHash = s_rt.sceneHash;"),
                        uniforms.index("if ( still && rtx_rt_accumulate && rtx_rt_accumulate->integer ) {"))
        settings = source_section(backend, "static int RT_TraceSettingsCount( void )", "\n}\n")
        for name in ("r_dlightFalloff", "r_dlightScale"):
            self.assertIn(name, settings)

    def test_the_compute_world_tree_leaves_no_host_copy(self) -> None:
        # the 32-bit client has 2 GB of address space, and a large ET map
        # takes most of it; once the world's tree is on the GPU, the arrays
        # the frames' model trees reuse shrink back to their size
        backend = read("code/rendererrtx/rt/rt_backend.c")
        world = source_section(backend, 'ri.Printf( PRINT_ALL, "RTX RT: compute backend world BVH:', "\n}\n")
        self.assertIn("RT_BvhFree( &s_rt.bvh );", world)
        self.assertLess(world.index("RT_BvhFree( &s_rt.bvh );"), world.index("return qtrue;"))
        grow = source_section(read("code/rendererrtx/rt/rt_bvh.c"), "static qboolean RT_BvhGrow(", "\n}\n")
        self.assertIn("newCapacity = *capacity ? *capacity : 1024;", grow)
        self.assertIn("if ( *array ) {", grow)
        # the tracer's host arrays come from the C heap, which gives a large
        # block back when it is freed; the engine's zone never shrinks
        vk = read("code/rendererrtx/vk.c")
        self.assertIn("#define RT_HostAlloc( bytes ) malloc( (size_t)( bytes ) )", vk)
        # tests/rtx_light_entity_tests.py compiles rt_lights.c with these
        self.assertIn("#define RT_HostFree( block ) free( block )", vk)
        harness = read("tests/rtx_light_entity_tests.py")
        self.assertIn("#define RT_HostAlloc( bytes ) malloc( (size_t)( bytes ) )", harness)
        self.assertIn("#define RT_HostFree( block ) free( block )", harness)
        self.assertLess(vk.index("#define RT_HostAlloc("), vk.index('#include "rt/rt_material.c"'))
        for name in ("rt_backend.c", "rt_bvh.c", "rt_lights.c", "rt_material.c"):
            source = read(f"code/rendererrtx/rt/{name}")
            with self.subTest(file=name):
                self.assertNotIn("ri.Malloc(", source)
                self.assertNotIn("ri.Free(", source)
        geometry = vk[vk.index("static void vk_rt_cpu_geometry_free( rtxRtCpuGeometry_t *geometry )\r\n{")
                      if "\r\n" in vk else vk.index("static void vk_rt_cpu_geometry_free( rtxRtCpuGeometry_t *geometry )\n{"):]
        geometry = geometry[:geometry.index("static qboolean vk_rt_cpu_geometry_add_vertex(")]
        self.assertNotIn("ri.Malloc(", geometry)
        self.assertNotIn("ri.Free(", geometry)

    def test_screenshot_capture_preserves_fnql_metadata_watermark_and_safety_contracts(
        self,
    ) -> None:
        init = read("code/rendererrtx/tr_init.c")
        screenshot = init[
            init.index("qboolean RB_TakeScreenshotPNG")
            : init.index("const void *RB_TakeVideoFrameCmd")
        ]
        levelshot = init[
            init.index("void RB_TakeLevelShot")
            : init.index("static void R_ScheduleLevelShot")
        ]
        command = init[
            init.index("static void R_ScreenShot_f")
            : init.index("//============================================================================")
        ]

        self.assertIn('renderercommon/tr_levelshot.h"', init)
        self.assertIn("R_LevelshotCheckedRgbBytes", levelshot)
        self.assertIn(
            "R_LevelshotFloatIsFinite( r_levelshotDownscale->value )", init
        )
        self.assertGreaterEqual(
            screenshot.count("R_ApplyScreenshotWatermark("), 4
        )
        self.assertGreaterEqual(
            screenshot.count("R_WriteScreenshotViewpos("), 4
        )
        self.assertEqual(cvar_default(init, "r_screenshotWriteViewpos"), "0")
        self.assertEqual(cvar_default(init, "r_screenshotCaptureMode"), "0")
        self.assertIn(
            'r_screenshotWatermark = ri.Cvar_Get( "r_screenshotWatermark", ""',
            init,
        )
        self.assertIn("captureMode %s", init)
        self.assertIn("captureColorSpace %s", init)
        self.assertIn("return \"sdr-srgb\";", init)
        self.assertIn("R_WarnExplicitHdrScreenshotCapture();", command)
        self.assertIn("R_ExpandScreenshotPattern", init)
        self.assertIn("R_ValidScreenshotBaseName", command)
        self.assertIn("if ( ri.PublishGameScreenshot && checkname[0] )", command)
        self.assertIn(
            "ri.PublishGameScreenshot( checkname, checkname );", command
        )
        self.assertIn(
            "screenshot cubemap capture is not yet available with the ", command
        )
        self.assertIn("no files were written", command)

    def test_raster_fallback_keeps_enhanced_flare_wal_and_liquid_vbo_parity(
        self,
    ) -> None:
        flares = read("code/rendererrtx/vk_flares.c")
        image = read("code/rendererrtx/tr_image.c")
        vbo = read("code/rendererrtx/vk_vbo.c")

        flare_helper = source_section(
            flares,
            "static void RB_AddLensFlareSprites",
            "static void RB_RenderFlare",
        )
        flare_render = source_section(
            flares,
            "static void RB_RenderFlare",
            "void RB_RenderFlares",
        )
        self.assertIn('../renderercommon/tr_lens_flare.h"', flares)
        self.assertIn("r_flares->integer < 2", flare_helper)
        self.assertIn("tr.lensFlareShader", flare_helper)
        self.assertIn("R_LensFlareEdgeAttenuation", flare_helper)
        self.assertIn("R_LensFlareSpriteCount", flare_helper)
        self.assertIn("R_LensFlareSpritePosition", flare_helper)
        self.assertEqual(flare_helper.count("R_LensFlareSpriteColor"), 3)
        flare_call = "RB_AddLensFlareSprites( f, size, color, fogFactors );"
        self.assertIn(flare_call, flare_render)
        self.assertLess(
            flare_render.index("RB_EndSurface();"),
            flare_render.index(flare_call),
        )

        loaders = source_section(
            image,
            "static const imageExtToLoaderMap_t imageLoaders[]",
            "static const int numImageLoaders",
        )
        self.assertIn('{ "wal",  R_LoadWAL }', loaders)

        static_shader = source_section(
            vbo,
            "static qboolean isStaticShader",
            "static void VBO_AddGeometry",
        )
        liquid_rejection = static_shader.index("if ( r_fbo && r_fbo->integer")
        cached_acceptance = static_shader.index("if ( shader->isStaticShader )")
        self.assertLess(liquid_rejection, cached_acceptance)
        self.assertIn("r_liquid->integer > 0", static_shader)
        self.assertIn("shader->sort >= SS_FOG", static_shader)
        self.assertIn("R_LiquidShaderSupported( shader )", static_shader)
        self.assertIn(
            "R_LiquidContentsEnabled( shader->contentFlags, r_liquid->integer )",
            static_shader,
        )
        self.assertIn(
            "return qfalse;",
            static_shader[liquid_rejection:cached_acceptance],
        )

    def test_maintainer_docs_link_the_retail_rtx_smoke_gate(self) -> None:
        build = read("BUILD.md")
        readme = read("README.md")
        technical = read("docs/fnql/TECHNICAL.md")
        template = read("docs/templates/README.md.in")
        install_template = read("docs/templates/install-readme.html.in")
        contract = read("docs/fnql/RTX_RENDERER.md")
        release = read("scripts/release.py")

        self.assertIn("exactly three renderer modules", build)
        self.assertIn("-Drenderers=glx,vk,rtx", build)
        self.assertIn("docs/fnql/RTX_RENDERER.md", template)
        self.assertIn("docs/fnql/RTX_RENDERER.md", readme)
        self.assertIn("docs/fnql/RTX_RENDERER.md", install_template)
        self.assertIn("scripts/rtx_runtime_smoke.py", technical)
        self.assertIn('ROOT / "docs" / "fnql" / "RTX_RENDERER.md"', release)
        self.assertIn("supported three-renderer set", contract)
        self.assertIn("r_fullscreen 0", contract)
        self.assertIn("Steam app ID `282440`", contract)
        self.assertIn("A plan-only manifest", contract)
        self.assertIn(
            "rejects that subcommand without writing files", contract
        )


if __name__ == "__main__":
    unittest.main()
