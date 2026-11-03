// Cel-shading post-process for the retail PC build.
// Hooks SDL_GL_SwapWindow and applies a posterize + saturation pass only to
// near geometry, with silhouettes derived from the game's scene depth FBO.
#include <dlfcn.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <cstddef>
#include <vector>
#include <string>
#include <algorithm>
#include <cmath>

typedef struct SDL_Window SDL_Window;
typedef unsigned int GLenum;
typedef unsigned int GLuint;
typedef unsigned int GLbitfield;
typedef int GLint;
typedef int GLsizei;
typedef float GLfloat;
typedef unsigned char GLboolean;
typedef char GLchar;
typedef ptrdiff_t GLsizeiptr;

static const GLenum GL_TEXTURE_2D = 0x0DE1;
static const GLenum GL_RGBA8 = 0x8058;
static const GLenum GL_RGBA = 0x1908;
static const GLenum GL_DEPTH_COMPONENT = 0x1902;
static const GLenum GL_FLOAT = 0x1406;
static const GLenum GL_UNSIGNED_BYTE = 0x1401;
static const GLenum GL_TEXTURE_MIN_FILTER = 0x2801;
static const GLenum GL_TEXTURE_MAG_FILTER = 0x2800;
static const GLenum GL_TEXTURE_WRAP_S = 0x2802;
static const GLenum GL_TEXTURE_WRAP_T = 0x2803;
static const GLenum GL_LINEAR = 0x2601;
static const GLenum GL_NEAREST = 0x2600;
static const GLenum GL_CLAMP_TO_EDGE = 0x812F;
static const GLenum GL_TEXTURE0 = 0x84C0;
static const GLenum GL_TEXTURE1 = 0x84C1;
static const GLenum GL_VERTEX_SHADER = 0x8B31;
static const GLenum GL_FRAGMENT_SHADER = 0x8B30;
static const GLenum GL_COMPILE_STATUS = 0x8B81;
static const GLenum GL_LINK_STATUS = 0x8B82;
static const GLenum GL_INFO_LOG_LENGTH = 0x8B84;
static const GLenum GL_TEXTURE_BINDING_2D = 0x8069;
static const GLenum GL_CURRENT_PROGRAM = 0x8B8D;
static const GLenum GL_VERTEX_ARRAY_BINDING = 0x85B5;
static const GLenum GL_ACTIVE_TEXTURE = 0x84E0;
static const GLenum GL_BLEND = 0x0BE2;
static const GLenum GL_DEPTH_TEST = 0x0B71;
static const GLenum GL_CULL_FACE = 0x0B44;
static const GLenum GL_SCISSOR_TEST = 0x0C11;
static const GLenum GL_FRAMEBUFFER_SRGB = 0x8DB9;
static const GLenum GL_VIEWPORT = 0x0BA2;
static const GLenum GL_READ_BUFFER = 0x0C02;
static const GLenum GL_BACK = 0x0405;
static const GLenum GL_TRIANGLES = 0x0004;
static const GLenum GL_FRAMEBUFFER = 0x8D40;
static const GLenum GL_READ_FRAMEBUFFER = 0x8CA8;
static const GLenum GL_DRAW_FRAMEBUFFER = 0x8CA9;
static const GLenum GL_READ_FRAMEBUFFER_BINDING = 0x8CAA;
static const GLenum GL_DRAW_FRAMEBUFFER_BINDING = 0x8CA6;
static const GLenum GL_DEPTH_ATTACHMENT = 0x8D00;
static const GLenum GL_DEPTH_BUFFER_BIT = 0x0100;
static const GLenum GL_COLOR_BUFFER_BIT = 0x4000;
static const GLenum GL_COLOR_ATTACHMENT0 = 0x8CE0;
static const GLenum GL_NONE = 0;
static const GLenum GL_FRAMEBUFFER_COMPLETE = 0x8CD5;
static const GLenum GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE = 0x8CD0;
static const GLenum GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME = 0x8CD1;
static const GLenum GL_RENDERBUFFER = 0x8D41;
static const GLenum GL_RENDERBUFFER_WIDTH = 0x8D42;
static const GLenum GL_RENDERBUFFER_HEIGHT = 0x8D43;
static const GLenum GL_SAMPLER_BINDING = 0x8919;

using SwapFn = void (*)(SDL_Window*);
using GetStringFn = const unsigned char* (*)(GLenum);
using GetIntegervFn = void (*)(GLenum, GLint*);
using IsEnabledFn = GLboolean (*)(GLenum);
using GenTexturesFn = void (*)(GLsizei, GLuint*);
using BindTextureFn = void (*)(GLenum, GLuint);
using TexParamFn = void (*)(GLenum, GLenum, GLint);
using TexImageFn = void (*)(GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void*);
using TexSubImageFn = void (*)(GLenum, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, const void*);
using CopyTexSubFn = void (*)(GLenum, GLint, GLint, GLint, GLint, GLint, GLsizei, GLsizei);
using ActiveTextureFn = void (*)(GLenum);
using GenVertexArraysFn = void (*)(GLsizei, GLuint*);
using BindVertexArrayFn = void (*)(GLuint);
using CreateShaderFn = GLuint (*)(GLenum);
using ShaderSourceFn = void (*)(GLuint, GLsizei, const GLchar* const*, const GLint*);
using CompileShaderFn = void (*)(GLuint);
using GetShaderivFn = void (*)(GLuint, GLenum, GLint*);
using GetShaderInfoLogFn = void (*)(GLuint, GLsizei, GLsizei*, GLchar*);
using CreateProgramFn = GLuint (*)(void);
using AttachShaderFn = void (*)(GLuint, GLuint);
using LinkProgramFn = void (*)(GLuint);
using GetProgramivFn = void (*)(GLuint, GLenum, GLint*);
using GetProgramInfoLogFn = void (*)(GLuint, GLsizei, GLsizei*, GLchar*);
using UseProgramFn = void (*)(GLuint);
using GetUniformLocationFn = GLint (*)(GLuint, const GLchar*);
using Uniform1iFn = void (*)(GLint, GLint);
using Uniform1fFn = void (*)(GLint, GLfloat);
using Uniform2fFn = void (*)(GLint, GLfloat, GLfloat);
using DrawArraysFn = void (*)(GLenum, GLint, GLsizei);
using EnableFn = void (*)(GLenum);
using DisableFn = void (*)(GLenum);
using ViewportFn = void (*)(GLint, GLint, GLsizei, GLsizei);
using ReadBufferFn = void (*)(GLenum);
using BindFramebufferFn = void (*)(GLenum, GLuint);
using GenFramebuffersFn = void (*)(GLsizei, GLuint*);
using FramebufferTexture2DFn = void (*)(GLenum, GLenum, GLenum, GLuint, GLint);
using DrawBufferFn = void (*)(GLenum);
using CheckFramebufferStatusFn = GLenum (*)(GLenum);
using BlitFramebufferFn = void (*)(GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLbitfield, GLenum);
using ReadPixelsFn = void (*)(GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, void*);
using GetFBAPFn = void (*)(GLenum, GLenum, GLenum, GLint*);
using BindRenderbufferFn = void (*)(GLenum, GLuint);
using GetRenderbufferParameterivFn = void (*)(GLenum, GLenum, GLint*);
using BindSamplerFn = void (*)(GLuint, GLuint);
using GetErrorFn = GLenum (*)(void);

