/* Independently authored engine-contract observation fixture. GPL-2.0-or-later.
 * Extends the import-597 fixture from .tmp/jka-server-boneanim: server imports
 * 595/596 set the same clips and bends through canonical and other-case names;
 * 597 and 587 observe the answers and poses. Import 593 observes bone/surface
 * matching and repeated bolt indices. No retail gameplay implementation.
 * Read-only retail clock addresses and PE identity are pinned in
 * docs/fntech3/2026-10-07-jka-server-bone-anim.json.
 * Build/run instructions: docs/fntech3/2026-10-07-jka-bone-names.md. */
#include "code/compat/jka_module_abi.h"
#include "code/compat/jka_game_abi.h"
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

static intptr_t (__cdecl *Call)(intptr_t, ...);
static fntech3_jka_sharedEntity_t entity[1];
static fntech3_jka_playerState_t player[1];

static const char Kyle[] = "models/players/kyle/model.glm";
static const char Pistol[] = "models/weapons2/blaster_pistol/blaster_pistol_w.glm";
static const char DefaultBone[] = "ModView internal default";
enum { Bodies = 2 };
static void *body[Bodies], *pistol;
static int frames;
static bool retail;

static bool RetailJamp() {
	const auto base = reinterpret_cast<std::uintptr_t>(GetModuleHandleA(nullptr));
	if (base != 0x400000u) return false;
	const auto *dos = reinterpret_cast<const IMAGE_DOS_HEADER *>(base);
	if (dos->e_magic != IMAGE_DOS_SIGNATURE) return false;
	const auto *pe = reinterpret_cast<const IMAGE_NT_HEADERS32 *>(base + std::uintptr_t(dos->e_lfanew));
	return pe->Signature == IMAGE_NT_SIGNATURE && pe->FileHeader.Machine == IMAGE_FILE_MACHINE_I386
		&& pe->FileHeader.TimeDateStamp == 0x3faa8a05u && pe->OptionalHeader.SizeOfImage == 0xf27000u
		&& pe->OptionalHeader.CheckSum == 0x18273cu && pe->OptionalHeader.ImageBase == 0x400000u;
}
static int RetailClock(int which) {
	if (!retail) return -1;
	return *reinterpret_cast<const volatile int *>(std::uintptr_t(which ? 0x10bcb78u : 0x10bcb74u));
}

static int Word(float v) { int x; std::memcpy(&x, &v, 4); return x; }
static void Print(const char *text) { Call(FNTECH3_JKA_G_PRINT, text); }
static void Report(const char *what, intptr_t value) {
	char text[160]; std::snprintf(text, sizeof(text), "JKA_BONEANIM %s=%d\n", what, int(value)); Print(text);
}

static void Play(void *owner, const char *label, int model, const char *bone, int first, int end, int flags,
	float speed, int now, float seed = -1, int blend = 0) {
	const int result = int(Call(FNTECH3_JKA_G_G2_PLAYANIM, owner, model, bone, first, end, flags,
		Word(speed), now, Word(seed), blend));
	char text[320]; std::snprintf(text, sizeof(text),
		"JKA_BONEANIM play owner=%s model=%d bone=%s first=%d end=%d flags=%x speed=%08x now=%d seed=%08x "
		"blend=%d result=%d\n", label, model, bone, first, end, flags, unsigned(Word(speed)), now,
		unsigned(Word(seed)), blend, result);
	Print(text);
}
static void Bend(void *owner, const char *label, const char *bone, int flags, int now) {
	const float angles[3]{25, 20, 30};
	const int result = int(Call(FNTECH3_JKA_G_G2_ANGLEOVERRIDE, owner, 0, bone, angles, flags, 1, 3, 2,
		nullptr, 0, now));
	char text[160]; std::snprintf(text, sizeof(text), "JKA_BONEANIM bend owner=%s bone=%s flags=%x result=%d\n",
		label, bone, flags, result);
	Print(text);
}

