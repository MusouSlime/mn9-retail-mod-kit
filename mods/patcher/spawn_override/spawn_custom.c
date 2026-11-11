// MN9 character-swap override (PC / ET_EXEC build).
//
// Consistent swap mechanism:
//   1. read .mn9charsel/character (beck|megaxel|spw01..spw20|bs01..bs20)
//   2. at first gameplay pawn, locate that character's cooked pawn archetype
//      (Default__MN9Spw09Pawn / Default__MN9Spw11Pawn / Default__MN9Spw01Pawn ...)
//      and read its 7 body/arm asset slots directly — no per-character tables.
//   3. write those slots onto the player pawn (resolved via the player
//      controller) every tick, and reattach live components whose mesh still
//      equals the value the pawn had when we first saw it.
// No class substitution: controller, input, physics and stage logic untouched.
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/mman.h>
#include <math.h>
#include <wchar.h>
#include <strings.h>
#include "names.inc"

#define TICK_ADDR    0x13785c0UL   // UWorld::Tick
#define FINDOBJ_ADDR 0x0dbce00UL   // UObject::StaticFindObject
#define FINDFIELD    0x0d37770UL   // UObject::FindObjectField
#define FNAME_CTOR   0x0de4f60UL   // FName(char const*, EFindName)
#define LOADOBJ_ADDR 0x0de2890UL   // UObject::StaticLoadObject
#define SETSKMESH    0x1171ef0UL   // USkeletalMeshComponent::SetSkeletalMesh
#define SETANIMTREE  0x115aaa0UL   // USkeletalMeshComponent::SetAnimTreeTemplate
#define GETNAME_ADDR 0x0e7e230UL   // UObject::GetName() const
#define FINDFUNC_ADDR 0x0dd9bf0UL  // UObject::FindFunction
#define PROCESSEVENT_ADDR 0x0de3b80UL // UObject::ProcessEvent
#define FNAME_TOSTRING 0x0dddb70UL  // FName::ToString(FString&)
#define GETPATHNAME    0x0d4e600UL  // UObject::GetPathName(FString&, UObject const*) const
#define SPAWNACTOR_ADDR 0x11868f0UL  // UWorld::SpawnActor
#define SPAWN_STUB_OFF 0x100
#define SPAWN_STUB_SZ  0x80
#define OFF_CLASS    0x50
#define OFF_PROP     0x90
#define GENGINE_ADDR 0x2ef2b80UL
#define OBJ_ARRAY    0x2dcdd08UL
#define OBJ_COUNT    0x2dcdd10UL

typedef void  (*tick_fn)(void*, unsigned, float);
typedef void* (*findobj_fn)(void*, void*, const wchar_t*, unsigned);
typedef void  (*fname_fn)(uint64_t*, const char*, int);
typedef void* (*findfield_fn)(void*, uint64_t, unsigned);
typedef void* (*loadobj_fn)(void*, void*, const wchar_t*, const wchar_t*, unsigned, void*, unsigned);
typedef void  (*setskmesh_fn)(void*, void*, unsigned);
typedef void  (*setanim_fn)(void*, void*);
typedef void* (*spawn_fn)(void*, void*, uint64_t, void*, void*, void*, unsigned, unsigned, void*, void*, unsigned, void*);
typedef void  (*getname_fn)(void* out, void* obj);
typedef struct { wchar_t* data; int count; int max; } fstring_t;

static const char* SLOT_PROPS[7] = {
  "BodyMeshNormal", "BodyMeshDash", "BodyMeshSwoop",
  "ArmLMeshNormal", "ArmLMeshBuster", "ArmRMeshNormal", "ArmRMeshBuster"
};

__attribute__((visibility("hidden"))) tick_fn g_real_tick;
static void* g_pc_class;
static void* g_arch;              // selected character archetype
static void* g_arch_slot[7];      // asset pointers from that archetype
static void* g_player_slots[7];   // original player pawn slot values (fallback)
static void* g_beck_mesh[7];      // Beck's own slot assets, for component matching
static void* g_form0;             // Spw01 (Pyro) form archetype object
static void* g_spw01_class;       // target player pawn class
static void* g_beck_class_pawn;   // MN9Pl01Pawn class
static void* g_spw09_class;       // MN9Spw09Pawn class
static unsigned char* g_spawn_stub;
static void* g_ctrl;
static int   g_ctrl_pawn_off;
static int   g_pawn_ctrl_off;
static void* g_swapped_pawn;
static int g_spawn_hooked;
static int g_spawn_armed;
static int g_native_swap;
static void* g_cache_comps[64];   // components owned by the tracked pawn
static int   g_cache_n;
static void* g_pawn;
static int   g_sk_off;
static int   g_logged;
static unsigned g_ticks;
static int g_tick_alive;
static char g_selection[32];
static int  g_model_mode;
static int  g_overlay_mode;
static int  g_probe_mode;
static int  g_freeze_mode;
static int  g_chunk_mode;
static int  g_inf_mode;
static int  g_bchunk_mode;
static int  g_gpu_mode;
static int  g_gpu2_mode;
static int  g_gpu3_mode;
static void* g_meshobj;
static int addr_mapped(const void*);

static int class_name_of(void* o, char* out, int n)
{
  out[0] = 0;
  if (!o || !addr_mapped(o)) return 0;
  void* cls = *(void**)((char*)o + OFF_CLASS);
  if (!cls || !addr_mapped(cls)) return 0;
  int nameidx = *(int*)((char*)cls + 0x48);
  if (nameidx < 0 || nameidx > 300000) return 0;
  unsigned char fs[16] = {0};
  ((void(*)(void*, void*))GETNAME_ADDR)(fs, cls);
  wchar_t* d = *(wchar_t**)fs; int c = *(int*)(fs + 8);
  if (!d || c <= 0 || c > 200) return 0;
  int k = c < n - 1 ? c : n - 1;
  for (int i = 0; i < k; i++) out[i] = (char)(d[i] & 0x7f);
  out[k] = 0;
  return 1;
}

static int name_equals(void* obj, const char* want)
{
  if (!obj || !addr_mapped(obj)) return 0;
  unsigned char fs[16] = {0};
  ((void(*)(void*, void*))GETNAME_ADDR)(fs, obj);
  wchar_t* d = *(wchar_t**)fs; int c = *(int*)(fs + 8);
  if (!d || c <= 0 || c > 200) return 0;
  int n = (int)strlen(want);
  if (c != n) return 0;
  for (int i = 0; i < n; i++) if ((wchar_t)(unsigned char)want[i] != (d[i] & 0xff)) return 0;
  return 1;
}

static int is_uobject(void* o)
{
  if (!o || !addr_mapped(o)) return 0;
  void* cls = *(void**)((char*)o + OFF_CLASS);
  if (!cls || !addr_mapped(cls)) return 0;
  void* meta = *(void**)((char*)cls + OFF_CLASS);
  if (!meta || !addr_mapped(meta)) return 0;
  return name_equals(meta, "Class");
}

static int path_of(void* obj, char* out, int n)
{
  if (!obj || !addr_mapped(obj)) { out[0]=0; return 0; }
  unsigned char fs[16] = {0};
  ((void(*)(void*, void*, void*))GETPATHNAME)(fs, obj, NULL);
  wchar_t* d = *(wchar_t**)fs; int c = *(int*)(fs + 8);
  if (!d || c <= 0) { out[0]=0; return 0; }
  int k = c < n-1 ? c : n-1;
  for (int i=0;i<k;i++) out[i]=(char)(d[i]&0x7f);
  out[k]=0; return k;
}

static int  g_follow_mode;
static int  g_swapcomp_mode;
static int  g_swapnull_mode;
static int  g_swapm_mode;
static int  g_lsn_mode;
static int  g_seqs_mode;
static int  g_bind_mode;
static int  g_maptest_mode;
static int  g_poseprobe_mode;
static int  g_match_mode;
static int  g_tn_mode;
static int  g_b2_mode;
static int  g_b3_mode;
static int  g_mats_mode;
static int  g_gpu8_mode;
static int  g_tex_mode;
static int  g_tex2_mode;
static int  g_retarget_mode;
static int  g_rb_mode;
static int  g_gb_mode;
static int  g_puppet_mode;
static int  g_pose_mode;
static int  g_comps_mode;
static int  g_classbs_mode;
static int  g_vis_mode;
static int  g_mimic_mode;
static int  g_banim_mode;
static int  g_meshonly_mode;
static int  g_archbs_mode;
static int  g_driv_mode;
static int  g_fire_mode;
static int  g_bossanim_mode;
static void* g_ba_pawn;
static int  g_visactor_mode;
static int  g_demoboss_mode;
static int  g_matvis_mode;
static void* g_demo_fire;
static int g_demo_face;
static int g_demo_yaw_flip;
static int g_demo_boss = 1;
static char g_demo_prefix[8] = "Bs01_";
static char g_demo_idle[48] = "Bs01_Stay";
static char g_demo_run[48] = "Bs01_Run";
static char g_demo_jumpup[48] = "Bs01_JumpUp";
static char g_demo_jumpdn[48] = "Bs01_JumpDown";
static char g_demo_land[48] = "Bs01_JumpLanding";
static int g_demo_has_run = 1, g_demo_has_jump = 1, g_demo_has_land = 1;
static void* g_demo_anim; static int g_demo_pick;
static int g_demo_idle_exact;
static int g_demo_has_attack, g_demo_atk, g_demo_atk_playing, g_demo_atk_cd;
static char g_demo_attack[48];
static void* g_demo_seen[24]; static int g_demo_seen_i, g_demo_shell_primed;
static void* g_demo_buster_class;
static void* g_demo_cls;
static void* g_demo_comp;
static void* g_demo_node;
static void* g_demo_run_seq; static int g_demo_run_link = -1;
static void* g_demo_stay_seq; static int g_demo_stay_link = -1;
static int g_demo_off_seq, g_demo_off_name, g_demo_off_link;
static int g_db_lo,g_db_ro,g_db_bl,g_db_br;
static int g_demo_test;
static int g_demo_atktest;
static void* g_va_comp;
static void* g_va_node;
static void* g_swap_keep_mesh;
static void* g_swap_keep_tree;
static void* g_follow_pawn;
static void* g_overlay_pawn;
static void* g_model_pawn;
static FILE* g_logf;
#define LOG(...) do { fprintf(stderr, __VA_ARGS__); if (g_logf) { fprintf(g_logf, __VA_ARGS__); fflush(g_logf); } } while (0)

static uint64_t fname(const char* s)
{
  uint64_t out = 0;
  ((fname_fn)FNAME_CTOR)(&out, s, 0);
  return out;
}

static int prop_offset(void* obj, const char* name)
{
  uint64_t fn = fname(name);
  if (!fn || !obj) return -1;
  if (!addr_mapped(obj)) return -1;
  if (!addr_mapped((char*)obj + OFF_CLASS)) return -1;
  void* __cls = *(void**)((char*)obj + OFF_CLASS);
  if (!__cls || !addr_mapped(__cls)) return -1;
  if (!addr_mapped((char*)__cls + OFF_CLASS)) return -1;
  void* __meta = *(void**)((char*)__cls + OFF_CLASS);
  if (!__meta || !addr_mapped(__meta)) return -1;
  void* field = ((findfield_fn)FINDFIELD)(obj, fn, 0);
  if (!field) return -1;
  int off = *(int*)((char*)field + OFF_PROP);
  return (off > 0 && off < 0x100000) ? off : -1;
}

// build "MN9Game.Default__MN9Spw09Pawn" style path from the selection name
static void arch_path(char* sel, wchar_t* out, int outn)
{
  const char* grp = "Spw";
  if (!strncmp(sel, "bs", 2)) grp = "Bs";
  else if (sel[0] == 'b' && sel[1] == 'e') grp = "Spw";   // beck -> player class
  wchar_t wgrp[16]; int wl = 0;
  for (const char* p = grp; *p; p++) wgrp[wl++] = (wchar_t)*p;
  wgrp[wl] = 0;
  // selection suffix digits (megaxel is the Spw11 archetype)
  wchar_t digits[8]; int dl = 0;
  if (!strcmp(sel, "megaxel")) { digits[dl++] = '1'; digits[dl++] = '1'; digits[dl] = 0; }
  else {
    for (const char* p = sel; *p; p++) if (*p >= '0' && *p <= '9') digits[dl++] = (wchar_t)*p;
    digits[dl] = 0;
  }
  // Call has her own pawn archetype (Pl02), not a SpwNN number.
  if (!strcmp(sel, "call")) {
    const wchar_t* lit = L"MN9Game.Default__MN9Pl02Pawn";
    int i = 0;
    for (; lit[i]; i++) out[i] = lit[i];
    out[i] = 0;
    return;
  }
  // beck = default pawn class itself; else arch of that character
  if (!strcmp(sel, "beck")) {
    const wchar_t* lit = L"MN9Game.Default__MN9Spw09Pawn";
    int i = 0;
    for (; lit[i]; i++) out[i] = lit[i];
    out[i] = 0;
    return;
  }
  const wchar_t* pre = L"MN9Game.Default__MN9";
  int k = 0;
  for (int i = 0; pre[i]; i++) out[k++] = pre[i];
  for (int i = 0; wgrp[i]; i++) out[k++] = wgrp[i];
  for (int i = 0; digits[i]; i++) out[k++] = digits[i];
  const wchar_t* suf = L"Pawn";
  for (int i = 0; suf[i]; i++) out[k++] = suf[i];
  out[k] = 0;
  (void)outn;
}

static int obj_name_is(void* obj, const wchar_t* want)
{
  fstring_t fs = {0,0,0};
  ((getname_fn)GETNAME_ADDR)(&fs, obj);
  if (!fs.data || fs.count <= 0) return 0;
  int i = 0;
  for (; want[i]; i++) {
    if (i >= fs.count || fs.data[i] != want[i]) return 0;
  }
  return i == fs.count;
}

// Build "MN9Game.MN9<Group><nn>Pawn" for the selected character.
static void spawn_class_path(const char* sel, wchar_t* out, int outn)
{
  const char* grp = "Spw";
  if (!strncmp(sel, "bs", 2)) grp = "Bs";
  else if (!strcmp(sel, "megaxel")) grp = "Spw";
  wchar_t w[80]; int k = 0;
  const wchar_t* pre = L"MN9Game.MN9";
  for (int i = 0; pre[i]; i++) w[k++] = pre[i];
  for (const char* q = grp; *q && k < 60; q++) w[k++] = (wchar_t)*q;
  if (!strcmp(sel, "megaxel")) { w[k++] = '1'; w[k++] = '1'; }
  else {
    for (const char* q = sel; *q && k < 70; q++)
      if (*q >= '0' && *q <= '9') w[k++] = (wchar_t)*q;
  }
  const wchar_t* suf = L"Pawn";
  for (int i = 0; suf[i]; i++) w[k++] = suf[i];
  w[k] = 0;
  int n = 0;
  for (; w[n] && n < outn - 1; n++) out[n] = w[n];
  out[n] = 0;
}

static void* find_loaded_named(const char* want);

static int resolve_pawn_slots(void)
{
  if (g_arch) return 1;
  findobj_fn find = (findobj_fn)FINDOBJ_ADDR;
  g_pc_class = g_pc_class ? g_pc_class : find(NULL, NULL, L"MN9Game.MN9Pl01Controller", 0);
  if (!g_pc_class) return 0;

  wchar_t ap[96];
  if (!strcmp(g_selection, "ray")) {
    // Ray has no player pawn class in MN9Game.  Use Beck's archetype only for
    // component identification and take the body/arm meshes from the map.
    void* body = find_loaded_named("PL03_Body");
    void* lhand = find_loaded_named("PL03_LHand");
    void* rclaw = find_loaded_named("PL03_RClaw");
    if (!body) return 0;   // wait for the map package
    g_arch = find(NULL, NULL, L"MN9Game.Default__MN9Spw09Pawn", 0);
    if (!g_arch) return 0;
    g_arch_slot[0] = g_arch_slot[1] = g_arch_slot[2] = body;
    g_arch_slot[3] = g_arch_slot[4] = lhand ? lhand : body;
    g_arch_slot[5] = g_arch_slot[6] = rclaw ? rclaw : body;
    LOG("[mn9pawn] ray slots body=%p lhand=%p rclaw=%p\n", body, lhand, rclaw);
  } else {
    arch_path(g_selection, ap, 96);
    g_arch = find(NULL, NULL, ap, 0);
    if (!g_arch) {
      LOG("[mn9pawn] archetype %ls not found\n", ap);
      return 0;
    }
    for (int i = 0; i < 7; i++) {
      int off = prop_offset(g_arch, SLOT_PROPS[i]);
      g_arch_slot[i] = off > 0 ? *(void**)((char*)g_arch + off) : NULL;
    }
  }
  if (!g_beck_mesh[0]) {
    loadobj_fn load = (loadobj_fn)LOADOBJ_ADDR;
    void* skclass = find(NULL, NULL, L"Engine.SkeletalMesh", 0);
    static const wchar_t* beck_names[7] = {
      L"Spw09_Body2", L"Spw09_Body2D", L"Spw09_LHand1",
      L"Spw09_LShot1", L"Spw09_RHand1", L"Spw09_RShot1", L"Spw10_Body1"
    };
    static const wchar_t* bpre[] = { L"Spw09_Model.Mesh.", L"MN9Game.Mesh.", L"MN9Game.", NULL };
    if (skclass) for (int i = 0; i < 7; i++) {
      for (int pi = 0; bpre[pi] && !g_beck_mesh[i]; pi++) {
        wchar_t path[128]; int wl = 0;
        for (int k = 0; bpre[pi][k]; k++) path[wl++] = bpre[pi][k];
        for (int k = 0; beck_names[i][k]; k++) path[wl++] = beck_names[i][k];
        path[wl] = 0;
        g_beck_mesh[i] = load(skclass, NULL, path, NULL, 0, NULL, 0);
      }
    }
    LOG("[mn9pawn] beck atlas %p %p %p %p %p %p %p\n",
        g_beck_mesh[0], g_beck_mesh[1], g_beck_mesh[2], g_beck_mesh[3],
        g_beck_mesh[4], g_beck_mesh[5], g_beck_mesh[6]);
  }
  LOG("[mn9pawn] target archetype %ls found (%p) slots %p %p %p %p %p %p %p\n", ap, g_arch,
      g_arch_slot[0], g_arch_slot[1], g_arch_slot[2], g_arch_slot[3],
      g_arch_slot[4], g_arch_slot[5], g_arch_slot[6]);
  return 1;
}

static int addr_mapped(const void* p)
{
  if ((uintptr_t)p < 0x10000UL) return 0;
  long ps = sysconf(_SC_PAGESIZE);
  unsigned char vec;
  uintptr_t pg = (uintptr_t)p & ~(uintptr_t)(ps - 1);
  return mincore((void*)pg, (size_t)ps, &vec) == 0;
}

static void arm_spawn_stub(void*, void*, void*);
static int install_spawn_hook(unsigned char*);

static void* find_player_pawn(void)
{
  // Fast path: cached controller, read its Pawn property (cheap, per tick).
  if (g_ctrl && g_ctrl_pawn_off > 0 && addr_mapped(g_ctrl)) {
    void* p = *(void**)((char*)g_ctrl + g_ctrl_pawn_off);
    if (p && addr_mapped(p) && g_pawn_ctrl_off > 0) {
      // ensure the pawn still belongs to the cached controller
      if (*(void**)((char*)p + g_pawn_ctrl_off) == g_ctrl) return p;
    }
    g_ctrl = NULL;
    g_ctrl_pawn_off = 0;
    g_pawn_ctrl_off = 0;
  }
  findobj_fn find = (findobj_fn)FINDOBJ_ADDR;
  void* pcclass = g_pc_class ? g_pc_class : find(NULL, NULL, L"MN9Game.MN9Pl01Controller", 0);
  if (!pcclass) return NULL;
  g_pc_class = pcclass;
  void** objs = *(void***)OBJ_ARRAY;
  int count = *(int*)OBJ_COUNT;
  if (!objs || count <= 0 || count > 400000) return NULL;
  int low = count - 20000; if (low < 0) low = 0;
  for (int i = count - 1; i >= low; i--) {
    if (!addr_mapped(objs + i)) break;
    void* o = objs[i];
    if (!o || !addr_mapped(o)) continue;
    if (*(void**)((char*)o + OFF_CLASS) != pcclass) continue;
    int po = prop_offset(o, "Pawn");
    if (po <= 0) continue;
    void* pawn = *(void**)((char*)o + po);
    if (!pawn) continue;
    g_ctrl = o;
    g_ctrl_pawn_off = po;
    g_pawn_ctrl_off = prop_offset(pawn, "Controller");
    LOG("[mn9pawn] controller cached=%p pawn_off=%d pctrl_off=%d\n", o, po, g_pawn_ctrl_off);
    return pawn;
  }
  return NULL;
}

static void comps_dump(void* pawn)
{
  int comps_off = prop_offset(pawn, "Components");
  if (comps_off <= 0) return;
  void** arr = *(void***)(comps_off + (char*)pawn);
  int n = *(int*)(comps_off + 8 + (char*)pawn);
  fprintf(stderr, "[mn9pawn] pawn=%p comps n=%d sk_off=%d\n", pawn, n, g_sk_off);
  for (int c = 0; arr && c < n && c < 16; c++) {
    void* comp = arr[c];
    if (!comp) continue;
    fprintf(stderr, "[mn9pawn]  comp[%d]=%p cls=%p sk=%p\n", c, comp,
            *(void**)((char*)comp + OFF_CLASS),
            g_sk_off > 0 ? *(void**)((char*)comp + g_sk_off) : NULL);
  }
}

typedef void (*fname_str_fn)(uint64_t* self, void* out);

static int fn_to_str(uint64_t fn, char* out, int outn)
{
  unsigned char fs[16] = {0};
  ((fname_str_fn)FNAME_TOSTRING)(&fn, fs);
  wchar_t* data = *(wchar_t**)fs;
  int count = *(int*)(fs + 8);
  if (!data || count <= 0) { out[0] = 0; return 0; }
  int n = count < outn - 1 ? count : outn - 1;
  for (int i = 0; i < n; i++) out[i] = (char)(data[i] & 0x7f);
  out[n] = 0;
  return n;
}

static int mesh_bone_count(void* mesh)
{
  if (!mesh || !addr_mapped(mesh)) return 0;
  int n = *(int*)((char*)mesh + 0xe8);
  return (n > 0 && n < 4096) ? n : 0;
}

static void mesh_bone_name(void* mesh, int i, char* out, int outn, int* parent)
{
  void* data = *(void**)((char*)mesh + 0xe0);
  *parent = -1;
  if (!data || !addr_mapped(data)) { out[0]=0; return; }
  char* b = (char*)data + i * 20;
  uint64_t fn = *(uint64_t*)b;
  *parent = *(int*)(b + 16);
  fn_to_str(fn, out, outn);
}

static void dump_bones(void* mesh, const char* tag, char names[][48], int* count, int maxn)
{
  int n = mesh_bone_count(mesh);
  LOG("[mn9pawn] %s bones=%d\n", tag, n);
  int m = n < maxn ? n : maxn;
  for (int i = 0; i < m; i++) {
    int parent;
    mesh_bone_name(mesh, i, names[i], 48, &parent);
    LOG("[mn9pawn] %s bone[%d] '%s' parent=%d\n", tag, i, names[i], parent);
  }
  *count = m;
}

static int obj_name_str(void* o, char* out, int n)
{
  out[0] = 0;
  if (!o || !addr_mapped(o)) return 0;
  unsigned char fs[16] = {0};
  ((void(*)(void*, void*))GETNAME_ADDR)(fs, o);
  wchar_t* d = *(wchar_t**)fs; int c = *(int*)(fs + 8);
  if (!d || c <= 0 || c > 200) return 0;
  int k = c < n - 1 ? c : n - 1;
  for (int i = 0; i < k; i++) out[i] = (char)(d[i] & 0x7f);
  out[k] = 0;
  return k;
}

static void* find_loaded_named(const char* want)
{
  void** objs = *(void***)OBJ_ARRAY;
  int count = *(int*)OBJ_COUNT;
  if (!objs || count <= 0 || count > 400000) return NULL;
  int low = count - 250000; if (low < 0) low = 0;
  char nm[160];
  for (int i = count - 1; i >= low; i--) {
    if (!addr_mapped(objs + i)) break;
    void* o = objs[i];
    if (!obj_name_str(o, nm, sizeof nm)) continue;
    if (!strcmp(nm, want)) return o;
  }
  return NULL;
}

static void* find_loaded_by_name(const char* name)
{
  uint64_t fn = fname(name);
  if (!fn) return NULL;
  void** objs = *(void***)OBJ_ARRAY;
  int count = *(int*)OBJ_COUNT;
  if (!objs || count <= 0 || count > 400000) return NULL;
  int low = count - 250000; if (low < 0) low = 0;
  for (int i = count - 1; i >= low; i--) {
    if (!addr_mapped(objs + i)) break;
    void* o = objs[i];
    if (!o || !addr_mapped(o)) continue;
    if (*(uint64_t*)((char*)o + 0x48) == fn) return o;
  }
  return NULL;
}

static void overlay_tick(void* world)
{
  void* pawn = find_player_pawn();
  if (!pawn || !world) return;
  int lo = prop_offset(pawn, "Location");
  int ro = prop_offset(pawn, "Rotation");
  if (lo <= 0 || ro <= 0) return;

  if (!g_overlay_pawn) {
    findobj_fn find = (findobj_fn)FINDOBJ_ADDR;
    wchar_t cp[96]; spawn_class_path(g_selection + 8, cp, 96);
    void* cls = find(NULL, NULL, cp, 0);
    if (!cls) return;
    spawn_fn sp = (spawn_fn)SPAWNACTOR_ADDR;
    g_overlay_pawn = sp(world, cls, 0, (char*)pawn + lo, (char*)pawn + ro,
                        NULL, 1, 0, NULL, NULL, 0, NULL);
    LOG("[mn9pawn] overlay spawn %ls -> %p\n", cp, g_overlay_pawn);
    if (!g_overlay_pawn) return;
    int ds = prop_offset(g_overlay_pawn, "DrawScale");
    if (ds > 0) *(float*)((char*)g_overlay_pawn + ds) = 0.45f;
    // hide Beck locally (owner-only) on the three visible comps
    static const char* cp3[3] = { "Mesh", "ArmLMeshComponent", "ArmRMeshComponent" };
    for (int i = 0; i < 3; i++) {
      int o = prop_offset(pawn, cp3[i]);
      if (o <= 0) continue;
      void* comp = *(void**)((char*)pawn + o);
      if (!comp) continue;
      int bo = prop_offset(comp, "bOwnerNoSee");
      if (bo > 0) *(unsigned char*)((char*)comp + bo) = 1;
    }
    LOG("[mn9pawn] overlay: player mesh hidden (local)\n");
    // force the boss actor's mesh components visible and log their contents
    void** objs = *(void***)OBJ_ARRAY;
    int cnt = *(int*)OBJ_COUNT;
    int low = cnt - 25000; if (low < 0) low = 0;
    int found = 0;
    for (int i = cnt - 1; i >= low; i--) {
      if (!addr_mapped(objs + i)) break;
      void* o = objs[i];
      if (!o || !addr_mapped(o)) continue;
      int ow = prop_offset(o, "Owner");
      if (ow <= 0 || *(void**)((char*)o + ow) != g_overlay_pawn) continue;
      int sk = prop_offset(o, "SkeletalMesh");
      if (sk <= 0) continue;
      found++;
      void* m = *(void**)((char*)o + sk);
      int hid = prop_offset(o, "bHidden");
      if (hid > 0) *(unsigned char*)((char*)o + hid) = 0;
      int ons = prop_offset(o, "bOwnerNoSee");
      if (ons > 0) *(unsigned char*)((char*)o + ons) = 0;
      LOG("[mn9pawn] overlay comp %p mesh=%p\n", o, m);
    }
    LOG("[mn9pawn] overlay: boss comps found=%d\n", found);
  } else {
    if (prop_offset(g_overlay_pawn, "Location") <= 0) return;
    *(void**)((char*)g_overlay_pawn + prop_offset(g_overlay_pawn,"Location")) = *(void**)((char*)pawn + lo);
    // copy full vectors (12 bytes each)
    memcpy((char*)g_overlay_pawn + prop_offset(g_overlay_pawn,"Location"),
           (char*)pawn + lo, 12);
    int orot = prop_offset(g_overlay_pawn, "Rotation");
    if (orot > 0) memcpy((char*)g_overlay_pawn + orot, (char*)pawn + ro, 12);
  }
}

