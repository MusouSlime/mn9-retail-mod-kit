/*
 * MN9 turbo-fire shim.
 *
 * Holding the game's fire button should keep firing. The retail build's
 * controller state is cached in kog::g_sdlControllers (base + 0x2f09700,
 * 0x1c bytes per controller, one byte per SDL button). The game polls that
 * cache through kog::SDLControllerGetButton; we toggle the configured fire
 * byte at the requested rate while the physical button is held.
 *
 * Mouse fire (LeftMouseButton -> EAK_Attack) is handled by gating
 * SDL_GetMouseState with the same phase.
 *
 * Env (set by the spawn wrapper from .mn9charsel/turbo.conf):
 *   MN9_TURBO_ENABLED  1/0
 *   MN9_TURBO_RATE     taps per second (default 10)
 *   MN9_TURBO_BUTTON   SDL controller button (A=0,B=1,X=2,Y=3; default 2)
 *   MN9_TURBO_KEY      optional runtime toggle key (empty = always on)
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

#define OFF_SDL_BUTTONS 0x2f09700u  /* kog::g_sdlControllers button area */
#define CTRL_STRIDE     0x1cu
#define CTRL_COUNT      4
#define MAX_SCANCODE    512

static uintptr_t g_base = 0;
static int g_base_found = 0;
static int g_inited = 0;
static int g_enabled = 0;
static int g_auto_on = 1;
static double g_rate = 10.0;
static int g_button = 2;             /* SDL_CONTROLLER_BUTTON_X */
static int g_key = -1;               /* optional toggle scancode */
static int g_fire_held = 0;
static int g_mouse_held = 0;
static double g_t0 = 0;

