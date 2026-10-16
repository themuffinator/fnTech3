from __future__ import annotations

import json
import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def read_source(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def function_body(source: str, name: str) -> str:
    start = source.index(f"{name}( void )")
    opening = source.index("{", start)
    depth = 1
    for index in range(opening + 1, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return source[opening + 1 : index]
    raise AssertionError(f"Unterminated {name}")


class LaunchAndTransitionSourceTests(unittest.TestCase):
    def test_vscode_launches_keep_profile_services_and_target_matching_binary(self) -> None:
        launch = json.loads(read_source(".vscode/launch.json"))
        inputs = {entry["id"]: entry for entry in launch["inputs"]}
        renderers = ("glx", "vk", "rtx")
        titles = {
            "q3": "Q3", "ql": "QL", "rtcw-sp": "RTCW SP", "rtcw-mp": "RTCW MP", "wolfet": "ET",
            "stvef-sp": "EF SP", "stvef-mp": "EF MP", "q3-ihv": "Q3 IHV", "jka-sp": "JKA SP",
            "jka-mp": "JKA MP", "jk2-sp": "JK2 SP", "jk2-mp": "JK2 MP", "alice": "Alice",
            "fakk2": "FAKK2", "mohaa": "MOHAA", "q3test": "Q3Test",
        }
        single_player = ("rtcw-sp", "stvef-sp", "jka-sp", "jk2-sp", "alice", "fakk2", "mohaa")
        # IHV's client is blocked. Jedi client entries are explicit module
        # boundary probes, matching their still-open development contracts.
        client_profiles = tuple(profile for profile in titles if profile != "q3-ihv")
        # Profiles added after ET bind loopback as ET's launches do.
        loopback_profiles = ("wolfet", "stvef-sp", "stvef-mp", "jka-sp", "jka-mp", "jk2-sp", "jk2-mp", "alice", "fakk2", "mohaa", "q3test")

        def input_values(value: str) -> list[str]:
            entry = inputs[value[len("${input:") : -1]]
            self.assertEqual(entry["type"], "pickString", value)
            values = [option if isinstance(option, str) else option["value"] for option in entry["options"]]
            self.assertEqual(len(values), len(set(values)), value)
            self.assertIn(entry["default"], values, value)
            return values

        self.assertTrue(launch["configurations"])
        combinations = set()
        pickers = set()
        named_dedicated = set()
        names = set()
        wolfet_homes = set()
        for configuration in launch["configurations"]:
            args = configuration["args"]
            name = configuration["name"]
            profile = args[args.index("com_gameProfile") + 1] if "com_gameProfile" in args else "auto"
            self.assertIn(profile, titles)
            steam_index = args.index("com_steamIntegration")
            self.assertEqual(args[steam_index + 1], "1" if profile == "ql" else "0", name)
            # Launching must not implicitly rebuild the engine. These profiles
            # intentionally target the selected, already-built x86 binary;
            # the separate build tasks below provide explicit Steam staging.
            self.assertNotIn("preLaunchTask", configuration, name)
            self.assertNotIn("disabled", name.lower())
            self.assertNotIn(name, names)
            names.add(name)
            self.assertIn("meson\\build\\win32-dev\\", configuration["program"])
            base_path = args[args.index("fs_basepath") + 1]
            self.assertEqual(inputs[base_path[len("${input:") : -1]]["type"], "promptString", name)
            home = args[args.index("fs_homepath") + 1]
            self.assertTrue(home.startswith(f"${{workspaceFolder}}/.tmp/vscode/{profile}/"), name)
            client = configuration["program"].endswith("\\fntech3.exe")
            if client:
                self.assertIn(profile, client_profiles, name)
                self.assertEqual(args[args.index("dedicated") + 1], "0", name)
                self.assertEqual(args[args.index("in_mouse") + 1], "${input:mouseInput}", name)
                self.assertEqual(inputs["mouseInput"]["default"], "2")
                look = "freelook" if profile in ("alice", "fakk2") else "cl_freelook"
                self.assertEqual(args[args.index(look) + 1], "${input:freeLook}", name)
            else:
                self.assertTrue(configuration["program"].endswith("\\fntech3.ded.exe"), name)
                self.assertEqual(args[args.index("dedicated") + 1], "1", name)
            command = "+spmap" if profile == "rtcw-sp" else "+map"
            if profile == "wolfet" and client:
                # ET's client launches start their maps as its client probe does.
                command = "+devmap"
            map_name = args[args.index(command) + 1] if command in args else None
            picker = bool(map_name) and map_name.startswith("${input:")
            if picker:
                input_values(map_name)
                pickers.add((profile, client))
                title = "MOHAA MP" if name == "MOHAA MP pick map" else titles[profile]
                self.assertEqual(name, f"{title} pick map" + ("" if client else " ded"))
                if profile != "alice":
                    self.assertEqual(args[args.index("logfile") + 1], "${input:launchLog}")
                    self.assertEqual(args[args.index("developer") + 1], "${input:launchDeveloper}")
                    if client:
                        self.assertEqual(args[args.index("r_mode") + 1], "${input:windowMode}")
                        self.assertEqual(args[args.index("s_backend") + 1], "${input:audioBackend}")
                    for setting in ("g_gameskill", "g_gametype", "sv_pure", "logfile", "developer", "s_backend", "r_mode"):
                        if setting in args:
                            self.assertLess(args.index(setting), args.index(command), name)
            if profile == "ql" and map_name:
                factory = args[args.index(command) + 2]
                if picker:
                    self.assertIn("ffa", input_values(factory))
                    self.assertEqual(inputs[factory[len("${input:") : -1]]["default"], "ffa")
                else:
                    self.assertEqual(factory, "ffa")
            if profile == "wolfet":
                # Each ET launch has its own writable home.
                self.assertNotIn(home, wolfet_homes, name)
                wolfet_homes.add(home)
            if profile in loopback_profiles:
                # These launches bind loopback only; the client, unlike the
                # dedicated server, also opens IPv6.
                self.assertEqual(args[args.index("net_ip") + 1], "127.0.0.1", name)
                if client:
                    self.assertEqual(args[args.index("net_ip6") + 1], "::1", name)
            if profile == "q3-ihv":
                # IHV's launch probes run offline; its sessions have their own probes.
                self.assertEqual(args[args.index("net_enabled") + 1], "0", name)
            mode = "SP" if profile in single_player else "ET" if profile == "wolfet" else "MP"
            if profile == "mohaa" and "g_gametype" in args and args[args.index("g_gametype") + 1] != "0":
                mode = "MP"
            if not client:
                self.assertNotIn("cl_renderer", args, name)
                if map_name and not picker:
                    named_dedicated.add(profile)
                    self.assertRegex(name, rf"^{mode} {re.escape(map_name)} .+ ded$")
                continue
            renderer = args[args.index("cl_renderer") + 1]
            if picker:
                self.assertEqual(input_values(renderer), list(renderers), name)
                self.assertTrue(home.endswith(("/" + renderer, "/" + map_name)), name)
            else:
                combinations.add((profile, renderer, bool(map_name)))
                if map_name:
                    self.assertRegex(name, rf"^{mode} {re.escape(map_name)} .+ {renderer}$")
                else:
                    self.assertEqual(name, f"{titles[profile]} {renderer}")
                    self.assertFalse(any(arg in args for arg in ("+map", "+spmap", "+devmap", "+spdevmap")))
                    self.assertNotIn("com_introplayed", args)
            self.assertEqual(args[args.index("r_fullscreen") + 1], "0")
            self.assertEqual(args[args.index("cl_webuiEnable") + 1], "1" if profile == "ql" else "0", name)
        self.assertEqual(combinations, {
            (profile, renderer, map_launch)
            for profile in client_profiles
            for renderer in renderers
            for map_launch in (False, True)
        })
        self.assertEqual(pickers, {(profile, True) for profile in client_profiles}
                         | {(profile, False) for profile in titles})
        self.assertEqual(named_dedicated, set(titles))

        tasks = json.loads(read_source(".vscode/tasks.json"))
        run_tasks = [task for task in tasks["tasks"] if task["label"].startswith("run:")]
        self.assertEqual(len(run_tasks), 32)
        task_inputs = {item["id"] for item in tasks["inputs"]}
        task_modes = set()
        picker_configs = {item["name"]: item for item in launch["configurations"] if " pick map" in item["name"]}
        for task in run_tasks:
            args = task["args"]
            profile = args[args.index("com_gameProfile") + 1]
            client = task["command"].endswith("\\fntech3.exe")
            task_modes.add((profile, client))
            key = f"{titles[profile]} pick map" + ("" if client else " ded")
            if task["label"] == "run: mohaa listen":
                key = "MOHAA MP pick map"
            self.assertEqual(args, picker_configs[key]["args"])
            self.assertNotIn("dependsOn", task)
            for token in args:
                for identifier in re.findall(r"\$\{input:([^}]+)\}", token):
                    self.assertIn(identifier, task_inputs, task["label"])
            if profile.startswith(("jka", "jk2")):
                self.assertIn("not playable support", task["detail"])
        self.assertEqual(task_modes, {(profile, True) for profile in client_profiles}
                         | {(profile, False) for profile in titles})

    def test_mohaa_launches_separate_campaign_and_multiplayer_modes(self) -> None:
        launch = json.loads(read_source(".vscode/launch.json"))
        inputs = {entry["id"]: entry for entry in launch["inputs"]}
        configs = {entry["name"]: entry for entry in launch["configurations"]}
        campaign = set(inputs["mohaaMap"]["options"])
        multiplayer = set(inputs["mohaaMpMap"]["options"])
        self.assertFalse(campaign & multiplayer)
        self.assertEqual(len(campaign | multiplayer), 54)
        self.assertIn("m1l1", campaign)
        self.assertIn("dm/mohdm1", multiplayer)
        self.assertTrue(all(name.startswith(("dm/", "obj/")) for name in multiplayer))
        self.assertEqual({item["value"] for item in inputs["mohaaMpGameType"]["options"]}, {"1", "2", "3", "4"})
        for renderer in ("glx", "vk", "rtx"):
            sp = configs[f"SP m1l1 Rangers Lead the Way {renderer}"]["args"]
            mp = configs[f"MP dm/mohdm1 South France {renderer}"]["args"]
            self.assertEqual(sp[sp.index("g_gametype")+1], "0")
            self.assertEqual(mp[mp.index("g_gametype")+1], "1")
            self.assertEqual(sp[sp.index("sv_maxclients")+1], "1")
            self.assertEqual(mp[mp.index("sv_maxclients")+1], "16")
            self.assertNotEqual(sp[sp.index("fs_homepath")+1], mp[mp.index("fs_homepath")+1])
        for name in ("MOHAA MP pick map", "MOHAA pick map ded"):
            args = configs[name]["args"]
            self.assertEqual(args[args.index("+map")+1], "${input:mohaaMpMap}")
            self.assertEqual(args[args.index("g_gametype")+1], "${input:mohaaMpGameType}")
        self.assertEqual(configs["MOHAA pick map"]["args"][-1], "${input:mohaaMap}")

    def test_vscode_build_tasks_stage_the_steam_provider(self) -> None:
        tasks = json.loads(read_source(".vscode/tasks.json"))["tasks"]
        build_tasks = [task for task in tasks if task.get("label", "").startswith("meson: build")]

        self.assertTrue(build_tasks)
        for task in build_tasks:
            self.assertEqual("-WithSteam" in task["args"], "(Steam)" in task["label"])
            platform_index = task["args"].index("-Platform")
            self.assertEqual(task["args"][platform_index + 1], "Win32")
            self.assertNotIn("-RunTests", task["args"])

        self.assertNotIn("meson: test", {task.get("label") for task in tasks})
        verify = next(task for task in tasks if task.get("label") == "meson: verify all games")
        self.assertIn("-RunTests", verify["args"])
        self.assertIn("-WolfETDevelopment", verify["args"])
        self.assertIn("-STVEFDevelopment", verify["args"])

    def test_game_transitions_release_the_browser_without_shutting_down_the_host(self) -> None:
        main = read_source("code/client/cl_main.cpp")
        map_loading = function_body(main, "CL_MapLoading")
        connect = function_body(main, "CL_Connect_f")
        webui = read_source("code/client/cl_webui.cpp")

        self.assertIn("CL_WebHost_HideForGameTransition();", map_loading)
        self.assertNotIn("CL_WebHost_Shutdown();", map_loading)
        self.assertIn("CL_Disconnect( qfalse );", map_loading)
        self.assertNotIn("CL_Disconnect( qtrue );", map_loading)
        self.assertIn("CL_WebHost_HideForGameTransition();", connect)
        self.assertLess(
            connect.index("CL_WebView_PublishGameStartForAddress"),
            connect.index("CL_WebHost_HideForGameTransition();"),
        )

        transition = function_body(webui, "CL_WebHost_HideForGameTransition")
        self.assertIn("cl_webui.keyCaptureArmed = qfalse;", transition)
        self.assertIn("CL_WebHost_HideBrowser();", transition)

        self.assertIn("static qboolean CL_WebHost_CommandStartsGameTransition", webui)
        native_request = webui[webui.index("static void CL_WebHost_ProcessNativeJavascriptRequest") :]
        hide = native_request.index("CL_WebHost_HideForGameTransition();")
        queue = native_request.index("Cbuf_ExecuteText( EXEC_APPEND")
        self.assertLess(hide, queue)

    def test_connection_and_level_load_screens_redraw_after_browser_release(self) -> None:
        screen = read_source("code/client/cl_scrn.cpp")
        main = read_source("code/client/cl_main.cpp")
        ui = read_source("code/client/cl_ui.cpp")
        cgame = read_source("code/client/cl_cgame.cpp")

        self.assertIn("uiMenuVisible = CL_UIMenusAreVisible();", screen)
        self.assertIn("cls.state == CA_LOADING || cls.state == CA_PRIMED || cls.state == CA_ACTIVE", screen)
        self.assertIn("uiFullscreen = false;", screen)
        self.assertIn("static void SCR_DrawNonGameBackdrop( void )", screen)
        self.assertIn("if ( cls.state != CA_ACTIVE )", screen)
        self.assertIn("SCR_DrawNonGameBackdrop();", screen)
        self.assertIn("drawConnectScreen = cls.state == CA_CONNECTING", screen)
        self.assertIn("browserOverlayAllowed = !drawConnectScreen && cls.state != CA_LOADING", screen)
        self.assertIn("&& cls.state != CA_PRIMED;", screen)
        self.assertIn("browserOverlayRequested = browserOverlayAllowed", screen)
        self.assertIn("uivm && ( !uiFullscreen || drawConnectScreen )", screen)
        browser_draw = screen[screen.index("// Native connection and level-loading screens"):]
        self.assertIn("if ( browserOverlayAllowed )", browser_draw)
        self.assertIn("CL_WebHost_DrawBrowserSurface();", browser_draw)
        self.assertIn("UI_DRAW_CONNECT_SCREEN, qfalse", screen)
        self.assertIn("UI_DRAW_CONNECT_SCREEN, qtrue", screen)

        # Retail connect.menu declares its backdrop as a 1920x1080 authored
        # surface. The cover crop must switch axes beyond 16:9 instead of
        # passing negative S coordinates to a clamped texture.
        self.assertIn("static qboolean SCR_ResolveAspectCoverCrop", screen)
        self.assertIn("if ( screenAspect > sourceAspect )", screen)
        self.assertIn("*t0 = ( 1.0f - visibleHeight ) * 0.5f;", screen)
        self.assertIn("else if ( screenAspect < sourceAspect )", screen)
        self.assertIn("*s0 = ( 1.0f - visibleWidth ) * 0.5f;", screen)
        self.assertNotIn("sourceWidth - screenWidth * ( sourceHeight / screenHeight )", screen)
        self.assertIn("static qboolean SCR_AdjustRetailAuthoredBackdropUV", screen)
        self.assertIn("retailS0 = ( 1.0f - screenAspect / sourceAspect ) * 0.5f;", screen)
        self.assertIn("SCR_FloatNear( width, screenWidth", screen)
        self.assertIn("SCR_FloatNear( *s0, retailS0", screen)
        self.assertIn("cls.state != CA_LOADING && cls.state != CA_PRIMED", screen)
        draw_start = ui.index("case UI_R_DRAWSTRETCHPIC:")
        draw_end = ui.index("case UI_R_MODELBOUNDS:", draw_start)
        draw_stretch_pic = ui[draw_start:draw_end]
        connect_adjust = "SCR_AdjustRetailConnectBackdropUV("
        menu_adjust = "CL_UIAdjustStretchPic( &x, &y, &w, &h );"
        self.assertIn("qboolean retailConnectBackdrop;", draw_stretch_pic)
        self.assertIn("retailConnectBackdrop = " + connect_adjust, draw_stretch_pic)
        self.assertIn("if ( !retailConnectBackdrop )", draw_stretch_pic)
        self.assertLess(
            draw_stretch_pic.index(connect_adjust),
            draw_stretch_pic.index(menu_adjust),
        )
        self.assertIn("re.DrawStretchPic( x, y, w, h, s0, t0, s1, t1, args[9] );", ui)
        self.assertIn("SCR_AdjustRetailCGameLoadingBackdropUV( x, y, w, h", cgame)
        self.assertIn("CL_HudQueueStretchPic( x, y, w, h, s0, t0, s1, t1, args[9] );", cgame)

        self.assertIn("!uiMenuVisible", screen)
        self.assertIn("!browserOverlayRequested", screen)
        self.assertIn("!CL_UIMenusAreVisible()", main)
        self.assertIn("KEYCATCH_BROWSER", main)

    def test_retail_disconnect_flow_restores_webui_with_native_fallback(self) -> None:
        main = read_source("code/client/cl_main.cpp")
        common = read_source("code/qcommon/common.c")
        keys = read_source("code/client/cl_keys.cpp")
        server = read_source("code/server/sv_init.cpp")
        webui = read_source("code/client/cl_webui.cpp")

        disconnect_start = main.index("qboolean CL_Disconnect( qboolean showMainMenu )")
        disconnect_end = main.index("void CL_ForwardCommandToServer", disconnect_start)
        disconnect = main[disconnect_start:disconnect_end]
        restore = function_body(main, "CL_RestoreMainMenuAfterDisconnect")

        self.assertLess(
            disconnect.index('Cvar_Set( "r_uiFullScreen", "1" );'),
            disconnect.index("CL_WebView_PublishGameEnd();"),
        )
        self.assertIn('Cvar_Set( "timescale", "1" );', disconnect)
        self.assertIn('CL_AddReliableCommand( "disconnect", qtrue );', disconnect)
        self.assertIn("CL_WritePacket( 2 );", disconnect)
        self.assertIn("CL_RestoreMainMenuAfterDisconnect();", disconnect)
        self.assertLess(
            disconnect.index("cls.state = CA_DISCONNECTED;"),
            disconnect.index("CL_RestoreMainMenuAfterDisconnect();"),
        )
        self.assertIn("CL_WebHost_ShowAfterDisconnect()", restore)
        self.assertIn("UI_SET_ACTIVE_MENU, UIMENU_MAIN", restore)
        self.assertIn(
            "Key_SetCatcher( ( Key_GetCatcher() & ~KEYCATCH_BROWSER ) | "
            "KEYCATCH_UI );",
            restore,
        )
        self.assertLess(
            restore.index("CL_WebHost_ShowAfterDisconnect()"),
            restore.index("UI_SET_ACTIVE_MENU, UIMENU_MAIN"),
        )

        show = function_body(webui, "CL_WebHost_ShowAfterDisconnect")
        self.assertIn("host.SetRenderingPaused( false )", show)
        self.assertIn("host.SetFocus( true )", show)
        self.assertIn("cl_webui.browserVisible = qtrue;", show)
        self.assertIn("cl_webui.browserActive = qtrue;", show)
        self.assertNotIn("CL_Awesomium_OpenURL", show)

        recovery_start = common.index("if ( code == ERR_DISCONNECT")
        recovery_end = common.index("} else {", common.index("} else if ( code == ERR_NEED_CD", recovery_start))
        recovery = common[recovery_start:recovery_end]
        self.assertNotIn("CL_Disconnect( qfalse );", recovery)
        self.assertGreaterEqual(recovery.count("CL_Disconnect( qtrue );"), 3)

        command_start = main.index("void CL_Disconnect_f( void )")
        command_end = main.index("void CL_Reconnect_f", command_start)
        command = main[command_start:command_end]
        self.assertIn('Com_Error( ERR_DISCONNECT, "Disconnected from server" );', command)
        self.assertNotIn("CL_Disconnect( qfalse )", command)
        local_shutdown = command.index('SV_Shutdown( "Server quit\\n" );')
        local_none = command.index("UI_SET_ACTIVE_MENU, UIMENU_NONE", local_shutdown)
        local_restore = command.index(
            "CL_RestoreMainMenuAfterDisconnect();", local_none
        )
        local_return = command.index("return;", local_restore)
        self.assertLess(local_shutdown, local_none)
        self.assertLess(local_none, local_restore)
        self.assertLess(local_restore, local_return)
        self.assertIn("if ( cls.state == CA_DISCONNECTED )", command)

        server_shutdown_start = server.index("void SV_Shutdown( const char *finalmsg )")
        server_shutdown = server[server_shutdown_start:]
        self.assertIn("CL_Disconnect( qfalse );", server_shutdown)

        self.assertIn('if ( !Q_stricmp( c, "disconnect" ) )', main)
        self.assertIn("cls.realtime - clc.lastPacketTime >= 3000", main)
        self.assertIn("NET_CompareAdr( from, &clc.netchan.remoteAddress )", main)
        self.assertIn("CL_Disconnect( qtrue )", keys)

    def test_retail_ui_coordinates_are_not_aspect_corrected_twice(self) -> None:
        ui = read_source("code/client/cl_ui.cpp")
        main = read_source("code/client/cl_main.cpp")

        self.assertIn('cl_menuAspect = Cvar_Get( "cl_menuAspect", "1", CVAR_ARCHIVE );', main)
        self.assertIn('Cvar_Flags( "cl_menuAspect" )', main)
        self.assertIn('Cvar_Get( "cl_menuAspectPolicyVersion", "0",', main)
        self.assertIn("ShouldMigrateMenuAspectToRetail", main)
        self.assertIn('Cvar_SetIntegerValue( "cl_menuAspect", 1 );', main)
        self.assertIn("The retail UI applies its centered 4:3 transform", ui)
        self.assertIn("!cl_menuAspect || cl_menuAspect->integer || cls.scale <= 0.0f", ui)
        self.assertIn("*x = ( *x - cls.biasX ) / cls.scale * xscale;", ui)
        self.assertIn("if ( !refdef || !cl_menuAspect || cl_menuAspect->integer )", ui)
        self.assertNotIn("SCR_AdjustFrom640Uniform( x, y, w, h );", ui)

    def test_retail_cgame_hud_and_fov_are_not_post_corrected_by_the_engine(self) -> None:
        cgame = read_source("code/client/cl_cgame.cpp")
        main = read_source("code/client/cl_main.cpp")
        public = read_source("code/renderercommon/tr_types.h")

        self.assertFalse((ROOT / "pkg/games/quakelive/baseq3/fnql-hud.json").exists())
        hud = read_source("code/client/cl_hud.cpp")
        # Every game whose HUD assumes a 4:3 screen; QL's and MoHAA's modules place their own.
        supported = " ".join(hud.split("\n\thudSupported = ", 1)[1].split(";", 1)[0].split())
        # Both Jedi multiplayer games' come through Raven's virtual 640x480 screen.
        self.assertEqual(supported, "Com_GameIsQuake3() || Com_GameIsRTCW() || Com_GameIsWolfET() "
                                    "|| Com_GameIsSTVEF() || Com_GameIsRitual() || Com_GameIsJKAMP() "
                                    "|| Com_GameIsJK2MP()")
        self.assertIn("if ( !hudSupported ) return qtrue;", hud)
        self.assertIn("CL_HudQueueStretchPic", cgame)
        self.assertNotIn("cl_hudAspect", main)
        self.assertNotIn("r_fovCorrection", main)
        self.assertIn("RDF_NOFOVCORRECTION 0x0002", public)
        fov = read_source("code/renderercommon/tr_q3_fov.h")
        self.assertIn('ri.Cvar_VariableString( "com_gameProfile" )', fov)
        self.assertIn('!Q_stricmp( profile, "quake3" )', fov)
        # Quake Live's cgame fits its own Hor+ to four shapes; it takes a rule of
        # its own for the rest, never the 4:3 one.
        ql_branch = fov.split('if ( quakeLive ) {\n\t\tR_CorrectQLViewportFov(', 1)[1]
        self.assertLess(ql_branch.index('return;'), ql_branch.index('R_CorrectQ3ViewportFov('))
        self.assertIn("if ( !correction || !correction->integer", fov)

    def test_screen_update_recursion_is_depth_bounded(self) -> None:
        screen = read_source("code/client/cl_scrn.cpp")
        update = function_body(screen, "SCR_UpdateScreen")

        self.assertIn("if ( recursive >= 2 )", update)
        self.assertIn("++recursive;", update)
        self.assertIn("--recursive;", update)
        self.assertNotIn("recursive = 1;", update)

    def test_internal_webui_state_cvars_do_not_retry_user_facing_rom_sets(self) -> None:
        webui = read_source("code/client/cl_webui.cpp")
        start = webui.index("static qboolean CL_WebUI_SetCvarIfChanged")
        end = webui.index("static const char *CL_WebUI_BackendProviderLabel", start)
        setter = webui[start:end]

        self.assertIn("Cvar_Set2( name, value, qtrue );", setter)
        self.assertNotIn("Cvar_Set( name, value );", setter)

    def test_webui_friend_snapshot_tracks_fallback_identity_changes(self) -> None:
        cgame = read_source("code/client/cl_cgame.cpp")
        config_start = cgame.index("static void CL_ConfigstringModified")
        config_end = cgame.index("static qboolean CL_GetServerCommand", config_start)
        config_update = cgame[config_start:config_end]
        mute_start = cgame.index("qboolean CL_ToggleSteamIdentityMute")
        mute_end = cgame.index("static uint64_t QL_CG_PackFloatBits64", mute_start)
        mute_update = cgame[mute_start:mute_end]

        self.assertIn(
            "index >= Com_GamePlayerConfigstringBase() &&",
            config_update,
        )
        self.assertIn("index < Com_GamePlayerConfigstringBase() + Com_GameClientLimit()", config_update)
        self.assertIn("CL_WebHost_InvalidateFriendSnapshot();", config_update)
        self.assertEqual(
            mute_update.count("CL_WebHost_InvalidateFriendSnapshot();"),
            2,
        )

    def test_vid_restart_invalidates_dynamic_webui_cvar_schema(self) -> None:
        client = read_source("code/client/cl_main.cpp")
        restart_start = client.index(
            "static void CL_Vid_Restart( refShutdownCode_t shutdownCode ) {"
        )
        restart_end = client.index("static void CL_Vid_Restart_f", restart_start)
        restart = client[restart_start:restart_end]

        self.assertIn("CL_StartHunkUsers();", restart)
        self.assertIn("CL_InitCGame();", restart)
        self.assertIn("CL_WebHost_InvalidateConfigSnapshot();", restart)
        self.assertGreater(
            restart.index("CL_WebHost_InvalidateConfigSnapshot();"),
            restart.index("CL_InitCGame();"),
        )

    def test_ip_database_shutdown_invalidates_aliases_before_freeing(self) -> None:
        server = read_source("code/server/sv_client.cpp")
        cleanup = function_body(server, "SV_FreeIP4DB")

        self.assertIn("iprange_t *allocation = ipdb_range;", cleanup)
        self.assertLess(cleanup.index("ipdb_range = nullptr;"), cleanup.index("SV_ZFree( allocation );"))
        self.assertLess(cleanup.index("ipdb_tld = nullptr;"), cleanup.index("SV_ZFree( allocation );"))
        self.assertIn("num_tlds = 0;", cleanup)

    def test_server_state_clearing_does_not_create_large_stack_temporaries(self) -> None:
        source = read_source("code/server/sv_init.cpp")
        clear_start = source.index("static void SV_ClearServer( void )")
        shutdown_start = source.index("void SV_Shutdown( const char *finalmsg )")
        clear = source[clear_start:shutdown_start]
        shutdown = source[shutdown_start:]

        self.assertIn("std::memset( &sv, 0, sizeof( sv ) );", clear)
        self.assertNotIn("sv = {};", clear)
        self.assertIn("std::memset( &svs, 0, sizeof( svs ) );", shutdown)
        self.assertNotIn("svs = {};", shutdown)

    def test_startup_system_console_flushes_before_the_main_message_pump(self) -> None:
        console = read_source("code/win32/win_syscon.cpp")

        self.assertIn("extern qboolean com_fullyInitialized;", console)
        self.assertIn("if ( !com_fullyInitialized && s_wcd.hwndBuffer && conBufPos )", console)
        self.assertIn("AddBufferText( conBuffer, conBufPos );", console)
        self.assertIn("UpdateWindow( s_wcd.hwndBuffer );", console)

    def test_windowed_windows_rendering_never_changes_the_desktop_gamma_ramp(self) -> None:
        gamma = read_source("code/win32/win_gamma.cpp")
        wndproc = read_source("code/win32/win_wndproc.cpp")

        start = gamma.index("void GLimp_SetGamma(")
        end = gamma.index("void GLW_RestoreGamma(", start)
        setter = gamma[start:end]
        guard = setter.index("if ( !glw_state.cdsFullscreen )")
        ramp = setter.index("GLW_WriteGammaRamp(")
        self.assertLess(guard, ramp)
        self.assertIn("GLW_RestoreGamma();", setter[guard:ramp])

        # A pass-through request means the renderer already owns gamma, so the
        # display keeps its own calibration instead of being flattened.
        identity = setter.index("GLW_IsIdentityRamp(")
        self.assertLess(identity, ramp)
        self.assertIn("GLW_RestoreGamma();", setter[identity:ramp])

        # Windows reports success for ramps it then reloads from the display
        # profile, so every write is confirmed against a readback.
        writer_start = gamma.index("static qboolean GLW_WriteGammaRamp(")
        writer = gamma[writer_start : gamma.index("static qboolean GLW_CaptureBaseline(")]
        self.assertIn("SetDeviceGammaRamp(", writer)
        self.assertIn("GetDeviceGammaRamp(", writer)

        # Sampling the desktop ramp while we still own the LUT would adopt the
        # game's own ramp as the one we owe the user back.
        init = gamma[gamma.index("void GLimp_InitGamma(") :]
        self.assertLess(
            init.index("GLW_RestoreGamma();"), init.index("GLW_CaptureBaseline(")
        )

        close = wndproc[wndproc.index("case WM_CLOSE:") :]
        self.assertLess(close.index("GLW_RestoreGamma();"), close.index('"quit\\n"'))

    def test_fullscreen_activation_reasserts_the_gamma_ramp_after_the_mode_set(self) -> None:
        wndproc = read_source("code/win32/win_wndproc.cpp")

        # ChangeDisplaySettings can still be settling when SetColorMappings
        # writes the ramp, so activation also schedules a delayed re-apply.
        activate = wndproc[
            wndproc.index("case WM_ACTIVATE:") : wndproc.index("case WM_SETFOCUS:")
        ]
        self.assertLess(
            activate.index("SetGameDisplaySettings();"),
            activate.index("WIN_ScheduleGammaReapply();"),
        )

        display = wndproc[
            wndproc.index("case WM_DISPLAYCHANGE:") : wndproc.index(
                "case WM_SETTINGCHANGE:"
            )
        ]
        self.assertIn("GLW_ReapplyGamma();", display)

        timer = wndproc[wndproc.index("if ( wParam == TIMER_G ) {") :]
        self.assertLess(timer.index("KillTimer"), timer.index("GLW_ReapplyGamma();"))

    def test_webui_searches_the_detected_retail_steam_install(self) -> None:
        webui = read_source("code/client/cl_webui.cpp")
        contract = read_source("code/client/webui_backend.hpp")
        backend = read_source("code/client/awesomium_backend_win32.cpp")

        self.assertIn('Cvar_VariableStringBuffer( "fs_steampath", retailPath', webui)
        self.assertIn("std::string_view retailPath{};", contract)
        self.assertIn("parameters.retailPath = retailPath ? retailPath", webui)
        self.assertIn("const std::wstring retail = ToWide( parameters.retailPath );", backend)
        self.assertIn("base, retail, home, ExecutableDirectory()", backend)


if __name__ == "__main__":
    unittest.main()
