// Mighty No. 9 PC audio shim.
// Hooks OpenAL and classifies sources by buffer length:
//   long (>=5s, or >=1.5s with repeated queueing) = music
//   <=0.45s = SFX, 0.45..5s = voice
// It can mute music, replace it with random files from a folder, randomize
// SFX pitch, mute voice, and apply per-bus gain.
#define _GNU_SOURCE
#include <dlfcn.h>
#include <link.h>
#include <dirent.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <math.h>
#include <ctype.h>
#include <stdint.h>

typedef unsigned int ALuint;
typedef int ALint;
typedef int ALenum;
typedef int ALsizei;
typedef float ALfloat;
typedef long long sf_count_t;

#define AL_NO_ERROR 0
#define AL_BUFFER 0x1009
#define AL_SOURCE_STATE 0x1010
#define AL_PLAYING 0x1012
#define AL_LOOPING 0x1007
#define AL_GAIN 0x100A
#define AL_PITCH 0x1003
#define AL_FORMAT_MONO16 0x1101
#define AL_FORMAT_STEREO16 0x1103
#define AL_SOURCE_RELATIVE 0x0202
#define AL_POSITION 0x1004
#define AL_TRUE 1
#define AL_CONE_INNER_ANGLE 0x1001
#define AL_CONE_OUTER_ANGLE 0x1002
#define AL_DIRECTION 0x1005
#define AL_MIN_GAIN 0x100D
#define AL_MAX_GAIN 0x100E
#define AL_ROLLOFF_FACTOR 0x1021
#define AL_CONE_OUTER_GAIN 0x1022
#define AL_DIRECT_FILTER 0x20005
#define AL_AUXILIARY_SEND_FILTER 0x20006
#define AL_FREQUENCY 0x2001
#define AL_BITS 0x2002
#define AL_CHANNELS 0x2003
#define AL_SIZE 0x2004
#define SFM_READ 0x10

typedef struct { sf_count_t frames; int samplerate; int channels; int format; int sections; int seekable; } SF_INFO;
typedef struct SNDFILE_tag SNDFILE;

static struct {
  void (*gen_sources)(ALsizei, ALuint*);
  void (*del_sources)(ALsizei, const ALuint*);
  void (*play)(ALuint);
  void (*stop)(ALuint);
  void (*queue)(ALuint, ALsizei, const ALuint*);
  void (*unqueue)(ALuint, ALsizei, ALuint*);
  void (*bufdata)(ALuint, ALenum, const void*, ALsizei, ALsizei);
  void (*gen_buffers)(ALsizei, ALuint*);
  void (*del_buffers)(ALsizei, const ALuint*);
  void (*sourcei)(ALuint, ALenum, ALint);
  void (*sourcef)(ALuint, ALenum, ALfloat);
  void (*sourcefv)(ALuint, ALenum, const ALfloat*);
  void (*getsourcei)(ALuint, ALenum, ALint*);
  void (*getsourcef)(ALuint, ALenum, ALfloat*);
  void (*getbufferiv)(ALuint, ALenum, ALint*);
  ALenum (*geterror)(void);
} al;

static SNDFILE* (*sf_open_)(const char*, int, SF_INFO*);
static sf_count_t (*sf_readf_short_)(SNDFILE*, short*, sf_count_t);
static int (*sf_close_)(SNDFILE*);

#define MAX_ID 65536
static ALsizei g_buf_bytes[MAX_ID];
static ALsizei g_buf_freq[MAX_ID];
static ALenum  g_buf_fmt[MAX_ID];
static int g_src_queue[MAX_ID];
static int g_src_cat[MAX_ID];      // 0 unknown, 1 music, 2 sfx, 3 voice
static float g_src_gain[MAX_ID];
static int g_src_playing_music[MAX_ID];
static float g_src_pitch[MAX_ID];
static int g_sfx_pitch_log = 0;

static int g_music_mode = 0;       // 0 game, 1 mute, 2 random
static int g_voice_mute = 0;
static float g_voice_pitch = 1.0f, g_voice_pitch_rand = 0.0f;
static double g_voice_line_last = 0.0;
static float g_voice_line_pitch = 1.0f;
static int g_voice_random = 0;
static ALuint g_voice_ext_src = 0, g_voice_ext_buf = 0;
static ALuint g_voice_game_src = 0;
static double g_voice_ext_until = 0;
static int g_voice_log = 0;
static int g_voice_state_log = 0;
static int g_voice_dump_count = 0;
#define VOICE_CACHE_MAX 512
#define VOICE_CACHE_BYTES (64ull * 1024ull * 1024ull)
typedef struct { ALuint id; short* pcm; ALsizei sz; ALsizei freq; ALenum fmt; double dur; } VoiceClip;
static VoiceClip g_vc[VOICE_CACHE_MAX];
static int g_vc_count = 0;
static size_t g_vc_bytes = 0;
static int g_voice_pool[VOICE_CACHE_MAX];
static int g_voice_pool_count = 0;
static float g_sfx_pitch = 0.0f;
static float g_music_gain = 1.0f, g_sfx_gain = 1.0f, g_voice_gain = 1.0f;
static float g_music_game_gain = 1.0f;
static ALuint g_bgm_src = 0;
static double g_bgm_dur = 0.0;
static char g_music_dir[1024];
static char g_files[512][1024];
static int g_file_count = -1;
static int g_last_file = -1;
static unsigned int g_rng = 1;
static double g_last_music_start = -100.0;
static ALuint g_ext_src = 0, g_ext_buf = 0;
static int g_music_playing = 0;
static int g_play_log = 0;

static char g_map_path[1024];
static int g_map_loaded = 0;
typedef struct { char pattern[96]; char path[1024]; } MapEntry;
static MapEntry g_map[128];
static int g_map_count = 0;
static char g_stage[128];
static double g_stage_last = 0;
static char g_cur_key[160];        // stage/BGM the assigned track belongs to
static char g_cur_path[1024];      // track currently playing for that BGM
static int g_cur_valid = 0;
typedef struct { char key[160]; char path[1024]; } MusicAssign;
static MusicAssign g_assign[64];   // remembered stage/BGM -> track for this run
static int g_assign_count = 0;
static const char* assign_lookup(const char* key) {
  for (int i = 0; i < g_assign_count; i++)
    if (!strcmp(g_assign[i].key, key)) return g_assign[i].path;
  return NULL;
}
static void assign_forget(const char* key) {
  for (int i = 0; i < g_assign_count; i++) {
    if (!strcmp(g_assign[i].key, key)) {
      g_assign[i] = g_assign[--g_assign_count];
      return;
    }
  }
}
static void assign_store(const char* key, const char* path) {
  for (int i = 0; i < g_assign_count; i++) {
    if (!strcmp(g_assign[i].key, key)) {
      snprintf(g_assign[i].path, sizeof(g_assign[i].path), "%s", path);
      return;
    }
  }
  if (g_assign_count < (int)(sizeof(g_assign) / sizeof(g_assign[0]))) {
    snprintf(g_assign[g_assign_count].key, sizeof(g_assign[0].key), "%s", key);
    snprintf(g_assign[g_assign_count].path, sizeof(g_assign[0].path), "%s", path);
    g_assign_count++;
  }
}

static int g_duck = 1;
static float g_duck_factor = 0.35f;
static double g_duck_until = 0;
static int g_night = 0;
static int g_next_index = 0;
static int g_bag[512], g_bag_pos = 0, g_bag_len = 0;
static ALuint g_music_srcs[64];
static int g_music_src_count = 0;

static FILE* g_dlog = NULL;
static char g_dlog_path[1400];
static float g_src_pitch_game[MAX_ID];
static float g_listener_gain_last = 1.0f;

