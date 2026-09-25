/*
===========================================================================
Copyright (C) 2026 fnTech3 contributors

This file is part of fnTech3 and is distributed under the terms of the GNU
General Public License, version 2 or (at your option) any later version.
===========================================================================
*/

#include "game_manager.h"
#include "q3_ihv_install.hpp"
#include "q3test_install.hpp"

#include <cctype>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <cstdint>
#include <string>
#include <system_error>
#include <vector>

namespace {

/* Steam AppIDs and conventional directories were observed in the local retail
 * manifests; see docs/fntech3/2026-09-07-steam-discovery.md. Discovery identity
 * is independent of each profile's optional service integration contract. */

const comGameTitle_t titles[] = {
	{ COM_GAME_TITLE_QUAKE3, "quake3", "Quake III Arena" },
	{ COM_GAME_TITLE_QUAKELIVE, "quakelive", "Quake Live" },
	{ COM_GAME_TITLE_RTCW, "rtcw", "Return to Castle Wolfenstein" },
	{ COM_GAME_TITLE_WOLFET, "wolfet", "Wolfenstein: Enemy Territory" },
	{ COM_GAME_TITLE_STVEF, "stvef", "Star Trek: Voyager - Elite Force" },
	{ COM_GAME_TITLE_JKA, "jka", "Star Wars Jedi Knight: Jedi Academy" },
	{ COM_GAME_TITLE_JK2, "jk2", "Star Wars Jedi Knight II: Jedi Outcast" },
	/* One title, one mode: the key names the profile directly, and profile
	 * lookup answers it before the mode question is ever asked. */
	{ COM_GAME_TITLE_ALICE, "alice", "American McGee's Alice" },
	/* The same for Heavy Metal: F.A.K.K.2, the title Alice's engine came from. */
	{ COM_GAME_TITLE_FAKK2, "fakk2", "Heavy Metal: F.A.K.K.2" },
	/* Medal of Honor: Allied Assault's single player and multiplayer are one
	 * executable and one installation with no mode switch, so it is one
	 * profile whose key the title shares. */
	{ COM_GAME_TITLE_MOHAA, "mohaa", "Medal of Honor: Allied Assault" }
};

/* One catalog owns title identity, aliases and startup contracts. RTCW facts
 * are pinned in docs/fntech3/2026-09-14-rtcw-contracts.md. Catalog registration
 * does not enable an unfinished engine boundary or claim runtime compatibility.
 */
const comGameProfile_t profiles[] = { {
	COM_GAME_QUAKE3,
	"quake3",
	"Quake III Arena",
	"baseq3",
	"pak0.pk3",
	68,
	68,
	COM_PROTOCOL_FAMILY_LEGACY_QUAKE3 | COM_PROTOCOL_FAMILY_IOQUAKE3 | COM_PROTOCOL_FAMILY_Q3_POINT_RELEASES,
	COM_MODULE_API_QVM | COM_MODULE_API_LEGACY_NATIVE,
	COM_MODULE_API_QVM,
	0,
	0,
	"2200",
	"Quake 3 Arena",
	COM_GAME_TITLE_QUAKE3,
	COM_GAME_MARKER_QUAKE3,
	{ "q3", "quake3arena", nullptr, nullptr },
	"q3config.cfg",
	"q3config_server.cfg",
	nullptr,
	nullptr,
	nullptr,
	64, 1024, 544, 256, COM_ENTITY_HULL_TRACE_SHAPE, COM_ENTITY_BOX_CONTENTS_BODY, "servercache.dat",
	{ "0", "20", "8", "8", nullptr, nullptr, 1, "1", 1 },
	{ "1", "64", "1024", 1024, COM_BOT_AI_QUAKE, 1, 0, 0, 1, 64 },
	COM_PURE_PROOF_QVM,
	{ "sarge", "headmodel", "sarge", "sarge", "color1", "5", 1, 0 },
	{ "cinematic idlogo.RoQ", "cinematic intro.RoQ", 0 }
}, {
	COM_GAME_QUAKELIVE,
	"quakelive",
	"Quake Live",
	"baseq3",
	"pak00.pk3",
	91,
	91,
	COM_PROTOCOL_FAMILY_LEGACY_QUAKE3 | COM_PROTOCOL_FAMILY_IOQUAKE3
		| COM_PROTOCOL_FAMILY_QUAKELIVE,
	COM_MODULE_API_QVM | COM_MODULE_API_LEGACY_NATIVE
		| COM_MODULE_API_STRUCTURED_NATIVE,
	COM_MODULE_API_STRUCTURED_NATIVE,
	1,
	1,
	"282440",
	"Quake Live",
	COM_GAME_TITLE_QUAKELIVE,
	COM_GAME_MARKER_QUAKELIVE,
	{ "ql", nullptr, nullptr, nullptr },
	"qzconfig.cfg",
	"q3config_server.cfg",
	nullptr,
	nullptr,
	nullptr,
	64, 1024, 544, 256, COM_ENTITY_HULL_TRACE_SHAPE, COM_ENTITY_BOX_CONTENTS_BODY, "servercache.dat",
	{ "0", "50", "8", "8", nullptr, nullptr, 1, "1", 1 },
	{ "1", "64", "1024", 1024, COM_BOT_AI_QUAKE, 1, 0, 0, 1, 64 },
	COM_PURE_PROOF_QL_NATIVE,
	{ "sarge", "headmodel", "sarge", "sarge", "color1", "5", 1, 0 },
	{ "cinematic idlogo.RoQ", "cinematic intro.RoQ", 0 }
}, {
	COM_GAME_RTCW_SP,
	"rtcw-sp",
	"Return to Castle Wolfenstein (Single Player)",
	"main",
	"pak0.pk3",
	49,
	49,
	COM_PROTOCOL_FAMILY_RTCW_SP | COM_PROTOCOL_FAMILY_IORTCW_SP,
	COM_MODULE_API_LEGACY_NATIVE,
	COM_MODULE_API_LEGACY_NATIVE,
	0,
	0,
	"9010",
	"Return to Castle Wolfenstein",
	COM_GAME_TITLE_RTCW,
	COM_GAME_MARKER_RTCW_SP,
	{ "rtcwsp", "wolfsp", nullptr, nullptr },
	"wolfconfig.cfg",
	"wolfconfig_server.cfg",
	"sp_",
	"mp_",
	"RTCW SP module, wire, savegame and renderer adapters are not yet complete",
	128, 2048, 544, 512, COM_ENTITY_HULL_ENTITY_FLAG, COM_ENTITY_BOX_CONTENTS_BODY, nullptr,
	{ "0", "20", "8", "32", "1", "64", 1, "0", 0 },
	{ "1", "128", "2048", 2048, COM_BOT_AI_MOVEMENT, 2, 1, 16384, 0, 256 },
	COM_PURE_PROOF_UNAVAILABLE,
	/* WolfSP.exe registers name as "Player". */
	{ "bj2", "head", "default", nullptr, "color", nullptr, 0, 1,
		nullptr, nullptr, nullptr, "Player" },
	{ nullptr, "cinematic wolfintro.RoQ 3", 1 }
}, {
	COM_GAME_RTCW_MP,
	"rtcw-mp",
	"Return to Castle Wolfenstein (Multiplayer)",
	"main",
	"mp_pak0.pk3",
	60,
	60,
	COM_PROTOCOL_FAMILY_RTCW_MP | COM_PROTOCOL_FAMILY_IORTCW_MP,
	COM_MODULE_API_LEGACY_NATIVE,
	COM_MODULE_API_LEGACY_NATIVE,
	0,
	0,
	"9010",
	"Return to Castle Wolfenstein",
	COM_GAME_TITLE_RTCW,
	COM_GAME_MARKER_RTCW_MP,
	{ "rtcwmp", "wolfmp", nullptr, nullptr },
	"wolfconfig_mp.cfg",
	"wolfconfig_server.cfg",
	"mp_",
	"sp_",
	"RTCW MP module, wire and renderer adapters are not yet complete",
	64, 2048, 576, 512, COM_ENTITY_HULL_ENTITY_FLAG, COM_ENTITY_BOX_CONTENTS_ENTITY, "fntech3-rtcw-mp-servercache.dat",
	{ "5", "0", "20", "32", "3", nullptr, 0, "1", 0 },
	{ "0", "128", "1024", 1024, 0, 2, 1, 16384, 0, 256 },
	COM_PURE_PROOF_RTCW_NATIVE,
	/* WolfMP.exe registers name as "WolfPlayer"; a fresh retail home joins as it. */
	{ "multi", "head", "default", nullptr, "color", nullptr, 0, 1,
		nullptr, nullptr, nullptr, "WolfPlayer" },
	{ "cinematic gmlogo.RoQ", "cinematic wolfintro.RoQ", 0 }
}, {
	COM_GAME_WOLFET,
	"wolfet",
	"Wolfenstein: Enemy Territory",
	"etmain",
	"pak0.pk3",
	84,
	84,
	COM_PROTOCOL_FAMILY_WOLFET,
	COM_MODULE_API_LEGACY_NATIVE,
	COM_MODULE_API_LEGACY_NATIVE,
	0,
	0,
	"1873030",
	"Wolfenstein Enemy Territory",
	COM_GAME_TITLE_WOLFET,
	COM_GAME_MARKER_WOLFET,
	{ "et", "enemy-territory", "enemyterritory", nullptr },
	"etconfig.cfg",
	"etconfig_server.cfg",
	nullptr,
	nullptr,
	"Wolfenstein: Enemy Territory module, wire and renderer adapters are not yet complete",
	64, 1024, 689, 512, COM_ENTITY_HULL_ENTITY_FLAG, COM_ENTITY_BOX_CONTENTS_ENTITY, "fntech3-wolfet-servercache.dat",
	{ "4", "0", "20", "20", nullptr, nullptr, 0, "1", 0 },
	/* ET sets up Quake III's four configured subsystems after AAS and actions:
	 * etded.exe's Export_BotLibSetup (0x436ed0) loads weapons.c, items.c,
	 * syn.c, rnd.c, match.c and rchat.c from botfiles and sets the movement
	 * variables, and et.exe's (0x46c570) does the same. Its game
	 * then loads each bot's item weights, weapon weights and chat file. */
	{ "0", "64", "1024", 1024, COM_BOT_AI_QUAKE, 1, 1, 16384, 0, 256 },
	COM_PURE_PROOF_WOLFET_NATIVE,
	/* et.exe registers name as "ETPlayer". */
	{ "multi", "head", "default", nullptr, "color", nullptr, 0, 1,
		nullptr, nullptr, nullptr, "ETPlayer" },
	{ "cinematic etintro.roq", nullptr, 0 }
}, {
	/* Retail stvoy.exe 1.20 (2001-04-16): loopback protocol 40, efconfig.cfg,
	 * efgamex86.dll/efuix86.dll beside the executable and Bink movies. See
	 * docs/fntech3/2026-09-18-stvef-contracts.md. */
	COM_GAME_STVEF_SP,
	"stvef-sp",
	"Star Trek: Voyager - Elite Force (Single Player)",
	"baseEF",
	"pak0.pk3",
	40,
	40,
	COM_PROTOCOL_FAMILY_STVEF_SP,
	COM_MODULE_API_STVEF_SP_NATIVE,
	COM_MODULE_API_STVEF_SP_NATIVE,
	0,
	0,
	nullptr,
	nullptr,
	COM_GAME_TITLE_STVEF,
	COM_GAME_MARKER_STVEF_SP,
	{ "efsp", "stvoy", "eliteforce-sp", nullptr },
	"efconfig.cfg",
	"efconfig_server.cfg",
	nullptr,
	nullptr,
	"Elite Force SP module, savegame, cinematic and renderer adapters are not yet complete",
	1, 1024, 522, 256, COM_ENTITY_HULL_TRACE_SHAPE, COM_ENTITY_BOX_CONTENTS_BODY, nullptr,
	{ "0", "0", "1", "1", nullptr, nullptr, 0, "0", 0 },
	{ "0", "1", "1024", 1024, 0, 1, 0, 0, 0, 64 },
	COM_PURE_PROOF_UNAVAILABLE,
	{ "hazard/default", "headModel", "munro/default", nullptr, nullptr, nullptr, 0, 1,
		"legsModel", "torsoModel", "hazard/default", "Munro" },
	{ "cinematic eflogo", nullptr, 0 }
}, {
	/* Retail stvoyHM.exe 1.20 (2001-04-18): Huffman-free protocol 24 and the
	 * Holomatch QVMs in baseEF/pak0-3.pk3; ioEF/Lilium Voyager speak 26. */
	COM_GAME_STVEF_MP,
	"stvef-mp",
	"Star Trek: Voyager - Elite Force (Holomatch)",
	"baseEF",
	"pak0.pk3",
	24,
	24,
	COM_PROTOCOL_FAMILY_STVEF_HM | COM_PROTOCOL_FAMILY_IOEF,
	COM_MODULE_API_QVM | COM_MODULE_API_LEGACY_NATIVE,
	COM_MODULE_API_QVM,
	0,
	0,
	nullptr,
	nullptr,
	COM_GAME_TITLE_STVEF,
	COM_GAME_MARKER_STVEF_MP,
	{ "stvef-hm", "holomatch", "efhm", "stvoyhm" },
	"hmconfig.cfg",
	"hmconfig_server.cfg",
	nullptr,
	nullptr,
	"Elite Force Holomatch module, wire and renderer adapters are not yet complete",
	64, 1024, 544, 256, COM_ENTITY_HULL_TRACE_SHAPE, COM_ENTITY_BOX_CONTENTS_BODY, "fntech3-stvef-servercache.dat",
	{ "0", "20", "8", "8", nullptr, nullptr, 1, "1", 1 },
	{ "1", "64", "1024", 1024, COM_BOT_AI_QUAKE, 1, 0, 0, 1, 64 },
	COM_PURE_PROOF_QVM,
	{ "munro/red", nullptr, nullptr, nullptr, "color", nullptr, 0, 1,
		nullptr, nullptr, nullptr, "RedShirt" },
	{ nullptr, nullptr, 0 }
}, {
	/* February 1999 IHV: independent native API 4, protocol 33 and BSP43.
	 * This shares Q3's title identity, not its installation or active ABI. */
	COM_GAME_QUAKE3_IHV,
	"q3-ihv",
	"Quake III Arena (IHV Test)",
	"baseq3",
	nullptr,
	33,
	33,
	COM_PROTOCOL_FAMILY_QUAKE3_IHV,
	COM_MODULE_API_QUAKE3_IHV_NATIVE,
	COM_MODULE_API_QUAKE3_IHV_NATIVE,
	0, 0, nullptr, nullptr,
	COM_GAME_TITLE_QUAKE3,
	COM_GAME_MARKER_QUAKE3_IHV,
	{ "quake3-ihv", "q3ihv", "ihv", nullptr },
	"q3config.cfg",
	"q3config_server.cfg",
	nullptr, nullptr,
	"IHV native game/client, transport and playback integration is not yet complete",
	// Original SV startup clamps sv_maxclients to 1..256 (default 8).
	// Native game maxclients is separate and is capped by maxentities / 8.
	256, 1024, 544, 256, COM_ENTITY_HULL_TRACE_SHAPE, COM_ENTITY_BOX_CONTENTS_BODY, nullptr,
	{ "0", "0", "8", "8", nullptr, nullptr, 1, "0", 0 },
	// Navigation and bot AI belong to the native game, not the retail AAS library.
	{ "0", "8", "1024", 1024, 0, 0, 0, 0, 0, 0 },
	COM_PURE_PROOF_UNAVAILABLE,
	{ "xian", nullptr, nullptr, nullptr, nullptr, nullptr, 0, 1,
		nullptr, nullptr, nullptr, "Player" },
	{ nullptr, nullptr, 0 }
}, {
	/* Retail jasp.exe: loopback protocol 40, jaconfig.cfg, and one
	 * base/jagamex86.dll holding both the game and the client game. Jedi
	 * Academy's single player has no separate menu module and no bot library.
	 * See docs/fntech3/2026-09-21-jka-contracts.md. */
	COM_GAME_JKA_SP,
	"jka-sp",
	"Star Wars Jedi Knight: Jedi Academy (Single Player)",
	"base",
	"assets2.pk3",
	40,
	40,
	COM_PROTOCOL_FAMILY_JKA_SP,
	COM_MODULE_API_JKA_SP_NATIVE,
	COM_MODULE_API_JKA_SP_NATIVE,
	0,
	0,
	nullptr,
	nullptr,
	COM_GAME_TITLE_JKA,
	COM_GAME_MARKER_JKA_SP,
	{ "jasp", "jediacademy-sp", "jedi-academy-sp", nullptr },
	"jaconfig.cfg",
	"jaconfig_server.cfg",
	nullptr,
	nullptr,
	"Jedi Academy single-player module, wire, savegame, asset and renderer adapters are not yet complete",
	1, 1300, 743, 512, COM_ENTITY_HULL_TRACE_SHAPE, COM_ENTITY_BOX_CONTENTS_BODY, nullptr,
	{ "0", "0", "1", "1", nullptr, nullptr, 0, "0", 0 },
	{ "0", "1", "1024", 1024, 0, 0, 0, 0, 0, 64 },
	COM_PURE_PROOF_UNAVAILABLE,
	/* Retail single player publishes no model key: its player is chosen from
	 * the menus and carried in the savegame. Only the name is an engine
	 * default here; sex, sound set and handicap belong to the SP client. */
	{ nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, 0, 1,
		nullptr, nullptr, nullptr, "Jaden" },
	{ nullptr, nullptr, 0 },
	"assets"
}, {
	/* Retail jamp.exe: protocol 26 with native modules only -- Raven shipped
	 * no QVM for Jedi Academy -- and jampconfig.cfg. Its game types are not
	 * Quake III's numbers, and its 1,700 configstrings put the players at
	 * 1,131. See docs/fntech3/2026-09-21-jka-contracts.md. */
	COM_GAME_JKA_MP,
	"jka-mp",
	"Star Wars Jedi Knight: Jedi Academy (Multiplayer)",
	"base",
	"assets2.pk3",
	26,
	26,
	COM_PROTOCOL_FAMILY_JKA_MP,
	COM_MODULE_API_LEGACY_NATIVE,
	COM_MODULE_API_LEGACY_NATIVE,
	0,
	0,
	nullptr,
	nullptr,
	COM_GAME_TITLE_JKA,
	COM_GAME_MARKER_JKA_MP,
	{ "jamp", "jediacademy-mp", "jedi-academy-mp", nullptr },
	"jampconfig.cfg",
	"jampconfig_server.cfg",
	nullptr,
	nullptr,
	"Jedi Academy multiplayer module, wire, asset and renderer adapters are not yet complete",
	32, 1700, 1131, 512, COM_ENTITY_HULL_TRACE_SHAPE, COM_ENTITY_BOX_CONTENTS_BODY, "fntech3-jka-mp-servercache.dat",
	{ "0", "20", "8", "8", nullptr, nullptr, 1, "0", 0 },
	/* Academy carries bots but not Quake III's configured AI. Measured from
	 * the retail module: it has a call thunk for 150 of the 796 imports and
	 * not one is in the AAS block at 300-318, so it never routes on area
	 * awareness; and the installation ships botfiles/*.jkb and bots.txt but
	 * none of Quake III's items.c, weapons.c, syn.c, rnd.c or match.c, so
	 * none of the four configured subsystems has a file to read. It does
	 * allocate goal, move and weapon states and drives them through the
	 * elementary-action block, which is what is left once the configuration
	 * is taken away. */
	{ "1", "32", "1024", 1024, 0, 1, 0, 16384, 0, 128 },
	COM_PURE_PROOF_JKA_NATIVE,
	{ "kyle/default", nullptr, nullptr, nullptr, "color1", "4", 0, 1,
		nullptr, nullptr, nullptr, "Padawan" },
	{ nullptr, nullptr, 0 },
	"assets"
}, {
	/* Retail jk2sp.exe 1.02a: loopback protocol 40 and jk2config.cfg, with one
	 * jk2gamex86.dll in the installation root holding the game and the client
	 * game, as Jedi Academy's single player does. Its 1,024 configstrings and
	 * 512 submodels are the engine's own bounds checks.
	 * See docs/fntech3/2026-09-22-jk2-contracts.md. */
	COM_GAME_JK2_SP,
	"jk2-sp",
	"Star Wars Jedi Knight II: Jedi Outcast (Single Player)",
	"base",
	"assets0.pk3",
	40,
	40,
	COM_PROTOCOL_FAMILY_JK2_SP,
	COM_MODULE_API_JK2_SP_NATIVE,
	COM_MODULE_API_JK2_SP_NATIVE,
	0,
	0,
	nullptr,
	nullptr,
	COM_GAME_TITLE_JK2,
	COM_GAME_MARKER_JK2_SP,
	{ "jk2sp", "jedioutcast-sp", "jedi-outcast-sp", nullptr },
	"jk2config.cfg",
	"jk2config_server.cfg",
	nullptr,
	nullptr,
	"Jedi Outcast single-player module, wire, savegame, asset and renderer adapters are not yet complete",
	1, 1024, 0, 512, COM_ENTITY_HULL_TRACE_SHAPE, COM_ENTITY_BOX_CONTENTS_BODY, nullptr,
	{ "0", "0", "1", "1", nullptr, nullptr, 0, "0", 0 },
	{ "0", "1", "1024", 1024, 0, 0, 0, 0, 0, 64 },
	COM_PURE_PROOF_UNAVAILABLE,
	/* Single player publishes no model key, as Jedi Academy's does not. Its
	 * player name default is not pinned, so none is claimed. */
	{ nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, 0, 1,
		nullptr, nullptr, nullptr, nullptr },
	{ nullptr, nullptr, 0 },
	"assets"
}, {
	/* Retail jk2mp.exe 1.02a: protocol 15 and jk2mpconfig.cfg. Unlike Jedi
	 * Academy, its three modules are QVMs, shipped as vm/jk2mpgame.qvm,
	 * vm/cgame.qvm and vm/ui.qvm inside assets0.pk3, so its module boundary is
	 * the interpreter fnTech3 already has. Its 1,400 configstrings, 256
	 * submodels and 32-client clamp are the engine's own.
	 * See docs/fntech3/2026-09-22-jk2-contracts.md. */
	COM_GAME_JK2_MP,
	"jk2-mp",
	"Star Wars Jedi Knight II: Jedi Outcast (Multiplayer)",
	"base",
	"assets0.pk3",
	15,
	15,
	COM_PROTOCOL_FAMILY_JK2_MP,
	COM_MODULE_API_QVM,
	COM_MODULE_API_QVM,
	0,
	0,
	nullptr,
	nullptr,
	COM_GAME_TITLE_JK2,
	COM_GAME_MARKER_JK2_MP,
	{ "jk2mp", "jedioutcast-mp", "jedi-outcast-mp", nullptr },
	"jk2mpconfig.cfg",
	"jk2mpconfig_server.cfg",
	nullptr,
	nullptr,
	"Jedi Outcast multiplayer module, wire, asset and renderer adapters are not yet complete",
	32, 1400, 0, 256, COM_ENTITY_HULL_TRACE_SHAPE, COM_ENTITY_BOX_CONTENTS_BODY, "fntech3-jk2-mp-servercache.dat",
	{ "0", "20", "8", "8", nullptr, nullptr, 1, "0", 0 },
	/* jk2mp.exe carries Quake III's bot library whole: bot_enable defaults to
	 * 1 and bot_minplayers to 0 in its own registrations, and it reads .aas
	 * navigation beside the botfiles the installation ships. */
	/* Outcast carries bots and, like Academy, not Quake III's configured AI.
	 * Its installation ships botfiles/*.jkb and bots.txt -- Raven's own bot
	 * characters -- and none of Quake III's items.c, weapons.c, syn.c, rnd.c
	 * or match.c, so none of the four subsystems has a file to read, and no
	 * map in it has an .aas file, so area awareness has nothing to load. Its
	 * module asks for the state machinery and the elementary-action block,
	 * which is what is left once the configuration is taken away. */
	{ "1", "32", "1024", 1024, 0, 1, 0, 16384, 0, 128 },
	COM_PURE_PROOF_QVM,
	/* Multiplayer's userinfo defaults are not pinned, so none is claimed. */
	{ nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, 0, 1,
		nullptr, nullptr, nullptr, nullptr },
	{ nullptr, nullptr, 0 },
	"assets"
}, {
	/* Retail alice.exe: protocol 1, config.cfg, and two libraries in base/
	 * rather than one -- fgamex86.dll for the game and cgamex86.dll for the
	 * client game -- each exporting its own function-table entry point. The
	 * menus are the engine's own .urc resources, so no UI module is loaded.
	 * Alice is a Ritual F.A.K.K.2-lineage engine, not a Quake III one: its
	 * maps are FAKK 42, its models TIKI, and its demos a fixed .dm3.
	 * See docs/fntech3/2026-09-22-alice-contracts.md. */
	COM_GAME_ALICE,
	"alice",
	"American McGee's Alice",
	"base",
	"pak0.pk3",
	1,
	1,
	COM_PROTOCOL_FAMILY_ALICE,
	COM_MODULE_API_ALICE_NATIVE,
	COM_MODULE_API_ALICE_NATIVE,
	0,
	0,
	nullptr,
	nullptr,
	COM_GAME_TITLE_ALICE,
	COM_GAME_MARKER_ALICE,
	{ "americanmcgeesalice", "mcgee-alice", "alice2000", nullptr },
	"config.cfg",
	"config_server.cfg",
	nullptr,
	nullptr,
	"Alice module, wire, savegame, asset and renderer adapters are not yet complete",
	/* sv_maxclients clamps to 128 in retail; MAX_CONFIGSTRINGS is 1,328 and
	 * MAX_SUBMODELS 512, both read from the engine's own bounds checks. The
	 * configstring index the players start at is not pinned yet. */
	128, 1328, 0, 512, COM_ENTITY_HULL_TRACE_SHAPE, COM_ENTITY_BOX_CONTENTS_BODY, nullptr,
	/* Retail's SV_Init registers sv_maxclients at "1", SERVERINFO and
	 * LATCH (0x0045566b..0x0045567c), and that first registration is the
	 * default: the "8" SV_BoundMaxClients passes (0x00454e0c) applies only to
	 * a cvar not yet registered. loadgame refuses above one. Retail has no
	 * spmap: its only map commands are map, gamemap and devmap, so the
	 * single-player command leaves the slot count where the profile puts it. */
	{ "0", "20", "1", "1", nullptr, nullptr, 1, "0", 0 },
	/* Ritual's engine carries no Quake bot library: it has no bot_enable,
	 * no .aas navigation and no botfiles in any retail pak. */
	{ "0", "1", "1024", 1024, 0, 0, 0, 0, 0, 128 },
	COM_PURE_PROOF_UNAVAILABLE,
	/* Retail registers name as UnnamedPlayer, which is the engine default a
	 * NULL here already means; its own default.cfg then setas it to "Player".
	 * Alice's player is the game's own character, so no model, head or
	 * colour key is published. */
	{ nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, 0, 1,
		nullptr, nullptr, nullptr, nullptr },
	{ nullptr, nullptr, 0 },
	nullptr
}, {
	/* Retail Heavy Metal: F.A.K.K.2 1.0 (fakk2.exe 1.0.0.1, linked
	 * 2000-07-31): the engine Alice's descends from, with fakk/ as its game
	 * directory and gamex86.dll and cgamex86.dll beside the paks. The disc's
	 * fakk2.exe is LaserLock-wrapped -- its code and data sections are
	 * encrypted on disk -- but the 1.02 patch's is plain, and it sends
	 * protocol 1 (0x0042161d). The module tables, the entity record and the
	 * configstring bounds are measured in the two retail libraries, which are
	 * plain; config.cfg (0x0042e16a), the 48 KB messages (0x00443bad) and the
	 * submodel and reliable-command bounds below are read from the 1.02
	 * engine. See docs/fntech3/2026-09-24-fakk2-contracts.md. */
	COM_GAME_FAKK2,
	"fakk2",
	"Heavy Metal: F.A.K.K.2",
	"fakk",
	"pak0.pk3",
	1,
	1,
	COM_PROTOCOL_FAMILY_FAKK2,
	COM_MODULE_API_FAKK2_NATIVE,
	COM_MODULE_API_FAKK2_NATIVE,
	0,
	0,
	nullptr,
	nullptr,
	COM_GAME_TITLE_FAKK2,
	COM_GAME_MARKER_FAKK2,
	{ "heavymetal-fakk2", "heavymetalfakk2", "hmfakk2", nullptr },
	"config.cfg",
	"config_server.cfg",
	nullptr,
	nullptr,
	"F.A.K.K.2 module, wire, savegame, asset and renderer adapters are not yet complete",
	/* The client game bounds a configstring at 1,200 (0x300222be) and
	 * registers models over [32, 416), sounds over [416, 672) and light
	 * styles over [736, 768) (0x30022057..0x3002211e), which is the SDK
	 * header's layout; the players start where the light styles end. The
	 * 128-client bound is that header's MAX_CLIENTS. fakk2.exe 1.02 refuses
	 * a map of more than 384 submodels (0x0042575f) and answers handle 383
	 * with its temporary box (0x00426702), where Alice's engine admits 512;
	 * no retail map has more than 201. */
	128, 1200, 768, 384, COM_ENTITY_HULL_TRACE_SHAPE, COM_ENTITY_BOX_CONTENTS_BODY, nullptr,
	{ "0", "20", "1", "1", nullptr, nullptr, 1, "0", 0 },
	/* No Quake bot library, as in Alice. fakk2.exe 1.02 rings 64 reliable
	 * commands, where Alice's engine rings 128: its server drops a client
	 * at the 65th unacknowledged one (0x004468dd). */
	{ "0", "1", "1024", 1024, 0, 0, 0, 0, 0, 64 },
	COM_PURE_PROOF_UNAVAILABLE,
	/* Julie is the game's own character: no model, head or colour key. */
	{ nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, 0, 1,
		nullptr, nullptr, nullptr, nullptr },
	{ nullptr, nullptr, 0 },
	nullptr
}, {
	/* Retail Medal of Honor: Allied Assault 1.00 (MOHAA.exe linked
	 * 2001-12-06): 2015's descendant of Ritual's engine, with main/ as its
	 * game directory, gamex86.dll and cgamex86.dll beside six paks whose
	 * names are capitalised, and the allocator both libraries import in
	 * system86.dll beside the executable. MOHAA.exe is SafeDisc-wrapped -- its
	 * code and data are encrypted on disk -- so no number here is read from
	 * the engine itself. The module tables, record sizes and configstring
	 * blocks are measured in the two plain retail libraries; the protocol,
	 * the configuration's name and the defaults the modules do not bound are
	 * OpenMoHAA's readings of the engine, and are marked so in
	 * docs/fntech3/2026-09-24-mohaa-contracts.md. */
	COM_GAME_MOHAA,
	"mohaa",
	"Medal of Honor: Allied Assault",
	"main",
	"Pak0.pk3",
	6,
	6,
	COM_PROTOCOL_FAMILY_MOHAA,
	COM_MODULE_API_MOHAA_NATIVE,
	COM_MODULE_API_MOHAA_NATIVE,
	0,
	0,
	nullptr,
	nullptr,
	COM_GAME_TITLE_MOHAA,
	COM_GAME_MARKER_MOHAA,
	{ "medalofhonor", "moh-aa", "alliedassault", nullptr },
	/* The engine keeps its configuration under configs/, named for the
	 * profile the player chose, and unnamedsoldier.cfg is the default one. */
	"configs/unnamedsoldier.cfg",
	"configs/unnamedsoldier_server.cfg",
	nullptr,
	nullptr,
	"Medal of Honor module, wire, savegame, asset and renderer adapters are not yet complete",
	/* The client game bounds a configstring at 2,736 (0x300234dd) and
	 * dispatches light styles over [1652, 1684) (0x3002324e..0x30023267), so
	 * the players' block starts at 1,684; it is 64 long, the client bound.
	 * 1,024 submodels is the maps' own ceiling. */
	64, 2736, 1684, 1024, COM_ENTITY_HULL_TRACE_SHAPE, COM_ENTITY_BOX_CONTENTS_BODY, nullptr,
	/* g_gametype 0 is single player; the engine registers it, and a
	 * listen server starts with one slot. */
	{ "0", "20", "1", "1", nullptr, nullptr, 1, "0", 0 },
	/* 1.00 has no bot library and its game exports no bot entry points (the
	 * two slots are null), so no bot ever takes a client slot. */
	{ "0", "1", "1024", 1024, 0, 0, 0, 0, 0, 0 },
	/* OpenMoHAA's CL_SendPureChecksums and SV_VerifyPaks_f; sv_pure is off
	 * by default, as the server policy above has it. */
	COM_PURE_PROOF_MOHAA_REFERENCES,
	/* Multiplayer publishes an Allied and a German model; the second rides
	 * in the torso slot, which is only a second key and default here. */
	{ "american_army", nullptr, nullptr, nullptr, nullptr, nullptr, 0, 1,
		"dm_playermodel", "dm_playergermanmodel", "german_wehrmacht_soldier", "UnnamedSoldier" },
	/* The intro is a staged sequence -- EA's logo movie, the title and legal
	 * pictures, 2015's logo movie and the intro movie -- that the menu system
	 * runs (cl_mohaa_ui.cpp); its command starts it each run, as the game's
	 * autoexec.cfg sets cl_playintro every time. */
	{ nullptr, "ui_startintro", 1 },
	nullptr
}, {
	/* The public Q3Test builds of 1999: GetGameAPI modules beside the
	 * executable, demoq3/ paks and each release's own protocol (35 to 40) and
	 * map format (IBSP 44 or 45). One release is pinned at startup from the
	 * installation (q3test_install.hpp); its protocol then replaces the one
	 * named here. docs/fntech3/Q3TEST.md records every value below. */
	COM_GAME_Q3TEST,
	"q3test",
	"Quake III Arena (Q3Test)",
	"demoq3",
	nullptr,
	40,
	40,
	COM_PROTOCOL_FAMILY_Q3TEST,
	COM_MODULE_API_Q3TEST_NATIVE,
	COM_MODULE_API_Q3TEST_NATIVE,
	0, 0, nullptr, nullptr,
	COM_GAME_TITLE_QUAKE3,
	COM_GAME_MARKER_Q3TEST,
	{ "q3t", "quake3-test", nullptr, nullptr },
	"q3config.cfg",
	"q3config_server.cfg",
	nullptr, nullptr,
	"Q3Test native module, transport, demo and renderer adapters are not yet complete",
	/* Every release clamps sv_maxclients to 128 (SV_Init, 1.02 0x41c126; 1.06
	 * SV_BoundMaxClients 0x41cb30), and its games size their clients to
	 * match. Players' configstrings start at 544 in 1.06-1.08 and at 672 in
	 * 1.05, whose own client path reads them. */
	128, 1024, 544, 256, COM_ENTITY_HULL_TRACE_SHAPE, COM_ENTITY_BOX_CONTENTS_BODY, nullptr,
	/* SV_Init in each executable: fraglimit 20, sv_maxclients 8 latched into
	 * serverinfo, and no sv_pure (1.08 0x0041d8f0). */
	{ "0", "20", "8", "8", nullptr, nullptr, 1, "0", 0 },
	/* 1.06 to 1.08 ring 64 reliable commands per client, bots included
	 * (SV_AddServerCommand 0x0041d3a0 in 1.06, 0x0041dd10 in 1.08). */
	{ "0", "8", "1024", 1024, 0, 0, 0, 0, 0, 64 },
	COM_PURE_PROOF_UNAVAILABLE,
	{ "visor", nullptr, nullptr, nullptr, "color", nullptr, 0, 1 },
	{ nullptr, nullptr, 0 },
	nullptr
} };

static_assert( sizeof( profiles ) / sizeof( profiles[0] ) == COM_GAME_COUNT - 1,
	"Each concrete game ID needs one catalog entry" );

const comGameProfile_t *activeProfile;
/* Pinned once, with the q3test profile, before the filesystem starts. */
const q3testRelease_t *activeQ3TestRelease;

bool EqualsIgnoreCase( const char *left, const char *right )
{
	unsigned char leftChar;
	unsigned char rightChar;

	if ( !left || !right ) {
		return false;
	}

	for ( ;; left++, right++ ) {
		leftChar = static_cast<unsigned char>( *left );
		rightChar = static_cast<unsigned char>( *right );
		if ( std::tolower( leftChar ) != std::tolower( rightChar ) ) {
			return false;
		}
		if ( leftChar == '\0' ) {
			return true;
		}
	}
}

bool RuntimeBlocked( const comGameProfile_t *profile )
{
#if defined(FNTECH3_Q3_IHV_DEVELOPMENT) && defined(DEDICATED) && defined(_WIN32) && (defined(_M_IX86) || defined(__i386__))
	if ( profile && profile->id == COM_GAME_QUAKE3_IHV ) return false;
#endif
#if defined( FNTECH3_JKA_DEVELOPMENT )
	/* Developer builds exercise Jedi Academy's single-player and multiplayer
	 * adapters with the retail modules. Neither mode's release gate moves. */
	if ( profile && profile->titleId == COM_GAME_TITLE_JKA ) return false;
#endif
#if defined( FNTECH3_JK2_DEVELOPMENT )
	/* The same for Jedi Outcast, whose two modes share this title's formats
	 * and differ from Jedi Academy chiefly in shipping QVMs. */
	if ( profile && profile->titleId == COM_GAME_TITLE_JK2 ) return false;
#endif
#if defined( FNTECH3_ALICE_DEVELOPMENT )
	/* The same for Alice, whose engine lineage differs from every other
	 * profile's. A developer server now loads a retail level and runs its
	 * frame loop, but 25 of the game's 111 imports are still unserved, no
	 * client can connect and nothing is rendered, so the release gate below
	 * does not move. */
	if ( profile && profile->titleId == COM_GAME_TITLE_ALICE ) return false;
#endif
#if defined( FNTECH3_FAKK2_DEVELOPMENT )
	/* The same for Heavy Metal: F.A.K.K.2, which runs on the Alice adapters
	 * where the two engines agree. Its release gate does not move either. */
	if ( profile && profile->titleId == COM_GAME_TITLE_FAKK2 ) return false;
#endif
#if defined( FNTECH3_MOHAA_DEVELOPMENT )
	/* The same for Medal of Honor: Allied Assault, whose modules speak 2015's
	 * own tables. Its release gate does not move either. */
	if ( profile && profile->titleId == COM_GAME_TITLE_MOHAA ) return false;
#endif
#if defined( FNTECH3_Q3TEST_DEVELOPMENT ) && defined( _WIN32 ) && ( defined( _M_IX86 ) || defined( __i386__ ) )
	/* Q3Test's retail modules are Win32 x86 libraries, so only that build
	 * runs them, and only for isolated probes; the release gate stays shut. */
	if ( profile && profile->id == COM_GAME_Q3TEST ) return false;
#endif
#if defined( FNTECH3_STVEF_DEVELOPMENT )
	/* Developer builds exercise Elite Force's SP and Holomatch adapters with
	 * the retail modules. Neither mode's release gate is lifted by this. */
	if ( profile && profile->titleId == COM_GAME_TITLE_STVEF ) return false;
#endif
#if defined(FNTECH3_WOLFET_DEVELOPMENT)
	/* Explicit developer builds exercise the ET client and server with real
	 * modules, as the RTCW developer build does. This does not alter the
	 * profile contract or lift the release gate below. */
	if (profile && profile->id == COM_GAME_WOLFET) return false;
#endif
#if defined( FNTECH3_RTCW_DEVELOPMENT )
	/* Explicit developer builds exercise incomplete adapters with real modules.
	 * This does not alter any profile contract or the release gate ledger. */
	if ( profile && profile->titleId == COM_GAME_TITLE_RTCW ) return false;
#endif
	return profile && profile->runtimeBlocker;
}

comGameDetectionResult_t Result( comGameDetectionStatus_t status,
	comGameDetectionEvidence_t evidence, const comGameProfile_t *profile )
{
	comGameDetectionResult_t result;
#if defined( FNTECH3_Q3_ONLY )
	/* Preserve evidence priority: a detected QL installation must fail here,
	 * before activation/filesystem startup, rather than falling through to a
	 * different installation or exposing a retail x86 module to an x64 host. */
	if ( profile && profile->id != COM_GAME_QUAKE3 ) {
		status = COM_GAME_DETECTION_UNSUPPORTED_BUILD;
		profile = nullptr;
	}
#endif
	if ( RuntimeBlocked( profile ) ) {
		status = COM_GAME_DETECTION_INCOMPLETE_ENGINE;
		profile = nullptr;
	}
	result.status = status;
	result.evidence = evidence;
	result.profile = profile;
	return result;
}

comGameDetectionResult_t DetectCandidate( unsigned markers,
	comGameDetectionEvidence_t evidence )
{
	if ( markers & COM_GAME_MARKER_UNRECOGNIZED_PAK ) {
		return Result( COM_GAME_DETECTION_UNRECOGNIZED_PAK, evidence, nullptr );
	}
	const comGameProfile_t *candidate = nullptr;
	bool multiple = false, mixedTitles = false;
	for ( const auto &profile : profiles ) {
		if ( !( markers & profile.installMarker ) ) continue;
		if ( candidate ) {
			multiple = true;
			mixedTitles |= candidate->titleId != profile.titleId
				|| candidate->titleId == COM_GAME_TITLE_QUAKE3;
		}
		candidate = &profile;
	}
	if ( multiple ) {
		return Result( mixedTitles ? COM_GAME_DETECTION_AMBIGUOUS
			: COM_GAME_DETECTION_MODE_REQUIRED, evidence, nullptr );
	}
	if ( candidate ) {
		return Result( COM_GAME_DETECTION_OK, evidence, candidate );
	}

	return Result( COM_GAME_DETECTION_NOT_FOUND, COM_GAME_EVIDENCE_NONE, nullptr );
}

bool IsRegularFile( const std::filesystem::path &path )
{
	std::error_code error;
	const bool regular = std::filesystem::is_regular_file( path, error );
	return regular && !error;
}

bool HasPakPrefix( const char *name, const char *prefix )
{
	if ( !name || !prefix ) return false;
	for ( ; *prefix; ++prefix, ++name ) {
		if ( std::tolower( static_cast<unsigned char>( *name ) )
			!= std::tolower( static_cast<unsigned char>( *prefix ) ) ) return false;
	}
	return true;
}

unsigned LE16( const unsigned char *p )
{
	return p[0] | (unsigned( p[1] ) << 8);
}

/* A pak is named by its default.cfg and one companion file, usually a map.
 * Jedi Academy keeps its maps in a different pak from its configuration, so
 * its companion is the multiplayer configuration beside it. F.A.K.K.2's
 * default.cfg ships loose beside its paks rather than in one, so its pak is
 * named by the menu configuration it does carry. */
struct PakIdentity {
	unsigned marker;
	std::uint32_t configSize, configCrc;
	const char *companion;
	std::uint32_t companionSize, companionCrc;
	/* NULL means default.cfg. */
	const char *config;
};

const PakIdentity pakIdentities[] = {
	{ COM_GAME_MARKER_QUAKE3, 1737, 0x9a18b2fdu,
		"maps/q3dm6.bsp", 4408212, 0x9fef9a4bu },
	{ COM_GAME_MARKER_QUAKELIVE, 1580, 0xc6c9f233u,
		"maps/campgrounds.bsp", 2924532, 0x0ddc0f1du },
	{ COM_GAME_MARKER_RTCW_SP, 2237, 0x844222c3u,
		"maps/escape1.bsp", 12366352, 0x5f08b87cu },
	{ COM_GAME_MARKER_RTCW_MP, 1372, 0x91dfaca0u,
		"maps/mp_beach.bsp", 7343512, 0x77533d56u },
	// Steam build 7997832: metadata only, no retail payload is incorporated.
	{ COM_GAME_MARKER_WOLFET, 1496, 0x061b5285u,
		"maps/goldrush.bsp", 14757996, 0x7ebac2f4u },
	// Retail baseEF/pak0.pk3 (2000-08-21) carries both Elite Force modes, so
	// SP and Holomatch share one identity and a probe reports both marks.
	{ COM_GAME_MARKER_STVEF_SP, 2330, 0xd7ab74a1u,
		"maps/borg1.bsp", 8142016, 0x4e7d14d6u },
	{ COM_GAME_MARKER_STVEF_MP, 2330, 0xd7ab74a1u,
		"maps/borg1.bsp", 8142016, 0x4e7d14d6u },
	// Retail Jedi Academy keeps both configurations in base/assets2.pk3 and
	// its maps in assets0.pk3, so the two configurations name the pak. One
	// installation carries both modes, as Elite Force's does.
	{ COM_GAME_MARKER_JKA_SP, 2178, 0xfb11a25bu,
		"mpdefault.cfg", 2845, 0x4999454au },
	{ COM_GAME_MARKER_JKA_MP, 2178, 0xfb11a25bu,
		"mpdefault.cfg", 2845, 0x4999454au },
	// Retail Jedi Outcast 1.02a keeps both configurations in base/assets0.pk3,
	// alongside its maps and its three multiplayer QVMs. One installation
	// carries both modes here as well.
	{ COM_GAME_MARKER_JK2_SP, 3048, 0xf62886cau,
		"mpdefault.cfg", 3442, 0x5c12b170u },
	{ COM_GAME_MARKER_JK2_MP, 3048, 0xf62886cau,
		"mpdefault.cfg", 3442, 0x5c12b170u },
	// Retail Alice keeps default.cfg at the root of base/pak0.pk3 and its
	// thirty-six maps beside it. garden1 is the first level the campaign
	// reaches and appears in no other supported title's paks.
	{ COM_GAME_MARKER_ALICE, 1322, 0x3781148bu,
		"maps/garden1.bsp", 6323368, 0xcb85fdaeu },
	// Retail F.A.K.K.2 1.0 keeps menu.cfg at the root of fakk/pak0.pk3 and all
	// thirty of its maps beside it; fakkhouse is the first level the campaign
	// reaches after the intro. The pak carries no default.cfg at all.
	{ COM_GAME_MARKER_FAKK2, 413, 0x609b5da3u,
		"maps/fakkhouse.bsp", 4700748, 0xb72ef3a6u, "menu.cfg" },
	// Retail Medal of Honor: Allied Assault 1.00 keeps default.cfg in
	// main/Pak0.pk3 and all fifty-four maps in Pak5.pk3, so its companion is
	// the sound alias script beside the configuration, which no other
	// supported title ships.
	{ COM_GAME_MARKER_MOHAA, 1856, 0xde36f158u,
		"ubersound/ubersound.scr", 173320, 0xf462d126u }
};

std::uint32_t LE32( const unsigned char *p )
{
	return LE16( p ) | (std::uint32_t( LE16( p + 2 ) ) << 16);
}

// This is an early identity hint, not authentication or a ZIP payload validator.
// Layout: PKWARE APPNOTE 6.3.10, sections 4.3.12 and 4.3.16.
// Retail metadata observations: docs/fntech3/2026-09-08-pak-identity.md.
bool RecognizedPak( const std::filesystem::path &path, unsigned expected )
{
	const PakIdentity *identity = nullptr;
	for ( const auto &candidate : pakIdentities ) {
		if ( candidate.marker == expected ) identity = &candidate;
	}
	if ( !identity ) return false;
	std::ifstream file( path, std::ios::binary | std::ios::ate );
	const auto end = file.tellg();
	if ( !file || end < 22 || end > std::streamoff( UINT32_MAX ) ) {
		return false;
	}
	const auto size = static_cast<std::uint32_t>( end );
	const auto tailSize = size < 65557u ? size : 65557u;
	std::vector<unsigned char> tail( tailSize );
	file.seekg( size - tailSize );
	if ( !file.read( reinterpret_cast<char *>( tail.data() ), tailSize ) ) {
		return false;
	}
	std::size_t eocd = tailSize;
	for ( std::size_t pos = tailSize - 22; ; --pos ) {
		if ( LE32( &tail[pos] ) == 0x06054b50u
			&& pos + 22u + LE16( &tail[pos + 20] ) == tailSize ) {
			eocd = pos;
			break;
		}
		if ( pos == 0 ) break;
	}
	if ( eocd == tailSize ) return false;
	const auto *e = &tail[eocd];
	const unsigned count = LE16( e + 10 );
	const std::uint32_t bytes = LE32( e + 12 ), offset = LE32( e + 16 );
	// Bound startup work. Split archives, ZIP64 and unknown layouts require an
	// explicit profile; the full filesystem remains responsible for loading.
	if ( LE16( e + 4 ) || LE16( e + 6 ) || LE16( e + 8 ) != count
		|| count == 0 || count == 65535 || bytes > 8u * 1024u * 1024u
		|| std::uint64_t( offset ) + bytes != size - tailSize + eocd ) {
		return false;
	}
	std::vector<unsigned char> directory( bytes );
	file.seekg( offset );
	if ( !file.read( reinterpret_cast<char *>( directory.data() ), bytes ) ) return false;
	unsigned defaultSeen = 0, companionSeen = 0;
	bool defaultMatches = false, companionMatches = false, foreignMapSeen = false;
	std::size_t pos = 0;
	for ( unsigned i = 0; i < count; ++i ) {
		if ( bytes - pos < 46 ) return false;
		const auto *h = directory.data() + pos;
		const unsigned nameLength = LE16( h + 28 );
		const std::size_t recordSize = 46u + nameLength + LE16( h + 30 ) + LE16( h + 32 );
		if ( LE32( h ) != 0x02014b50u || recordSize > bytes - pos
			|| !nameLength || LE16( h + 34 ) || ( LE16( h + 8 ) & 1u )
			|| ( LE16( h + 10 ) != 0 && LE16( h + 10 ) != 8 )
			|| std::uint64_t( LE32( h + 42 ) ) + 30u + LE32( h + 20 ) > offset ) {
			return false;
		}
		const std::string name( reinterpret_cast<const char *>( h + 46 ), nameLength );
		const auto length = LE32( h + 24 ), crc = LE32( h + 16 );
		if ( name == ( identity->config ? identity->config : "default.cfg" ) ) {
			++defaultSeen;
			defaultMatches = length == identity->configSize && crc == identity->configCrc;
		} else if ( name == identity->companion ) {
			++companionSeen;
			companionMatches = length == identity->companionSize
				&& crc == identity->companionCrc;
		} else {
			for ( const auto &other : pakIdentities ) {
				if ( name == other.companion ) foreignMapSeen = true;
			}
		}
		pos += recordSize;
	}
	return pos == bytes && defaultSeen == 1 && defaultMatches
		&& companionSeen == 1 && companionMatches && !foreignMapSeen;
}

} // namespace