static void model_tick(void)
{
  void* pawn = find_player_pawn();
  if (!pawn) return;
  int mo = prop_offset(pawn, "Mesh");
  if (mo <= 0) return;
  void* comp = *(void**)((char*)pawn + mo);
  if (!comp) return;
  static void* mesh;
  if (!mesh) {
    mesh = find_loaded_by_name("Bs01_Body1");
    if (!mesh) {
      findobj_fn find = (findobj_fn)FINDOBJ_ADDR;
      loadobj_fn load = (loadobj_fn)LOADOBJ_ADDR;
      void* skclass = find(NULL, NULL, L"Engine.SkeletalMesh", 0);
      static const wchar_t* paths[] = {
        L"Bs01_model.Mesh.Bs01_Body1", L"MN9Game.Mesh.Bs01_Body1", NULL };
      for (int i = 0; skclass && paths[i] && !mesh; i++) {
        mesh = load(skclass, NULL, paths[i], NULL, 0, NULL, 0);
        if (mesh) LOG("[mn9pawn] model: loaded %ls\n", paths[i]);
      }
    }
    LOG("[mn9pawn] model: mesh=%p\n", mesh);
    if (mesh) {
      unsigned char fs[16] = {0};
      ((void(*)(void*, void*, void*))GETPATHNAME)(fs, mesh, NULL);
      wchar_t* d = *(wchar_t**)fs; int n = *(int*)(fs + 8);
      if (d && n > 0) {
        char b[256]; int k = n < 240 ? n : 240;
        for (int i = 0; i < k; i++) b[i] = (char)(d[i] & 0x7f);
        b[k] = 0;
        LOG("[mn9pawn] model path: %s\n", b);
      }
    }
  }
  if (!mesh) return;
  int sk = prop_offset(comp, "SkeletalMesh");
  if (sk > 0 && *(void**)((char*)comp + sk) != mesh) {
    ((setskmesh_fn)SETSKMESH)(comp, mesh, 1);
    LOG("[mn9pawn] model: set boss mesh on player comp\n");
  }
}


/* ---- 3x4 row-major matrix helpers for bone retargeting ---- */
static void atm(const float* a, float M[12]) {
  float x=a[0],y=a[1],z=a[2],w=a[3];
  float xx=x*x,yy=y*y,zz=z*z,xy=x*y,xz=x*z,yz=y*z,wx=w*x,wy=w*y,wz=w*z;
  M[0]=1-2*(yy+zz); M[1]=2*(xy-wz);   M[2]=2*(xz+wy);  M[3]=a[4];
  M[4]=2*(xy+wz);   M[5]=1-2*(xx+zz); M[6]=2*(yz-wx);  M[7]=a[5];
  M[8]=2*(xz-wy);   M[9]=2*(yz+wx);   M[10]=1-2*(xx+yy); M[11]=a[6];
}
static void mmul(const float A[12], const float B[12], float O[12]) {
  for (int r=0;r<3;r++) {
    for (int c=0;c<3;c++)
      O[r*4+c]=A[r*4+0]*B[0*4+c]+A[r*4+1]*B[1*4+c]+A[r*4+2]*B[2*4+c];
    O[r*4+3]=A[r*4+0]*B[3]+A[r*4+1]*B[7]+A[r*4+2]*B[11]+A[r*4+3];
  }
}
static void minv(const float A[12], float O[12]) {
  O[0]=A[0]; O[1]=A[4]; O[2]=A[8];
  O[4]=A[1]; O[5]=A[5]; O[6]=A[9];
  O[8]=A[2]; O[9]=A[6]; O[10]=A[10];
  O[3]=-(O[0]*A[3]+O[1]*A[7]+O[2]*A[11]);
  O[7]=-(O[4]*A[3]+O[5]*A[7]+O[6]*A[11]);
  O[11]=-(O[8]*A[3]+O[9]*A[7]+O[10]*A[11]);
}
static void mtoa(const float M[12], float* a) {
  float tr=M[0]+M[5]+M[10];
  float x,y,z,w;
  if (tr>0) { float s=sqrtf(tr+1.0f)*2; w=0.25f*s; x=(M[9]-M[6])/s; y=(M[2]-M[8])/s; z=(M[4]-M[1])/s; }
  else if (M[0]>M[5] && M[0]>M[10]) { float s=sqrtf(1.0f+M[0]-M[5]-M[10])*2; w=(M[9]-M[6])/s; x=0.25f*s; y=(M[1]+M[4])/s; z=(M[2]+M[8])/s; }
  else if (M[5]>M[10]) { float s=sqrtf(1.0f+M[5]-M[0]-M[10])*2; w=(M[2]-M[8])/s; x=(M[1]+M[4])/s; y=0.25f*s; z=(M[6]+M[9])/s; }
  else { float s=sqrtf(1.0f+M[10]-M[0]-M[5])*2; w=(M[4]-M[1])/s; x=(M[2]+M[8])/s; y=(M[6]+M[9])/s; z=0.25f*s; }
  a[0]=x;a[1]=y;a[2]=z;a[3]=w;a[4]=M[3];a[5]=M[7];a[6]=M[11];a[7]=1;a[8]=1;a[9]=1;
}


static void demo_append_set(void* comp)
{
  char setname[64]; snprintf(setname, sizeof setname, "%sAnimSet", g_demo_prefix);
  void* aset = find_loaded_named(setname);
  if (!aset) { ((void*(*)(void*, const wchar_t*, unsigned))0x0da0490UL)(NULL,L"Map14_Factory_Persistent00",0); aset = find_loaded_named(setname); }
  int ao = prop_offset(comp, "AnimSets");
  if (!aset || ao <= 0) { LOG("[mn9pawn] demoanim append: aset=%p ao=%d\n", aset, ao); return; }
  void** arr = *(void***)((char*)comp + ao);
  int n = *(int*)((char*)comp + ao + 8), mx = *(int*)((char*)comp + ao + 12);
  int have = 0;
  for (int i = 0; i < n; i++) if (arr[i] == aset) have = 1;
  if (!have) {
    if (n >= mx) { int nm2 = mx ? mx * 2 : 8; arr = (void**)((void*(*)(void*, unsigned, unsigned))0xdb78a0UL)(arr, nm2 * 8, 8); *(void***)((char*)comp + ao) = arr; *(int*)((char*)comp + ao + 12) = nm2; }
    arr[n] = aset; *(int*)((char*)comp + ao + 8) = n + 1;
    LOG("[mn9pawn] demoanim animset added (%d)\n", n + 1);
  }
  int anoff = prop_offset(comp, "Animations"); if (anoff <= 0) anoff = 0x268;
  void* ai = *(void**)((char*)comp + anoff);
  if (!ai || !addr_mapped(ai)) { LOG("[mn9pawn] demoanim no Animations instance\n"); return; }
  struct { void** data; int count; int max; } qa = {0,0,0};
  ((void(*)(void*, void*, unsigned))0x17cc440UL)(ai, &qa, 1);
  int linked = 0, total = 0;
  for (int i = 0; qa.data && i < qa.count; i++) {
    void* nd = qa.data[i]; if (!nd || !addr_mapped(nd)) continue;
    char cn[96] = "?"; class_name_of(nd, cn, sizeof cn);
    if (!strstr(cn, "AnimNodeSequence")) continue;
    int no = prop_offset(nd, "AnimSeqName"); if (no <= 0) continue;
    uint64_t fn = *(uint64_t*)((char*)nd + no);
    total++;
    if (!fn) continue;
    ((void(*)(void*, uint64_t))0x1198f30UL)(nd, fn);
    int so = prop_offset(nd, "AnimSeq"); void* sq = so > 0 ? *(void**)((char*)nd + so) : NULL;
    if (sq) linked++;
  }
  LOG("[mn9pawn] demoanim linked %d/%d sequence nodes\n", linked, total);
  // authoritative clip names from the AnimSet itself
  g_demo_has_run = 0; g_demo_has_jump = 0; g_demo_has_land = 0; g_demo_idle_exact = 0;
  int sqo = prop_offset(aset, "Sequences");
  void** sq = sqo > 0 ? *(void***)((char*)aset + sqo) : NULL;
  int sqn = sqo > 0 ? *(int*)((char*)aset + sqo + 8) : 0;
  char want[8][64]; const char* sf[8] = {"Stay","Run","JumpUp","JumpStart","JumpDown","JumpLanding","FlyMove","CateRun"};
  for (int i=0;i<8;i++) snprintf(want[i],64,"%s%s",g_demo_prefix,sf[i]);
  char atk1[48]="",atk2[48]="",atk3[48]="",atk4[48]="";
  for (int i = 0; sq && i < sqn && i < 200; i++) {
    void* se = sq[i]; if (!se || !addr_mapped(se)) continue;
    int no = prop_offset(se, "SequenceName"); if (no <= 0) continue;
    char nm[96]=""; fn_to_str(*(uint64_t*)((char*)se+no), nm, sizeof nm);
    if (!strcasecmp(nm, want[0])) { snprintf(g_demo_idle,sizeof g_demo_idle,"%s",nm); g_demo_idle_exact=1; }
    else if (!strcasecmp(nm, want[1])) { snprintf(g_demo_run,sizeof g_demo_run,"%s",nm); g_demo_has_run=1; }
    else if (!strcasecmp(nm, want[2]) || !strcasecmp(nm, want[3])) { snprintf(g_demo_jumpup,sizeof g_demo_jumpup,"%s",nm); g_demo_has_jump=1; }
    else if (!strcasecmp(nm, want[4])) { snprintf(g_demo_jumpdn,sizeof g_demo_jumpdn,"%s",nm); }
    else if (!strcasecmp(nm, want[5])) { snprintf(g_demo_land,sizeof g_demo_land,"%s",nm); g_demo_has_land=1; }
    else if (!g_demo_has_run && (!strcasecmp(nm, want[6]) || !strcasecmp(nm, want[7]))) { snprintf(g_demo_run,sizeof g_demo_run,"%s",nm); g_demo_has_run=1; }
    if (!strncasecmp(nm, g_demo_prefix, strlen(g_demo_prefix))) {
      const char* sfx = nm + strlen(g_demo_prefix);
      int excl = strcasestr(sfx,"Herald")||strcasestr(sfx,"Return")||strcasestr(sfx,"Start")||strcasestr(sfx,"Landing")||strcasestr(sfx,"Loop")||strcasestr(sfx,"End");
      if (!strcasecmp(sfx,"LariatAttack")||!strcasecmp(sfx,"FrozenShot")) { if(!atk1[0]) snprintf(atk1,sizeof atk1,"%s",nm); }
      else if (!excl && strcasestr(sfx,"Attack") && !atk2[0]) snprintf(atk2,sizeof atk2,"%s",nm);
      else if (!excl && strcasestr(sfx,"Shot") && !atk3[0]) snprintf(atk3,sizeof atk3,"%s",nm);
      else if (!strcasecmp(sfx,"CutStart") && !atk4[0]) snprintf(atk4,sizeof atk4,"%s",nm);
    }
  }
  if (atk1[0]) { snprintf(g_demo_attack,sizeof g_demo_attack,"%s",atk1); g_demo_has_attack=1; }
  else if (atk2[0]) { snprintf(g_demo_attack,sizeof g_demo_attack,"%s",atk2); g_demo_has_attack=1; }
  else if (atk3[0]) { snprintf(g_demo_attack,sizeof g_demo_attack,"%s",atk3); g_demo_has_attack=1; }
  else if (atk4[0]) { snprintf(g_demo_attack,sizeof g_demo_attack,"%s",atk4); g_demo_has_attack=1; }
  g_demo_has_attack = g_demo_attack[0] ? 1 : 0;
  LOG("[mn9pawn] demoanim clips idle=%s run=%s hasRun=%d hasJump=%d\n", g_demo_idle, g_demo_run, g_demo_has_run, g_demo_has_jump);
}

static void demoboss_anim_setup(void* comp, void* tree)
{
  g_demo_comp = comp; g_demo_node = NULL;
  g_demo_anim = NULL; g_demo_pick = 40; g_demo_atk = 0; g_demo_atk_playing = 0; g_demo_atk_cd = 0; g_demo_shell_primed = 0; g_demo_seen_i = 0;
  if (!comp || !tree) return;
  int anoff = prop_offset(comp, "Animations"); if (anoff <= 0) anoff = 0x268;
  void* ai = *(void**)((char*)comp + anoff);
  if (!ai || !addr_mapped(ai)) { LOG("[mn9pawn] demoanim no instance\n"); return; }
  g_demo_anim = ai;
  struct { void** data; int count; int max; } qa = {0,0,0};
  ((void(*)(void*, void*, unsigned))0x17cc440UL)(ai, &qa, 1);
  void* fallback = NULL;
  for (int i = 0; qa.data && i < qa.count; i++) {
    void* nd = qa.data[i]; if (!nd || !addr_mapped(nd)) continue;
    char cn[96] = "?"; class_name_of(nd, cn, sizeof cn);
    if (!strstr(cn, "AnimNodeSequence")) {
      if (g_demo_boss != 1 && strstr(cn, "Blend")) {
        int po = prop_offset(nd, "PropertyName"); char pn2[96] = "";
        if (po > 0) fn_to_str(*(uint64_t*)((char*)nd + po), pn2, sizeof pn2);
        LOG("[mn9pawn] demoanim blend node=%s prop=%s\n", cn, pn2);
      }
      continue;
    }
    int no = prop_offset(nd, "AnimSeqName"); char sn[96] = "";
    if (no > 0) fn_to_str(*(uint64_t*)((char*)nd + no), sn, sizeof sn);
    int bo = prop_offset(nd, "bPlaying"); int bp = bo > 0 ? *(unsigned char*)((char*)nd + bo) : -1;
    if (!strncasecmp(sn, g_demo_prefix, strlen(g_demo_prefix))) {
      const char* sfx = sn + strlen(g_demo_prefix);
      if (!strcasecmp(sfx, "Stay")) { snprintf(g_demo_idle, sizeof g_demo_idle, "%s", sn); g_demo_idle_exact = 1; if (!g_demo_node) { g_demo_node = nd; LOG("[mn9pawn] demoanim idle node=%p seq=%s\n", nd, sn); } }
      else if (!strcasecmp(sfx, "Run")) { snprintf(g_demo_run, sizeof g_demo_run, "%s", sn); g_demo_has_run = 1; }
      else if (!strcasecmp(sfx, "JumpUp") || !strcasecmp(sfx, "JumpStart")) { snprintf(g_demo_jumpup, sizeof g_demo_jumpup, "%s", sn); g_demo_has_jump = 1; }
      else if (!strcasecmp(sfx, "JumpDown")) { snprintf(g_demo_jumpdn, sizeof g_demo_jumpdn, "%s", sn); }
      else if (!strcasecmp(sfx, "JumpLanding") || !strcasecmp(sfx, "JumpLandingReturn")) { snprintf(g_demo_land, sizeof g_demo_land, "%s", sn); g_demo_has_land = 1; }
      else if (!g_demo_has_run && (!strcasecmp(sfx, "FlyMove") || !strcasecmp(sfx, "CateRun") || !strcasecmp(sfx, "DashLoop") || !strcasecmp(sfx, "Move"))) { snprintf(g_demo_run, sizeof g_demo_run, "%s", sn); g_demo_has_run = 1; }
    } else if (!g_demo_node && strstr(sn, "Stay")) {
      snprintf(g_demo_idle, sizeof g_demo_idle, "%s", sn);
      g_demo_node = nd;
      LOG("[mn9pawn] demoanim idle fallback node=%p seq=%s\n", nd, sn);
    }
    if (!fallback && (bp & 1)) fallback = nd;
  }
  if (!g_demo_node) g_demo_node = fallback;
  LOG("[mn9pawn] demoanim drive node=%p\n", g_demo_node);
}

static void demo_play_clip(const char* nm, int loop)
{
  if (!g_demo_node || !addr_mapped(g_demo_node)) return;
  ((void(*)(void*, uint64_t))0x1198f30UL)(g_demo_node, fname(nm));
  ((void(*)(void*, unsigned, float, float))0x119f450UL)(g_demo_node, (unsigned)(loop ? 1 : 0), 1.0f, 0.0f);
  int so = prop_offset(g_demo_node, "AnimSeq");
  void* sq = so > 0 ? *(void**)((char*)g_demo_node + so) : NULL;
  int ct = prop_offset(g_demo_node, "CurrentTime");
  LOG("[mn9pawn] demoanim clip -> %s loop=%d seq=%p ct=%.2f\n", nm, loop, sq, ct > 0 ? *(float*)((char*)g_demo_node + ct) : -1.0f);
}

static void demo_set_clip(const char* nm) { demo_play_clip(nm, 1); }

static void demoboss_anim_drive(void* pn, void* act)
{
  if (!g_demo_node || !addr_mapped(g_demo_node)) return;
  (void)act;
  static int state = -1, vo = 0, land = 0;
  static unsigned tk;
  if (g_demo_atk_cd > 0) g_demo_atk_cd--;
  if (g_demo_pick > 0 && g_demo_anim && addr_mapped(g_demo_anim)) {
    g_demo_pick--;
    if ((tk % 5) == 0) {
      struct { void** data; int count; int max; } qa = {0,0,0};
      ((void(*)(void*, void*, unsigned))0x17cc440UL)(g_demo_anim, &qa, 1);
      void* best = NULL; float bw = 0; char bsn[96] = "";
      for (int i = 0; qa.data && i < qa.count; i++) {
        void* nd = qa.data[i]; if (!nd || !addr_mapped(nd)) continue;
        char cn[96] = "?"; class_name_of(nd, cn, sizeof cn);
        if (!strstr(cn, "AnimNodeSequence")) continue;
        int wo = prop_offset(nd, "NodeTotalWeight"); float wv = wo > 0 ? *(float*)((char*)nd + wo) : 0;
        if (wv > bw) { bw = wv; best = nd; int no = prop_offset(nd, "AnimSeqName"); if (no > 0) fn_to_str(*(uint64_t*)((char*)nd + no), bsn, sizeof bsn); }
      }
      if (best && bw > 0.05f) {
        g_demo_node = best; g_demo_pick = 0; state = -1;
        if (bsn[0] && !g_demo_idle_exact) snprintf(g_demo_idle, sizeof g_demo_idle, "%s", bsn);
        LOG("[mn9pawn] demoanim active node=%p seq=%s weight=%.2f\n", best, bsn, bw);
        int ow = prop_offset(act, "Owner"); void* own = ow > 0 ? *(void**)((char*)act + ow) : NULL;
        if (own) {
          static const char* st[] = {"AnimActionState","ActionState","bDead","bCollapse","BattleState","AnimState","bBattleStart","State", NULL};
          for (int si=0; st[si]; si++) { int so = prop_offset(own, st[si]); if (so > 0) { unsigned char r[4]; memcpy(r,(char*)own+so,4); LOG("[mn9pawn] demoanim actprop %s @%d = %02x %02x %02x %02x\n", st[si], so, r[0],r[1],r[2],r[3]); } }
        }
      }
    }
  }
  if (g_demo_atktest && g_demo_has_attack && (tk % 120) == 0) g_demo_atk = 22;
  if (g_demo_atk > 0) {
    if (!g_demo_atk_playing && g_demo_has_attack) { g_demo_atk_playing = 1; demo_play_clip(g_demo_attack, 0); LOG("[mn9pawn] demoanim ATTACK %s\n", g_demo_attack); }
    g_demo_atk--;
    if (g_demo_atk <= 0) { g_demo_atk_playing = 0; state = -1; }
    tk++;
    return;
  }
  float sp = 0, vz = 0;
  if (!vo) vo = prop_offset(pn, "Velocity");
  if (vo > 0) {
    float vx = *(float*)((char*)pn + vo), vy = *(float*)((char*)pn + vo + 4);
    vz = *(float*)((char*)pn + vo + 8);
    sp = sqrtf(vx * vx + vy * vy);
  }
  int air = (vz > 220.0f) ? 1 : ((vz < -260.0f) ? 2 : 0);
  if (g_demo_test) air = ((tk / 150) & 1) ? 0 : 0;   // test cycles ground clips only

  int want;
  if (air == 1 && g_demo_has_jump) want = 2;      // jump up
  else if (air == 2 && g_demo_has_jump) want = 3; // falling
  else want = (sp > 150.0f) ? 1 : 0;              // run / stay

  if (g_demo_test) want = (int)((tk / 150) & 1);     // 0 stay, 1 run

  if (want != state) {
    int was_air = (state == 2 || state == 3);
    state = want;
    if ((want == 0 || want == 1) && was_air) land = 20;
    const char* nm = want == 0 ? g_demo_idle : (want == 1 ? (g_demo_has_run ? g_demo_run : g_demo_idle) : (want == 2 ? g_demo_jumpup : g_demo_jumpdn));
    if (land > 0 && (want == 0 || want == 1) && g_demo_has_land) { nm = g_demo_land; land--; }
    demo_set_clip(nm);
    LOG("[mn9pawn] demoanim state -> %d (%s) sp=%.1f vz=%.1f\n", want, nm, sp, vz);
  } else if (land > 0) {
    land--;
    if (land == 0) demo_set_clip(state == 1 && g_demo_has_run ? g_demo_run : g_demo_idle);
  }
  if ((tk % 180) == 0) {
    int ct = prop_offset(g_demo_node, "CurrentTime");
    LOG("[mn9pawn] demoanim progress ct=%.2f state=%d sp=%.1f vz=%.1f\n", ct > 0 ? *(float*)((char*)g_demo_node + ct) : -1.0f, state, sp, vz);
  }
  tk++;
}

/* ---- native character swap: pawn archetype + player controller ----
 *
 * Ported from Xel Launcher 1.0.7-1.4.0 (libmn9spawn.so md5 7b14d970...), which
 * hooked UWorld::SpawnActor before the first world tick.  The stage spawns the
 * player pawn during InitGame, so a tick-based patch is always too late; the
 * old builds substituted, at spawn time:
 *
 *   arg2 (class)   MN9Game.MN9Spw09Pawn    -> MN9Game.MN9Pl02Pawn / MN9GameContent.MN9Pl03Pawn
 *   arg6 (template) Beck archetype          -> Pl02_Archetype.Pl02_Archetype /
 *                                              pl03_archetype.Pawn.Pl03Pawn
 *   arg2 (class)   MN9Game.MN9Pl01Controller-> MN9Game.MN9Pl02Controller /
 *                                              MN9GameContent.MN9Pl03Controller
 */
__attribute__((visibility("hidden"))) void* g_real_spawn;
static int   g_nsp_resolved;
static void* g_nsp_beck_pawn;
static void* g_nsp_beck_ctrl;
static void* g_nsp_pawn;
static void* g_nsp_ctrl;
static void* g_nsp_arch;
static int   g_nsp_log_pawn;
static int   g_nsp_log_ctrl;

static int native_spawn_wanted(void)
{
  return !strcmp(g_selection, "call") || !strcmp(g_selection, "ray");
}

static void native_spawn_resolve(void)
{
  if (g_nsp_resolved) return;
  findobj_fn find = (findobj_fn)FINDOBJ_ADDR;
  loadobj_fn load = (loadobj_fn)LOADOBJ_ADDR;
  int is_call = !strcmp(g_selection, "call");

  g_nsp_beck_pawn = find(NULL, NULL, L"MN9Game.MN9Spw09Pawn", 0);
  g_nsp_beck_ctrl = find(NULL, NULL, L"MN9Game.MN9Pl01Controller", 0);

  if (is_call) {
    g_nsp_pawn = find(NULL, NULL, L"MN9Game.MN9Pl02Pawn", 0);
    g_nsp_ctrl = find(NULL, NULL, L"MN9Game.MN9Pl02Controller", 0);
    g_nsp_arch = find(NULL, NULL, L"Pl02_Archetype.Pl02_Archetype", 0);
    if (!g_nsp_arch && g_nsp_pawn && load)
      g_nsp_arch = load(g_nsp_pawn, NULL, L"Pl02_Archetype.Pl02_Archetype", NULL, 0, NULL, 0);
    if (!g_nsp_arch && load)
      g_nsp_arch = load(NULL, NULL, L"Pl02_Archetype.Pl02_Archetype", NULL, 0, NULL, 0);
  } else {
    g_nsp_pawn = find(NULL, NULL, L"MN9GameContent.MN9Pl03Pawn", 0);
    g_nsp_ctrl = find(NULL, NULL, L"MN9GameContent.MN9Pl03Controller", 0);
    if (!g_nsp_pawn && load)
      g_nsp_pawn = load(NULL, NULL, L"pl03_archetype.MN9Pl03Pawn", NULL, 0, NULL, 0);
    if (!g_nsp_ctrl && load)
      g_nsp_ctrl = load(NULL, NULL, L"pl03_archetype.MN9Pl03Controller", NULL, 0, NULL, 0);
    g_nsp_arch = find(NULL, NULL, L"pl03_archetype.Pawn.Pl03Pawn", 0);
    if (!g_nsp_arch && load)
      g_nsp_arch = load(NULL, NULL, L"pl03_archetype.Pawn.Pl03Pawn", NULL, 0, NULL, 0);
  }

  if (g_nsp_beck_pawn && g_nsp_pawn && g_nsp_ctrl) {
    g_nsp_resolved = 1;
    LOG("[mn9pawn] native spawn ready %s: beck=%p pawn=%p ctrl=%p arch=%p\n",
        g_selection, g_nsp_beck_pawn, g_nsp_pawn, g_nsp_ctrl, g_nsp_arch);
  }
}

__attribute__((visibility("hidden"))) void* spawn_hook(void* world, void* cls, uint64_t fname, void* outer,
                        void* a5, void* tpl, unsigned a7, unsigned a8,
                        void* a9, void* a10, unsigned a11, void* a12)
{
  if (!g_nsp_resolved) native_spawn_resolve();
  if (g_nsp_resolved) {
    if (cls == g_nsp_beck_pawn) {
      cls = g_nsp_pawn;
      if (g_nsp_arch) tpl = g_nsp_arch;
      if (!g_nsp_log_pawn) {
        g_nsp_log_pawn = 1;
        LOG("[mn9pawn] native spawn: pawn %p -> %p arch -> %p\n",
            g_nsp_beck_pawn, g_nsp_pawn, tpl);
      }
    } else if (cls == g_nsp_beck_ctrl) {
      cls = g_nsp_ctrl;
      if (!g_nsp_log_ctrl) {
        g_nsp_log_ctrl = 1;
        LOG("[mn9pawn] native spawn: controller %p -> %p\n", g_nsp_beck_ctrl, g_nsp_ctrl);
      }
    }
  }
  return ((spawn_fn)g_real_spawn)(world, cls, fname, outer, a5, tpl,
                                  a7, a8, a9, a10, a11, a12);
}


/* ---- Call barrier: Xel absorb (Resorption) ----
 *
 * Beck absorbs resorption-ready enemies by dashing through them.  Call's
 * barrier is a shield actor (MN9Shell_Pl02Barrier); while it is up, run the
 * same enemy Resorption() on anything weakened inside the bubble. */
static int   g_ba_enabled = 1;
static int   g_ba_debug;
static unsigned g_ba_ticks;
static void* g_ba_pawn;
static void* g_ba_barrier;
static int   g_ba_absorbed;
static int   g_ba_loc_off;
static int   g_ba_ctrl_off;
static int   g_ba_rg_off;
static int   g_ba_hp_off;
static int   g_ba_hpmax_off;
static int   g_ba_owner_off;
static void* g_ba_ecls[128];
static int   g_ba_necls;
static void* g_ba_xcls[256];
static int   g_ba_nxcls;

static void* ba_find_fn(void* obj, const char* name)
{
  if (!obj || !addr_mapped(obj)) return NULL;
  uint64_t fnn = fname(name);
  return fnn ? ((void*(*)(void*, uint64_t, unsigned))FINDFUNC_ADDR)(obj, fnn, 0) : NULL;
}

static void ba_call(void* obj, void* fn, void* parms)
{
  ((void(*)(void*, void*, void*, void*))PROCESSEVENT_ADDR)(obj, fn, parms, NULL);
}

static void* ba_find_pawn(void)
{
  static void* cls;
  if (g_ba_pawn && addr_mapped(g_ba_pawn)) {
    if (g_ba_ctrl_off <= 0 || *(void**)((char*)g_ba_pawn + g_ba_ctrl_off)) return g_ba_pawn;
    g_ba_pawn = NULL;
  }
  if (!cls) cls = ((findobj_fn)FINDOBJ_ADDR)(NULL, NULL, L"MN9Game.MN9Pl02Pawn", 0);
  if (!cls) return NULL;
  void** objs = *(void***)OBJ_ARRAY;
  int count = *(int*)OBJ_COUNT;
  if (!objs || count <= 0 || count > 400000) return NULL;
  int low = count - 20000; if (low < 0) low = 0;
  for (int i = count - 1; i >= low; i--) {
    if (!addr_mapped(objs + i)) break;
    void* o = objs[i];
    if (!o || !addr_mapped(o) || *(void**)((char*)o + OFF_CLASS) != cls) continue;
    if (g_ba_ctrl_off <= 0) g_ba_ctrl_off = prop_offset(o, "Controller");
    if (g_ba_ctrl_off > 0 && !*(void**)((char*)o + g_ba_ctrl_off)) continue;
    g_ba_pawn = o;
    return o;
  }
  return NULL;
}

static void* ba_find_barrier(void* pawn)
{
  void** objs = *(void***)OBJ_ARRAY;
  int count = *(int*)OBJ_COUNT;
  if (!objs || count <= 0 || count > 400000) return NULL;
  int low = count - 25000; if (low < 0) low = 0;
  for (int i = count - 1; i >= low; i--) {
    if (!addr_mapped(objs + i)) break;
    void* o = objs[i];
    if (!o || !addr_mapped(o)) continue;
    if (g_ba_owner_off > 0 && *(void**)((char*)o + g_ba_owner_off) != pawn) continue;
    char cn[96];
    if (!class_name_of(o, cn, sizeof cn)) continue;
    if (strncmp(cn, "MN9Shell_Pl02Barrier", 20)) continue;
    return o;
  }
  return NULL;
}

