/*
 * MN9 skin shim: swap Beck's skeletal mesh while keeping Beck's pawn,
 * controller, AnimTree and therefore his movement/moveset.
 *
 * The archetype used for the player is `Spw09_Archetype.Spw09_Archetype`.
 * APawn::Mesh is at +0x478; USkeletalMeshComponent::SkeletalMesh is +0x250.
 * We set the template mesh before spawn and, if a player pawn already exists,
 * call USkeletalMeshComponent::SetSkeletalMesh on it.
 */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <link.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>

typedef void* UObjectPtr;
typedef void* UClassPtr;
typedef void* USkeletalMeshPtr;
typedef void* USkelCompPtr;
typedef void* APawnPtr;

#define OFF_GWORLD          0x2ec3c08u
#define OFF_STATIC_LOAD     0xde2890u
#define OFF_GET_WORLD_INFO  0x115dea0u
#define OFF_SKEL_CLASS      0x2eec690u
#define OFF_SKEL_INIT       0x17a4f40u
#define OFF_SET_SKEL_MESH   0x1171ef0u
#define OFF_IS_LOCAL        0x157bf80u
#define OFF_PAWN_MESH       0x478u
#define OFF_COMP_SKEL       0x250u
#define OFF_PAWN_NEXT       0x280u
#define OFF_WORLDINFO_PAWN  0x558u

typedef UObjectPtr (*StaticLoadFn)(UClassPtr, UObjectPtr, const wchar_t*, const wchar_t*, unsigned int, void*, unsigned int);
typedef UObjectPtr (*GetWorldInfoFn)(UObjectPtr, unsigned int);
typedef void (*SetSkelMeshFn)(USkelCompPtr, USkeletalMeshPtr, unsigned int);
typedef int (*IsLocalFn)(APawnPtr);
typedef UClassPtr (*SkelInitFn)(const wchar_t*);
typedef UClassPtr (*ObjStaticClassFn)(void);

static uintptr_t g_base = 0;
static int g_base_found = 0;
static int g_inited = 0;
static int g_enabled = 0;
static int g_applied = 0;
static int g_log = 0;
static const char* g_skin = NULL;
static USkeletalMeshPtr g_mesh = NULL;
static double g_last = 0;

static double now_s(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return ts.tv_sec + ts.tv_nsec / 1e9;
}

static int phdr_cb(struct dl_phdr_info* info, size_t size, void* data) {
  (void)size;
  if (info->dlpi_name && info->dlpi_name[0] != '\0') return 0;
  *(uintptr_t*)data = (uintptr_t)info->dlpi_addr;
  return 1;
}
static uintptr_t main_base(void) {
  if (g_base_found) return g_base;
  uintptr_t b = 0;
  if (dl_iterate_phdr(phdr_cb, &b) > 0) { g_base = b; g_base_found = 1; }
  return g_base;
}

static int plausible(uintptr_t p) {
  return p > 0x10000u && p < 0x800000000000ull && (p & 7u) == 0;
}

static UClassPtr skel_class(void) {
  UClassPtr c = *(UClassPtr*)(g_base + OFF_SKEL_CLASS);
  if (c) return c;
  SkelInitFn init = (SkelInitFn)(g_base + OFF_SKEL_INIT);
  return init ? init(NULL) : NULL;
}

static USkeletalMeshPtr load_mesh(void) {
  if (g_mesh) return g_mesh;
  StaticLoadFn load = (StaticLoadFn)(g_base + OFF_STATIC_LOAD);
  if (!load) return NULL;
  UClassPtr cls = skel_class();
  if (!cls) return NULL;

  const char* paths[8];
  int n = 0;
  if (!strcmp(g_skin, "megaxel")) {
    paths[n++] = "Spw11_Model.Mesh.Spw11_Body1";
    paths[n++] = "Spw11_Model.Mesh.Spw11_XelBody1";
    paths[n++] = "Spw11_Model.Spw11_Body1";
    paths[n++] = "MN9Game.SkeletalMesh3.Spw11_Body1";
  } else if (!strcmp(g_skin, "call")) {
    paths[n++] = "Spw10_Model.Mesh.Spw10_Body1";
    paths[n++] = "Spw10_Model.Spw10_Body1";
    paths[n++] = "MN9Game.SkeletalMesh3.Spw10_Body1";
    paths[n++] = "Spw10_Body1";
  } else if (!strcmp(g_skin, "ray")) {
    paths[n++] = "Pl03_Model.Mesh.PL03_Body";
    paths[n++] = "Pl03_Model.Mesh.PL03_Body";
    paths[n++] = "Pl03_Model.PL03_Body";
    paths[n++] = "PL03_Body";
  }

  for (int i = 0; i < n; i++) {
    wchar_t wide[256];
    size_t j = 0;
    for (; paths[i][j] && j < 255; j++) wide[j] = (wchar_t)(unsigned char)paths[i][j];
    wide[j] = 0;
    UObjectPtr o = load(cls, NULL, wide, NULL, 0, NULL, 0);
    if (o) {
      g_mesh = o;
      fprintf(stderr, "[skin] loaded %s -> %p\n", paths[i], o);
      return o;
    }
    if (g_log++ < 8) fprintf(stderr, "[skin] not found: %s\n", paths[i]);
  }
  fprintf(stderr, "[skin] no mesh found for %s\n", g_skin);
  return NULL;
}