struct Answer { int result; std::uint32_t frame, start, end, flags, speed, list; };

/* Every output starts as its own sentinel, so a word the engine leaves alone
 * is told from any value it writes. The model-list word points at a canary
 * that retail converts and never reads. */
static Answer Get(const char *phase, const char *owner, void *ghoul2, int model, const char *bone, int word) {
	std::uint32_t out[5]{0x11111111u, 0x22222222u, 0x33333333u, 0x44444444u, 0x55555555u};
	int list[2]{0x66666666, 0x66666666};
	Answer a{};
	a.result = int(Call(FNTECH3_JKA_G_G2_GETBONEANIM, ghoul2, bone, word, &out[0], &out[1], &out[2],
		&out[3], &out[4], list, model));
	a.frame = out[0]; a.start = out[1]; a.end = out[2]; a.flags = out[3]; a.speed = out[4];
	a.list = std::uint32_t(list[0]) ^ std::uint32_t(list[1]) ^ 0x66666666u;
	char text[400]; std::snprintf(text, sizeof(text),
		"JKA_BONEANIM get phase=%s owner=%s model=%d bone=%s word=%d clock0=%d clock1=%d result=%d "
		"frame=%08x start=%08x end=%08x flags=%08x speed=%08x list=%08x\n",
		phase, owner, model, *bone ? bone : "<empty>", word, RetailClock(0), RetailClock(1), a.result,
		unsigned(a.frame), unsigned(a.start), unsigned(a.end), unsigned(a.flags), unsigned(a.speed),
		unsigned(a.list));
	Print(text);
	return a;
}

static void Bolt(const char *phase, const char *owner, void *ghoul2, int bolt, int now) {
	float matrix[12]; for (float &v : matrix) v = -777.0f;
	const float angles[3]{0, 0, 0}, position[3]{0, 0, 0}, scale[3]{1, 1, 1};
	const int result = int(Call(FNTECH3_JKA_G_G2_GETBOLT, ghoul2, 0, bolt, matrix, angles, position, now,
		nullptr, scale));
	char text[400];
	int used = std::snprintf(text, sizeof(text), "JKA_BONEANIM bolt phase=%s owner=%s bolt=%d result=%d bits=",
		phase, owner, bolt, result);
	for (int i = 0; i < 12; i++) used += std::snprintf(text + used, sizeof(text) - std::size_t(used),
		"%s%08x", i ? "," : "", unsigned(Word(matrix[i])));
	std::snprintf(text + used, sizeof(text) - std::size_t(used), "\n");
	Print(text);
}