struct Gl {
    GetStringFn GetString;
    GetIntegervFn GetIntegerv;
    IsEnabledFn IsEnabled;
    GenTexturesFn GenTextures;
    BindTextureFn BindTexture;
    TexParamFn TexParameteri;
    TexImageFn TexImage2D;
    TexSubImageFn TexSubImage2D;
    CopyTexSubFn CopyTexSubImage2D;
    ActiveTextureFn ActiveTexture;
    GenVertexArraysFn GenVertexArrays;
    BindVertexArrayFn BindVertexArray;
    CreateShaderFn CreateShader;
    ShaderSourceFn ShaderSource;
    CompileShaderFn CompileShader;
    GetShaderivFn GetShaderiv;
    GetShaderInfoLogFn GetShaderInfoLog;
    CreateProgramFn CreateProgram;
    AttachShaderFn AttachShader;
    LinkProgramFn LinkProgram;
    GetProgramivFn GetProgramiv;
    GetProgramInfoLogFn GetProgramInfoLog;
    UseProgramFn UseProgram;
    GetUniformLocationFn GetUniformLocation;
    Uniform1iFn Uniform1i;
    Uniform1fFn Uniform1f;
    Uniform2fFn Uniform2f;
    DrawArraysFn DrawArrays;
    EnableFn Enable;
    DisableFn Disable;
    ViewportFn Viewport;
    ReadBufferFn ReadBuffer;
    BindFramebufferFn BindFramebuffer;
    GenFramebuffersFn GenFramebuffers;
    FramebufferTexture2DFn FramebufferTexture2D;
    DrawBufferFn DrawBuffer;
    CheckFramebufferStatusFn CheckFramebufferStatus;
    BlitFramebufferFn BlitFramebuffer;
    ReadPixelsFn ReadPixels;
    GetFBAPFn GetFramebufferAttachmentParameteriv;
    BindRenderbufferFn BindRenderbuffer;
    GetRenderbufferParameterivFn GetRenderbufferParameteriv;
    BindSamplerFn BindSampler;
    GetErrorFn GetError;
};

static Gl g;
static bool g_loaded = false;
static SwapFn g_swap = nullptr;
static bool g_enabled = false;
static bool g_failed = false;
static GLuint g_program = 0;
static GLuint g_vao = 0;
static GLuint g_color_tex = 0;
static GLuint g_color_fbo = 0;
static GLuint g_mask_tex = 0;
static int g_w = 0, g_h = 0;
static int g_mask_w = 0, g_mask_h = 0;
static GLint g_loc_color = -1, g_loc_mask = -1, g_loc_texel = -1;
static GLint g_loc_levels = -1, g_loc_saturation = -1, g_loc_outline = -1;
static float g_levels = 5.0f;
static float g_saturation = 1.45f;
static float g_outline = 1.0f;
static long g_frame = 0;

// Retro mode (replaces the old cel shader).
static bool g_retro = false;
static GLuint g_retro_program = 0;
static GLint g_r_color = -1, g_r_res = -1, g_r_levels = -1;
static GLint g_r_scan = -1, g_r_vig = -1, g_r_dith = -1;
static float g_retro_height = 240.0f;
static float g_retro_levels = 8.0f;
static float g_retro_scan = 0.0f;
static float g_retro_vig = 0.15f;
static float g_retro_dither = 1.0f;

// Extra styles: 0 noir, 1 dot matrix, 2 cinematic, 3 posterize.
static int g_style = -1;
static GLuint g_style_program = 0;
static GLint g_s_color = -1, g_s_texel = -1, g_s_time = -1, g_s_aspect = -1, g_s_style = -1, g_s_aa = -1;
static int g_fxaa = 1;

static bool g_skip_ui = true;
static float g_min_near = 0.01f;
static float g_check_range = 0.0f;
static float g_check_near = 0.0f;
static int g_ui_check = 0;
static int g_flat_state = -1;
static std::vector<float> g_flat_buf;

// FBO tracking. The game resolves GL entry points through SDL/GLX, so we wrap
// glBindFramebuffer to remember which FBOs it renders to.
static GLuint g_fbo_hist[96];
static int g_fbo_hist_pos = 0;
static int g_fbo_hist_count = 0;
static BindFramebufferFn g_real_bindfb = nullptr;

static void record_fbo(GLuint fb) {
    if (!fb) return;
    if (g_fbo_hist_count > 0) {
        int last = (g_fbo_hist_pos - 1 + 96) % 96;
        if (g_fbo_hist[last] == fb) return;
    }
    g_fbo_hist[g_fbo_hist_pos] = fb;
    g_fbo_hist_pos = (g_fbo_hist_pos + 1) % 96;
    if (g_fbo_hist_count < 96) g_fbo_hist_count++;
}

static void wrap_bindfb(GLenum target, GLuint fb) {
    record_fbo(fb);
    if (g_real_bindfb) g_real_bindfb(target, fb);
}

static void* wrap_gl_symbol(const char* name, void* p) {
    if (name && std::strcmp(name, "glBindFramebuffer") == 0 && p) {
        if (p == (void*)&wrap_bindfb) return p;
        g_real_bindfb = (BindFramebufferFn)p;
        return (void*)&wrap_bindfb;
    }
    return p;
}

extern "C" void* SDL_GL_GetProcAddress(const char* name) {
    static void* (*real)(const char*) = nullptr;
    if (!real) real = (void*(*)(const char*))dlsym(RTLD_NEXT, "SDL_GL_GetProcAddress");
    return wrap_gl_symbol(name, real ? real(name) : nullptr);
}

typedef void* (*GLXProc)(const unsigned char*);
extern "C" void* glXGetProcAddressARB(const unsigned char* name) {
    static GLXProc real = nullptr;
    if (!real) real = (GLXProc)dlsym(RTLD_NEXT, "glXGetProcAddressARB");
    return wrap_gl_symbol((const char*)name, real ? real(name) : nullptr);
}

