"""Execute the actual HUD module with synthetic filesystem and renderer endpoints."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]

HARNESS = r'''
#define _CRT_SECURE_NO_WARNINGS 1
#include "code/client/cl_hud.cpp"
#include <cassert>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <map>
#include <string>
#include <vector>

clientStatic_t cls{};
refexport_t re{};
static cvar_t capture{}, hideHud{};
cvar_t *cl_captureActive = &capture, *r_levelshotHideHud = &hideHud;
static std::map<std::string, cvar_t> variables;
static std::map<std::string, std::string> files;
static std::string dump;
static int profile, reads, writes, registrations, commands, draws, liveFiles;
static void (*reload)();
static std::array<float, 8> submitted;
static qhandle_t submittedShader, pictureShader = 17;
// 0 Q3, 1 QL, 2 RTCW SP, 3 RTCW MP, 4 F.A.K.K.2, 5 ET, 6 EF SP, 7 EF MP, 8 Alice, 9 MoHAA
int Com_GameIsQuake3() { return profile == 0; }
int Com_GameIsRTCW() { return profile == 2 || profile == 3; }
int Com_GameIsWolfET() { return profile == 5; }
int Com_GameIsSTVEF() { return profile == 6 || profile == 7; }
int Com_GameIsRitual() { return profile == 4 || profile == 8; }
static bool Unsupported() { return profile == 1 || profile == 9; }
cvar_t *Cvar_Get(const char *name, const char *value, int) {
    ++registrations;
    auto entry = variables.emplace(name, cvar_t{});
    if (entry.second) entry.first->second.integer = std::atoi(value);
    return &entry.first->second;
}
void Cvar_CheckRange(cvar_t *, const char *, const char *, cvarValidator_t) {}
void Cvar_SetDescription(cvar_t *, const char *) {}
void Cmd_AddCommand(const char *name, xcommand_t function) {
    assert(!std::strcmp(name, "hud_reload")); ++commands; reload = function;
}
void Cmd_RemoveCommand(const char *name) { assert(!std::strcmp(name, "hud_reload")); --commands; reload = nullptr; }
void QDECL Com_Printf(const char *, ...) {}
void QDECL Com_Error(errorParm_t, const char *, ...) { std::abort(); }
const char *QDECL va(const char *format, ...) {
    static char buffer[1024]; va_list args; va_start(args, format);
    std::vsnprintf(buffer, sizeof(buffer), format, args); va_end(args); return buffer;
}
int Q_stricmpn(const char *a, const char *b, int n) {
    for (int i = 0; i < n; ++i) {
        int ca = std::tolower(static_cast<unsigned char>(a[i]));
        int cb = std::tolower(static_cast<unsigned char>(b[i]));
        if (ca != cb || !ca) return ca - cb;
    }
    return 0;
}
int Q_stricmp(const char *a, const char *b) { return Q_stricmpn(a, b, INT_MAX); }
void Q_strncpyz(char *destination, const char *source, int size) {
    assert(size > 0); std::snprintf(destination, static_cast<std::size_t>(size), "%s", source);
}
int FS_ReadFile(const char *path, void **buffer) {
    ++reads; auto file = files.find(path);
    if (file == files.end()) { if (buffer) *buffer = nullptr; return -1; }
    if (buffer) {
        *buffer = std::malloc(file->second.size() + 1); ++liveFiles;
        std::memcpy(*buffer, file->second.c_str(), file->second.size() + 1);
    }
    return static_cast<int>(file->second.size());
}
void FS_FreeFile(void *buffer) { std::free(buffer); --liveFiles; }
fileHandle_t FS_FOpenFileWrite(const char *name) {
    assert(!std::strcmp(name, "fntech3-hud-dump.json")); ++writes; dump.clear(); return 1;
}
int FS_Write(const void *buffer, int length, fileHandle_t file) {
    assert(file == 1); dump.append(static_cast<const char *>(buffer), length); return length;
}
void QDECL FS_Printf(fileHandle_t file, const char *format, ...) {
    char buffer[2048]; va_list args; va_start(args, format);
    int length = std::vsnprintf(buffer, sizeof(buffer), format, args); va_end(args);
    assert(length >= 0 && length < static_cast<int>(sizeof(buffer))); FS_Write(buffer, length, file);
}
void FS_FCloseFile(fileHandle_t file) { assert(file == 1); }
static void Draw(float x, float y, float w, float h, float s1, float t1, float s2, float t2, qhandle_t shader) {
    ++draws; submitted = {x,y,w,h,s1,t1,s2,t2}; submittedShader = shader;
}
static void Size(int w, int h) {
    cls.glconfig.vidWidth = w; cls.glconfig.vidHeight = h;
    cls.scale = std::min(w / 640.0f, h / 480.0f);
    cls.biasX = (w - 640 * cls.scale) / 2; cls.biasY = (h - 480 * cls.scale) / 2;
}
static void Expect(float x, float y, float w, float h) {
    const float expected[] = {x,y,w,h};
    for (int i = 0; i < 4; ++i) {
        if (std::fabs(submitted[i] - expected[i]) > 0.002f) {
            std::fprintf(stderr, "component %d: got %f expected %f\n", i, submitted[i], expected[i]); std::abort();
        }
    }
    assert(submitted[4] == 0.1f && submitted[5] == 0.2f && submitted[6] == 0.8f && submitted[7] == 0.9f);
    assert(submittedShader == pictureShader);
}
static void Picture(float x=300, float y=90, float w=120, float h=90) {
    CL_HudDrawStretchPic(x,y,w,h,0.1f,0.2f,0.8f,0.9f,pictureShader);
}
static void Rules(const std::string &json) { files["fntech3-hud.json"] = json; reload(); assert(liveFiles == 0); }
int main(int argc, char **argv) {
    assert(argc == 3 || argc == 4); profile = std::atoi(argv[1]); re.DrawStretchPic = Draw;
    cls.charSetShader = 1; cls.whiteShader = 2; cls.consoleShader = 3;
    Size(1920, 1080);
    // Even explicitly set legacy settings cannot opt QL or MoHAA into correction.
    if (Unsupported()) {
        variables["cl_hudAspect"].integer = variables["cl_hudDump"].integer = 1;
        files["fntech3-hud.json"] = "[{\"mode\":\"uniform\"}]";
    }
    CL_HudInit();
    refdef_t view{}; view.x=300; view.y=90; view.width=120; view.height=90;
    view.fov_x=30; view.fov_y=40; view.rdflags=RDF_NOWORLDMODEL;
    const auto original = view;
    CL_HudRegisterShaderName(17,"icons/test");
    CL_HudBeginFrame(); Picture(); CL_HudAdjustRefdef(&view); CL_HudEndFrame();
    if (Unsupported()) {
        Expect(300,90,120,90); assert(!std::memcmp(&original,&view,sizeof(view)));
        CL_HudResetCGame(); CL_HudShutdown();
        assert(reads==0 && writes==0 && registrations==0 && commands==0); return 0;
    }
    assert(registrations == 2 && commands == 1);
    Expect(465,90,90,90);
    assert(view.x==465 && view.y==90 && view.width==90 && view.height==90);
    assert(view.fov_x==30 && view.fov_y==40 && (view.rdflags & RDF_NOFOVCORRECTION));
    view = original; view.rdflags=0; const auto world=view;
    CL_HudAdjustRefdef(&view); assert(!std::memcmp(&view,&world,sizeof(view)));
    view = original; view.x=INT_MAX; CL_HudAdjustRefdef(&view); // bounded conversion
    CL_HudAdjustRefdef(nullptr);
    Picture(0,0,1920,1080); Expect(0,0,1920,1080); // fades/loading screens
    Picture(300,90,-120,-90); Expect(465,90,-90,-90); // flipped geometry
    Size(640,480); Picture(100,40,40,40); Expect(100,40,40,40);
    Size(2560,1080); Picture(400,90,160,90); Expect(785,90,90,90);
    Size(1080,1920); Picture(168.75f,160,67.5f,160); Expect(168.75f,622.5f,67.5f,67.5f);
    Size(1920,1080);
    variables["cl_hudAspect"].integer=0; Picture(); Expect(300,90,120,90);
    variables["cl_hudAspect"].integer=1;
    Rules(R"([{"match":{"shader":"ICONS/*"},"align":{"x":"right","y":"top"}}])");
    Picture(); Expect(705,90,90,90);
    Rules(R"([{"match":{"region":{"x":90,"y":30,"w":60,"h":60}},"mode":"stretch"}])");
    Picture(); Expect(300,90,120,90);
    Rules(R"([{"match":{"textLike":true},"align":{"x":"left"}}])");
    CL_HudRegisterShaderName(17,"font/test");
    Picture(300,90,48,36); Expect(225,90,36,36); // classification in 640x480, not pixels
    CL_HudRegisterShaderName(17,"icons/test");
    Rules(R"([{"match":{"refdef":true},"align":{"x":"right"}}])");
    Picture(); Expect(465,90,90,90); view=original; CL_HudAdjustRefdef(&view); assert(view.x==705);
    // First matching rule wins, and wildcard matching is case-insensitive.
    Rules(R"([{"match":{"shader":"icons/*"},"mode":"stretch"},{"align":{"x":"left"}}])");
    Picture(); Expect(300,90,120,90);
    for (const char *bad : {"", "{", "[}", "[{\"mode\":\"stretch\"}", "[] trailing", "[{},]", "[0x1]", "[01]", "[1.]", "[\"bad\\u12\"]",
        R"([{"mode":"invalid"}])", R"([{"match":{"region":{"x":1,"y":2,"w":-1,"h":4}},"mode":"stretch"}])",
        R"([{"match":{"region":{"x":1e999,"y":2,"w":3,"h":4}},"mode":"stretch"}])",
        R"([{"match":{"region":{"x":0,"y":0,"w":640}},"mode":"stretch"}])",
        R"([{"match":"icons/*","mode":"stretch"}])", R"([{"align":{"x":"invalid"},"mode":"stretch"}])"}) {
        Rules(bad); Picture(); Expect(465,90,90,90);
    }
    Rules(std::string(1000,'[')+std::string(1000,']')); Picture(); Expect(465,90,90,90);
    Rules(std::string(300000,' ')); Picture(); Expect(465,90,90,90);
    Rules(std::string("[]\0[]",5)); Picture(); Expect(465,90,90,90);
    // Read-once fallback, legacy filename, canonical priority, and VM restart.
    files.clear(); files["fnq3-hud.json"] = R"([{"mode":"stretch"}])";
    reload(); Picture(); Expect(300,90,120,90);
    Rules("[]"); Picture(); Expect(465,90,90,90);
    files["fntech3-hud.json"] = R"([{"mode":"stretch"}])";
    CL_HudResetCGame(); Picture(); Expect(300,90,120,90);
    Rules(R"([{"match":{"shader":"icons/*"},"mode":"stretch"}])");
    Picture(); Expect(465,90,90,90); // stale shader handles were discarded
    CL_HudRegisterShaderName(17,"icons/test"); Picture(); Expect(300,90,120,90);
    const int beforeReads = reads; Picture(); assert(reads==beforeReads);
    // Real dump output must be valid JSON, reusable as rules, and deduplicated.
    variables["cl_hudDump"].integer=1;
    CL_HudBeginFrame(); Picture(); view=original; CL_HudAdjustRefdef(&view); CL_HudEndFrame();
    assert(writes==1 && dump.find("\"refdef\": 1") != std::string::npos);
    CL_HudBeginFrame(); Picture(); CL_HudEndFrame();
    assert(dump.find("\"samples\": 2") != std::string::npos);
    std::FILE *output=std::fopen(argv[2],"wb"); assert(output);
    assert(std::fwrite(dump.data(),1,dump.size(),output)==dump.size()); std::fclose(output);
    Rules(dump); Picture();
    variables["cl_hudDump"].integer=0;
    capture.integer=hideHud.integer=1; const int before=draws; Picture(); assert(draws==before);
    capture.integer=0; float x=NAN,y=0,w=1,h=1; assert(!CL_HudAdjustPic(&x,&y,&w,&h,17));
    Size(0,0); Picture(); Expect(300,90,120,90);
    if (argc == 4) {
        // Exercise the shipped rules with stock module and menu draw coordinates.
        std::ifstream preset(argv[3]); assert(preset);
        Rules(std::string(std::istreambuf_iterator<char>(preset), {}));
        Size(2560,1080);
        const auto sample = [](const char *shader, float x, float y, float w, float h, float expectedX,
            qhandle_t handle = 17) {
            pictureShader = handle;
            CL_HudRegisterShaderName(handle, shader);
            Picture(x*4, y*2.25f, w*4, h*2.25f);
            Expect(expectedX, y*2.25f, w*2.25f, h*2.25f);
            pictureShader = 17;
        };
        sample("gfx/2d/crosshaire",296,216,48,48,1226);
        sample("gfx/2d/hudchars",152,120,16,20,902); // centered warmup/message
        if (profile == 0) {
            sample("gfx/2d/numbers/one_32b",540,150,16,24,2335);
            sample("icons/iconh_green",584,160,32,32,2434);
            sample("icons/iconf_red1",584,380,32,32,2434);
            sample("icons/iconw_rocket",300,380,32,32,1235);
            sample("icons/iconh_green",8,384,32,32,18);
            // Q3's cgame registers the client's own charset.
            sample("gfx/2d/bigchars",600,2,16,16,2470,cls.charSetShader);
        } else if (profile == 2) {
            sample("white",10,400,10,70,22.5f);
            sample("font/health",39.5f,456,15,15,88.875f);
            sample("font/armor",188,456,18,15,423);
            sample("icons/iconw_mp40_1",550,425,80,40,2357.5f);
            sample("font/ammo",600,456,16,15,2470);
            sample("icons/holdable",600,310,36,36,2470);
            sample("gfx/2d/hudchars",500,398,10,10,1125); // long pickup line stays together
            sample("ui/hint",296,420,48,48,1226);
        } else if (profile == 4) {
            // F.A.K.K.2's HUD menus as its UI draws them: each widget cut to its
            // menu and the screen, in the menus' 640x480 units.
            sample("textures/hud/health_gradient",1,22,64,72,2.25f);
            sample("textures/hud/water_gradient",63,22,64,72,141.75f);
            sample("textures/hud/health_thingie",0,0,128,128,0);
            sample("textures/hud/ammo_gradient_right",601,50,32,32,2472.25f);
            sample("textures/hud/ammo_gradient_left",456,50,32,32,2146);
            sample("textures/hud/weapon_right",544,0,96,96,2344);
            sample("textures/hud/weapon_left",448,0,96,96,2128);
            sample("textures/hud/2handed",448,0,192,96,2128);
            sample("textures/menu/bosshealth",0,224,128,32,0);
            sample("textures/menu/bosshealth_bar",0,240,64,16,0);
            sample("textures/menu/bosshealth",0,224,62,32,0); // sliding in from the left
            // A hand's model view goes with its menu, keeping its fields of view.
            refdef_t hand{}; hand.x=468*4; hand.y=45; hand.width=56*4; hand.height=126;
            hand.fov_x=hand.fov_y=30; hand.rdflags=RDF_NOWORLDMODEL;
            CL_HudAdjustRefdef(&hand);
            assert(hand.x==2173 && hand.y==45 && hand.width==126 && hand.height==126);
            assert(hand.fov_x==30 && hand.fov_y==30);
        } else if (profile == 8) {
            // Alice's HUD widgets: each a view of the whole screen shifted by
            // its offset from the centre, which the rules make a 4:3 screen's.
            const auto widget = [](float offsetX, float offsetY, int expectedX, int expectedY) {
                refdef_t v{}; v.x=int(2560/640.0f*offsetX); v.y=int(1080/480.0f*offsetY);
                v.width=2560; v.height=1080; v.fov_x=v.fov_y=30; v.rdflags=RDF_NOWORLDMODEL;
                CL_HudAdjustRefdef(&v);
                if (v.x!=expectedX || v.y!=expectedY || v.width!=1440 || v.height!=1080) {
                    std::fprintf(stderr, "widget %f: %d %d %d %d\n", offsetX, v.x, v.y, v.width, v.height); std::abort();
                }
                assert(v.fov_x==30 && v.fov_y==30);
            };
            widget(-312,-25,-702,-56); widget(-312,42,-702,94); // health back, riser and bar
            widget(312,-25,1822,-56); widget(312,42,1822,94);   // mana
            widget(-275,182,-619,409); widget(275,182,1739,409); // weapon and powerup cards
        } else if (profile == 5) {
            // Enemy Territory's stock HUD (cg_draw.c's rectangles).
            sample("gfx/2d/compass.tga",508,8,124,124,2263);
            sample("levelshots/oasis_cc_automap",520,20,100,100,2290);
            sample("ui/assets/mp_arrow_blue.tga",564,64,12,12,2389);
            sample("white",585,152,52,14,2436.25f);            // timer box
            sample("sprintbar",4,388,32,72,9);
            sample("gfx/hud/ic_health",24,464,12,12,54);
            sample("gfx/limbo/ic_engineer",112,444,14,14,252);
            sample("icons/iconw_thompson_1_select",558,424,60,32,2375.5f);
            sample("gfx/hud/ic_power",626,464,8,14,2528.5f);
            sample("lagometer",592,280,48,48,2452);
            sample("AxisHelmet",4,360,48,24,9);
            sample("white",10,11,204,16,22.5f);                // fireteam overlay
            sample("white",50,208,10,64,112.5f);               // weapon stability
            // Popups and the big popup keep their text beside their icons.
            sample("sprites/voiceChat",4,360,20,20,569);
            sample("gfx/limbo/cm_flagallied",584,270,48,48,1874);
            // A scope's side masks reach the screen's edges beside its centred reticle.
            CL_HudRegisterShaderName(17,"white");
            Picture(0,0,80*4,1080); Expect(0,0,740,1080);
            Picture(560*4,0,80*4,1080); Expect(1820,0,740,1080);
            // Its own screens: the rules pause, or it is drawn as retail drew it.
            CL_HudSetGamePanel(HUD_PANEL_CENTERED);
            CL_HudRegisterShaderName(17,"gfx/2d/compass.tga");
            Picture(508*4,8*2.25f,124*4,124*2.25f); Expect(1703,18,279,279);
            CL_HudSetGamePanel(HUD_PANEL_STRETCHED);
            Picture(508*4,8*2.25f,124*4,124*2.25f); Expect(508*4,18,124*4,279);
            refdef_t head{}; head.x=44*4; head.y=873; head.width=62*4; head.height=180;
            head.fov_x=8; head.fov_y=10; head.rdflags=RDF_NOWORLDMODEL;
            const refdef_t retailHead = head; CL_HudAdjustRefdef(&head);
            assert(head.x==retailHead.x && head.width==retailHead.width && (head.rdflags & RDF_NOFOVCORRECTION));
            CL_HudSetGamePanel(HUD_PANEL_NONE);
            Picture(508*4,8*2.25f,124*4,124*2.25f); Expect(2263,18,279,279);
            // Its world view is its own: no correction moves it.
            refdef_t world{}; world.x=287*4; world.y=859; world.width=128*4; world.height=216;
            world.fov_x=60; world.fov_y=30; const refdef_t retailWorld = world;
            CL_HudAdjustRefdef(&world); assert(!std::memcmp(&world,&retailWorld,sizeof(world)));
        } else if (profile == 6 || profile == 7) {
            // Elite Force's LCARS gauges, keyed to their own art: the sliders'
            // art is also the talk box's and the zoom ticks', which stay centred.
            sample("gfx/interface/healthcap1",5,429,32,64,11.25f);
            sample("gfx/interface/ammobar",72,429,73,41,162);
            sample("gfx/interface/armorcap1",20,458,32,16,45);
            sample("gfx/2d/numbers/one",25,425,16,32,56.25f);
            sample("gfx/2d/numbers/t_one",46,458,16,16,103.5f);
            sample("gfx/interface/ammouppercap1",613,429,32,64,2499.25f);
            sample("gfx/interface/ammobar",578,458,33,12,2420.5f);
            sample("gfx/2d/numbers/three",575,425,16,32,2413.75f);
            sample("gfx/interface/ammobar",75,71,70,338,728.75f);  // zoom ticks
            sample("gfx/2d/numbers/one",468,100,16,32,1613);       // zoom readout
            if (profile == 7) {
                sample("gfx/2d/select",621,389,19,24,2517.25f);   // score box
            } else {
                // Single player is told its screen is the 4:3 frame, so its view
                // and the points it projects are 4:3 ones; its pixels are mapped.
                int frameW = 2560, frameH = 1080;
                CL_HudModuleScreen(&frameW, &frameH); assert(frameW==1440 && frameH==1080);
                CL_HudRegisterShaderName(17,"gfx/interface/healthcap1");
                Picture(5*2.25f,429*2.25f,32*2.25f,64*2.25f); Expect(11.25f,965.25f,72,144);
                CL_HudRegisterShaderName(17,"white");
                Picture(0,0,1440,1080); Expect(0,0,2560,1080);         // fades stretch
                Picture(0,0,1440,48*2.25f); Expect(0,0,2560,108);      // cutscene letterbox
                Picture(0,432*2.25f,1440,48*2.25f); Expect(0,972,2560,108);
                refdef_t world{}; world.width=1440; world.height=1080; world.fov_x=80; world.fov_y=64;
                CL_HudAdjustRefdef(&world);
                assert(world.x==0 && world.width==2560 && world.height==1080 && !(world.rdflags & RDF_NOFOVCORRECTION));
                world = refdef_t{}; world.x=360; world.y=270; world.width=720; world.height=540;
                world.fov_x=80; world.fov_y=64; CL_HudAdjustRefdef(&world); // cg_viewsize 50
                assert(world.x==920 && world.y==270 && world.width==720 && world.height==540);
                assert(world.rdflags & RDF_NOFOVCORRECTION);
                refdef_t talker{}; talker.x=int(-6*2.25f); talker.y=int(25*2.25f); talker.width=162; talker.height=162;
                talker.fov_x=talker.fov_y=30; talker.rdflags=RDF_NOWORLDMODEL;
                CL_HudAdjustRefdef(&talker); assert(talker.x==547 && talker.width==162); // stays with its talk box
                variables["cl_hudAspect"].integer=0;                   // retail's stretch of its frame
                CL_HudRegisterShaderName(17,"gfx/interface/healthcap1");
                Picture(5*2.25f,429*2.25f,32*2.25f,64*2.25f); Expect(20,965.25f,128,144);
                variables["cl_hudAspect"].integer=1;
                CL_HudResetCGame(); frameW = 2560; frameH = 1080;
                variables["cl_hudAspect"].integer=0; CL_HudModuleScreen(&frameW, &frameH);
                assert(frameW==2560 && frameH==1080); variables["cl_hudAspect"].integer=1;
            }
        } else if (profile == 3) {
            sample("ui_mp/assets/hudsprint.tga",5,390,16,80,11.25f);
            sample("white",5,396,16,65.6f,11.25f);
            sample("gfx/2d/numbers/one_32b",30,445,16,25,67.5f);
            sample("AxisHelmet",122,443,64,32,274.5f);
            sample("AlliedHelmet",122,443,64,32,274.5f);
            sample("sprites/shield",36,401,36,36,81);
            sample("gfx/2d/numbers/three_32b",534,445,16,25,2321.5f);
            sample("icons/iconw_mp40_1",525,400,80,40,2301.25f);
            sample("ui_mp/assets/hudtrim.tga",619,390,16,80,2512.75f);
            sample("white",619,396,16,67.2f,2512.75f);
            sample("gfx/2d/compass.tga",290,420,60,60,1212.5f);
            sample("gfx/2d/compass2.tga",290,420,60,60,1212.5f);
            sample("sprites/medic",310,440,12,12,1257.5f);
            sample("gfx/2d/hudchars",5,48,8,16,11.25f);
            // The client's charset is the cgame's gfx/2d/hudchars too, one
            // handle under two names. round_timer names the cgame's.
            sample("gfx/2d/hudchars",5,48,8,16,11.25f,cls.charSetShader);
            sample("ui_mp/assets/ger_flag.tga",5,68,24,14,11.25f);
            sample("white",4,67,26,1,9);
            sample("lagometer",585,340,48,48,2436.25f);
            sample("white",620,350,1,10,2515);
            sample("gfx/2d/hudchars",534,445,8,16,1761.5f); // scoreboard, not ammo
            sample("gfx/2d/hudchars",619,450,8,16,1952.75f); // not a charge bar
        }
        CL_HudRegisterShaderName(17,"white");
        Picture(0,0,2560,1080); Expect(0,0,2560,1080);
        // Bottom anchors also apply on portrait displays; crosshair stays centered.
        Size(1080,1920);
        if (profile == 2 || profile == 3) {
            CL_HudRegisterShaderName(17,profile==2 ? "font/health" : "gfx/2d/numbers/one_32b");
            Picture(30*1.6875f,445*4,16*1.6875f,25*4);
            Expect(50.625f,1860.9375f,27,42.1875f);
        }
        CL_HudRegisterShaderName(17,"gfx/2d/crosshaire");
        Picture(296*1.6875f,216*4,48*1.6875f,48*4); Expect(499.5f,919.5f,81,81);
        Size(640,480); Picture(296,216,48,48); Expect(296,216,48,48);
        if (profile != 2) {
            // A rule may still name the shared handle by the client's alias,
            // and a dump keeps reporting the alias, not the module's name.
            const char *moduleCharset = profile == 0 ? "gfx/2d/bigchars" : "gfx/2d/hudchars";
            Size(2560,1080); pictureShader = cls.charSetShader;
            CL_HudRegisterShaderName(cls.charSetShader, moduleCharset);
            Rules(R"([{"match":{"shader":"charset"},"align":{"x":"left","y":"top"}}])");
            Picture(300*4,200*2.25f,8*4,16*2.25f); Expect(675,450,18,36);
            Rules(R"([{"match":{"shader":"GFX/2D/*"},"align":{"x":"right","y":"top"}}])");
            Picture(300*4,200*2.25f,8*4,16*2.25f); Expect(1795,450,18,36);
            variables["cl_hudDump"].integer=1; CL_HudResetCGame();
            CL_HudRegisterShaderName(cls.charSetShader, moduleCharset);
            CL_HudBeginFrame(); Picture(300*4,200*2.25f,8*4,16*2.25f); CL_HudEndFrame();
            assert(dump.find("\"shader\": \"charset\"") != std::string::npos);
            assert(dump.find(moduleCharset) == std::string::npos);
            variables["cl_hudDump"].integer=0; pictureShader = 17;
        }
    }
    CL_HudShutdown(); CL_HudShutdown(); assert(commands==0 && liveFiles==0);
}
'''


class HudAspectTests(unittest.TestCase):
    def test_layout_rules_dump_and_profile_isolation(self):
        import json
        compiler = shutil.which("clang++") or shutil.which("g++")
        if not compiler:
            self.skipTest("C++17 compiler unavailable")
        scratch = ROOT / ".tmp"
        scratch.mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(prefix="hud-aspect-", dir=scratch) as directory:
            path = Path(directory)
            fixture = path / "hud.cpp"
            fixture.write_text(HARNESS, encoding="utf-8")
            executable = path / ("hud.exe" if os.name == "nt" else "hud")
            target = compiler_target_args(compiler)
            if os.name == "nt" and not target:
                target = ["-DFNTECH3_Q3_ONLY"]
            elif os.name != "nt":
                target = ["-m32"]
            build = subprocess.run([compiler, *target, "-std=c++17", "-I", str(ROOT),
                str(fixture), "-o", str(executable)], cwd=ROOT, capture_output=True, text=True, timeout=60)
            self.assertEqual(build.returncode, 0, build.stdout + build.stderr)
            presets = {
                0: "quake3/baseq3/fnq3-hud.json",
                2: "rtcw-sp/main/fntech3-hud.json",
                3: "rtcw-mp/main/fntech3-hud.json",
                4: "fakk2/fakk/fntech3-hud.json",
                5: "wolfet/etmain/fntech3-hud.json",
                6: "stvef-sp/baseEF/fntech3-hud.json",
                7: "stvef-mp/baseEF/fntech3-hud.json",
                8: "alice/base/fntech3-hud.json",
            }
            for profile in range(10):
                with self.subTest(profile=profile):
                    output = path / f"dump-{profile}.json"
                    command = [str(executable), str(profile), str(output)]
                    if profile in presets:
                        command.append(str(ROOT / "pkg" / "games" / presets[profile]))
                    result = subprocess.run(command,
                        cwd=ROOT, capture_output=True, text=True, timeout=15)
                    self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                    if profile not in (1, 9):
                        rules = json.loads(output.read_text(encoding="utf-8"))["rules"]
                        self.assertTrue(any(rule["match"].get("refdef") for rule in rules))
                        self.assertEqual(len(rules), 2)
                    else:
                        self.assertFalse(output.exists())


if __name__ == "__main__":
    unittest.main()
