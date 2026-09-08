/* fnTech3 test-only legacy Q3 qagame fixture, GPL-2.0-or-later.
 * Ordinals/layout facts: pinned FnQuake3 g_public.h and q_shared.h,
 * 7d8c9629cdc7979f09697266470359101ffd9eda. Independently authored;
 * no retail game behavior or implementation text is incorporated.
 */
#include <stdarg.h>
#include <stdint.h>
#include <string.h>

enum {
  PRINT = 0,
  CVAR_INTEGER = 6,
  ARGV = 9,
  SEND_CONSOLE = 14,
  LOCATE = 15,
  SET_CONFIG = 18,
  GET_CONFIG = 19,
  TRACE = 24,
  POINT_CONTENTS = 25,
  LINK = 30,
  UNLINK = 31,
  ENTITIES_IN_BOX = 32,
  GET_USERCMD = 36,
  GET_TOKEN = 37,
  BOT_USER_COMMAND = 211
};
enum {
  ENTITY_BYTES = 516,
  PLAYER_BYTES = 468,
  GUARD = 8,
  COUNT = 64,
  BODY = 0x2000000
};
typedef union {
  uint32_t align;
  unsigned char bytes[ENTITY_BYTES + 16];
} Entity;
typedef union {
  uint32_t align;
  unsigned char bytes[PLAYER_BYTES + 16];
} Player;
static Entity entities[COUNT];
static Player players[COUNT];
static intptr_t(__cdecl *call)(intptr_t, ...);
static int frames;
static int liveClient = -1, liveFrames, liveCommand;

