/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#include "../code/client/client_jka_ui.hpp"
#include "../code/compat/jka_ui_exports.h"
#include <cstdio>
#include <limits>

#define CHECK(test) do { if (!(test)) { std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #test); return __LINE__; } } while (false)
namespace ui = fntech3::jka::ui;
using Call = fntech3::jka::module::Call;

static_assert(sizeof(fntech3_jka_mp_uiClientState_t) == 3084);
static_assert(offsetof(fntech3_jka_mp_uiClientState_t, servername) == 12);
static_assert(offsetof(fntech3_jka_mp_uiClientState_t, updateInfoString) == 1036);
static_assert(offsetof(fntech3_jka_mp_uiClientState_t, messageString) == 2060);
static_assert(sizeof(fntech3_jka_mp_pc_token_t) == 1040);
static_assert(offsetof(fntech3_jka_mp_pc_token_t, string) == 16);
static_assert(sizeof(fntech3_jka_mp_qtime_t) == 36);
static_assert(sizeof(fntech3_jka_vmCvar_t) == 272);
static_assert(sizeof(fntech3_jka_mp_glconfig_t) == 76);
static_assert(offsetof(fntech3_jka_mp_glconfig_t, maxTextureSize) == 16);
static_assert(offsetof(fntech3_jka_mp_glconfig_t, colorBits) == 28);
static_assert(offsetof(fntech3_jka_mp_glconfig_t, vidWidth) == 56);
static_assert(offsetof(fntech3_jka_mp_glconfig_t, vidHeight) == 60);
static_assert(offsetof(fntech3_jka_mp_glconfig_t, stereoEnabled) == 72);

