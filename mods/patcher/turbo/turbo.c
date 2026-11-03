/*
 * MN9 turbo (fast-forward) shim.
 *
 * The retail PC build has the dev `slomo` bindings in DefaultInput.ini, but the
 * console command itself is stripped from the executable, so binding a key to
 * `slomo` does nothing. Instead we scale AWorldInfo::TimeDilation, which is the
 * exact float UWorld::Tick multiplies DeltaSeconds by:
 *
 *   UWorld::Tick      0x13785c0
 *   WorldInfo         [UWorld + 0x188]
 *   TimeDilation      [AWorldInfo + 0x488]
 *   GWorld            base + 0x2ec3c08
 *
 * We use SDL_GetKeyboardState as a once-per-frame callback (FSDL2Viewport::
 * ProcessInput calls it every frame), so the write always happens on the game
 * thread just before UWorld::Tick reads it. No threads, no code patching.
 *
 * Env (set by mn9-charselect.sh from .mn9charsel/turbo.conf):
 *   MN9_TURBO_ENABLED  1/0
 *   MN9_TURBO_MODE     hold|toggle
 *   MN9_TURBO_SPEED    float, e.g. 2.0
 *   MN9_TURBO_KEY      SDL scancode name, e.g. F11
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

/* SDL scancodes we need (SDL_scancode.h values). */
enum {
    SC_A = 4, SC_Z = 29,
    SC_1 = 30, SC_0 = 39,
    SC_TAB = 43, SC_SPACE = 44,
    SC_GRAVE = 53,
    SC_CAPSLOCK = 57,
    SC_F1 = 58, SC_F12 = 69,
    SC_F13 = 104,
    SC_INSERT = 73, SC_HOME = 74, SC_PAGEUP = 75,
    SC_DELETE = 76, SC_END = 77, SC_PAGEDOWN = 78,
    SC_RIGHT = 79, SC_LEFT = 80, SC_DOWN = 81, SC_UP = 82,
    SC_NUMLOCK = 83, SC_KP_DIVIDE = 84, SC_KP_MULTIPLY = 85,
    SC_KP_MINUS = 86, SC_KP_PLUS = 87, SC_KP_ENTER = 88,
    SC_KP_1 = 89, SC_KP_0 = 98, SC_KP_PERIOD = 99,
};

#define OFF_GWORLD        0x2ec3c08u
#define OFF_GETWORLDINFO  0x115dea0u   /* UWorld::GetWorldInfo(unsigned int) const */
#define OFF_TIMEDILATION  0x488u       /* AWorldInfo::TimeDilation */
#define MAX_SCANCODE      512

static uintptr_t g_base = 0;
static int g_base_found = 0;
static int g_inited = 0;
static int g_enabled = 0;
static int g_mode_toggle = 0;
static int g_scancode = 68;             /* F11 */
static float g_speed = 2.0f;
static int g_on = 0;
static int g_prev_key = 0;
static int g_applied = 0;
static float g_saved = 1.0f;
static int g_bad = 0;
static int g_debug = 0;
static int g_dbg_td_fail = 0;