static bool load_gl() {
    void* lib = dlopen("libGL.so.1", RTLD_LAZY | RTLD_LOCAL);
    if (!lib) return false;
#define L(name, type) do { g.name = (type)dlsym(lib, "gl" #name); if (!g.name) { std::fprintf(stderr, "[postfx] missing gl%s\n", #name); return false; } } while (0)
    L(GetString, GetStringFn);
    L(GetIntegerv, GetIntegervFn);
    L(IsEnabled, IsEnabledFn);
    L(GenTextures, GenTexturesFn);
    L(BindTexture, BindTextureFn);
    L(TexParameteri, TexParamFn);
    L(TexImage2D, TexImageFn);
    L(TexSubImage2D, TexSubImageFn);
    L(CopyTexSubImage2D, CopyTexSubFn);
    L(ActiveTexture, ActiveTextureFn);
    L(GenVertexArrays, GenVertexArraysFn);
    L(BindVertexArray, BindVertexArrayFn);
    L(CreateShader, CreateShaderFn);
    L(ShaderSource, ShaderSourceFn);
    L(CompileShader, CompileShaderFn);
    L(GetShaderiv, GetShaderivFn);
    L(GetShaderInfoLog, GetShaderInfoLogFn);
    L(CreateProgram, CreateProgramFn);
    L(AttachShader, AttachShaderFn);
    L(LinkProgram, LinkProgramFn);
    L(GetProgramiv, GetProgramivFn);
    L(GetProgramInfoLog, GetProgramInfoLogFn);
    L(UseProgram, UseProgramFn);
    L(GetUniformLocation, GetUniformLocationFn);
    L(Uniform1i, Uniform1iFn);
    L(Uniform1f, Uniform1fFn);
    L(Uniform2f, Uniform2fFn);
    L(DrawArrays, DrawArraysFn);
    L(Enable, EnableFn);
    L(Disable, DisableFn);
    L(Viewport, ViewportFn);
    L(ReadBuffer, ReadBufferFn);
    L(BindFramebuffer, BindFramebufferFn);
    L(GenFramebuffers, GenFramebuffersFn);
    L(FramebufferTexture2D, FramebufferTexture2DFn);
    L(DrawBuffer, DrawBufferFn);
    L(CheckFramebufferStatus, CheckFramebufferStatusFn);
    L(BlitFramebuffer, BlitFramebufferFn);
    L(ReadPixels, ReadPixelsFn);
    L(GetFramebufferAttachmentParameteriv, GetFBAPFn);
    L(BindRenderbuffer, BindRenderbufferFn);
    L(GetRenderbufferParameteriv, GetRenderbufferParameterivFn);
    L(BindSampler, BindSamplerFn);
    L(GetError, GetErrorFn);
#undef L
    return true;
}

static GLuint compile(GLenum type, const char* src) {
    GLuint s = g.CreateShader(type);
    g.ShaderSource(s, 1, &src, nullptr);
    g.CompileShader(s);
    GLint ok = 0;
    g.GetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[4096] = {};
        g.GetShaderInfoLog(s, sizeof(log), nullptr, log);
        std::fprintf(stderr, "[postfx] shader compile failed: %s\n", log);
        return 0;
    }
    return s;
}

static bool make_program() {
    static const char* vs =
        "#version 330 core\n"
        "out vec2 vUv;\n"
        "void main() {\n"
        "  vec2 p = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);\n"
        "  vUv = p;\n"
        "  gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);\n"
        "}\n";
    static const char* fs =
        "#version 330 core\n"
        "in vec2 vUv;\n"
        "out vec4 fragColor;\n"
        "uniform sampler2D colorTex;\n"
        "uniform sampler2D maskTex;\n"
        "uniform vec2 texel;\n"
        "uniform float levels;\n"
        "uniform float saturation;\n"
        "uniform float outlineStrength;\n"
        "const vec3 W = vec3(0.299, 0.587, 0.114);\n"
        "void main() {\n"
        "  vec3 c = texture(colorTex, vUv).rgb;\n"
        "  vec2 m = texture(maskTex, vUv).rg;\n"
        "  float nearMask = m.r;\n"
        "  float edge = m.g;\n"
        "  float lum = dot(c, W);\n"
        "  vec3 sat = mix(vec3(lum), c, saturation * (1.0 + 0.22 * nearMask));\n"
        "  float q = floor(lum * levels + 0.5) / levels;\n"
        "  vec3 cel = mix(vec3(q), sat, 0.62);\n"
        "  cel = clamp((cel - 0.5) * 1.07 + 0.5, 0.0, 1.0);\n"
        "  float lumOut = dot(cel, W);\n"
        "  cel *= clamp(lum / max(lumOut, 0.001), 0.92, 1.10);\n"
        "  vec3 result = mix(c, cel, nearMask * 0.88);\n"
        "  float edgeDist = min(min(vUv.x, 1.0 - vUv.x), min(vUv.y, 1.0 - vUv.y));\n"
        "  float hudMask = smoothstep(0.045, 0.15, edgeDist);\n"
        "  result = mix(result, vec3(0.0), clamp(edge * nearMask * hudMask * outlineStrength, 0.0, 1.0));\n"
        "  fragColor = vec4(result, 1.0);\n"
        "}\n";
    GLuint v = compile(GL_VERTEX_SHADER, vs);
    GLuint f = compile(GL_FRAGMENT_SHADER, fs);
    if (!v || !f) return false;
    g_program = g.CreateProgram();
    g.AttachShader(g_program, v);
    g.AttachShader(g_program, f);
    g.LinkProgram(g_program);
    GLint ok = 0;
    g.GetProgramiv(g_program, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[4096] = {};
        g.GetProgramInfoLog(g_program, sizeof(log), nullptr, log);
        std::fprintf(stderr, "[postfx] program link failed: %s\n", log);
        return false;
    }
    g_loc_color = g.GetUniformLocation(g_program, "colorTex");
    g_loc_mask = g.GetUniformLocation(g_program, "maskTex");
    g_loc_texel = g.GetUniformLocation(g_program, "texel");
    g_loc_levels = g.GetUniformLocation(g_program, "levels");
    g_loc_saturation = g.GetUniformLocation(g_program, "saturation");
    g_loc_outline = g.GetUniformLocation(g_program, "outlineStrength");
    g.GenVertexArrays(1, &g_vao);
    return true;
}


