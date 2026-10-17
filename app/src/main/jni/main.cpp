// Standalone host: NativeActivity + EGL + OpenGL ES 3. Everything menu related is in nl_ui.
#include <cstdint>

#include "imgui.h"
#include "imgui_impl_android.h"
#include "imgui_impl_opengl3.h"

#include <android/asset_manager.h>
#include <android/input.h>
#include <android/log.h>
#include <android/native_window.h>
#include <android_native_app_glue.h>
#include <EGL/egl.h>
#include <GLES3/gl3.h>

#include "nl_ui.hpp"
#ifdef NL_EMBED_ASSETS
#include "nl_embedded_assets.hpp"
#endif

#define LOG_TAG "Neverlose"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

static android_app* g_App = nullptr;
static bool g_Initialized = false;
static EGLDisplay g_EglDisplay = EGL_NO_DISPLAY;
static EGLSurface g_EglSurface = EGL_NO_SURFACE;
static EGLContext g_EglContext = EGL_NO_CONTEXT;

#ifndef NL_EMBED_ASSETS
static void* ReadApkAsset(const char* name, size_t* out_size)
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
#endif

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
    ImGui::GetIO().IniFilename = nullptr;
    ImGui_ImplAndroid_Init(g_App->window);
    ImGui_ImplOpenGL3_Init("#version 300 es");

#ifdef NL_EMBED_ASSETS
    nl_ui::SetAssetReader(nl_embedded_asset_reader);
#else
    nl_ui::SetAssetReader(ReadApkAsset);
#endif
    EGLint w = 0, h = 0;
    eglQuerySurface(g_EglDisplay, g_EglSurface, EGL_WIDTH, &w);
    eglQuerySurface(g_EglDisplay, g_EglSurface, EGL_HEIGHT, &h);
    nl_ui::Init(w, h);

    g_Initialized = true;
    LOGI("initialized %dx%d", w, h);
    return true;
}

static void Shutdown()
{
    if (!g_Initialized) return;

    nl_ui::Shutdown();
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

    if (g_App && g_App->window) ANativeWindow_release(g_App->window);
    g_Initialized = false;
}

static void MainLoopStep()
{
    if (!g_Initialized || g_EglDisplay == EGL_NO_DISPLAY) return;

    EGLint w = 0, h = 0;
    eglQuerySurface(g_EglDisplay, g_EglSurface, EGL_WIDTH, &w);
    eglQuerySurface(g_EglDisplay, g_EglSurface, EGL_HEIGHT, &h);
    if (w <= 0 || h <= 0) return;

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplAndroid_NewFrame();
    nl_ui::BeginFrame(w, h);
    ImGui::NewFrame();

    nl_ui::Draw();

    ImGui::Render();
    nl_ui::EndFrame();

    glViewport(0, 0, w, h);
    glClearColor(0.035f, 0.043f, 0.063f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    eglSwapBuffers(g_EglDisplay, g_EglSurface);
}

static int32_t HandleInput(android_app*, AInputEvent* event)
{
    if (!g_Initialized) return 0;

    if (AInputEvent_getType(event) == AINPUT_EVENT_TYPE_MOTION)
    {
        const int32_t action = AMotionEvent_getAction(event) & AMOTION_EVENT_ACTION_MASK;
        const float x = AMotionEvent_getX(event, 0), y = AMotionEvent_getY(event, 0);
        switch (action)
        {
        case AMOTION_EVENT_ACTION_DOWN: nl_ui::OnTouch(nl_ui::Touch::Down, x, y); break;
        case AMOTION_EVENT_ACTION_MOVE:
        case AMOTION_EVENT_ACTION_HOVER_MOVE: nl_ui::OnTouch(nl_ui::Touch::Move, x, y); break;
        case AMOTION_EVENT_ACTION_UP:
        case AMOTION_EVENT_ACTION_CANCEL: nl_ui::OnTouch(nl_ui::Touch::Up, x, y); break;
        default: break;
        }
        return 1;
    }
    return ImGui_ImplAndroid_HandleInputEvent(event);
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
        while (ALooper_pollOnce(g_Initialized ? 0 : -1, nullptr, &events, (void**)&source) >= 0)
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