static void dlog_open(void) {
  if (g_dlog) return;
  const char* explicit_path = getenv("MN9_DIALOG_LOG");
  if (explicit_path && *explicit_path) {
    snprintf(g_dlog_path, sizeof(g_dlog_path), "%s", explicit_path);
  } else {
    const char* gd = getenv("MN9_GAME_DIR");
    if (gd && *gd) snprintf(g_dlog_path, sizeof(g_dlog_path), "%s/.mn9charsel/dialog.log", gd);
    else snprintf(g_dlog_path, sizeof(g_dlog_path), "/tmp/mn9-dialog.log");
  }
  g_dlog = fopen(g_dlog_path, "a");
  if (g_dlog) {
    struct timespec ts; clock_gettime(CLOCK_REALTIME, &ts);
    struct tm tm; localtime_r(&ts.tv_sec, &tm);
    fprintf(g_dlog, "\n[%02d:%02d:%02d.%03ld] ---- session pid=%d ----\n",
            tm.tm_hour, tm.tm_min, tm.tm_sec, ts.tv_nsec / 1000000, (int)getpid());
    fflush(g_dlog);
  }
}
static void dlog(const char* fmt, ...) {
  dlog_open();
  if (!g_dlog) return;
  struct timespec ts; clock_gettime(CLOCK_REALTIME, &ts);
  struct tm tm; localtime_r(&ts.tv_sec, &tm);
  fprintf(g_dlog, "[%02d:%02d:%02d.%03ld] ", tm.tm_hour, tm.tm_min, tm.tm_sec, ts.tv_nsec / 1000000);
  va_list ap; va_start(ap, fmt);
  vfprintf(g_dlog, fmt, ap);
  va_end(ap);
  fputc('\n', g_dlog);
  fflush(g_dlog);
}

static int voice_debug(void) {
  static int d = -1;
  if (d < 0) { const char* v = getenv("MN9_VOICE_DEBUG"); d = (v && *v && *v != '0'); }
  return d;
}

static void* (*real_alc_open)(const char*);
static void* (*real_alc_create)(void*, const int*);
static int   (*real_alc_make_current)(void*);
static void* (*real_alc_get_current)(void);
static void* (*real_alc_get_device)(void*);

void* alcOpenDevice(const char* name) {
  if (!real_alc_open) real_alc_open = dlsym(RTLD_NEXT, "alcOpenDevice");
  void* d = real_alc_open ? real_alc_open(name) : NULL;
  if (voice_debug()) dlog("alcOpenDevice name=%s dev=%p", (name && *name) ? name : "(default)", d);
  return d;
}
void* alcCreateContext(void* dev, const int* attr) {
  if (!real_alc_create) real_alc_create = dlsym(RTLD_NEXT, "alcCreateContext");
  void* c = real_alc_create ? real_alc_create(dev, attr) : NULL;
  if (voice_debug()) dlog("alcCreateContext dev=%p ctx=%p", dev, c);
  return c;
}
int alcMakeContextCurrent(void* ctx) {
  if (!real_alc_make_current) real_alc_make_current = dlsym(RTLD_NEXT, "alcMakeContextCurrent");
  int r = real_alc_make_current ? real_alc_make_current(ctx) : 0;
  if (voice_debug()) dlog("alcMakeContextCurrent ctx=%p -> %d", ctx, r);
  return r;
}
void* alcGetCurrentContext(void) {
  if (!real_alc_get_current) real_alc_get_current = dlsym(RTLD_NEXT, "alcGetCurrentContext");
  return real_alc_get_current ? real_alc_get_current() : NULL;
}
void* alcGetContextsDevice(void* ctx) {
  if (!real_alc_get_device) real_alc_get_device = dlsym(RTLD_NEXT, "alcGetContextsDevice");
  return real_alc_get_device ? real_alc_get_device(ctx) : NULL;
}

static uintptr_t g_base = 0;
static int g_base_found = 0;
static int g_classes_set = 0;

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

typedef struct { int32_t index, number; } GameFName;
typedef void (*FNameCtorFn)(GameFName*, const wchar_t*, int, int);
typedef void* (*GetAudioDeviceFn)(void*);
typedef void (*SetClassVolumeFn)(void*, GameFName, float);

static void set_class_volumes(void) {
  if (g_classes_set) return;
  uintptr_t base = main_base();
  if (!g_base_found) return;
  void* engine = *(void**)(base + 0x2ef2b80);
  if (!engine) return;
  GetAudioDeviceFn get_device = (GetAudioDeviceFn)(base + 0x1995420);
  SetClassVolumeFn set_class = (SetClassVolumeFn)(base + 0x1564920);
  FNameCtorFn fname_ctor = (FNameCtorFn)(base + 0xde5130);
  if (!get_device || !set_class || !fname_ctor) return;
  void* device = get_device(engine);
  if (!device) return;
  const wchar_t* names[] = { L"ConfigBGM", L"ConfigAmbient", L"ConfigSE", L"ConfigVoice" };
  for (int i = 0; i < 4; i++) {
    GameFName fn = { 0, 0 };
    fname_ctor(&fn, names[i], 0, 1);  /* FNAME_Add */
    set_class(device, fn, 1.0f);
    dlog("class volume set %ls fn=%d/%d", names[i], fn.index, fn.number);
  }
  g_classes_set = 1;
  dlog("class volumes done (BGM/Ambient/SE/Voice=1.0) device=%p", device);
  fprintf(stderr, "[audiofx] class volumes set (BGM/Ambient/SE/Voice=1.0)\n");
}