static bool make_retro_program() {
    static const char* vs =
        "#version 330 core\n"
        "out vec2 vUv;\n"
        "void main() {\n"
        "  vec2 p = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);\n"
        "  vUv = p;\n"
        "  gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);\n"
        "}\n";
    static const char* fs =
        "#version 330 core\n"
        "in vec2 vUv;\n"
        "out vec4 fragColor;\n"
        "uniform sampler2D colorTex;\n"
        "uniform vec2 virtualRes;\n"
        "uniform float levels;\n"
        "uniform float scanline;\n"
        "uniform float vignette;\n"
        "uniform float dither;\n"
        "const float bayer[16] = float[16](\n"
        "  0.0,  8.0,  2.0, 10.0,\n"
        " 12.0,  4.0, 14.0,  6.0,\n"
        "  3.0, 11.0,  1.0,  9.0,\n"
        " 15.0,  7.0, 13.0,  5.0);\n"
        "void main() {\n"
        "  vec2 vr = max(virtualRes, vec2(1.0));\n"
        "  vec2 p = floor(vUv * vr);\n"
        "  vec2 uv = (p + 0.5) / vr;\n"
        "  vec3 c = texture(colorTex, uv).rgb;\n"
        "  if (dither > 0.0) {\n"
        "    int bx = int(mod(p.x, 4.0));\n"
        "    int by = int(mod(p.y, 4.0));\n"
        "    float d = (bayer[by * 4 + bx] / 16.0 - 0.5) * dither;\n"
        "    c += d / max(levels, 1.0);\n"
        "  }\n"
        "  c = floor(clamp(c, 0.0, 1.0) * levels + 0.5) / levels;\n"
        "  if (scanline > 0.0) {\n"
        "    float line = mod(floor(vUv.y * vr.y), 2.0);\n"
        "    c *= 1.0 - scanline * line * 0.55;\n"
        "  }\n"
        "  if (vignette > 0.0) {\n"
        "    vec2 q = vUv - 0.5;\n"
        "    c *= 1.0 - vignette * dot(q, q) * 1.6;\n"
        "  }\n"
        "  fragColor = vec4(clamp(c, 0.0, 1.0), 1.0);\n"
        "}\n";
    GLuint v = compile(GL_VERTEX_SHADER, vs);
    GLuint f = compile(GL_FRAGMENT_SHADER, fs);
    if (!v || !f) return false;
    g_retro_program = g.CreateProgram();
    g.AttachShader(g_retro_program, v);
    g.AttachShader(g_retro_program, f);
    g.LinkProgram(g_retro_program);
    GLint ok = 0;
    g.GetProgramiv(g_retro_program, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[4096] = {};
        g.GetProgramInfoLog(g_retro_program, sizeof(log), nullptr, log);
        std::fprintf(stderr, "[postfx] retro link failed: %s\n", log);
        return false;
    }
    g_r_color = g.GetUniformLocation(g_retro_program, "colorTex");
    g_r_res = g.GetUniformLocation(g_retro_program, "virtualRes");
    g_r_levels = g.GetUniformLocation(g_retro_program, "levels");
    g_r_scan = g.GetUniformLocation(g_retro_program, "scanline");
    g_r_vig = g.GetUniformLocation(g_retro_program, "vignette");
    g_r_dith = g.GetUniformLocation(g_retro_program, "dither");
    if (!g_vao) g.GenVertexArrays(1, &g_vao);
    return true;
}