static int ba_enemy_class(void* o)
{
  void* cls = *(void**)((char*)o + OFF_CLASS);
  if (!cls) return 0;
  for (int i = 0; i < g_ba_necls; i++) if (g_ba_ecls[i] == cls) return 1;
  for (int i = 0; i < g_ba_nxcls; i++) if (g_ba_xcls[i] == cls) return 0;
  void* fn = ba_find_fn(o, "IsResorption");
  if (fn) {
    if (g_ba_necls < 128) g_ba_ecls[g_ba_necls++] = cls;
    return 1;
  }
  if (g_ba_nxcls < 256) g_ba_xcls[g_ba_nxcls++] = cls;
  return 0;
}

static void barrier_absorb_tick(void* world)
{
  (void)world;
  if (!g_ba_enabled) return;
  unsigned t = g_ba_ticks++;
  void* pawn = ba_find_pawn();
  if (!pawn) return;
  if (!g_ba_loc_off) {
    g_ba_loc_off = prop_offset(pawn, "Location");
    g_ba_ctrl_off = prop_offset(pawn, "Controller");
    g_ba_owner_off = prop_offset(pawn, "Owner");
    g_ba_rg_off = prop_offset(pawn, "ResorptionGrade");
    g_ba_hp_off = prop_offset(pawn, "Health");
    g_ba_hpmax_off = prop_offset(pawn, "HealthMax");
  }
  if ((t % 15) == 0) g_ba_barrier = ba_find_barrier(pawn);
  if (g_ba_debug && (t % 180) == 0)
    LOG("[mn9bar] t=%u pawn=%p barrier=%p absorbed=%d\n", t, pawn, g_ba_barrier, g_ba_absorbed);
  if (!g_ba_barrier || !addr_mapped(g_ba_barrier)) return;
  if ((t % 20) != 0 || g_ba_loc_off <= 0) return;

  float* bp = (float*)((char*)g_ba_barrier + g_ba_loc_off);
  void** objs = *(void***)OBJ_ARRAY;
  int count = *(int*)OBJ_COUNT;
  if (!objs || count <= 0 || count > 400000) return;
  int low = count - 30000; if (low < 0) low = 0;
  for (int i = count - 1; i >= low; i--) {
    if (!addr_mapped(objs + i)) break;
    void* o = objs[i];
    if (!o || !addr_mapped(o) || o == pawn || o == g_ba_barrier) continue;
    if (!ba_enemy_class(o)) continue;
    if (g_ba_ctrl_off > 0 && !*(void**)((char*)o + g_ba_ctrl_off)) continue;  // not a live pawn
    float* ep = (float*)((char*)o + g_ba_loc_off);
    float dx = ep[0] - bp[0], dy = ep[1] - bp[1], dz = ep[2] - bp[2];
    if (dx*dx + dy*dy + dz*dz > 190.0f * 190.0f) continue;

    int hp = g_ba_hp_off > 0 ? *(int*)((char*)o + g_ba_hp_off) : -1;
    int hpmax = g_ba_hpmax_off > 0 ? *(int*)((char*)o + g_ba_hpmax_off) : -1;
    int grade = g_ba_rg_off > 0 ? *(int*)((char*)o + g_ba_rg_off) : -1;
    if (!(grade >= 0 || (hp >= 0 && hpmax > 0 && hp * 4 <= hpmax))) continue;

    unsigned char b[256]; memset(b, 0, sizeof b);
    void* can = ba_find_fn(o, "CanGotoStateResorption");
    if (can) {
      ba_call(o, can, b);
      if (!b[0]) continue;
    }
    void* res = ba_find_fn(o, "Resorption");
    if (!res) continue;
    char cn[96], on[96];
    class_name_of(o, cn, sizeof cn);
    obj_name_str(o, on, sizeof on);
    LOG("[mn9bar] absorb %s (%s) hp=%d/%d grade=%d d=%.0f\n",
        cn, on, hp, hpmax, grade,
        sqrtf(dx*dx + dy*dy + dz*dz));
    memset(b, 0, sizeof b);
    ba_call(o, res, b);
    g_ba_absorbed++;
  }
}