static void apply(void) {
  if (!g_enabled || g_applied) return;
  if (!g_base_found) main_base();
  if (!g_mesh && !load_mesh()) return;

  /* 1. Template archetype: affects every future spawn. */
  StaticLoadFn load = (StaticLoadFn)(g_base + OFF_STATIC_LOAD);
  ObjStaticClassFn obj_class = (ObjStaticClassFn)(g_base + 0xe7b8c0);
  if (load && obj_class) {
    const wchar_t* arch_name = L"Spw09_Archetype.Spw09_Archetype";
    UObjectPtr arch = load(obj_class(), NULL, arch_name, NULL, 0, NULL, 0);
    if (arch) {
      USkelCompPtr comp = *(USkelCompPtr*)((char*)arch + OFF_PAWN_MESH);
      if (comp && plausible((uintptr_t)comp)) {
        *(USkeletalMeshPtr*)((char*)comp + OFF_COMP_SKEL) = g_mesh;
        fprintf(stderr, "[skin] archetype mesh -> %p\n", g_mesh);
      }
    }
  }

  /* 2. Current player pawn, if it already exists. */
  UObjectPtr world = *(UObjectPtr*)(g_base + OFF_GWORLD);
  if (world && plausible((uintptr_t)world)) {
    GetWorldInfoFn get_wi = (GetWorldInfoFn)(g_base + OFF_GET_WORLD_INFO);
    IsLocalFn is_local = (IsLocalFn)(g_base + OFF_IS_LOCAL);
    SetSkelMeshFn set_mesh = (SetSkelMeshFn)(g_base + OFF_SET_SKEL_MESH);
    UObjectPtr wi = get_wi ? get_wi(world, 0) : NULL;
    if (wi && plausible((uintptr_t)wi) && is_local && set_mesh) {
      APawnPtr pawn = *(APawnPtr*)((char*)wi + OFF_WORLDINFO_PAWN);
      for (int i = 0; i < 128 && pawn && plausible((uintptr_t)pawn); i++) {
        uintptr_t ctrl = *(uintptr_t*)((char*)pawn + 0x278);
        uintptr_t vt = ctrl ? *(uintptr_t*)ctrl : 0;
        if (!plausible(ctrl) || !plausible(vt)) {
          pawn = *(APawnPtr*)((char*)pawn + OFF_PAWN_NEXT);
          continue;
        }
        if (is_local(pawn)) {
          USkelCompPtr comp = *(USkelCompPtr*)((char*)pawn + OFF_PAWN_MESH);
          if (comp && plausible((uintptr_t)comp)) {
            set_mesh(comp, g_mesh, 0);
            fprintf(stderr, "[skin] player pawn mesh -> %p\n", g_mesh);
            g_applied = 1;
          }
          break;
        }
        pawn = *(APawnPtr*)((char*)pawn + OFF_PAWN_NEXT);
      }
    }
  }

  if (g_applied && g_mesh) fprintf(stderr, "[skin] applied %s\n", g_skin);
}

static void skin_tick(void) {
  if (!g_enabled || g_applied) return;
  double t = now_s();
  if (t - g_last < 1.0) return;
  g_last = t;
  apply();
}

typedef int (*PollEventFn)(void*);
extern "C" int SDL_PollEvent(void* ev) {
  static PollEventFn real = NULL;
  if (!real) real = (PollEventFn)dlsym(RTLD_NEXT, "SDL_PollEvent");
  int r = real ? real(ev) : 0;
  if (!g_inited) {
    g_inited = 1;
    main_base();
    g_skin = getenv("MN9_SKIN");
    g_enabled = g_skin && *g_skin;
    if (g_enabled) fprintf(stderr, "[skin] enabled skin=%s\n", g_skin);
  }
  skin_tick();
  return r;
}
