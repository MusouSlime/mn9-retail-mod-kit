/*
 * Beck boss-palette shim.
 *
 * The PC build bakes Beck's colours into material instances.  The Mighty
 * Numbers' body/head/eye/cheek/mouth materials are full UMaterialInstanceConstant
 * assets, so swapping the parent of Beck's five body MICs gives the exact boss
 * palette without touching textures.
 *
 * Config: <game>/.mn9charsel/palette.conf
 *   enabled=1
 *   mode=regular|boss|random
 *   boss=1..8
 *
 * The shim applies once per spawned MN9Spw09Pawn (so each stage gets a fresh
 * palette when mode=random).  It is inactive for Call/Ray/MegaXEL skins.
 */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <wchar.h>

typedef struct { int index, number; } FName;
typedef void (*FNameCtor)(void*, const wchar_t*, int, unsigned int);
typedef void* (*FindFunctionFn)(void*, uint64_t, unsigned int);
typedef void (*ProcessEventFn)(void*, void*, void*);
typedef void (*SetParentFn)(void*, void*);
typedef void (*GetNameFn)(void*, void*);
typedef struct { wchar_t* data; int count; int max; } FString;

static void* g_applied_pawn = NULL;
static int g_applied_boss = -1;
static double g_seen = 0;
static double g_last = 0;
static int g_logged = 0;

static double now_s(void) {
  struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
  return ts.tv_sec + ts.tv_nsec / 1e9;
}
static int plausible(uintptr_t p) {
  return p > 0x10000u && p < 0x800000000000ull && (p & 7u) == 0;
}
static void obj_name(void* o, wchar_t* out, size_t n) {
  out[0] = 0;
  GetNameFn gn = (GetNameFn)0xe7e230;
  if (!gn || !o) return;
  FString s = {0,0,0};
  gn(&s, o);
  if (s.data && s.count > 0) {
    size_t k = 0;
    for (; k + 1 < n && k < (size_t)s.count && s.data[k]; k++) out[k] = s.data[k];
    out[k] = 0;
  }
}
static int issubclass_of(void* cls, void* base) {
  for (int i = 0; cls && i < 32; i++) {
    if (cls == base) return 1;
    cls = *(void**)((char*)cls + 0x78);
  }
  return 0;
}
static void* find_object(const wchar_t* want) {
  void** objs = *(void***)0x2dcdd08;
  int n = *(int*)0x2dcdd10;
  if (!objs || n <= 0 || n > 1000000) return NULL;
  for (int i = 0; i < n; i++) {
    void* o = objs[i];
    if (!o) continue;
    wchar_t nm[128];
    obj_name(o, nm, 128);
    if (!wcscmp(nm, want)) return o;
  }
  return NULL;
}
static void* find_live_pawn(void) {
  void** objs = *(void***)0x2dcdd08;
  int n = *(int*)0x2dcdd10;
  if (!objs || n <= 0 || n > 1000000) return NULL;
  void* pawn_class = *(void**)0x2dccb28;   /* AMN9PlayerPawn */
  if (!pawn_class) return NULL;
  int (*is_local)(void*) = (void*)0x157bf80;
  for (int i = 0; i < n; i++) {
    void* o = objs[i];
    if (!o) continue;
    void* cls = *(void**)((char*)o + 0x50);
    if (!issubclass_of(cls, pawn_class)) continue;
    wchar_t nm[128];
    obj_name(o, nm, 128);
    if (!wcsncmp(nm, L"Default__", 9)) continue;
    void* ctrl = *(void**)((char*)o + 0x278);
    if (!ctrl || !plausible((uintptr_t)ctrl)) continue;
    if (is_local && is_local(o)) return o;
  }
  return NULL;
}

/* ---- config ---------------------------------------------------------- */
static int config_enabled(void) {
  const char* gd = getenv("MN9_GAME_DIR");
  if (!gd || !*gd) return 0;
  char path[1400];
  snprintf(path, sizeof(path), "%s/.mn9charsel/palette.conf", gd);
  FILE* f = fopen(path, "r");
  if (!f) return 0;
  int enabled = 1;
  char line[256];
  while (fgets(line, sizeof(line), f)) {
    char* eq = strchr(line, '=');
    if (!eq) continue;
    *eq = 0;
    char* k = line; while (*k == ' ' || *k == '\t') k++;
    char* v = eq + 1; while (*v == ' ' || *v == '\t') v++;
    char* e = v + strlen(v); while (e > v && (e[-1] == '\n' || e[-1] == '\r' || e[-1] == ' ')) *--e = 0;
    if (!strcmp(k, "enabled")) enabled = *v && *v != '0';
  }
  fclose(f);
  return enabled;
}
static int config_boss(int* random_mode) {
  const char* gd = getenv("MN9_GAME_DIR");
  if (!gd || !*gd) return 0;
  char path[1400];
  snprintf(path, sizeof(path), "%s/.mn9charsel/palette.conf", gd);
  FILE* f = fopen(path, "r");
  if (!f) return 0;
  int boss = 0;
  char mode[32] = "regular";
  char line[256];
  while (fgets(line, sizeof(line), f)) {
    char* eq = strchr(line, '=');
    if (!eq) continue;
    *eq = 0;
    char* k = line; while (*k == ' ' || *k == '\t') k++;
    char* v = eq + 1; while (*v == ' ' || *v == '\t') v++;
    char* e = v + strlen(v); while (e > v && (e[-1] == '\n' || e[-1] == '\r' || e[-1] == ' ')) *--e = 0;
    if (!strcmp(k, "boss")) boss = atoi(v);
    else if (!strcmp(k, "mode")) snprintf(mode, sizeof(mode), "%s", v);
  }
  fclose(f);
  if (!strcmp(mode, "random")) { *random_mode = 1; return 1 + (int)(now_s() * 1000.0) % 8; }
  *random_mode = 0;
  if (!strcmp(mode, "boss")) {
    if (boss < 1 || boss > 8) boss = 1;
    return boss;
  }
  return 0;
}

