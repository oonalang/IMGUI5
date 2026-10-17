// Neverlose / HexSync ImGui menu on Android (NativeActivity + EGL + OpenGL ES 3).
#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_android.h"
#include "imgui_impl_opengl3.h"

#include <android/asset_manager.h>
#include <android/input.h>
#include <android/log.h>
#include <android/native_window.h>
#include <android_native_app_glue.h>
#include <EGL/egl.h>
#include <GLES3/gl3.h>

#include <cfloat>
#include <cmath>
#include <cstdint>
#include <cstring>

#include "hashes.hpp"
#include "hexsync_intro.hpp"
#include "neverlose_menu.hpp"
#include "nl_layout.hpp"

#define LOG_TAG "Neverlose"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// ---- tweakables -------------------------------------------------------------
static constexpr float kFill = 0.97f;        // share of the screen the menu may cover
static constexpr float kUserScale = 1.0f;    // extra multiplier on top of auto-fit
static constexpr bool  kPlayIntro = true;    // HexSync logo animation at start
static constexpr bool  kDrawBackground = true;

// ---- state ------------------------------------------------------------------
static android_app* g_App = nullptr;
static bool g_Initialized = false;
static EGLDisplay g_EglDisplay = EGL_NO_DISPLAY;
static EGLSurface g_EglSurface = EGL_NO_SURFACE;
static EGLContext g_EglContext = EGL_NO_CONTEXT;

static nl::Layout g_Layout;
static float g_FontScale = 0.0f;
static bool g_FontsLive = false;             // atlas texture already uploaded?

static ImTextureID g_Avatar = nullptr;
static ImTextureID g_Logo = nullptr;
static ImTextureID g_Glow = nullptr;
static ImFont* g_Wordmark = nullptr;
static double g_RevealStart = -1.0;

// ---- assets -----------------------------------------------------------------
// Returns a buffer allocated with IM_ALLOC (ImGui's atlas frees it), or null.
static void* ReadAsset(const char* name, size_t* out_size)
{
    AAsset* asset = AAssetManager_open(g_App->activity->assetManager, name, AASSET_MODE_BUFFER);
    if (!asset)
    {
        LOGE("missing asset: %s", name);
        return nullptr;
    }
    const size_t size = (size_t)AAsset_getLength(asset);
    void* data = IM_ALLOC(size);
    AAsset_read(asset, data, size);
    AAsset_close(asset);
    *out_size = size;
    return data;
}

static ImTextureID MakeTexture(const unsigned char* rgba, int w, int h)
{
    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
    return (ImTextureID)(intptr_t)tex;
}

// avatar.rgba / logo.rgba: uint32 width, uint32 height (little endian), then RGBA8 pixels.
static ImTextureID LoadRgbaAsset(const char* name)
{
    size_t size = 0;
    unsigned char* data = (unsigned char*)ReadAsset(name, &size);
    if (!data) return nullptr;
    ImTextureID tex = nullptr;
    if (size > 8)
    {
        uint32_t w, h;
        std::memcpy(&w, data, 4);
        std::memcpy(&h, data + 4, 4);
        if ((size_t)w * h * 4 + 8 == size)
            tex = MakeTexture(data + 8, (int)w, (int)h);
        else
            LOGE("bad size in %s", name);
    }
    IM_FREE(data);
    return tex;
}

static void LoadTextures()
{
    g_Avatar = LoadRgbaAsset("avatar.rgba");
    g_Logo = LoadRgbaAsset("logo.rgba");

    static std::uint32_t glow[128 * 128];
    BuildHexSyncGlowPixels(glow, 128);
    g_Glow = MakeTexture((const unsigned char*)glow, 128, 128);
}

static void FreeTextures()
{
    ImTextureID* all[] = { &g_Avatar, &g_Logo, &g_Glow };
    for (ImTextureID* t : all)
    {
        if (*t)
        {
            GLuint id = (GLuint)(intptr_t)*t;
            glDeleteTextures(1, &id);
            *t = nullptr;
        }
    }
}

// ---- fonts ------------------------------------------------------------------
// Glyphs are rasterized at the physical size and the logical layout stays at
// 100%, so text is sharp after the draw data is scaled to the screen.
static void AddFontsAtScale(float scale)
{
    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->Clear();
    io.FontGlobalScale = 1.0f / scale;
    g_Wordmark = nullptr;

    ImFontConfig text;
    text.OversampleH = 3;
    text.PixelSnapH = true;

    static const ImWchar icon_ranges[] = { ICON_MIN_FA, ICON_MAX_FA, 0 };
    ImFontConfig icons;
    icons.MergeMode = true;
    icons.PixelSnapH = true;

    size_t size = 0;
    void* data = ReadAsset("SSTMedium.TTF", &size);                       // font 0
    if (data) io.Fonts->AddFontFromMemoryTTF(data, (int)size, 15.0f * scale, &text);
    else io.Fonts->AddFontDefault();

    data = ReadAsset("fa-solid-900.ttf", &size);                          // merged icons
    if (data) io.Fonts->AddFontFromMemoryTTF(data, (int)size, 14.0f * scale, &icons, icon_ranges);

    data = ReadAsset("SSTBold.TTF", &size);                               // font 1
    if (data) io.Fonts->AddFontFromMemoryTTF(data, (int)size, 16.0f * scale, &text);

    ImFontConfig word;
    word.OversampleH = 3;
    word.PixelSnapH = true;
    data = ReadAsset("SpaceGrotesk-SemiBold.ttf", &size);                 // font 2
    if (data) g_Wordmark = io.Fonts->AddFontFromMemoryTTF(data, (int)size, 40.0f * scale, &word);

    g_FontScale = scale;
}