static bool make_style_program() {
    static const char* vs =
        "#version 330 core\n"
        "out vec2 vUv;\n"
        "void main() {\n"
        "  vec2 p = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);\n"
        "  vUv = p;\n"
        "  gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);\n"
        "}\n";
    static const char* fs =
        "#version 330 core\n"
        "in vec2 vUv;\n"
        "out vec4 fragColor;\n"
        "uniform sampler2D colorTex;\n"
        "uniform vec2 texel;\n"
        "uniform float time;\n"
        "uniform float aspect;\n"
        "uniform int style;\n"
        "uniform int aa;\n"
        "const vec3 W = vec3(0.299, 0.587, 0.114);\n"
        "float hash(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }\n"
        "float luma(vec3 c) { return dot(c, W); }\n"
        "vec3 fxaa(vec2 uv) {\n"
        "  vec3 rgbM = texture(colorTex, uv).rgb;\n"
        "  vec3 rgbNW = texture(colorTex, uv + vec2(-1.0, -1.0) * texel).rgb;\n"
        "  vec3 rgbNE = texture(colorTex, uv + vec2( 1.0, -1.0) * texel).rgb;\n"
        "  vec3 rgbSW = texture(colorTex, uv + vec2(-1.0,  1.0) * texel).rgb;\n"
        "  vec3 rgbSE = texture(colorTex, uv + vec2( 1.0,  1.0) * texel).rgb;\n"
        "  float lM = luma(rgbM);\n"
        "  float lNW = luma(rgbNW), lNE = luma(rgbNE), lSW = luma(rgbSW), lSE = luma(rgbSE);\n"
        "  float lMin = min(lM, min(min(lNW, lNE), min(lSW, lSE)));\n"
        "  float lMax = max(lM, max(max(lNW, lNE), max(lSW, lSE)));\n"
        "  if (lMax - lMin < 0.06) return rgbM;\n"
        "  vec2 dir = vec2(-((lNW + lNE) - (lSW + lSE)), ((lNW + lSW) - (lNE + lSE)));\n"
        "  float reduce = max((lNW + lNE + lSW + lSE) * 0.25 * 0.03125, 1.0 / 128.0);\n"
        "  float rcp = 1.0 / (min(abs(dir.x), abs(dir.y)) + reduce);\n"
        "  dir = clamp(dir * rcp, vec2(-8.0), vec2(8.0)) * texel;\n"
        "  vec3 rgbA = 0.5 * (texture(colorTex, uv + dir * (1.0 / 3.0 - 0.5)).rgb +\n"
        "                     texture(colorTex, uv + dir * (2.0 / 3.0 - 0.5)).rgb);\n"
        "  vec3 rgbB = rgbA * 0.5 + 0.25 * (texture(colorTex, uv - dir * 0.5).rgb +\n"
        "                                   texture(colorTex, uv + dir * 0.5).rgb);\n"
        "  float lB = luma(rgbB);\n"
        "  if (lB < lMin || lB > lMax) return rgbA;\n"
        "  return rgbB;\n"
        "}\n"
        "void main() {\n"
        "  vec3 c = aa != 0 ? fxaa(vUv) : texture(colorTex, vUv).rgb;\n"
        "  if (style == 0) {\n"                       // Noir
        "    float l = clamp((dot(c, W) - 0.5) * 1.28 + 0.5, 0.0, 1.0);\n"
        "    l = pow(l, 1.06);\n"
        "    float vig = 1.0 - 0.42 * dot(vUv - 0.5, vUv - 0.5) * 1.6;\n"
        "    float g = (hash(vUv * vec2(1920.0, 1080.0) + time) - 0.5) * 0.05;\n"
        "    fragColor = vec4(vec3(clamp(l * vig + g, 0.0, 1.0)), 1.0);\n"
        "  } else if (style == 1) {\n"               // Dot matrix (green LED)
        "    float rows = 180.0;\n"
        "    vec2 grid = vec2(rows * aspect, rows);\n"
        "    vec2 p = vUv * grid;\n"
        "    vec2 cell = floor(p);\n"
        "    vec2 f = fract(p) - 0.5;\n"
        "    vec3 sc = texture(colorTex, (cell + 0.5) / grid).rgb;\n"
        "    float l = dot(sc, W);\n"
        "    float radius = 0.47 * sqrt(clamp(l, 0.0, 1.0));\n"
        "    float on = step(length(f), radius);\n"
        "    vec3 dotc = mix(vec3(0.15, 1.0, 0.35), sc, 0.35) * (0.35 + 0.85 * l);\n"
        "    vec3 bg = vec3(0.008, 0.028, 0.014);\n"
        "    fragColor = vec4(mix(bg, dotc, on), 1.0);\n"
        "  } else if (style == 2) {\n"               // Cinematic
        "    float l = dot(c, W);\n"
        "    vec3 grade = mix(vec3(0.94, 1.02, 1.10), vec3(1.06, 1.00, 0.92), smoothstep(0.2, 0.8, l));\n"
        "    c *= grade;\n"
        "    c = clamp((c - 0.5) * 1.08 + 0.5, 0.0, 1.0);\n"
        "    float vig = 1.0 - 0.32 * dot(vUv - 0.5, vUv - 0.5) * 1.6;\n"
        "    c *= vig;\n"
        "    c += (hash(vUv * vec2(1920.0, 1080.0) + time) - 0.5) * 0.03;\n"
        "    float bar = (1.0 - aspect / 2.39) * 0.5;\n"
        "    if (bar > 0.0 && (vUv.y < bar || vUv.y > 1.0 - bar)) c = vec3(0.0);\n"
        "    fragColor = vec4(clamp(c, 0.0, 1.0), 1.0);\n"
        "  } else if (style == 3) {\n"               // Posterize
        "    float levels = 6.0;\n"
        "    vec3 q = floor(c * levels + 0.5) / levels;\n"
        "    float l = dot(q, W);\n"
        "    q = mix(vec3(l), q, 1.15);\n"
        "    fragColor = vec4(clamp(q, 0.0, 1.0), 1.0);\n"
        "  } else {\n"                               // Metallic (smooth)
        "    float l = dot(c, W);\n"
        "    vec3 col = mix(vec3(l), c, 1.30);\n"
        "    vec3 steel = vec3(0.90, 0.95, 1.04);\n"
        "    col *= mix(vec3(1.0), steel, 0.30);\n"
        "    col = clamp((col - 0.5) * 1.05 + 0.515, 0.0, 1.0);\n"
        "    float shine = smoothstep(0.28, 1.0, l);\n"
        "    col += vec3(0.115, 0.125, 0.145) * shine;\n"
        "    float d = vUv.y - (0.30 + 0.09 * sin(vUv.x * 6.2831853));\n"
        "    float band = exp(-d * d * 22.0);\n"
        "    col += vec3(0.16, 0.18, 0.21) * band * smoothstep(0.20, 0.90, l);\n"
        "    float brush = 0.5 + 0.5 * sin(vUv.y * 160.0 + sin(vUv.x * 8.0) * 0.6);\n"
        "    col *= 0.992 + 0.016 * brush;\n"
        "    float sweep = 0.5 + 0.5 * sin((vUv.x * 1.5 + vUv.y * 0.6 + time * 0.07) * 6.2831853);\n"
        "    col += vec3(0.03, 0.034, 0.04) * pow(sweep, 3.0) * smoothstep(0.40, 1.0, l);\n"
        "    col *= 1.0 - 0.07 * dot(vUv - 0.5, vUv - 0.5) * 1.6;\n"
        "    col = pow(max(col, vec3(0.0)), vec3(0.96)) + 0.005;\n"
        "    fragColor = vec4(clamp(col, 0.0, 1.0), 1.0);\n"
        "  }\n"
        "}\n";
    GLuint v = compile(GL_VERTEX_SHADER, vs);
    GLuint f = compile(GL_FRAGMENT_SHADER, fs);
    if (!v || !f) return false;
    g_style_program = g.CreateProgram();
    g.AttachShader(g_style_program, v);
    g.AttachShader(g_style_program, f);
    g.LinkProgram(g_style_program);
    GLint ok = 0;
    g.GetProgramiv(g_style_program, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[4096] = {};
        g.GetProgramInfoLog(g_style_program, sizeof(log), nullptr, log);
        std::fprintf(stderr, "[postfx] style link failed: %s\n", log);
        return false;
    }
    g_s_color = g.GetUniformLocation(g_style_program, "colorTex");
    g_s_texel = g.GetUniformLocation(g_style_program, "texel");
    g_s_time = g.GetUniformLocation(g_style_program, "time");
    g_s_aspect = g.GetUniformLocation(g_style_program, "aspect");
    g_s_style = g.GetUniformLocation(g_style_program, "style");
    g_s_aa = g.GetUniformLocation(g_style_program, "aa");
    if (!g_vao) g.GenVertexArrays(1, &g_vao);
    return true;
}

static bool get_fbo_depth_size(GLuint fbo, int* out_w, int* out_h) {
    g.BindFramebuffer(GL_READ_FRAMEBUFFER, fbo);
    if (g.CheckFramebufferStatus(GL_READ_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) return false;
    GLint type = 0, name = 0, bw = 0, bh = 0;
    g.GetFramebufferAttachmentParameteriv(GL_READ_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &type);
    if (type == 0) return false;
    g.GetFramebufferAttachmentParameteriv(GL_READ_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, &name);
    if (type == GL_RENDERBUFFER && name) {
        g.BindRenderbuffer(GL_RENDERBUFFER, (GLuint)name);
        g.GetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_WIDTH, &bw);
        g.GetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_HEIGHT, &bh);
        g.BindRenderbuffer(GL_RENDERBUFFER, 0);
    }
    *out_w = bw;
    *out_h = bh;
    return bw > 0 && bh > 0;
}

// Find the half-resolution depth FBO the renderer uses for the scene.
static GLuint find_scene_fbo(int w, int h, int* out_w, int* out_h) {
    GLuint best = 0;
    int best_w = 0, best_h = 0;
    int best_score = -1;
    for (int i = 0; i < g_fbo_hist_count; i++) {
        int idx = (g_fbo_hist_pos - 1 - i + 96 * 2) % 96;
        GLuint fb = g_fbo_hist[idx];
        if (!fb) continue;
        int bw = 0, bh = 0;
        if (!get_fbo_depth_size(fb, &bw, &bh)) continue;
        int score = 0;
        if (bw == w / 2 && bh == h / 2) score = 100;
        else if (bw == w && bh == h) score = 10;
        else if (bw < w && bh < h) score = 20;
        if (score > best_score) {
            best_score = score;
            best = fb;
            best_w = bw;
            best_h = bh;
        }
        if (score == 100) break;
    }
    g.BindFramebuffer(GL_FRAMEBUFFER, 0);
    if (out_w) *out_w = best_w;
    if (out_h) *out_h = best_h;
    return best;
}