static double now_s(void) { struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts); return ts.tv_sec + ts.tv_nsec / 1e9; }
static int clamp_id(ALuint id) { return id < MAX_ID; }
static unsigned int rng_next(void) {
  unsigned int x = g_rng ? g_rng : 1;
  x ^= x << 13; x ^= x >> 17; x ^= x << 5;
  g_rng = x ? x : 1;
  return g_rng;
}
static double dur(ALuint b) {
  if (!clamp_id(b) || !g_buf_freq[b] || !g_buf_bytes[b]) return 0.0;
  int ch = (g_buf_fmt[b] == AL_FORMAT_STEREO16) ? 2 : 1;
  return (double)g_buf_bytes[b] / (double)(g_buf_freq[b] * ch * 2);
}
static double pcm_dur(ALsizei sz, ALsizei fr, ALenum fmt) {
  int ch = (fmt == AL_FORMAT_STEREO16) ? 2 : 1;
  if (fr <= 0) return 0.0;
  return (double)sz / (double)(fr * ch * 2);
}
static int vc_find(ALuint id) {
  for (int i = 0; i < g_vc_count; i++) if (g_vc[i].id == id) return i;
  return -1;
}
static void vc_pool_remove(int idx) {
  for (int i = 0; i < g_voice_pool_count; i++) {
    if (g_voice_pool[i] == idx) {
      g_voice_pool[i] = g_voice_pool[--g_voice_pool_count];
      return;
    }
  }
}
static void vc_pool_add(int idx) {
  for (int i = 0; i < g_voice_pool_count; i++) if (g_voice_pool[i] == idx) return;
  if (g_voice_pool_count < VOICE_CACHE_MAX) g_voice_pool[g_voice_pool_count++] = idx;
}
static void vc_store(ALuint id, const void* data, ALsizei sz, ALsizei fr, ALenum fmt) {
  if (!data || sz <= 0) return;
  double d = pcm_dur(sz, fr, fmt);
  if (d < 0.6 || d > 12.0) return;
  if ((size_t)sz > VOICE_CACHE_BYTES || g_vc_bytes + (size_t)sz > VOICE_CACHE_BYTES) return;
  int old = vc_find(id);
  if (old >= 0) {
    vc_pool_remove(old);
    g_vc_bytes -= (size_t)g_vc[old].sz;
    free(g_vc[old].pcm);
    g_vc[old] = g_vc[--g_vc_count];
    // the moved entry's pool index changed
    for (int i = 0; i < g_voice_pool_count; i++) if (g_voice_pool[i] == g_vc_count) g_voice_pool[i] = old;
  }
  if (g_vc_count >= VOICE_CACHE_MAX) {
    if (voice_debug()) dlog("voice cache skip buf=%u dur=%.2f (full %d)", id, d, g_vc_count);
    return;
  }
  short* copy = (short*)malloc((size_t)sz);
  if (!copy) return;
  memcpy(copy, data, (size_t)sz);
  g_vc[g_vc_count].id = id;
  g_vc[g_vc_count].pcm = copy;
  g_vc[g_vc_count].sz = sz;
  g_vc[g_vc_count].freq = fr;
  g_vc[g_vc_count].fmt = fmt;
  g_vc[g_vc_count].dur = d;
  g_vc_bytes += (size_t)sz;
  g_vc_count++;
  if (d >= 1.5) {
    int peak = 0;
    const short* samples = (const short*)copy;
    for (size_t i = 0; i < (size_t)sz / 2; i++) {
      int a = samples[i] < 0 ? -samples[i] : samples[i];
      if (a > peak) peak = a;
    }
    if (voice_debug()) dlog("voice cache buf=%u dur=%.2f peak=%d count=%d bytes=%zu",
         id, d, peak, g_vc_count, g_vc_bytes);
  } else {
    if (voice_debug()) dlog("voice cache buf=%u dur=%.2f count=%d bytes=%zu", id, d, g_vc_count, g_vc_bytes);
  }
}
static int vc_find_or_add(ALuint id) {
  int i = vc_find(id);
  if (i >= 0) { vc_pool_add(i); return i; }
  return -1;
}
static int g_recent_pick[8];
static int g_recent_count = 0;
static int recent_has(int idx) {
  for (int i = 0; i < g_recent_count; i++) if (g_recent_pick[i] == idx) return 1;
  return 0;
}
static void recent_add(int idx) {
  if (g_recent_count < 8) g_recent_pick[g_recent_count++] = idx;
  else {
    for (int i = 1; i < 8; i++) g_recent_pick[i-1] = g_recent_pick[i];
    g_recent_pick[7] = idx;
  }
}
static int vc_pick(int current) {
  if (g_voice_pool_count <= 0) return -1;
  int idx = current;
  for (int tries = 0; tries < 24; tries++) {
    int c = g_voice_pool[rng_next() % (unsigned)g_voice_pool_count];
    if (c != current && !recent_has(c)) { idx = c; break; }
  }
  if (idx == current) return -1;
  recent_add(idx);
  return idx;
}
static float voice_pitch_factor(void) {
  float p = g_voice_pitch;
  if (g_voice_pitch_rand > 0.0f)
    p *= 1.0f + ((float)(rng_next() % 2001) / 1000.0f - 1.0f) * g_voice_pitch_rand;
  return p;
}
static int is_dir(const char* p) { struct stat st; return stat(p, &st) == 0 && S_ISDIR(st.st_mode); }
static int file_exists(const char* p) { struct stat st; return p && *p && stat(p, &st) == 0 && S_ISREG(st.st_mode); }
static int list_audio(const char* dir, char files[][1024], int max) {
  DIR* d = opendir(dir); if (!d) return 0; int n = 0; struct dirent* e;
  while ((e = readdir(d)) && n < max) {
    const char* dot = strrchr(e->d_name, '.'); if (!dot) continue;
    if (strcasecmp(dot, ".ogg") && strcasecmp(dot, ".wav") && strcasecmp(dot, ".flac") &&
        strcasecmp(dot, ".mp3") && strcasecmp(dot, ".opus")) continue;
    snprintf(files[n], 1024, "%s/%s", dir, e->d_name); n++;
  }
  closedir(d); return n;
}
static void load_map(void) {
  if (g_map_loaded) return; g_map_loaded = 1;
  if (!g_map_path[0]) return;
  FILE* f = fopen(g_map_path, "r"); if (!f) return;
  char line[1200];
  while (fgets(line, sizeof(line), f)) {
    char* h = strchr(line, '#'); if (h) *h = 0;
    char* eq = strchr(line, '='); if (!eq) continue;
    *eq = 0;
    char* a = line; while (*a && isspace((unsigned char)*a)) a++;
    char* b = eq + 1; while (*b && isspace((unsigned char)*b)) b++;
    char* ae = a + strlen(a); while (ae > a && isspace((unsigned char)ae[-1])) *--ae = 0;
    char* be = b + strlen(b); while (be > b && isspace((unsigned char)be[-1])) *--be = 0;
    if (!*a || !*b || g_map_count >= 128) continue;
    snprintf(g_map[g_map_count].pattern, sizeof(g_map[0].pattern), "%s", a);
    snprintf(g_map[g_map_count].path, sizeof(g_map[0].path), "%s", b);
    g_map_count++;
  }
  fclose(f);
  fprintf(stderr, "[audiofx] stage map: %d entr%s from %s\n", g_map_count, g_map_count == 1 ? "y" : "ies", g_map_path);
}
static void update_stage_ex(double min_interval) {
  double t = now_s(); if (t - g_stage_last < min_interval) return; g_stage_last = t;
  const char* gd = getenv("MN9_GAME_DIR"); if (!gd || !*gd) return;
  char path[1400]; snprintf(path, sizeof(path), "%s/MN9Game/Logs/Launch.log", gd);
  FILE* f = fopen(path, "rb"); if (!f) return;
  fseek(f, 0, SEEK_END); long sz = ftell(f); long start = sz > 32768 ? sz - 32768 : 0;
  fseek(f, start, SEEK_SET);
  char buf[33000]; size_t n = fread(buf, 1, sizeof(buf) - 1, f); fclose(f); buf[n] = 0;
  const char* p = NULL; char* q = buf;
  while ((q = strstr(q, "LoadMap:"))) { p = q; q += 8; }
  if (!p) { q = buf; while ((q = strstr(q, "Server switch level:"))) { p = q; q += 20; } }
  if (!p) return;
  p = strchr(p, ':'); if (!p) return; p++;
  while (*p == ' ') p++;
  char name[128]; int i = 0;
  while (*p && *p != '?' && *p != '\n' && *p != '\r' && *p != ' ' && i < 127) name[i++] = *p++;
  name[i] = 0;
  if (*name && strcmp(name, g_stage)) {
    snprintf(g_stage, sizeof(g_stage), "%s", name);
    fprintf(stderr, "[audiofx] stage=%s\n", g_stage);
  }
}
static void update_stage(void) { update_stage_ex(1.0); }
static const char* mapped_path(void) {
  load_map(); update_stage(); if (!g_stage[0] || !g_map_count) return NULL;
  const char* def = NULL;
  for (int i = 0; i < g_map_count; i++) {
    if (!strcasecmp(g_map[i].pattern, "default")) { def = g_map[i].path; continue; }
    if (strcasestr(g_stage, g_map[i].pattern)) return g_map[i].path;
  }
  return def;
}
static int duck_active(void) { return g_duck && now_s() < g_duck_until; }
static float music_gain_now(void) { return g_music_gain * g_music_game_gain * (duck_active() ? g_duck_factor : 1.0f); }
static void night_process(short* data, size_t count) {
  for (size_t i = 0; i < count; i++) {
    float x = data[i] / 32768.0f;
    float a = fabsf(x);
    const float th = 0.25f;
    float g = a > th ? (th + (a - th) * 0.35f) / a : 1.0f;
    float y = x * g * 1.55f;
    if (y > 1.0f) y = 1.0f; if (y < -1.0f) y = -1.0f;
    data[i] = (short)(y * 32767.0f);
  }
}
static int pick_global(void) {
  if (g_file_count <= 0) return -1;
  if (g_music_mode == 3) { int i = g_next_index % g_file_count; g_next_index = (g_next_index + 1) % g_file_count; return i; }
  if (g_music_mode == 4) {
    if (g_bag_pos >= g_bag_len) {
      g_bag_len = g_file_count; for (int i = 0; i < g_bag_len; i++) g_bag[i] = i;
      for (int i = g_bag_len - 1; i > 0; i--) { int j = (int)(rng_next() % (unsigned)(i + 1)); unsigned t = g_bag[i]; g_bag[i] = g_bag[j]; g_bag[j] = (int)t; }
      if (g_bag_len > 1 && g_bag[0] == g_last_file) { int t = g_bag[0]; g_bag[0] = g_bag[1]; g_bag[1] = t; }
      g_bag_pos = 0;
    }
    return g_bag[g_bag_pos++];
  }
  int pick = (int)(rng_next() % (unsigned)g_file_count);
  if (g_file_count > 1 && pick == g_last_file) pick = (pick + 1) % g_file_count;
  return pick;
}
static float voice_gain_for(float v) {
  float b = v * 3.0f;
  if (b > 1.0f) b = 1.0f;
  if (b < 0.0f) b = 0.0f;
  return b;
}

static float bus_gain(int cat) {
  if (cat == 1) {
    if (g_music_mode == 0) return g_music_gain * (duck_active() ? g_duck_factor : 1.0f);
    if (g_music_mode == 1) return 0.0f;
    // If no replacement track is available, keep the game's own BGM.
    return g_ext_src ? 0.0f : g_music_gain * (duck_active() ? g_duck_factor : 1.0f);
  }
  if (cat == 2) return g_sfx_gain;
  if (cat == 3) {
    if (g_voice_mute) return 0.0f;
    if (g_voice_ext_src && now_s() < g_voice_ext_until) return 0.0f;
    return g_voice_gain;
  }
  return 1.0f;
}
static void apply_gain(ALuint s) {
  if (!clamp_id(s)) return;
  float base = g_src_gain[s] > 0.001f ? g_src_gain[s] : 1.0f;
  if (g_src_cat[s] == 3) base = voice_gain_for(base);
  if (al.sourcef) al.sourcef(s, AL_GAIN, base * bus_gain(g_src_cat[s]));
}
static void refresh_gains(void) {
  if (g_ext_src && al.sourcef) al.sourcef(g_ext_src, AL_GAIN, music_gain_now());
  for (int i = 0; i < g_music_src_count; i++) apply_gain(g_music_srcs[i]);
}
static void stop_voice_external(void);