static const char *Label(int b) { return b ? "folded" : "canonical"; }
static int Add(void *owner, const char *label, const char *name) {
    const int result = int(Call(FNTECH3_JKA_G_G2_ADDBOLT, owner, 0, name));
    char text[200]; std::snprintf(text, sizeof(text),
        "JKA_BONEANIM add owner=%s name=%s result=%d\n", label, *name ? name : "<empty>", result);
    Print(text); return result;
}
static void Observe(const char *phase, int now) {
    for (int b=0; b<Bodies; ++b) {
        for (const char *name : {"model_root", "MODEL_ROOT", "Model_Root", "pelvis", "PELVIS", "PeLvIs", "thoracic"})
            Get(phase, Label(b), body[b], 0, name, now);
        for (int bolt=0; bolt<3; ++bolt) Bolt(phase, Label(b), body[b], bolt, now);
    }
    Get(phase, "pistol", pistol, 0, DefaultBone, now);
    Bolt(phase, "pistol", pistol, 0, now);
}
static void Init(int now) {
    Call(FNTECH3_JKA_G_LOCATE_GAME_DATA, entity, 1, int(sizeof(entity[0])), player, int(sizeof(player[0])));
    retail = RetailJamp(); Report("retail_jamp", retail);
    for (int b=0; b<Bodies; ++b) {
        if (Call(FNTECH3_JKA_G_G2_INITGHOUL2MODEL, &body[b], Kyle, 0, 0, 0, 0, 0)<0 || !body[b]) {
            Print("JKA_BONEANIM FAILED_INIT\n"); return;
        }
        // Both first insertion and duplicate lookup, on surfaces and bones.
        Add(body[b],Label(b), b ? "*L_HAND" : "*l_hand");
        Add(body[b],Label(b), b ? "PeLvIs" : "pelvis");
        Add(body[b],Label(b), b ? "MODEL_ROOT" : "model_root");
        for (const char *name : {"*l_hand", "*L_HAND", "*L_hAnD", "pelvis", "PELVIS", "PeLvIs", "model_root", "Model_Root",
                                "", "NO_SUCH_BONE", "PELV", "PELVIS_extra", "*L_HAN", "*L_HAND_extra"})
            Add(body[b],Label(b),name);
        Play(body[b],Label(b),0,b ? "MODEL_ROOT" : "model_root",10,20,0x58,1,now);
        Bend(body[b],Label(b),b ? "PELVIS" : "pelvis",2,now);
        Play(body[b],Label(b),0,b ? "PeLvIs" : "pelvis",60,80,0x58,.5f,now);
        Bend(body[b],Label(b),b ? "ThOrAcIc" : "thoracic",2,now);
        for (const char *name : {"", "NO_SUCH_BONE", "PELV", "PELVIS_extra"}) {
            Play(body[b],Label(b),0,name,100,120,0x48,1,now);
            Bend(body[b],Label(b),name,4,now);
        }
    }
    if (Call(FNTECH3_JKA_G_G2_INITGHOUL2MODEL, &pistol, Pistol, 0, 0, 0, 0, 0)<0 || !pistol) {
        Print("JKA_BONEANIM FAILED_INIT\n"); return;
    }
    Add(pistol,"pistol","MODVIEW INTERNAL DEFAULT");
    Add(pistol,"pistol",DefaultBone);
    Play(pistol,"pistol",0,"MODVIEW INTERNAL DEFAULT",0,1,0x48,1,now);
    Bend(pistol,"pistol","modview internal default",2,now);
    Observe("init",now);
}
static void Frame(int now) {
    ++frames;
    char phase[16]; std::snprintf(phase,sizeof(phase),"f%02d",frames);
    if (frames==3 || frames==7) for (int b=0; b<Bodies; ++b) {
        // Replacement must find the earlier differently cased clip for blending.
        Play(body[b],Label(b),0,b ? (frames==3 ? "Model_Root" : "MODEL_ROOT") : "model_root",
             frames==3 ? 100 : 200,frames==3 ? 120 : 220,0xc8,1,now,-1,300);
        Bend(body[b],Label(b),b ? "pElViS" : "pelvis",frames==3 ? 4 : 2,now);
    }
    Observe(phase,now);
    if (frames==12) {
        bool clean=true;
        for (void *&owner : body) { Call(FNTECH3_JKA_G_G2_CLEANMODELS,&owner); clean=clean&&!owner; }
        Call(FNTECH3_JKA_G_G2_CLEANMODELS,&pistol); clean=clean&&!pistol;
        Report("cleaned",clean); Print("JKA_BONEANIM COMPLETE\n");
        Call(FNTECH3_JKA_G_SEND_CONSOLE_COMMAND,2,"quit\n");
    }
}
extern "C" __declspec(dllexport) void __cdecl dllEntry(intptr_t (__cdecl *host)(intptr_t,...)) { Call=host; }
extern "C" __declspec(dllexport) intptr_t __cdecl vmMain(int operation,int a0,int,int,int,int,int,int,int,int,int,int,int) {
    if (operation==FNTECH3_JKA_GAME_INIT) Init(a0);
    if (operation==FNTECH3_JKA_GAME_RUN_FRAME && body[1] && pistol) Frame(a0);
    return 0;
}
