#define IMGUI_DEFINE_MATH_OPERATORS
#include "nl_ui.hpp"

#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_opengl3.h"

#include <GLES3/gl3.h>

#include <cfloat>
#include <cmath>
#include <cstdint>
#include <cstring>

#include "hashes.hpp"
#include "hexsync_intro.hpp"
#include "neverlose_menu.hpp"
#include "nl_layout.hpp"

namespace
{
    nl_ui::AssetReader g_Reader = nullptr;
    nl_ui::Config g_Config;

    nl::Layout g_Layout;
    float g_FontScale = 0.0f;

    ImTextureID g_Avatar = nullptr;
    ImTextureID g_Logo = nullptr;
    ImTextureID g_Glow = nullptr;
    ImFont* g_Wordmark = nullptr;
    double g_RevealStart = -1.0;

    void* ReadAsset(const char* name, std::size_t* size)
    {
        return g_Reader ? g_Reader(name, size) : nullptr;
    }

    ImTextureID MakeTexture(const unsigned char* rgba, int w, int h)
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

    // width, height as little endian uint32, then RGBA8 pixels
    ImTextureID LoadRgbaAsset(const char* name)
    {
        std::size_t size = 0;
        unsigned char* data = (unsigned char*)ReadAsset(name, &size);
        if (!data) return nullptr;
        ImTextureID tex = nullptr;
        if (size > 8)
        {
            std::uint32_t w, h;
            std::memcpy(&w, data, 4);
            std::memcpy(&h, data + 4, 4);
            if ((std::size_t)w * h * 4 + 8 == size)
                tex = MakeTexture(data + 8, (int)w, (int)h);
        }
        IM_FREE(data);
        return tex;
    }

    void LoadTextures()
    {
        g_Avatar = LoadRgbaAsset("avatar.rgba");
        g_Logo = LoadRgbaAsset("logo.rgba");

        static std::uint32_t glow[128 * 128];
        BuildHexSyncGlowPixels(glow, 128);
        g_Glow = MakeTexture((const unsigned char*)glow, 128, 128);
    }

    void FreeTextures()
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

    // Glyphs are rasterized at the physical size while the layout stays at 100%,
    // so text stays sharp after the draw data is scaled to the screen.
    void AddFontsAtScale(float scale)
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

        std::size_t size = 0;
        void* data = ReadAsset("SSTMedium.TTF", &size);                    // font 0
        if (data) io.Fonts->AddFontFromMemoryTTF(data, (int)size, 15.0f * scale, &text);
        else io.Fonts->AddFontDefault();

        data = ReadAsset("fa-solid-900.ttf", &size);                       // merged icons
        if (data) io.Fonts->AddFontFromMemoryTTF(data, (int)size, 14.0f * scale, &icons, icon_ranges);

        data = ReadAsset("SSTBold.TTF", &size);                            // font 1
        if (data) io.Fonts->AddFontFromMemoryTTF(data, (int)size, 16.0f * scale, &text);

        ImFontConfig word;
        word.OversampleH = 3;
        word.PixelSnapH = true;
        data = ReadAsset("SpaceGrotesk-SemiBold.ttf", &size);              // font 2
        if (data) g_Wordmark = io.Fonts->AddFontFromMemoryTTF(data, (int)size, 40.0f * scale, &word);

        g_FontScale = scale;
    }

    void SyncFonts(float scale)
    {
        if (g_FontScale > 0.0f && std::fabs(scale - g_FontScale) < 0.02f * g_FontScale)
            return;
        ImGuiIO& io = ImGui::GetIO();
        const bool uploaded = io.Fonts->TexID != nullptr;   // false until the first NewFrame
        if (uploaded) ImGui_ImplOpenGL3_DestroyFontsTexture();
        AddFontsAtScale(scale);
        if (uploaded) ImGui_ImplOpenGL3_CreateFontsTexture();
    }
}

namespace nl_ui
{
    void SetAssetReader(AssetReader reader) { g_Reader = reader; }
    void SetConfig(const Config& config) { g_Config = config; }

    bool Init(int surface_w, int surface_h)
    {
        g_FontScale = 0.0f;
        g_RevealStart = -1.0;
        g_Layout = nl::Fit(surface_w, surface_h, g_Config.fill, g_Config.user_scale);
        SyncFonts(g_Layout.scale);
        LoadTextures();
        return true;
    }

    void Shutdown()
    {
        FreeTextures();
        g_Wordmark = nullptr;
        g_FontScale = 0.0f;
    }

    void BeginFrame(int surface_w, int surface_h)
    {
        g_Layout = nl::Fit(surface_w, surface_h, g_Config.fill, g_Config.user_scale);
        SyncFonts(g_Layout.scale);

        ImGuiIO& io = ImGui::GetIO();
        io.DisplaySize = ImVec2(nl::kCanvasW, nl::kCanvasH);   // logical canvas, not the screen
        io.DisplayFramebufferScale = ImVec2(1.0f, 1.0f);
    }

    void Draw()
    {
        if (g_Config.draw_background)
        {
            ImDrawList* bg = ImGui::GetBackgroundDrawList();
            bg->PushClipRectFullScreen();                       // also cover the letterbox area
            const ImVec2 lo = nl::ToLogical(g_Layout, 0.0f, 0.0f);
            const ImVec2 hi = nl::ToLogical(g_Layout, (float)g_Layout.width, (float)g_Layout.height);
            const ImU32 top = IM_COL32(44, 82, 150, 255);
            const ImU32 bottom = IM_COL32(8, 10, 16, 255);
            bg->AddRectFilledMultiColor(lo, hi, top, top, bottom, bottom);
            bg->PopClipRect();
        }

        if (g_Config.play_intro && RenderHexSyncIntro(g_Logo, g_Glow, g_Wordmark))
        {
            g_RevealStart = -1.0;
            return;
        }

        if (g_RevealStart < 0.0) g_RevealStart = ImGui::GetTime();
        const float reveal = ImClamp((float)((ImGui::GetTime() - g_RevealStart) / 0.48), 0.0f, 1.0f);
        RenderNeverloseMenu(g_Avatar, reveal);
    }

    void EndFrame()
    {
        nl::TransformDrawData(ImGui::GetDrawData(), g_Layout);
    }

    void OnTouch(Touch type, float x, float y)
    {
        if (!ImGui::GetCurrentContext()) return;
        ImGuiIO& io = ImGui::GetIO();
        const ImVec2 p = nl::ToLogical(g_Layout, x, y);
        io.AddMouseSourceEvent(ImGuiMouseSource_TouchScreen);
        switch (type)
        {
        case Touch::Down:
            io.AddMousePosEvent(p.x, p.y);
            io.AddMouseButtonEvent(0, true);
            break;
        case Touch::Move:
            io.AddMousePosEvent(p.x, p.y);
            break;
        case Touch::Up:
            io.AddMousePosEvent(p.x, p.y);
            io.AddMouseButtonEvent(0, false);
            io.AddMousePosEvent(-FLT_MAX, -FLT_MAX);            // no hover highlight after lift-off
            break;
        }
    }
}
