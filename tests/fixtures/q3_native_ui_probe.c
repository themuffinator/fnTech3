/* fnTech3 test-only legacy Q3 UI fixture, GPL-2.0-or-later.
 * Ordinals/layout facts: pinned FnQuake3 ui_public.h,
 * 7d8c9629cdc7979f09697266470359101ffd9eda. Independently authored; no game/UI
 * behavior is reconstructed.
 */
#include <stdint.h>
#include <string.h>
enum {
  PRINT = 1,
  ARGV = 11,
  GETGLCONFIG = 43,
  GETCLIENTSTATE = 44,
  GETCONFIGSTRING = 45
};
static intptr_t(__cdecl *call)(intptr_t, ...);
static unsigned char config[0x2c44 + 16], state[3084 + 16], text[128 + 16];
static unsigned int u32(const unsigned char *p) {
  return p[0] | ((unsigned int)p[1] << 8) | ((unsigned int)p[2] << 16) |
         ((unsigned int)p[3] << 24);
}
static int guards(const unsigned char *p, int size) {
  int i;
  for (i = 0; i < 8; i++)
    if (p[i] != 0xa5 || p[size + 8 + i] != 0xa5)
      return 0;
  return 1;
}
static int terminated(const unsigned char *p, int size) {
  int i;
  for (i = 0; i < size; i++)
    if (!p[i])
      return 1;
  return 0;
}
static void result(const char *ok, const char *bad, int valid) {
  call(PRINT, valid ? ok : bad);
}
__declspec(dllexport) void __cdecl dllEntry(
    intptr_t(__cdecl *syscallptr)(intptr_t, ...)) {
  call = syscallptr;
}
__declspec(dllexport) intptr_t __cdecl vmMain(int operation, ...) {
  if (operation == 0) {
    call(PRINT, "Q3 UI native probe: api ok\n");
    return 6;
  }
  if (operation == 1) {
    memset(config, 0xa5, sizeof(config));
    call(GETGLCONFIG, config + 8);
    result("Q3 UI native probe: glconfig ok\n",
           "Q3 UI native probe: glconfig FAILED\n",
           guards(config, 0x2c44) && config[8] != 0 &&
               u32(config + 8 + 11304) > 0 &&
               u32(config + 8 + 11304) <= 16384 &&
               u32(config + 8 + 11308) > 0 && u32(config + 8 + 11308) <= 16384);
  }
  if (operation == 2)
    call(PRINT, "Q3 UI native probe: shutdown ok\n");
  if (operation == 8) {
    char arg[64] = {0};
    call(ARGV, 0, arg, sizeof(arg));
    if (!strcmp(arg, "fntech3_q3_ui_probe")) {
      int valid, i;
      memset(state, 0xa5, sizeof(state));
      call(GETCLIENTSTATE, state + 8);
      valid =
          guards(state, 3084) && u32(state + 8) == 8 && u32(state + 8 + 8) < 64;
      for (i = 0; i < 3; i++)
        if (!terminated(state + 8 + 12 + i * 1024, 1024))
          valid = 0;
      result("Q3 UI native probe: clientstate ok\n",
             "Q3 UI native probe: clientstate FAILED\n", valid);
      memset(text, 0xa5, sizeof(text));
      valid = (int)call(GETCONFIGSTRING, 0, text + 8, 128);
      result("Q3 UI native probe: configstring ok\n",
             "Q3 UI native probe: configstring FAILED\n",
             valid && guards(text, 128) && text[8] != 0 &&
                 terminated(text + 8, 128));
      call(PRINT, "Q3 UI native probe: command ok\n");
      return 1;
    }
  }
  return 0;
}
