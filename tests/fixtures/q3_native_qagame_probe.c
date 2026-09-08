/* fnTech3 test-only legacy Q3 qagame fixture, GPL-2.0-or-later.
 * Ordinals/layout facts: pinned FnQuake3 g_public.h and q_shared.h,
 * 7d8c9629cdc7979f09697266470359101ffd9eda. Independently authored;
 * no retail game behavior or implementation text is incorporated.
 */
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
  GET_TOKEN = 37
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
__declspec(dllexport) void __cdecl dllEntry(
    intptr_t(__cdecl *syscallptr)(intptr_t, ...)) {
  call = syscallptr;
}
__declspec(dllexport) intptr_t __cdecl vmMain(int operation, ...) {
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