static void maybe_reload_conf(void) {
  static double last = 0;
  double t = now_s();
  if (t - last < 2.0) return;
  last = t;
  const char* gd = getenv("MN9_GAME_DIR");
  if (!gd || !*gd) return;
  char path[1400];
  snprintf(path, sizeof(path), "%s/.mn9charsel/audio.conf", gd);
  FILE* f = fopen(path, "r");
  if (!f) return;
  float mg = g_music_gain, sg = g_sfx_gain, vg = g_voice_gain, sp = g_sfx_pitch;
  float vp = g_voice_pitch, vpr = g_voice_pitch_rand;
  int duck = g_duck, night = g_night, voice_mute = g_voice_mute, vrand = g_voice_random;
  char line[512];
  while (fgets(line, sizeof(line), f)) {
    char* eq = strchr(line, '=');
    if (!eq) continue;
    *eq = 0;
    char* k = line; while (*k == ' ' || *k == '\t') k++;
    char* v = eq + 1; while (*v == ' ' || *v == '\t') v++;
    char* e = v + strlen(v); while (e > v && (e[-1] == '\n' || e[-1] == '\r' || e[-1] == ' ')) *--e = 0;
    if (!strcmp(k, "music_gain")) mg = (float)atof(v);
    else if (!strcmp(k, "sfx_gain")) sg = (float)atof(v);
    else if (!strcmp(k, "voice_gain")) vg = (float)atof(v);
    else if (!strcmp(k, "sfx_pitch")) sp = (float)atof(v);
    else if (!strcmp(k, "duck")) duck = (*v && *v != '0');
    else if (!strcmp(k, "night")) night = (*v && *v != '0');
    else if (!strcmp(k, "voice")) voice_mute = !strcmp(v, "mute");
    else if (!strcmp(k, "voice_pitch")) vp = (float)atof(v);
    else if (!strcmp(k, "voice_pitch_rand")) vpr = (float)atof(v);
    else if (!strcmp(k, "voice_random")) vrand = (*v && *v != '0');
  }
  fclose(f);
  int changed = fabsf(mg - g_music_gain) > 0.001f ||
                fabsf(sg - g_sfx_gain) > 0.001f ||
                fabsf(vg - g_voice_gain) > 0.001f ||
                fabsf(sp - g_sfx_pitch) > 0.001f ||
                fabsf(vp - g_voice_pitch) > 0.001f ||
                fabsf(vpr - g_voice_pitch_rand) > 0.001f ||
                vrand != g_voice_random ||
                duck != g_duck || night != g_night || voice_mute != g_voice_mute;
  g_music_gain = mg; g_sfx_gain = sg; g_voice_gain = vg; g_sfx_pitch = sp;
  g_voice_pitch = vp; g_voice_pitch_rand = vpr;
  if (g_voice_random && !vrand) { g_voice_ext_until = 0; stop_voice_external(); }
  g_voice_random = vrand;
  g_duck = duck; g_night = night; g_voice_mute = voice_mute;
  if (changed) {
    refresh_gains();
    dlog("config music=%.2f sfx=%.2f voice=%.2f pitch=%.2f rand=%.2f random=%d duck=%d",
         g_music_gain, g_sfx_gain, g_voice_gain, g_voice_pitch,
         g_voice_pitch_rand, g_voice_random, g_duck);
    fprintf(stderr, "[audiofx] live gains music=%.2f sfx=%.2f voice=%.2f duck=%d\n",
            g_music_gain, g_sfx_gain, g_voice_gain, g_duck);
  }
}

static void note_music_gain(ALuint s, float v) {
  if (!clamp_id(s) || g_src_cat[s] != 1) return;
  if (s != g_bgm_src) return;
  if (v < 0.0f) v = 0.0f;
  if (fabsf(v - g_music_game_gain) < 0.001f) return;
  g_music_game_gain = v;
  refresh_gains();
  fprintf(stderr, "[audiofx] music volume %.2f -> external %.2f\n", v, music_gain_now());
}

static void load_sndfile(void) {
  static int tried = 0;
  if (tried) return;
  tried = 1;
  void* lib = dlopen("libsndfile.so.1", RTLD_LAZY | RTLD_LOCAL);
  if (!lib) lib = dlopen("libsndfile.so", RTLD_LAZY | RTLD_LOCAL);
  if (!lib) { fprintf(stderr, "[audiofx] libsndfile not found\n"); return; }
  sf_open_ = dlsym(lib, "sf_open");
  sf_readf_short_ = dlsym(lib, "sf_readf_short");
  sf_close_ = dlsym(lib, "sf_close");
}
static int cmp_name(const void* a, const void* b) { return strcmp((const char*)a, (const char*)b); }
static void scan_dir_recursive(const char* dir, int depth) {
  DIR* d = opendir(dir); if (!d) return;
  struct dirent* e;
  while ((e = readdir(d)) && g_file_count < 512) {
    if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..") || e->d_name[0] == '.') continue;
    char full[1200]; snprintf(full, sizeof(full), "%s/%s", dir, e->d_name);
    struct stat st;
    if (stat(full, &st) != 0) continue;
    if (S_ISDIR(st.st_mode)) { if (depth > 0) scan_dir_recursive(full, depth - 1); continue; }
    const char* dot = strrchr(e->d_name, '.');
    if (!dot) continue;
    if (strcasecmp(dot, ".ogg") && strcasecmp(dot, ".wav") && strcasecmp(dot, ".flac") &&
        strcasecmp(dot, ".mp3") && strcasecmp(dot, ".opus")) continue;
    snprintf(g_files[g_file_count], sizeof(g_files[0]), "%s", full);
    g_file_count++;
  }
  closedir(d);
}
static void scan_music(void) {
  if (g_file_count >= 0) return;
  g_file_count = 0;
  if (!g_music_dir[0]) return;
  scan_dir_recursive(g_music_dir, 3);
  if (g_file_count > 1) qsort(g_files, (size_t)g_file_count, sizeof(g_files[0]), cmp_name);
  fprintf(stderr, "[audiofx] music folder: %d track(s) under %s\n", g_file_count, g_music_dir);
}
static int load_track_sndfile(const char* path, short** out, long long* frames, int* rate, int* ch, int* fmt) {
  load_sndfile();
  if (!sf_open_ || !sf_readf_short_ || !sf_close_) return 0;
  SF_INFO info; memset(&info, 0, sizeof(info));
  SNDFILE* f = sf_open_(path, SFM_READ, &info);
  if (!f) { fprintf(stderr, "[audiofx] cannot open %s\n", path); return 0; }
  if (info.frames <= 0 || info.channels < 1) { sf_close_(f); return 0; }
  short* data = (short*)malloc((size_t)info.frames * info.channels * sizeof(short));
  if (!data) { sf_close_(f); return 0; }
  sf_count_t got = sf_readf_short_(f, data, info.frames);
  sf_close_(f);
  if (got <= 0) { free(data); return 0; }
  int use_ch = info.channels >= 2 ? 2 : 1;
  short* final = data;
  if (info.channels > 2) {
    final = (short*)malloc((size_t)got * 2 * sizeof(short));
    if (!final) { free(data); return 0; }
    for (sf_count_t i = 0; i < got; i++) {
      final[i * 2 + 0] = data[i * info.channels + 0];
      final[i * 2 + 1] = data[i * info.channels + 1];
    }
    free(data);
  }
  *out = final; *frames = got; *rate = info.samplerate; *ch = use_ch;
  *fmt = use_ch == 1 ? AL_FORMAT_MONO16 : AL_FORMAT_STEREO16;
  return 1;
}
/* libmpg123 is present in the Steam Linux Runtime, whose libsndfile 1.0.25
   cannot decode MP3. Use mpg123 for .mp3 and libsndfile for everything else. */
typedef struct mpg123_handle_struct mpg123_handle;
#define MPG123_OK 0
#define MPG123_DONE -12
#define MPG123_NEW_FORMAT -11
#define MPG123_NEED_MORE -10
#define MPG123_ENC_SIGNED_16 0x0D0

