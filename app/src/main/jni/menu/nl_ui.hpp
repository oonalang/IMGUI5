#pragma once

// Host independent entry point for the Neverlose / HexSync menu.
// Works with any Android ImGui host that uses imgui_impl_opengl3 (GLES3):
// your own NativeActivity loop, or an overlay running inside another process.
//
// Per frame, in this order:
//   ImGui_ImplOpenGL3_NewFrame();  (your backend / platform NewFrame calls)
//   nl_ui::BeginFrame(surfaceW, surfaceH);   // sets DisplaySize, rescales fonts
//   ImGui::NewFrame();
//   nl_ui::Draw();                           // background + intro + menu
//   ImGui::Render();
//   nl_ui::EndFrame();                       // converts draw data to screen pixels
//   ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

#include <cstddef>

namespace nl_ui
{
    // Returns a buffer allocated with IM_ALLOC (ImGui's font atlas frees it), or nullptr.
    // `name` is one of: SSTMedium.TTF SSTBold.TTF fa-solid-900.ttf SpaceGrotesk-SemiBold.ttf
    //                   avatar.rgba logo.rgba
    using AssetReader = void* (*)(const char* name, std::size_t* out_size);

    struct Config
    {
        float fill = 0.97f;          // share of the screen the menu may cover
        float user_scale = 1.0f;     // extra multiplier on top of auto-fit
        bool play_intro = true;
        bool draw_background = true; // set false for a transparent overlay
    };

    enum class Touch { Down, Move, Up };

    void SetAssetReader(AssetReader reader);   // call before Init
    void SetConfig(const Config& config);

    // After ImGui::CreateContext() and ImGui_ImplOpenGL3_Init(), with the GL context current.
    bool Init(int surface_w, int surface_h);
    // With the GL context still current.
    void Shutdown();

    void BeginFrame(int surface_w, int surface_h);
    void Draw();
    void EndFrame();

    // Screen pixel coordinates straight from the touch event.
    void OnTouch(Touch type, float x, float y);
}
