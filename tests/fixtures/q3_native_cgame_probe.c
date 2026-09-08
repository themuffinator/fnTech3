/* fnTech3 test-only legacy Q3 cgame fixture, GPL-2.0-or-later.
 * Independently authored from the pinned Q3 import/export/layout contracts.
 * No retail game behavior or assets are reconstructed here.
 */
#include <stdint.h>
#include <string.h>
/* Ordinals: FnQuake3 cg_public.h at 7d8c9629cdc7979f09697266470359101ffd9eda.
 */
enum {
  PRINT = 0,
  ARGV = 8,
  ADDCOMMAND = 15,
  GETGLCONFIG = 49,
  GETGAMESTATE = 50,
  GETSNAPNUMBER = 51,
  GETSNAP = 52,
  GETCMDNUMBER = 54,
  GETCMD = 55,
  SETCMD = 56
};
static intptr_t(__cdecl *call)(intptr_t, ...);
static unsigned char gl[0x2c44 + 16], gs[20100 + 16], cmd[24 + 16],
    snap[0xd20c + 16];
static int commandOK, snapshotOK, drew;
#ifdef FNTECH3_NATIVE_PAIR
static int initialStateOK, deltaStateOK;
static float f32(const unsigned char *p) {
  float v;
  memcpy(&v, p, 4);
  return v;
}
#endif
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
static void result(const char *ok, const char *bad, int value) {
  call(PRINT, value ? ok : bad);
}
__declspec(dllexport) void __cdecl dllEntry(
    intptr_t(__cdecl *syscallptr)(intptr_t, ...)) {
  call = syscallptr;
}
__declspec(dllexport) intptr_t __cdecl vmMain(int operation, ...) {
  if (operation == 0) {
    unsigned int count, i;
    int valid;
    float sensitivity = 0.625f;
    int bits;
    memset(gl, 0xa5, sizeof(gl));
    call(GETGLCONFIG, gl + 8);
    valid = guards(gl, 0x2c44) && gl[8] != 0 && u32(gl + 8 + 11304) > 0 &&
            u32(gl + 8 + 11304) <= 16384 && u32(gl + 8 + 11308) > 0 &&
            u32(gl + 8 + 11308) <= 16384;
    result("Q3 native probe: glconfig ok\n",
           "Q3 native probe: glconfig FAILED\n", valid);
    memset(gs, 0xa5, sizeof(gs));
    call(GETGAMESTATE, gs + 8);
    count = u32(gs + 8 + 20096);
    valid =
        guards(gs, 20100) && count > 0 && count <= 16000 && gs[8 + 4096] == 0;
    if (valid)
      for (i = 0; i < 1024; i++) {
        unsigned int offset = u32(gs + 8 + i * 4), end;
        if (offset >= count) {
          valid = 0;
          break;
        }
        for (end = offset; end < count && gs[8 + 4096 + end]; end++) {
        }
        if (end == count) {
          valid = 0;
          break;
        }
      }
    result("Q3 native probe: gamestate ok\n",
           "Q3 native probe: gamestate FAILED\n", valid);
    memcpy(&bits, &sensitivity, 4);
    call(SETCMD, 7, bits);
    call(ADDCOMMAND, "fntech3_q3_probe");
  }
  if (operation == 2) {
    char text[64] = {0};
    call(ARGV, 0, text, sizeof(text));
    if (!strcmp(text, "fntech3_q3_probe")) {
      call(PRINT, "Q3 native probe: command ok\n");
      return 1;
    }
  }
  if (operation == 3) {
#ifdef FNTECH3_NATIVE_PAIR
    if (!deltaStateOK) {
      int number = 0, time = 0, i, found = 0, valid;
      unsigned int count, health;
      unsigned char *ps = snap + 8 + 44;
      call(GETSNAPNUMBER, &number, &time);
      memset(snap, 0xa5, sizeof(snap));
      if (number > 0 && call(GETSNAP, number, snap + 8)) {
        count = u32(snap + 8 + 512);
        health = u32(ps + 184);
        valid = guards(snap, 0xd20c) && count <= 256 &&
                u32(snap + 16) == (unsigned int)time && u32(ps + 140) == 0 &&
                u32(ps + 144) == 7 && u32(ps + 184 + 15 * 4) == 321 &&
                u32(ps + 248 + 15 * 4) == 1234 && u32(ps + 376 + 7 * 4) == 47 &&
                f32(ps + 20) == 123.25f && f32(ps + 24) == -64.5f &&
                f32(ps + 28) == 32.0f;
        if (valid)
          for (i = 0; i < (int)count; i++) {
            unsigned char *e = snap + 8 + 516 + i * 208;
            if (u32(e) == 17) {
              found = 1;
              valid = u32(e + 4) == 2 && u32(e + 192) == 7 &&
                      u32(e + 204) == 91 &&
                      u32(e + 84) == (health == 113 ? 4242u : 5678u) &&
                      f32(e + 24) == 123.25f && f32(e + 28) == -64.5f &&
                      f32(e + 32) == 32.0f && f32(e + 92) == 123.25f &&
                      f32(e + 96) == -64.5f && f32(e + 100) == 32.0f;
              break;
            }
          }
        if (health == 113 && !initialStateOK) {
          initialStateOK = 1;
          result("Q3 native probe: live initial snapshot ok\n",
                 "Q3 native probe: live initial snapshot FAILED\n",
                 valid && found);
        }
        if (health == 127) {
          deltaStateOK = 1;
          result("Q3 native probe: live delta snapshot ok\n",
                 "Q3 native probe: live delta snapshot FAILED\n",
                 valid && found && initialStateOK && u32(ps) > 0);
        }
      }
    }
#endif
    if (!drew) {
      call(PRINT, "Q3 native probe: frame ok\n");
      drew = 1;
    }
    if (!commandOK) {
      int n = (int)call(GETCMDNUMBER);
      memset(cmd, 0xa5, sizeof(cmd));
      if (n > 0 && call(GETCMD, n, cmd + 8)) {
        commandOK = 1;
        result("Q3 native probe: usercmd ok\n",
               "Q3 native probe: usercmd FAILED\n",
               guards(cmd, 24) && cmd[8 + 20] == 7);
      }
    }
    if (!snapshotOK) {
      int n = 0, time = 0, i, valid;
      unsigned int count;
      call(GETSNAPNUMBER, &n, &time);
      memset(snap, 0xa5, sizeof(snap));
      valid = !call(GETSNAP, -1, snap + 8);
      for (i = 0; i < (int)sizeof(snap); i++)
        if (snap[i] != 0xa5)
          valid = 0;
      if (n > 0 && call(GETSNAP, n, snap + 8)) {
        snapshotOK = 1;
        count = u32(snap + 8 + 512);
        valid = valid && guards(snap, 0xd20c) &&
                u32(snap + 8 + 8) == (unsigned int)time && count <= 256;
        if (valid)
          for (i = 0; i < (int)count; i++)
            if (u32(snap + 8 + 516 + i * 208) >= 1024)
              valid = 0;
        result("Q3 native probe: snapshot ok\n",
               "Q3 native probe: snapshot FAILED\n", valid);
      }
    }
  }
  return 0;
}