static int (*mpg_init)(void);
static mpg123_handle* (*mpg_new)(const char*, int*);
static void (*mpg_delete)(mpg123_handle*);
static int (*mpg_open)(mpg123_handle*, const char*);
static int (*mpg_close)(mpg123_handle*);
static int (*mpg_getformat)(mpg123_handle*, long*, int*, int*);
static int (*mpg_read)(mpg123_handle*, void*, size_t, size_t*);
static int (*mpg_format_none)(mpg123_handle*);
static int (*mpg_format)(mpg123_handle*, long, int, int);
static const char* (*mpg_strerror)(mpg123_handle*);
static int g_mpg_ready = 0, g_mpg_tried = 0;

static void load_mpg123(void) {
  if (g_mpg_tried) return;
  g_mpg_tried = 1;
  void* lib = dlopen("libmpg123.so.0", RTLD_LAZY | RTLD_LOCAL);
  if (!lib) lib = dlopen("libmpg123.so", RTLD_LAZY | RTLD_LOCAL);
  if (!lib) { fprintf(stderr, "[audiofx] libmpg123 not found\n"); return; }
  mpg_init = dlsym(lib, "mpg123_init");
  mpg_new = dlsym(lib, "mpg123_new");
  mpg_delete = dlsym(lib, "mpg123_delete");
  mpg_open = dlsym(lib, "mpg123_open");
  mpg_close = dlsym(lib, "mpg123_close");
  mpg_getformat = dlsym(lib, "mpg123_getformat");
  mpg_read = dlsym(lib, "mpg123_read");
  mpg_format_none = dlsym(lib, "mpg123_format_none");
  mpg_format = dlsym(lib, "mpg123_format");
  mpg_strerror = dlsym(lib, "mpg123_strerror");
  g_mpg_ready = mpg_new && mpg_delete && mpg_open && mpg_close &&
                mpg_getformat && mpg_read;
  if (g_mpg_ready && mpg_init) mpg_init();
  fprintf(stderr, "[audiofx] libmpg123 %s\n", g_mpg_ready ? "ready" : "incomplete");
}

static int is_mp3(const char* path) {
  const char* dot = strrchr(path, '.');
  return dot && !strcasecmp(dot, ".mp3");
}

static int load_track_mp3(const char* path, short** out, long long* frames, int* rate, int* ch, int* fmt) {
  load_mpg123();
  if (!g_mpg_ready) return 0;
  int err = 0;
  mpg123_handle* h = mpg_new(NULL, &err);
  if (!h) return 0;
  if (mpg_open(h, path) != 0) {
    fprintf(stderr, "[audiofx] mp3 open failed: %s (%s)\n", path,
            mpg_strerror ? mpg_strerror(h) : "?");
    mpg_delete(h);
    return 0;
  }
  long r = 0; int channels = 0, enc = 0;
  if (mpg_getformat(h, &r, &channels, &enc) != 0 || r <= 0 || channels < 1) {
    mpg_close(h); mpg_delete(h); return 0;
  }
  if (enc != MPG123_ENC_SIGNED_16) {
    if (!mpg_format_none || !mpg_format ||
        mpg_format_none(h) != 0 ||
        mpg_format(h, r, channels, MPG123_ENC_SIGNED_16) != 0) {
      fprintf(stderr, "[audiofx] mp3 unsupported encoding 0x%X\n", enc);
      mpg_close(h); mpg_delete(h); return 0;
    }
  }
  size_t cap = 1 << 20, used = 0;
  unsigned char* buf = (unsigned char*)malloc(cap);
  if (!buf) { mpg_close(h); mpg_delete(h); return 0; }
  for (;;) {
    if (used + 65536 > cap) {
      size_t nc = cap * 2;
      unsigned char* nb = (unsigned char*)realloc(buf, nc);
      if (!nb) break;
      buf = nb; cap = nc;
    }
    size_t done = 0;
    int rc = mpg_read(h, buf + used, cap - used, &done);
    used += done;
    if (rc == MPG123_DONE) break;
    if (rc == MPG123_NEW_FORMAT) {
      if (mpg_getformat(h, &r, &channels, &enc) != 0) break;
      continue;
    }
    if (rc == MPG123_NEED_MORE) { if (done == 0) break; continue; }
    if (rc != MPG123_OK) {
      fprintf(stderr, "[audiofx] mp3 read error %d\n", rc);
      break;
    }
    if (done == 0) break;
  }
  mpg_close(h); mpg_delete(h);
  if (used < 2) { free(buf); return 0; }
  int use_ch = channels >= 2 ? 2 : 1;
  short* data = (short*)buf;
  if (channels > 2) {
    int nf = (int)(used / (size_t)(channels * 2));
    short* mix = (short*)malloc((size_t)nf * 2 * sizeof(short));
    if (!mix) { free(buf); return 0; }
    for (int i = 0; i < nf; i++) {
      mix[i * 2 + 0] = data[i * channels + 0];
      mix[i * 2 + 1] = data[i * channels + 1];
    }
    free(buf); data = mix; use_ch = 2; used = (size_t)nf * 4; channels = 2;
  }
  *out = data;
  *frames = (long long)(used / (size_t)(channels * 2));
  *rate = (int)r;
  *ch = use_ch;
  *fmt = use_ch == 1 ? AL_FORMAT_MONO16 : AL_FORMAT_STEREO16;
  fprintf(stderr, "[audiofx] mp3 %s (%.1fs %ldHz %dch)\n",
          path, r ? (double)*frames / (double)r : 0.0, r, channels);
  return 1;
}

static int load_track(const char* path, short** out, long long* frames, int* rate, int* ch, int* fmt) {
  if (is_mp3(path) && load_track_mp3(path, out, frames, rate, ch, fmt)) return 1;
  return load_track_sndfile(path, out, frames, rate, ch, fmt);
}