extern "C" {

unsigned Com_GameManagerTitleCount( void )
{
	return sizeof( titles ) / sizeof( titles[0] );
}

const comGameTitle_t *Com_GameManagerTitleAt( unsigned index )
{
	return index < Com_GameManagerTitleCount() ? &titles[index] : nullptr;
}

const comGameTitle_t *Com_GameManagerTitleById( comGameTitleId_t id )
{
	for ( const auto &title : titles ) {
		if ( title.id == id ) return &title;
	}
	return nullptr;
}

unsigned Com_GameManagerProfileCount( void )
{
	return sizeof( profiles ) / sizeof( profiles[0] );
}

const comGameProfile_t *Com_GameManagerProfileAt( unsigned index )
{
	return index < Com_GameManagerProfileCount() ? &profiles[index] : nullptr;
}

const comGameProfile_t *Com_GameManagerFindProfile( const char *key )
{
	if ( !key || !key[0] ) return nullptr;
	for ( const auto &profile : profiles ) {
		if ( EqualsIgnoreCase( key, profile.key ) ) return &profile;
		for ( const char *alias : profile.aliases ) {
			if ( EqualsIgnoreCase( key, alias ) ) return &profile;
		}
	}
	return nullptr;
}

unsigned Com_GameManagerTitleProfileCount( comGameTitleId_t title )
{
	unsigned count = 0;
	for ( const auto &profile : profiles ) {
		if ( profile.titleId == title ) ++count;
	}
	return count;
}

const comGameProfile_t *Com_GameManagerTitleProfileAt(
	comGameTitleId_t title, unsigned index )
{
	for ( const auto &profile : profiles ) {
		if ( profile.titleId == title && index-- == 0 ) return &profile;
	}
	return nullptr;
}

int Com_GameManagerCanActivate( const comGameProfile_t *profile )
{
	if ( !profile || Com_GameProfileById( profile->id ) != profile
		|| RuntimeBlocked( profile ) ) return 0;
#if defined( FNTECH3_Q3_ONLY )
	return profile->id == COM_GAME_QUAKE3;
#else
	return 1;
#endif
}

comGameBspFormat_t Com_GameManagerBspFormat( const comGameProfile_t *profile,
	int version )
{
	if ( !profile || Com_GameProfileById( profile->id ) != profile )
		return COM_GAME_BSP_UNSUPPORTED;
	if ( profile->id == COM_GAME_QUAKE3_IHV )
		return version == 43 ? COM_GAME_BSP_QUAKE3_IHV : COM_GAME_BSP_UNSUPPORTED;
	/* A Q3Test release reads its own maps' version only: 44 up to 1.05 and
	 * 45 from 1.06. Before a release is pinned both are the profile's. */
	if ( profile->id == COM_GAME_Q3TEST ) {
		const q3testRelease_t *release = profile == activeProfile ? Com_Q3TestRelease() : nullptr;
		const bool known = release ? version == release->bspVersion : version == 44 || version == 45;
		return known ? COM_GAME_BSP_Q3TEST : COM_GAME_BSP_UNSUPPORTED;
	}
	if ( profile->titleId == COM_GAME_TITLE_RTCW )
		return version == 47 ? COM_GAME_BSP_RTCW : COM_GAME_BSP_UNSUPPORTED;
	if ( profile->titleId == COM_GAME_TITLE_WOLFET )
		return version == 47 ? COM_GAME_BSP_WOLFET : COM_GAME_BSP_UNSUPPORTED;
	/* Every retail Elite Force map is IBSP 46 with Quake III's lump layout;
	 * Raven's changes are surface/content flag meanings owned by the modules. */
	if ( profile->titleId == COM_GAME_TITLE_STVEF )
		return version == 46 ? COM_GAME_BSP_QUAKE3 : COM_GAME_BSP_UNSUPPORTED;
	/* Both Raven titles read only their own "RBSP" 1 maps, whose ident the
	 * caller checks separately: an IBSP 46 map is not one of theirs. */
	if ( profile->titleId == COM_GAME_TITLE_JKA || profile->titleId == COM_GAME_TITLE_JK2 )
		return version == 1 ? COM_GAME_BSP_RBSP : COM_GAME_BSP_UNSUPPORTED;
	/* Every retail Alice map is "FAKK" 42; the ident is checked separately,
	 * because F.A.K.K.2's own 12 and Medal of Honor's share that ident. */
	if ( profile->titleId == COM_GAME_TITLE_ALICE )
		return version == 42 ? COM_GAME_BSP_FAKK : COM_GAME_BSP_UNSUPPORTED;
	/* And every retail F.A.K.K.2 map is "FAKK" 12, the version Ritual's SDK
	 * header names; its records are the ones Alice's 42 kept. */
	if ( profile->titleId == COM_GAME_TITLE_FAKK2 )
		return version == 12 ? COM_GAME_BSP_FAKK : COM_GAME_BSP_UNSUPPORTED;
	/* Every retail Medal of Honor 1.00 map is "2015" 19, and its ident is
	 * checked separately. The beta's 17 and 18 and the expansions' 20 and 21
	 * move lumps and grow records, and none ships with this installation. */
	if ( profile->titleId == COM_GAME_TITLE_MOHAA )
		return version == 19 ? COM_GAME_BSP_MOHAA : COM_GAME_BSP_UNSUPPORTED;
	if ( profile->titleId != COM_GAME_TITLE_QUAKE3 && profile->titleId != COM_GAME_TITLE_QUAKELIVE )
		return COM_GAME_BSP_UNSUPPORTED;
	/* Preserve the existing Q3/QL asset set, including IHV translation. RTCW's
	 * v47 header is pinned to the source/retail observations in the RTCW ledger. */
	switch ( version ) {
	case 43: return COM_GAME_BSP_QUAKE3_IHV;
	case 46: return COM_GAME_BSP_QUAKE3;
	case 47: return COM_GAME_BSP_QUAKELIVE;
	default: return COM_GAME_BSP_UNSUPPORTED;
	}
}

comGameBspFormat_t Com_GameBspFormat( int version )
{
	return Com_GameManagerBspFormat( activeProfile, version );
}

comGameModeLaunchStatus_t Com_GameManagerFormatModeLaunch(
	const comGameProfile_t *current, const comGameProfile_t *target,
	const char *basePath, const char *homePath, int fullscreen,
	char *destination, unsigned capacity )
{
	if ( !current || !target || Com_GameProfileById( current->id ) != current ||
		Com_GameProfileById( target->id ) != target ) return COM_GAME_MODE_LAUNCH_INVALID_PROFILE;
	if ( current == target ) return COM_GAME_MODE_LAUNCH_ALREADY_SELECTED;
	if ( current->titleId != target->titleId ) return COM_GAME_MODE_LAUNCH_DIFFERENT_TITLE;
	if ( !destination || !basePath || !basePath[0] || !homePath || !homePath[0] ||
		( fullscreen != 0 && fullscreen != 1 ) ) return COM_GAME_MODE_LAUNCH_INVALID_VALUE;
	for ( const char *value : { basePath, homePath } ) {
		unsigned length = 0;
		for ( ; length < COM_GAME_MODE_LAUNCH_BYTES && value[length]; ++length ) {
			const unsigned char ch = static_cast<unsigned char>( value[length] );
			if ( ch < 32 || ch == 127 || ch == '"' ) return COM_GAME_MODE_LAUNCH_INVALID_VALUE;
		}
		if ( length == COM_GAME_MODE_LAUNCH_BYTES ) return COM_GAME_MODE_LAUNCH_TOO_LONG;
	}
	std::string command = "+set com_gameProfile \"";
	command += target->key;
	command += "\" +set fs_basepath \"";
	command += basePath;
	command += "\" +set fs_homepath \"";
	command += homePath;
	command += "\" +set fs_game \"\" +set r_fullscreen \"";
	command += fullscreen ? '1' : '0';
	command += '"';
	if ( command.size() >= COM_GAME_MODE_LAUNCH_BYTES || command.size() >= capacity )
		return COM_GAME_MODE_LAUNCH_TOO_LONG;
	std::memcpy( destination, command.c_str(), command.size() + 1 );
	return COM_GAME_MODE_LAUNCH_READY;
}

const char *Com_GameManagerModeLaunchStatusMessage( comGameModeLaunchStatus_t status )
{
	switch ( status ) {
	case COM_GAME_MODE_LAUNCH_READY: return "mode relaunch command is ready";
	case COM_GAME_MODE_LAUNCH_INVALID_PROFILE: return "choose a registered game mode";
	case COM_GAME_MODE_LAUNCH_ALREADY_SELECTED: return "this game mode is already selected";
	case COM_GAME_MODE_LAUNCH_DIFFERENT_TITLE: return "mode switching requires the same game title";
	case COM_GAME_MODE_LAUNCH_INVALID_VALUE: return "mode switching requires non-empty paths without quotes or control characters and a valid window mode";
	case COM_GAME_MODE_LAUNCH_TOO_LONG: return "mode relaunch paths exceed the startup command capacity";
	default: return "invalid mode relaunch request";
	}
}

const char *Com_GameManagerConfigFile( const comGameProfile_t *profile,
	int dedicated )
{
	if ( !profile ) return nullptr;
	return dedicated ? profile->serverConfig : profile->clientConfig;
}

const char *Com_GameManagerServerCacheFile( const comGameProfile_t *profile )
{
	if ( !profile || profile != Com_GameProfileById( profile->id ) ) return nullptr;
	return profile->serverCacheFile;
}

const char *Com_GameConfigFile( void )
{
#ifdef DEDICATED
	return Com_GameManagerConfigFile( activeProfile, 1 );
#else
	return Com_GameManagerConfigFile( activeProfile, 0 );
#endif
}

int Com_GameManagerIsConfigFile( const char *filename )
{
	if ( EqualsIgnoreCase( filename, "autoexec.cfg" )
		|| EqualsIgnoreCase( filename, "repconfig.cfg" ) ) return 1;
	for ( const auto &profile : profiles ) {
		/* Every other profile's configuration is named after its own title, so
		 * banning all of them from every pak costs nothing. Alice's retail name
		 * is the generic config.cfg, and banning that everywhere would silently
		 * hide a file another game's pak may legitimately carry, so Alice's two
		 * names are protected only while Alice is the selected profile.
		 * F.A.K.K.2 shares the names and the rule. */
		if ( ( profile.titleId == COM_GAME_TITLE_ALICE || profile.titleId == COM_GAME_TITLE_FAKK2 )
			&& activeProfile != &profile ) continue;
		if ( EqualsIgnoreCase( filename, profile.clientConfig )
			|| EqualsIgnoreCase( filename, profile.serverConfig ) ) return 1;
	}
	return 0;
}

int Com_GameManagerNativeModuleFilename( const comGameProfile_t *profile,
	const char *module, const char *architecture, const char *extension,
	unsigned candidate, char *destination, unsigned capacity )
{
	if ( !profile || Com_GameProfileById( profile->id ) != profile
		|| !module || !architecture || !extension || !destination || !capacity
		|| ( std::strcmp( module, "cgame" ) && std::strcmp( module, "qagame" )
			&& std::strcmp( module, "ui" ) ) ) return 0;
	const auto architectureLength = std::strlen( architecture );
	if ( !architectureLength || architectureLength > 32 ) return 0;
	for ( const char *p = architecture; *p; ++p ) {
		if ( !( ( *p >= 'a' && *p <= 'z' ) || ( *p >= 'A' && *p <= 'Z' )
			|| ( *p >= '0' && *p <= '9' ) || *p == '_' ) ) return 0;
	}
	const bool windows = !std::strcmp( extension, ".dll" );
	if ( !windows && std::strcmp( extension, ".so" )
		&& std::strcmp( extension, ".dylib" ) ) return 0;
	if ( profile->id == COM_GAME_QUAKE3_IHV ) {
		// The supplied IHV build has two Win32 x86 function-table libraries
		// and no separate UI module.
		if ( candidate || !windows || std::strcmp( architecture, "x86" )
			|| !std::strcmp( module, "ui" ) ) return 0;
		const char *leaf = !std::strcmp( module, "qagame" ) ? "qagamex86.dll" : "cgamex86.dll";
		const auto length = std::strlen( leaf ); if ( length >= capacity ) return 0;
		std::memcpy( destination, leaf, length + 1 ); return 1;
	}
	if ( profile->id == COM_GAME_Q3TEST ) {
		/* Every Q3Test installer put its Win32 x86 libraries beside
		 * quake3.exe: qagamex86.dll, cgamex86.dll from 1.05 and uix86.dll
		 * from 1.06. There is no pak-borne module and no other platform's. */
		if ( candidate || !windows || std::strcmp( architecture, "x86" ) ) return 0;
		const char *leaf = !std::strcmp( module, "qagame" ) ? "qagamex86.dll"
			: !std::strcmp( module, "cgame" ) ? "cgamex86.dll" : "uix86.dll";
		const auto length = std::strlen( leaf ); if ( length >= capacity ) return 0;
		std::memcpy( destination, leaf, length + 1 ); return 1;
	}
	if ( profile->titleId == COM_GAME_TITLE_JKA ) {
		/* Raven shipped Win32 x86 modules only. Single player loads one
		 * jagamex86.dll, beside jasp.exe in the installation root, for both
		 * its game and its client game, and has no menu module. Multiplayer's
		 * three modules ship inside base/assets2.pk3 and assets3.pk3, so they
		 * are read through the filesystem like any other pak member. */
		if ( candidate || !windows || std::strcmp( architecture, "x86" ) ) return 0;
		const bool singlePlayer = profile->id == COM_GAME_JKA_SP;
		if ( singlePlayer && !std::strcmp( module, "ui" ) ) return 0;
		const char *leaf;
		if ( !std::strcmp( module, "ui" ) ) leaf = "uix86.dll";
		else if ( !std::strcmp( module, "cgame" ) ) leaf = singlePlayer ? "jagamex86.dll" : "cgamex86.dll";
		else leaf = singlePlayer ? "jagamex86.dll" : "jampgamex86.dll";
		const auto length = std::strlen( leaf );
		if ( length >= capacity ) return 0;
		std::memcpy( destination, leaf, length + 1 );
		return 1;
	}
	if ( profile->id == COM_GAME_JK2_SP ) {
		/* Retail jk2sp.exe loads one jk2gamex86.dll from the installation root
		 * for both its game and its client game, as Jedi Academy's single
		 * player does, and has no menu module of its own. Its multiplayer is
		 * not here at all: Raven shipped that as QVMs inside assets0.pk3. */
		if ( candidate || !windows || std::strcmp( architecture, "x86" ) ) return 0;
		if ( !std::strcmp( module, "ui" ) ) return 0;
		const char *leaf = "jk2gamex86.dll";
		const auto length = std::strlen( leaf );
		if ( length >= capacity ) return 0;
		std::memcpy( destination, leaf, length + 1 );
		return 1;
	}
	if ( profile->titleId == COM_GAME_TITLE_ALICE ) {
		/* Ritual shipped Win32 x86 libraries only, and both sit in base/
		 * beside the paks rather than in the installation root. The game is
		 * fgamex86.dll, not a qagame; the client game keeps Quake III's
		 * cgamex86.dll name but exports GetCGameAPI instead of vmMain. */
		if ( candidate || !windows || std::strcmp( architecture, "x86" )
			|| !std::strcmp( module, "ui" ) ) return 0;
		const char *leaf = std::strcmp( module, "cgame" ) ? "fgamex86.dll" : "cgamex86.dll";
		const auto length = std::strlen( leaf );
		if ( length >= capacity ) return 0;
		std::memcpy( destination, leaf, length + 1 );
		return 1;
	}
	if ( profile->titleId == COM_GAME_TITLE_FAKK2 ) {
		/* The installer's "Game DLLs" group puts both libraries in fakk/:
		 * gamex86.dll, which is neither Quake III's qagamex86.dll nor Alice's
		 * fgamex86.dll, and cgamex86.dll exporting GetCGameAPI. Win32 x86
		 * only, and no menu module. */
		if ( candidate || !windows || std::strcmp( architecture, "x86" )
			|| !std::strcmp( module, "ui" ) ) return 0;
		const char *leaf = std::strcmp( module, "cgame" ) ? "gamex86.dll" : "cgamex86.dll";
		const auto length = std::strlen( leaf );
		if ( length >= capacity ) return 0;
		std::memcpy( destination, leaf, length + 1 );
		return 1;
	}
	if ( profile->titleId == COM_GAME_TITLE_MOHAA ) {
		/* The installer puts both libraries in main/ beside the paks:
		 * gamex86.dll exporting GetGameAPI and cgamex86.dll exporting
		 * GetCGameAPI. 2015 shipped Win32 x86 libraries only, and the menus are
		 * the engine's own .urc resources, so there is no menu module. */
		if ( candidate || !windows || std::strcmp( architecture, "x86" )
			|| !std::strcmp( module, "ui" ) ) return 0;
		const char *leaf = std::strcmp( module, "cgame" ) ? "gamex86.dll" : "cgamex86.dll";
		const auto length = std::strlen( leaf );
		if ( length >= capacity ) return 0;
		std::memcpy( destination, leaf, length + 1 );
		return 1;
	}
	if ( profile->id == COM_GAME_STVEF_SP ) {
		/* Retail stvoy.exe loads the game and cgame from one efgamex86.dll and
		 * the menus from efuix86.dll. Raven shipped only Win32 x86 modules. */
		if ( candidate || !windows || std::strcmp( architecture, "x86" ) ) return 0;
		const char *leaf = std::strcmp( module, "ui" ) ? "efgamex86.dll" : "efuix86.dll";
		const auto length = std::strlen( leaf );
		if ( length >= capacity ) return 0;
		std::memcpy( destination, leaf, length + 1 );
		return 1;
	}
	const bool rtcw = profile->titleId == COM_GAME_TITLE_RTCW || profile->id == COM_GAME_WOLFET;
	const char *arch = architecture;
	if ( rtcw ) {
		if ( windows && !std::strcmp( arch, "x86_64" ) ) arch = "x64";
		else if ( windows && !std::strcmp( arch, "i386" ) ) arch = "x86";
		else if ( !windows && !std::strcmp( arch, "x86" ) ) arch = "i386";
	}
	std::string name = module;
	if ( rtcw ) {
		if ( candidate == 0 ) {
			name += profile->id == COM_GAME_RTCW_SP
				? ( windows ? "_sp_" : ".sp." ) : ( windows ? "_mp_" : ".mp." );
		} else if ( candidate != 1 || profile->id != COM_GAME_RTCW_SP ) {
			return 0;
		}
	} else if ( candidate ) {
		// Preserve the existing Linux game-only x64 alias after the normal name.
		if ( candidate != 1 || std::strcmp( extension, ".so" )
			|| std::strcmp( arch, "x86_64" ) || std::strcmp( module, "qagame" ) ) return 0;
		arch = "x64";
	}
	name += arch;
	name += extension;
	if ( name.size() >= capacity ) return 0;
	std::memcpy( destination, name.c_str(), name.size() + 1 );
	return 1;
}

int Com_GameManagerPureModuleFilename( const comGameProfile_t *profile,
	unsigned slot, const char *architecture, const char *extension,
	char *destination, unsigned capacity )
{
	if ( !profile || Com_GameProfileById( profile->id ) != profile || !destination || !capacity ) return 0;
	const char *name;
	switch ( profile->pureProof ) {
	case COM_PURE_PROOF_QVM:
		if ( slot > 1 ) return 0;
		name = slot ? "vm/ui.qvm" : "vm/cgame.qvm";
		break;
	case COM_PURE_PROOF_QL_NATIVE:
		if ( slot ) return 0;
		name = "cgamex86.dll"; // Retail QL's native contract is fixed to x86.
		break;
	case COM_PURE_PROOF_RTCW_NATIVE:
	case COM_PURE_PROOF_WOLFET_NATIVE:
	case COM_PURE_PROOF_JKA_NATIVE:
		if ( slot > 1 ) return 0;
		return Com_GameManagerNativeModuleFilename( profile, slot ? "ui" : "cgame",
			architecture, extension, 0, destination, capacity );
	default: return 0;
	}
	const auto length = std::strlen( name );
	if ( length >= capacity ) return 0;
	std::memcpy( destination, name, length + 1 );
	return 1;
}

int Com_GameManagerNativeModuleCandidate( const comGameProfile_t *profile,
	const char *module, const char *architecture, const char *extension,
	unsigned candidate, char *destination, unsigned capacity, int *installationRoot )
{
	if ( !installationRoot ) return 0;
	if ( profile && ( profile == Com_GameProfileById( COM_GAME_STVEF_SP )
		|| profile == Com_GameProfileById( COM_GAME_JKA_SP )
		|| profile == Com_GameProfileById( COM_GAME_JK2_SP )
		|| profile == Com_GameProfileById( COM_GAME_QUAKE3_IHV )
		|| profile == Com_GameProfileById( COM_GAME_Q3TEST ) ) ) {
		/* These single-player engines never read their modules from paks or
		 * mod directories; they are installed beside the executable, where a
		 * retail Jedi Academy keeps jagamex86.dll beside jasp.exe. */
		if ( !Com_GameManagerNativeModuleFilename( profile, module, architecture,
			extension, candidate, destination, capacity ) ) return 0;
		*installationRoot = 1;
		return 1;
	}
	const bool root = profile == Com_GameProfileById( COM_GAME_RTCW_SP ) && candidate >= 2;
	if ( !Com_GameManagerNativeModuleFilename( profile, module, architecture,
		extension, root ? candidate - 2 : candidate, destination, capacity ) ) return 0;
	*installationRoot = root ? 1 : 0;
	return 1;
}

int Com_GameManagerResolveRootModule( const comGameProfile_t *profile,
	const char *root, const char *filename, const char *architecture,
	const char *extension, char *destination, unsigned capacity )
{
	if ( !profile || ( profile != Com_GameProfileById( COM_GAME_RTCW_SP )
			&& profile != Com_GameProfileById( COM_GAME_STVEF_SP )
			&& profile != Com_GameProfileById( COM_GAME_JKA_SP )
			&& profile != Com_GameProfileById( COM_GAME_JK2_SP )
			&& profile != Com_GameProfileById( COM_GAME_QUAKE3_IHV )
			&& profile != Com_GameProfileById( COM_GAME_Q3TEST ) )
		|| !root || !root[0] || !filename || !destination || !capacity ) return 0;
	bool known = false;
	for ( const char *module : { "cgame", "qagame", "ui" } ) {
		char candidate[64];
		for ( unsigned i = 0; Com_GameManagerNativeModuleFilename( profile,
			module, architecture, extension, i, candidate, sizeof( candidate ) ); ++i ) {
			if ( !std::strcmp( filename, candidate ) ) known = true;
		}
	}
	if ( !known ) return 0;
	try {
		std::error_code error;
		const auto base = std::filesystem::canonical( std::filesystem::u8path( root ), error );
		if ( error ) return 0;
		const auto file = std::filesystem::canonical( base / filename, error );
		if ( error || file.parent_path() != base || !IsRegularFile( file ) ) return 0;
		const auto path = file.u8string();
		if ( path.size() >= capacity ) return 0;
		std::memcpy( destination, path.c_str(), path.size() + 1 );
		return 1;
	} catch ( const std::exception & ) {
		return 0;
	}
}

int Com_GameManagerInstallationMatches( const comGameProfile_t *profile,
	unsigned markers )
{
	if ( !profile || !( markers & profile->installMarker ) ) return 0;
	// Retail Q3 and IHV use separate installations even though they share a title.
	if ( profile->titleId == COM_GAME_TITLE_QUAKE3 ) return markers == profile->installMarker;
	unsigned titleMarkers = 0;
	for ( const auto &candidate : profiles ) {
		if ( candidate.titleId == profile->titleId ) titleMarkers |= candidate.installMarker;
	}
	return !( markers & ~titleMarkers );
}

int Com_GameManagerAllowsPak( const comGameProfile_t *profile,
	const char *filename )
{
	if ( !profile || !filename || !filename[0]
		|| std::strpbrk( filename, "/\\:" ) ) return 0;
	return !HasPakPrefix( filename, profile->pakExcludePrefix );
}

int Com_GameManagerComparePaks( const comGameProfile_t *profile,
	const char *left, const char *right )
{
	if ( !left || !right ) return left ? 1 : right ? -1 : 0;
	const bool leftOverride = profile && HasPakPrefix( left, profile->pakOverridePrefix );
	const bool rightOverride = profile && HasPakPrefix( right, profile->pakOverridePrefix );
	for ( unsigned i = 0; ; ++i ) {
		const auto l = std::tolower( static_cast<unsigned char>( leftOverride && i < 2 ? 'z' : left[i] ) );
		const auto r = std::tolower( static_cast<unsigned char>( rightOverride && i < 2 ? 'z' : right[i] ) );
		if ( l != r ) return l < r ? -1 : 1;
		if ( !l ) {
			// A literal zz_ archive and a mode archive may have equal virtual
			// keys. Break ties by actual name for deterministic mount order.
			if ( leftOverride != rightOverride ) return leftOverride ? -1 : 1;
			return 0;
		}
	}
}

const comGameProfile_t *Com_GameProfileById( comGameId_t id )
{
	for ( const auto &profile : profiles ) {
		if ( profile.id == id ) return &profile;
	}
	return nullptr;
}

comGameDetectionResult_t Com_DetectGameProfileMarkers(
	const comGameMarkerInputs_t *inputs )
{
	comGameDetectionResult_t result;

	if ( !inputs ) {
		return Result( COM_GAME_DETECTION_NOT_FOUND, COM_GAME_EVIDENCE_NONE, nullptr );
	}

	const char *key = inputs->requestedProfile;
	const comGameProfile_t *requested = Com_GameManagerFindProfile( key );
	if ( requested ) {
		return Result( COM_GAME_DETECTION_OK, COM_GAME_EVIDENCE_EXPLICIT,
			requested );
	}
	for ( const auto &title : titles ) {
		if ( EqualsIgnoreCase( key, title.key ) ) {
			return Result( COM_GAME_DETECTION_MODE_REQUIRED,
				COM_GAME_EVIDENCE_EXPLICIT, nullptr );
		}
	}
	if ( key && key[0] && !EqualsIgnoreCase( key, "auto" ) ) {
		return Result( COM_GAME_DETECTION_UNKNOWN_REQUEST,
			COM_GAME_EVIDENCE_EXPLICIT, nullptr );
	}

	result = DetectCandidate( inputs->basePathMarkers, COM_GAME_EVIDENCE_BASE_PATH );
	if ( result.status != COM_GAME_DETECTION_NOT_FOUND ) {
		return result;
	}

	result = DetectCandidate( inputs->appPathMarkers, COM_GAME_EVIDENCE_APP_PATH );
	if ( result.status != COM_GAME_DETECTION_NOT_FOUND ) {
		return result;
	}

	result = DetectCandidate( inputs->steamPathMarkers, COM_GAME_EVIDENCE_STEAM_PATH );
	if ( result.status != COM_GAME_DETECTION_NOT_FOUND ) {
		return result;
	}

	return Result( COM_GAME_DETECTION_NOT_FOUND, COM_GAME_EVIDENCE_NONE, nullptr );
}

unsigned Com_ProbeGameProfileMarkers( const char *rootPath )
{
	unsigned markers = COM_GAME_MARKER_NONE;

	if ( !rootPath || !rootPath[0] ) {
		return markers;
	}

	try {
		const auto root = std::filesystem::u8path( rootPath );
		/* Two titles can name the same file: Return to Castle Wolfenstein's
		 * main/pak0.pk3 and Medal of Honor's main/Pak0.pk3 are one file on a
		 * case-insensitive filesystem. A pak is unrecognized only when no
		 * profile that probes that file recognizes it; one title's probe must
		 * not condemn the other's installation. */
		struct ProbedPak {
			std::filesystem::path path;
			unsigned recognized;
		};
		std::vector<ProbedPak> probed;
		for ( const auto &profile : profiles ) {
			if ( !profile.basePakName ) continue;
			const auto pak = root / profile.baseGame / profile.basePakName;
			if ( !IsRegularFile( pak ) ) continue;
			ProbedPak *entry = nullptr;
			for ( auto &candidate : probed ) {
				std::error_code error;
				if ( std::filesystem::equivalent( candidate.path, pak, error ) && !error ) {
					entry = &candidate;
					break;
				}
			}
			if ( !entry ) {
				probed.push_back( { pak, 0 } );
				entry = &probed.back();
			}
			if ( RecognizedPak( pak, profile.installMarker ) ) {
				entry->recognized |= profile.installMarker;
			}
		}
		for ( const auto &entry : probed ) {
			markers |= entry.recognized ? entry.recognized : COM_GAME_MARKER_UNRECOGNIZED_PAK;
		}
		if ( fntech3::q3ihv::install::Recognizes( root ) ) markers |= COM_GAME_MARKER_QUAKE3_IHV;
		if ( fntech3::q3test::install::Recognize( root ) != Q3TEST_RELEASE_NONE ) markers |= COM_GAME_MARKER_Q3TEST;
	} catch ( const std::system_error & ) {
		// Windows path conversion can fail before the error-code filesystem
		// queries below. An unrepresentable candidate supplies no evidence;
		// do not let a conversion exception escape this C-facing probe.
		return markers;
	}

	return markers;
}

comGameDetectionResult_t Com_DetectGameProfile(
	const comGameDetectionInputs_t *inputs )
{
	comGameMarkerInputs_t markerInputs = {};

	if ( !inputs ) {
		return Result( COM_GAME_DETECTION_NOT_FOUND, COM_GAME_EVIDENCE_NONE, nullptr );
	}

	markerInputs.requestedProfile = inputs->requestedProfile;
	if ( inputs->requestedProfile && inputs->requestedProfile[0]
		&& !EqualsIgnoreCase( inputs->requestedProfile, "auto" ) ) {
		return Com_DetectGameProfileMarkers( &markerInputs );
	}
	markerInputs.basePathMarkers = Com_ProbeGameProfileMarkers( inputs->basePath );
	if ( markerInputs.basePathMarkers ) return Com_DetectGameProfileMarkers( &markerInputs );
	markerInputs.appPathMarkers = Com_ProbeGameProfileMarkers( inputs->appPath );
	if ( markerInputs.appPathMarkers ) return Com_DetectGameProfileMarkers( &markerInputs );
	for ( const auto &profile : profiles ) {
		const char *path = inputs->steamPaths[profile.id];
		if ( !path || !path[0] ) continue;
		bool duplicate = false;
		for ( unsigned i = 1; i < profile.id; ++i ) {
			const char *earlier = inputs->steamPaths[i];
			if ( earlier && !std::strcmp( earlier, path ) ) duplicate = true;
		}
		if ( !duplicate ) markerInputs.steamPathMarkers |= Com_ProbeGameProfileMarkers( path );
	}
	return Com_DetectGameProfileMarkers( &markerInputs );
}

int Com_ActivateGameProfile( comGameId_t id )
{
#if defined( FNTECH3_Q3_ONLY )
	if ( id != COM_GAME_QUAKE3 ) {
		return 0;
	}
#endif
	const comGameProfile_t *requested = Com_GameProfileById( id );

	if ( !Com_GameManagerCanActivate( requested ) ) {
		return 0;
	}
	if ( activeProfile && activeProfile != requested ) {
		return 0;
	}

	activeProfile = requested;
	return 1;
}

const comGameProfile_t *Com_GameProfile( void )
{
	return activeProfile;
}

int Com_GameClientLimit( void )
{
	return activeProfile ? activeProfile->maxClients : 0;
}

int Com_GameConfigstringLimit( void )
{
	return activeProfile ? activeProfile->maxConfigstrings : 0;
}

int Com_GamePlayerConfigstringBase( void )
{
	return activeProfile ? activeProfile->playerConfigstringBase : 0;
}

int Com_GameIsQuake3( void )
{
	return activeProfile && activeProfile->id == COM_GAME_QUAKE3;
}

int Com_GameIsQuake3IHV( void )
{
	return activeProfile && activeProfile->id == COM_GAME_QUAKE3_IHV;
}

int Com_GameIsQuakeLive( void )
{
	return activeProfile && activeProfile->id == COM_GAME_QUAKELIVE;
}

int Com_GameIsRTCW( void )
{
	return activeProfile && activeProfile->titleId == COM_GAME_TITLE_RTCW;
}

int Com_GameIsRTCWSP( void )
{
	return activeProfile && activeProfile->id == COM_GAME_RTCW_SP;
}

int Com_GameIsRTCWMP( void )
{
	return activeProfile && activeProfile->id == COM_GAME_RTCW_MP;
}

int Com_GameIsWolfET( void )
{
	return activeProfile && activeProfile->id == COM_GAME_WOLFET;
}

int Com_GameIsSTVEF( void )
{
	return activeProfile && activeProfile->titleId == COM_GAME_TITLE_STVEF;
}

int Com_GameIsSTVEFSP( void )
{
	return activeProfile && activeProfile->id == COM_GAME_STVEF_SP;
}

int Com_GameIsSTVEFMP( void )
{
	return activeProfile && activeProfile->id == COM_GAME_STVEF_MP;
}

int Com_GameIsJKA( void )
{
	return activeProfile && activeProfile->titleId == COM_GAME_TITLE_JKA;
}

int Com_GameIsJKASP( void )
{
	return activeProfile && activeProfile->id == COM_GAME_JKA_SP;
}

int Com_GameIsJKAMP( void )
{
	return activeProfile && activeProfile->id == COM_GAME_JKA_MP;
}

int Com_GameIsJK2( void )
{
	return activeProfile && activeProfile->titleId == COM_GAME_TITLE_JK2;
}

int Com_GameIsJK2SP( void )
{
	return activeProfile && activeProfile->id == COM_GAME_JK2_SP;
}

int Com_GameIsJK2MP( void )
{
	return activeProfile && activeProfile->id == COM_GAME_JK2_MP;
}

int Com_GameIsAlice( void )
{
	return activeProfile && activeProfile->titleId == COM_GAME_TITLE_ALICE;
}

int Com_GameIsFAKK2( void )
{
	return activeProfile && activeProfile->titleId == COM_GAME_TITLE_FAKK2;
}

int Com_GameIsRitual( void )
{
	return Com_GameIsAlice() || Com_GameIsFAKK2();
}

int Com_GameIsMOHAA( void )
{
	return activeProfile && activeProfile->titleId == COM_GAME_TITLE_MOHAA;
}

int Com_GameIsQ3Test( void )
{
	return activeProfile && activeProfile->id == COM_GAME_Q3TEST;
}

int Com_GamePreferredProtocol( void )
{
	const q3testRelease_t *release = Com_Q3TestRelease();
	return release ? release->protocol : activeProfile ? activeProfile->preferredProtocol : 0;
}

int Com_GamePreferredDemoProtocol( void )
{
	const q3testRelease_t *release = Com_Q3TestRelease();
	return release ? release->protocol : activeProfile ? activeProfile->preferredDemoProtocol : 0;
}

const q3testRelease_t *Com_Q3TestReleaseById( q3testReleaseId_t id )
{
	return fntech3::q3test::install::ById( id );
}

const q3testRelease_t *Com_Q3TestRelease( void )
{
	return Com_GameIsQ3Test() ? activeQ3TestRelease : nullptr;
}

int Com_Q3TestActivateRelease( q3testReleaseId_t id )
{
	const q3testRelease_t *release = Com_Q3TestReleaseById( id );
	if ( !release || !Com_GameIsQ3Test() ) return 0;
	if ( activeQ3TestRelease && activeQ3TestRelease != release ) return 0;
	activeQ3TestRelease = release;
	return 1;
}

q3testReleaseId_t Com_Q3TestDetectRelease( const char *rootPath )
{
	if ( !rootPath || !rootPath[0] ) return Q3TEST_RELEASE_NONE;
	try {
		return fntech3::q3test::install::Recognize( std::filesystem::u8path( rootPath ) );
	} catch ( const std::exception & ) {
		return Q3TEST_RELEASE_NONE;
	}
}

int Com_GameDefaultServerPort( void )
{
	/* Medal of Honor's engine listens on 12203 (OpenMoHAA's PORT_SERVER, and
	 * the port its retail servers are known by); every other profile keeps
	 * Quake III's 27960 (qcommon.h PORT_SERVER). */
	return Com_GameIsMOHAA() ? 12203 : 27960;
}

int Com_GameStateCharLimit( void )
{
	/* Medal of Honor 1.00's gamestate text is 32,000 bytes: its client game
	 * keeps 2,736 offsets, then the text, then the count, and the graphics
	 * configuration follows 32,000 bytes after the text begins (cgamex86.dll
	 * 0x30023504, 0x3002350e). OpenMoHAA's 41,952 is its later protocol's.
	 * Every other profile's is Quake III's 16,000. */
	return Com_GameIsMOHAA() ? 32000 : 16000;
}

int Com_PureProofIsNativeModulePair( const comGameProfile_t *profile )
{
	return profile && ( profile->pureProof == COM_PURE_PROOF_RTCW_NATIVE
		|| profile->pureProof == COM_PURE_PROOF_WOLFET_NATIVE
		|| profile->pureProof == COM_PURE_PROOF_JKA_NATIVE );
}


unsigned Com_ProtocolFamilyForVersion( int protocol )
{
	switch ( protocol ) {
		case 1:
			/* Alice's and F.A.K.K.2's engines both speak 1. */
			return COM_PROTOCOL_FAMILY_ALICE | COM_PROTOCOL_FAMILY_FAKK2;
		case 6:
			return COM_PROTOCOL_FAMILY_MOHAA;
		case 33:
			return COM_PROTOCOL_FAMILY_QUAKE3_IHV;
		case 35:
		case 36:
		case 38:
		case 39:
			/* Q3Test 1.02/1.03, 1.05, 1.06 and 1.07; 1.08's 40 is below. */
			return COM_PROTOCOL_FAMILY_Q3TEST;
		case 24:
			return COM_PROTOCOL_FAMILY_STVEF_HM;
		case 26:
			/* ioEF and Jedi Academy multiplayer both speak 26, as Elite
			 * Force's single player and Jedi Academy's both speak 40. A
			 * version names every family that uses it; a profile admits
			 * only the family it was registered with. */
			return COM_PROTOCOL_FAMILY_IOEF | COM_PROTOCOL_FAMILY_JKA_MP;
		case 15:
			return COM_PROTOCOL_FAMILY_JK2_MP;
		case 40:
			/* Three single players speak 40: Elite Force's, Jedi Academy's
			 * and Jedi Outcast's. So does Q3Test 1.08, a year before them. */
			return COM_PROTOCOL_FAMILY_STVEF_SP | COM_PROTOCOL_FAMILY_JKA_SP
				| COM_PROTOCOL_FAMILY_JK2_SP | COM_PROTOCOL_FAMILY_Q3TEST;
		case 49:
			return COM_PROTOCOL_FAMILY_RTCW_SP;
		case 50:
			return COM_PROTOCOL_FAMILY_IORTCW_SP;
		case 60:
			return COM_PROTOCOL_FAMILY_RTCW_MP;
		case 61:
			return COM_PROTOCOL_FAMILY_IORTCW_MP;
		case 66:
		case 67:
			return COM_PROTOCOL_FAMILY_Q3_POINT_RELEASES;
		case 68:
			return COM_PROTOCOL_FAMILY_LEGACY_QUAKE3;
		case 71:
			return COM_PROTOCOL_FAMILY_IOQUAKE3;
		case 84:
			return COM_PROTOCOL_FAMILY_WOLFET;
		case 91:
			return COM_PROTOCOL_FAMILY_QUAKELIVE;
		default:
			return 0;
	}
}

int Com_GameProfileAllowsProtocol( const comGameProfile_t *profile,
	int protocol )
{
	const unsigned family = Com_ProtocolFamilyForVersion( protocol );
	/* Once Q3Test has pinned a release, that release's number is its only
	 * protocol: the family spans five incompatible contracts. */
	if ( profile && profile == activeProfile && activeQ3TestRelease && profile->id == COM_GAME_Q3TEST ) {
		return protocol == activeQ3TestRelease->protocol;
	}
	return profile && family != 0
		&& ( profile->allowedProtocolFamilies & family ) != 0;
}

int Com_GameProfileAllowsModuleApi( const comGameProfile_t *profile,
	unsigned moduleApi )
{
	const unsigned knownApis = COM_MODULE_API_QVM
		| COM_MODULE_API_LEGACY_NATIVE
		| COM_MODULE_API_STRUCTURED_NATIVE
		| COM_MODULE_API_STVEF_SP_NATIVE
		| COM_MODULE_API_QUAKE3_IHV_NATIVE
		| COM_MODULE_API_JKA_SP_NATIVE
		| COM_MODULE_API_JK2_SP_NATIVE
		| COM_MODULE_API_ALICE_NATIVE
		| COM_MODULE_API_FAKK2_NATIVE
		| COM_MODULE_API_MOHAA_NATIVE
		| COM_MODULE_API_Q3TEST_NATIVE;
	return profile && moduleApi != 0
		&& ( moduleApi & ( moduleApi - 1 ) ) == 0
		&& ( moduleApi & knownApis ) != 0
		&& ( profile->allowedModuleApis & moduleApi ) != 0;
}

const char *Com_GameDetectionStatusName( comGameDetectionStatus_t status )
{
	switch ( status ) {
		case COM_GAME_DETECTION_OK:
			return "ok";
		case COM_GAME_DETECTION_NOT_FOUND:
			return "not found";
		case COM_GAME_DETECTION_AMBIGUOUS:
			return "ambiguous";
		case COM_GAME_DETECTION_UNKNOWN_REQUEST:
			return "unknown profile";
		case COM_GAME_DETECTION_UNSUPPORTED_BUILD:
			return "this game is unavailable in the Q3-only build; use the shared Win32 fnTech3 binary";
		case COM_GAME_DETECTION_UNRECOGNIZED_PAK:
			return "unrecognized base pak identity; verify the installation or select its profile explicitly";
		case COM_GAME_DETECTION_MODE_REQUIRED:
			return "choose the game mode explicitly with +set com_gameProfile rtcw-sp, rtcw-mp, stvef-sp, stvef-mp, jka-sp, jka-mp, jk2-sp or jk2-mp";
		case COM_GAME_DETECTION_INCOMPLETE_ENGINE:
			return "the game is recognized, but its engine adapters are not yet complete; see docs/fntech3/COMPATIBILITY.md";
		default:
			return "invalid status";
	}
}

const char *Com_GameDetectionEvidenceName( comGameDetectionEvidence_t evidence )
{
	switch ( evidence ) {
		case COM_GAME_EVIDENCE_EXPLICIT:
			return "command line";
		case COM_GAME_EVIDENCE_BASE_PATH:
			return "fs_basepath";
		case COM_GAME_EVIDENCE_APP_PATH:
			return "application directory";
		case COM_GAME_EVIDENCE_STEAM_PATH:
			return "Steam installation";
		default:
			return "none";
	}
}

} // extern "C"