static int name_to_scancode(const char* name)
{
    if (!name || !*name) return 68;
    if (name[0] && !name[1]) {
        char c = name[0];
        if (c >= 'a' && c <= 'z') c -= 32;
        if (c >= 'A' && c <= 'Z') return SC_A + (c - 'A');
        if (c >= '0' && c <= '9') return (c == '0') ? SC_0 : (SC_1 + (c - '1'));
    }
    if (!strcasecmp(name, "F1"))  return SC_F1;
    if (!strcasecmp(name, "F2"))  return SC_F1 + 1;
    if (!strcasecmp(name, "F3"))  return SC_F1 + 2;
    if (!strcasecmp(name, "F4"))  return SC_F1 + 3;
    if (!strcasecmp(name, "F5"))  return SC_F1 + 4;
    if (!strcasecmp(name, "F6"))  return SC_F1 + 5;
    if (!strcasecmp(name, "F7"))  return SC_F1 + 6;
    if (!strcasecmp(name, "F8"))  return SC_F1 + 7;
    if (!strcasecmp(name, "F9"))  return SC_F1 + 8;
    if (!strcasecmp(name, "F10")) return SC_F1 + 9;
    if (!strcasecmp(name, "F11")) return SC_F1 + 10;
    if (!strcasecmp(name, "F12")) return SC_F12;
    if (!strncasecmp(name, "F1", 2) && name[2] >= '3' && name[2] <= '9' && !name[3])
        return SC_F13 + (name[2] - '3');
    if (!strcasecmp(name, "Tab"))      return SC_TAB;
    if (!strcasecmp(name, "Space"))    return SC_SPACE;
    if (!strcasecmp(name, "CapsLock")) return SC_CAPSLOCK;
    if (!strcasecmp(name, "Grave") || !strcasecmp(name, "`")) return SC_GRAVE;
    if (!strcasecmp(name, "Insert"))   return SC_INSERT;
    if (!strcasecmp(name, "Home"))     return SC_HOME;
    if (!strcasecmp(name, "PageUp"))   return SC_PAGEUP;
    if (!strcasecmp(name, "Delete"))   return SC_DELETE;
    if (!strcasecmp(name, "End"))      return SC_END;
    if (!strcasecmp(name, "PageDown")) return SC_PAGEDOWN;
    if (!strcasecmp(name, "Up"))       return SC_UP;
    if (!strcasecmp(name, "Down"))     return SC_DOWN;
    if (!strcasecmp(name, "Left"))     return SC_LEFT;
    if (!strcasecmp(name, "Right"))    return SC_RIGHT;
    if (!strcasecmp(name, "NumLock"))  return SC_NUMLOCK;
    if (!strcasecmp(name, "KPPlus"))   return SC_KP_PLUS;
    if (!strcasecmp(name, "KPMinus"))  return SC_KP_MINUS;
    if (!strcasecmp(name, "KPEnter"))  return SC_KP_ENTER;
    return 68;
}

static int phdr_cb(struct dl_phdr_info* info, size_t size, void* data)
{
    (void)size;
    if (info->dlpi_name && info->dlpi_name[0] != '\0') return 0;
    *(uintptr_t*)data = (uintptr_t)info->dlpi_addr;
    return 1;
}

static uintptr_t main_base(void)
{
    if (g_base_found) return g_base;
    uintptr_t base = 0;
    if (dl_iterate_phdr(phdr_cb, &base) > 0) {
        g_base = base;          /* ET_EXEC: 0, ET_DYN: load bias */
        g_base_found = 1;
    }
    return g_base;
}

static void init_once(void)
{
    g_inited = 1;
    const char* e = getenv("MN9_TURBO_ENABLED");
    g_enabled = e && (*e == '1' || *e == 't' || *e == 'T' || *e == 'y' || *e == 'Y');
    const char* m = getenv("MN9_TURBO_MODE");
    g_mode_toggle = !m || strcasecmp(m, "hold") != 0;
    const char* s = getenv("MN9_TURBO_SPEED");
    if (s && *s) g_speed = (float)atof(s);
    if (g_speed < 1.05f) g_speed = 1.05f;
    if (g_speed > 5.0f) g_speed = 5.0f;
    const char* k = getenv("MN9_TURBO_KEY");
    g_scancode = name_to_scancode(k);
    const char* d = getenv("MN9_TURBO_DEBUG");
    g_debug = d ? atoi(d) : 0;
    if (g_scancode < 0 || g_scancode >= MAX_SCANCODE) g_scancode = 68;
    if (g_enabled)
        fprintf(stderr, "[turbo] enabled mode=%s speed=%.2f key=%s scancode=%d\n",
                g_mode_toggle ? "toggle" : "hold", g_speed,
                k ? k : "F11", g_scancode);
}

static float* time_dilation(void)
{
    uintptr_t base = main_base();
    if (!g_base_found) return NULL;
    void* world = *(void**)(base + OFF_GWORLD);
    if (!world) return NULL;
    typedef void* (*GetWorldInfoFn)(void*, unsigned int);
    GetWorldInfoFn get_info = (GetWorldInfoFn)(base + OFF_GETWORLDINFO);
    void* wi = get_info(world, 0);
    if (!wi) return NULL;
    float* td = (float*)((char*)wi + OFF_TIMEDILATION);
    float cur = *td;
    /* Sanity: the real field is 1.0 in normal play. Refuse to touch anything
       that does not look like a plausible TimeDilation value. */
    if (!(cur >= 0.01f && cur <= 10.0f)) {
        if (!g_bad) {
            g_bad = 1;
            fprintf(stderr, "[turbo] unexpected TimeDilation %.3f, disabling writes\n", cur);
        }
        return NULL;
    }
    return td;
}

static void enforce(void)
{
    if (!g_enabled || !g_applied) return;
    float* td = time_dilation();
    if (td && *td != g_speed) *td = g_speed;
}