static void SyncFonts(float scale)
{
    if (g_FontScale > 0.0f && std::fabs(scale - g_FontScale) < 0.02f * g_FontScale)
        return;
    if (g_FontsLive)
        ImGui_ImplOpenGL3_DestroyFontsTexture();
    AddFontsAtScale(scale);
    if (g_FontsLive)
        ImGui_ImplOpenGL3_CreateFontsTexture();   // first upload happens inside NewFrame
}

// ---- EGL / ImGui lifetime -----------------------------------------------------
static bool Init(android_app* app)
{
    if (g_Initialized) return true;
    g_App = app;
    if (!g_App->window) return false;
    ANativeWindow_acquire(g_App->window);

    g_EglDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (g_EglDisplay == EGL_NO_DISPLAY) { LOGE("eglGetDisplay failed"); return false; }
    if (eglInitialize(g_EglDisplay, nullptr, nullptr) != EGL_TRUE) { LOGE("eglInitialize failed"); return false; }

    const EGLint attrs[] = {
        EGL_BLUE_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_RED_SIZE, 8, EGL_ALPHA_SIZE, 8,
        EGL_DEPTH_SIZE, 0, EGL_SURFACE_TYPE, EGL_WINDOW_BIT, EGL_NONE };
    EGLConfig config = nullptr;
    EGLint num = 0;
    eglChooseConfig(g_EglDisplay, attrs, &config, 1, &num);
    if (num < 1)
    {
        const EGLint fallback[] = { EGL_BLUE_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_RED_SIZE, 8,
                                    EGL_SURFACE_TYPE, EGL_WINDOW_BIT, EGL_NONE };
        eglChooseConfig(g_EglDisplay, fallback, &config, 1, &num);
    }
    if (num < 1) { LOGE("no EGL config"); return false; }

    EGLint format = 0;
    eglGetConfigAttrib(g_EglDisplay, config, EGL_NATIVE_VISUAL_ID, &format);
    ANativeWindow_setBuffersGeometry(g_App->window, 0, 0, format);

    const EGLint ctx_attrs[] = { EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE };
    g_EglContext = eglCreateContext(g_EglDisplay, config, EGL_NO_CONTEXT, ctx_attrs);
    g_EglSurface = eglCreateWindowSurface(g_EglDisplay, config, g_App->window, nullptr);
    if (g_EglContext == EGL_NO_CONTEXT || g_EglSurface == EGL_NO_SURFACE) { LOGE("EGL context/surface failed"); return false; }
    eglMakeCurrent(g_EglDisplay, g_EglSurface, g_EglSurface, g_EglContext);
    eglSwapInterval(g_EglDisplay, 1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;                       // no imgui.ini on device
    ImGui::StyleColorsDark();

    ImGui_ImplAndroid_Init(g_App->window);
    ImGui_ImplOpenGL3_Init("#version 300 es");

    g_FontScale = 0.0f;
    g_FontsLive = false;
    EGLint w = 0, h = 0;
    eglQuerySurface(g_EglDisplay, g_EglSurface, EGL_WIDTH, &w);
    eglQuerySurface(g_EglDisplay, g_EglSurface, EGL_HEIGHT, &h);
    g_Layout = nl::Fit(w, h, kFill, kUserScale);
    SyncFonts(g_Layout.scale);

    LoadTextures();
    g_Initialized = true;
    LOGI("initialized %dx%d scale %.3f", w, h, g_Layout.scale);
    return true;
}

static void Shutdown()
{
    if (!g_Initialized) return;

    FreeTextures();
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplAndroid_Shutdown();
    ImGui::DestroyContext();

    if (g_EglDisplay != EGL_NO_DISPLAY)
    {
        eglMakeCurrent(g_EglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (g_EglContext != EGL_NO_CONTEXT) eglDestroyContext(g_EglDisplay, g_EglContext);
        if (g_EglSurface != EGL_NO_SURFACE) eglDestroySurface(g_EglDisplay, g_EglSurface);
        eglTerminate(g_EglDisplay);
    }
    g_EglDisplay = EGL_NO_DISPLAY;
    g_EglContext = EGL_NO_CONTEXT;
    g_EglSurface = EGL_NO_SURFACE;
    g_FontsLive = false;
    g_FontScale = 0.0f;
    g_Wordmark = nullptr;

    if (g_App && g_App->window) ANativeWindow_release(g_App->window);
    g_Initialized = false;
}

// ---- frame ------------------------------------------------------------------
static void DrawBackground()
{
    if (!kDrawBackground) return;
    ImDrawList* bg = ImGui::GetBackgroundDrawList();
    bg->PushClipRectFullScreen();                    // cover the letterbox area too
    const ImVec2 lo = nl::ToLogical(g_Layout, 0.0f, 0.0f);
    const ImVec2 hi = nl::ToLogical(g_Layout, (float)g_Layout.width, (float)g_Layout.height);
    const ImU32 top = IM_COL32(44, 82, 150, 255);
    const ImU32 bottom = IM_COL32(8, 10, 16, 255);
    bg->AddRectFilledMultiColor(lo, hi, top, top, bottom, bottom);
    bg->PopClipRect();
}

static void MainLoopStep()
{
    if (!g_Initialized || g_EglDisplay == EGL_NO_DISPLAY) return;

    EGLint w = 0, h = 0;
    eglQuerySurface(g_EglDisplay, g_EglSurface, EGL_WIDTH, &w);
    eglQuerySurface(g_EglDisplay, g_EglSurface, EGL_HEIGHT, &h);
    if (w <= 0 || h <= 0) return;
    g_Layout = nl::Fit(w, h, kFill, kUserScale);
    SyncFonts(g_Layout.scale);

    ImGuiIO& io = ImGui::GetIO();
    ImGui_ImplOpenGL3_NewFrame();                    // uploads the atlas on the first frame
    g_FontsLive = true;
    ImGui_ImplAndroid_NewFrame();
    io.DisplaySize = ImVec2(nl::kCanvasW, nl::kCanvasH);   // logical canvas, not the screen
    io.DisplayFramebufferScale = ImVec2(1.0f, 1.0f);
    ImGui::NewFrame();

    DrawBackground();

    const bool intro_active = kPlayIntro && RenderHexSyncIntro(g_Logo, g_Glow, g_Wordmark);
    if (intro_active)
    {
        g_RevealStart = -1.0;
    }
    else
    {
        if (g_RevealStart < 0.0) g_RevealStart = ImGui::GetTime();
        const float reveal = ImClamp((float)((ImGui::GetTime() - g_RevealStart) / 0.48), 0.0f, 1.0f);
        RenderNeverloseMenu(g_Avatar, reveal);
    }

    ImGui::Render();
    ImDrawData* dd = ImGui::GetDrawData();
    nl::TransformDrawData(dd, g_Layout);

    glViewport(0, 0, w, h);
    glClearColor(0.035f, 0.043f, 0.063f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(dd);
    eglSwapBuffers(g_EglDisplay, g_EglSurface);
}

// ---- input ------------------------------------------------------------------
static int32_t HandleInput(android_app*, AInputEvent* event)
{
    if (!g_Initialized) return 0;
    ImGuiIO& io = ImGui::GetIO();

    if (AInputEvent_getType(event) == AINPUT_EVENT_TYPE_MOTION)
    {
        // Touch positions arrive in physical pixels; the UI lives in the logical canvas.
        const int32_t action = AMotionEvent_getAction(event) & AMOTION_EVENT_ACTION_MASK;
        const ImVec2 p = nl::ToLogical(g_Layout, AMotionEvent_getX(event, 0), AMotionEvent_getY(event, 0));
        io.AddMouseSourceEvent(ImGuiMouseSource_TouchScreen);
        switch (action)
        {
        case AMOTION_EVENT_ACTION_DOWN:
            io.AddMousePosEvent(p.x, p.y);
            io.AddMouseButtonEvent(0, true);
            break;
        case AMOTION_EVENT_ACTION_MOVE:
        case AMOTION_EVENT_ACTION_HOVER_MOVE:
            io.AddMousePosEvent(p.x, p.y);
            break;
        case AMOTION_EVENT_ACTION_UP:
        case AMOTION_EVENT_ACTION_CANCEL:
            io.AddMousePosEvent(p.x, p.y);
            io.AddMouseButtonEvent(0, false);
            io.AddMousePosEvent(-FLT_MAX, -FLT_MAX);   // no hover highlight after the finger lifts
            break;
        default:
            break;
        }
        return 1;
    }
    return ImGui_ImplAndroid_HandleInputEvent(event);  // keys (Esc closes popups)
}

static void HandleCmd(android_app* app, int32_t cmd)
{
    switch (cmd)
    {
    case APP_CMD_INIT_WINDOW: Init(app); break;
    case APP_CMD_TERM_WINDOW: Shutdown(); break;
    default: break;
    }
}

void android_main(android_app* app)
{
    app->onAppCmd = HandleCmd;
    app->onInputEvent = HandleInput;

    while (true)
    {
        int events = 0;
        android_poll_source* source = nullptr;
        int id;
        while ((id = ALooper_pollOnce(g_Initialized ? 0 : -1, nullptr, &events, (void**)&source)) >= 0)
        {
            if (source) source->process(app, source);
            if (app->destroyRequested != 0)
            {
                Shutdown();
                return;
            }
        }
        MainLoopStep();
    }
}