__attribute__((visibility("hidden"), used)) void mega_tick(void* world)
{
  (void)world;
  if (!g_tick_alive) { g_tick_alive = 1; LOG("[mn9pawn] world tick hook alive\n"); }
  // Native call: the spawn hook already replaced the pawn, archetype and
  // controller; the only thing left is the barrier Xel absorb.
  if (!strcmp(g_selection, "call")) { barrier_absorb_tick(world); return; }
  if (native_spawn_wanted()) return;
  if (g_matvis_mode) {
    static int done; static void* ma; 

static void* mc;
    void* pn=find_player_pawn(); if (!pn) return;
    int lo=prop_offset(pn,"Location"), ro=prop_offset(pn,"Rotation");
    if (!done) {
      void** objs=*(void***)OBJ_ARRAY; int cnt=*(int*)OBJ_COUNT;
      for (int i=0;i<cnt;i++) {
        if (!addr_mapped(objs+i)) break;
        void* o=objs[i]; if(!o||!addr_mapped(o)) continue;
        char nm[96]="?"; obj_name_str(o,nm,sizeof nm);
        if (strncmp(nm,"SkeletalMeshActorMAT",20)) continue;
        int mo=prop_offset(o,"Mesh"); if(mo<=0) continue;
        void* c=*(void**)((char*)o+mo); if(!c||!addr_mapped(c)) continue;
        int sk=prop_offset(c,"SkeletalMesh"); void* mm=sk>0?*(void**)((char*)c+sk):NULL;
        char mp[160]="?"; path_of(mm,mp,sizeof mp);
        if (!strstr(mp,"Bs01_Body1")) continue;
        ma=o; mc=c; break;
      }
      LOG("[mn9pawn] matvis actor=%p comp=%p\n", ma, mc);
      if (mc) {
        demo_append_set(mc);
        void* tr=find_loaded_named("Bs01_AnimTree");
        demoboss_anim_setup(mc, tr);
        done=1;
      }
      const char* cp[3]={"Mesh","ArmLMeshComponent","ArmRMeshComponent"};
      for (int i=0;i<3;i++){ int o=prop_offset(pn,cp[i]); if(o<=0) continue; void* c=*(void**)((char*)pn+o); if(!c) continue;
        ((void(*)(void*, unsigned))0x14801b0UL)(c,1);
        int h=prop_offset(c,"bHidden"); if(h>0) *(unsigned char*)((char*)c+h)=1; }
      int hd=prop_offset(ma,"bHidden"); if(hd>0) *(unsigned char*)((char*)ma+hd)=0;
      LOG("[mn9pawn] matvis hid beck\n");
    }
    if (ma && lo>0) {
      int bl=prop_offset(ma,"Location"), br=prop_offset(ma,"Rotation");
      if(bl>0) memcpy((char*)ma+bl,(char*)pn+lo,12);
      if(br>0&&ro>0) memcpy((char*)ma+br,(char*)pn+ro,12);
    }
    return;
  }
  if (g_demoboss_mode) {
    static int done; static void* dp;
    void* pn=find_player_pawn();
    if (!pn) return;
    if (!done) {
      done=1;
      findobj_fn find=(findobj_fn)FINDOBJ_ADDR;
      char meshname[64], treename[64];
      snprintf(meshname, sizeof meshname, "%sBody1", g_demo_prefix);
      void* bcls=NULL;
      void* mesh=NULL; void* tree=NULL;
      {
        const wchar_t* homes[] = {L"", L"OilPlant_BsRoom", L"WaterWorks_BsRoom", L"PowerStation_BsRoom", L"Mine_BsRoom", L"MilitaryBase_BsRoom", L"RadioTower_BsRoom", L"HighWay_BsRoom", L"WhiteHouse_BsRoom", L"", L"Tutorial_BsRoom"};
        const wchar_t* home = (g_demo_boss >= 1 && g_demo_boss <= 10) ? homes[g_demo_boss] : L"";
        if (home && *home) ((void*(*)(void*, const wchar_t*, unsigned))0x0da0490UL)(NULL,home,0);
        const wchar_t* mats[] = {L"MN9Game.Spw09SkeletalMeshCinematicActor", L"MN9Game.Spw09SkeletalMeshActorMAT", L"Engine.SkeletalMeshActorMAT", L"MN9Game.SkeletalMeshActor", L"Engine.SkeletalMeshActor", NULL};
        for (int mi=0; mats[mi]; mi++) { bcls=find(NULL,NULL,mats[mi],0); if (bcls) { LOG("[mn9pawn] demoboss actor class=%ls\n", mats[mi]); break; } }
        mesh=find_loaded_named(meshname);
        static const char* tsfx[] = {"BattleAnimTree","AnimTree","BattleDemoAnimTree","DemoAnimTree","StoryAnimTree", NULL};
        for (int ti=0; tsfx[ti] && !tree; ti++) { snprintf(treename, sizeof treename, "%s%s", g_demo_prefix, tsfx[ti]); tree=find_loaded_named(treename); if (tree) LOG("[mn9pawn] demoboss tree=%s\n", treename); }
      }
      LOG("[mn9pawn] demoboss boss=%d cls=%p mesh=%p tree=%p\n", g_demo_boss, bcls, mesh, tree);
      int lo=prop_offset(pn,"Location"), ro=prop_offset(pn,"Rotation");
      if (bcls && lo>0 && ro>0) {
        spawn_fn sp=(spawn_fn)SPAWNACTOR_ADDR;
        void* bp=sp(world,bcls,0,(char*)pn+lo,(char*)pn+ro,NULL,1,0,NULL,NULL,0,NULL);
        LOG("[mn9pawn] demoboss spawned=%p\n", bp);
        if (bp) {
          dp=bp;
          int ow=prop_offset(bp,"Owner"); if(ow>0) *(void**)((char*)bp+ow)=pn;
          int ba=prop_offset(bp,"Base"); if(ba>0) *(void**)((char*)bp+ba)=pn;
          int ha=prop_offset(bp,"bHardAttach"); if(ha>0) *(unsigned char*)((char*)bp+ha)=1;
          int hd=prop_offset(bp,"bHidden"); if(hd>0) *(unsigned char*)((char*)bp+hd)=0;
          void* bodycomp=NULL;
          int mo=prop_offset(bp,"Mesh");
          if (mo>0) { void* c=*(void**)((char*)bp+mo); if (c && addr_mapped(c)) { int sk=prop_offset(c,"SkeletalMesh"); if (sk>0) bodycomp=c; } }
          if (!bodycomp) {
            void** objs=*(void***)OBJ_ARRAY; int cnt=*(int*)OBJ_COUNT;
            for (int i=cnt-1;i>=0 && i>cnt-30000;i--) {
              if (!addr_mapped(objs+i)) break;
              void* o=objs[i]; if(!o||!addr_mapped(o)) continue;
              int ow2=prop_offset(o,"Owner"); if(ow2<=0||*(void**)((char*)o+ow2)!=bp) continue;
              int sk=prop_offset(o,"SkeletalMesh"); if(sk<=0) continue;
              bodycomp=o; break;
            }
          }
          int got=0;
          if (bodycomp) {
            got=1;
            if (mesh) ((setskmesh_fn)SETSKMESH)(bodycomp, mesh, 0);
            int mc=prop_offset(bodycomp,"Materials"); if(mc>0 && *(void***)((char*)bodycomp+mc)) *(int*)((char*)bodycomp+mc+8)=0;
            if (tree) { ((setanim_fn)SETANIMTREE)(bodycomp, tree); ((void(*)(void*, unsigned))0x115ac00UL)(bodycomp,1); }
            demo_append_set(bodycomp);
            int h=prop_offset(bodycomp,"bHidden"); if(h>0) *(unsigned char*)((char*)bodycomp+h)=0;
            int on=prop_offset(bodycomp,"bOwnerNoSee"); if(on>0) *(unsigned char*)((char*)bodycomp+on)=0;
            demoboss_anim_setup(bodycomp, tree);
          }
          LOG("[mn9pawn] demoboss comps=%d body=%p\n", got, bodycomp);
          g_demo_cls = *(void**)bp;
        }
      }
      const char* cp[3]={"Mesh","ArmLMeshComponent","ArmRMeshComponent"};
      for (int i=0;i<3;i++){ int o=prop_offset(pn,cp[i]); if(o<=0) continue; void* c=*(void**)((char*)pn+o); if(!c) continue;
        ((void(*)(void*, unsigned))0x14801b0UL)(c,1);
        int h=prop_offset(c,"bHidden"); if(h>0) *(unsigned char*)((char*)c+h)=1; }
      // stages that render Beck through separate cinematic/mesh actors
      {
        void** objs2=*(void***)OBJ_ARRAY; int cnt2=*(int*)OBJ_COUNT; int hidn=0;
        for (int i=cnt2-1;i>=0 && i>cnt2-40000;i--) {
          if (!addr_mapped(objs2+i)) break;
          void* o=objs2[i]; if(!o||!addr_mapped(o)) continue;
          if (o == dp) continue;
          char cn[96]="?"; class_name_of(o,cn,sizeof cn);
          if (strstr(cn,"SkeletalMeshCinematicActor")||strstr(cn,"Spw09SkeletalMeshActor")) {
            int os=prop_offset(o,"bOwnerNoSee"); if(os>0) *(unsigned char*)((char*)o+os)=1;
            int hh=prop_offset(o,"bHidden"); if(hh>0) *(unsigned char*)((char*)o+hh)=1;
            hidn++;
          }
        }
        int hidm=0;
        for (int i=cnt2-1;i>=0 && i>cnt2-40000;i--) {
          if (!addr_mapped(objs2+i)) break;
          void* o=objs2[i]; if(!o||!addr_mapped(o)) continue;
          if (o == dp) continue;
          int sk=prop_offset(o,"SkeletalMesh"); if(sk<=0) continue;
          void* mm=*(void**)((char*)o+sk); char mp[160]="?"; path_of(mm,mp,sizeof mp);
          if (strstr(mp,"Spw09_")) { ((void(*)(void*, unsigned))0x14801b0UL)(o,1); hidm++; }
        }
        LOG("[mn9pawn] demoboss hid beck comps + cinematics=%d spw09=%d\n", hidn, hidm);
      }
    }
    if (dp) {
      if (!addr_mapped(dp) || (g_demo_cls && *(void**)dp != g_demo_cls)) {
        LOG("[mn9pawn] demoboss stale actor -> respawn\n");
        dp=NULL; done=0; g_demo_cls=NULL; g_demo_comp=NULL; g_demo_node=NULL; g_db_bl=0; g_db_lo=g_db_ro=g_db_br=0;
        return;
      }
      if (g_db_bl<=0) { g_db_lo=prop_offset(pn,"Location"); g_db_ro=prop_offset(pn,"Rotation"); g_db_bl=prop_offset(dp,"Location"); g_db_br=prop_offset(dp,"Rotation"); }
      if(g_db_lo>0&&g_db_bl>0) memcpy((char*)dp+g_db_bl,(char*)pn+g_db_lo,12);
      if(g_db_ro>0&&g_db_br>0) {
        memcpy((char*)dp+g_db_br,(char*)pn+g_db_ro,12);
        static int lastYaw = -1;
        int vo=prop_offset(pn,"Velocity");
        if (vo>0) {
          float vx=*(float*)((char*)pn+vo), vy=*(float*)((char*)pn+vo+4);
          float sp2 = sqrtf(vx*vx + vy*vy);
          if (sp2 > 60.0f) {
            // face the actual movement direction; the mesh's visual forward is
            // actorYaw + 180, so subtract half a turn.
            float ang = atan2f(vy, vx) * (65536.0f / 6.28318530718f);
            int yoff = g_demo_yaw_flip ? 0 : 32768;
            lastYaw = ((int)ang - yoff) & 65535;
            g_demo_face = (vy >= 0) ? 1 : -1;
          }
        }
        if (lastYaw >= 0) *(int*)((char*)dp+g_db_br+4) = lastYaw;
        else *(int*)((char*)dp+g_db_br+4) += (g_demo_yaw_flip ? 0 : 32768);
      }
      // give the avatar the player's light environment (otherwise it is scene-lit and dark)
      {
        static unsigned le_t;
        if (g_demo_comp && (le_t++ % 30) == 0) {
          int mo=prop_offset(pn,"Mesh"); void* pc=mo>0?*(void**)((char*)pn+mo):NULL;
          void* lev=NULL;
          if (pc) { int le=prop_offset(pc,"LightEnvironment"); if(le>0) lev=*(void**)((char*)pc+le); }
          if (!lev) { int pe=prop_offset(pn,"LightEnvironment"); if(pe>0) lev=*(void**)((char*)pn+pe); }
          if (lev) {
            int ae=prop_offset(g_demo_comp,"LightEnvironment");
            if (ae>0 && *(void**)((char*)g_demo_comp+ae)!=lev) { *(void**)((char*)g_demo_comp+ae)=lev; LOG("[mn9pawn] demoanim lightenv=%p\n", lev); }
          }
          if (pc) {
            int lc=prop_offset(pc,"LightingChannels"); int la=prop_offset(g_demo_comp,"LightingChannels");
            if (lc>0 && la>0) memcpy((char*)g_demo_comp+la,(char*)pc+lc,4);
          }
        }
      }
      // keep the avatar visible even if a mode/cinematic flag hides it
      if (g_demo_comp && addr_mapped(g_demo_comp)) {
        ((void(*)(void*, unsigned))0x14801b0UL)(g_demo_comp, 0);
        int dh=prop_offset(g_demo_comp,"bHidden"); if(dh>0) *(unsigned char*)((char*)g_demo_comp+dh)=0;
        int dpz=prop_offset(g_demo_comp,"bPauseAnims"); if(dpz>0) *(unsigned char*)((char*)g_demo_comp+dpz)=0;
      }
      if (dp && addr_mapped(dp)) {
        int ah=prop_offset(dp,"bHidden"); if(ah>0) *(unsigned char*)((char*)dp+ah)=0;
        int ao=prop_offset(dp,"bOwnerNoSee"); if(ao>0) *(unsigned char*)((char*)dp+ao)=0;
      }
      // keep Beck hidden even if a form change or respawn re-enables him
      {
        const char* cp[3]={"Mesh","ArmLMeshComponent","ArmRMeshComponent"};
        for (int i=0;i<3;i++) {
          int o=prop_offset(pn,cp[i]); if(o<=0) continue;
          void* c=*(void**)((char*)pn+o); if(!c) continue;
          ((void(*)(void*, unsigned))0x14801b0UL)(c,1);
          int h=prop_offset(c,"bHidden"); if(h>0) *(unsigned char*)((char*)c+h)=1;
        }
      }
      // detect a newly spawned buster shot to trigger a boss attack clip
      if (!g_demo_buster_class) {
        int abo=prop_offset(pn,"ArchetypedBuster");
        void* ar=abo>0?*(void**)((char*)pn+abo):NULL;
        if (ar && addr_mapped(ar)) g_demo_buster_class=*(void**)((char*)ar+OFF_CLASS);
        if (!g_demo_buster_class) { findobj_fn ffind=(findobj_fn)FINDOBJ_ADDR; void* c=ffind(NULL,NULL,L"MN9Game.MN9Shell_Spw09Buster",0); if(c) g_demo_buster_class=c; }
        if (g_demo_buster_class) LOG("[mn9pawn] demoanim buster class=%p\n", g_demo_buster_class);
      }
      if (g_demo_buster_class && g_demo_has_attack) {
        void** objs=*(void***)OBJ_ARRAY; int cnt=*(int*)OBJ_COUNT; int low=cnt-800; if(low<0) low=0;
        for (int i=cnt-1;i>=low;i--) {
          if (!addr_mapped(objs+i)) break;
          void* o=objs[i]; if(!o||!addr_mapped(o)) continue;
          if (*(void**)((char*)o+OFF_CLASS)!=g_demo_buster_class) continue;
          int ow=prop_offset(o,"Owner"); if(ow>0 && *(void**)((char*)o+ow)!=pn) continue;
          int seen=0; for(int k=0;k<24;k++) if(g_demo_seen[k]==o){seen=1;break;}
          if (!seen) { g_demo_seen[g_demo_seen_i++ % 24]=o; if (g_demo_shell_primed && g_demo_atk<=0 && g_demo_atk_cd<=0) { g_demo_atk=22; g_demo_atk_cd=55; LOG("[mn9pawn] demoanim fire -> attack\n"); } }
        }
        g_demo_shell_primed=1;
      }
      demoboss_anim_drive(pn, dp);
    }
    return;
  }
  if (g_visactor_mode) {
    static int done;
    void* pn=find_player_pawn();
    if (!pn) return;
    if (!done) {
      done=1;
      findobj_fn find=(findobj_fn)FINDOBJ_ADDR;
      const wchar_t* classes[] = {
        L"MN9Game.Spw09SkeletalMeshCinematicActor",
        L"MN9Game.Spw09SkeletalMeshActorMAT",
        L"MN9Game.SkeletalMeshActor",
        L"Engine.SkeletalMeshActor", NULL };
      void* cls=NULL;
      for (int i=0; classes[i]; i++) { cls=find(NULL,NULL,classes[i],0); if (cls) { LOG("[mn9pawn] visactor class=%ls -> %p\n", classes[i], cls); break; } }
      void* mesh=find_loaded_named("Bs01_Body1");
      void* tree=find_loaded_named("Bs01_AnimTree");
      if (!mesh) { ((void*(*)(void*, const wchar_t*, unsigned))0x0da0490UL)(NULL,L"Map11_OIL_Persistent00",0); mesh=find_loaded_named("Bs01_Body1"); tree=find_loaded_named("Bs01_AnimTree"); }
      int lo=prop_offset(pn,"Location"), ro=prop_offset(pn,"Rotation");
      LOG("[mn9pawn] visactor cls=%p mesh=%p tree=%p\n", cls, mesh, tree);
      if (cls && mesh && lo>0 && ro>0) {
        spawn_fn sp=(spawn_fn)SPAWNACTOR_ADDR;
        void* vp=sp(world,cls,0,(char*)pn+lo,(char*)pn+ro,NULL,1,0,NULL,NULL,0,NULL);
        LOG("[mn9pawn] visactor spawned=%p\n", vp);
        if (vp) {
          g_ba_pawn=vp;
          int ow=prop_offset(vp,"Owner"); if(ow>0) *(void**)((char*)vp+ow)=pn;
          int ba=prop_offset(vp,"Base"); if(ba>0) *(void**)((char*)vp+ba)=pn;
          int ha=prop_offset(vp,"bHardAttach"); if(ha>0) *(unsigned char*)((char*)vp+ha)=1;
          int hd=prop_offset(vp,"bHidden"); if(hd>0) *(unsigned char*)((char*)vp+hd)=0;
          void** objs=*(void***)OBJ_ARRAY; int cnt=*(int*)OBJ_COUNT; int got=0;
          for (int i=cnt-1;i>=0 && i>cnt-30000;i--) {
            if (!addr_mapped(objs+i)) break;
            void* o=objs[i]; if(!o||!addr_mapped(o)) continue;
            int ow2=prop_offset(o,"Owner"); if(ow2<=0||*(void**)((char*)o+ow2)!=vp) continue;
            int sk=prop_offset(o,"SkeletalMesh"); if(sk<=0) continue;
            got++;
            ((setskmesh_fn)SETSKMESH)(o, mesh, 1);
            int mc=prop_offset(o,"Materials"); if(mc>0 && *(void***)((char*)o+mc)) *(int*)((char*)o+mc+8)=0;
            if (tree) { ((setanim_fn)SETANIMTREE)(o, tree); ((void(*)(void*, unsigned))0x115ac00UL)(o,1); }
            int h=prop_offset(o,"bHidden"); if(h>0) *(unsigned char*)((char*)o+h)=0;
            int on=prop_offset(o,"bOwnerNoSee"); if(on>0) *(unsigned char*)((char*)o+on)=0;
            g_va_comp = o;
          }
          LOG("[mn9pawn] visactor comps=%d comp=%p\n", got, g_va_comp);
          if (g_va_comp && tree) {
            int ao=prop_offset(g_va_comp,"AnimSets");
            void* aset=find_loaded_named("Bs01_AnimSet");
            if (ao>0 && aset) { void** arr=*(void***)((char*)g_va_comp+ao); int n=*(int*)((char*)g_va_comp+ao+8), mx=*(int*)((char*)g_va_comp+ao+12);
              if (n>=mx){int nm2=mx?mx*2:8; arr=(void**)((void*(*)(void*,unsigned,unsigned))0xdb78a0UL)(arr,nm2*8,8); *(void***)((char*)g_va_comp+ao)=arr; *(int*)((char*)g_va_comp+ao+12)=nm2;}
              arr[n]=aset; *(int*)((char*)g_va_comp+ao+8)=n+1; LOG("[mn9pawn] visactor animset appended %d\n", n); }

          }
        }
      }
      const char* cp[3]={"Mesh","ArmLMeshComponent","ArmRMeshComponent"};
      for (int i=0;i<3;i++){ int o=prop_offset(pn,cp[i]); if(o<=0) continue; void* c=*(void**)((char*)pn+o); if(!c) continue;
        ((void(*)(void*, unsigned))0x14801b0UL)(c,1);
        int h=prop_offset(c,"bHidden"); if(h>0) *(unsigned char*)((char*)c+h)=1; }
      LOG("[mn9pawn] visactor hid beck\n");
    }
    if (g_ba_pawn) {
      int lo=prop_offset(pn,"Location"), ro=prop_offset(pn,"Rotation");
      int bl=prop_offset(g_ba_pawn,"Location"), br=prop_offset(g_ba_pawn,"Rotation");
      if(lo>0&&bl>0) memcpy((char*)g_ba_pawn+bl,(char*)pn+lo,12);
      if(ro>0&&br>0) memcpy((char*)g_ba_pawn+br,(char*)pn+ro,12);
      // locomotion: pick Bs01 clip by player speed
      static float lastx,lasty,lastz; static int st=-1; static unsigned tk;
      if (lo>0) {
        float x=*(float*)((char*)pn+lo), y=*(float*)((char*)pn+lo+4), z=*(float*)((char*)pn+lo+8);
        float sp=0; if (tk) { float dx=x-lastx,dy=y-lasty,dz=z-lastz; sp=sqrtf(dx*dx+dy*dy+dz*dz); }
        lastx=x;lasty=y;lastz=z;
        int want = (sp > 0.6f) ? 1 : 0;
        if (g_va_node && want!=st) {
          st=want;
          const char* nm = want? "Bs01_Run" : "Bs01_Stay";
          ((void(*)(void*, uint64_t))0x1198f30UL)(g_va_node, fname(nm));
          ((void(*)(void*, unsigned, float, float))0x119f450UL)(g_va_node, 1, 1.0f, 0.0f);
          LOG("[mn9pawn] visactor anim -> %s (sp=%.2f)\n", nm, sp);
        }
        tk++;
      }
    }
    return;
  }
  if (g_bossanim_mode) {
    static int done;
    void* pn=find_player_pawn();
    if (!pn) return;
    if (!done) {
      done=1;
      findobj_fn find=(findobj_fn)FINDOBJ_ADDR;
      void* bcls=find(NULL,NULL,L"MN9Game.MN9Bs01Pawn",0);
      int lo=prop_offset(pn,"Location"), ro=prop_offset(pn,"Rotation");
      LOG("[mn9pawn] bossanim cls=%p\n", bcls);
      if (bcls && lo>0 && ro>0) {
        spawn_fn sp=(spawn_fn)SPAWNACTOR_ADDR;
        void* bp=sp(world,bcls,0,(char*)pn+lo,(char*)pn+ro,NULL,1,0,NULL,NULL,0,NULL);
        LOG("[mn9pawn] bossanim spawned=%p\n", bp);
        if (bp) {
          g_ba_pawn=bp;
          int ow=prop_offset(bp,"Owner"); if(ow>0) *(void**)((char*)bp+ow)=pn;
          int ba=prop_offset(bp,"Base"); if(ba>0) *(void**)((char*)bp+ba)=pn;
          int ha=prop_offset(bp,"bHardAttach"); if(ha>0) *(unsigned char*)((char*)bp+ha)=1;
          int hd=prop_offset(bp,"bHidden"); if(hd>0) *(unsigned char*)((char*)bp+hd)=0;
          void** objs=*(void***)OBJ_ARRAY; int cnt=*(int*)OBJ_COUNT;
          for (int i=cnt-1;i>=0 && i>cnt-30000;i--) {
            if (!addr_mapped(objs+i)) break;
            void* o=objs[i]; if(!o||!addr_mapped(o)) continue;
            int ow2=prop_offset(o,"Owner"); if(ow2<=0||*(void**)((char*)o+ow2)!=bp) continue;
            int sk=prop_offset(o,"SkeletalMesh"); if(sk<=0) continue;
            LOG("[mn9pawn] bossanim realcomp=%p mesh=%p\n", o, *(void**)((char*)o+sk));
            break;
          }
        }
      }
      const char* cp[3]={"Mesh","ArmLMeshComponent","ArmRMeshComponent"};
      for (int i=0;i<3;i++){ int o=prop_offset(pn,cp[i]); if(o<=0) continue; void* c=*(void**)((char*)pn+o); if(!c) continue;
        ((void(*)(void*, unsigned))0x14801b0UL)(c,1);
        int h=prop_offset(c,"bHidden"); if(h>0) *(unsigned char*)((char*)c+h)=1; }
      LOG("[mn9pawn] bossanim hid beck comps\n");
    }
    if (g_ba_pawn) {
      static int dumped; static unsigned bt;
      int lo=prop_offset(pn,"Location"), ro=prop_offset(pn,"Rotation");
      int bl=prop_offset(g_ba_pawn,"Location"), br=prop_offset(g_ba_pawn,"Rotation");
      if(lo>0&&bl>0) memcpy((char*)g_ba_pawn+bl,(char*)pn+lo,12);
      if(ro>0&&br>0) memcpy((char*)g_ba_pawn+br,(char*)pn+ro,12);
      bt++;
      if (!dumped && bt > 90 && (bt % 30) == 0) {
        void** objs=*(void***)OBJ_ARRAY; int cnt=*(int*)OBJ_COUNT;
        for (int i=cnt-1;i>=0 && i>cnt-30000;i--) {
          if (!addr_mapped(objs+i)) break;
          void* o=objs[i]; if(!o||!addr_mapped(o)) continue;
          int ow2=prop_offset(o,"Owner"); if(ow2<=0||*(void**)((char*)o+ow2)!=g_ba_pawn) continue;
          int sk=prop_offset(o,"SkeletalMesh"); if(sk<=0) continue;
          void* mm=*(void**)((char*)o+sk);
          if (!mm) { if(bt%120==0) LOG("[mn9pawn] bossdump comp=%p mesh=NULL\n", o); continue; }
          char mp[160]="?"; path_of(mm,mp,sizeof mp);
          int aoo=prop_offset(o,"AnimSets"); int aoo2=prop_offset(o,"Animations"); int ato=prop_offset(o,"AnimTreeTemplate");
          LOG("[mn9pawn] bossdump comp=%p mesh=%s animsets=%d animations=%p template=%p\n", o, mp, aoo>0?*(int*)((char*)o+aoo+8):-1, aoo2>0?*(void**)((char*)o+aoo2):NULL, ato>0?*(void**)((char*)o+ato):NULL);
          void* tr=find_loaded_named("Bs01_AnimTree");
          demoboss_anim_setup(o, tr);
          dumped=1; break;
        }
      }
    }
    return;
  }
  if (g_fire_mode) {
    static int done; static unsigned ft;
    void* pn=find_player_pawn();
    if (!pn) return;
    int mo=prop_offset(pn,"Mesh"); void* comp=mo>0?*(void**)((char*)pn+mo):NULL;
    if (!comp) return;
    if (!done) {
      done=1;
      void* mesh=find_loaded_named("Bs01_Body1");
      void* tree=find_loaded_named("Bs01_AnimTree");
      if (!mesh) { ((void*(*)(void*, const wchar_t*, unsigned))0x0da0490UL)(NULL,L"Map11_OIL_Persistent00",0); mesh=find_loaded_named("Bs01_Body1"); }
      int mc=prop_offset(comp,"Materials"); if(mc>0 && *(void***)((char*)comp+mc)) *(int*)((char*)comp+mc+8)=0;
      if (mesh) ((setskmesh_fn)SETSKMESH)(comp, mesh, 1);
      if (tree) { ((setanim_fn)SETANIMTREE)(comp, tree); ((void(*)(void*, unsigned))0x115ac00UL)(comp,1); }
      LOG("[mn9pawn] fire: body mesh=%p tree=%p\n", mesh, tree);
    }
    static void* fxactor;
    if (!fxactor) {
      void** objs=*(void***)OBJ_ARRAY; int cnt=*(int*)OBJ_COUNT;
      for (int i=cnt-1;i>=0 && i>cnt-60000;i--) {
        if (!addr_mapped(objs+i)) break;
        void* o=objs[i]; if(!o||!addr_mapped(o)) continue;
        char cn[96]="?"; class_name_of(o,cn,sizeof cn);
        if (strstr(cn,"Bs01BodyFire") || strstr(cn,"Bs01BodyFire_Light")) {
          // skip class/default objects
          if (strstr(cn,"Class") || strstr(cn,"Default__")) continue;
          fxactor=o; break;
        }
      }
      LOG("[mn9pawn] fire: existing bodyfire actor=%p\n", fxactor);
    }
    if (fxactor) {
      int ow=prop_offset(fxactor,"Owner"); if(ow>0) *(void**)((char*)fxactor+ow)=pn;
      int ba=prop_offset(fxactor,"Base"); if(ba>0) *(void**)((char*)fxactor+ba)=pn;
      int lo2=prop_offset(pn,"Location"), ro2=prop_offset(pn,"Rotation");
      int bl=prop_offset(fxactor,"Location"), br=prop_offset(fxactor,"Rotation");
      if(lo2>0&&bl>0) memcpy((char*)fxactor+bl,(char*)pn+lo2,12);
      if(ro2>0&&br>0) memcpy((char*)fxactor+br,(char*)pn+ro2,12);
      int hd=prop_offset(fxactor,"bHidden"); if(hd>0) *(unsigned char*)((char*)fxactor+hd)=0;
      static int moved;
      if (!moved) {
        moved=1;
        void** objs=*(void***)OBJ_ARRAY; int cnt=*(int*)OBJ_COUNT;
        int oc=0;
        for (int i=cnt-1;i>=0 && i>cnt-60000;i--) {
          if (!addr_mapped(objs+i)) break;
          void* o=objs[i]; if(!o||!addr_mapped(o)) continue;
          int ow2=prop_offset(o,"Owner"); if(ow2<=0||*(void**)((char*)o+ow2)!=fxactor) continue;
          oc++;
          int h2=prop_offset(o,"bHidden"); if(h2>0) *(unsigned char*)((char*)o+h2)=0;
          int on2=prop_offset(o,"bOwnerNoSee"); if(on2>0) *(unsigned char*)((char*)o+on2)=0;
          int l2=prop_offset(o,"Location"); if(l2>0&&lo2>0) memcpy((char*)o+l2,(char*)pn+lo2,12);
          char cn2[96]="?"; class_name_of(o,cn2,sizeof cn2);
          LOG("[mn9pawn] fire: comp %s o=%p\n", cn2, o);
        }
        LOG("[mn9pawn] fire: moved %d components\n", oc);
      }
      return;
    }
    if ((ft++ % 45) == 0) {
      findobj_fn find=(findobj_fn)FINDOBJ_ADDR;
      void* cls=find(NULL,NULL,L"MN9Game.MN9Shell_Bs01BodyFire",0);
      int lo=prop_offset(pn,"Location"), ro=prop_offset(pn,"Rotation");
      if (cls && lo>0 && ro>0) {
        spawn_fn sp=(spawn_fn)SPAWNACTOR_ADDR;
        void* fx=sp(world, cls, 0, (char*)pn+lo, (char*)pn+ro, pn, 1, 0, NULL, NULL, 0, NULL);
        if (fx) {
          int ba=prop_offset(fx,"Base"); if(ba>0) *(void**)((char*)fx+ba)=pn;
          int ow=prop_offset(fx,"Owner"); if(ow>0) *(void**)((char*)fx+ow)=pn;
          LOG("[mn9pawn] fire: spawned shell=%p\n", fx);
        } else LOG("[mn9pawn] fire: shell spawn failed cls=%p\n", cls);
      } else if (!cls) LOG("[mn9pawn] fire: class not found\n");
    }
    return;
  }
  if (g_driv_mode) {
    static int done;
    if (!done) {
      void* pn=find_player_pawn();
      if (pn) {
        int mo=prop_offset(pn,"Mesh"); void* comp=mo>0?*(void**)((char*)pn+mo):NULL;
        if (comp) {
          void* mesh=find_loaded_named("Bs01_Body1");
          void* tree=find_loaded_named("Bs01_AnimTree");
          void* aset=find_loaded_named("Bs01_AnimSet");
          if (!mesh) { ((void*(*)(void*, const wchar_t*, unsigned))0x0da0490UL)(NULL,L"Map11_OIL_Persistent00",0); mesh=find_loaded_named("Bs01_Body1"); }
          if (!aset) aset=find_loaded_named("Bs01_AnimSet");
          LOG("[mn9pawn] driv mesh=%p tree=%p aset=%p\n", mesh, tree, aset);
          if (mesh && tree) {
            done=1;
            int mc=prop_offset(comp,"Materials"); if(mc>0 && *(void***)((char*)comp+mc)) *(int*)((char*)comp+mc+8)=0;
            ((setskmesh_fn)SETSKMESH)(comp, mesh, 1);
            if (aset) {
              int ao=prop_offset(comp,"AnimSets");
              if (ao>0) { void** arr=*(void***)((char*)comp+ao); int n=*(int*)((char*)comp+ao+8), mx=*(int*)((char*)comp+ao+12);
                if (n>=mx){int nm2=mx?mx*2:8; arr=(void**)((void*(*)(void*,unsigned,unsigned))0xdb78a0UL)(arr,nm2*8,8); *(void***)((char*)comp+ao)=arr; *(int*)((char*)comp+ao+12)=nm2;}
                arr[n]=aset; *(int*)((char*)comp+ao+8)=n+1; LOG("[mn9pawn] driv animset appended at %d\n", n); }
            }
            ((setanim_fn)SETANIMTREE)(comp, tree); ((void(*)(void*, unsigned))0x115ac00UL)(comp,1);
            struct { void** data; int count; int max; } qa={0,0,0};
            ((void(*)(void*, void*, unsigned))0x17cc440UL)(tree,&qa,1);
            int played=0; uint64_t fr=fname("Bs01_Run");
            for (int i=0; qa.data && i<qa.count; i++) {
              void* nd=qa.data[i]; if(!nd||!addr_mapped(nd)) continue;
              char cn[96]="?"; class_name_of(nd,cn,sizeof cn);
              if (strstr(cn,"AnimNodeSequence")) {
                ((void(*)(void*, uint64_t))0x1198f30UL)(nd, fr);      // SetAnim
                ((void(*)(void*, unsigned, float, float))0x119f450UL)(nd, 1, 1.0f, 0.0f); // PlayAnim
                played++;
              }
            }
            LOG("[mn9pawn] driv played Bs01_Run on %d nodes\n", played);
          }
        }
      }
    }
    return;
  }
  if (g_archbs_mode) {
    static int done;
    if (!done) {
      findobj_fn find=(findobj_fn)FINDOBJ_ADDR;
      void* bsmesh=find_loaded_named("Bs01_Body1");
      if (!bsmesh) { ((void*(*)(void*, const wchar_t*, unsigned))0x0da0490UL)(NULL,L"Map11_OIL_Persistent00",0); bsmesh=find_loaded_named("Bs01_Body1"); }
      const wchar_t* archs[] = { L"MN9Game.Default__MN9Spw09Pawn", L"MN9Game.Default__MN9Pl01Pawn", NULL };
      int n=0;
      for (int i=0; archs[i]; i++) {
        void* a=find(NULL,NULL,archs[i],0);
        if (!a) { LOG("[mn9pawn] archbs missing %ls\n", archs[i]); continue; }
        int mo=prop_offset(a,"Mesh"); void* comp=mo>0?*(void**)((char*)a+mo):NULL;
        if (comp && bsmesh) {
          int sk=prop_offset(comp,"SkeletalMesh");
          if (sk>0) { *(void**)((char*)comp+sk)=bsmesh; n++; LOG("[mn9pawn] archbs %ls Mesh->%p\n", archs[i], bsmesh); }
        }
        const char* slots[3]={"BodyMeshNormal","BodyMeshDash","BodyMeshSwoop"};
        for (int k=0;k<3;k++){ int o=prop_offset(a,slots[k]); if(o>0 && bsmesh){ *(void**)((char*)a+o)=bsmesh; } }
      }
      if (n) { done=1; LOG("[mn9pawn] archbs done (%d)\n", n); }
    }
    return;
  }
  if (g_meshonly_mode) {
    static int done;
    if (!done) {
      void* pn=find_player_pawn();
      if (pn) {
        int mo=prop_offset(pn,"Mesh"); void* comp=mo>0?*(void**)((char*)pn+mo):NULL;
        if (comp) {
          void* mesh=find_loaded_named("Bs01_Body1");
          if (!mesh) { ((void*(*)(void*, const wchar_t*, unsigned))0x0da0490UL)(NULL,L"Map11_OIL_Persistent00",0); mesh=find_loaded_named("Bs01_Body1"); }
          if (mesh) {
            done=1;
            int mc=prop_offset(comp,"Materials");
            LOG("[mn9pawn] meshonly comp=%p mesh=%p matsOff=%d matsN=%d\n", comp, mesh, mc, mc>0?*(int*)((char*)comp+mc+8):-1);
            if (mc>0) *(int*)((char*)comp+mc+8)=0;
            ((setskmesh_fn)SETSKMESH)(comp, mesh, 1);
            ((void(*)(void*, unsigned))0x115ac00UL)(comp, 1);   // InitAnimTree for new skeleton
            LOG("[mn9pawn] meshonly applied + InitAnimTree\n");
          }
        }
      }
    }
    return;
  }
  if (g_banim_mode) {
    static int done;
    if (!done) {
      void* pn=find_player_pawn();
      if (pn) {
        int mo=prop_offset(pn,"Mesh"); void* comp=mo>0?*(void**)((char*)pn+mo):NULL;
        if (comp) {
          void* mesh=find_loaded_named("Bs01_Body1");
          void* tree=find_loaded_named("Spw09_AnimTree");
          void* aset=find_loaded_named("Spw09_AnimSet");
          if (!mesh) { ((void*(*)(void*, const wchar_t*, unsigned))0x0da0490UL)(NULL,L"Map11_OIL_Persistent00",0); mesh=find_loaded_named("Bs01_Body1"); }
          LOG("[mn9pawn] banim mesh=%p tree=%p animset=%p\n", mesh, tree, aset);
          if (mesh) {
            done=1;
            int mc=prop_offset(comp,"Materials"); if(mc>0 && *(void***)((char*)comp+mc)) *(int*)((char*)comp+mc+8)=0;
            ((setskmesh_fn)SETSKMESH)(comp, mesh, 1);
            int ao=prop_offset(comp,"AnimSets");
            if (ao>0 && aset) {
              void** arr=*(void***)((char*)comp+ao); int n=*(int*)((char*)comp+ao+8), mx=*(int*)((char*)comp+ao+12);
              LOG("[mn9pawn] banim AnimSets n=%d max=%d arr=%p\n", n, mx, arr);
              if (n>=mx) { int nm2=mx?mx*2:8; arr=(void**)((void*(*)(void*,unsigned,unsigned))0xdb78a0UL)(arr,nm2*8,8); *(void***)((char*)comp+ao)=arr; *(int*)((char*)comp+ao+12)=nm2; }
              arr[n]=aset; *(int*)((char*)comp+ao+8)=n+1;
              LOG("[mn9pawn] banim appended AnimSet at %d\n", n);
            }
            if (tree) { ((setanim_fn)SETANIMTREE)(comp, tree); ((void(*)(void*, unsigned))0x115ac00UL)(comp,1); }
            LOG("[mn9pawn] banim applied tree\n");
          }
        }
      }
    }
    return;
  }
  if (g_mimic_mode) {
    static int init; static int prep; static void* tgt; static void* bp_pawn; static int map[80]; static float bSinv[80][12]; static float bT[80][12]; static char* tnames[80]; static int hid;
    void* pn=find_player_pawn();
    if (!pn) return;
    int mo=prop_offset(pn,"Mesh");
    void* src=mo>0?*(void**)((char*)pn+mo):NULL;
    if (!src) return;
    if (!init) {
      init=1;
      findobj_fn find=(findobj_fn)FINDOBJ_ADDR;
      void* bcls=find(NULL,NULL,L"MN9Game.MN9Bs01Pawn",0);
      void* mesh=find_loaded_named("Bs01_Body1");
      if (!mesh) { ((void*(*)(void*, const wchar_t*, unsigned))0x0da0490UL)(NULL,L"Map11_OIL_Persistent00",0); mesh=find_loaded_named("Bs01_Body1"); }
      LOG("[mn9pawn] mimic cls=%p mesh=%p src=%p\n", bcls, mesh, src);
      if (!bcls || !mesh) return;
      int lo=prop_offset(pn,"Location"), ro=prop_offset(pn,"Rotation");
      spawn_fn sp=(spawn_fn)SPAWNACTOR_ADDR;
      void* bp=sp(world,bcls,0,(char*)pn+lo,(char*)pn+ro,NULL,1,0,NULL,NULL,0,NULL);
      LOG("[mn9pawn] mimic spawned=%p\n", bp);
      if (!bp) return;
      bp_pawn=bp;
      int ow=prop_offset(bp,"Owner"); if(ow>0) *(void**)((char*)bp+ow)=pn;
      int ba=prop_offset(bp,"Base"); if(ba>0) *(void**)((char*)bp+ba)=pn;
      int ha=prop_offset(bp,"bHardAttach"); if(ha>0) *(unsigned char*)((char*)bp+ha)=1;
      int hd=prop_offset(bp,"bHidden"); if(hd>0) *(unsigned char*)((char*)bp+hd)=0;
      // component selection is deferred until after spawn initializes them
    }
    if (!prep && bp_pawn) {
      static int waits;
      if (++waits > 20) {
        void** objs=*(void***)OBJ_ARRAY; int cnt=*(int*)OBJ_COUNT;
        int best=-1; void* bestc=NULL; int bestn=-1;
        for (int i=cnt-1;i>=0 && i>cnt-30000;i--) {
          if (!addr_mapped(objs+i)) break;
          void* o=objs[i]; if(!o||!addr_mapped(o)) continue;
          int ow2=prop_offset(o,"Owner"); if(ow2<=0||*(void**)((char*)o+ow2)!=bp_pawn) continue;
          int sk=prop_offset(o,"SkeletalMesh"); if(sk<=0) continue;
          void* cur=*(void**)((char*)o+sk);
          char nm[160]="?"; path_of(cur,nm,sizeof nm);
          int ac=*(int*)((char*)o+0x300);
          LOG("[mn9pawn] mimic comp %p mesh='%s' atoms=%d\n", o, nm, ac);
          int score = (nm[0]?2:0) + (strstr(nm,"Bs01_Body1")?10:0) + (ac>0?1:0);
          if (score>bestn) { bestn=score; best=i; bestc=o; }
        }
        // hide all other owned skeletal comps, configure the chosen one
        for (int i=cnt-1;i>=0 && i>cnt-30000;i--) {
          if (!addr_mapped(objs+i)) break;
          void* o=objs[i]; if(!o||!addr_mapped(o)) continue;
          int ow2=prop_offset(o,"Owner"); if(ow2<=0||*(void**)((char*)o+ow2)!=bp_pawn) continue;
          int sk=prop_offset(o,"SkeletalMesh"); if(sk<=0) continue;
          if (o!=bestc) { ((void(*)(void*, unsigned))0x14801b0UL)(o,1); }
        }
        if (bestc) {
          void* mesh=find_loaded_named("Bs01_Body1");
          if (!mesh) { ((void*(*)(void*, const wchar_t*, unsigned))0x0da0490UL)(NULL,L"Map11_OIL_Persistent00",0); mesh=find_loaded_named("Bs01_Body1"); }
          if (mesh) ((setskmesh_fn)SETSKMESH)(bestc, mesh, 1);
          int mc=prop_offset(bestc,"Materials"); if(mc>0 && *(void***)((char*)bestc+mc)) *(int*)((char*)bestc+mc+8)=0;
          ((setanim_fn)SETANIMTREE)(bestc, NULL);
          ((void(*)(void*, float, unsigned))0x115b6c0UL)(bestc, 0.0f, 1);
          int p1=prop_offset(bestc,"bPauseAnims"); if(p1>0) *(unsigned char*)((char*)bestc+p1)=1;
          int p2=prop_offset(bestc,"bNoSkeletonUpdate"); if(p2>0) *(unsigned char*)((char*)bestc+p2)=1;
          tgt=bestc; prep=1;
          LOG("[mn9pawn] mimic chosen comp=%p score=%d mesh=%p\n", bestc, bestn, mesh);
          // derive bone correspondence by matching bind-pose atom positions
          int at=prop_offset(src,"AnimTreeTemplate");
          void* savedTree = at>0 ? *(void**)((char*)src+at) : NULL;
          ((setanim_fn)SETANIMTREE)(src, NULL);
          ((void(*)(void*, float, unsigned))0x115b6c0UL)(src, 0.0f, 1);
          void* sa=*(void**)((char*)src+0x2f8); int sc=*(int*)((char*)src+0x300);
          void* ta=*(void**)((char*)bestc+0x2f8); int tc=*(int*)((char*)bestc+0x300);
          LOG("[mn9pawn] mimic map srcAtoms=%p n=%d tgtAtoms=%p n=%d\n", sa,sc,ta,tc);
          for (int i=0;i<80;i++) map[i]=-1;
          if (sa && ta) {
            for (int j=0;j<sc && j<80;j++) { float M[12]; atm((float*)((char*)sa+j*48), M); minv(M, bSinv[j]); }
            for (int i=0;i<tc && i<80;i++) { float M[12]; atm((float*)((char*)ta+i*48), M); for(int k=0;k<12;k++) bT[i][k]=M[k]; }
            for (int i=0;i<tc && i<80;i++) {
              float* tp=(float*)((char*)ta + i*48);
              float tx=tp[4],ty=tp[5],tz=tp[6];
              int best=-1; double bd=1e30;
              for (int j=0;j<sc;j++) {
                float* sp=(float*)((char*)sa + j*48);
                double dx=sp[4]-tx, dy=sp[5]-ty, dz=sp[6]-tz;
                double d=dx*dx+dy*dy+dz*dz;
                if (d<bd) { bd=d; best=j; }
              }
              map[i]=best;
            }
            for (int i=0;i<12;i++) LOG("[mn9pawn] mimic map tgt%d -> src%d\n", i, map[i]);
          }
          if (savedTree) { ((setanim_fn)SETANIMTREE)(src, savedTree); ((void(*)(void*, unsigned))0x115ac00UL)(src, 1); }
        }
      }
    }
    if (!tgt || !bp_pawn) return;    if (!tgt || !bp_pawn) return;
    int lo=prop_offset(pn,"Location"), ro=prop_offset(pn,"Rotation");
    int bl=prop_offset(bp_pawn,"Location"), br=prop_offset(bp_pawn,"Rotation");
    if (lo>0&&bl>0) memcpy((char*)bp_pawn+bl,(char*)pn+lo,12);
    if (ro>0&&br>0) memcpy((char*)bp_pawn+br,(char*)pn+ro,12);
    void* harr=*(void**)((char*)src+0x2f8); int hcnt=*(int*)((char*)src+0x300);
    void* barr=*(void**)((char*)tgt+0x2f8); int bcnt=*(int*)((char*)tgt+0x300);
    static unsigned d; if((d++%180)==0) LOG("[mn9pawn] mimic ptrs harr=%p hcnt=%d barr=%p bcnt=%d\n",harr,hcnt,barr,bcnt);
    if(!harr||!barr||hcnt<=0||bcnt<=0) return;
    int copied=0;
    for (int i=0;i<bcnt && i<80;i++){
      int j=map[i];
      if (j<0 || j>=hcnt) continue;
      float P[12], D[12], O[12];
      atm((float*)((char*)harr+j*48), P);
      mmul(P, bSinv[j], D);
      mmul(D, bT[i], O);
      mtoa(O, (float*)((char*)barr+i*48));
      copied++;
    }
    static unsigned rc; if((rc++%30)==0){
      float* b6=(float*)((char*)barr+6*48); float* b18=(float*)((char*)barr+18*48);
      LOG("[mn9pawn] mimic t=%u copied=%d head=(%.1f,%.1f,%.1f) hand=(%.1f,%.1f,%.1f)\n",
          rc,copied,b6[4],b6[5],b6[6],b18[4],b18[5],b18[6]);
    }
    if(!hid && copied>0){ hid=1;
      int om=prop_offset(pn,"Mesh");
      void* bm=om>0?*(void**)((char*)pn+om):NULL;
      if (bm) ((void(*)(void*, unsigned))0x14801b0UL)(bm,1);
      const char* cps[2]={"ArmLMeshComponent","ArmRMeshComponent"};
      for(int k=0;k<2;k++){int o2=prop_offset(pn,cps[k]); if(o2>0){void* c=*(void**)((char*)pn+o2); if(c) ((void(*)(void*, unsigned))0x14801b0UL)(c,1);} }
      LOG("[mn9pawn] mimic hid beck body/arms\n");
    }
    return;
  }
  if (g_vis_mode) {
    static int init, paused; static void* tgt; static void* bp_pawn; static int map[80];
    void* pn=find_player_pawn();
    if (!pn) return;
    int mo=prop_offset(pn,"Mesh");
    void* src=mo>0?*(void**)((char*)pn+mo):NULL;
    if (!src) return;
    if (!init) {
      init=1;
      int ons=prop_offset(src,"bOwnerNoSee"); if(ons>0) *(unsigned char*)((char*)src+ons)=1;
      findobj_fn find=(findobj_fn)FINDOBJ_ADDR;
      void* bcls=find(NULL,NULL,L"MN9Game.MN9Bs01Pawn",0);
      void* mesh=find_loaded_named("Bs01_Body1");
      if (!mesh) { ((void*(*)(void*, const wchar_t*, unsigned))0x0da0490UL)(NULL,L"Map11_OIL_Persistent00",0); mesh=find_loaded_named("Bs01_Body1"); }
      LOG("[mn9pawn] vis cls=%p mesh=%p\n", bcls, mesh);
      if (bcls) {
        int lo=prop_offset(pn,"Location"), ro=prop_offset(pn,"Rotation");
        spawn_fn sp=(spawn_fn)SPAWNACTOR_ADDR;
        void* bp=sp(world,bcls,0,(char*)pn+lo,(char*)pn+ro,NULL,1,0,NULL,NULL,0,NULL);
        LOG("[mn9pawn] vis spawned=%p\n", bp);
        if (bp) {
          bp_pawn=bp;
          int ow=prop_offset(bp,"Owner"); if(ow>0) *(void**)((char*)bp+ow)=pn;
          int ba=prop_offset(bp,"Base"); if(ba>0) *(void**)((char*)bp+ba)=pn;
          int ha=prop_offset(bp,"bHardAttach"); if(ha>0) *(unsigned char*)((char*)bp+ha)=1;
          int hd=prop_offset(bp,"bHidden"); if(hd>0) *(unsigned char*)((char*)bp+hd)=0;
          void** objs=*(void***)OBJ_ARRAY; int cnt=*(int*)OBJ_COUNT;
          for (int i=cnt-1;i>=0 && i>cnt-30000;i--) {
            if (!addr_mapped(objs+i)) break;
            void* o=objs[i]; if(!o||!addr_mapped(o)) continue;
            int ow2=prop_offset(o,"Owner"); if(ow2<=0||*(void**)((char*)o+ow2)!=bp) continue;
            int sk=prop_offset(o,"SkeletalMesh"); if(sk<=0) continue;
            if (mesh) ((setskmesh_fn)SETSKMESH)(o, mesh, 0);
            int mc=prop_offset(o,"Materials"); if(mc>0) *(int*)((char*)o+mc+8)=0;
            ((setanim_fn)SETANIMTREE)(o, NULL);
            int on2=prop_offset(o,"bOwnerNoSee"); if(on2>0) *(unsigned char*)((char*)o+on2)=0;
            tgt=o; LOG("[mn9pawn] vis target comp=%p\n", o); break;
          }
        }
      }
    }
    if (!tgt || !bp_pawn) return;
    int lo=prop_offset(pn,"Location"), ro=prop_offset(pn,"Rotation");
    int bl=prop_offset(bp_pawn,"Location"), br=prop_offset(bp_pawn,"Rotation");
    if (lo>0&&bl>0) memcpy((char*)bp_pawn+bl,(char*)pn+lo,12);
    if (ro>0&&br>0) memcpy((char*)bp_pawn+br,(char*)pn+ro,12);
    void* harr=*(void**)((char*)src+0x2f8); int hcnt=*(int*)((char*)src+0x300);
    void* barr=*(void**)((char*)tgt+0x2f8); int bcnt=*(int*)((char*)tgt+0x300);
    static unsigned d;
    if ((d++%180)==0) LOG("[mn9pawn] vis ptrs harr=%p hcnt=%d barr=%p bcnt=%d\n", harr,hcnt,barr,bcnt);
    if (!harr||!barr||hcnt<=0||bcnt<=0) return;
    int copied=0;
    for (int i=0;i<bcnt && i<80;i++){
      int j=map[i];
      if (j<0 || j>=hcnt) continue;
      memcpy((char*)barr+i*48,(char*)harr+j*48,48);
      copied++;
    }
    static unsigned rc; if((rc++%30)==0){
      float* b6=(float*)((char*)barr+6*48); float* b18=(float*)((char*)barr+18*48);
      LOG("[mn9pawn] vis t=%u copied=%d head=(%.1f,%.1f,%.1f) hand=(%.1f,%.1f,%.1f)\n",
          rc,copied,b6[4],b6[5],b6[6],b18[4],b18[5],b18[6]);
    }
    if(!paused&&copied>0){ paused=1;
      int p1=prop_offset(tgt,"bPauseAnims"); if(p1>0) *(unsigned char*)((char*)tgt+p1)=1;
      int p2=prop_offset(tgt,"bNoSkeletonUpdate"); if(p2>0) *(unsigned char*)((char*)tgt+p2)=1;
      LOG("[mn9pawn] vis paused target (p1=%d p2=%d)\n",p1,p2);
    }
    return;
  }
  if (g_classbs_mode) {
    static int done;
    if (!done) {
      findobj_fn find=(findobj_fn)FINDOBJ_ADDR;
      void* beck=find(NULL,NULL,L"MN9Game.MN9Pl01Pawn",0);
      void* spw09=find(NULL,NULL,L"MN9Game.MN9Spw09Pawn",0);
      void* tgt=find(NULL,NULL,L"MN9Game.MN9Bs01Pawn",0);
      if (beck && spw09 && tgt) {
        done=1;
        g_beck_class_pawn=beck; g_spw09_class=spw09; g_spw01_class=tgt;
        arm_spawn_stub(beck, spw09, tgt);
        static const wchar_t* gis[]={
          L"MN9Game.Default__MN9GameInfo_Challenge",
          L"MN9Game.Default__MN9GameInfo_EX",
          L"MN9Game.Default__MN9GameInfo",
          L"MN9OnlineGame.Default__MN9OnlineGame", NULL };
        for (int i=0; gis[i]; i++) {
          void* gi=find(NULL,NULL,gis[i],0);
          if (!gi) continue;
          int o=prop_offset(gi,"DefaultPawnClass");
          if (o>0) { *(void**)((char*)gi+o)=tgt; LOG("[mn9pawn] classbs: %ls DefaultPawnClass=%p\n", gis[i], tgt); }
        }
        LOG("[mn9pawn] classbs armed beck=%p tgt=%p\n", beck, tgt);
      } else if ((g_ticks%600)==0) LOG("[mn9pawn] classbs waiting beck=%p spw09=%p tgt=%p\n", beck, spw09, tgt);
    }
    return;
  }
  if (g_comps_mode) {
    static int done;
    if (!done) {
      void* pn = find_player_pawn();
      if (!pn) return;
      done = 1;
      void** objs=*(void***)OBJ_ARRAY; int cnt=*(int*)OBJ_COUNT;
      LOG("[mn9pawn] comps pawn=%p\n", pn);
      for (int i=cnt-1;i>=0 && i>cnt-40000;i--) {
        if (!addr_mapped(objs+i)) break;
        void* o=objs[i]; if(!o||!addr_mapped(o)) continue;
        int ow=prop_offset(o,"Owner"); if(ow<=0||*(void**)((char*)o+ow)!=pn) continue;
        int sk=prop_offset(o,"SkeletalMesh"); if(sk<=0) continue;
        void* mesh=*(void**)((char*)o+sk);
        char nm[128]="?"; path_of(mesh,nm,sizeof nm);
        void* arr=*(void**)((char*)o+0x2f8); int ac=*(int*)((char*)o+0x300);
        char cn[96]="?"; class_name_of(o,cn,sizeof cn);
        LOG("[mn9pawn] comp %s o=%p mesh='%s' atoms=%p n=%d\n", cn, o, nm, arr, ac);
      }
    }
    return;
  }
  if (g_pose_mode) {
    static int init; static void* clone;
    void* pn = find_player_pawn();
    if (!pn) return;
    int mo = prop_offset(pn, "Mesh");
    void* src = mo > 0 ? *(void**)((char*)pn + mo) : NULL;
    if (!src) return;
    if (!init) {
      init = 1;
      findobj_fn find = (findobj_fn)FINDOBJ_ADDR;
      void* cls = find(NULL, NULL, L"Engine.SkeletalMeshComponent", 0);
      if (!cls) { LOG("[mn9pawn] pose: no class\n"); return; }
      void* (*sco)(void*, void*, uint64_t, unsigned long, void*, void*, void*, void*) =
        (void*(*)(void*,void*,uint64_t,unsigned long,void*,void*,void*,void*))0xd36650UL;
      clone = sco(cls, pn, 0, 0, src, NULL, NULL, NULL);
      LOG("[mn9pawn] pose clone=%p cls=%p src=%p\n", clone, cls, src);
      if (!clone) return;
      int ons = prop_offset(clone, "bOwnerNoSee");
      if (ons > 0) *(unsigned char*)((char*)clone + ons) = 1;
      // real scene registration (scene pointer from the existing Mesh comp)
      void* scene = *(void**)((char*)src + 0x70);
      void* own = *(void**)((char*)src + 0x78);
      LOG("[mn9pawn] pose scene=%p owner78=%p\n", scene, own);
      float ident[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
      ((void(*)(void*, void*, void*, void*))0x184d0b0UL)(clone, scene, pn, ident);
      LOG("[mn9pawn] pose conditional-attached\n");
      // append to pawn Components array
      int co = prop_offset(pn, "Components");
      if (co > 0) {
        void** arr = *(void***)((char*)pn + co);
        int n = *(int*)((char*)pn + co + 8), mx = *(int*)((char*)pn + co + 12);
        LOG("[mn9pawn] pose components n=%d max=%d arr=%p\n", n, mx, arr);
        if (n >= mx) {
          int nm = mx ? mx*2 : 16;
          arr = (void**)((void*(*)(void*,unsigned,unsigned))0xdb78a0UL)(arr, nm*8, 8);
          *(void***)((char*)pn + co) = arr;
          *(int*)((char*)pn + co + 12) = nm;
        }
        arr[n] = clone;
        *(int*)((char*)pn + co + 8) = n + 1;
        LOG("[mn9pawn] pose appended to components at %d\n", n);
      }
    }
    if (!clone) return;
    void* arr = *(void**)((char*)clone + 0x2f8);
    int cnt = *(int*)((char*)clone + 0x300);
    static unsigned t;
    if ((t++ % 30) == 0) {
      float hx=0,hy=0,hz=0;
      if (arr && cnt > 6) { float* b=(float*)((char*)arr+6*48); hx=b[4];hy=b[5];hz=b[6]; }
      LOG("[mn9pawn] pose t=%u arr=%p cnt=%d head=(%.2f,%.2f,%.2f)\n", t, arr, cnt, hx,hy,hz);
    }
    return;
  }
  if (g_puppet_mode) {
    static int init, paused;
    static void* tgt_comp;
    static char* trueName[80];
    void* pn = find_player_pawn();
    if (!pn) return;
    int mo = prop_offset(pn, "Mesh");
    void* src = mo > 0 ? *(void**)((char*)pn + mo) : NULL;
    if (!src) return;
    if (!init) {
      init = 1;
      // keep source on Beck mesh, hidden
      void* beck = find_loaded_named("Spw09_Body1");
      int so = prop_offset(src, "SkeletalMesh");
      if (beck && so > 0 && *(void**)((char*)src+so) != beck)
        ((setskmesh_fn)SETSKMESH)(src, beck, 1);
      int ons = prop_offset(src, "bOwnerNoSee");
      if (ons > 0) *(unsigned char*)((char*)src + ons) = 1;
      // spawn attached render actor
      findobj_fn find = (findobj_fn)FINDOBJ_ADDR;
      void* cls = find(NULL, NULL, L"MN9Game.Spw09SkeletalMeshCinematicActor", 0);
      LOG("[mn9pawn] puppet src=%p hide=%d cls=%p\n", src, ons, cls);
      if (cls) {
        int lo = prop_offset(pn, "Location"), ro = prop_offset(pn, "Rotation");
        spawn_fn sp = (spawn_fn)SPAWNACTOR_ADDR;
        void* bp = sp(world, cls, 0, (char*)pn+lo, (char*)pn+ro, NULL, 1, 0, NULL, NULL, 0, NULL);
        LOG("[mn9pawn] puppet spawned=%p\n", bp);
        if (bp) {
          int ow=prop_offset(bp,"Owner"); if(ow>0) *(void**)((char*)bp+ow)=pn;
          int ba=prop_offset(bp,"Base"); if(ba>0) *(void**)((char*)bp+ba)=pn;
          int ha=prop_offset(bp,"bHardAttach"); if(ha>0) *(unsigned char*)((char*)bp+ha)=1;
          int hd=prop_offset(bp,"bHidden"); if(hd>0) *(unsigned char*)((char*)bp+hd)=0;
          void* mesh = find_loaded_named("Bs01_Body1");
          if (!mesh) { ((void*(*)(void*, const wchar_t*, unsigned))0x0da0490UL)(NULL,L"Map11_OIL_Persistent00",0); mesh=find_loaded_named("Bs01_Body1"); }
          // find comps owned by bp
          void** objs=*(void***)OBJ_ARRAY; int cnt=*(int*)OBJ_COUNT;
          for (int i=cnt-1;i>=0 && i>cnt-30000;i--) {
            if (!addr_mapped(objs+i)) break;
            void* o=objs[i]; if(!o||!addr_mapped(o)) continue;
            int ow2=prop_offset(o,"Owner"); if(ow2<=0||*(void**)((char*)o+ow2)!=bp) continue;
            int sk=prop_offset(o,"SkeletalMesh"); if(sk<=0) continue;
            if (mesh) ((setskmesh_fn)SETSKMESH)(o, mesh, 0);
            ((setanim_fn)SETANIMTREE)(o, NULL);
            int mc=prop_offset(o,"Materials"); if(mc>0) *(int*)((char*)o+mc+8)=0;
            int ons2=prop_offset(o,"bOwnerNoSee"); if(ons2>0) *(unsigned char*)((char*)o+ons2)=0;
            tgt_comp=o;
            LOG("[mn9pawn] puppet target comp=%p mesh=%p\n", o, mesh);
            break;
          }
          // build name map from Bs01 mesh
          if (mesh) for (int i=0;i<70;i++) {
            int idx=((int(*)(void*, uint64_t))0x17a4ec0UL)(mesh, fname(BS01_NAMES[i]));
            if (idx>=0 && idx<80 && !trueName[idx]) trueName[idx]=(char*)BS01_NAMES[i];
          }
        }
      }
    }
    if (!tgt_comp) return;
    int lo=prop_offset(pn,"Location"), ro=prop_offset(pn,"Rotation");
    int bl=prop_offset((void*)((char*)0), "x"); // unused
    (void)bl;
    // keep attached transform in sync
    int tl=prop_offset(pn,"Location"), tr=prop_offset(pn,"Rotation");
    (void)tl;(void)tr;
    void* harr=*(void**)((char*)src+0x2f8); int hcnt=*(int*)((char*)src+0x300);
    void* barr=*(void**)((char*)tgt_comp+0x2f8); int bcnt=*(int*)((char*)tgt_comp+0x300);
    if (!harr || !barr || hcnt<=0 || bcnt<=0) {
      static unsigned d; if((d++%300)==0) LOG("[mn9pawn] puppet ptrs harr=%p hcnt=%d barr=%p bcnt=%d\n",harr,hcnt,barr,bcnt);
      return;
    }
    // build beck name->index map on src mesh each tick (cheap)
    int copied=0;
    void* smesh = *(void**)((char*)src + (prop_offset(src,"SkeletalMesh")>0?prop_offset(src,"SkeletalMesh"):0));
    for (int i=0;i<bcnt && i<80;i++) {
      if (!trueName[i]) continue;
      int j = ((int(*)(void*, uint64_t))0x17a4ec0UL)(smesh, fname(trueName[i]));
      if (j<0 || j>=hcnt) continue;
      memcpy((char*)barr+i*48,(char*)harr+j*48,48);
      copied++;
    }
    static unsigned rc; if ((rc++%30)==0){
      float* b6=(float*)((char*)barr+6*48); float* b18=(float*)((char*)barr+18*48); float* b60=(float*)((char*)barr+60*48);
      LOG("[mn9pawn] puppet t=%u copied=%d head=(%.2f,%.2f,%.2f) hand=(%.2f,%.2f,%.2f) leg=(%.2f,%.2f,%.2f)\n",
          rc, copied, b6[4],b6[5],b6[6], b18[4],b18[5],b18[6], b60[4],b60[5],b60[6]);
    }
    if (!paused && copied>0){
      paused=1;
      int p1=prop_offset(tgt_comp,"bPauseAnims"); if(p1>0) *(unsigned char*)((char*)tgt_comp+p1)=1;
      int p2=prop_offset(tgt_comp,"bNoSkeletonUpdate"); if(p2>0) *(unsigned char*)((char*)tgt_comp+p2)=1;
      LOG("[mn9pawn] puppet paused target update (p1=%d p2=%d)\n", p1, p2);
    }
    return;
  }
  if (g_gb_mode) {
    static int done;
    if (!done) {
      void* pn = find_player_pawn();
      if (pn) {
        int mo = prop_offset(pn, "Mesh");
        void* comp = mo > 0 ? *(void**)((char*)pn + mo) : NULL;
        if (comp) {
          done = 1;
          float m[16]; for (int i=0;i<16;i++) m[i]=12345.0f;
          ((void(*)(void*, void*, unsigned))0x115dfa0UL)(m, comp, 3);
          void* mesh = *(void**)((char*)comp + 0x250);
          LOG("[mn9pawn] gb comp=%p mesh250=%p m360=%p req368=%p cnt370=%d\n",
              comp, mesh, *(void**)((char*)comp+0x360), *(void**)((char*)comp+0x368), *(int*)((char*)comp+0x370));
          LOG("[mn9pawn] gb after call m[0..3]=%.3f %.3f %.3f %.3f m[12..14]=%.3f %.3f %.3f\n",
              m[0],m[1],m[2],m[3],m[12],m[13],m[14]);
          LOG("[mn9pawn] gb comp+0x2f8=%p cnt+0x300=%d\n", *(void**)((char*)comp+0x2f8), *(int*)((char*)comp+0x300));
          if (mesh) {
            LOG("[mn9pawn] gb mesh+0x2f8=%p cnt+0x300=%d\n", *(void**)((char*)mesh+0x2f8), *(int*)((char*)mesh+0x300));
            void* d = *(void**)((char*)mesh+0x2f8);
            int n = *(int*)((char*)mesh+0x300);
            if (d && addr_mapped(d) && n>0 && n<200) {
              LOG("[mn9pawn] gb meshmat first:");
              for (int k=0;k<8;k++) LOG(" %.2f", ((float*)d)[k]);
              LOG("\n");
            }
          }
        }
      }
    }
    return;
  }
  if (g_rb_mode) {
    static unsigned t;
    if ((t++ % 300) != 0) return;
    void* pn = find_player_pawn();
    if (!pn) return;
    int mo = prop_offset(pn, "Mesh");
    void* comp = mo > 0 ? *(void**)((char*)pn + mo) : NULL;
    if (!comp) return;
    void* m = *(void**)((char*)comp + 0x360);
    void* req = *(void**)((char*)comp + 0x368);
    int cnt = *(int*)((char*)comp + 0x370);
    float mm[16];
    ((void(*)(void*, void*, unsigned))0x115dfa0UL)(mm, comp, 3);
    LOG("[mn9pawn] rb t=%u comp=%p m360=%p req368=%p cnt370=%d M3=(%.1f,%.1f,%.1f)\n",
        t, comp, m, req, cnt, mm[12], mm[13], mm[14]);
    if (req && addr_mapped(req)) {
      LOG("[mn9pawn] rb req first:");
      for (int i=0;i<12;i++) LOG(" %d", ((int*)req)[i]);
      LOG("\n");
    }
    for (int off = 0x200; off < 0x700; off += 8) {
      void* q = *(void**)((char*)comp + off);
      if (!q || !addr_mapped(q)) continue;
      int n = *(int*)((char*)comp + off + 8);
      if (n < 60 || n > 80) continue;
      if (!addr_mapped((char*)q + 4)) continue;
      int a0=((int*)q)[0], a1=((int*)q)[1], a2=((int*)q)[2];
      if (a0<0||a0>90||a1<0||a1>90||a2<0||a2>90) continue;
      LOG("[mn9pawn] rb cand off=0x%x q=%p n=%d first %d %d %d\n", off, q, n, a0,a1,a2);
    }
    return;
  }
  if (g_retarget_mode) {
    static int init;
    static char* trueName[80];
    static int beckOf[80];
    void* pn = find_player_pawn();
    if (!pn) return;
    int mo = prop_offset(pn, "Mesh");
    void* comp = mo > 0 ? *(void**)((char*)pn + mo) : NULL;
    int hoff = prop_offset(pn, "ArmLMeshComponent");
    void* hand = hoff > 0 ? *(void**)((char*)pn + hoff) : NULL;
    if (!comp || !hand) return;
    if (!init) {
      void* mesh = find_loaded_named("Bs01_Body1");
      if (!mesh) {
        ((void*(*)(void*, const wchar_t*, unsigned))0x0da0490UL)(NULL, L"Map11_OIL_Persistent00", 0);
        mesh = find_loaded_named("Bs01_Body1");
      }
      if (!mesh) return;
      init = 1;
      int mc = prop_offset(comp, "Materials");
      if (mc > 0) *(int*)((char*)comp + mc + 8) = 0;
      ((setskmesh_fn)SETSKMESH)(comp, mesh, 1);
      ((setanim_fn)SETANIMTREE)(comp, NULL);   // stop Beck tree driving Bs01 bones

      // build true Bs01 index->name via MatchRefBone, and Beck name->index
      for (int i = 0; i < 70; i++) {
        int idx = ((int(*)(void*, uint64_t))0x17a4ec0UL)(mesh, fname(BS01_NAMES[i]));
        if (idx >= 0 && idx < 80 && !trueName[idx]) trueName[idx] = (char*)BS01_NAMES[i];
      }
      int hso = prop_offset(hand, "SkeletalMesh");
      void* hmesh = hso > 0 ? *(void**)((char*)hand + hso) : NULL;
      for (int i = 0; i < 80; i++) beckOf[i] = -1;
      if (hmesh) {
        for (int i = 0; i < 68; i++) {
          int idx = ((int(*)(void*, uint64_t))0x17a4ec0UL)(hmesh, fname(BECK_NAMES[i]));
          if (idx >= 0 && idx < 80) beckOf[i] = idx;
        }
      }
      LOG("[mn9pawn] retarget init mesh=%p hand=%p hmesh=%p\n", mesh, hand, hmesh);
      int filled = 0; for (int i=0;i<80;i++) if (trueName[i]) filled++;
      LOG("[mn9pawn] retarget names mapped=%d\n", filled);
    }
    void* harr = *(void**)((char*)hand + 0x2f8);
    int hcnt = *(int*)((char*)hand + 0x300);
    void* barr = *(void**)((char*)comp + 0x2f8);
    int bcnt = *(int*)((char*)comp + 0x300);
    static unsigned dbg;
    if ((dbg++ % 300) == 0) LOG("[mn9pawn] retarget ptrs harr=%p hcnt=%d barr=%p bcnt=%d\n", harr, hcnt, barr, bcnt);
    if (!harr || !barr || hcnt <= 0 || bcnt <= 0) return;
    int copied = 0;
    for (int i = 0; i < bcnt && i < 80; i++) {
      if (!trueName[i]) continue;
      int j = -1;
      for (int k = 0; k < 68; k++) if (!strcmp(BECK_NAMES[k], trueName[i])) { j = beckOf[k]; break; }
      if (j < 0 || j >= hcnt) continue;
      memcpy((char*)barr + i * 48, (char*)harr + j * 48, 48);
      copied++;
    }
    static int paused;
    if (!paused && copied > 0) {
      paused = 1;
      int pp = prop_offset(comp, "bPauseAnims");
      if (pp > 0) *(unsigned char*)((char*)comp + pp) = 1;
      int pn2 = prop_offset(comp, "bNoSkeletonUpdate");
      if (pn2 > 0) *(unsigned char*)((char*)comp + pn2) = 1;
      LOG("[mn9pawn] retarget paused own update (pp=%d pn=%d) copied=%d\n", pp, pn2, copied);
    }
    static unsigned rc;
    if ((rc++ % 30) == 0) {
      float* b6=(float*)((char*)barr+6*48); float* b18=(float*)((char*)barr+18*48);
      float* b60=(float*)((char*)barr+60*48); float* b65=(float*)((char*)barr+65*48);
      LOG("[mn9pawn] retarget t=%u copied=%d head=(%.1f,%.1f,%.1f) hand=(%.1f,%.1f,%.1f) legL=(%.1f,%.1f,%.1f) legR=(%.1f,%.1f,%.1f)\n",
          rc, copied, b6[4],b6[5],b6[6], b18[4],b18[5],b18[6], b60[4],b60[5],b60[6], b65[4],b65[5],b65[6]);
    }
    return;
  }
  if (g_tex2_mode) {
    static int done;
    if (!done) {
      void* pn = find_player_pawn();
      if (pn) {
        int mo = prop_offset(pn, "Mesh");
        void* comp = mo > 0 ? *(void**)((char*)pn + mo) : NULL;
        int so = comp ? prop_offset(comp, "SkeletalMesh") : -1;
        void* bm = (comp && so > 0) ? *(void**)((char*)comp + so) : NULL;
        void* bs = find_loaded_named("Bs01_Body1");
        if (!bs) {
          ((void*(*)(void*, const wchar_t*, unsigned))0x0da0490UL)(NULL, L"Map11_OIL_Persistent00", 0);
          bs = find_loaded_named("Bs01_Body1");
        }
        if (bm && bs) {
          done = 1;
          int m1=prop_offset(bs,"Materials"), m2=prop_offset(bm,"Materials");
          void** d1 = m1>0 ? *(void***)((char*)bs+m1) : NULL; int n1 = m1>0?*(int*)((char*)bs+m1+8):0;
          void** d2 = m2>0 ? *(void***)((char*)bm+m2) : NULL; int n2 = m2>0?*(int*)((char*)bm+m2+8):0;
          void* src = d1 && n1>0 ? d1[0] : NULL;
          void* dst = d2 && n2>0 ? d2[0] : NULL;
          if (src && dst) {
            int o1=prop_offset(src,"TextureParameterValues");
            void** a1 = o1>0 ? *(void***)((char*)src+o1) : NULL;
            int c1 = o1>0 ? *(int*)((char*)src+o1+8) : 0;
            void* tex = NULL; char tn[96]="?";
            if (a1 && c1>0) { tex = *(void**)((char*)a1+8); uint64_t fn=*(uint64_t*)a1; fn_to_str(fn,tn,sizeof tn); }
            LOG("[mn9pawn] tex2 srcTex=%p '%s' (entry32)\n", tex, tn);
            if (tex) {
              uint64_t fb = fname("Base");
              ((void(*)(void*, uint64_t, void*))0x11cbd00UL)(dst, fb, tex);
              LOG("[mn9pawn] tex2 set 'Base' done\n");
            int so2 = prop_offset(comp, "Scale");
            if (so2 > 0 && addr_mapped((char*)comp + so2 + 12)) {
              float* sv = (float*)((char*)comp + so2);
              LOG("[mn9pawn] tex2 comp Scale old=(%.2f,%.2f,%.2f)\n", sv[0],sv[1],sv[2]);
              sv[0]=1.45f; sv[1]=1.45f; sv[2]=1.45f;
              LOG("[mn9pawn] tex2 comp Scale new=(%.2f,%.2f,%.2f)\n", sv[0],sv[1],sv[2]);
            }
            }
          }
        }
      }
    }
    return;
  }
  if (g_tex_mode) {
    static int done;
    if (!done) {
      void* pn = find_player_pawn();
      if (pn) {
        int mo = prop_offset(pn, "Mesh");
        void* comp = mo > 0 ? *(void**)((char*)pn + mo) : NULL;
        int so = comp ? prop_offset(comp, "SkeletalMesh") : -1;
        void* bm = (comp && so > 0) ? *(void**)((char*)comp + so) : NULL;
        void* bs = find_loaded_named("Bs01_Body1");
        if (!bs) {
          ((void*(*)(void*, const wchar_t*, unsigned))0x0da0490UL)(NULL, L"Map11_OIL_Persistent00", 0);
          bs = find_loaded_named("Bs01_Body1");
        }
        if (bm && bs) {
          done = 1;
          int m1=prop_offset(bs,"Materials"), m2=prop_offset(bm,"Materials");
          void** d1 = m1>0 ? *(void***)((char*)bs+m1) : NULL; int n1 = m1>0?*(int*)((char*)bs+m1+8):0;
          void** d2 = m2>0 ? *(void***)((char*)bm+m2) : NULL; int n2 = m2>0?*(int*)((char*)bm+m2+8):0;
          LOG("[mn9pawn] tex src n=%d dst n=%d\n", n1, n2);
          int nmin = n1<n2?n1:n2;
          for (int k=0;k<nmin;k++) {
            void* src=d1[k]; void* dst=d2[k];
            if (!src || !dst) continue;
            int o1=prop_offset(src,"TextureParameterValues"), o2=prop_offset(dst,"TextureParameterValues");
            if (o1<=0 || o2<=0) { LOG("[mn9pawn] tex slot %d missing param arrays (%d/%d)\n", k, o1, o2); continue; }
            void** a1=*(void***)((char*)src+o1); int c1=*(int*)((char*)src+o1+8);
            void** a2=*(void***)((char*)dst+o2); int c2=*(int*)((char*)dst+o2+8);
            LOG("[mn9pawn] tex slot %d srcParams=%d dstParams=%d\n", k, c1, c2);
            if (!a1 || !a2 || c1<=0 || c2<=0) continue;
            // copy by matching parameter FName; fallback: first
            for (int i=0;i<c1;i++) {
              unsigned long long n1v=*(unsigned long long*)((char*)a1+i*16);
              void* tv=*(void**)((char*)a1+i*16+8);
              char nm[96]="?"; uint64_t fn=n1v; fn_to_str(fn, nm, sizeof nm);
              int found=-1;
              for (int j=0;j<c2;j++) {
                unsigned long long n2v=*(unsigned long long*)((char*)a2+j*16);
                if (n2v==n1v) { found=j; break; }
              }
              if (found<0) found=0;
              *(void**)((char*)a2+found*16+8)=tv;
              LOG("[mn9pawn] tex slot %d '%s' -> dst[%d]\n", k, nm, found);
            }
          }
        }
      }
    }
    return;
  }
  if (g_gpu8_mode) {
    static int stage;
    void* pn = find_player_pawn();
    if (!pn) return;
    int mo = prop_offset(pn, "Mesh");
    void* comp = mo > 0 ? *(void**)((char*)pn + mo) : NULL;
    if (!comp) return;
    if (stage == 0) {
      findobj_fn find = (findobj_fn)FINDOBJ_ADDR;
      void* bcls = find(NULL, NULL, L"MN9Game.MN9Bs01Pawn", 0);
      void** objs = *(void***)OBJ_ARRAY; int cnt = *(int*)OBJ_COUNT;
      if (bcls && objs) {
        for (int i = cnt - 1; i >= 0 && i > cnt - 30000; i--) {
          if (!addr_mapped(objs + i)) break;
          void* o = objs[i];
          if (!o || !addr_mapped(o)) continue;
          if (*(void**)((char*)o + OFF_CLASS) != bcls) continue;
          int mo2 = prop_offset(o, "Mesh");
          void* bc = mo2 > 0 ? *(void**)((char*)o + mo2) : NULL;
          if (bc) { comp = bc; stage = 1; LOG("[mn9pawn] gpu8 boss=%p comp=%p\n", o, bc); break; }
        }
      }
      if (!stage) LOG("[mn9pawn] gpu8 no boss pawn\n");
      return;
    }
    static int dumped; static unsigned nn;
    nn++;
    if (dumped) return;
    if (nn < 900) return;
    if (nn % 120) return;
    void* obj = *(void**)((char*)comp + 0x2e8);
    if (!obj) { LOG("[mn9pawn] gpu8 obj nil t=%u comp=%p\n", nn, comp); return; }
    dumped = 1;
    LOG("[mn9pawn] gpu8 obj=%p vt=%p\n", obj, obj?*(void**)obj:NULL);
    if (!obj || !addr_mapped(obj)) return;
    // scan obj and its child structs for embedded buffers, plausible counts only
    for (int level = 0; level < 2; level++) {
      for (int po = 0; po < (level?0x300:0x100); po += 8) {
        char* base = (char*)obj;
        if (level) {
          void* q = *(void**)((char*)obj + po);
          if (!q || !addr_mapped(q)) continue;
          base = (char*)q;
        }
        for (int off = 0; off < 0x600; off += 8) {
          char* b = base + off;
          if (!addr_mapped(b + 0x60)) continue;
          unsigned char st = *(unsigned char*)(b + 0x58);
          if (st != 8 && st != 32) continue;
          int cnt = *(int*)(b + 0x28);
          void* d = *(void**)(b + 0x30);
          if (cnt < 1000 || cnt > 60000) continue;
          if (!d || !addr_mapped(d)) continue;
          LOG("[mn9pawn] gpu10 L%d po=0x%x off=0x%x buf=%p stride=%d cnt=%d data=%p\n",
              level, po, off, b, st, cnt, d);
          LOG("[mn9pawn] gpu10  bytes:");
          for (int k = 0; k < 24; k++) LOG(" %02x", *(unsigned char*)((char*)d + k));
          LOG("\n");
        }
      }
    }
    return;
  }
  if (g_mats_mode) {
    static int done;
    if (!done) {
      void* pn = find_player_pawn();
      if (pn) {
        int mo = prop_offset(pn, "Mesh");
        void* comp = mo > 0 ? *(void**)((char*)pn + mo) : NULL;
        void* bm = comp ? *(void**)((char*)comp + (prop_offset(comp,"SkeletalMesh")>0?prop_offset(comp,"SkeletalMesh"):0)) : NULL;
        void* bs = find_loaded_named("Bs01_Body1");
        if (!bs) {
          ((void*(*)(void*, const wchar_t*, unsigned))0x0da0490UL)(NULL, L"Map11_OIL_Persistent00", 0);
          bs = find_loaded_named("Bs01_Body1");
        }
        if (bm && bs) {
          done = 1;
          int m1 = prop_offset(bs, "Materials"), m2 = prop_offset(bm, "Materials");
          void** d1 = m1>0 ? *(void***)((char*)bs+m1) : NULL; int n1 = m1>0 ? *(int*)((char*)bs+m1+8) : 0;
          void** d2 = m2>0 ? *(void***)((char*)bm+m2) : NULL; int n2 = m2>0 ? *(int*)((char*)bm+m2+8) : 0;
          LOG("[mn9pawn] mats bs=%p bm=%p bsM off=%d n=%d bmM off=%d n=%d\n", bs, bm, m1, n1, m2, n2);
          for (int k=0; d1 && k<4 && k<8; k++) {
            char nm[128]="?"; path_of(d1[k], nm, sizeof nm);
            LOG("[mn9pawn] mats bs[%d]=%p '%s'\n", k, d1[k], nm);
          }
          if (d1 && d2 && n1>0 && n2>0) {
            for (int k=0; k<n2 && k<n1; k++) d2[k]=d1[k];
            LOG("[mn9pawn] mats copied %d slots\n", n1<n2?n1:n2);
          }
          // component override: point slot 0 at Bs01 body material
          int mc = prop_offset(comp, "Materials");
          if (d1 && n1>0 && mc>0) {
            void** dc = *(void***)((char*)comp+mc); int nc = *(int*)((char*)comp+mc+8);
            int nmax = *(int*)((char*)comp+mc+4);
            LOG("[mn9pawn] mats comp override data=%p n=%d max=%d\n", dc, nc, nmax);
            if (dc && nc>0) {
              for (int k=0;k<nc && k<n1;k++) dc[k]=d1[k];
              LOG("[mn9pawn] mats comp override set %d slots -> '%s'\n", nc<n1?nc:n1, "Bs01");
            }
          }
        }
      }
    }
    return;
  }
  if (g_b3_mode) {
    static int done;
    if (!done) {
      void* pn = find_player_pawn();
      if (pn) {
        int mo = prop_offset(pn, "Mesh");
        void* comp = mo > 0 ? *(void**)((char*)pn + mo) : NULL;
        if (comp) {
          int isb = !strcmp(g_selection + 6, "bs01");
          if (isb) {
            void* mesh = find_loaded_named("Bs01_Body1");
            if (!mesh) {
              ((void*(*)(void*, const wchar_t*, unsigned))0x0da0490UL)(NULL, L"Map11_OIL_Persistent00", 0);
              mesh = find_loaded_named("Bs01_Body1");
            }
            if (mesh) ((setskmesh_fn)SETSKMESH)(comp, mesh, 1);
          }
          ((setanim_fn)SETANIMTREE)(comp, NULL);
          ((void(*)(void*, float, unsigned))0x115b6c0UL)(comp, 0.0f, 1);
          done = 1;
          int* req = *(int**)((char*)comp + 0x368);
          int cnt = *(int*)((char*)comp + 0x370);
          LOG("[mn9pawn] b3 %s comp=%p req=%p cnt=%d\n", isb?"bs01":"beck", comp, req, cnt);
          for (int c = 0; req && c < cnt && c < 90; c++) {
            float m[16];
            ((void(*)(void*, void*, unsigned))0x115dfa0UL)(m, comp, (unsigned)c);
            LOG("[mn9pawn] B3 %d %d %.4f %.4f %.4f\n", c, req[c], m[12], m[13], m[14]);
          }
          LOG("[mn9pawn] b3 done\n");
        }
      }
    }
    return;
  }
  if (g_b2_mode) {
    static int done;
    if (!done) {
      void* pn = find_player_pawn();
      if (pn) {
        int mo = prop_offset(pn, "Mesh");
        void* comp = mo > 0 ? *(void**)((char*)pn + mo) : NULL;
        if (comp) {
          int isb = !strcmp(g_selection + 6, "bs01");
          if (isb) {
            void* mesh = find_loaded_named("Bs01_Body1");
            if (!mesh) {
              ((void*(*)(void*, const wchar_t*, unsigned))0x0da0490UL)(NULL, L"Map11_OIL_Persistent00", 0);
              mesh = find_loaded_named("Bs01_Body1");
            }
            if (mesh) ((setskmesh_fn)SETSKMESH)(comp, mesh, 1);
          }
          ((setanim_fn)SETANIMTREE)(comp, NULL);
          ((void(*)(void*, float, unsigned))0x115b6c0UL)(comp, 0.0f, 1);
          done = 1;
          int n = isb ? 70 : 68;
          LOG("[mn9pawn] b2 %s comp=%p\n", isb?"bs01":"beck", comp);
          for (int i = 0; i < n; i++) {
            float m[16];
            ((void(*)(void*, void*, unsigned))0x115dfa0UL)(m, comp, (unsigned)i);
            LOG("[mn9pawn] B2 %d %.4f %.4f %.4f\n", i, m[12], m[13], m[14]);
          }
          LOG("[mn9pawn] b2 done\n");
        }
      }
    }
    return;
  }
  if (g_tn_mode) {
    static int done;
    if (!done) {
      done = 1;
      void* bs = find_loaded_named("Bs01_Body1");
      if (!bs) {
        ((void*(*)(void*, const wchar_t*, unsigned))0x0da0490UL)(NULL, L"Map11_OIL_Persistent00", 0);
        bs = find_loaded_named("Bs01_Body1");
      }
      void* pn = find_player_pawn();
      void* comp = NULL;
      if (pn) { int mo=prop_offset(pn,"Mesh"); if(mo>0) comp=*(void**)((char*)pn+mo); }
      int sko = comp ? prop_offset(comp,"SkeletalMesh") : -1;
      void* bk = (comp && sko>0) ? *(void**)((char*)comp+sko) : NULL;
      LOG("[mn9pawn] tn bs=%p beck=%p\n", bs, bk);
      if (bs) for (int i=0;i<70;i++){
        int idx=((int(*)(void*, uint64_t))0x17a4ec0UL)(bs, fname(BS01_NAMES[i]));
        LOG("[mn9pawn] TN bs01 %d %s -> %d\n", i, BS01_NAMES[i], idx);
      }
      if (bk) for (int i=0;i<68;i++){
        int idx=((int(*)(void*, uint64_t))0x17a4ec0UL)(bk, fname(BECK_NAMES[i]));
        LOG("[mn9pawn] TN beck %d %s -> %d\n", i, BECK_NAMES[i], idx);
      }
      LOG("[mn9pawn] tn done\n");
    }
    return;
  }
  if (g_match_mode) {
    static int done;
    if (!done) {
      void* mesh = find_loaded_named("Bs01_Body1");
      if (!mesh) {
        ((void*(*)(void*, const wchar_t*, unsigned))0x0da0490UL)(NULL, L"Map11_OIL_Persistent00", 0);
        mesh = find_loaded_named("Bs01_Body1");
      }
      if (mesh) {
        done = 1;
        const char* nm[] = {"Global_SRT","M_Spine1","M_Spine2","M_Head","M_Hip",
          "L_Pinky2","L_Hand","L_Arm1","L_Arm3","M_Neck1","R_Leg1","R_shell","M_Center",0};
        LOG("[mn9pawn] match mesh=%p\n", mesh);
        for (int i = 0; nm[i]; i++) {
          uint64_t fn = fname(nm[i]);
          int idx = ((int(*)(void*, uint64_t))0x17a4ec0UL)(mesh, fn);
          LOG("[mn9pawn] match %s -> %d\n", nm[i], idx);
        }
      }
    }
    return;
  }
  if (g_poseprobe_mode) {
    static unsigned pt;
    if ((pt++ % 40) == 5) {
      void* pn = find_player_pawn();
      if (pn) {
        int mo = prop_offset(pn, "Mesh");
        void* comp = mo > 0 ? *(void**)((char*)pn + mo) : NULL;
        if (comp) {
          void* arr = *(void**)((char*)comp + 0x2f8);
          int cnt = *(int*)((char*)comp + 0x300);
          if (arr && cnt > 66) {
            float* a10=(float*)((char*)arr+10*48); float* a29=(float*)((char*)arr+29*48);
            float* a56=(float*)((char*)arr+56*48); float* a65=(float*)((char*)arr+65*48);
            LOG("[mn9pawn] pose t=%u h10=(%.1f,%.1f,%.1f) h29=(%.1f,%.1f,%.1f) l56=(%.1f,%.1f,%.1f) l65=(%.1f,%.1f,%.1f)\n",
                pt, a10[4],a10[5],a10[6], a29[4],a29[5],a29[6], a56[4],a56[5],a56[6], a65[4],a65[5],a65[6]);
          }
        }
      }
    }
    return;
  }
  if (g_maptest_mode) {
    static int done;
    void* pn = find_player_pawn();
    if (!pn) return;
    int mo = prop_offset(pn, "Mesh");
    void* comp = mo > 0 ? *(void**)((char*)pn + mo) : NULL;
    if (!comp) return;
    if (!done) {
      void* mesh = find_loaded_named("Bs01_Body1");
      if (!mesh) {
        ((void*(*)(void*, const wchar_t*, unsigned))0x0da0490UL)(NULL, L"Map11_OIL_Persistent00", 0);
        mesh = find_loaded_named("Bs01_Body1");
      }
      LOG("[mn9pawn] maptest mesh=%p\n", mesh);
      if (mesh) {
        done = 1;
        int mo2 = prop_offset(comp, "Materials");
        if (mo2 > 0) *(int*)((char*)comp + mo2 + 8) = 0;
        ((setskmesh_fn)SETSKMESH)(comp, mesh, 1);
        void** lods = *(void***)((char*)mesh + 0x130);
        if (lods) {
          char* lod = (char*)lods[0];
          char* chunks = *(char**)(lod + 0x10);
          int cn = *(int*)(lod + 0x18);
          LOG("[mn9pawn] maptest chunks=%p n=%d\n", chunks, cn);
          if (chunks && cn > 0) {
            unsigned short* bm = *(unsigned short**)(chunks + 0x28);
            int bmn = *(int*)(chunks + 0x30);
            LOG("[mn9pawn] maptest bm=%p n=%d old[29]=%d\n", bm, bmn, bmn>29?bm[29]:-1);
            if (bm && bmn > 29) { bm[29] = 55; LOG("[mn9pawn] maptest set bm[29]=55 (M_Hip)\n"); }
          }
        }
      }
    }
    return;
  }
  if (g_bind_mode) {
    static int done;
    if (!done) {
      void* pn = find_player_pawn();
      if (pn) {
        int mo = prop_offset(pn, "Mesh");
        void* comp = mo > 0 ? *(void**)((char*)pn + mo) : NULL;
        if (comp) {
          done = 1;
          int isb = !strcmp(g_selection + 5, "bs01");
          if (isb) {
            void* mesh = find_loaded_named("Bs01_Body1");
            if (!mesh) {
              ((void*(*)(void*, const wchar_t*, unsigned))0x0da0490UL)(NULL, L"Map11_OIL_Persistent00", 0);
              mesh = find_loaded_named("Bs01_Body1");
            }
            LOG("[mn9pawn] bind: bs01 mesh=%p\n", mesh);
            if (mesh) ((setskmesh_fn)SETSKMESH)(comp, mesh, 1);
          }
          ((setanim_fn)SETANIMTREE)(comp, NULL);
          ((void(*)(void*, float, unsigned))0x115b6c0UL)(comp, 0.0f, 1);  // UpdateSkelPose
          void* arr = *(void**)((char*)comp + 0x2f8);
          int cnt = *(int*)((char*)comp + 0x300);
          LOG("[mn9pawn] bind: %s comp=%p arr=%p bones=%d\n",
              isb ? "bs01" : "beck", comp, arr, cnt);
          for (int i = 0; arr && i < cnt && i < 80; i++) {
            float* a = (float*)((char*)arr + i * 48);
            LOG("[mn9pawn] BIND %d %.7f %.7f %.7f %.7f %.7f %.7f %.7f\n",
                i, a[0],a[1],a[2],a[3], a[4],a[5],a[6]);
          }
          LOG("[mn9pawn] bind done\n");
        }
      }
    }
    return;
  }
  if (g_seqs_mode) {
    static int done;
    if (!done) {
      done = 1;
      void* as = find_loaded_named(g_selection + 5);
      LOG("[mn9pawn] seqs animset %s = %p\n", g_selection + 5, as);
      if (as) {
        int so = prop_offset(as, "Sequences");
        void** arr = so > 0 ? *(void***)((char*)as + so) : NULL;
        int cnt = so > 0 ? *(int*)((char*)as + so + 8) : 0;
        LOG("[mn9pawn] seqs count=%d arr=%p\n", cnt, arr);
        for (int i = 0; arr && i < cnt && i < 200; i++) {
          char nm[160]; obj_name_str(arr[i], nm, sizeof nm);
          int lo = prop_offset(arr[i], "SequenceLength");
          float len = lo > 0 ? *(float*)((char*)arr[i] + lo) : -1.f;
          LOG("[mn9pawn] seq %d: %s (%p) len=%.3f\n", i, nm, arr[i], len);
        }
      }
    }
    return;
  }
  if (g_lsn_mode) {
    static int done;
    if (!done) {
      done = 1;
      const char* sub = g_selection + 4;
      void** objs = *(void***)OBJ_ARRAY; int count = *(int*)OBJ_COUNT;
      int low = count - 250000; if (low < 0) low = 0; int shown = 0;
      for (int i = count - 1; i >= low && shown < 120; i--) {
        if (!addr_mapped(objs + i)) break;
        void* o = objs[i]; char nm[160], cn[96];
        if (!obj_name_str(o, nm, sizeof nm)) continue;
        if (!strstr(nm, sub)) continue;
        shown++;
        class_name_of(o, cn, sizeof cn);
        LOG("[mn9pawn] lsn %s : %s\n", cn, nm);
      }
      LOG("[mn9pawn] lsn done shown=%d\n", shown);
    }
    return;
  }
  if (g_swapm_mode) {
    static int done;
    void* pn = find_player_pawn();
    if (!pn) return;
    int mo = prop_offset(pn, "Mesh");
    void* comp = mo > 0 ? *(void**)((char*)pn + mo) : NULL;
    if (!comp) return;
    if (!done) {
      done = 1;
      void* mesh = find_loaded_by_name(g_selection + 6);
      LOG("[mn9pawn] swapm: comp=%p target='%s' mesh=%p\n", comp, g_selection + 6, mesh);
      if (mesh) {
        ((setskmesh_fn)SETSKMESH)(comp, mesh, 1);
        int sk = prop_offset(comp, "SkeletalMesh");
        LOG("[mn9pawn] swapm: newsk=%p\n", sk>0?*(void**)((char*)comp+sk):NULL);
      }
    }
    return;
  }
  if (g_swapnull_mode) {
    static int done;
    void* pn = find_player_pawn();
    if (!pn) return;
    int mo = prop_offset(pn, "Mesh");
    void* comp = mo > 0 ? *(void**)((char*)pn + mo) : NULL;
    if (!comp) return;
    if (!done) {
      done = 1;
      int sk = prop_offset(comp, "SkeletalMesh");
      LOG("[mn9pawn] swapnull: comp=%p oldsk=%p\n", comp, sk>0?*(void**)((char*)comp+sk):NULL);
      ((setskmesh_fn)SETSKMESH)(comp, NULL, 1);
      LOG("[mn9pawn] swapnull: set NULL newsk=%p\n", sk>0?*(void**)((char*)comp+sk):NULL);
    }
    return;
  }
  if (g_swapcomp_mode) {
    static int done;
    void* pn = find_player_pawn();
    if (!pn) return;
    int mo = prop_offset(pn, "Mesh");
    void* comp = mo > 0 ? *(void**)((char*)pn + mo) : NULL;
    if (!comp) return;
    if (!done) {
      void* mesh = find_loaded_named("Bs01_Body1");
      void* tree = find_loaded_named("Bs01_AnimTree");
      void* skel = find_loaded_named("Bs01_Skeleton");
      if (!mesh) {
        LOG("[mn9pawn] swapcomp: Bs01 not loaded, loading Map11_OIL_Persistent00\n");
        ((void*(*)(void*, const wchar_t*, unsigned))0x0da0490UL)(NULL, L"Map11_OIL_Persistent00", 0);
        mesh = find_loaded_named("Bs01_Body1");
        tree = find_loaded_named("Bs01_AnimTree");
        LOG("[mn9pawn] swapcomp: after LoadPackage mesh=%p tree=%p\n", mesh, tree);
      }
      char mp[160] = "?"; path_of(mesh, mp, sizeof mp);
      char pcn[64] = "?";
      class_name_of(pn, pcn, sizeof pcn);
      LOG("[mn9pawn] swapcomp: pawn=%p cls=%s comp=%p mesh=%p '%s' tree=%p skel=%p bones=%d\n",
          pn, pcn, comp, mesh, mp, tree, skel, mesh_bone_count(mesh));
      if (!mesh) { LOG("[mn9pawn] swapcomp: mesh not loaded\n"); return; }
      done = 1;
      // Force every form slot the pawn uses (normal/dash/swoop) to the Bs01 body
      const char* forms[3] = { "BodyMeshNormal", "BodyMeshDash", "BodyMeshSwoop" };
      for (int fi = 0; fi < 3; fi++) {
        int fo = prop_offset(pn, forms[fi]);
        if (fo > 0) { *(void**)((char*)pn + fo) = mesh; }
      }
      findobj_fn ffind=(findobj_fn)FINDOBJ_ADDR;
      const wchar_t* archs[] = { L"MN9Game.Default__MN9Spw09Pawn", L"MN9Game.Default__MN9Pl01Pawn", NULL };
      for (int ai2=0; archs[ai2]; ai2++) {
        void* ar=ffind(NULL,NULL,archs[ai2],0); if(!ar) continue;
        for (int fi=0; fi<3; fi++) { int fo=prop_offset(ar,forms[fi]); if(fo>0) *(void**)((char*)ar+fo)=mesh; }
      }
      g_swap_keep_mesh = mesh; g_swap_keep_tree = tree;
      LOG("[mn9pawn] swapcomp: form slots -> Bs01 (%p)\n", mesh);
      int mo2 = prop_offset(comp, "Materials");
      if (mo2 > 0) {
        int oldn = *(int*)((char*)comp + mo2 + 8);
        *(int*)((char*)comp + mo2 + 8) = 0;
        LOG("[mn9pawn] swapcomp: cleared Materials count %d\n", oldn);
      }
      int at = prop_offset(comp, "AnimTreeTemplate");
      if (at > 0 && tree) ((setanim_fn)SETANIMTREE)(comp, tree);
      if (tree) ((void(*)(void*, unsigned))0x115ac00UL)(comp, 1);  // InitAnimTree
      ((setskmesh_fn)SETSKMESH)(comp, mesh, 1);
      if (tree) ((void(*)(void*, unsigned))0x115ac00UL)(comp, 1);  // re-init after mesh
      char tp[160] = "?"; path_of(at > 0 ? *(void**)((char*)comp + at) : NULL, tp, sizeof tp);
      int aio = 0x260;
      void* ai = *(void**)((char*)comp + 0x260);
      char an[96] = "?"; class_name_of(ai, an, sizeof an);
      LOG("[mn9pawn] swapcomp: set+init AnimTree '%s' instance=%p cls=%s off=%d\n", tp, ai, an, aio);
      if (ai) {
        struct { void** data; int count; int max; } qa = {0,0,0};
        ((void(*)(void*, void*, unsigned))0x17cc440UL)(ai, &qa, 1);
        LOG("[mn9pawn] nodes tree=%p count=%d data=%p\n", ai, qa.count, qa.data);
        int co=prop_offset(ai,"Children");
        if (co>0) {
          void** ch=*(void***)((char*)ai+co); int cn=*(int*)((char*)ai+co+8);
          LOG("[mn9pawn] root children n=%d arr=%p\n", cn, ch);
          for (int ci=0; ch && ci<cn && ci<8; ci++) {
            void* c=ch[ci]; if(!c||!addr_mapped(c)) continue;
            char cc[96]="?"; char nn2[128]="?"; class_name_of(c,cc,sizeof cc); obj_name_str(c,nn2,sizeof nn2);
            LOG("[mn9pawn] root child[%d]=%p cls=%s name=%s\n", ci, c, cc, nn2);
            int c2=prop_offset(c,"Children");
            if (c2>0) { void** g=*(void***)((char*)c+c2); int gn=*(int*)((char*)c+c2+8);
              for (int gi=0; g && gi<gn && gi<6; gi++){ void* gg=g[gi]; if(!gg||!addr_mapped(gg))continue;
                char gc[96]="?"; char gnn[128]="?"; class_name_of(gg,gc,sizeof gc); obj_name_str(gg,gnn,sizeof gnn);
                LOG("[mn9pawn]   child[%d] c%d=%p cls=%s name=%s\n", ci,gi,gg,gc,gnn); } }
          }
        }
        for (int i=0; qa.data && i<qa.count && i<50; i++) {
          void* nd=qa.data[i]; if (!nd || !addr_mapped(nd)) continue;
          char cn[96]="?"; char nn[128]="?";
          class_name_of(nd, cn, sizeof cn); obj_name_str(nd, nn, sizeof nn);
          char an2[128]=""; if (strstr(cn,"AnimNodeSequence")) {
            int ao=prop_offset(nd,"AnimSeq"); void* asq=ao>0?*(void**)((char*)nd+ao):NULL;
            obj_name_str(asq, an2, sizeof an2);
            int bp=prop_offset(nd,"bPlaying"); int bpv=bp>0?*(unsigned char*)((char*)nd+bp):-1;
            LOG("[mn9pawn] node[%d]=%p cls=%s name=%s seq=%s playing=%d\n", i, nd, cn, nn, an2, bpv);
          } else {
            LOG("[mn9pawn] node[%d]=%p cls=%s name=%s\n", i, nd, cn, nn);
          }
        }
      }
      static const char* cp[2] = { "ArmLMeshComponent", "ArmRMeshComponent" };
      for (int i = 0; i < 2; i++) {
        int o = prop_offset(pn, cp[i]);
        if (o <= 0) continue;
        void* ac = *(void**)((char*)pn + o);
        if (!ac) continue;
        ((void(*)(void*, unsigned))0x14801b0UL)(ac, 1);   // SetOwnerNoSee (render-aware)
        int hid = prop_offset(ac, "bHidden");
        if (hid > 0) *(unsigned char*)((char*)ac + hid) = 1;
        LOG("[mn9pawn] swapcomp: hid %s=%p\n", cp[i], ac);
      }
    }
    // persistent guard: form changes (dash/swoop) swap the mesh away; restore it
    {
      void* keepMesh = g_swap_keep_mesh;
      void* keepTree = g_swap_keep_tree;
      if (!keepMesh) keepMesh = find_loaded_named("Bs01_Body1");
      if (!keepTree) keepTree = find_loaded_named("Bs01_AnimTree");
      int hd0=prop_offset(comp,"bHidden"); if(hd0>0) *(unsigned char*)((char*)comp+hd0)=0;
      int on0=prop_offset(comp,"bOwnerNoSee"); if(on0>0) *(unsigned char*)((char*)comp+on0)=0;
      int pa0=prop_offset(comp,"bPauseAnims"); if(pa0>0) *(unsigned char*)((char*)comp+pa0)=0;
      int sk = prop_offset(comp, "SkeletalMesh");
      void* cur = sk>0 ? *(void**)((char*)comp+sk) : NULL;
      if (keepMesh && cur != keepMesh) {
        int mc=prop_offset(comp,"Materials");
        if (mc>0 && *(void***)((char*)comp+mc)) *(int*)((char*)comp+mc+8)=0;
        if (keepTree) ((setanim_fn)SETANIMTREE)(comp, keepTree);
        ((setskmesh_fn)SETSKMESH)(comp, keepMesh, 1);
        if (keepTree) ((void(*)(void*, unsigned))0x115ac00UL)(comp,1);
        LOG("[mn9pawn] swapcomp: restored Bs01 body after form change (cur=%p keep=%p)\n", cur, keepMesh);
      }
    }
    return;
  }
  if (g_follow_mode) {
    static int done;
    void* pn = find_player_pawn();
    if (!pn) return;
    int lo = prop_offset(pn, "Location"), ro = prop_offset(pn, "Rotation");
    if (lo <= 0 || ro <= 0) return;
    if (!done) {
      done = 1;
      findobj_fn find = (findobj_fn)FINDOBJ_ADDR;
      void* bcls = find(NULL, NULL, L"MN9Game.MN9Bs01Pawn", 0);
      void* mesh = find_loaded_by_name("Bs01_Body1");
      void* tree = find_loaded_by_name("Bs01_AnimTree");
      if (!mesh) {
        ((void*(*)(void*, const wchar_t*, unsigned))0x0da0490UL)(NULL, L"Map11_OIL_Persistent00", 0);
        mesh = find_loaded_by_name("Bs01_Body1");
        if (!tree) tree = find_loaded_by_name("Bs01_AnimTree");
        LOG("[mn9pawn] follow: loaded boss package mesh=%p tree=%p\n", mesh, tree);
      }
      LOG("[mn9pawn] follow: class=%p mesh=%p tree=%p\n", bcls, mesh, tree);
      if (!bcls || !mesh) return;
      spawn_fn sp = (spawn_fn)SPAWNACTOR_ADDR;
      void* bp = sp(world, bcls, 0, (char*)pn + lo, (char*)pn + ro, NULL, 1, 0, NULL, NULL, 0, NULL);
      LOG("[mn9pawn] follow: spawned=%p\n", bp);
      if (!bp) return;
      g_follow_pawn = bp;
      int ow = prop_offset(bp, "Owner");
      if (ow > 0) *(void**)((char*)bp + ow) = pn;
      int ba = prop_offset(bp, "Base");
      if (ba > 0) *(void**)((char*)bp + ba) = pn;
      int ha = prop_offset(bp, "bHardAttach");
      if (ha > 0) *(unsigned char*)((char*)bp + ha) = 1;
      LOG("[mn9pawn] follow: attached owner=%p base=%p hardattach=%d\n",
          pn, pn, ha > 0);
      void** objs = *(void***)OBJ_ARRAY;
      int cnt = *(int*)OBJ_COUNT;
      int low = cnt - 30000; if (low < 0) low = 0;
      int hits = 0;
      for (int i = cnt - 1; i >= low; i--) {
        if (!addr_mapped(objs + i)) break;
        void* o = objs[i];
        if (!o || !addr_mapped(o)) continue;
        int ow = prop_offset(o, "Owner");
        if (ow <= 0 || *(void**)((char*)o + ow) != bp) continue;
        int sk = prop_offset(o, "SkeletalMesh");
        if (sk <= 0) continue;
        hits++;
        ((setskmesh_fn)SETSKMESH)(o, mesh, 1);
        int mc = prop_offset(o, "Materials");
        int m1 = prop_offset(mesh, "Materials");
        void** d1 = m1>0 ? *(void***)((char*)mesh+m1) : NULL; int n1 = m1>0?*(int*)((char*)mesh+m1+8):0;
        void** d2 = mc>0 ? *(void***)((char*)o+mc) : NULL; int n2 = mc>0?*(int*)((char*)o+mc+8):0;
        LOG("[mn9pawn] follow: mats mesh n=%d comp n=%d d1=%p d2=%p\n", n1, n2, d1, d2);
        if (d1 && d2 && n1>0) {
          int nc = n2>0?n2:n1;
          for (int k=0;k<nc && k<n1;k++) d2[k]=d1[k];
          if (n2<1 && mc>0) *(int*)((char*)o+mc+8)=1;
          LOG("[mn9pawn] follow: forced %d Bs01 material slots\n", nc<n1?nc:n1);
        } else if (mc > 0) {
          *(int*)((char*)o + mc + 8) = 0;
        }
        int at = prop_offset(o, "AnimTreeTemplate");
        if (at > 0 && tree) ((setanim_fn)SETANIMTREE)(o, tree);
        if (tree) ((void(*)(void*, unsigned))0x115ac00UL)(o, 1);   // InitAnimTree
        LOG("[mn9pawn] follow: tree=%p at=%d init=1\n", tree, at);
        int hid = prop_offset(o, "bHidden");
        if (hid > 0) *(unsigned char*)((char*)o + hid) = 0;
        int ons = prop_offset(o, "bOwnerNoSee");
        if (ons > 0) *(unsigned char*)((char*)o + ons) = 0;
      }
      LOG("[mn9pawn] follow: boss comps set=%d\n", hits);
      static const char* cp[3] = { "Mesh", "ArmLMeshComponent", "ArmRMeshComponent" };
      for (int i = 0; i < 3; i++) {
        int o = prop_offset(pn, cp[i]);
        if (o <= 0) continue;
        void* comp = *(void**)((char*)pn + o);
        if (!comp) continue;
        ((void(*)(void*, unsigned))0x14801b0UL)(comp, 1);   // SetOwnerNoSee
        int hid = prop_offset(comp, "bHidden");
        if (hid > 0) *(unsigned char*)((char*)comp + hid) = 1;
      }
      LOG("[mn9pawn] follow: hid beck components\n");
      // Tutorial-style stages render Beck via separate cinematic actors
      void** objs2 = *(void***)OBJ_ARRAY; int cnt2 = *(int*)OBJ_COUNT;
      int hidn = 0;
      for (int i = cnt2 - 1; i >= 0 && i > cnt2 - 40000; i--) {
        if (!addr_mapped(objs2 + i)) break;
        void* o = objs2[i]; if (!o || !addr_mapped(o)) continue;
        char cn[96] = "?"; class_name_of(o, cn, sizeof cn);
        if (strstr(cn, "SkeletalMeshCinematicActor") || strstr(cn, "Spw09SkeletalMeshActor")) {
          int ons3 = prop_offset(o, "bOwnerNoSee");
          if (ons3 > 0) *(unsigned char*)((char*)o + ons3) = 1;
          hidn++;
        }
      }
      LOG("[mn9pawn] follow: hid cinematic beck actors=%d\n", hidn);
      int hidm = 0;
      for (int i = cnt2 - 1; i >= 0 && i > cnt2 - 40000; i--) {
        if (!addr_mapped(objs2 + i)) break;
        void* o = objs2[i]; if (!o || !addr_mapped(o)) continue;
        int sk = prop_offset(o, "SkeletalMesh"); if (sk <= 0) continue;
        void* mm = *(void**)((char*)o + sk);
        char mp[160] = "?"; path_of(mm, mp, sizeof mp);
        if (strstr(mp, "Spw09_")) {
          ((void(*)(void*, unsigned))0x14801b0UL)(o, 1);   // comp SetOwnerNoSee
          hidm++;
        }
      }
      LOG("[mn9pawn] follow: hid Spw09 mesh comps=%d\n", hidm);
      LOG("[mn9pawn] follow: component-level hiding only\n");
    }
    if (g_follow_pawn) {
      int bl = prop_offset(g_follow_pawn, "Location"), br = prop_offset(g_follow_pawn, "Rotation");
      if (bl > 0) memcpy((char*)g_follow_pawn + bl, (char*)pn + lo, 12);
      if (br > 0) memcpy((char*)g_follow_pawn + br, (char*)pn + ro, 12);
    }
    return;
  }
  if (g_gpu3_mode) {
    static int done6;
    if (!done6) {
      void* pn = find_player_pawn();
      if (pn) {
        int mo = prop_offset(pn, "Mesh");
        void* comp = mo > 0 ? *(void**)((char*)pn + mo) : NULL;
        if (comp) {
          void* obj = *(void**)((char*)comp + 0x2e8);
          if (obj && addr_mapped(obj)) {
            done6 = 1;
            LOG("[mn9pawn] gpu6 obj=%p\n", obj);
            for (int f = 0; f < 0x100; f += 8) {
              void* lod = *(void**)((char*)obj + f);
              if (!lod || !addr_mapped(lod)) continue;
              LOG("[mn9pawn] gpu6 f+0x%02x lod=%p\n", f, lod);
              for (int bo = 0x70; bo <= 0xf0; bo += 0x80) {
                char* buf = (char*)lod + bo;
                if (!addr_mapped(buf)) continue;
                unsigned char st = *(unsigned char*)(buf + 0x58);
                void* vt = *(void**)buf;
                LOG("[mn9pawn]  buf+0x%x vt=%p stride=%d i20=%d i28=%d i30=%d i38=%d\n",
                    bo, vt, st, *(int*)(buf+0x20), *(int*)(buf+0x28),
                    *(int*)(buf+0x30), *(int*)(buf+0x38));
                void* d = *(void**)(buf + 0x28);
                if (addr_mapped(d)) {
                  LOG("[mn9pawn]   d28 bytes:");
                  for (int k=0;k<12;k++) LOG(" %02x", *(unsigned char*)((char*)d+k));
                  LOG("\n");
                }
                void* d2 = *(void**)(buf + 0x30);
                if (addr_mapped(d2)) {
                  LOG("[mn9pawn]   d30 bytes:");
                  for (int k=0;k<12;k++) LOG(" %02x", *(unsigned char*)((char*)d2+k));
                  LOG("\n");
                }
              }
            }
          }
        }
      }
    }
    return;
  }
  if (g_gpu3_mode) {
    static int done5;
    if (!done5) {
      void* pn = find_player_pawn();
      if (pn) {
        int mo = prop_offset(pn, "Mesh");
        void* comp = mo > 0 ? *(void**)((char*)pn + mo) : NULL;
        if (comp) {
          void* obj = *(void**)((char*)comp + 0x2e8);
          if (obj && addr_mapped(obj)) {
            done5 = 1;
            void* lod = *(void**)((char*)obj + 0x68);
            int cnt = *(int*)((char*)obj + 0x70);
            LOG("[mn9pawn] gpu5 obj=%p lod=%p cnt=%d\n", obj, lod, cnt);
            if (lod && addr_mapped(lod)) {
              for (int off = 0; off < 0x1000; off += 8) {
                void* buf = *(void**)((char*)lod + off);
                if (!buf || !addr_mapped(buf)) continue;
                unsigned char st = *(unsigned char*)((char*)buf + 0x58);
                if (st == 8 || st == 32) {
                  LOG("[mn9pawn] gpu5 lod+0x%x buf=%p stride=%d\n", off, buf, st);
                  int* q = (int*)buf;
                  for (int k = 0; k < 20; k++) LOG(" %d", q[k]);
                  LOG("\n");
                  void* d28 = *(void**)((char*)buf + 0x28);
                  void* d30 = *(void**)((char*)buf + 0x30);
                  void* d08 = *(void**)((char*)buf + 0x08);
                  LOG("[mn9pawn] gpu5  d08=%p d28=%p d30=%p\n", d08, d28, d30);
                  if (st == 8 && d08 && addr_mapped(d08)) {
                    LOG("[mn9pawn] gpu5  data08:");
                    for (int k = 0; k < 16; k++) LOG(" %02x", *(unsigned char*)((char*)d08 + k));
                    LOG("\n");
                  }
                }
              }
            }
          }
        }
      }
    }
    return;
  }
  if (g_gpu3_mode) {
    static int done6;
    if (!done6) {
      void* pn = find_player_pawn();
      if (pn) {
        int mo = prop_offset(pn, "Mesh");
        void* comp = mo > 0 ? *(void**)((char*)pn + mo) : NULL;
        if (comp) {
          void* obj = *(void**)((char*)comp + 0x2e8);
          if (obj && addr_mapped(obj)) {
            done6 = 1;
            LOG("[mn9pawn] gpu6 obj=%p\n", obj);
            for (int f = 0; f < 0x100; f += 8) {
              void* lod = *(void**)((char*)obj + f);
              if (!lod || !addr_mapped(lod)) continue;
              LOG("[mn9pawn] gpu6 f+0x%02x lod=%p\n", f, lod);
              for (int bo = 0x70; bo <= 0xf0; bo += 0x80) {
                char* buf = (char*)lod + bo;
                if (!addr_mapped(buf)) continue;
                unsigned char st = *(unsigned char*)(buf + 0x58);
                void* vt = *(void**)buf;
                LOG("[mn9pawn]  buf+0x%x vt=%p stride=%d i20=%d i28=%d i30=%d i38=%d\n",
                    bo, vt, st, *(int*)(buf+0x20), *(int*)(buf+0x28),
                    *(int*)(buf+0x30), *(int*)(buf+0x38));
                void* d = *(void**)(buf + 0x28);
                if (addr_mapped(d)) {
                  LOG("[mn9pawn]   d28 bytes:");
                  for (int k=0;k<12;k++) LOG(" %02x", *(unsigned char*)((char*)d+k));
                  LOG("\n");
                }
                void* d2 = *(void**)(buf + 0x30);
                if (addr_mapped(d2)) {
                  LOG("[mn9pawn]   d30 bytes:");
                  for (int k=0;k<12;k++) LOG(" %02x", *(unsigned char*)((char*)d2+k));
                  LOG("\n");
                }
              }
            }
          }
        }
      }
    }
    return;
  }
  if (g_gpu3_mode) {
    static int done;
    if (!done) {
      void* pn = find_player_pawn();
      if (pn) {
        int mo = prop_offset(pn, "Mesh");
        void* comp = mo > 0 ? *(void**)((char*)pn + mo) : NULL;
        if (comp) {
          void* obj = *(void**)((char*)comp + 0x2e8);
          if (obj && addr_mapped(obj)) {
            done = 1;
            LOG("[mn9pawn] gpu4 obj=%p vtable=%p\n", obj, *(void**)obj);
            for (int f = 0; f < 0x100; f += 8) {
              void* cont = *(void**)((char*)obj + f);
              if (!cont || !addr_mapped(cont)) continue;
              for (int g = 0; g < 0x200; g += 8) {
                void* buf = *(void**)((char*)cont + g);
                if (!buf || !addr_mapped(buf)) continue;
                unsigned char stride = *(unsigned char*)((char*)buf + 0x58);
                if (stride == 8 || stride == 32) {
                  void* data = *(void**)((char*)buf + 0x28);
                  int count = *(int*)((char*)buf + 0x20);
                  LOG("[mn9pawn] gpu4 f+0x%02x g+0x%02x buf=%p stride=%d data=%p cnt=%d\n",
                      f, g, buf, stride, data, count);
                  if (stride == 8 && data && addr_mapped(data)) {
                    LOG("[mn9pawn] gpu4  bone bytes:");
                    for (int k = 0; k < 16; k++) LOG(" %02x", *(unsigned char*)((char*)data + k));
                    LOG("\n");
                  }
                }
              }
            }
          }
        }
      }
    }
    return;
  }
  if (g_gpu3_mode) {
    static int done6;
    if (!done6) {
      void* pn = find_player_pawn();
      if (pn) {
        int mo = prop_offset(pn, "Mesh");
        void* comp = mo > 0 ? *(void**)((char*)pn + mo) : NULL;
        if (comp) {
          void* obj = *(void**)((char*)comp + 0x2e8);
          if (obj && addr_mapped(obj)) {
            done6 = 1;
            LOG("[mn9pawn] gpu6 obj=%p\n", obj);
            for (int f = 0; f < 0x100; f += 8) {
              void* lod = *(void**)((char*)obj + f);
              if (!lod || !addr_mapped(lod)) continue;
              LOG("[mn9pawn] gpu6 f+0x%02x lod=%p\n", f, lod);
              for (int bo = 0x70; bo <= 0xf0; bo += 0x80) {
                char* buf = (char*)lod + bo;
                if (!addr_mapped(buf)) continue;
                unsigned char st = *(unsigned char*)(buf + 0x58);
                void* vt = *(void**)buf;
                LOG("[mn9pawn]  buf+0x%x vt=%p stride=%d i20=%d i28=%d i30=%d i38=%d\n",
                    bo, vt, st, *(int*)(buf+0x20), *(int*)(buf+0x28),
                    *(int*)(buf+0x30), *(int*)(buf+0x38));
                void* d = *(void**)(buf + 0x28);
                if (addr_mapped(d)) {
                  LOG("[mn9pawn]   d28 bytes:");
                  for (int k=0;k<12;k++) LOG(" %02x", *(unsigned char*)((char*)d+k));
                  LOG("\n");
                }
                void* d2 = *(void**)(buf + 0x30);
                if (addr_mapped(d2)) {
                  LOG("[mn9pawn]   d30 bytes:");
                  for (int k=0;k<12;k++) LOG(" %02x", *(unsigned char*)((char*)d2+k));
                  LOG("\n");
                }
              }
            }
          }
        }
      }
    }
    return;
  }
  if (g_gpu3_mode) {
    static int done5;
    if (!done5) {
      void* pn = find_player_pawn();
      if (pn) {
        int mo = prop_offset(pn, "Mesh");
        void* comp = mo > 0 ? *(void**)((char*)pn + mo) : NULL;
        if (comp) {
          void* obj = *(void**)((char*)comp + 0x2e8);
          if (obj && addr_mapped(obj)) {
            done5 = 1;
            void* lod = *(void**)((char*)obj + 0x68);
            int cnt = *(int*)((char*)obj + 0x70);
            LOG("[mn9pawn] gpu5 obj=%p lod=%p cnt=%d\n", obj, lod, cnt);
            if (lod && addr_mapped(lod)) {
              for (int off = 0; off < 0x1000; off += 8) {
                void* buf = *(void**)((char*)lod + off);
                if (!buf || !addr_mapped(buf)) continue;
                unsigned char st = *(unsigned char*)((char*)buf + 0x58);
                if (st == 8 || st == 32) {
                  LOG("[mn9pawn] gpu5 lod+0x%x buf=%p stride=%d\n", off, buf, st);
                  int* q = (int*)buf;
                  for (int k = 0; k < 20; k++) LOG(" %d", q[k]);
                  LOG("\n");
                  void* d28 = *(void**)((char*)buf + 0x28);
                  void* d30 = *(void**)((char*)buf + 0x30);
                  void* d08 = *(void**)((char*)buf + 0x08);
                  LOG("[mn9pawn] gpu5  d08=%p d28=%p d30=%p\n", d08, d28, d30);
                  if (st == 8 && d08 && addr_mapped(d08)) {
                    LOG("[mn9pawn] gpu5  data08:");
                    for (int k = 0; k < 16; k++) LOG(" %02x", *(unsigned char*)((char*)d08 + k));
                    LOG("\n");
                  }
                }
              }
            }
          }
        }
      }
    }
    return;
  }
  if (g_gpu3_mode) {
    static int done6;
    if (!done6) {
      void* pn = find_player_pawn();
      if (pn) {
        int mo = prop_offset(pn, "Mesh");
        void* comp = mo > 0 ? *(void**)((char*)pn + mo) : NULL;
        if (comp) {
          void* obj = *(void**)((char*)comp + 0x2e8);
          if (obj && addr_mapped(obj)) {
            done6 = 1;
            LOG("[mn9pawn] gpu6 obj=%p\n", obj);
            for (int f = 0; f < 0x100; f += 8) {
              void* lod = *(void**)((char*)obj + f);
              if (!lod || !addr_mapped(lod)) continue;
              LOG("[mn9pawn] gpu6 f+0x%02x lod=%p\n", f, lod);
              for (int bo = 0x70; bo <= 0xf0; bo += 0x80) {
                char* buf = (char*)lod + bo;
                if (!addr_mapped(buf)) continue;
                unsigned char st = *(unsigned char*)(buf + 0x58);
                void* vt = *(void**)buf;
                LOG("[mn9pawn]  buf+0x%x vt=%p stride=%d i20=%d i28=%d i30=%d i38=%d\n",
                    bo, vt, st, *(int*)(buf+0x20), *(int*)(buf+0x28),
                    *(int*)(buf+0x30), *(int*)(buf+0x38));
                void* d = *(void**)(buf + 0x28);
                if (addr_mapped(d)) {
                  LOG("[mn9pawn]   d28 bytes:");
                  for (int k=0;k<12;k++) LOG(" %02x", *(unsigned char*)((char*)d+k));
                  LOG("\n");
                }
                void* d2 = *(void**)(buf + 0x30);
                if (addr_mapped(d2)) {
                  LOG("[mn9pawn]   d30 bytes:");
                  for (int k=0;k<12;k++) LOG(" %02x", *(unsigned char*)((char*)d2+k));
                  LOG("\n");
                }
              }
            }
          }
        }
      }
    }
    return;
  }
  if (g_gpu3_mode) {
    static int done;
    if (!done) {
      void* pn = find_player_pawn();
      if (pn) {
        int mo = prop_offset(pn, "Mesh");
        void* comp = mo > 0 ? *(void**)((char*)pn + mo) : NULL;
        if (comp) {
          done = 1;
          LOG("[mn9pawn] gpu3 comp=%p\n", comp);
          for (int off = 0; off < 0x1200; off += 8) {
            void* q = *(void**)((char*)comp + off);
            if (!addr_mapped(q)) continue;
            void* vt = *(void**)q;
            if ((uintptr_t)vt >= 0x23d2f00UL && (uintptr_t)vt < 0x23d4000UL) {
              LOG("[mn9pawn] gpu3 FOUND GPUSkin obj at comp+0x%x = %p\n", off, q);
              for (int k = 0; k < 24; k++) {
                void* w = *(void**)((char*)q + k * 8);
                if (addr_mapped(w)) LOG("[mn9pawn]   +0x%02x=%p\n", k * 8, w);
              }
            }
          }
        }
      }
    }
    return;
  }
  if (g_gpu2_mode) {
    static int done;
    if (!done) {
      void* pn = find_player_pawn();
      if (pn) {
        int mo = prop_offset(pn, "Mesh");
        void* comp = mo > 0 ? *(void**)((char*)pn + mo) : NULL;
        if (comp) {
          done = 1;
          void* obj = *(void**)((char*)comp + 0x250);
          LOG("[mn9pawn] gpu2 comp=%p meshobj=%p\n", comp, obj);
          if (obj && addr_mapped(obj)) {
            LOG("[mn9pawn] gpu2 vtable=%p (gpuskin vtable=0x23d2fd0)\n", *(void**)obj);
            for (int k = 0; k < 32; k++) {
              void* q = *(void**)((char*)obj + k * 8);
              if (addr_mapped(q)) LOG("[mn9pawn] gpu2 +0x%02x=%p\n", k * 8, q);
            }
          }
        }
      }
    }
    return;
  }
  if (g_gpu_mode) {
    static int done;
    if (!done) {
      void** objs = *(void***)OBJ_ARRAY;
      int count = *(int*)OBJ_COUNT;
      LOG("[mn9pawn] gpu: scanning %d objects\n", count);
      if (objs && count > 0 && count < 400000) {
        int found = 0, skel = 0;
        for (int i = count - 1; i >= 0 && found < 6; i--) {
          if (!addr_mapped(objs + i)) break;
          void* o = objs[i];
          char nm[160];
          if (!class_name_of(o, nm, sizeof nm)) continue;
          if (strstr(nm, "Skeletal") || strstr(nm, "MeshObject")) {
            if (skel < 20) LOG("[mn9pawn] gpu: class %s obj=%p\n", nm, o);
            skel++;
            if (strstr(nm, "SkeletalMeshObject")) {
              found++;
              LOG("[mn9pawn] gpu: MESHOBJ %s ptr=%p\n", nm, o);
              for (int k = 0; k < 20; k++) {
                void* q = *(void**)((char*)o + k * 8);
                if (addr_mapped(q)) LOG("[mn9pawn]   +0x%02x=%p\n", k * 8, q);
              }
            }
          }
        }
        LOG("[mn9pawn] gpu: skelclasses=%d found=%d\n", skel, found);
        done = 1;
      }
    }
    return;
  }
  if (g_bchunk_mode) {
    static int done;
    if (!done) {
      findobj_fn find = (findobj_fn)FINDOBJ_ADDR;
      void* mesh = find(NULL, NULL, L"Bs01_model.Mesh.Bs01_Body1", 0);
      if (!mesh) mesh = find_loaded_by_name("Bs01_Body1");
      if (mesh) {
        done = 1;
        void** lods = *(void***)((char*)mesh + 0x130);
        int lodn = *(int*)((char*)mesh + 0x138);
        LOG("[mn9pawn] bchunk mesh=%p lods=%p n=%d\n", mesh, lods, lodn);
        if (lods && lodn > 0) {
          char* lod = (char*)lods[0];
          char* chunks = *(char**)(lod + 0x10);
          int cn = *(int*)(lod + 0x18);
          LOG("[mn9pawn] bchunk n=%d\n", cn);
          for (int ci = 0; ci < cn && ci < 8; ci++) {
            char* c = chunks + ci * 0x48;
            void* bm = *(void**)(c + 0x28);
            int bmn = *(int*)(c + 0x30);
            int fv = *(int*)(c + 0x38), nv = *(int*)(c + 0x3c);
            LOG("[mn9pawn] BCH%d fv=%d nv=%d n=%d\n", ci, fv, nv, bmn);
            if (bm && addr_mapped(bm) && bmn > 0 && bmn < 200) {
              LOG("[mn9pawn] BBM%d:", ci);
              for (int k = 0; k < bmn; k++) LOG(" %d", *(unsigned short*)((char*)bm + k * 2));
              LOG("\n");
            }
          }
        }
      }
    }
    return;
  }
  if (g_inf_mode) {
    static int done;
    if (!done) {
      void* pn = find_player_pawn();
      if (pn) {
        int mo = prop_offset(pn, "Mesh");
        void* comp = mo > 0 ? *(void**)((char*)pn + mo) : NULL;
        int sko = comp ? prop_offset(comp, "SkeletalMesh") : -1;
        void* mesh = (comp && sko > 0) ? *(void**)((char*)comp + sko) : NULL;
        if (mesh) {
          done = 1;
          void** lods = *(void***)((char*)mesh + 0x130);
          int lodn = *(int*)((char*)mesh + 0x138);
          if (lods && lodn > 0) {
            char* lod = (char*)lods[0];
            char* chunks = *(char**)(lod + 0x10);
            int cn = *(int*)(lod + 0x18);
            LOG("[mn9pawn] inf: chunks=%p n=%d\n", chunks, cn);
            for (int ci = 0; ci < cn && ci < 2; ci++) {
              char* c = chunks + ci * 0x48;
              unsigned char* inf = *(unsigned char**)(c + 0x18);
              int nv = *(int*)(c + 0x20);
              LOG("[mn9pawn] inf chunk%d ptr=%p n=%d\n", ci, inf, nv);
              for (int k = 0; inf && k < 2 && k < nv; k++) {
                unsigned char* e = inf + k * 0x44;
                LOG("[mn9pawn]  rec%d:", k);
                for (int j = 0; j < 0x44; j++) LOG(" %02x", e[j]);
                LOG("\n");
              }
            }
          }
        }
      }
    }
    return;
  }
  if (g_chunk_mode) {
    static int done;
    if (!done) {
      void* pn = find_player_pawn();
      if (pn) {
        int mo = prop_offset(pn, "Mesh");
        void* comp = mo > 0 ? *(void**)((char*)pn + mo) : NULL;
        int sko = comp ? prop_offset(comp, "SkeletalMesh") : -1;
        void* mesh = (comp && sko > 0) ? *(void**)((char*)comp + sko) : NULL;
        if (mesh) {
          done = 1;
          void** lods = *(void***)((char*)mesh + 0x130);
          int lodn = *(int*)((char*)mesh + 0x138);
          LOG("[mn9pawn] chunk: mesh=%p lods=%p n=%d\n", mesh, lods, lodn);
          if (lods && lodn > 0) {
            void* lod = lods[0];
            void** chunks = *(void***)((char*)lod + 0x10);
            int cn = *(int*)((char*)lod + 0x18);
            LOG("[mn9pawn] chunk: lod=%p chunks=%p n=%d\n", lod, chunks, cn);
            for (int ci = 0; chunks && ci < 6; ci++) {
              char* c = (char*)chunks + ci * 0x48;
              LOG("[mn9pawn] CH%d ints:", ci);
              for (int k = 0; k < 18; k++) LOG(" %d", *(int*)(c + k * 4));
              LOG("\n");
              void* bm = *(void**)(c + 0x28);
              int bmn = *(int*)(c + 0x30);
              if (bm && addr_mapped(bm) && bmn > 0 && bmn < 100) {
                LOG("[mn9pawn] BM%d n=%d:", ci, bmn);
                for (int k = 0; k < bmn; k++) LOG(" %d", *(unsigned short*)((char*)bm + k * 2));
                LOG("\n");
              }
            }
          }
        }
      }
    }
    return;
  }
  if (g_freeze_mode) {
    static int done;
    if (!done) {
      void* pn = find_player_pawn();
      if (pn) {
        done = 1;
        static const char* cp[3] = { "Mesh", "ArmLMeshComponent", "ArmRMeshComponent" };
        for (int i = 0; i < 3; i++) {
          int o = prop_offset(pn, cp[i]);
          if (o <= 0) continue;
          void* comp = *(void**)((char*)pn + o);
          if (!comp) continue;
          int at = prop_offset(comp, "AnimTreeTemplate");
          if (at > 0) *(void**)((char*)comp + at) = NULL;
          ((setanim_fn)SETANIMTREE)(comp, NULL);
          LOG("[mn9pawn] freeze %s animtree cleared\n", cp[i]);
        }
      }
    }
    return;
  }
  if (g_probe_mode) {
    static int done;
    if (!done) {
      void* pn = find_player_pawn();
      if (pn) {
        done = 1;
        static const char* cp[3] = { "Mesh", "ArmLMeshComponent", "ArmRMeshComponent" };
        for (int i = 0; i < 3; i++) {
          int o = prop_offset(pn, cp[i]);
          if (o <= 0) continue;
          void* comp = *(void**)((char*)pn + o);
          if (!comp) continue;
          int sk = prop_offset(comp, "SkeletalMesh");
          void* m = sk > 0 ? *(void**)((char*)comp + sk) : NULL;
          if (!m) { LOG("[mn9pawn] probe %s mesh=nil\n", cp[i]); continue; }
          unsigned char fs[16] = {0};
          ((void(*)(void*, void*, void*))GETPATHNAME)(fs, m, NULL);
          wchar_t* d = *(wchar_t**)fs; int n = *(int*)(fs + 8);
          char b[256] = "?"; int k = n > 0 && n < 240 ? n : 0;
          for (int j = 0; j < k; j++) b[j] = (char)(d[j] & 0x7f);
          b[k] = 0;
          LOG("[mn9pawn] probe %s -> %s\n", cp[i], b);
        }
      }
    }
    return;
  }
  if (g_overlay_mode) { overlay_tick(world); return; }
  if (g_model_mode) { model_tick(); return; }
  unsigned tick = g_ticks++;
  if (!g_spawn_armed) {
    findobj_fn find = (findobj_fn)FINDOBJ_ADDR;
    void* beck  = find(NULL, NULL, L"MN9Game.MN9Pl01Pawn", 0);
    void* spw09 = find(NULL, NULL, L"MN9Game.MN9Spw09Pawn", 0);
    wchar_t cp[96];
    spawn_class_path(g_selection, cp, 96);
    void* tgt = find(NULL, NULL, cp, 0);
    if (beck && spw09 && tgt) {
      g_beck_class_pawn = beck;
      g_spw09_class = spw09;
      g_spw01_class = tgt;
      arm_spawn_stub(beck, spw09, tgt);
      g_spawn_armed = 1;
      LOG("[mn9pawn] spawn substitution armed: beck=%p spw09=%p target=%p\n", beck, spw09, tgt);

      // Native swap: make the game spawn the target class as the player pawn.
      static const wchar_t* gis[] = {
        L"MN9Game.Default__MN9GameInfo_Challenge",
        L"MN9Game.Default__MN9GameInfo_EX",
        L"MN9Game.Default__MN9GameInfo",
        L"MN9Game.Default__MN9OnlineGame", NULL };
      for (int i = 0; gis[i]; i++) {
        void* gi = find(NULL, NULL, gis[i], 0);
        if (!gi) continue;
        int o = prop_offset(gi, "DefaultPawnClass");
        if (o > 0) {
          *(void**)((char*)gi + o) = tgt;
          g_native_swap = 1;
          LOG("[mn9pawn] GameInfo %ls DefaultPawnClass -> %p\n", gis[i], tgt);
        } else {
          LOG("[mn9pawn] GameInfo %ls has no DefaultPawnClass offset\n", gis[i]);
        }
      }
    }
  }
  void* pawn = find_player_pawn();
  if (pawn && g_spw01_class && *(void**)((char*)pawn + OFF_CLASS) == g_spw01_class)
    return; // already playing as the target pawn
  if (!pawn) {
    if ((tick & 1023) == 0) LOG("[mn9pawn] no pawn yet (tick %u)\n", tick);
    return;
  }
  if (!resolve_pawn_slots()) {
    if ((tick % 256) == 0) LOG("[mn9pawn] resolve pending (pc=%p arch=%p)\n", g_pc_class, g_arch);
    return;
  }

  if (!g_sk_off) {
    int comps_off = prop_offset(pawn, "Components");
    if (comps_off > 0) {
      void** arr = *(void***)(comps_off + (char*)pawn);
      int n = *(int*)(comps_off + 8 + (char*)pawn);
      for (int c = 0; arr && c < n && c < 8; c++) {
        void* comp = arr[c];
        if (!comp) continue;
        int o = prop_offset(comp, "SkeletalMesh");
        if (o > 0) { g_sk_off = o; break; }
      }
    }
  }
  if (!g_sk_off) return;

  if (pawn != g_pawn) {
    LOG("[mn9pawn] new player pawn=%p\n", pawn);
    comps_dump(pawn);
    g_pawn = pawn;
  }
  if ((tick & 255) == 0) {
    int mo = prop_offset(pawn, "Mesh");
    void* comp = mo > 0 ? *(void**)((char*)pawn + mo) : NULL;
    void* skp = (comp && g_sk_off) ? *(void**)((char*)comp + g_sk_off) : NULL;
    LOG("[mn9pawn] t%u pawn=%p MeshComp.sk=%p\n", tick, pawn, skp);
  }

  // Apply the target archetype's slot assets to the player pawn's named
  // components: Mesh (body), ArmLMeshComponent, ArmRMeshComponent. Then keep
  // the pawn's slot properties in sync so state changes use the same assets.
  setskmesh_fn setmesh = (setskmesh_fn)SETSKMESH;
  setanim_fn setanim = (setanim_fn)SETANIMTREE;
  int hits = 0;

  // Collect components owned by this pawn (cached; they are stable), then
  // map each component by which Beck mesh it currently holds.
  if (!g_cache_n) {
    void** objs = *(void***)OBJ_ARRAY;
    int count = *(int*)OBJ_COUNT;
    int low = count - 120000; if (low < 0) low = 0;
    for (int i = count - 1; i >= low && g_cache_n < 64; i--) {
      if (!addr_mapped(objs + i)) break;
      void* o = objs[i];
      if (!o || !addr_mapped(o)) continue;
      int own = prop_offset(o, "Owner");
      if (own <= 0) continue;
      if (*(void**)((char*)o + own) != pawn) continue;
      if (prop_offset(o, "SkeletalMesh") <= 0) continue;
      g_cache_comps[g_cache_n++] = o;
    }
    LOG("[mn9pawn] owned comps cached: %d\n", g_cache_n);
  }

  for (int c = 0; c < g_cache_n; c++) {
    void* comp = g_cache_comps[c];
    int sk = prop_offset(comp, "SkeletalMesh");
    if (sk <= 0) continue;
    void* cur = *(void**)((char*)comp + sk);
    if (!cur) continue;
    int slot = -1;
    if (cur == g_beck_mesh[0] || cur == g_beck_mesh[1] || cur == g_beck_mesh[2]) slot = 0;
    else if (cur == g_beck_mesh[3] || cur == g_beck_mesh[4]) slot = 3;
    else if (cur == g_beck_mesh[5] || cur == g_beck_mesh[6]) slot = 5;
    if (slot < 0 || !g_arch_slot[slot]) continue;
    if (cur != g_arch_slot[slot]) {
      LOG("[mn9pawn] owned apply comp=%p sk=%p -> %p (slot %d)\n", comp, cur, g_arch_slot[slot], slot);
      setmesh(comp, g_arch_slot[slot], 1);
      hits++;
    }
  }
  // explicit body (the generic pass expects older Beck mesh names)
  {
    int mo = prop_offset(pawn, "Mesh");
    void* bcomp = mo > 0 ? *(void**)((char*)pawn + mo) : NULL;
    void* basset = g_arch_slot[0];
    if (bcomp && basset) {
      int sk = prop_offset(bcomp, "SkeletalMesh");
      void* cur = sk > 0 ? *(void**)((char*)bcomp + sk) : NULL;
      if (cur != basset) { setmesh(bcomp, basset, 1); LOG("[mn9pawn] body -> %p\n", basset); }
      int so = prop_offset(pawn, SLOT_PROPS[0]); if (so > 0) *(void**)((char*)pawn + so) = basset;
    }
  }
  // the generic pass can miss an arm whose current mesh is a buster-only asset
  {
    static const char* armcomp[2] = {"ArmLMeshComponent", "ArmRMeshComponent"};
    static const int armslot[2] = {3, 5};
    for (int ai = 0; ai < 2; ai++) {
      int co = prop_offset(pawn, armcomp[ai]);
      void* comp = co > 0 ? *(void**)((char*)pawn + co) : NULL;
      void* asset = g_arch_slot[armslot[ai]];
      if (!comp || !asset) continue;
      int sk = prop_offset(comp, "SkeletalMesh");
      void* cur = sk > 0 ? *(void**)((char*)comp + sk) : NULL;
      if (cur != asset) { setmesh(comp, asset, 1); LOG("[mn9pawn] arm %s -> %p\n", armcomp[ai], asset); }
      int so = prop_offset(pawn, SLOT_PROPS[armslot[ai]]);
      if (so > 0) *(void**)((char*)pawn + so) = asset;
    }
  }
  // keep pawn slot properties in sync with the archetype
  for (int i = 0; i < 7; i++) {
    if (!g_arch_slot[i]) continue;
    int off = prop_offset(pawn, SLOT_PROPS[i]);
    if (off > 0) *(void**)((char*)pawn + off) = g_arch_slot[i];
  }
  if (!g_logged) {
    g_logged = 1;
    LOG("[mn9pawn] named-component apply on pawn=%p (hits %d)\n", pawn, hits);
  }

  // --- Pawn spawn + possess (Pyro) ---
  static int swap_ok;
  if (0 && pawn != g_swapped_pawn) {
    g_swapped_pawn = pawn;   // mark attempted for this pawn
    findobj_fn find = (findobj_fn)FINDOBJ_ADDR;
    wchar_t cls_path[96];
    spawn_class_path(g_selection, cls_path, 96);
    void* spw01 = find(NULL, NULL, cls_path, 0);
    LOG("[mn9pawn] spawn class %ls -> %p\n", cls_path, spw01);
    int co = prop_offset(pawn, "Controller");
    void* ctrl = co > 0 ? *(void**)((char*)pawn + co) : NULL;
    int lo = prop_offset(pawn, "Location");
    int ro = prop_offset(pawn, "Rotation");
    g_spw01_class = spw01;
    LOG("[mn9pawn] pawnswap: spw01=%p ctrl=%p loc=%d rot=%d world=%p\n",
        spw01, ctrl, lo, ro, world);
    if (spw01 && ctrl && lo > 0 && ro > 0 && world && addr_mapped(world)) {
      spawn_fn sp = (spawn_fn)SPAWNACTOR_ADDR;
      float loc[3];
      for (int i = 0; i < 3; i++) loc[i] = *(float*)((char*)pawn + lo + i * 4);
      loc[2] += 150.0f;
      void* np = sp(world, spw01, 0, loc, (char*)pawn + ro,
                    NULL, 1, 0, NULL, NULL, 0, NULL);
      LOG("[mn9pawn] pawnswap: spawned=%p\n", np);
      if (np && addr_mapped(np)) {
        uint64_t fnn = fname("Possess");
        void* fn = fnn ? ((void*(*)(void*, uint64_t, unsigned))FINDFUNC_ADDR)(ctrl, fnn, 0) : NULL;
        LOG("[mn9pawn] pawnswap: Possess fn=%p parms=%d\n", fn, fn ? *(int*)((char*)fn + 0x88) : -1);
        // shrink the boss to player scale so the camera sees it
        int ods = prop_offset(pawn, "DrawScale");
        int nds = prop_offset(np, "DrawScale");
        if (ods > 0 && nds > 0) {
          float oldscale = *(float*)((char*)pawn + ods);
          float newscale = *(float*)((char*)np + nds);
          LOG("[mn9pawn] pawnswap: DrawScale old=%.3f new=%.3f -> 0.450\n", oldscale, newscale);
          *(float*)((char*)np + nds) = 0.45f;
        }
        if (fn) {
          unsigned char parms[64];
          for (int i = 0; i < 64; i++) parms[i] = 0;
          *(void**)parms = np;
          ((void(*)(void*, void*, void*, void*))PROCESSEVENT_ADDR)(ctrl, fn, parms, NULL);
          LOG("[mn9pawn] pawnswap: possess called\n");
          swap_ok = 1;
          g_swapped_pawn = pawn;
        }
      }
    }
    if (swap_ok) { g_pawn = NULL; return; }
  }
  return;

  // Full-rig apply: copy mesh + anim tree from the Pyro form archetype's
  // component subobjects onto the player pawn's corresponding components.
  if (g_form0 && addr_mapped(g_form0)) {
    static const char* comp_props[3] = { "Mesh", "ArmLMeshComponent", "ArmRMeshComponent" };
    for (int k = 0; k < 3; k++) {
      int so = prop_offset(g_form0, comp_props[k]);
      int po = prop_offset(pawn, comp_props[k]);
      if (so <= 0 || po <= 0) continue;
      void* src = *(void**)((char*)g_form0 + so);
      void* dst = *(void**)((char*)pawn + po);
      if (!src || !dst || !addr_mapped(src) || !addr_mapped(dst)) continue;
      int ssk = prop_offset(src, "SkeletalMesh");
      int dsk = prop_offset(dst, "SkeletalMesh");
      if (ssk > 0 && dsk > 0) {
        void* m = *(void**)((char*)src + ssk);
        if (m && *(void**)((char*)dst + dsk) != m) {
          LOG("[mn9pawn] rig %s mesh -> %p\n", comp_props[k], m);
          setmesh(dst, m, 1);
        }
      }
      int sat = prop_offset(src, "AnimTreeTemplate");
      int dat = prop_offset(dst, "AnimTreeTemplate");
      if (sat > 0 && dat > 0) {
        void* t = *(void**)((char*)src + sat);
        if (t && *(void**)((char*)dst + dat) != t) {
          LOG("[mn9pawn] rig %s tree -> %p\n", comp_props[k], t);
          setanim(dst, t);
        }
      }
    }
  }

  static int form_probed;
  if (!form_probed) {
    form_probed = 1;
    int co = prop_offset(pawn, "Controller");
    void* ctrl = co > 0 ? *(void**)((char*)pawn + co) : NULL;
    if (ctrl && addr_mapped(ctrl)) {
      int so = prop_offset(ctrl, "SpwArchetyped");
      if (so > 0) {
        g_form0 = *(void**)((char*)ctrl + so);
        LOG("[mn9pawn] form0 archetype=%p\n", g_form0);
      }
    }
  }

  // --- form table probe (read-only) ---
  static int probed;
  if (!probed) {
    probed = 1;
    findobj_fn find = (findobj_fn)FINDOBJ_ADDR;
    int co = prop_offset(pawn, "Controller");
    void* ctrl = co > 0 ? *(void**)((char*)pawn + co) : NULL;
    LOG("[mn9pawn] probe controller=%p\n", ctrl);
    if (ctrl && addr_mapped(ctrl)) {
      int so = prop_offset(ctrl, "SpwArchetyped");
      LOG("[mn9pawn] SpwArchetyped off=0x%x\n", so);
      if (so > 0) {
        for (int i = 0; i < 9; i++) {
          void* e = *(void**)((char*)ctrl + so + i * 8);
          fstring_t fs = {0,0,0};
          if (e && addr_mapped(e)) ((getname_fn)GETNAME_ADDR)(&fs, e);
          char nm[64] = "?";
          if (fs.data && fs.count > 0) {
            int n = fs.count < 63 ? fs.count : 63;
            for (int k = 0; k < n; k++) nm[k] = (char)(fs.data[k] & 0xff);
            nm[n] = 0;
          }
          void* ref = find(NULL, NULL, L"MN9Game.Default__MN9Spw01Pawn", 0);
          LOG("[mn9pawn] form[%d]=%p name='%s' spw01=%d\n", i, e, nm, e == ref);
        }
      }
    }
  }


}

__attribute__((naked)) static void tick_entry(void)
{
  __asm__(
    "subq $40, %rsp\n"
    "movq %rdi, 0(%rsp)\n"
    "movq %rsi, 8(%rsp)\n"
    "movdqu %xmm0, 16(%rsp)\n"
    "call mega_tick\n"
    "movq 0(%rsp), %rdi\n"
    "movq 8(%rsp), %rsi\n"
    "movdqu 16(%rsp), %xmm0\n"
    "addq $40, %rsp\n"
    "popq %rbx\n"
    "popq %r12\n"
    "popq %r13\n"
    "popq %r14\n"
    "popq %r15\n"
    "popq %rbp\n"
    "jmp *g_real_tick(%rip)\n"
  );
}

static void arm_spawn_stub(void* beck, void* spw09, void* target)
{
  if (!g_spawn_stub) return;
  memcpy(g_spawn_stub + 2,  &beck,   8);
  memcpy(g_spawn_stub + 21, &spw09,  8);
  memcpy(g_spawn_stub + 40, &target, 8);
  uint64_t back = SPAWNACTOR_ADDR + 10;
  memcpy(g_spawn_stub + 60, &back, 8);
  __builtin___clear_cache((char*)g_spawn_stub, (char*)g_spawn_stub + SPAWN_STUB_SZ);
}

static unsigned char* g_near_page;

/* Install the native character spawn hook (see spawn_hook above). */
static int install_spawn_hook(unsigned char* near)
{
  if (g_spawn_hooked) return 0;
  unsigned char expect[10] = { 0x55, 0x41, 0x57, 0x41, 0x56, 0x41, 0x55, 0x41, 0x54, 0x53 };
  unsigned char* target = (unsigned char*)SPAWNACTOR_ADDR;
  if (memcmp(target, expect, sizeof expect) != 0) {
    LOG("[mn9pawn] spawn hook: unexpected prologue\n");
    return -1;
  }

  // entry stub: jump straight into spawn_hook before the prologue runs
  unsigned char absjmp[12] = { 0x48, 0xb8, 0,0,0,0,0,0,0,0, 0xff, 0xe0 };
  unsigned char* st = near + SPAWN_STUB_OFF;
  memset(st, 0x90, SPAWN_STUB_SZ);
  uint64_t entry = (uint64_t)(uintptr_t)spawn_hook;
  memcpy(absjmp + 2, &entry, 8);
  memcpy(st, absjmp, 12);

  // real body: original prologue, then resume at +10
  unsigned char* real = st + 0x40;
  memcpy(real, target, 10);
  uint64_t back = SPAWNACTOR_ADDR + 10;
  memcpy(absjmp + 2, &back, 8);
  memcpy(real + 10, absjmp, 12);
  g_real_spawn = (void*)real;
  g_spawn_stub = st;

  long ps = sysconf(_SC_PAGESIZE);
  uintptr_t page = (uintptr_t)target & ~(uintptr_t)(ps - 1);
  if (mprotect((void*)page, ps, PROT_READ | PROT_WRITE) != 0) return -1;
  int32_t rel = (int32_t)((uintptr_t)st - (uintptr_t)(target + 5));
  target[0] = 0xE9;
  memcpy(target + 1, &rel, 4);
  memset(target + 5, 0x90, 5);
  mprotect((void*)page, ps, PROT_READ | PROT_EXEC);
  __builtin___clear_cache((char*)target, (char*)target + 10);
  g_spawn_hooked = 1;
  LOG("[mn9pawn] native spawn hook installed\n");
  return 0;
}

static int addr_in_maps(unsigned long a)
{
  FILE* f = fopen("/proc/self/maps","r");
  if (!f) return 1;
  char line[512];
  while (fgets(line,sizeof line,f)) {
    unsigned long lo,hi; char perms[8];
    if (sscanf(line,"%lx-%lx %7s",&lo,&hi,perms)!=3) continue;
    if (a>=lo && a<hi) { fclose(f); return 1; }
  }
  fclose(f); return 0;
}

static int install_tick_hook(void)
{
  unsigned char expect[10] = { 0x55, 0x41, 0x57, 0x41, 0x56, 0x41, 0x55, 0x41, 0x54, 0x53 };
  unsigned char* target = (unsigned char*)TICK_ADDR;
  LOG("[mn9pawn] tickh: target=%p\n", (void*)target);
  if (!addr_in_maps(TICK_ADDR)) { LOG("[mn9pawn] tickh: not mapped, defer\n"); return -1; }
  if (memcmp(target, expect, sizeof expect) != 0) {
    fprintf(stderr, "[mn9pawn] unexpected UWorld::Tick prologue\n");
    return -1;
  }

  long ps = sysconf(_SC_PAGESIZE);
  unsigned char* near = MAP_FAILED;
  for (long k = 0; k <= 64 && near == MAP_FAILED; k++) {
    void* want = (void*)(0x20000000UL + (unsigned long)(k * ps));
    near = mmap(want, ps, PROT_READ | PROT_WRITE | PROT_EXEC,
                MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
  }
  if (near == MAP_FAILED) {
    near = mmap(NULL, ps, PROT_READ | PROT_WRITE | PROT_EXEC,
                MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (near != MAP_FAILED) {
      uintptr_t d1 = (uintptr_t)near > TICK_ADDR ? (uintptr_t)near - TICK_ADDR : TICK_ADDR - (uintptr_t)near;
      uintptr_t d2 = (uintptr_t)near > SPAWNACTOR_ADDR ? (uintptr_t)near - SPAWNACTOR_ADDR : SPAWNACTOR_ADDR - (uintptr_t)near;
      if (d1 > 0x7F000000UL || d2 > 0x7F000000UL) { munmap(near, ps); near = MAP_FAILED; }
    }
  }
  if (near == MAP_FAILED) { LOG("[mn9pawn] near page unavailable\n"); return -1; }
  g_near_page = near;

  unsigned char absjmp[12] = { 0x48, 0xb8, 0,0,0,0,0,0,0,0, 0xff, 0xe0 };
  memcpy(near, target, 10);
  uint64_t entry = (uint64_t)(uintptr_t)tick_entry;
  memcpy(absjmp + 2, &entry, 8);
  memcpy(near + 10, absjmp, 12);

  LOG("[mn9pawn] tickh: near=%p\n", (void*)near);
  unsigned char* real = near + 0x40;
  memcpy(real, target, 10);
  uint64_t back = TICK_ADDR + 10;
  memcpy(absjmp + 2, &back, 8);
  memcpy(real + 10, absjmp, 12);
  g_real_tick = (tick_fn)real;

  uintptr_t page = (uintptr_t)target & ~(uintptr_t)(ps - 1);
  LOG("[mn9pawn] tickh: mprotect page=%p\n", (void*)page);
  if (mprotect((void*)page, ps, PROT_READ | PROT_WRITE) != 0) return -1;
  target[0] = 0xE9;
  int32_t rel = (int32_t)((uintptr_t)near - (uintptr_t)(target + 5));
  memcpy(target + 1, &rel, 4);
  memset(target + 5, 0x90, 5);
  mprotect((void*)page, ps, PROT_READ | PROT_EXEC);
  return 0;
}

__attribute__((constructor)) static void mn9pawn_init(void)
{
  const char* gd = getenv("MN9_GAME_DIR");
  if (!gd || !*gd) return;
  {
    char lp[1200];
    snprintf(lp, sizeof lp, "%s/.mn9charsel/swap.log", gd);
    g_logf = fopen(lp, "a");
    LOG("[mn9pawn] init enter\n");
  }

  char conf[1200];
  snprintf(conf, sizeof conf, "%s/.mn9charsel/character", gd);
  FILE* f = fopen(conf, "r");
  if (!f) return;
  if (fgets(g_selection, sizeof g_selection, f)) {
    char* p = g_selection;
    while (*p && *p != '\n' && *p != '\r' && *p != ' ') p++;
    *p = 0;
  }
  fclose(f);

  // Independent swap target: .mn9charsel/swap_target (or MN9_SWAP_TARGET),
  // so the wrapper still loads this shim via character=megaxel while the
  // body comes from any other character archetype.
  int allow_swap = !strcmp(g_selection, "megaxel");
  const char* tgt = getenv("MN9_SWAP_TARGET");
  if (tgt && *tgt) {
    snprintf(g_selection, sizeof g_selection, "%s", tgt);
  } else if (allow_swap) {
    char tpath[1200];
    snprintf(tpath, sizeof tpath, "%s/.mn9charsel/swap_target", gd);
    FILE* tf = fopen(tpath, "r");
    if (tf) {
      char buf[32] = {0};
      if (fgets(buf, sizeof buf, tf)) {
        char* q = buf;
        while (*q && *q != '\n' && *q != '\r' && *q != ' ') q++;
        *q = 0;
        if (*buf) snprintf(g_selection, sizeof g_selection, "%s", buf);
      }
      fclose(tf);
    }
  }
  if (!strncmp(g_selection, "model:", 6)) {
    g_model_mode = 1;
    LOG("[mn9pawn] model mode for %s\n", g_selection + 6);
  } else if (!strncmp(g_selection, "matvis:", 7)) {
    g_matvis_mode = 1;
  } else if (!strncmp(g_selection, "demoboss:", 9)) {
    g_demoboss_mode = 1;
    if (strstr(g_selection, "test")) g_demo_test = 1;
    if (strstr(g_selection, "atk")) g_demo_atktest = 1;
    if (!strncmp(g_selection + 9, "bs", 2)) {
      int id = atoi(g_selection + 11);
      if (id >= 1 && id <= 20) { g_demo_boss = id; snprintf(g_demo_prefix, sizeof g_demo_prefix, "Bs%02d_", id); }
    }
    g_demo_yaw_flip = 1;  // neutral actor: all boss meshes use the same authored forward
    snprintf(g_demo_idle, sizeof g_demo_idle, "%sStay", g_demo_prefix);
    snprintf(g_demo_run, sizeof g_demo_run, "%sRun", g_demo_prefix);
    snprintf(g_demo_jumpup, sizeof g_demo_jumpup, "%sJumpUp", g_demo_prefix);
    snprintf(g_demo_jumpdn, sizeof g_demo_jumpdn, "%sJumpDown", g_demo_prefix);
    snprintf(g_demo_land, sizeof g_demo_land, "%sJumpLanding", g_demo_prefix);
    LOG("[mn9pawn] demoboss mode boss=%d prefix=%s\n", g_demo_boss, g_demo_prefix);
  } else if (!strncmp(g_selection, "visactor:", 9)) {
    g_visactor_mode = 1;
    LOG("[mn9pawn] visactor mode\n");
  } else if (!strncmp(g_selection, "bossanim:", 9)) {
    g_bossanim_mode = 1;
    LOG("[mn9pawn] bossanim mode\n");
  } else if (!strncmp(g_selection, "fire:", 5)) {
    g_fire_mode = 1;
    LOG("[mn9pawn] fire mode\n");
  } else if (!strncmp(g_selection, "driv:", 5)) {
    g_driv_mode = 1;
    LOG("[mn9pawn] driv mode\n");
  } else if (!strncmp(g_selection, "archbs:", 7)) {
    g_archbs_mode = 1;
    LOG("[mn9pawn] archbs mode\n");
  } else if (!strncmp(g_selection, "meshonly:", 9)) {
    g_meshonly_mode = 1;
    LOG("[mn9pawn] meshonly mode\n");
  } else if (!strncmp(g_selection, "banim:", 6)) {
    g_banim_mode = 1;
    LOG("[mn9pawn] banim mode\n");
  } else if (!strncmp(g_selection, "mimic:", 6)) {
    g_mimic_mode = 1;
    LOG("[mn9pawn] mimic mode\n");
  } else if (!strncmp(g_selection, "vis:", 4)) {
    g_vis_mode = 1;
    LOG("[mn9pawn] vis mode\n");
  } else if (!strncmp(g_selection, "classbs:", 8)) {
    g_classbs_mode = 1;
    LOG("[mn9pawn] classbs mode\n");
  } else if (!strncmp(g_selection, "comps:", 6)) {
    g_comps_mode = 1;
    LOG("[mn9pawn] comps mode\n");
  } else if (!strncmp(g_selection, "pose:", 5)) {
    g_pose_mode = 1;
    LOG("[mn9pawn] pose mode\n");
  } else if (!strncmp(g_selection, "puppet:", 7)) {
    g_puppet_mode = 1;
    LOG("[mn9pawn] puppet mode\n");
  } else if (!strncmp(g_selection, "gb:", 3)) {
    g_gb_mode = 1;
    LOG("[mn9pawn] gb mode\n");
  } else if (!strncmp(g_selection, "rb:", 3)) {
    g_rb_mode = 1;
    LOG("[mn9pawn] rb mode\n");
  } else if (!strncmp(g_selection, "retarget:", 9)) {
    g_retarget_mode = 1;
    LOG("[mn9pawn] retarget mode\n");
  } else if (!strncmp(g_selection, "tex2:", 5)) {
    g_tex2_mode = 1;
    LOG("[mn9pawn] tex2 mode\n");
  } else if (!strncmp(g_selection, "tex:", 4)) {
    g_tex_mode = 1;
    LOG("[mn9pawn] tex mode\n");
  } else if (!strncmp(g_selection, "gpu8:", 5)) {
    g_gpu8_mode = 1;
    LOG("[mn9pawn] gpu8 mode\n");
  } else if (!strncmp(g_selection, "mats:", 5)) {
    g_mats_mode = 1;
    LOG("[mn9pawn] mats mode\n");
  } else if (!strncmp(g_selection, "bind3:", 6)) {
    g_b3_mode = 1;
    LOG("[mn9pawn] bind3 mode\n");
  } else if (!strncmp(g_selection, "bind2:", 6)) {
    g_b2_mode = 1;
    LOG("[mn9pawn] bind2 mode\n");
  } else if (!strncmp(g_selection, "truenames:", 10)) {
    g_tn_mode = 1;
    LOG("[mn9pawn] truenames mode\n");
  } else if (!strncmp(g_selection, "match:", 6)) {
    g_match_mode = 1;
    LOG("[mn9pawn] match mode\n");
  } else if (!strncmp(g_selection, "poseprobe:", 10)) {
    g_poseprobe_mode = 1;
    LOG("[mn9pawn] poseprobe mode\n");
  } else if (!strncmp(g_selection, "maptest:", 8)) {
    g_maptest_mode = 1;
    LOG("[mn9pawn] maptest mode\n");
  } else if (!strncmp(g_selection, "bind:", 5)) {
    g_bind_mode = 1;
    LOG("[mn9pawn] bind mode\n");
  } else if (!strncmp(g_selection, "seqs:", 5)) {
    g_seqs_mode = 1;
    LOG("[mn9pawn] seqs mode\n");
  } else if (!strncmp(g_selection, "lsn:", 4)) {
    g_lsn_mode = 1;
    LOG("[mn9pawn] lsn mode %s\n", g_selection + 4);
  } else if (!strncmp(g_selection, "swapm:", 6)) {
    g_swapm_mode = 1;
    LOG("[mn9pawn] swapm mode %s\n", g_selection + 6);
  } else if (!strncmp(g_selection, "swapnull:", 9)) {
    g_swapnull_mode = 1;
    LOG("[mn9pawn] swapnull mode\n");
  } else if (!strncmp(g_selection, "swapcomp:", 9)) {
    g_swapcomp_mode = 1;
    LOG("[mn9pawn] swapcomp mode\n");
  } else if (!strncmp(g_selection, "follow:", 7)) {
    g_follow_mode = 1;
    LOG("[mn9pawn] follow mode\n");
  } else if (!strncmp(g_selection, "gpu3:", 5)) {
    g_gpu3_mode = 1;
    LOG("[mn9pawn] gpu3 mode\n");
  } else if (!strncmp(g_selection, "gpu2:", 5)) {
    g_gpu2_mode = 1;
    LOG("[mn9pawn] gpu2 mode\n");
  } else if (!strncmp(g_selection, "gpu:", 4)) {
    g_gpu_mode = 1;
    LOG("[mn9pawn] gpu mode\n");
  } else if (!strncmp(g_selection, "bchunk:", 7)) {
    g_bchunk_mode = 1;
    LOG("[mn9pawn] bchunk mode\n");
  } else if (!strncmp(g_selection, "inf:", 4)) {
    g_inf_mode = 1;
    LOG("[mn9pawn] inf mode\n");
  } else if (!strncmp(g_selection, "chunk:", 6)) {
    g_chunk_mode = 1;
    LOG("[mn9pawn] chunk mode\n");
  } else if (!strncmp(g_selection, "freeze:", 7)) {
    g_freeze_mode = 1;
    LOG("[mn9pawn] freeze mode\n");
  } else if (!strncmp(g_selection, "probe:", 6)) {
    g_probe_mode = 1;
    LOG("[mn9pawn] probe mode\n");
  } else if (!strncmp(g_selection, "overlay:", 8)) {
    g_overlay_mode = 1;
    LOG("[mn9pawn] overlay mode for %s\n", g_selection + 8);
  }
  if (!strcmp(g_selection, "call")) {
    const char* ba = getenv("MN9_BARRIER_ABSORB");
    g_ba_enabled = !ba || (*ba && *ba != '0');
    const char* dbg = getenv("MN9_BARRIER_DEBUG");
    g_ba_debug = dbg && *dbg && *dbg != '0';
    LOG("[mn9bar] barrier absorb %s debug=%d\n", g_ba_enabled ? "on" : "off", g_ba_debug);
  }
  if (!strcmp(g_selection, "beck") || !*g_selection) return;

  {
    char lpath[1200];
    snprintf(lpath, sizeof lpath, "%s/.mn9charsel/swap.log", gd);
    g_logf = fopen(lpath, "a");
  }
  LOG("[mn9pawn] installing tick hook\n");
  if (install_tick_hook() != 0) {
    LOG("[mn9pawn] tick hook install failed\n");
    return;
  }
  if (native_spawn_wanted() && g_near_page) {
    if (install_spawn_hook(g_near_page) != 0)
      LOG("[mn9pawn] native spawn hook install failed\n");
  }
  LOG("[mn9pawn] character swap hook ready (%s)\n", g_selection);
}