static std::vector<float> g_depth_buf;
static std::vector<unsigned char> g_mask_buf;
static GLuint g_scene_fbo = 0;
static int g_scene_w = 0, g_scene_h = 0;

static GLuint ensure_scene_fbo(int w, int h);

static int scene_is_flat(int w, int h) {
    GLint prev_read = 0, prev_draw = 0, prev_buf = 0;
    g.GetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &prev_read);
    g.GetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &prev_draw);
    g.GetIntegerv(GL_READ_BUFFER, &prev_buf);
    GLuint scene = ensure_scene_fbo(w, h);
    int result = -1;
    int sw = g_scene_w, sh = g_scene_h;
    if (scene && sw >= 4 && sh >= 4) {
        int rw = std::min(sw, 320), rh = std::min(sh, 180);
        int rx = (sw - rw) / 2, ry = (sh - rh) / 2;
        if ((int)g_flat_buf.size() < rw * rh) g_flat_buf.resize((size_t)rw * rh);
        g.BindFramebuffer(GL_READ_FRAMEBUFFER, scene);
        g.ReadBuffer(GL_NONE);
        g.ReadPixels(rx, ry, rw, rh, GL_DEPTH_COMPONENT, GL_FLOAT, g_flat_buf.data());
        float dmin = 1.0f, dmax = 0.0f;
        int near = 0;
        for (int i = 0; i < rw * rh; i++) {
            float d = g_flat_buf[i];
            if (d < dmin) dmin = d;
            if (d > dmax) dmax = d;
            if (d < 0.85f) near++;
        }
        float frac = (float)near / (float)(rw * rh);
        // A menu has flat depth, or a 3D backdrop but almost no near geometry.
        result = (((dmax - dmin) < 0.0004f) || (frac < g_min_near)) ? 1 : 0;
        g_check_range = dmax - dmin;
        g_check_near = frac;
        int zeros = 0;
        for (int i = 0; i < rw * rh; i++) if (g_flat_buf[i] == 0.0f) zeros++;
        if ((g_frame % 120) == 0) {
            std::fprintf(stderr, "[postfx] depth fbo=%u size=%dx%d sample=%dx%d min=%.4f max=%.4f zeros=%d/%d near=%.3f\n",
                         (unsigned)scene, sw, sh, rw, rh, dmin, dmax, zeros, rw * rh, frac);
        }
    }
    g.BindFramebuffer(GL_READ_FRAMEBUFFER, (GLuint)prev_read);
    g.BindFramebuffer(GL_DRAW_FRAMEBUFFER, (GLuint)prev_draw);
    g.ReadBuffer((GLenum)prev_buf);
    return result;
}

static GLuint ensure_scene_fbo(int w, int h) {
    if (g_scene_fbo) {
        int bw = 0, bh = 0;
        if (get_fbo_depth_size(g_scene_fbo, &bw, &bh) && bw > 0) {
            g_scene_w = bw;
            g_scene_h = bh;
            g.BindFramebuffer(GL_FRAMEBUFFER, 0);
            return g_scene_fbo;
        }
        g_scene_fbo = 0;
    }
    g_scene_fbo = find_scene_fbo(w, h, &g_scene_w, &g_scene_h);
    { static int once = 0; if (!once) { once = 1;
        std::fprintf(stderr, "[postfx] fbo history=%d chosen=%u depth=%dx%d viewport=%dx%d\n",
                     g_fbo_hist_count, (unsigned)g_scene_fbo, g_scene_w, g_scene_h, w, h); } }
    return g_scene_fbo;
}

static bool build_mask(int w, int h) {
    GLuint scene = ensure_scene_fbo(w, h);
    int sw = g_scene_w, sh = g_scene_h;
    if (!scene || sw < 4 || sh < 4) return false;
    if ((int)g_depth_buf.size() < sw * sh) g_depth_buf.resize((size_t)sw * sh);

    g.BindFramebuffer(GL_READ_FRAMEBUFFER, scene);
    g.ReadBuffer(GL_NONE);
    g.ReadPixels(0, 0, sw, sh, GL_DEPTH_COMPONENT, GL_FLOAT, g_depth_buf.data());
    g.BindFramebuffer(GL_FRAMEBUFFER, 0);

    float dmin = 1.0f, dmax = 0.0f;
    for (int i = 0; i < sw * sh; i++) {
        float d = g_depth_buf[i];
        if (d < dmin) dmin = d;
        if (d > dmax) dmax = d;
    }
    float range = dmax - dmin;
    // Menus and full-screen 2D screens have flat depth: leave them untouched.
    if (range < 0.0004f) return false;

    int mw = sw;
    int mh = sh;
    if (mw < 1) mw = 1;
    if (mh < 1) mh = 1;
    if (g_mask_w != mw || g_mask_h != mh) {
        g_mask_w = mw;
        g_mask_h = mh;
        g_mask_buf.assign((size_t)mw * mh * 4, 0);
        g.ActiveTexture(GL_TEXTURE1);
        g.BindTexture(GL_TEXTURE_2D, g_mask_tex);
        g.TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, mw, mh, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    }

    float inv = 1.0f / range;
    int mw4 = mw * 4;
    for (int y = 0; y < mh; y++) {
        int sy = std::min(y, sh - 1);
        for (int x = 0; x < mw; x++) {
            int sx = std::min(x, sw - 1);
            auto zat = [&](int px, int py) {
                px = std::max(0, std::min(px, sw - 1));
                py = std::max(0, std::min(py, sh - 1));
                return (g_depth_buf[(size_t)py * sw + px] - dmin) * inv;
            };
            float z00 = zat(sx - 1, sy - 1), z10 = zat(sx, sy - 1), z20 = zat(sx + 1, sy - 1);
            float z01 = zat(sx - 1, sy),     z11 = zat(sx, sy),     z21 = zat(sx + 1, sy);
            float z02 = zat(sx - 1, sy + 1), z12 = zat(sx, sy + 1), z22 = zat(sx + 1, sy + 1);
            float gx = -z00 - 2.0f * z10 - z20 + z02 + 2.0f * z12 + z22;
            float gy = -z00 - 2.0f * z01 - z02 + z20 + 2.0f * z21 + z22;
            float e = std::sqrt(gx * gx + gy * gy);
            float zc = z11;
            float nearMask = 1.0f - std::max(0.0f, std::min(1.0f, (zc - 0.80f) / 0.20f));
            nearMask = nearMask * nearMask * (3.0f - 2.0f * nearMask);
            float edge = std::max(0.0f, std::min(1.0f, (e - 0.35f) * 0.9f));
            edge = edge * edge * (3.0f - 2.0f * edge);
            unsigned char* p = &g_mask_buf[(size_t)y * mw4 + x * 4];
            p[0] = (unsigned char)(nearMask * 255.0f);
            p[1] = (unsigned char)(edge * 255.0f);
            p[2] = 0;
            p[3] = 255;
        }
    }

    g.ActiveTexture(GL_TEXTURE1);
    g.BindTexture(GL_TEXTURE_2D, g_mask_tex);
    g.TexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, mw, mh, GL_RGBA, GL_UNSIGNED_BYTE, g_mask_buf.data());
    return true;
}