static double now_s(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

static int name_to_scancode(const char* name)
{
    if (!name || !*name) return -1;
    if (!strcasecmp(name, "none") || !strcasecmp(name, "off")) return -1;
    if (name[0] && !name[1]) {
        char c = name[0];
        if (c >= 'a' && c <= 'z') c -= 32;
        if (c >= 'A' && c <= 'Z') return 4 + (c - 'A');
        if (c >= '0' && c <= '9') return (c == '0') ? 39 : (30 + (c - '1'));
    }
    if (!strcasecmp(name, "F1"))  return 58;
    if (!strcasecmp(name, "F2"))  return 59;
    if (!strcasecmp(name, "F3"))  return 60;
    if (!strcasecmp(name, "F4"))  return 61;
    if (!strcasecmp(name, "F5"))  return 62;
    if (!strcasecmp(name, "F6"))  return 63;
    if (!strcasecmp(name, "F7"))  return 64;
    if (!strcasecmp(name, "F8"))  return 65;
    if (!strcasecmp(name, "F9"))  return 66;
    if (!strcasecmp(name, "F10")) return 67;
    if (!strcasecmp(name, "F11")) return 68;
    if (!strcasecmp(name, "F12")) return 69;
    if (!strcasecmp(name, "Tab"))      return 43;
    if (!strcasecmp(name, "Space"))    return 44;
    if (!strcasecmp(name, "CapsLock")) return 57;
    if (!strcasecmp(name, "Grave") || !strcasecmp(name, "`")) return 53;
    return -1;
}

static int phdr_cb(struct dl_phdr_info* info, size_t size, void* data)
{
    (void)size;
    if (info->dlpi_name && info->dlpi_name[0] != '\0') return 0;
    *(uintptr_t*)data = (uintptr_t)info->dlpi_addr;
    return 1;
}

static void init_once(void)
{
    g_inited = 1;
    g_t0 = now_s();

    const char* e = getenv("MN9_TURBO_ENABLED");
    g_enabled = e && (*e == '1' || *e == 't' || *e == 'T' || *e == 'y' || *e == 'Y');

    const char* r = getenv("MN9_TURBO_RATE");
    if (r && *r) g_rate = atof(r);
    if (g_rate < 2.0) g_rate = 2.0;
    if (g_rate > 30.0) g_rate = 30.0;

    const char* b = getenv("MN9_TURBO_BUTTON");
    if (b && *b) g_button = atoi(b);
    if (g_button < 0 || g_button > 3) g_button = 2;

    g_key = name_to_scancode(getenv("MN9_TURBO_KEY"));

    uintptr_t base = 0;
    if (dl_iterate_phdr(phdr_cb, &base) > 0) {
        g_base = base;
        g_base_found = 1;
    }

    if (g_enabled)
        fprintf(stderr, "[turbo] auto-fire enabled rate=%.0fHz button=%d key=%s\n",
                g_rate, g_button, g_key >= 0 ? "on" : "off");
}

static int fire_phase(void)
{
    double t = now_s() - g_t0;
    long long n = (long long)(t * g_rate);
    return (int)(n & 1);
}

static void write_controller_fire(unsigned char value)
{
    if (!g_base_found) return;
    for (int c = 0; c < CTRL_COUNT; c++) {
        unsigned char* p = (unsigned char*)(g_base + OFF_SDL_BUTTONS +
                                            (uintptr_t)c * CTRL_STRIDE + (uintptr_t)g_button);
        *p = value;
    }
}

static void apply_fire(void)
{
    if (!g_enabled || !g_auto_on || !g_fire_held) return;
    write_controller_fire(fire_phase() ? 1 : 0);
}

static void clear_fire(void)
{
    if (g_fire_held) write_controller_fire(0);
}

/* SDL_Event heads. */
typedef struct {
    uint32_t type, timestamp;
    int32_t which;
    uint8_t button, state, pad1, pad2;
} CtrlButtonEvent;

typedef struct {
    uint32_t type, timestamp, windowID, which;
    uint8_t button, state, clicks, pad;
} MouseButtonEvent;

typedef struct {
    uint32_t type, timestamp, windowID;
    uint8_t state, repeat, pad2, pad3;
    int32_t scancode, sym;
    uint16_t mod;
    uint32_t unused;
} KeyEvent;

typedef int (*PollEventFn)(void*);
typedef uint32_t (*GetMouseStateFn)(int*, int*);

int SDL_PollEvent(void* ev)
{
    static PollEventFn real = NULL;
    if (!real) real = (PollEventFn)dlsym(RTLD_NEXT, "SDL_PollEvent");
    int r = real ? real(ev) : 0;
    if (!g_inited) init_once();

    if (r && ev) {
        uint32_t type = *(uint32_t*)ev;
        if (type == 0x653 || type == 0x654) {           /* controller down/up */
            CtrlButtonEvent* c = (CtrlButtonEvent*)ev;
            if (c->button == (uint8_t)g_button) g_fire_held = (type == 0x653);
        } else if (type == 0x401 || type == 0x402) {    /* mouse down/up */
            MouseButtonEvent* m = (MouseButtonEvent*)ev;
            if (m->button == 1) g_mouse_held = (type == 0x401);
        } else if (type == 0x300 && g_key >= 0) {       /* key down */
            KeyEvent* k = (KeyEvent*)ev;
            if (k->scancode == g_key && !k->repeat) {
                g_auto_on = !g_auto_on;
                if (!g_auto_on) clear_fire();
                fprintf(stderr, "[turbo] auto-fire %s\n", g_auto_on ? "on" : "off");
            }
        }
    }

    apply_fire();
    return r;
}

uint32_t SDL_GetMouseState(int* x, int* y)
{
    static GetMouseStateFn real = NULL;
    if (!real) real = (GetMouseStateFn)dlsym(RTLD_NEXT, "SDL_GetMouseState");
    uint32_t buttons = real ? real(x, y) : 0;
    if (!g_inited) init_once();
    if (g_enabled && g_auto_on && g_mouse_held && !fire_phase())
        buttons &= ~1u;   /* release the left button during the off phase */
    return buttons;
}