static void stop_external(void) {
  if (g_ext_src) { if (al.stop) al.stop(g_ext_src); if (al.del_sources) al.del_sources(1, &g_ext_src); g_ext_src = 0; }
  if (g_ext_buf) { if (al.del_buffers) al.del_buffers(1, &g_ext_buf); g_ext_buf = 0; }
  refresh_gains();
}
static void resolve_core(void) {
  if (!al.gen_sources) al.gen_sources = dlsym(RTLD_NEXT, "alGenSources");
  if (!al.del_sources) al.del_sources = dlsym(RTLD_NEXT, "alDeleteSources");
  if (!al.play) al.play = dlsym(RTLD_NEXT, "alSourcePlay");
  if (!al.stop) al.stop = dlsym(RTLD_NEXT, "alSourceStop");
  if (!al.bufdata) al.bufdata = dlsym(RTLD_NEXT, "alBufferData");
  if (!al.gen_buffers) al.gen_buffers = dlsym(RTLD_NEXT, "alGenBuffers");
  if (!al.del_buffers) al.del_buffers = dlsym(RTLD_NEXT, "alDeleteBuffers");
  if (!al.sourcei) al.sourcei = dlsym(RTLD_NEXT, "alSourcei");
  if (!al.sourcef) al.sourcef = dlsym(RTLD_NEXT, "alSourcef");
  if (!al.sourcefv) al.sourcefv = dlsym(RTLD_NEXT, "alSourcefv");
  if (!al.getsourcei) al.getsourcei = dlsym(RTLD_NEXT, "alGetSourcei");
  if (!al.getsourcef) al.getsourcef = dlsym(RTLD_NEXT, "alGetSourcef");
  if (!al.getbufferiv) al.getbufferiv = dlsym(RTLD_NEXT, "alGetBufferiv");
  if (!al.geterror) al.geterror = dlsym(RTLD_NEXT, "alGetError");
}
static void start_external(void) {
  if (g_music_mode < 2) return;
  resolve_core();
  update_stage_ex(0.2);
  const char* forced = mapped_path();
  const char* key = g_stage[0] ? g_stage : "default";

  // Same stage/BGM and the assigned track is already playing: leave it alone.
  // The game starts several "music" sources per stage; re-rolling on each one
  // was cutting tracks off mid-song.
  if (g_ext_src && g_cur_valid && !strcmp(key, g_cur_key)) return;

  char path[1024]; path[0] = 0;
  const char* remembered = assign_lookup(key);
  if (remembered && file_exists(remembered)) {
    snprintf(path, sizeof(path), "%s", remembered);
    fprintf(stderr, "[audiofx] keep %s -> %s\n", key, path);
  } else {
    if (forced && *forced) {
      if (is_dir(forced)) {
        char sf[128][1024]; int n = list_audio(forced, sf, 128);
        if (n <= 0) return;
        int k = (int)(rng_next() % (unsigned)n); snprintf(path, sizeof(path), "%s", sf[k]);
      } else {
        snprintf(path, sizeof(path), "%s", forced);
      }
    } else {
      if (g_music_mode == 1) return;
      scan_music();
      int pick = pick_global(); if (pick < 0) return;
      g_last_file = pick; snprintf(path, sizeof(path), "%s", g_files[pick]);
    }
    assign_store(key, path);
    fprintf(stderr, "[audiofx] assign %s -> %s\n", key, path);
  }
  snprintf(g_cur_path, sizeof(g_cur_path), "%s", path);
  snprintf(g_cur_key, sizeof(g_cur_key), "%s", key);
  g_cur_valid = 1;
  short* pcm = NULL; long long frames = 0; int rate = 0, ch = 0, fmt = 0;
  if (!load_track(path, &pcm, &frames, &rate, &ch, &fmt)) {
    assign_forget(key);
    return;
  }
  if (g_night) night_process(pcm, (size_t)frames * ch);
  stop_external();
  if (!al.gen_buffers || !al.gen_sources || !al.bufdata || !al.sourcei) { free(pcm); return; }
  al.gen_buffers(1, &g_ext_buf);
  al.bufdata(g_ext_buf, fmt, pcm, (ALsizei)(frames * ch * 2), rate);
  al.gen_sources(1, &g_ext_src);
  al.sourcei(g_ext_src, AL_BUFFER, (ALint)g_ext_buf);
  al.sourcei(g_ext_src, AL_SOURCE_RELATIVE, AL_TRUE);
  if (al.sourcefv) { ALfloat p[3] = {0,0,0}; al.sourcefv(g_ext_src, AL_POSITION, p); }
  al.sourcei(g_ext_src, AL_LOOPING, 1);
  if (al.sourcef) al.sourcef(g_ext_src, AL_GAIN, music_gain_now());
  al.play(g_ext_src);
  const char* base = strrchr(path, '/'); base = base ? base + 1 : path;
  fprintf(stderr, "[audiofx] music -> %s%s%s (%.1fs %dHz %dch)\n", g_stage[0] ? g_stage : "-", g_stage[0] ? " / " : "", base, (double)frames / rate, rate, ch);
  free(pcm);
}
static void stop_voice_external(void) {
  ALuint gs = g_voice_game_src;
  double until = g_voice_ext_until;
  if (g_voice_ext_src) {
    if (al.stop) al.stop(g_voice_ext_src);
    if (al.del_sources) al.del_sources(1, &g_voice_ext_src);
    g_voice_ext_src = 0;
  }
  if (g_voice_ext_buf) {
    if (al.del_buffers) al.del_buffers(1, &g_voice_ext_buf);
    g_voice_ext_buf = 0;
  }
  g_voice_game_src = 0;
  g_voice_ext_until = 0;
  if (gs && now_s() >= until) apply_gain(gs);
}

static void start_voice_external(int idx, ALuint game_src, double orig_dur) {
  if (idx < 0 || idx >= g_vc_count || g_voice_mute) return;
  stop_voice_external();
  VoiceClip* c = &g_vc[idx];
  if (!al.gen_buffers || !al.gen_sources || !al.bufdata || !al.sourcei) return;
  al.gen_buffers(1, &g_voice_ext_buf);
  al.bufdata(g_voice_ext_buf, c->fmt, c->pcm, c->sz, c->freq);
  al.gen_sources(1, &g_voice_ext_src);
  al.sourcei(g_voice_ext_src, AL_BUFFER, (ALint)g_voice_ext_buf);
  al.sourcei(g_voice_ext_src, AL_SOURCE_RELATIVE, AL_TRUE);
  if (al.sourcefv) { ALfloat p[3] = {0,0,0}; al.sourcefv(g_voice_ext_src, AL_POSITION, p); }
  al.sourcei(g_voice_ext_src, AL_LOOPING, 0);
  if (al.sourcef) {
    al.sourcef(g_voice_ext_src, AL_GAIN, g_voice_gain);
    al.sourcef(g_voice_ext_src, AL_PITCH, g_voice_line_pitch);
  }
  al.play(g_voice_ext_src);
  ALint state = 0;
  if (al.getsourcei) al.getsourcei(g_voice_ext_src, AL_SOURCE_STATE, &state);
  if (state != AL_PLAYING) { stop_voice_external(); return; }
  g_voice_game_src = game_src;
  g_voice_ext_until = now_s() + (orig_dur > 0.0 ? orig_dur : c->dur);
  apply_gain(game_src);  /* bus_gain(3) now returns 0 while this source plays */
  dlog("voice external src=%u clip=%.2f line=%.2f gain=%.2f pitch=%.2f",
       game_src, c->dur, orig_dur, g_voice_gain, g_voice_line_pitch);
  if (g_voice_log++ < 16)
    fprintf(stderr, "[audiofx] dialog -> shim %.1fs clip (line %.1fs)\n", c->dur, orig_dur);
}

static void voice_tick(void) {
  if (g_voice_ext_src && now_s() > g_voice_ext_until) stop_voice_external();
}

static void dump_voice_clip(int idx, ALuint buf) {
  if (!voice_debug()) return;
  if (idx < 0 || idx >= g_vc_count || g_voice_dump_count >= 8) return;
  const char* gd = getenv("MN9_GAME_DIR");
  if (!gd || !*gd) return;
  char path[1400];
  snprintf(path, sizeof(path), "%s/.mn9charsel/voice-dump-%u.wav", gd, buf);
  FILE* f = fopen(path, "wb");
  if (!f) return;
  VoiceClip* c = &g_vc[idx];
  int ch = (c->fmt == AL_FORMAT_STEREO16) ? 2 : 1;
  unsigned int bytes = (unsigned int)c->sz;
  unsigned int riff = 36 + bytes;
  unsigned int fmt = 16;
  unsigned short pcm = 1, channels = (unsigned short)ch, align = (unsigned short)(ch * 2), bits = 16;
  unsigned int rate = (unsigned int)c->freq, byte_rate = rate * align;
  fwrite("RIFF", 1, 4, f); fwrite(&riff, 4, 1, f); fwrite("WAVEfmt ", 1, 8, f);
  fwrite(&fmt, 4, 1, f); fwrite(&pcm, 2, 1, f); fwrite(&channels, 2, 1, f);
  fwrite(&rate, 4, 1, f); fwrite(&byte_rate, 4, 1, f); fwrite(&align, 2, 1, f); fwrite(&bits, 2, 1, f);
  fwrite("data", 1, 4, f); fwrite(&bytes, 4, 1, f); fwrite(c->pcm, 1, bytes, f);
  fclose(f);
  g_voice_dump_count++;
  dlog("voice dump buf=%u clip=%.2f path=%s", buf, c->dur, path);
}