static bool ensure_targets(int w, int h) {
    if (g_color_tex && w == g_w && h == g_h) return true;
    g_w = w;
    g_h = h;

    if (!g_color_tex) g.GenTextures(1, &g_color_tex);
    g.ActiveTexture(GL_TEXTURE0);
    g.BindTexture(GL_TEXTURE_2D, g_color_tex);
    g.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    g.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    g.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    g.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    g.TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    if (!g_color_fbo) g.GenFramebuffers(1, &g_color_fbo);
    g.BindFramebuffer(GL_DRAW_FRAMEBUFFER, g_color_fbo);
    g.FramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, g_color_tex, 0);
    g.BindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

    if (!g_mask_tex) g.GenTextures(1, &g_mask_tex);
    g.ActiveTexture(GL_TEXTURE1);
    g.BindTexture(GL_TEXTURE_2D, g_mask_tex);
    g.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    g.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    g.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    g.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    g.TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    g_mask_w = 0;
    g_mask_h = 0;
    return true;
}

static void post_process(int w, int h) {
    GLint prev_program = 0, prev_active = GL_TEXTURE0, prev_tex0 = 0, prev_tex1 = 0, prev_vao = 0;
    GLint prev_viewport[4] = {0,0,0,0}, prev_read = GL_BACK;
    GLint prev_read_fbo = 0, prev_draw_fbo = 0;
    g.GetIntegerv(GL_CURRENT_PROGRAM, &prev_program);
    g.GetIntegerv(GL_ACTIVE_TEXTURE, &prev_active);
    g.GetIntegerv(GL_VERTEX_ARRAY_BINDING, &prev_vao);
    g.GetIntegerv(GL_VIEWPORT, prev_viewport);
    g.GetIntegerv(GL_READ_BUFFER, &prev_read);
    g.GetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &prev_read_fbo);
    g.GetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &prev_draw_fbo);
    g.ActiveTexture(GL_TEXTURE0);
    g.GetIntegerv(GL_TEXTURE_BINDING_2D, &prev_tex0);
    GLint prev_samp0 = 0, prev_samp1 = 0;
    g.GetIntegerv(GL_SAMPLER_BINDING, &prev_samp0);
    g.ActiveTexture(GL_TEXTURE1);
    g.GetIntegerv(GL_TEXTURE_BINDING_2D, &prev_tex1);
    g.GetIntegerv(GL_SAMPLER_BINDING, &prev_samp1);
    GLboolean blend = g.IsEnabled(GL_BLEND);
    GLboolean depth = g.IsEnabled(GL_DEPTH_TEST);
    GLboolean cull = g.IsEnabled(GL_CULL_FACE);
    GLboolean scissor = g.IsEnabled(GL_SCISSOR_TEST);
    GLboolean srgb = g.IsEnabled(GL_FRAMEBUFFER_SRGB);

    { static int once = 0; if (!once) { once = 1;
        std::fprintf(stderr, "[postfx] bindings read_fbo=%d draw_fbo=%d read_buf=0x%X\n",
                     (int)prev_read_fbo, (int)prev_draw_fbo, (unsigned)prev_read); } }

    if (!ensure_targets(w, h)) return;

    // Resolve/copy the final color buffer into our texture. A blit handles a
    // multisampled default framebuffer, which CopyTexSubImage2D cannot.
    if (srgb) g.Disable(GL_FRAMEBUFFER_SRGB);
    GLuint src_fbo = (GLuint)prev_draw_fbo;
    g.BindFramebuffer(GL_READ_FRAMEBUFFER, src_fbo);
    if (src_fbo) g.ReadBuffer(GL_COLOR_ATTACHMENT0); else g.ReadBuffer((GLenum)prev_read);
    g.BindFramebuffer(GL_DRAW_FRAMEBUFFER, g_color_fbo);
    g.BlitFramebuffer(0, 0, w, h, 0, 0, w, h, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    g.BindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    { static int once = 0; GLenum e = g.GetError();
      if (e && !once) {
          once = 1;
          GLint samples = 0, sbufs = 0, rbuf = 0, rfb = 0, dfb = 0;
          g.GetIntegerv(0x80A9, &samples); g.GetIntegerv(0x80A8, &sbufs);
          g.GetIntegerv(GL_READ_BUFFER, &rbuf);
          g.GetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &rfb);
          g.GetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &dfb);
          std::fprintf(stderr, "[postfx] GL error after blit: 0x%X samples=%d sbufs=%d rbuf=0x%X rfb=%d dfb=%d\n",
                       e, samples, sbufs, rbuf, rfb, dfb);
      } }

    g.Disable(GL_BLEND);
    g.Disable(GL_DEPTH_TEST);
    g.Disable(GL_CULL_FACE);
    g.Disable(GL_SCISSOR_TEST);
    g.Disable(GL_FRAMEBUFFER_SRGB);
    g.Viewport(0, 0, w, h);
    g.ActiveTexture(GL_TEXTURE0);
    g.BindTexture(GL_TEXTURE_2D, g_color_tex);
    g.BindSampler(0, 0);

    if (g_style >= 0) {
        g.UseProgram(g_style_program);
        g.Uniform1i(g_s_color, 0);
        g.Uniform2f(g_s_texel, 1.0f / (float)w, 1.0f / (float)h);
        g.Uniform1f(g_s_time, (float)g_frame / 60.0f);
        g.Uniform1f(g_s_aspect, (float)w / (float)h);
        g.Uniform1i(g_s_style, g_style);
        g.Uniform1i(g_s_aa, g_fxaa);
    } else if (g_retro) {
        g.UseProgram(g_retro_program);
        float vh = g_retro_height;
        float vw = std::max(1.0f, std::roundf((float)w * vh / (float)h));
        g.Uniform1i(g_r_color, 0);
        g.Uniform2f(g_r_res, vw, vh);
        g.Uniform1f(g_r_levels, g_retro_levels);
        g.Uniform1f(g_r_scan, g_retro_scan);
        g.Uniform1f(g_r_vig, g_retro_vig);
        g.Uniform1f(g_r_dith, g_retro_dither);
    } else {
        g.ActiveTexture(GL_TEXTURE1);
        g.BindTexture(GL_TEXTURE_2D, g_mask_tex);
        g.BindSampler(1, 0);
        g.UseProgram(g_program);
        g.Uniform1i(g_loc_color, 0);
        g.Uniform1i(g_loc_mask, 1);
        g.Uniform2f(g_loc_texel, 1.0f / (float)w, 1.0f / (float)h);
        g.Uniform1f(g_loc_levels, g_levels);
        g.Uniform1f(g_loc_saturation, g_saturation);
        g.Uniform1f(g_loc_outline, g_outline);
    }
    g.BindVertexArray(g_vao);
    g.DrawArrays(GL_TRIANGLES, 0, 3);
    { GLenum e = g.GetError(); if (e) std::fprintf(stderr, "[postfx] GL error after draw: 0x%X\n", e); }

    g.BindVertexArray((GLuint)prev_vao);
    g.UseProgram((GLuint)prev_program);
    g.ActiveTexture(GL_TEXTURE1);
    g.BindSampler(1, (GLuint)prev_samp1);
    g.BindTexture(GL_TEXTURE_2D, (GLuint)prev_tex1);
    g.ActiveTexture(GL_TEXTURE0);
    g.BindSampler(0, (GLuint)prev_samp0);
    g.BindTexture(GL_TEXTURE_2D, (GLuint)prev_tex0);
    g.ActiveTexture((GLenum)prev_active);
    g.Viewport(prev_viewport[0], prev_viewport[1], prev_viewport[2], prev_viewport[3]);
    g.BindFramebuffer(GL_READ_FRAMEBUFFER, (GLuint)prev_read_fbo);
    g.BindFramebuffer(GL_DRAW_FRAMEBUFFER, (GLuint)prev_draw_fbo);
    g.ReadBuffer((GLenum)prev_read);
    if (blend) g.Enable(GL_BLEND);
    if (depth) g.Enable(GL_DEPTH_TEST);
    if (cull) g.Enable(GL_CULL_FACE);
    if (scissor) g.Enable(GL_SCISSOR_TEST);
    if (srgb) g.Enable(GL_FRAMEBUFFER_SRGB);
}