int Mapping() {
    CHECK(ui::VersionSupported(7, false));
    CHECK(!ui::VersionSupported(3, false) && !ui::VersionSupported(7, true));
    CHECK(FNTECH3_JKA_MP_UI_API_VERSION == 3); // separate modern API stays named
    CHECK(ui::SharedImport(20) == UI_R_REGISTERSHADERNOMIP);
    CHECK(ui::SharedImport(21) == -1); // shader-name output needs its own bounds
    CHECK(ui::SharedImport(22) == UI_R_CLEARSCENE && ui::SharedImport(27) == UI_R_SETCOLOR);
    CHECK(ui::SharedImport(23) == UI_R_ADDREFENTITYTOSCENE && ui::SharedImport(26) == UI_R_RENDERSCENE);
    CHECK(ui::SharedImport(FNTECH3_JKA_UI_FLOOR) == UI_FLOOR);
    CHECK(ui::SharedImport(FNTECH3_JKA_UI_CEIL) == UI_CEIL);
    CHECK(ui::SharedImport(FNTECH3_JKA_UI_R_REGISTERFONT) == -1);
    CHECK(ui::SharedImport(FNTECH3_JKA_UI_GETGLCONFIG) == -1);
    CHECK(ui::SharedImport(FNTECH3_JKA_UI_GETCLIENTSTATE) == -1);
    CHECK(ui::SharedImport(FNTECH3_JKA_UI_CVAR_REGISTER) == -1);
    CHECK(ui::SharedImport(FNTECH3_JKA_UI_S_REGISTERSOUND) == -1); // different argument count
    for (const auto number : {-1, 99, 199, 1000}) CHECK(ui::SharedImport(number) == -1);
    CHECK(ui::SharedImport(std::numeric_limits<intptr_t>::max()) == -1);
    CHECK(ui::ExportOrdinal(UI_KEY_EVENT) == FNTECH3_JKA_UI_KEY_EVENT);
    CHECK(ui::ExportOrdinal(UI_DRAW_CONNECT_SCREEN) == FNTECH3_JKA_UI_DRAW_CONNECT_SCREEN);
    CHECK(ui::ExportOrdinal(UI_REFRESH_DISPLAY_CONTEXT) == -1); // ordinal 11 is JKA MENU_RESET
    const int arguments[]{0, 1, 0, 2, 2, 1, 0, 1, 1, 1, 0};
    for (int operation = 0; operation <= UI_HASUNIQUECDKEY; ++operation) {
        CHECK(FNTech3_JKAUIExportOrdinal(operation) == ui::ExportOrdinal(operation));
        CHECK(FNTech3_JKAUIExportArgumentCount(operation) == arguments[operation]);
    }
    const int refused[]{-1, UI_REFRESH_DISPLAY_CONTEXT, UI_GET_ACTIVE_MENU, 999};
    for (const auto operation : refused) {
        CHECK(FNTech3_JKAUIExportOrdinal(operation) == -1);
        CHECK(FNTech3_JKAUIExportArgumentCount(operation) == -1);
    }
    for (int menu = 0; menu < UIMENU_COUNT; ++menu)
        CHECK(FNTech3_JKAUIMenuToExternal(menu) == ui::MenuToExternal(menu));
    CHECK(FNTech3_JKAUIMenuToExternal(-1) == -1);
    // The engine's own vocabulary names menus 0 to 5, each exactly once.
    int named[6]{};
    for (int menu = 0; menu < UIMENU_COUNT; ++menu) {
        const int external = FNTech3_JKAUIMenuToExternal(menu);
        CHECK(external >= -1 && external < 6);
        if (external >= 0) ++named[external];
    }
    for (const int count : named) CHECK(count == 1);
    CHECK(FNTech3_JKAUIMenuToExternal(UIMENU_JKA_PLAYERCONFIG) == 3);
    CHECK(UIMENU_JKA_PLAYERCONFIG != UIMENU_NEED_CD);
    CHECK(ui::MenuToExternal(UIMENU_JKA_PLAYERCONFIG) == 3);
    // A client game's number is the module's own (retail jamp.exe 0x413032):
    // export 7 with that one word, never the engine request translated above.
    static_assert(FNTECH3_JKA_UI_SET_ACTIVE_MENU == 7);
    static_assert(ui::ExportOrdinal(UI_SET_MODULE_MENU) == FNTECH3_JKA_UI_SET_ACTIVE_MENU);
    CHECK(FNTech3_JKAUIExportOrdinal(UI_SET_MODULE_MENU) == FNTECH3_JKA_UI_SET_ACTIVE_MENU);
    CHECK(FNTech3_JKAUIExportArgumentCount(UI_SET_MODULE_MENU) == 1);
    CHECK(FNTech3_JKAUIForwardedArguments(UI_SET_MODULE_MENU, 1) == 1);
    for (const int count : {0, 2, 3}) CHECK(FNTech3_JKAUIForwardedArguments(UI_SET_MODULE_MENU, count) == -1);
    const int moduleMenu = UI_SET_MODULE_MENU;
    for (const int request : {int(UI_SET_ACTIVE_MENU), int(UI_GET_ACTIVE_MENU), int(UI_CHECKEXECKEY),
                              int(UI_WANTSBINDKEYS), int(UI_HASUNIQUECDKEY), int(UI_REFRESH_DISPLAY_CONTEXT)})
        CHECK(moduleMenu != request);
    CHECK(ui::MenuToExternal(UIMENU_MAIN) == 1 && ui::MenuToExternal(UIMENU_TEAM) == 4);
    CHECK(ui::MenuToExternal(UIMENU_POSTGAME) == 5 && ui::MenuToExternal(UIMENU_NEED_CD) == -1);
    CHECK(ui::BrowserSource(0) == AS_LOCAL && ui::BrowserSource(1) == AS_GLOBAL && ui::BrowserSource(2) == AS_FAVORITES);
    CHECK(ui::BrowserSource(3) == -1 && ui::BrowserSource(-1) == -1);
    const intptr_t args[]{FNTECH3_JKA_UI_LAN_COMPARESERVERS, 2, 1, 2, 3, 4};
    std::array<intptr_t, 6> translated{};
    CHECK(ui::BrowserArguments(Call::UI_LAN_COMPARESERVERS, args, translated));
    CHECK(translated[1] == AS_FAVORITES && translated[2] == 1 && translated[5] == 4);
    CHECK(!ui::BrowserArguments(Call::UI_LAN_GETPINGINFO, args, translated)); // ping slots are not sources
    CHECK(!ui::BrowserArguments(Call::UI_LAN_COMPARESERVERS, nullptr, translated));
    CHECK(ui::CvarFlags(0xffffffffu) == 0x7ff && ui::CvarFlags(0x1800) == 0);
    CHECK(!std::strcmp(ui::OpenBoundary(Call::UI_GETGLCONFIG), "renderer record"));
    CHECK(!std::strcmp(ui::OpenBoundary(Call::UI_R_REGISTERFONT), "Raven font"));
    CHECK(!std::strcmp(ui::OpenBoundary(Call::UI_G2_GETBOLT), "Ghoul2"));
    return 0;
}