static void classify_and_start(ALuint s) {
  if (!clamp_id(s)) return;
  resolve_core();
  ALint buf = 0;
  if (al.getsourcei) al.getsourcei(s, AL_BUFFER, &buf);
  double d = dur((ALuint)buf);
  int cat = 0;
  // Only long/streamed sources are BGM.  NPC dialogue often runs 5-11 s;
  // treating it as music made it get muted with the game's BGM.
  if (d >= 12.0 || (d >= 6.0 && g_src_queue[s] >= 4)) cat = 1;
  else if (d > 0.0 && d <= 0.45) cat = 2;
  else if (d > 0.45) cat = 3;
  g_src_cat[s] = cat;
  g_src_pitch[s] = 1.0f;
  if (cat == 3) {
    if (al.sourcei) {
      al.sourcei(s, AL_SOURCE_RELATIVE, AL_TRUE);
      al.sourcei(s, AL_DIRECT_FILTER, 0);
    }
    if (al.sourcefv) { ALfloat pos[3] = {0,0,0}; al.sourcefv(s, AL_POSITION, pos); }
    if (al.sourcef) {
      al.sourcef(s, AL_ROLLOFF_FACTOR, 0.0f);
      al.sourcef(s, AL_MIN_GAIN, 0.0f);
      al.sourcef(s, AL_MAX_GAIN, 1.0f);
      al.sourcef(s, AL_CONE_OUTER_GAIN, 1.0f);
      al.sourcef(s, AL_CONE_INNER_ANGLE, 360.0f);
      al.sourcef(s, AL_CONE_OUTER_ANGLE, 360.0f);
    }
  }
  if (cat == 1) {
    if (g_bgm_src == 0 || d > g_bgm_dur || s == g_bgm_src) {
      if (s != g_bgm_src) { g_bgm_src = s; g_bgm_dur = d; }
      if (g_src_gain[s] > 0.001f) note_music_gain(s, g_src_gain[s]);
    }
  }
  if (cat == 3 && d >= 0.45) {
    double now = now_s();
    if (now - g_voice_line_last > 0.7)
      g_voice_line_pitch = voice_pitch_factor();
    g_voice_line_last = now;
    float p = g_voice_line_pitch;
    g_src_pitch[s] = p;
    if (al.sourcef) al.sourcef(s, AL_PITCH, p);
    dlog("voice start src=%u dur=%.2f gain=%.3f bus=%.3f ext=%u pitch=%.3f",
         s, d, g_src_gain[s], bus_gain(3), g_voice_ext_src, p);
    {
      int vi = vc_find((ALuint)buf);
      if (vi >= 0) dump_voice_clip(vi, (ALuint)buf);
      ALint bsz = 0, bfreq = 0, bbits = 0, bch = 0;
      if (al.getbufferiv) {
        al.getbufferiv((ALuint)buf, AL_SIZE, &bsz);
        al.getbufferiv((ALuint)buf, AL_FREQUENCY, &bfreq);
        al.getbufferiv((ALuint)buf, AL_BITS, &bbits);
        al.getbufferiv((ALuint)buf, AL_CHANNELS, &bch);
      }
      if (voice_debug()) dlog("voice buffer buf=%u cached=%d size=%d freq=%d bits=%d ch=%d fmt=0x%x",
           buf, vi, bsz, bfreq, bbits, bch, g_buf_fmt[(ALuint)buf]);
    }
    if (g_voice_log++ < 16)
      fprintf(stderr, "[audiofx] dialog pitch src=%u %.2f\n", s, p);
    if (!g_voice_mute && g_voice_random && d >= 2.0) {
      if (!g_voice_ext_src || now_s() >= g_voice_ext_until) {
        int vi = vc_find_or_add((ALuint)buf);
        int pick = vi >= 0 ? vc_pick(vi) : -1;
        if (pick >= 0) start_voice_external(pick, s, d);
        else stop_voice_external();
      }
    } else if (!g_voice_ext_src || now_s() >= g_voice_ext_until) {
      stop_voice_external();
    }
  }
  if (g_play_log++ < 24) fprintf(stderr, "[audiofx] play pid=%d src=%u dur=%.2f q=%d cat=%d\n", (int)getpid(), s, d, g_src_queue[s], cat);
  if (cat == 1) {
    if (!g_src_playing_music[s]) {
      g_src_playing_music[s] = 1; g_music_playing++;
      if (g_music_src_count < 64) g_music_srcs[g_music_src_count++] = s;
    }
    double t = now_s();
    if (t - g_last_music_start > 0.8) { g_last_music_start = t; start_external(); }
  }
  if (cat == 3 && d > 0.4 && !g_voice_mute) g_duck_until = now_s() + d + 0.25;
  if (cat == 2 && g_sfx_pitch > 0.0f && al.sourcef) {
    float p = 1.0f + ((float)(rng_next() % 2001) / 1000.0f - 1.0f) * g_sfx_pitch;
    g_src_pitch[s] = p;
    al.sourcef(s, AL_PITCH, p);
    if (g_sfx_pitch_log++ < 16)
      fprintf(stderr, "[audiofx] sfx pitch src=%u %.2f\n", s, p);
  }
  apply_gain(s);
}
static void on_stop(ALuint s) {
  if (!clamp_id(s)) return;
  if (g_src_cat[s] == 3) dlog("voice stop src=%u ext=%u", s, g_voice_ext_src);
  if (s == g_voice_game_src) stop_voice_external();
  if (s == g_bgm_src) { g_bgm_src = 0; g_bgm_dur = 0.0; }
  if (g_src_playing_music[s]) {
    g_src_playing_music[s] = 0; if (g_music_playing > 0) g_music_playing--;
    for (int i = 0; i < g_music_src_count; i++) if (g_music_srcs[i] == s) { g_music_srcs[i] = g_music_srcs[--g_music_src_count]; break; }
  }
  if (g_music_playing == 0 && g_music_mode == 2) stop_external();
}