/* ---- game calls ------------------------------------------------------ */
static void* get_mic(void* pawn, const wchar_t* part) {
  FNameCtor fnc = (FNameCtor)0xde4990;
  FindFunctionFn find = (FindFunctionFn)0xdd9bf0;
  ProcessEventFn pe = (ProcessEventFn)0xde3b80;
  if (!fnc || !find || !pe) return NULL;
  uint64_t gmfn = 0;
  fnc(&gmfn, L"GetMaterialInstance", 0, 0);
  if (!gmfn) return NULL;
  void* gm = find(pawn, gmfn, 0);
  if (!gm) return NULL;
  uint64_t pn = 0;
  fnc(&pn, part, 0, 0);
  if (!pn) return NULL;
  unsigned char params[64] = {0};
  *(uint64_t*)params = pn;
  pe(pawn, gm, params);
  void* mic = *(void**)(params + 8);
  return (mic && plausible((uintptr_t)mic)) ? mic : NULL;
}

static int find_boss_materials(int boss, void*** out_data, int* out_num) {
  wchar_t mesh_name[64];
  swprintf(mesh_name, 64, L"Spw%02d_Body1", boss);
  void* mesh = find_object(mesh_name);
  if (!mesh) return 0;
  for (int off = 0; off < 0x600; off += 8) {
    void* data = *(void**)((char*)mesh + off);
    int num = *(int*)((char*)mesh + off + 8);
    int max = *(int*)((char*)mesh + off + 12);
    if (!data || !plausible((uintptr_t)data) || num < 4 || num > 16 || max < num) continue;
    void* first = *(void**)data;
    if (!first || !plausible((uintptr_t)first)) continue;
    void* cls = *(void**)((char*)first + 0x50);
    wchar_t cn[128];
    obj_name(cls, cn, 128);
    if (!wcsstr(cn, L"Material")) continue;
    *out_data = (void**)data;
    *out_num = num;
    return 1;
  }
  return 0;
}

static void apply_palette(void* pawn) {
  if (g_applied_pawn == pawn) return;
  int random_mode = 0;
  int boss = config_boss(&random_mode);
  if (boss < 1 || boss > 8) { g_applied_pawn = pawn; g_applied_boss = 0; return; }
  void** data = NULL; int num = 0;
  if (!find_boss_materials(boss, &data, &num)) return;

  static const wchar_t* parts[5] = { L"Body1", L"Head1", L"Eye1", L"Cheek1", L"Mouth1" };
  SetParentFn set_parent = (SetParentFn)0x11cc500;
  int changed = 0;
  for (int p = 0; p < 5; p++) {
    wchar_t full[64];
    swprintf(full, 64, L"Spw09_%ls", parts[p]);
    void* mic = get_mic(pawn, full);
    if (!mic) continue;
    void* target = NULL;
    for (int j = 0; j < num; j++) {
      void* m = data[j];
      if (!m) continue;
      wchar_t mn[128];
      obj_name(m, mn, 128);
      if (wcsstr(mn, parts[p])) { target = m; break; }
    }
    if (!target && p < num) target = data[p];
    if (target) { set_parent(mic, target); changed++; }
  }
  g_applied_pawn = pawn;
  g_applied_boss = boss;

  const char* gd = getenv("MN9_GAME_DIR");
  if (gd && *gd) {
    char path[1400];
    snprintf(path, sizeof(path), "%s/.mn9charsel/palette.log", gd);
    FILE* f = fopen(path, "a");
    if (f) {
      fprintf(f, "[palette] pawn=%p boss=%d parts=%d%s\n", pawn, boss, changed,
              random_mode ? " random" : "");
      fclose(f);
    }
  }
}

static void palette_tick(void) {
  if (g_applied_pawn && g_applied_boss >= 0) {
    /* Re-apply on a new pawn (stage change), but don't spam the same pawn. */
  }
  if (!config_enabled()) return;
  const char* ch = getenv("MN9_CHARACTER");
  const char* skin = getenv("MN9_SKIN");
  if ((ch && *ch && strcmp(ch, "beck")) || (skin && *skin)) return;
  if ((pid_t)syscall(SYS_gettid) != getpid()) return;
  double t = now_s();
  if (t - g_last < 1.0) return;
  g_last = t;
  void* pawn = find_live_pawn();
  if (!pawn) { g_seen = 0; return; }
  if (g_seen == 0) { g_seen = t; return; }
  if (t - g_seen < 2.0) return;
  apply_palette(pawn);
}

typedef int (*PollEventFn)(void*);
int SDL_PollEvent(void* ev) {
  static PollEventFn real = NULL;
  if (!real) real = (PollEventFn)dlsym(RTLD_NEXT, "SDL_PollEvent");
  int r = real ? real(ev) : 0;
  palette_tick();
  return r;
}