int Records() {
    uiClientState_t state{};
    state.connState = CA_ACTIVE; state.connectPacketCount = 3; state.clientNum = 31;
    std::memset(state.servername, 's', sizeof(state.servername));
    std::memcpy(state.updateInfoString, "\x80\xff", 3);
    struct Guarded { unsigned before; fntech3_jka_mp_uiClientState_t state; unsigned after; } out{};
    std::memset(&out, 0xa5, sizeof(out));
    CHECK(ui::ClientState(state, out.state));
    CHECK(out.before == 0xa5a5a5a5u && out.after == 0xa5a5a5a5u);
    CHECK(out.state.connState == 8 && out.state.clientNum == 31 && out.state.connectPacketCount == 3);
    CHECK(out.state.servername[1022] == 's' && out.state.servername[1023] == 0);
    CHECK(static_cast<unsigned char>(out.state.updateInfoString[0]) == 0x80);
    const auto expected = out;
    const unsigned invalid = 0xffffffffu;
    std::memcpy(&state.connState, &invalid, 4);
    CHECK(!ui::ClientState(state, out.state));
    CHECK(!std::memcmp(&expected, &out, sizeof(out)));
    vmCvar_t cvar{}; cvar.handle = 7; cvar.modificationCount = 8; cvar.value = 4.5f; cvar.integer = 4; cvar.flags = 123;
    std::memset(cvar.string, 'c', sizeof(cvar.string));
    fntech3_jka_vmCvar_t external{}; ui::CvarToExternal(cvar, external);
    CHECK(external.handle == 7 && external.modificationCount == 8 && external.value == 4.5f && external.integer == 4);
    CHECK(external.string[254] == 'c' && external.string[255] == 0);
    vmCvar_t internal{}; ui::CvarToInternal(external, internal);
    CHECK(internal.flags == 0 && internal.handle == 7 && internal.value == 4.5f);
    pc_token_t token{}; token.type = 1; token.subtype = 2; token.intvalue = -3; token.floatvalue = .125f;
    std::memset(token.string, 't', sizeof(token.string));
    fntech3_jka_mp_pc_token_t raw{}; ui::Token(token, raw);
    CHECK(raw.type == 1 && raw.subtype == 2 && raw.intvalue == -3 && raw.floatvalue == .125f);
    CHECK(raw.string[1022] == 't' && raw.string[1023] == 0);
    qtime_t time{1, 2, 3, 4, 5, 6, 7, 8, 9}; fntech3_jka_mp_qtime_t clock{};
    ui::Time(time, clock); CHECK(clock.tm_sec == 1 && clock.tm_year == 6 && clock.tm_isdst == 9);
    return 0;
}
int RendererRecord() {
    CHECK(ui::TextureCompression(TC_NONE) == 0);
    CHECK(ui::TextureCompression(TC_S3TC) == 1);
    CHECK(ui::TextureCompression(TC_S3TC_ARB) == 3);
    CHECK(ui::TextureCompression(-1) == -1 && ui::TextureCompression(3) == -1);
    glconfig_t config{};
    const char renderer[]{"fnTech3 renderer"};
    std::memcpy(config.renderer_string, renderer, sizeof(renderer));
    config.maxTextureSize = 8192; config.numTextureUnits = 8;
    config.colorBits = 24; config.depthBits = 24; config.stencilBits = 8;
    config.textureCompression = TC_S3TC_ARB; config.deviceSupportsGamma = qtrue;
    config.textureEnvAddAvailable = qtrue; config.vidWidth = 1600; config.vidHeight = 900;
    config.displayFrequency = 144; config.stereoEnabled = qfalse; config.isFullscreen = qfalse;
    struct Guarded { unsigned before; fntech3_jka_mp_glconfig_t config; unsigned after; } out;
    std::memset(&out, 0xa5, sizeof(out));
    const bool pointersFit = reinterpret_cast<std::uintptr_t>(config.extensions_string) <= 0xffffffffu;
    CHECK(ui::Glconfig(config, 16, true, out.config) == pointersFit);
    CHECK(out.before == 0xa5a5a5a5u && out.after == 0xa5a5a5a5u);
    if (pointersFit) {
        CHECK(out.config.renderer_string == reinterpret_cast<std::uintptr_t>(config.renderer_string));
        CHECK(out.config.vendor_string == reinterpret_cast<std::uintptr_t>(config.vendor_string));
        CHECK(out.config.version_string == reinterpret_cast<std::uintptr_t>(config.version_string));
        CHECK(out.config.extensions_string == reinterpret_cast<std::uintptr_t>(config.extensions_string));
        CHECK(out.config.maxTextureSize == 8192 && out.config.maxActiveTextures == 8);
        CHECK(out.config.maxTextureFilterAnisotropy == 16 && out.config.clampToEdgeAvailable == 1);
        CHECK(out.config.colorBits == 24 && out.config.depthBits == 24 && out.config.stencilBits == 8);
        CHECK(out.config.textureCompression == 3 && out.config.textureEnvAddAvailable == 1);
        CHECK(out.config.vidWidth == 1600 && out.config.vidHeight == 900 && out.config.displayFrequency == 144);
    } else CHECK(out.config.renderer_string == 0xa5a5a5a5u);
    const auto expected = out;
    const int unknownCompression = 3;
    std::memcpy(&config.textureCompression, &unknownCompression, sizeof(unknownCompression));
    CHECK(!ui::Glconfig(config, 16, true, out.config));
    CHECK(!std::memcmp(&expected, &out, sizeof(out)));
    config.textureCompression = TC_NONE;
    for (const std::uint32_t invalidBits : {0xbf800000u, 0x7f800000u, 0x7fc00000u}) {
        volatile std::uint32_t encoded = invalidBits;
        const std::uint32_t bits = encoded;
        float invalid; std::memcpy(&invalid, &bits, sizeof(invalid));
        CHECK(!ui::Glconfig(config, invalid, true, out.config));
        CHECK(!std::memcmp(&expected, &out, sizeof(out)));
    }
    return 0;
}
int main() {
    if (const auto result = Mapping()) return result;
    if (const auto result = Records()) return result;
    return RendererRecord();
}