static void apply(int on)
{
    if (!g_enabled) return;
    if (on == g_applied) {
        if (on) enforce();
        return;
    }
    float* td = time_dilation();
    if (on) {
        if (td) {
            g_saved = *td;
            *td = g_speed;
            if (g_debug) fprintf(stderr, "[turbo] td before=%.3f now=%.3f\n", g_saved, *td);
        } else if (g_debug && !g_dbg_td_fail) {
            g_dbg_td_fail = 1;
            fprintf(stderr, "[turbo] no TimeDilation yet; will retry next frame\n");
        }
        g_applied = 1;
    } else {
        if (td) *td = g_saved;
        g_applied = 0;
    }
    fprintf(stderr, "[turbo] %s (%.2fx, was %.3f)\n", on ? "on" : "off",
            on ? g_speed : 1.0f, g_saved);
}

typedef unsigned char* (*GetKeyboardStateFn)(int*);

static GetKeyboardStateFn real_keyboard_state(void)
{
    static GetKeyboardStateFn real = NULL;
    if (!real) real = (GetKeyboardStateFn)dlsym(RTLD_NEXT, "SDL_GetKeyboardState");
    return real;
}

static void debug_probe(void)
{
    static uint64_t last_ms = 0;
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    uint64_t ms = (uint64_t)ts.tv_sec * 1000u + (uint64_t)ts.tv_nsec / 1000000u;
    if (ms - last_ms < 1000u) return;
    last_ms = ms;
    uintptr_t b = main_base();
    void* w = g_base_found ? *(void**)(b + OFF_GWORLD) : NULL;
    void* wi = NULL;
    if (w) {
        typedef void* (*GetWorldInfoFn)(void*, unsigned int);
        GetWorldInfoFn get_info = (GetWorldInfoFn)(b + OFF_GETWORLDINFO);
        wi = get_info(w, 0);
    }
    float td = wi ? *(float*)((char*)wi + OFF_TIMEDILATION) : -1.0f;
    fprintf(stderr, "[turbo] probe base=%p world=%p info=%p td=%.3f\n",
            (void*)b, w, wi, td);
}

static void on_key_event(int type, int scancode, int repeat)
{
    if (!g_enabled || scancode != g_scancode) return;
    if (type == 0x300) {                 /* SDL_KEYDOWN */
        if (repeat) return;
        if (g_mode_toggle) g_on = !g_on;
        else g_on = 1;
        if (g_debug) fprintf(stderr, "[turbo] key down -> %s\n", g_on ? "on" : "off");
        apply(g_on);
    } else if (type == 0x301) {          /* SDL_KEYUP */
        if (!g_mode_toggle) {
            g_on = 0;
            if (g_debug) fprintf(stderr, "[turbo] key up -> off\n");
            apply(0);
        }
    }
}

unsigned char* SDL_GetKeyboardState(int* numkeys)
{
    GetKeyboardStateFn real = real_keyboard_state();
    unsigned char* st = real ? real(numkeys) : NULL;
    if (!g_inited) init_once();
    if (g_debug >= 2) debug_probe();
    return st;
}

/* SDL_KEYDOWN / SDL_KEYUP; SDL_Event starts with the type, keyboard events
   carry the scancode after the common header. */
typedef struct {
    uint32_t type;
    uint32_t timestamp;
    uint32_t windowID;
    uint8_t state;
    uint8_t repeat;
    uint8_t pad2;
    uint8_t pad3;
    int32_t scancode;
    int32_t sym;
    uint16_t mod;
    uint32_t unused;
} TurboKeyEvent;

typedef int (*PollEventFn)(void*);

int SDL_PollEvent(void* ev)
{
    static PollEventFn real = NULL;
    if (!real) real = (PollEventFn)dlsym(RTLD_NEXT, "SDL_PollEvent");
    int r = real ? real(ev) : 0;
    if (!g_inited) init_once();
    if (g_enabled && g_applied) enforce();
    if (r && ev) {
        TurboKeyEvent* ke = (TurboKeyEvent*)ev;
        if (ke->type == 0x300 || ke->type == 0x301) {
            if (g_debug && g_enabled)
                fprintf(stderr, "[turbo] key %s scancode=%d sym=%d repeat=%d\n",
                        ke->type == 0x300 ? "down" : "up", ke->scancode, ke->sym, ke->repeat);
            on_key_event((int)ke->type, ke->scancode, ke->repeat);
        }
        if (g_debug >= 2) debug_probe();
    }
    return r;
}
