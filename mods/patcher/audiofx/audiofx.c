// Mighty No. 9 PC audio shim.
// Hooks OpenAL and classifies sources by buffer length:
//   long (>=5s, or >=1.5s with repeated queueing) = music
//   <=0.45s = SFX, 0.45..5s = voice
// It can mute music, replace it with random files from a folder, randomize
// SFX pitch, mute voice, and apply per-bus gain.
#define _GNU_SOURCE
#include <dlfcn.h>
#include <dirent.h>
#include <stdio.h>
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
  void (*getsourcei)(ALuint, ALenum, ALint*);
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

static int g_music_mode = 0;       // 0 game, 1 mute, 2 random
static int g_voice_mute = 0;
static float g_sfx_pitch = 0.0f;
static float g_music_gain = 1.0f, g_sfx_gain = 1.0f, g_voice_gain = 1.0f;
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
static float music_gain_now(void) { return g_music_gain * (duck_active() ? g_duck_factor : 1.0f); }
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
static float bus_gain(int cat) {
  if (cat == 1) {
    if (g_music_mode == 0) return g_music_gain * (duck_active() ? g_duck_factor : 1.0f);
    if (g_music_mode == 1) return 0.0f;
    // If no replacement track is available, keep the game's own BGM.
    return g_ext_src ? 0.0f : g_music_gain * (duck_active() ? g_duck_factor : 1.0f);
  }
  if (cat == 2) return g_sfx_gain;
  if (cat == 3) return g_voice_mute ? 0.0f : g_voice_gain;
  return 1.0f;
}
static void apply_gain(ALuint s) {
  if (!clamp_id(s)) return;
  float base = g_src_gain[s] > 0.001f ? g_src_gain[s] : 1.0f;
  if (al.sourcef) al.sourcef(s, AL_GAIN, base * bus_gain(g_src_cat[s]));
}
static void refresh_gains(void) {
  if (g_ext_src && al.sourcef) al.sourcef(g_ext_src, AL_GAIN, music_gain_now());
  for (int i = 0; i < g_music_src_count; i++) apply_gain(g_music_srcs[i]);
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
static int load_track(const char* path, short** out, long long* frames, int* rate, int* ch, int* fmt) {
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
  if (!al.getsourcei) al.getsourcei = dlsym(RTLD_NEXT, "alGetSourcei");
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
  if (!load_track(path, &pcm, &frames, &rate, &ch, &fmt)) return;
  if (g_night) night_process(pcm, (size_t)frames * ch);
  stop_external();
  if (!al.gen_buffers || !al.gen_sources || !al.bufdata || !al.sourcei) { free(pcm); return; }
  al.gen_buffers(1, &g_ext_buf);
  al.bufdata(g_ext_buf, fmt, pcm, (ALsizei)(frames * ch * 2), rate);
  al.gen_sources(1, &g_ext_src);
  al.sourcei(g_ext_src, AL_BUFFER, (ALint)g_ext_buf);
  al.sourcei(g_ext_src, AL_LOOPING, 1);
  if (al.sourcef) al.sourcef(g_ext_src, AL_GAIN, music_gain_now());
  al.play(g_ext_src);
  const char* base = strrchr(path, '/'); base = base ? base + 1 : path;
  fprintf(stderr, "[audiofx] music -> %s%s%s (%.1fs %dHz %dch)\n", g_stage[0] ? g_stage : "-", g_stage[0] ? " / " : "", base, (double)frames / rate, rate, ch);
  free(pcm);
}
static void classify_and_start(ALuint s) {
  if (!clamp_id(s)) return;
  resolve_core();
  ALint buf = 0;
  if (al.getsourcei) al.getsourcei(s, AL_BUFFER, &buf);
  double d = dur((ALuint)buf);
  int cat = 0;
  if (d >= 5.0 || (d >= 1.5 && g_src_queue[s] >= 2)) cat = 1;
  else if (d > 0.0 && d <= 0.45) cat = 2;
  else if (d > 0.45) cat = 3;
  g_src_cat[s] = cat;
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
    al.sourcef(s, AL_PITCH, p);
  }
  apply_gain(s);
}
static void on_stop(ALuint s) {
  if (!clamp_id(s)) return;
  if (g_src_playing_music[s]) {
    g_src_playing_music[s] = 0; if (g_music_playing > 0) g_music_playing--;
    for (int i = 0; i < g_music_src_count; i++) if (g_music_srcs[i] == s) { g_music_srcs[i] = g_music_srcs[--g_music_src_count]; break; }
  }
  if (g_music_playing == 0 && g_music_mode == 2) stop_external();
}

// --- interposition ---------------------------------------------------------
void alSourcePlay(ALuint s) {
  if (!al.play) al.play = dlsym(RTLD_NEXT, "alSourcePlay");
  if (!al.getsourcei) al.getsourcei = dlsym(RTLD_NEXT, "alGetSourcei");
  if (!al.sourcef) al.sourcef = dlsym(RTLD_NEXT, "alSourcef");
  classify_and_start(s);
  if (al.play) al.play(s);
}
void alSourcePlayv(ALsizei n, const ALuint* s) {
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
  if (al.sourcei) al.sourcei(s, p, v);
}
void alSourcef(ALuint s, ALenum p, ALfloat v) {
  if (!al.sourcef) al.sourcef = dlsym(RTLD_NEXT, "alSourcef");
  if (!al.sourcef) return;
  if (p == AL_GAIN && clamp_id(s)) {
    g_src_gain[s] = v;
    al.sourcef(s, p, v * bus_gain(g_src_cat[s]));
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
  fprintf(stderr, "[audiofx] init pid=%d music=%s sfx_pitch=%.2f voice=%s duck=%d night=%d gains=%.2f/%.2f/%.2f\n", (int)getpid(),
          g_music_mode == 0 ? "game" : g_music_mode == 1 ? "mute" : g_music_mode == 3 ? "playlist" : g_music_mode == 4 ? "shuffle" : "random",
          g_sfx_pitch, g_voice_mute ? "mute" : "game", g_duck, g_night, g_music_gain, g_sfx_gain, g_voice_gain);
}