static int u32(const unsigned char *p) {
  int v;
  memcpy(&v, p, 4);
  return v;
}
static float f32(const unsigned char *p) {
  float v;
  memcpy(&v, p, 4);
  return v;
}
static void puti(unsigned char *p, int v) { memcpy(p, &v, 4); }
static void putf(unsigned char *p, float v) { memcpy(p, &v, 4); }
static int guards(const unsigned char *p, int size) {
  int i;
  for (i = 0; i < GUARD; i++)
    if (p[i] != 0xa5 || p[GUARD + size + i] != 0xa5)
      return 0;
  return 1;
}
static void result(const char *name, int valid) {
  call(PRINT, "Q3 qagame native probe: ");
  call(PRINT, name);
  call(PRINT, valid ? " ok\n" : " FAILED\n");
}
static int all_guards(void) {
  int i;
  for (i = 0; i < COUNT; i++)
    if (!guards(entities[i].bytes, ENTITY_BYTES) ||
        !guards(players[i].bytes, PLAYER_BYTES))
      return 0;
  return 1;
}
static void init(void) {
  unsigned char token[128 + 16], config[64 + 16], command[24 + 16];
  int i;
  frames = 0;
  liveClient = -1;
  liveFrames = liveCommand = 0;
  memset(entities, 0xa5, sizeof(entities));
  memset(players, 0xa5, sizeof(players));
  for (i = 0; i < COUNT; i++) {
    memset(entities[i].bytes + GUARD, 0, ENTITY_BYTES);
    memset(players[i].bytes + GUARD, 0, PLAYER_BYTES);
    puti(entities[i].bytes + GUARD, i);
    puti(entities[i].bytes + GUARD + 512, 1023);
  }
  if (call(CVAR_INTEGER, "fntech3_probe_bad_stride")) {
    call(LOCATE, entities[0].bytes + GUARD, COUNT, ENTITY_BYTES - 1,
         players[0].bytes + GUARD, sizeof(Player));
    result("invalid binding rejected", 0);
    return;
  }
  call(LOCATE, entities[0].bytes + GUARD, COUNT, sizeof(Entity),
       players[0].bytes + GUARD, sizeof(Player));
  result("binding", all_guards());
  memset(token, 0xa5, sizeof(token));
  i = (int)call(GET_TOKEN, token + GUARD, 128);
  result("entity token", i && guards(token, 128) && token[GUARD] == '{' &&
                             token[GUARD + 1] == 0);
  call(SET_CONFIG, 10, "q3-native-qagame-fixture");
  memset(config, 0xa5, sizeof(config));
  call(GET_CONFIG, 10, config + GUARD, 64);
  result("configstring", guards(config, 64) &&
                             !memcmp(config + GUARD, "q3-native-qagame-fixture",
                                     sizeof("q3-native-qagame-fixture")));
  memset(command, 0xa5, sizeof(command));
  call(GET_USERCMD, 0, command + GUARD);
  for (i = 0; i < 24; i++)
    if (command[GUARD + i])
      break;
  result("usercmd", guards(command, 24) && i == 24);
  {
    unsigned char sent[24 + 16], received[24 + 16];
    memset(sent, 0xa5, sizeof(sent));
    memset(sent + GUARD, 0, 24);
    puti(sent + GUARD, 12345);
    puti(sent + GUARD + 4, -1234);
    puti(sent + GUARD + 8, 2345);
    puti(sent + GUARD + 12, -3456);
    puti(sent + GUARD + 16, 0x81);
    sent[GUARD + 20] = 7;
    sent[GUARD + 21] = (unsigned char)-40;
    sent[GUARD + 22] = 50;
    sent[GUARD + 23] = (unsigned char)-60;
    call(BOT_USER_COMMAND, 1, sent + GUARD);
    memset(received, 0xa5, sizeof(received));
    call(GET_USERCMD, 1, received + GUARD);
    result("bot usercmd", guards(sent, 24) && guards(received, 24) &&
                              !memcmp(sent + GUARD, received + GUARD, 24));
  }
  result("init", 1);
}
static void collision(void) {
  unsigned char *e = entities[1].bytes + GUARD;
  unsigned char list[32 + 16], trace[56 + 16];
  float low[3] = {-32, -32, -32}, high[3] = {32, 32, 32};
  float start[3] = {-100, 0, 0}, end[3] = {100, 0, 0}, zero[3] = {0, 0, 0};
  int i, n, valid;
  for (i = 0; i < 3; i++) {
    putf(e + 436 + 4 * i, -16);
    putf(e + 448 + 4 * i, 16);
  }
  puti(e + 460, BODY);
  call(LINK, e);
  valid = all_guards() && u32(e + 416) == 1 && u32(e + 420) == 1;
  for (i = 0; i < 3; i++)
    if (f32(e + 464 + 4 * i) != -17 || f32(e + 476 + 4 * i) != 17)
      valid = 0;
  result("link", valid);
  memset(list, 0xa5, sizeof(list));
  n = (int)call(ENTITIES_IN_BOX, low, high, list + GUARD, 8);
  result("area", guards(list, 32) && n == 1 && u32(list + GUARD) == 1);
  memset(trace, 0xa5, sizeof(trace));
  call(TRACE, trace + GUARD, start, zero, zero, end, 1023, BODY);
  result("trace", guards(trace, 56) && u32(trace + GUARD + 52) == 1 &&
                      f32(trace + GUARD + 8) > 0 && f32(trace + GUARD + 8) < 1);
  result("contents", (call(POINT_CONTENTS, zero, 1023) & BODY) != 0);
  call(UNLINK, e);
  n = (int)call(ENTITIES_IN_BOX, low, high, list + GUARD, 8);
  result("unlink",
         all_guards() && u32(e + 416) == 0 && n == 0 && guards(list, 32));
}
static void live_state(int client, int second) {
  unsigned char *ps = players[client].bytes + GUARD;
  unsigned char *e = entities[17].bytes + GUARD;
  float origin[3] = {123.25f, -64.5f, 32.0f};
  int i;
  puti(ps + 140, client);
  puti(ps + 144, 7);
  puti(ps + 184, second ? 127 : 113);
  puti(ps + 184 + 15 * 4, 321);
  puti(ps + 248 + 15 * 4, 1234);
  puti(ps + 376 + 7 * 4, 47);
  puti(e + 4, 2);
  puti(e + 84, second ? 5678 : 4242);
  puti(e + 192, 7);
  puti(e + 204, 91);
  puti(e + 424,
       0x20); /* SVF_BROADCAST: remove PVS visibility from this ABI test. */
  for (i = 0; i < 3; i++) {
    putf(ps + 20 + 4 * i, origin[i]);
    putf(e + 24 + 4 * i, origin[i]);
    putf(e + 92 + 4 * i, origin[i]);
    putf(e + 488 + 4 * i, origin[i]);
    putf(e + 436 + 4 * i, -8);
    putf(e + 448 + 4 * i, 8);
  }
  call(LINK, e);
  result(second ? "live delta state" : "live initial state", all_guards());
}
__declspec(dllexport) void __cdecl dllEntry(
    intptr_t(__cdecl *syscallptr)(intptr_t, ...)) {
  call = syscallptr;
}
__declspec(dllexport) intptr_t __cdecl vmMain(int operation, ...) {
  if (operation == 3 && call(CVAR_INTEGER, "fntech3_probe_live")) {
    va_list args;
    va_start(args, operation);
    liveClient = (int)va_arg(args, intptr_t);
    va_end(args);
    if (liveClient >= 0 && liveClient < COUNT)
      live_state(liveClient, 0);
    else
      result("client index", 0);
  }
  if (operation == 7 && liveClient >= 0 && liveClient < COUNT) {
    unsigned char command[24 + 16];
    memset(command, 0xa5, sizeof(command));
    call(GET_USERCMD, liveClient, command + GUARD);
    puti(players[liveClient].bytes + GUARD, u32(command + GUARD));
    if (!liveCommand && command[GUARD + 20] == 7 && u32(command + GUARD) > 0) {
      liveCommand = 1;
      result("live usercmd", guards(command, 24) && all_guards());
    }
  }
  if (operation == 0)
    init();
  if (operation == 1) {
    result("shutdown", all_guards());
    /* ERR_DROP clears the old command buffer before module shutdown. Queue
     * a fresh valid map run to prove the server can recover and rebind. */
    if (call(CVAR_INTEGER, "fntech3_probe_bad_stride"))
      call(SEND_CONSOLE, 2, "set fntech3_probe_bad_stride 0; exec probe.cfg\n");
  }
  if (operation == 8 && ++frames == 1) {
    collision();
    result("frame", 1);
  }
  if (operation == 8 && liveClient >= 0 && liveClient < COUNT &&
      ++liveFrames == 30)
    live_state(liveClient, 1);
  if (operation == 9) {
    char arg[64] = {0};
    call(ARGV, 0, arg, sizeof(arg));
    if (!memcmp(arg, "fntech3_q3_qagame_probe",
                sizeof("fntech3_q3_qagame_probe"))) {
      result("command", frames > 0 && all_guards());
      return 1;
    }
  }
  return 0;
}