extern "C" void SDL_GL_SwapWindow(SDL_Window* win) {
    if (!g_swap) g_swap = (SwapFn)dlsym(RTLD_NEXT, "SDL_GL_SwapWindow");
    if (!g_loaded) {
        g_loaded = true;
        const char* mode = std::getenv("MN9_POSTFX_MODE");
        const char* retro = std::getenv("MN9_RETRO_SHADING");
        const char* cel = std::getenv("MN9_CEL_SHADING");
        int style = -1;
        if (mode) {
            if (!std::strcmp(mode, "noir")) style = 0;
            else if (!std::strcmp(mode, "dotmatrix")) style = 1;
            else if (!std::strcmp(mode, "cinematic")) style = 2;
            else if (!std::strcmp(mode, "posterize")) style = 3;
            else if (!std::strcmp(mode, "metallic")) style = 4;
        }
        bool wantRetro = style < 0 &&
                         ((mode && std::strcmp(mode, "retro") == 0) ||
                          (retro && *retro && *retro != '0') ||
                          (cel && *cel && *cel != '0'));
        g_style = style;
        g_enabled = wantRetro || style >= 0;
        g_retro = wantRetro;
        if (const char* e = std::getenv("MN9_FXAA")) g_fxaa = (*e && *e != '0') ? 1 : 0;
        if (style >= 0) {
            if (!load_gl() || !make_style_program()) {
                g_failed = true;
                std::fprintf(stderr, "[postfx] disabled (init failed)\n");
            } else {
                static const char* names[] = { "noir", "dotmatrix", "cinematic", "posterize", "metallic" };
                std::fprintf(stderr, "[postfx] style=%s\n", names[style]);
            }
        } else if (wantRetro) {
            const char* preset = std::getenv("MN9_RETRO_PRESET");
            std::string p = preset ? preset : "";
            if (p.empty()) {
                float lv = 5.0f;
                if (const char* e = std::getenv("MN9_CEL_LEVELS")) lv = (float)std::atof(e);
                p = (lv >= 5.5f) ? "soft" : (lv <= 4.5f) ? "crt" : "8bit";
            }
            if (p == "soft") { g_retro_height = 270; g_retro_levels = 32; g_retro_scan = 0.0f; g_retro_vig = 0.10f; g_retro_dither = 0.0f; }
            else if (p == "crt") { g_retro_height = 240; g_retro_levels = 16; g_retro_scan = 0.55f; g_retro_vig = 0.40f; g_retro_dither = 1.0f; }
            else { g_retro_height = 240; g_retro_levels = 8; g_retro_scan = 0.0f; g_retro_vig = 0.15f; g_retro_dither = 1.0f; }
            if (const char* e = std::getenv("MN9_RETRO_HEIGHT")) g_retro_height = (float)std::atof(e);
            if (const char* e = std::getenv("MN9_RETRO_LEVELS")) g_retro_levels = (float)std::atof(e);
            if (const char* e = std::getenv("MN9_RETRO_SCANLINES")) g_retro_scan = (float)std::atof(e);
            if (const char* e = std::getenv("MN9_RETRO_VIGNETTE")) g_retro_vig = (float)std::atof(e);
            if (const char* e = std::getenv("MN9_RETRO_DITHER")) g_retro_dither = (float)std::atof(e);
            if (!load_gl() || !make_retro_program()) {
                g_failed = true;
                std::fprintf(stderr, "[postfx] disabled (init failed)\n");
            } else {
                std::fprintf(stderr, "[postfx] retro preset=%s height=%.0f levels=%.0f\n",
                             p.c_str(), g_retro_height, g_retro_levels);
            }
        }
    }
    if (g_enabled && !g_failed) {
        static bool once = false;
        if (!once) {
            once = true;
            std::fprintf(stderr, "[postfx] GL_VERSION=%s\n", (const char*)g.GetString(0x1F02));
        }
        GLint vp[4] = {0,0,0,0};
        g.GetIntegerv(GL_VIEWPORT, vp);
        if (vp[2] > 0 && vp[3] > 0) {
            // Always process, from the very first frame. No stage gating.
            post_process(vp[2], vp[3]);
        }
    }
    g_frame++;
    if (g_swap) g_swap(win);
}