// --- interposition ---------------------------------------------------------
void alSourcePlay(ALuint s) {
  set_class_volumes();
  maybe_reload_conf();
  if (!g_voice_random && g_voice_ext_src) stop_voice_external();
  voice_tick();
  if (!al.play) al.play = dlsym(RTLD_NEXT, "alSourcePlay");
  if (!al.getsourcei) al.getsourcei = dlsym(RTLD_NEXT, "alGetSourcei");
  if (!al.sourcef) al.sourcef = dlsym(RTLD_NEXT, "alSourcef");
  classify_and_start(s);
  if (al.play) al.play(s);
  if (voice_debug() && g_src_cat[s] == 3 && g_voice_state_log < 12) {
    g_voice_state_log++;
    ALint st = 0, rel = 0; ALfloat g = -1, pit = -1, mx = -1, mn = -1, ro = -1;
    if (al.getsourcei) al.getsourcei(s, AL_SOURCE_STATE, &st);
    if (al.getsourcei) al.getsourcei(s, AL_SOURCE_RELATIVE, &rel);
    if (al.getsourcef) {
      al.getsourcef(s, AL_GAIN, &g); al.getsourcef(s, AL_PITCH, &pit);
      al.getsourcef(s, AL_MAX_GAIN, &mx); al.getsourcef(s, AL_MIN_GAIN, &mn);
      al.getsourcef(s, AL_ROLLOFF_FACTOR, &ro);
    }
    ALenum err = al.geterror ? al.geterror() : 0;
    dlog("voice state src=%u state=%d rel=%d gain=%.3f pitch=%.3f min=%.3f max=%.3f rolloff=%.3f err=0x%x",
         s, st, rel, g, pit, mn, mx, ro, err);
  }
}
void alSourcePlayv(ALsizei n, const ALuint* s) {
  set_class_volumes();
  maybe_reload_conf();
  if (!g_voice_random && g_voice_ext_src) stop_voice_external();
  voice_tick();
  if (!al.play) al.play = dlsym(RTLD_NEXT, "alSourcePlay");
  if (!al.getsourcei) al.getsourcei = dlsym(RTLD_NEXT, "alGetSourcei");
  if (!al.sourcef) al.sourcef = dlsym(RTLD_NEXT, "alSourcef");
  for (ALsizei i = 0; i < n; i++) { classify_and_start(s[i]); if (al.play) al.play(s[i]); }
}
void alSourceStop(ALuint s) {
  if (!al.stop) al.stop = dlsym(RTLD_NEXT, "alSourceStop");
  on_stop(s);
  if (al.stop) al.stop(s);
}
void alSourceStopv(ALsizei n, const ALuint* s) {
  if (!al.stop) al.stop = dlsym(RTLD_NEXT, "alSourceStop");
  for (ALsizei i = 0; i < n; i++) { on_stop(s[i]); if (al.stop) al.stop(s[i]); }
}
void alSourceQueueBuffers(ALuint s, ALsizei n, const ALuint* b) {
  if (!al.queue) al.queue = dlsym(RTLD_NEXT, "alSourceQueueBuffers");
  if (clamp_id(s)) g_src_queue[s] += n;
  refresh_gains();
  if (al.queue) al.queue(s, n, b);
}
void alSourceUnqueueBuffers(ALuint s, ALsizei n, ALuint* b) {
  if (!al.unqueue) al.unqueue = dlsym(RTLD_NEXT, "alSourceUnqueueBuffers");
  if (clamp_id(s)) { g_src_queue[s] -= n; if (g_src_queue[s] < 0) g_src_queue[s] = 0; }
  if (al.unqueue) al.unqueue(s, n, b);
}
void alBufferData(ALuint b, ALenum f, const void* d, ALsizei sz, ALsizei fr) {
  if (!al.bufdata) al.bufdata = dlsym(RTLD_NEXT, "alBufferData");
  if (clamp_id(b)) { g_buf_bytes[b] = sz; g_buf_freq[b] = fr; g_buf_fmt[b] = f; }
  if (!al.bufdata) return;
  if (sz > 1 && d && (f == AL_FORMAT_MONO16 || f == AL_FORMAT_STEREO16))
    vc_store(b, d, sz, fr, f);
  if (g_night && sz > 1 && (f == AL_FORMAT_MONO16 || f == AL_FORMAT_STEREO16)) {
    short* copy = (short*)malloc((size_t)sz);
    if (copy) {
      memcpy(copy, d, (size_t)sz);
      night_process(copy, (size_t)sz / 2);
      al.bufdata(b, f, copy, sz, fr);
      free(copy);
      return;
    }
  }
  al.bufdata(b, f, d, sz, fr);
}
void alSourcei(ALuint s, ALenum p, ALint v) {
  if (!al.sourcei) al.sourcei = dlsym(RTLD_NEXT, "alSourcei");
  if (clamp_id(s) && g_src_cat[s] == 3) {
    if (p == AL_SOURCE_RELATIVE) v = AL_TRUE;
    if (p == AL_DIRECT_FILTER) v = 0;
    if (p == AL_AUXILIARY_SEND_FILTER) v = 0;
  }
  if (al.sourcei) al.sourcei(s, p, v);
}
void alListenerfv(ALenum p, const ALfloat* v) {
  static void (*real)(ALenum, const ALfloat*) = NULL;
  if (!real) real = (void (*)(ALenum, const ALfloat*))dlsym(RTLD_NEXT, "alListenerfv");
  if (p == AL_GAIN && v) {
    if (fabsf(v[0] - g_listener_gain_last) > 0.01f)
      dlog("listener gain %.3f -> %.3f", g_listener_gain_last, v[0]);
    g_listener_gain_last = v[0];
    fprintf(stderr, "[audiofx] listener gain %.2f\n", v[0]);
  }
  if (real) real(p, v);
}
void alSourcefv(ALuint s, ALenum p, const ALfloat* v) {
  if (!al.sourcefv) al.sourcefv = dlsym(RTLD_NEXT, "alSourcefv");
  if (!al.sourcefv) return;
  if (p == AL_GAIN && v && clamp_id(s)) {
    float g = v[0];
    g_src_gain[s] = g;
    note_music_gain(s, g);
    float base = g;
    if (g_src_cat[s] == 3 && base < 0.001f && g_voice_gain > 0.001f) base = 1.0f;
    float scaled = base * bus_gain(g_src_cat[s]);
    al.sourcefv(s, p, &scaled);
    return;
  }
  if (p == AL_PITCH && v && clamp_id(s)) {
    float mul = g_src_pitch[s] > 0.0f ? g_src_pitch[s] : 1.0f;
    if (g_src_cat[s] == 3 && fabsf(v[0] - g_src_pitch_game[s]) > 0.01f)
      dlog("voice game pitchv src=%u %.3f -> %.3f (x%.3f)", s, g_src_pitch_game[s], v[0], mul);
    g_src_pitch_game[s] = v[0];
    float scaled = v[0] * mul;
    al.sourcefv(s, p, &scaled);
    return;
  }
  al.sourcefv(s, p, v);
}
void alSourcef(ALuint s, ALenum p, ALfloat v) {
  if (!al.sourcef) al.sourcef = dlsym(RTLD_NEXT, "alSourcef");
  if (!al.sourcef) return;
  if (clamp_id(s) && g_src_cat[s] == 3) {
    if (p == AL_MAX_GAIN) { al.sourcef(s, p, 1.0f); return; }
    if (p == AL_MIN_GAIN) { al.sourcef(s, p, 0.0f); return; }
    if (p == AL_ROLLOFF_FACTOR) { al.sourcef(s, p, 0.0f); return; }
    if (p == AL_CONE_OUTER_GAIN) { al.sourcef(s, p, 1.0f); return; }
  }
  if (p == AL_GAIN && clamp_id(s)) {
    float old = g_src_gain[s];
    g_src_gain[s] = v;
    note_music_gain(s, v);
    float base = v;
    if (g_src_cat[s] == 3) base = voice_gain_for(v);
    if (g_src_cat[s] == 3 && fabsf(v - old) > 0.01f)
      if (voice_debug()) dlog("voice gain src=%u %.3f -> %.3f (base %.3f bus %.3f ext %u)",
           s, old, v, base, bus_gain(3), g_voice_ext_src);
    al.sourcef(s, p, base * bus_gain(g_src_cat[s]));
    return;
  }
  if (p == AL_POSITION && clamp_id(s) && g_src_cat[s] == 3) {
    static const ALfloat zero[3] = {0,0,0};
    al.sourcefv(s, p, zero);
    return;
  }
  if (p == AL_PITCH && clamp_id(s)) {
    float mul = g_src_pitch[s] > 0.0f ? g_src_pitch[s] : 1.0f;
    if (g_src_cat[s] == 3) {
      g_src_pitch_game[s] = v;
      al.sourcef(s, p, mul);
      return;
    }
    al.sourcef(s, p, v * mul);
    return;
  }
  al.sourcef(s, p, v);
}

__attribute__((constructor)) static void audiofx_init(void) {
  const char* m = getenv("MN9_AUDIO_MUSIC");
  if (m && !strcmp(m, "mute")) g_music_mode = 1;
  else if (m && !strcmp(m, "random")) g_music_mode = 2;
  else if (m && !strcmp(m, "playlist")) g_music_mode = 3;
  else if (m && !strcmp(m, "shuffle")) g_music_mode = 4;
  const char* d = getenv("MN9_AUDIO_MUSIC_DIR");
  if (d) snprintf(g_music_dir, sizeof(g_music_dir), "%s", d);
  if (const char* v = getenv("MN9_AUDIO_SFX_PITCH")) g_sfx_pitch = (float)atof(v);
  if (const char* v = getenv("MN9_AUDIO_VOICE")) g_voice_mute = !strcmp(v, "mute");
  if (const char* v = getenv("MN9_AUDIO_VOICE_PITCH")) g_voice_pitch = (float)atof(v);
  if (const char* v = getenv("MN9_AUDIO_VOICE_PITCH_RAND")) g_voice_pitch_rand = (float)atof(v);
  if (g_voice_pitch < 0.5f || g_voice_pitch > 2.0f) g_voice_pitch = 1.0f;
  if (g_voice_pitch_rand < 0.0f || g_voice_pitch_rand > 0.5f) g_voice_pitch_rand = 0.0f;
  if (const char* v = getenv("MN9_AUDIO_VOICE_RANDOM")) g_voice_random = (*v && *v != '0');
  if (const char* v = getenv("MN9_AUDIO_MUSIC_MAP")) snprintf(g_map_path, sizeof(g_map_path), "%s", v);
  if (const char* v = getenv("MN9_AUDIO_DUCK")) g_duck = (*v && *v != '0');
  if (const char* v = getenv("MN9_AUDIO_NIGHT")) g_night = (*v && *v != '0');
  if (const char* v = getenv("MN9_AUDIO_MUSIC_GAIN")) g_music_gain = (float)atof(v);
  if (const char* v = getenv("MN9_AUDIO_SFX_GAIN")) g_sfx_gain = (float)atof(v);
  if (const char* v = getenv("MN9_AUDIO_VOICE_GAIN")) g_voice_gain = (float)atof(v);
  g_rng = (unsigned int)(time(NULL) ^ getpid() ^ (unsigned long)&g_rng);
  if (!g_rng) g_rng = 1;
  if (g_music_mode >= 2) scan_music();
  if (g_music_mode >= 2 && g_map_path[0]) load_map();
  dlog("init music=%s voice=%s voice_pitch=%.3f rand=%.3f random=%d voice_gain=%.2f voice_mute=%d",
       m ? m : "game", g_voice_mute ? "mute" : "game", g_voice_pitch,
       g_voice_pitch_rand, g_voice_random, g_voice_gain, g_voice_mute);
  fprintf(stderr, "[audiofx] init pid=%d music=%s sfx_pitch=%.2f voice=%s duck=%d night=%d gains=%.2f/%.2f/%.2f\n", (int)getpid(),
          g_music_mode == 0 ? "game" : g_music_mode == 1 ? "mute" : g_music_mode == 3 ? "playlist" : g_music_mode == 4 ? "shuffle" : "random",
          g_sfx_pitch, g_voice_mute ? "mute" : "game", g_duck, g_night, g_music_gain, g_sfx_gain, g_voice_gain);
}
